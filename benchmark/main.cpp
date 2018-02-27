#include <iostream>
#include "benchmark/benchmark.h"

int main(int argc, const char *argv[]) {
	Benchmark benchmark;
	if (argc != 4) {
		std::cerr << "Usage: " << argv[0] << " host port num_of_requests" << std::endl;
		return 1;
	}
	benchmark.start(argv[1], argv[2], atoll(argv[3]));
	std::cout << benchmark.stats();
}
