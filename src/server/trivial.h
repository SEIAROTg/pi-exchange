#include <unistd.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include "src/exchange/exchange.h"
#include "src/packets/packets.h"
#include "src/utilities/socket.h"

namespace piex {
class Server {
public:
	Server() : exchange_(*this) {}

	// assume clients always send legitimate data
	void listen(const char *host, const char *port) {
		int sck_listen = utilities::get_ready_socket(host, port, SOCK_STREAM, true);
		int ret;
		ret = ::listen(sck_listen, 0);
		if (ret < 0) {
			throw std::runtime_error(std::strerror(errno));
		}
		std::uint8_t buf[sizeof(Request)];
		Request &request = *reinterpret_cast<Request *>(buf);
		while (true) {
			fd_ = accept(sck_listen, NULL, NULL);
			while (true) {
				ret = utilities::readn(fd_, &request.data(), sizeof(Request::Header));
				if (!ret) {
					break;
				}
				switch (request.data().header.type()) {
				case Request::PLACE:
					utilities::readn(fd_, &request.data(), sizeof(Request::Place), sizeof(Request::Header));
					exchange_.process_request(request.data().place);
					break;
				case Request::CANCEL:
					utilities::readn(fd_, &request.data(), sizeof(Request::Cancel), sizeof(Request::Header));
					exchange_.process_request(request.data().cancel);
					break;
				}
			}
		}
	}
	void on_place(const Response::Place &response) {
		if (fd_ != -1) {
			write(fd_, &response, sizeof(response));
		}
	}
	void on_cancel(const Response::Cancel &response) {
		if (fd_ != -1) {
			write(fd_, &response, sizeof(response));
		}
	}
	void on_match(const Response::Match &response) {
		if (fd_ != -1) {
			write(fd_, &response, sizeof(response));
		}
	}
private:
	Exchange<Server> exchange_;
	int fd_ = -1;
};
}
