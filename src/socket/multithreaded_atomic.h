#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
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

class Buffer {
public:
	Buffer(std::mutex *mutex, std::size_t cursor, std::size_t size) : mutex_(mutex), cursor_(cursor), size_(size) {}
	Buffer(std::mutex *mutex) : Buffer(mutex, 0, 0) {}
	std::pair<std::size_t, std::size_t> lock_interval(std::size_t n) {
		if (size_.load() == 0) {
			std::unique_lock<std::mutex> lock(*mutex_);
			nonempty_.wait(lock, [this] { return terminated_ || size_.load() > 0; });
			if (size_.load() == 0) {
				return {0, 0};
			}
		}
		std::size_t size = size_.load();
		std::size_t cursor = cursor_;
		std::size_t len;
		do {
			len = std::min({n, size, PIEX_OPTION_SOCKET_BUFFER_SIZE - cursor_});
		} while (!size_.compare_exchange_strong(size, size - len));
		cursor_ += len;
		cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
		return {cursor, len};
	}
	void release(std::size_t n) {
		cursor_ += PIEX_OPTION_SOCKET_BUFFER_SIZE - n; // avoid underflow
		cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
		size_ += n;
	}
	template <class F>
	std::size_t lock(const F &op) {
		size_t offset, len;
		std::tie(offset, len) = lock_interval(PIEX_OPTION_SOCKET_BUFFER_SIZE);
		if (len == 0) {
			return 0;
		}
		int ret = op(offset, len);
		if (ret > 0 && static_cast<unsigned>(ret) < len) {
			release(len - ret);
		}
		return ret;
	}
	void expand(std::size_t n) {
		if (size_.fetch_add(n) == 0) {
			std::lock_guard<std::mutex> lock(*mutex_);
			nonempty_.notify_one();
		}
	}
	bool nonempty() {
		return size_.load() > 0;
	}
	void terminate() {
		std::lock_guard<std::mutex> lock(*mutex_);
		terminated_ = true;
		nonempty_.notify_one();
	}
private:
	bool terminated_ = false;
	std::mutex *mutex_;
	std::condition_variable nonempty_;
	std::size_t cursor_;
	volatile std::atomic_size_t size_;
};

struct Daemon {
public:
	~Daemon() {
		data_.terminate();
		space_.terminate();
		thread_.join();
	}
	int fd_ = -1;
	char buffer_[PIEX_OPTION_SOCKET_BUFFER_SIZE];
	std::mutex mutex_;
	Buffer data_, space_;
	std::thread thread_;
protected:
	template <class Function, class... Args>
	Daemon(int fd, Function &&f, Args &&...args) :
		fd_(fd),
		data_(&mutex_, 0, 0),
		space_(&mutex_, 0, PIEX_OPTION_SOCKET_BUFFER_SIZE),
		thread_(f, args...) {};
};

class ReaderDaemon : public Daemon {
public:
	ReaderDaemon(int fd) : Daemon(fd, &ReaderDaemon::body, this) {}
private:
	void body() {
		while (true) {
			int ret = space_.lock([this](std::size_t offset, std::size_t len) {
				return ::read(fd_, buffer_ + offset, len);
			});
			if (ret <= 0) {
				return;
			}
			data_.expand(ret);
		}
	}
};

class WriterDaemon : public Daemon {
public:
	WriterDaemon(int fd) : Daemon(fd, &WriterDaemon::body, this) {}
private:
	void body() {
		while (true) {
			int ret = data_.lock([this](std::size_t offset, std::size_t len) {
				return ::write(fd_, buffer_ + offset, len);
			});
			if (ret <= 0) {
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
		start_daemon();
	}
	void listen(const char *host, const char *port) {
		fd_ = utility::socket::create_socket(host, port, true);
		int ret = ::listen(fd_, 0);
		if (ret < 0) {
			throw std::runtime_error(std::strerror(errno));
		}
	}
	int read(void *buf, size_t nbytes_total, size_t nbytes_read = 0) {
		while (nbytes_read < nbytes_total) {
			int ret = reader_->data_.lock([this, &buf, &nbytes_total, &nbytes_read](size_t offset, size_t len) {
				size_t nbytes_to_copy = std::min(len, nbytes_total - nbytes_read);
				std::memcpy(static_cast<char *>(buf) + nbytes_read, reader_->buffer_ + offset, nbytes_to_copy);
				return nbytes_to_copy;
			});
			if (ret <= 0) {
				break;
			}
			reader_->space_.expand(ret);
			nbytes_read += ret;
		}
		return nbytes_read;
	}
	int write(const void *buf, size_t nbytes_total) {
		size_t nbytes_written = 0;
		while (nbytes_written < nbytes_total) {
			int ret = writer_->space_.lock([this, &buf, &nbytes_total, &nbytes_written](size_t offset, size_t len) {
				size_t nbytes_to_copy = std::min(len, nbytes_total - nbytes_written);
				std::memcpy(writer_->buffer_ + offset, static_cast<const char *>(buf) + nbytes_written, nbytes_to_copy);
				return nbytes_to_copy;
			});
			if (ret <= 0) {
				break;
			}
			writer_->data_.expand(ret);
			nbytes_written += ret;
		}
		return nbytes_written;
	}
	int flush() {
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
