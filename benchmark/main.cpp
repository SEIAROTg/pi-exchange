#include <iostream>
#include "benchmark/benchmark.h"

int main(int argc, const char *argv[]) {
	if (argc != 5) {
		std::cerr << "Usage: " << argv[0] << " host port book_size num_of_requests" << std::endl;
		return 1;
	}
	Benchmark benchmark(atoll(argv[3]));
	benchmark.start(argv[1], argv[2], atoll(argv[4]));
	std::cout << benchmark.stats();
}
