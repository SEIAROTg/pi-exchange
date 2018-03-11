#ifndef PIEX_HEADER_BENCHMARK_DESTINATION_EXCHANGE
#define PIEX_HEADER_BENCHMARK_DESTINATION_EXCHANGE

#include "src/packets/packets.h"
#include "src/exchange/exchange.h"

namespace piex {
namespace benchmark {
namespace destination {

template <class Handler>
class Exchange : public Destination<Handler> {
public:
	Exchange(Handler &handler) : handler_(handler), exchange_(*this) {}
	void process(const Request::Place &request) {
		exchange_.process_request(request);
	}
	void process(const Request::Cancel &request) {
		exchange_.process_request(request);
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
	piex::Exchange<Exchange> exchange_;
};

}
}
}

#endif
