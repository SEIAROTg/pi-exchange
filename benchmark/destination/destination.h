#ifndef PIEX_HEADER_BENCHMARK_DESTINATION_DESTINATION
#define PIEX_HEADER_BENCHMARK_DESTINATION_DESTINATION

#include "src/packets/packets.h"

namespace piex {
namespace benchmark {
namespace destination {

template <class Handler>
class Destination {
public:
	virtual ~Destination() = default;
	virtual void process(const Request::Place &request) = 0;
	virtual void process(const Request::Cancel &request) = 0;
	virtual void wait_response() {}
	virtual void flush() {}
};

}
}
}

#endif
