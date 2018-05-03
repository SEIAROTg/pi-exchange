#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
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
	}
	void listen(const char *host, const char *port) {
		fd_ = utility::socket::create_socket(host, port, true);
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
};

}
