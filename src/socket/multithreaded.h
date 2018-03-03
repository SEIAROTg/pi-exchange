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
#include <condition_variable>
#include "config.h"

namespace piex {
namespace socket {

struct Daemon {
public:
	~Daemon() {
		thread_.join();
	}
	std::pair<std::size_t, std::size_t> space_interval() {
		std::size_t offset, len;
		if (cursor_ + size_ >= PIEX_OPTION_SOCKET_BUFFER_SIZE) {
			offset = cursor_ + size_ - PIEX_OPTION_SOCKET_BUFFER_SIZE;
			len = PIEX_OPTION_SOCKET_BUFFER_SIZE - size_;
		} else {
			offset = cursor_ + size_;
			len = PIEX_OPTION_SOCKET_BUFFER_SIZE - size_ - cursor_;
		}
		return {offset, len};
	}
	std::pair<std::size_t, std::size_t> data_interval() {
		std::size_t len;
		if (cursor_ + size_ >= PIEX_OPTION_SOCKET_BUFFER_SIZE) {
			len = PIEX_OPTION_SOCKET_BUFFER_SIZE - cursor_;
		} else {
			len = size_;
		}
		return {cursor_, len};
	}
	bool ready_read() {
		return size_ > 0 || terminated_;
	}
	bool ready_write() {
		return size_ < PIEX_OPTION_SOCKET_BUFFER_SIZE || terminated_;
	}
	int fd_ = -1;
	char buffer_[PIEX_OPTION_SOCKET_BUFFER_SIZE];
	std::size_t cursor_ = 0, size_ = 0;
	bool terminated_ = false;
	std::mutex mutex_;
	std::condition_variable data_available_, space_available_;
	std::thread thread_;
protected:
	template <class Function, class... Args>
	Daemon(int fd, Function &&f, Args &&...args) : fd_(fd), thread_(f, args...) {};
};

class ReaderDaemon : public Daemon {
public:
	ReaderDaemon(int fd) : Daemon(fd, &ReaderDaemon::body, this) {}
	~ReaderDaemon() {
		terminated_ = true;
		space_available_.notify_one();
	}
private:
	void body() {
		while (true) {
			int offset, len;
			{
				std::unique_lock<std::mutex> lock(mutex_);
				if (!ready_write()) {
					space_available_.wait(lock, [this] { return ready_write(); });
				}
				if (terminated_) {
					return;
				}
				std::tie(offset, len) = space_interval();
			}
			int ret = ::read(fd_, buffer_ + offset, len);
			if (ret <= 0) {
				return;
			}
			{
				std::lock_guard<std::mutex> lock(mutex_);
				size_ += ret;
			}
			data_available_.notify_one();
		}
	}
};

class WriterDaemon : public Daemon {
public:
	WriterDaemon(int fd) : Daemon(fd, &WriterDaemon::body, this) {}
	~WriterDaemon() {
		terminated_ = true;
		data_available_.notify_one();
	}
private:
	void body() {
		while (true) {
			int offset, len;
			{
				std::unique_lock<std::mutex> lock(mutex_);
				if (!ready_read()) {
					data_available_.wait(lock, [this] { return ready_read(); });
				}
				if (terminated_) {
					return;
				}
				offset = cursor_;
				std::tie(offset, len) = data_interval();
			}
			int ret = ::write(fd_, buffer_ + offset, len);
			if (ret <= 0) {
				return;
			}
			{
				std::lock_guard<std::mutex> lock(mutex_);
				cursor_ += ret;
				cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
				size_ -= ret;
			}
			space_available_.notify_one();
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
	int read(void *buf, std::size_t nbytes_total, std::size_t nbytes_read = 0) {
		while (nbytes_read < nbytes_total) {
			std::size_t offset, len;
			{
				std::unique_lock<std::mutex> lock(reader_->mutex_);
				if (!reader_->ready_read()) {
					reader_->data_available_.wait(lock, [this] { return reader_->ready_read(); });
				}
				if (reader_->terminated_) {
					return 0;
				}
				offset = reader_->cursor_;
				std::tie(offset, len) = reader_->data_interval();
			}
			std::size_t nbytes_to_copy = std::min(len, nbytes_total - nbytes_read);
			std::memcpy(static_cast<char *>(buf) + nbytes_read, reader_->buffer_ + offset, nbytes_to_copy);
			{
				std::lock_guard<std::mutex> lock(reader_->mutex_);
				reader_->cursor_ += nbytes_to_copy;
				reader_->cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
				reader_->size_ -= nbytes_to_copy;
			}
			reader_->space_available_.notify_one();
			nbytes_read += nbytes_to_copy;
		}
		return nbytes_total;
	}
	int write(const void *buf, std::size_t nbytes) {
		std::size_t nbytes_written = 0;
		while (nbytes_written < nbytes) {
			std::size_t offset, len;
			{
				std::unique_lock<std::mutex> lock(writer_->mutex_);
				if (!writer_->ready_write()) {
					writer_->space_available_.wait(lock, [this] { return writer_->ready_write(); });
				}
				if (writer_->terminated_) {
					return 0;
				}
				std::tie(offset, len) = writer_->space_interval();
			}
			std::size_t nbytes_to_copy = std::min(len, nbytes - nbytes_written);
			std::memcpy(writer_->buffer_ + offset, static_cast<const char *>(buf) + nbytes_written, nbytes_to_copy);
			{
				std::lock_guard<std::mutex> lock(writer_->mutex_);
				writer_->size_ += nbytes_to_copy;
			}
			writer_->data_available_.notify_one();
			nbytes_written += nbytes_to_copy;
		}
		return nbytes;
	}
	int flush() {
		return 0;
	}
	bool read_ready() {
		return reader_->size_ > 0;
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
