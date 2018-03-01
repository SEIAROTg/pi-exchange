#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>

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
		int ret = 1;
		while (nbytes_read < nbytes_total && ret) {
			ret = ::read(fd_, static_cast<char *>(buf) + nbytes_read, nbytes_total - nbytes_read);
			nbytes_read += ret;
		}
		return nbytes_read;
	}
	int write(const void *buf, size_t nbytes) {
		return ::write(fd_, buf, nbytes);
	}
	int flush() {
		return 0;
	}
	bool read_ready() {
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
