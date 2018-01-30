#ifndef PIEX_HEADER_UTILITIES
#define PIEX_HEADER_UTILITIES

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace piex {
namespace utilities {
int get_ready_socket(const char *host, const char *port, int socktype, bool is_server) {
	int ret;
	addrinfo hints = {
		.ai_flags = is_server ? AI_PASSIVE : 0,
		.ai_family = AF_UNSPEC,
		.ai_socktype = socktype,
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
			ret = (is_server ? bind : connect)(fd, cursor->ai_addr, cursor->ai_addrlen);
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

#endif
