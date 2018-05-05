#ifndef PIEX_HEADER_UTILITY_CIRCULAR
#define PIEX_HEADER_UTILITY_CIRCULAR

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>

namespace piex {
namespace utility {
namespace circular {

template <class Integral, Integral N>
class circular {
public:
	circular(Integral val = Integral()) : val_(val) {}
	circular<Integral, N> &operator+=(Integral add) {
		val_ += add;
		val_ %= N;
		return *this;
	}
	operator Integral() const {
		return val_;
	}
private:
	Integral val_;
};

template <std::size_t N>
class Buffer {
public:
	ssize_t read_from(int fd, std::size_t offset, std::size_t len) {
		if (offset + len > N) {
			iovec iov[2] = {
				{ buf_ + offset, N - offset },
				{ buf_, offset + len - N },
			};
			return ::readv(fd, iov, 2);
		}
		return ::read(fd, buf_ + offset, len); 
	}
	ssize_t write_to(int fd, std::size_t offset, std::size_t len) {
		if (offset + len > N) {
			iovec iov[2] = {
				{ buf_ + offset, N - offset },
				{ buf_, offset + len - N },
			};
			return ::writev(fd, iov, 2);
		}
		return ::write(fd, buf_ + offset, len); 
	}
	void read(void *buf, std::size_t offset, std::size_t len) {
		if (offset + len > N) {
			std::memcpy(static_cast<char *>(buf), buf_ + offset, N - offset);
			std::memcpy(static_cast<char *>(buf) + N - offset, buf_, offset + len - N);
		} else {
			std::memcpy(static_cast<char *>(buf), buf_ + offset, len);
		}
	}
	void write(const void *buf, std::size_t offset, std::size_t len) {
		if (offset + len > N) {
			std::memcpy(buf_ + offset, static_cast<const char *>(buf), N - offset);
			std::memcpy(buf_, static_cast<const char *>(buf) + N - offset, offset + len - N);
		} else {
			std::memcpy(buf_ + offset, static_cast<const char *>(buf), len);
		}
	}
private:
	char buf_[N];
};

}
}
}

#endif
