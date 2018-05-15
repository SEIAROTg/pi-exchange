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
	/// \effects Connect to remote host
	/// \param host Remote host
	/// \param port Port of remote host
	void connect(const char *host, const char *port) {
		fd_ = utility::socket::create_socket(host, port, false);
	}
	/// \effects Listen
	/// \param host Host to listen at
	/// \param port Port to listen at
	void listen(const char *host, const char *port) {
		fd_ = utility::socket::create_socket(host, port, true);
		int ret = ::listen(fd_, 0);
		if (ret < 0) {
			throw std::runtime_error(std::strerror(errno));
		}
	}
	/// \effects Read data from socket
	/// \param buf The buffer to store the read data
	/// \param nbytes_total The total number of bytes to read
	/// \param nbytes_read The number of bytes already read
	/// \returns Total number of read bytes (always nbytes_total on success)
	/// \remarks This function will read (nbytes_total - nbytes_read) bytes of data and place them at (buf + nbytes_read)
	int read(void *buf, size_t nbytes_total, size_t nbytes_read = 0) {
		int ret = 1;
		while (nbytes_read < nbytes_total && ret) {
			ret = ::read(fd_, static_cast<char *>(buf) + nbytes_read, nbytes_total - nbytes_read);
			nbytes_read += ret;
		}
		return nbytes_read;
	}
	/// \effects Write data to socket
	/// \param buf Data to write
	/// \param nbytes Number of bytes to write
	/// \returns The number of bytes written
	int write(const void *buf, size_t nbytes) {
		return ::write(fd_, buf, nbytes);
	}
	/// \effects Send data in buffer immediately
	/// \returns The number of bytes written
	int flush() {
		return 0;
	}
	/// \effects Check if there is available data to read
	bool read_ready() {
		fd_set rfds;
		struct timeval tv = {0, 0};
		FD_ZERO(&rfds);
		FD_SET(fd_, &rfds);
		int ret = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
		return ret > 0;
	}
	/// \effects Close the socket
	void close() {
		::close(fd_);
		fd_ = -1;
	}
	/// \effects Accept a connection
	/// \returns A new connected Socket
	Socket accept() {
		Socket socket;
		socket.fd_ = ::accept(fd_, nullptr, nullptr);
		return socket;
	}
private:
	int fd_ = -1;
};

}
