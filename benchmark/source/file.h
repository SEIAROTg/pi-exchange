#ifndef PIEX_HEADER_BENCHMARK_SOURCE_FILE
#define PIEX_HEADER_BENCHMARK_SOURCE_FILE

#include <string>
#include <fstream>
#include <stdexcept>
#include "src/packets/packets.h"
#include "benchmark/source/source.h"

namespace piex {
namespace benchmark {
namespace source {

template <class Handler>
class File : public Source<Handler> {
public:
	File(Handler &handler, const std::string path) :
		handler_(handler),
		file_(path, std::ios_base::in | std::ios_base::binary)
	{
		if (!file_.is_open()) {
			throw std::invalid_argument("cannot open file \"" + path + "\"");
		}
	}
	void yield() {
		file_.read(buf_, sizeof(Request::Header));
		switch (request_.data().header.type()) {
		case Request::PLACE:
			file_.read(buf_ + sizeof(Request::Header), sizeof(Request::Place) - sizeof(Request::Header));
			handler_.process(request_.data().place);
			break;
		case Request::CANCEL:
			file_.read(buf_ + sizeof(Request::Header), sizeof(Request::Cancel) - sizeof(Request::Header));
			handler_.process(request_.data().cancel);
			break;
		case Request::FLUSH:
			break;
		}
	}
private:
	Handler &handler_;
	char buf_[sizeof(Request)];
	Request &request_ = *reinterpret_cast<Request *>(buf_);
	std::fstream file_;
};

}
}
}

#endif
