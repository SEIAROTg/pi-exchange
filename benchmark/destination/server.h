#ifndef PIEX_HEADER_BENCHMARK_DESTINATION_SERVER
#define PIEX_HEADER_BENCHMARK_DESTINATION_SERVER

#include "src/packets/packets.h"
#include "src/client/client.h"

namespace piex {
namespace benchmark {
namespace destination {

template <class Handler>
class Server : public Destination<Handler> {
public:
	Server(Handler &handler, const char *host, const char *port) : handler_(handler), client_(*this) {
		client_.connect(host, port);
	}
	void process(const Request::Place &request) {
		client_.process(request);
		client_.try_receive_responses();
	}
	void process(const Request::Cancel &request) {
		client_.process(request);
		client_.try_receive_responses();
	}
	void wait_response() {
		client_.receive_response();
	}
	void flush() {
		client_.flush();
	}
	void on_place(const Response::Place &response) {
		handler_.process(response);
	}
	void on_cancel(const Response::Cancel &response) {
		handler_.process(response);
	}
	void on_match(const Response::Match &response) {
		handler_.process(response);
	}
private:
	Handler &handler_;
	piex::Client<Server> client_;
};

}
}
}

#endif
