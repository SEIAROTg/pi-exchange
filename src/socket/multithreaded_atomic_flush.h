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
#include "config/config.h"

namespace piex {
namespace socket {

class Interval {
public:
	Interval(std::size_t cursor, std::size_t size) :
		cursor_(cursor),
		size_(size) {}
	Interval() : Interval(0, 0) {}
	// wait until interval size >= n or flushed, and lock the whole interval or an interval with size flush_size
	template <class F>
	ssize_t lock_at_least(std::size_t n, const F &op) {
		if (size_.load() < n && to_flush_ == 0 && !terminated_) {
			std::unique_lock<std::mutex> lock(mutex_);
			waiting_size_ = n;
			nonempty_.wait(lock, [this, n] { return size_.load() >= n || to_flush_ > 0 || terminated_; });
			waiting_size_ = 0;
		}
		if (size_.load() < n && to_flush_ == 0) {
			return 0;
		}
		std::size_t size = size_.load();
		std::size_t offset = cursor_;
		std::size_t len = size >= n ? size : to_flush_;
		to_flush_ = 0;
		cursor_ += len;
		cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
		size_ -= len;
		ssize_t ret = op(offset, len);
		if (ret <= 0) {
			cursor_ -= len;
			cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
			size_ += len;
		} else if (static_cast<unsigned>(ret) < len) {
			unsigned diff = len - ret;
			cursor_ -= diff;
			cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
			size_ += diff;
			if (len < n) {
				to_flush_ += diff;
			}
		}
		return ret;
	}
	template <class F>
	ssize_t lock_exact(std::size_t n, const F &op) {
		if (size_.load() < n && !terminated_) {
			std::unique_lock<std::mutex> lock(mutex_);
			waiting_size_ = n;
			nonempty_.wait(lock, [this, n] { return size_.load() >= n || terminated_; });
			waiting_size_ = 0;
		}
		if (size_.load() < n) {
			return 0;
		}
		std::size_t offset = cursor_;
		std::size_t len = n;
		cursor_ += len;
		cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
		size_ -= len;
		ssize_t ret = op(offset, len);
		return ret;
	}
	void expand(std::size_t n) {
		std::size_t size = size_ += n;
		std::size_t waiting_size = waiting_size_.load();
		if (size - n < waiting_size && size >= waiting_size) {
			std::lock_guard<std::mutex> lock(mutex_);
			nonempty_.notify_one();
		}
	}
	bool nonempty() {
		return size_.load() > 0;
	}
	void terminate() {
		std::lock_guard<std::mutex> lock(mutex_);
		terminated_ = true;
		nonempty_.notify_one();
	}
	void flush() {
		to_flush_ = size_.load();
		nonempty_.notify_one();
	}
	bool terminated_ = false;
private:
	std::size_t to_flush_ = 0;
	std::mutex mutex_;
	std::condition_variable nonempty_;
	std::size_t cursor_;
	std::atomic_size_t size_;
	std::atomic_size_t waiting_size_ = 0;
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
	Interval data_, space_;
	std::thread thread_;
protected:
	template <class Function, class... Args>
	Daemon(int fd, Function &&f, Args &&...args) :
		fd_(fd),
		data_(0, 0),
		space_(0, PIEX_OPTION_SOCKET_BUFFER_SIZE),
		thread_(f, args...) {};
};

class ReaderDaemon : public Daemon {
public:
	ReaderDaemon(int fd) : Daemon(fd, &ReaderDaemon::body, this) {}
private:
	void body() {
		while (true) {
			ssize_t ret = space_.lock_at_least(PIEX_OPTION_SOCKET_FLUSH_THRESHOLD, [this](std::size_t offset, std::size_t len) {
				size_t nbytes_to_read = std::min(PIEX_OPTION_SOCKET_BUFFER_SIZE - offset, len);
				return ::read(fd_, buffer_ + offset, nbytes_to_read);
			});
			if (ret <= 0 || space_.terminated_) {
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
			ssize_t ret = data_.lock_at_least(PIEX_OPTION_SOCKET_FLUSH_THRESHOLD, [this](std::size_t offset, std::size_t len) {
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
		fd_ = get_ready_socket(host, port, false);
		start_daemon();
	}
	void listen(const char *host, const char *port) {
		fd_ = get_ready_socket(host, port, true);
		int ret = ::listen(fd_, 0);
		if (ret < 0) {
			throw std::runtime_error(std::strerror(errno));
		}
	}
	ssize_t read(void *buf, std::size_t nbytes_total, std::size_t nbytes_read = 0) {
		while (nbytes_read < nbytes_total) {
			std::size_t nbytes_to_read = std::min<std::size_t>(nbytes_total - nbytes_read, PIEX_OPTION_SOCKET_FLUSH_THRESHOLD);
			ssize_t ret = reader_->data_.lock_exact(nbytes_to_read, [this, buf, &nbytes_read](std::size_t offset, std::size_t len) {
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
			ssize_t ret = writer_->space_.lock_exact(nbytes_total - nbytes_written, [this, buf, &nbytes_written](std::size_t offset, std::size_t len) {
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
			writer_->space_.lock_at_least(PIEX_OPTION_SOCKET_BUFFER_SIZE, [this, buf, nbytes_total, &nbytes_written](std::size_t, std::size_t) {
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
	static int get_ready_socket(const char *host, const char *port, bool is_server) {
		int ret;
		addrinfo hints = {
			.ai_flags = is_server ? AI_PASSIVE : 0,
			.ai_family = AF_UNSPEC,
			.ai_socktype = SOCK_STREAM,
			.ai_protocol = 0,
			.ai_addrlen = 0,
			.ai_addr = nullptr,
			.ai_canonname = nullptr,
			.ai_next = nullptr,
		};
		addrinfo *results;
		ret = getaddrinfo(host, port, &hints, &results);
		if (ret) {
			throw std::runtime_error(gai_strerror(ret));
		}
		for (addrinfo *cursor = results; cursor; cursor = cursor->ai_next) {
			if (cursor->ai_family == AF_INET || cursor->ai_family == AF_INET6) {
				int fd = ::socket(cursor->ai_family, cursor->ai_socktype, cursor->ai_protocol);
				int on = 1;
				setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
				if (fd < 0) {
					freeaddrinfo(results);
					throw std::runtime_error(std::strerror(errno));
				}
				ret = (is_server ? bind : ::connect)(fd, cursor->ai_addr, cursor->ai_addrlen);
				if (ret < 0) {
					freeaddrinfo(results);
					throw std::runtime_error(std::strerror(errno));
				}
				freeaddrinfo(results);
				return fd;
			}
		}
		throw std::runtime_error("getaddrinfo returns no data");
	}
	void start_daemon() {
		reader_ = std::make_unique<ReaderDaemon>(fd_);
		writer_ = std::make_unique<WriterDaemon>(fd_);
	}
};

}

using socket::Socket;

}
