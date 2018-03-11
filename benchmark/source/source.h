#ifndef PIEX_HEADER_BENCHMARK_SOURCE_SOURCE
#define PIEX_HEADER_BENCHMARK_SOURCE_SOURCE

namespace piex {
namespace benchmark {
namespace source {

template <class Handler>
class Source {
public:
	virtual ~Source() = default;
	virtual void yield() = 0;
};

}
}
}

#endif
