#ifndef PIEX_HEADER_BENCHMARK_DESTINATION_FILE
#define PIEX_HEADER_BENCHMARK_DESTINATION_FILE

#include <string>
#include <fstream>
#include <stdexcept>
#include "src/packets/packets.h"
#include "benchmark/destination/destination.h"

namespace piex {
namespace benchmark {
namespace destination {

template <class Handler>
class File : public Destination<Handler> {
public:
	File(Handler &handler, const std::string path) :
		handler_(handler),
		file_(path, std::ios_base::out | std::ios_base::binary)
	{
		if (!file_.is_open()) {
			throw std::invalid_argument("cannot open file \"" + path + "\"");
		}
	}
	void process(const Request::Place &request) {
		file_.write(reinterpret_cast<const char *>(&request), sizeof(request));
		handler_.process(Response::Place{true, request.order().id()});
	}
	void process(const Request::Cancel &request) {
		file_.write(reinterpret_cast<const char *>(&request), sizeof(request));
		handler_.process(Response::Cancel{true, request.id()});
	}
private:
	Handler &handler_;
	std::ofstream file_;
};

}
}
}

#endif
