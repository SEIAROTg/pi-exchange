#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <utility>
#include <tuple>
#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>
#include "src/utility/socket.h"
#include "config/config.h"

namespace piex {
namespace socket {

// for non-deterministic interval
struct SizeN {
	std::size_t size;
	std::size_t flush_size;
};

// non-deterministic interval: support partial consuming and flushing
class IntervalN {
public:
	IntervalN(std::size_t size) : size_({size, 0}) {}
	std::pair<std::size_t, std::size_t> load() {
		SizeN sizes = size_.load();
		return {cursor_, sizes.size};
	}
	void consume(std::size_t n) {
		SizeN sizes = size_.load();
		cursor_ += n;
		cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
		while (!size_.compare_exchange_weak(sizes, {sizes.size - n, n >= sizes.flush_size ? 0 : sizes.flush_size - n}));
	}
	std::size_t expand(std::size_t n) {
		SizeN sizes = size_.load();
		while (!size_.compare_exchange_weak(sizes, {sizes.size + n, sizes.flush_size}));
		return sizes.size;
	}
	void flush() {
		SizeN sizes = size_.load();
		while (!size_.compare_exchange_weak(sizes, {sizes.size, sizes.size}));
	}
	bool ready(std::size_t n) {
		SizeN sizes = size_.load();
		return sizes.size >= n || sizes.flush_size;
	}
private:
	std::size_t cursor_ = 0;
	std::atomic<SizeN> size_;
};

// deterministic interval: no support for partial consuming and flushing
class IntervalD {
public:
	IntervalD(std::size_t size) : size_(size) {} 
	std::pair<std::size_t, std::size_t> load() {
		return {cursor_, size_.load()};
	}
	void consume(std::size_t n) {
		cursor_ += n;
		cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
		size_ -= n;
	}
	std::size_t expand(std::size_t n) {
		return size_.fetch_add(n);
	}
	void flush() {
		// not supported
	}
	bool ready(std::size_t n) {
		return size_.load() >= n;
	}
private:
	std::size_t cursor_ = 0;
	std::atomic<std::size_t> size_;
};

template <class Interval>
class Buffer {
public:
	Buffer(std::size_t size) : interval_(size) {};
	Buffer() : Buffer(0) {}

