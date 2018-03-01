#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <algorithm>

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
		fd_ = get_ready_socket(host, port, false);
	}
	void listen(const char *host, const char *port) {
		fd_ = get_ready_socket(host, port, true);
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
				int fd = socket(cursor->ai_family, cursor->ai_socktype, cursor->ai_protocol);
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
};

}
