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
#include "src/utility/circular.h"
#include "src/nsemaphore/nsemaphore.h"
#include "config/config.h"

namespace piex {
namespace socket {

template <class Semaphore, std::size_t N>
struct Interval {
	Interval(std::size_t size) : size_(size) {}
	utility::circular::circular<std::size_t, N> cursor_ = 0;
	Semaphore size_;
};

template <class DataSemaphore, class SpaceSemaphore>
struct Daemon {
public:
	~Daemon() {
		data_.size_.terminate();
		space_.size_.terminate();
		thread_.join();
	}
	int fd_ = -1;
	utility::circular::Buffer<PIEX_OPTION_SOCKET_BUFFER_SIZE> buffer_;
	Interval<DataSemaphore, PIEX_OPTION_SOCKET_BUFFER_SIZE> data_ = 0;
	Interval<SpaceSemaphore, PIEX_OPTION_SOCKET_BUFFER_SIZE> space_ = PIEX_OPTION_SOCKET_BUFFER_SIZE;
	std::thread thread_;
protected:
	template <class Function, class... Args>
	Daemon(int fd, Function &&f, Args &&...args) :
		fd_(fd),
		thread_(f, args...) {};
};

class ReaderDaemon : public Daemon<nsemaphore::Strict, nsemaphore::Loose> {
public:
	ReaderDaemon(int fd) : Daemon(fd, &ReaderDaemon::body, this) {}
private:
	void body() {
		while (true) {
			space_.size_.wait(PIEX_OPTION_SOCKET_FLUSH_THRESHOLD);
			std::size_t offset = space_.cursor_;
			std::size_t len = space_.size_.load();
			if (len == 0) {
				return;
			}
			ssize_t ret = buffer_.read_from(fd_, offset, len);
			if (ret <= 0) {
				return;
			}
			space_.cursor_ += ret;
			space_.size_.consume(ret);
			data_.size_.post(ret);
		}
	}
};

class WriterDaemon : public Daemon<nsemaphore::Loose, nsemaphore::Strict> {
public:
	WriterDaemon(int fd) : Daemon(fd, &WriterDaemon::body, this) {}
private:
	void body() {
		while (true) {
			data_.size_.wait(PIEX_OPTION_SOCKET_FLUSH_THRESHOLD);
			std::size_t offset = data_.cursor_;
			std::size_t len = data_.size_.load();
			if (len == 0) {
				return;
			}
			ssize_t ret = buffer_.write_to(fd_, offset, len);
			if (ret <= 0) {
				return;
			}
			data_.cursor_ += ret;
			data_.size_.consume(ret);
			space_.size_.post(ret);
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
		std::size_t nbytes_to_read = nbytes_total - nbytes_read;
		reader_->data_.size_.wait(nbytes_to_read);
		std::size_t offset = reader_->data_.cursor_;
		std::size_t len = nbytes_to_read;
		reader_->data_.size_.consume(len);
		if (len == 0) {
			return 0;
		}
		reader_->data_.cursor_ += len;
		reader_->buffer_.read(static_cast<char *>(buf) + nbytes_read, offset, len);
		reader_->space_.size_.post(nbytes_to_read);
		return nbytes_total;
	}
	ssize_t write(const void *buf, std::size_t nbytes_total) {
		writer_->space_.size_.wait(nbytes_total);
		std::size_t offset = writer_->space_.cursor_;
		std::size_t len = nbytes_total;
		writer_->space_.size_.consume(len);
		if (len == 0) {
			return 0;
		}
		writer_->space_.cursor_ += len;
		writer_->buffer_.write(buf, offset, len);
		writer_->data_.size_.post(nbytes_total);
		return nbytes_total;
	}
	int flush() {
		writer_->data_.size_.flush();
		return 0;
	}
	bool read_ready() {
		return reader_->data_.size_.load() > 0;
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