	/* wait until interval is ready and lock a buffer segment for op.
	 *     if Interval is IntervalN, the whole buffer will be locked and the consumed size will depend on op
	 *     if Interval is IntervalD, a buffer segment of size n will be locked and consumed
	 */
	template <class F>
	ssize_t lock(std::size_t n, const F &op) {
		if (!interval_.ready(n) && !terminated_) {
			std::unique_lock<std::mutex> lock(mutex_);
			waiting_size_ = n;
			nonempty_.wait(lock, [this, n] { return interval_.ready(n) || terminated_; });
			waiting_size_ = 0;
		}
		if (!interval_.ready(n)) {
			return 0;
		}
		ssize_t ret;
		if (std::is_same<Interval, IntervalN>()) {
			auto [offset, len] = interval_.load();
			ret = op(offset, len);
			if (ret > 0) {
				interval_.consume(ret);
			}
		} else {
			std::size_t offset = interval_.load().first;
			interval_.consume(n);
			ret = op(offset, n);
		}
		return ret;
	}
	void expand(std::size_t n) {
		std::size_t size = interval_.expand(n);
		std::size_t waiting_size = waiting_size_.load();
		if (size < waiting_size && size + n >= waiting_size) {
			std::lock_guard<std::mutex> lock(mutex_);
			nonempty_.notify_one();
		}
	}
	bool nonempty() {
		return interval_.load().second > 0;
	}
	void terminate() {
		std::lock_guard<std::mutex> lock(mutex_);
		terminated_ = true;
		nonempty_.notify_one();
	}
	void flush() {
		interval_.flush();
		nonempty_.notify_one();
	}
	bool terminated_ = false;
private:
	std::mutex mutex_;
	std::condition_variable nonempty_;
	Interval interval_;
	std::atomic_size_t waiting_size_ = 0;
};

template <class DataInterval, class SpaceInterval>
struct Daemon {
public:
	~Daemon() {
		data_.terminate();
		space_.terminate();
		thread_.join();
	}
	int fd_ = -1;
	char buffer_[PIEX_OPTION_SOCKET_BUFFER_SIZE];
	Buffer<DataInterval> data_;
	Buffer<SpaceInterval> space_;
	std::thread thread_;
protected:
	template <class Function, class... Args>
	Daemon(int fd, Function &&f, Args &&...args) :
		fd_(fd),
		data_(0),
		space_(PIEX_OPTION_SOCKET_BUFFER_SIZE),
		thread_(f, args...) {};
};

class ReaderDaemon : public Daemon<IntervalD, IntervalN> {
public:
	ReaderDaemon(int fd) : Daemon(fd, &ReaderDaemon::body, this) {}
private:
	void body() {
		while (true) {
			ssize_t ret = space_.lock(PIEX_OPTION_SOCKET_FLUSH_THRESHOLD, [this](std::size_t offset, std::size_t len) {
				size_t nbytes_to_read = std::min(PIEX_OPTION_SOCKET_BUFFER_SIZE - offset, len);
				ssize_t ret = ::read(fd_, buffer_ + offset, nbytes_to_read);
				return ret;
			});
			if (ret <= 0 || space_.terminated_) {
				return;
			}
			data_.expand(ret);
		}
	}
};

class WriterDaemon : public Daemon<IntervalN, IntervalD> {
public:
	WriterDaemon(int fd) : Daemon(fd, &WriterDaemon::body, this) {}
private:
	ssize_t writen(const void *buf, std::size_t nbytes_total) {
		std::size_t nbytes_written = 0;
		while (nbytes_written < nbytes_total) {
			ssize_t ret = ::write(fd_, static_cast<const char*>(buf) + nbytes_written, nbytes_total - nbytes_written);
			if (ret <= 0) {
				break;
			}
			nbytes_written += ret;
		}
		return nbytes_written;
	}
	void body() {
		while (true) {
			ssize_t ret = data_.lock(PIEX_OPTION_SOCKET_FLUSH_THRESHOLD, [this](std::size_t offset, std::size_t len) {
				// must write out everything once
				if (len + offset > PIEX_OPTION_SOCKET_BUFFER_SIZE) {
					ssize_t ret1 = writen(buffer_ + offset, PIEX_OPTION_SOCKET_BUFFER_SIZE - offset);
					if (ret1 <= 0) {
						return ret1;
					}
					ssize_t ret2 = writen(buffer_, offset + len - PIEX_OPTION_SOCKET_BUFFER_SIZE);
					if (ret2 <= 0) {
						return ret1;
					}
					return ret1 + ret2;
				} else {
					return writen(buffer_ + offset, len);
				}
			});
			if (ret <= 0 || data_.terminated_) {
				return;
			}
			space_.expand(ret);
		}
	}
};

// not thread safe
class Socket {
public:
	Socket() = default;
	Socket(Socket &&other) {
		fd_ = other.fd_;
		other.fd_ = -1;
		reader_ = std::move(other.reader_);
		writer_ = std::move(other.writer_);
	}
	~Socket() {
		if (fd_ != -1) {
			close();
		}
	}
	void connect(const char *host, const char *port) {
		fd_ = utility::socket::create_socket(host, port, false);
		utility::socket::enable_option(fd_, IPPROTO_TCP, TCP_NODELAY);
		start_daemon();
	}
	void listen(const char *host, const char *port) {
		fd_ = utility::socket::create_socket(host, port, true);
		utility::socket::enable_option(fd_, IPPROTO_TCP, TCP_NODELAY);
		int ret = ::listen(fd_, 0);
		if (ret < 0) {
			throw std::runtime_error(std::strerror(errno));
		}
	}
	ssize_t read(void *buf, std::size_t nbytes_total, std::size_t nbytes_read = 0) {
		while (nbytes_read < nbytes_total) {
			std::size_t nbytes_to_read = std::min<std::size_t>(nbytes_total - nbytes_read, PIEX_OPTION_SOCKET_FLUSH_THRESHOLD);
			ssize_t ret = reader_->data_.lock(nbytes_to_read, [this, buf, &nbytes_read](std::size_t offset, std::size_t len) {
				if (offset + len > PIEX_OPTION_SOCKET_BUFFER_SIZE) {
					std::memcpy(
						static_cast<char *>(buf) + nbytes_read,
						reader_->buffer_ + offset,
						PIEX_OPTION_SOCKET_BUFFER_SIZE - offset);
					std::memcpy(
						static_cast<char *>(buf) + nbytes_read + PIEX_OPTION_SOCKET_BUFFER_SIZE - offset,
						reader_->buffer_,
						offset + len - PIEX_OPTION_SOCKET_BUFFER_SIZE);
				} else {
					std::memcpy(static_cast<char *>(buf) + nbytes_read, reader_->buffer_ + offset, len);
				}
				nbytes_read += len;
				return len;
			});
			reader_->space_.expand(ret);
		}
		return nbytes_read;
	}
	ssize_t write(const void *buf, std::size_t nbytes_total) {
		std::size_t nbytes_written = 0;
		if (nbytes_total <= PIEX_OPTION_SOCKET_BUFFER_SIZE) {
			ssize_t ret = writer_->space_.lock(nbytes_total - nbytes_written, [this, buf, &nbytes_written](std::size_t offset, std::size_t len) {
				if (offset + len > PIEX_OPTION_SOCKET_BUFFER_SIZE) {
					std::memcpy(
						writer_->buffer_ + offset,
						static_cast<const char *>(buf) + nbytes_written,
						PIEX_OPTION_SOCKET_BUFFER_SIZE - offset);
					std::memcpy(
						writer_->buffer_,
						static_cast<const char *>(buf) + nbytes_written + PIEX_OPTION_SOCKET_BUFFER_SIZE - offset,
						offset + len - PIEX_OPTION_SOCKET_BUFFER_SIZE);
				} else {
					std::memcpy(writer_->buffer_ + offset, static_cast<const char *>(buf) + nbytes_written, len);
				}
				nbytes_written += len;
				return len;
			});
			writer_->data_.expand(ret);
		} else {
			writer_->space_.lock(PIEX_OPTION_SOCKET_BUFFER_SIZE, [this, buf, nbytes_total, &nbytes_written](std::size_t, std::size_t) {
				while (nbytes_written < nbytes_total) {
					ssize_t ret = ::write(fd_, static_cast<const char *>(buf) + nbytes_written, nbytes_total - nbytes_written);
					if (ret <= 0) {
						return ret;
					}
					nbytes_written += ret;
				}
				return (ssize_t)0;
			});
		}
		return nbytes_written;
	}
	int flush() {
		writer_->data_.flush();
		return 0;
	}
	bool read_ready() {
		return reader_->data_.nonempty();
	}
	void close() {
		::shutdown(fd_, SHUT_RDWR);
		::close(fd_);
		fd_ = -1;
		reader_.reset();
		writer_.reset();
	}
	Socket accept() {
		Socket socket;
		socket.fd_ = ::accept(fd_, nullptr, nullptr);
		socket.start_daemon();
		return socket;
	}
private:
	int fd_ = -1;
	std::unique_ptr<ReaderDaemon> reader_;
	std::unique_ptr<WriterDaemon> writer_;
	void start_daemon() {
		reader_ = std::make_unique<ReaderDaemon>(fd_);
		writer_ = std::make_unique<WriterDaemon>(fd_);
	}
};

}

using socket::Socket;

}
