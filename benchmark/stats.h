#include <cstdint>
#include <ostream>
#include <vector>
#include <algorithm>
#include <utility>
#include <chrono>
#include <iomanip>
#include <ios>

class Stats {
public:
	Stats() : latencies(LATENCY_VALUES + 1) {};
	void start() {
		start_time = std::chrono::high_resolution_clock::now();
	}
	void add_order(std::uint64_t latency) {
		max_latency = std::max(max_latency, latency);
		if (latency >= LATENCY_VALUES) {
			latency = LATENCY_VALUES;
		}
		total_latency += latency;
		++latencies[latency];
		++order_num;
	}
	void end() {
		end_time = std::chrono::high_resolution_clock::now();
	}
	double throughput_average() const {
		return static_cast<double>(order_num) * 1000 * 1000 * 1000 / std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
	}
	double latency_average() const {
		return static_cast<double>(total_latency) / order_num;
	}
	std::vector<std::pair<double, std::uint64_t>> latency_distribution() const {
		std::vector<std::pair<double, std::uint64_t>> ret;
		uint64_t cursor = 0;
		std::uint64_t scanned_order_num = 0;
		for (double percentile : PERCENTILES) {
			while (cursor <= LATENCY_VALUES && scanned_order_num * 100 < order_num * percentile) {
				scanned_order_num += latencies[cursor];
				++cursor;
			}
			if (cursor == LATENCY_VALUES + 1) {
				ret.push_back(std::make_pair(percentile, static_cast<uint64_t>(-1)));
			} else {
				ret.push_back(std::make_pair(percentile, cursor - 1));
			}
		}
		ret.push_back(std::make_pair(100, max_latency));
		return ret;
	}
private:
	constexpr static int LATENCY_VALUES = 1000000;
	constexpr static double PERCENTILES[] = {0.1, 1, 5, 25, 50, 75, 95, 99, 99.9, 99.99};
	std::uint64_t order_num = 0;
	std::uint64_t total_latency = 0;
	std::uint64_t max_latency = 0;
	std::vector<std::uint64_t> latencies;
	std::chrono::high_resolution_clock::time_point start_time;
	std::chrono::high_resolution_clock::time_point end_time;
};

std::ostream &operator<<(std::ostream &os, const Stats &stats) {
	std::streamsize precision = std::cout.precision();
	os
		<< std::setprecision(2)
		<< "Throughput:" << std::endl
		<< "    Average: " << std::fixed << stats.throughput_average() << " req/s" << std::endl
		<< std::endl
		<< "Latency:" << std::endl
		<< "    Average: " << std::fixed << stats.latency_average() / 10.0 << " ms" << std::endl
		<< "    Distribution:" << std::endl;
	os << std::setprecision(precision);
	auto distribution = stats.latency_distribution();
	for (auto &entry : distribution) {
		if (entry.second != static_cast<uint64_t>(-1)) {
			os
				<< std::setw(8) << ""
				<< std::setw(5) << std::setprecision(precision) << std::defaultfloat << entry.first << " %  < "
				<< std::setw(8)	<< std::setprecision(1) << std::fixed << entry.second / 10.0 << " ms" << std::endl;
		}
	}
	os << std::setprecision(precision);
	return os;
}
