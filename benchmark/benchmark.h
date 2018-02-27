#include <cstdint>
#include <iostream>
#include <random>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include "src/client/client.h"
#include "src/packets/packets.h"
#include "src/order/order.h"
#include "benchmark/stats.h"

using piex::Order;
using piex::BuyOrder;
using piex::SellOrder;
using piex::Response;

class Benchmark {
public:
	Benchmark() : gen(0), client(*this) {}
	void on_place(const Response::Place &response) {
		stats_.add_order(elapsed_time(place_time[response.id()]));
		place_time.erase(response.id());
		--pending_requests;
	}
	void on_cancel(const Response::Cancel &response) {
		stats_.add_order(elapsed_time(cancel_time[response.id()]));
		cancel_time.erase(response.id());
		--pending_requests;
	}
	void on_match(const Response::Match &response) {
		price = response.price();
		Orders<BuyOrder> &buy_orders = std::get<Orders<BuyOrder>>(orders);
		Orders<SellOrder> &sell_orders = std::get<Orders<SellOrder>>(orders);
		auto buy_it = buy_orders.by_id.find(response.buy_id());
		if (buy_it != buy_orders.by_id.end()) {
			if (buy_it->second.quantity() > response.quantity()) {
				buy_it->second.quantity() -= response.quantity();
			} else {
				buy_orders.by_price.erase(buy_it->second);
				buy_orders.by_id.erase(buy_it);
			}
		}
		auto sell_it = sell_orders.by_id.find(response.sell_id());
		if (sell_it != sell_orders.by_id.end()) {
			if (sell_it->second.quantity() > response.quantity()) {
				sell_it->second.quantity() -= response.quantity();
			} else {
				sell_orders.by_price.erase(sell_it->second);
				sell_orders.by_id.erase(sell_it);
			}
		}
		buy_orders.top_price = response.top_sell_price() ?: buy_orders.top_price;
		sell_orders.top_price = response.top_sell_price() ?: sell_orders.top_price;
	}
	void start(const char *host, const char *port, const std::uint64_t num_of_requests) {
		client.connect(host, port);
		stats_.start();
		while (total_requests < num_of_requests) {
			if (pending_requests < REQUEST_WINDOW_SIZE) {
				send_request();
			}
			client.receive_responses();
		}
		while (pending_requests) {
			client.receive_responses();
		}
		stats_.end();
	}
	const Stats &stats() const {
		return stats_;
	}
private:
	constexpr static int REQUEST_WINDOW_SIZE = 100;
	constexpr static int INITIAL_PRICE = 1000000;
	template <class T>
	struct Orders {
		Order::PriceType top_price = INITIAL_PRICE;
		std::unordered_map<Order::IdType, T> by_id;
		std::set<T> by_price;
	};

	std::mt19937_64 gen;
	std::uint64_t pending_requests = 0;
	std::uint64_t total_requests = 0;
	Order::PriceType price = INITIAL_PRICE;

	std::pair<Orders<BuyOrder>, Orders<SellOrder>> orders;
	std::unordered_map<Order::IdType, std::chrono::high_resolution_clock::time_point> place_time;
	std::unordered_map<Order::IdType, std::chrono::high_resolution_clock::time_point> cancel_time;

	piex::Client<Benchmark> client;
	Stats stats_;

	Order::PriceType random_half_poisson(Order::PriceType mean) {
		std::poisson_distribution<Order::PriceType> dis(mean);
		Order::PriceType ret = dis(gen);
		if (ret < mean) {
			ret = 2 * mean - ret - 1;
		}
		return ret - mean;
	}

	Order::QuantityType random_quantity() {
		static std::uniform_int_distribution<Order::QuantityType> dis(1, 100);
		return dis(gen);
	}
	template <class T>
	Order::PriceType random_price() {
		constexpr static Order::PriceType MIN_PRICE = 100;
		Order::PriceType diff = random_half_poisson(INITIAL_PRICE);
		Order::PriceType price = std::get<Orders<T>>(orders).top_price;
		if (std::is_same<T, BuyOrder>()) {
			return std::max(price - diff, MIN_PRICE);
		}
		return price + diff;
	}
	template <class T>
	Order::PriceType match_price() {
		return std::is_same<T, BuyOrder>()
			? std::get<Orders<SellOrder>>(orders).top_price
			: std::get<Orders<BuyOrder>>(orders).top_price;
	}
	Order::IdType next_id() {
		static Order::IdType id = 0;
		return id++;
	}
	void send_request() {
		++pending_requests;
		++total_requests;
		static std::uniform_int_distribution<> random_action(0, 3);
		static std::uniform_int_distribution<> random_buysell(0, 1);
		int buysell = random_buysell(gen);
		switch (random_action(gen)) {
		case 0: // new
			[[fallthrough]];
		case 1:
			buysell == 0 ? place_new<BuyOrder>() : place_new<SellOrder>();
			break;
		case 2: // match
			buysell == 0 ? place_match<BuyOrder>() : place_match<SellOrder>();
			break;
		case 3: // cancel
			buysell == 0 ? cancel<BuyOrder>() : cancel<SellOrder>();
			break;
		}
	}
	template <class T>
	void place(const T &order) {
		place_time.insert({order.id(), std::chrono::high_resolution_clock::now()});
		client.place(order);
		std::get<Orders<T>>(orders).by_id.insert({order.id(), order});
		std::get<Orders<T>>(orders).by_price.insert(order);
	}
	template <class T>
	void place_new() {
		place<T>({next_id(), random_price<T>(), random_quantity()});
	}
	template <class T>
	void place_match() {
		place<T>({next_id(), match_price<T>(), random_quantity()});
	}
	template <class T>
	void cancel() {
		auto &by_price = std::get<Orders<T>>(orders).by_price;
		if (by_price.empty()) {
			--pending_requests;
			--total_requests;
			return;
		}
		Order::PriceType bottom_price = by_price.rbegin()->price();
		Order::PriceType top_price = by_price.begin()->price();
		Order::PriceType price = std::is_same<T, BuyOrder>()
			? std::min(top_price, bottom_price + random_half_poisson(top_price - bottom_price))
			: std::max(top_price, bottom_price - random_half_poisson(bottom_price - top_price));

		auto it = by_price.lower_bound({0, price, 0});
		Order::IdType id = it->id();
		by_price.erase(it);
		std::get<Orders<T>>(orders).by_id.erase(id);
		cancel_time.insert({id, std::chrono::high_resolution_clock::now()});
		client.cancel<T>(id);
	}
	// return duration in 10^(-4) s
	std::uint64_t elapsed_time(std::chrono::high_resolution_clock::time_point tp) {
		return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - tp).count() / 100;
	}
};
