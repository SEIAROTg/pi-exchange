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
#include "src/utility/socket.h"
#include "config/config.h"

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
		return size_ > 0;
	}
	bool ready_write() {
		return size_ < PIEX_OPTION_SOCKET_BUFFER_SIZE;
	}
	template <class F>
	int read(const F &op) {
		int offset, len;
		{
			std::unique_lock<std::mutex> lock(mutex_);
			data_available_.wait(lock, [this] { return ready_read() || terminated_; });
			if (!ready_read()) {
				return 0;
			}
			offset = cursor_;
			std::tie(offset, len) = data_interval();
		}
		int ret = op(offset, len);
		if (ret > 0) {
			std::lock_guard<std::mutex> lock(mutex_);
			cursor_ += ret;
			cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
			size_ -= ret;
		}
		return ret;
	}
	template <class F>
	int write(const F &op) {
		int offset, len;
		{
			std::unique_lock<std::mutex> lock(mutex_);
			space_available_.wait(lock, [this] { return ready_write() || terminated_; });
			if (!ready_write()) {
				return 0;
			}
			std::tie(offset, len) = space_interval();
		}
		int ret = op(offset, len);
		if (ret > 0) {
			std::lock_guard<std::mutex> lock(mutex_);
			size_ += ret;
		}
		return ret;
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
			int ret = write([this](std::size_t offset, std::size_t len) {
				return ::read(fd_, buffer_ + offset, len);
			});
			if (ret <= 0) {
				return;
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
			int ret = read([this](std::size_t offset, std::size_t len) {
				return ::write(fd_, buffer_ + offset, len);
			});
			if (ret <= 0 || terminated_) {
				return;
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
	int read(void *buf, std::size_t nbytes_total, std::size_t nbytes_read = 0) {
		while (nbytes_read < nbytes_total) {
			int ret = reader_->read([this, &buf, &nbytes_total, &nbytes_read](std::size_t offset, std::size_t len) {
				std::size_t nbytes_to_copy = std::min(len, nbytes_total - nbytes_read);
				std::memcpy(static_cast<char *>(buf) + nbytes_read, reader_->buffer_ + offset, nbytes_to_copy);
				return nbytes_to_copy;
			});
			if (ret <= 0) {
				break;
			}
			nbytes_read += ret;
			reader_->space_available_.notify_one();
		}
		return nbytes_read;
	}
	int write(const void *buf, std::size_t nbytes_total) {
		std::size_t nbytes_written = 0;
		while (nbytes_written < nbytes_total) {
			int ret = writer_->write([this, &buf, &nbytes_total, &nbytes_written](std::size_t offset, std::size_t len) {
				std::size_t nbytes_to_copy = std::min(len, nbytes_total - nbytes_written);
				std::memcpy(writer_->buffer_ + offset, static_cast<const char *>(buf) + nbytes_written, nbytes_to_copy);
				return nbytes_to_copy;
			});
			if (ret <= 0) {
				break;
			}
			nbytes_written += ret;
			writer_->data_available_.notify_one();
		}
		return nbytes_written;
	}
	int flush() {
		return 0;
	}
	bool read_ready() {
		return reader_->size_ > 0;
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
