#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <memory>
#include <iostream>
#include "benchmark/source/source.h"
#include "benchmark/source/generator.h"
#include "benchmark/source/file.h"
#include "benchmark/destination/destination.h"
#include "benchmark/destination/exchange.h"
#include "benchmark/destination/server.h"
#include "benchmark/destination/file.h"
#include "benchmark/benchmark.h"

using namespace piex::benchmark;

void error(const char *prog) {
	std::cerr
		<< "Usage: " << prog << "source destination book_size num_of_requests" << std::endl
		<< std::endl
		<< "    source           \"generator\" or file path" << std::endl
		<< "    destination      \"exchange\", host:port or file path" << std::endl
		<< "    book_size        an integer" << std::endl
		<< "    num_of_requests  an integer" << std::endl;
	std::exit(1);
}

int main(int argc, const char *argv[]) {
	if (argc != 5) {
		error(argv[0]);
	}

	char *end;
	std::uint64_t book_size = strtoull(argv[3], &end, 0);
	if (*end != '\0') {
		error(argv[0]);
	}
	std::uint64_t num_of_requests = strtoull(argv[4], &end, 0);
	if (*end != '\0') {
		error(argv[0]);
	}

	Benchmark benchmark;
	std::unique_ptr<source::Source<Benchmark>> source;
	std::unique_ptr<destination::Destination<Benchmark>> destination;

	if (std::strcmp(argv[1], "generator") == 0) {
		source = std::make_unique<source::Generator<Benchmark>>(benchmark, book_size);
	} else {
		source = std::make_unique<source::File<Benchmark>>(benchmark, argv[1]);
	}

	if (std::strcmp(argv[2], "exchange") == 0) {
		destination = std::make_unique<destination::Exchange<Benchmark>>(benchmark);
	} else {
		const char *pos = std::strchr(argv[2], ':');
		if (pos != nullptr) {
			std::string host(argv[2], pos - argv[2]);
			std::string port(pos + 1);
			destination = std::make_unique<destination::Server<Benchmark>>(benchmark, host.c_str(), port.c_str());
		} else {
			destination = std::make_unique<destination::File<Benchmark>>(benchmark, argv[2]);
		}
	}

	benchmark.start(source.get(), destination.get(), num_of_requests);
	std::cout << benchmark.stats() << std::endl;
}
