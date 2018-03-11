#ifndef PIEX_HEADER_BENCHMARK_BENCHMARK
#define PIEX_HEADER_BENCHMARK_BENCHMARK

#include <cstdint>
#include <chrono>
#include <unordered_map>
#include "src/order/order.h"
#include "src/packets/packets.h"
#include "benchmark/source/source.h"
#include "benchmark/destination/destination.h"
#include "benchmark/stats.h"

namespace piex {
namespace benchmark {

class Benchmark {
public:
	void start(
		source::Source<Benchmark> *source,
		destination::Destination<Benchmark> *destination,
		std::uint64_t requests_total)
	{
		destination_ = destination;
		requests_submitted_ = 0;
		requests_processed_ = 0;
		stats_.start();

		while (requests_submitted_  < requests_total) {
			while (requests_submitted_ - requests_processed_ > PIEX_OPTION_BENCHMARK_REQUEST_WINDOW_SIZE) {
				destination_->wait_response();
			}
			source->yield();
		}
		destination_->flush();
		while (requests_submitted_ > requests_processed_) {
			destination_->wait_response();
		}
		stats_.end();
	}
	void process(const Request::Place &request) {
		++requests_submitted_;
		place_time_.insert({ request.order().id(), std::chrono::high_resolution_clock::now() });
		destination_->process(request);
	}
	void process(const Request::Cancel &request) {
		++requests_submitted_;
		cancel_time_.insert({ request.id(), std::chrono::high_resolution_clock::now() });
		destination_->process(request);
	}
	void process(const Response::Place &response) {
		++requests_processed_;
		auto it = place_time_.find(response.id());
		stats_.add_entry(elapsed_time(it->second));
		place_time_.erase(it);
	}
	void process(const Response::Cancel &response) {
		++requests_processed_;
		auto it = cancel_time_.find(response.id());
		stats_.add_entry(elapsed_time(it->second));
		cancel_time_.erase(it);
	}
	void process(const Response::Match &) {}
	const Stats &stats() {
		return stats_;
	}
private:
	destination::Destination<Benchmark> *destination_;
	std::uint64_t requests_submitted_, requests_processed_;
	Stats stats_;
	std::unordered_map<Order::IdType, std::chrono::high_resolution_clock::time_point> place_time_, cancel_time_;

	// elapsed time in 10^(-4) s
	static std::uint64_t elapsed_time(std::chrono::high_resolution_clock::time_point tp) {
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - tp).count() / 100;
	}

};

}
}

#endif
