#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include "src/utility/socket.h"

namespace piex {

class Socket {
public:
	Socket() = default;
	Socket(Socket &&other) {
		fd_ = other.fd_;
		other.fd_ = -1;
	}
	~Socket() {
		if (fd_ != -1) {
			close();
		}
	}
	void connect(const char *host, const char *port) {
		fd_ = utility::socket::create_socket(host, port, false);
		utility::socket::enable_option(fd_, IPPROTO_TCP, TCP_NODELAY);
	}
	void listen(const char *host, const char *port) {
		fd_ = utility::socket::create_socket(host, port, true);
		utility::socket::enable_option(fd_, IPPROTO_TCP, TCP_NODELAY);
		int ret = ::listen(fd_, 0);
		if (ret < 0) {
			throw std::runtime_error(std::strerror(errno));
		}
	}
	int read(void *buf, size_t nbytes_total, size_t nbytes_read = 0) {
		while (nbytes_read < nbytes_total) {
			if (read_buf_size_ == 0) {
				int ret = ::read(fd_, read_buf_, PIEX_OPTION_SOCKET_BUFFER_SIZE);
				if (!ret) {
					return nbytes_read;
				}
				read_buf_cursor_ = 0;
				read_buf_size_ = ret;
			}
			size_t nbytes_to_copy = std::min({nbytes_total - nbytes_read, read_buf_size_, PIEX_OPTION_SOCKET_BUFFER_SIZE - read_buf_cursor_});
			std::memcpy(static_cast<char *>(buf) + nbytes_read, read_buf_ + read_buf_cursor_, nbytes_to_copy);
			nbytes_read += nbytes_to_copy;
			read_buf_cursor_ += nbytes_to_copy;
			read_buf_cursor_ %= PIEX_OPTION_SOCKET_BUFFER_SIZE;
			read_buf_size_ -= nbytes_to_copy;
		}
		return nbytes_total;
	}
	int write(const void *buf, size_t nbytes) {
		if (write_buf_size_ + nbytes >= PIEX_OPTION_SOCKET_BUFFER_SIZE) {
			flush();
		}
		if (write_buf_size_ + nbytes >= PIEX_OPTION_SOCKET_BUFFER_SIZE) {
			::write(fd_, buf, nbytes);
		} else {
			std::memcpy(write_buf_ + write_buf_size_, buf, nbytes);
			write_buf_size_ += nbytes;
		}
		return nbytes;
	}
	int flush() {
		size_t nbytes_written = ::write(fd_, write_buf_, write_buf_size_);
		write_buf_size_ = 0;
		return nbytes_written;
	}
	bool read_ready() {
		if (read_buf_size_) {
			return true;
		}
		fd_set rfds;
		struct timeval tv = {0, 0};
		FD_ZERO(&rfds);
		FD_SET(fd_, &rfds);
		int ret = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
		return ret > 0;
	}
	void close() {
		::close(fd_);
		fd_ = -1;
	}
	Socket accept() {
		Socket socket;
		socket.fd_ = ::accept(fd_, nullptr, nullptr);
		return socket;
	}
private:
	int fd_ = -1;
	char read_buf_[PIEX_OPTION_SOCKET_BUFFER_SIZE];
	char write_buf_[PIEX_OPTION_SOCKET_BUFFER_SIZE];
	size_t read_buf_cursor_ = 0, read_buf_size_ = 0;
	size_t write_buf_size_ = 0;
};

}
