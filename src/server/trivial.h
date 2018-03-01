#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <memory>
#include "src/exchange/exchange.h"
#include "src/packets/packets.h"
#include "src/socket/socket.h"

namespace piex {
class Server {
public:
	Server() : exchange_(*this) {}

	// assume clients always send legitimate data
	void listen(const char *host, const char *port) {
		sck_listen.listen(host, port);
		std::uint8_t buf[sizeof(Request)];
		Request &request = *reinterpret_cast<Request *>(buf);
		while (true) {
			socket = std::make_unique<Socket>(sck_listen.accept());
			while (true) {
				int ret = socket->read(&request.data(), sizeof(Request::Header));
				if (!ret) {
					break;
				}
				switch (request.data().header.type()) {
				case Request::PLACE:
					socket->read(&request.data(), sizeof(Request::Place), sizeof(Request::Header));
					exchange_.process_request(request.data().place);
					break;
				case Request::CANCEL:
					socket->read(&request.data(), sizeof(Request::Cancel), sizeof(Request::Header));
					exchange_.process_request(request.data().cancel);
					break;
				}
			}
		}
	}
	void on_place(const Response::Place &response) {
		socket->write(&response, sizeof(response));
	}
	void on_cancel(const Response::Cancel &response) {
		socket->write(&response, sizeof(response));
	}
	void on_match(const Response::Match &response) {
		socket->write(&response, sizeof(response));
	}
private:
	Exchange<Server> exchange_;
	Socket sck_listen;
	std::unique_ptr<Socket> socket;
};
}
