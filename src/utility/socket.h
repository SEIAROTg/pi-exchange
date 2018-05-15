#ifndef PIEX_HEADER_UTILITY_SOCKET
#define PIEX_HEADER_UTILITY_SOCKET

#include <sys/types.h>
#include <sys/socket.h>

namespace piex {
namespace utility {
namespace socket {

inline int enable_option(int fd, int level, int option_name) {
	int on = 1;
	return setsockopt(fd, level, option_name, &on, sizeof(on));
}

/// \effects Create a socket that has been bind or connected
/// \param host The host to connect / bind
/// \param port The port to connect / bind
/// \param is_server Whether the socket is to bind or to connect
inline int create_socket(const char *host, const char *port, bool is_server) {
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
			enable_option(fd, SOL_SOCKET, SO_REUSEADDR);
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

}
}
}

#endif
