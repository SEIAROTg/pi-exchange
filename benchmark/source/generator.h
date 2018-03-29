#ifndef PIEX_HEADER_BENCHMARK_SOURCE_GENERATOR
#define PIEX_HEADER_BENCHMARK_SOURCE_GENERATOR

#include <cstdint>
#include <utility>
#include <random>
#include <set>
#include <unordered_map>
#include <algorithm>
#include "src/packets/packets.h"
#include "src/order/order.h"
#include "benchmark/source/source.h"

namespace piex {
namespace benchmark {
namespace source {
namespace generator {

constexpr static int INITIAL_PRICE = 1000000;

template <class O>
struct OppositeOrder {};

template <>
struct OppositeOrder<BuyOrder> {
	using Type = SellOrder;
};

template <>
struct OppositeOrder<SellOrder> {
	using Type = BuyOrder;
};

template <class O>
class Orders : public std::set<O> {
public:
	Order::PriceType top_price() {
		return this->empty() ? INITIAL_PRICE : this->begin()->price();
	}
};

template <class Handler>
class Generator : public Source<Handler> {
public:
	Generator(Handler &handler, std::uint64_t book_size) : handler_(handler), book_size_(book_size), gen_(0) {}

	// generate random requests
	void yield() {
		bool buysell = random_buysell();
		if (random_action_match()) {
			// place match
			buysell ? place_match<BuyOrder>() : place_match<SellOrder>();
		} else if (random_action()) {
			// place new
			buysell ? place_new<BuyOrder>() : place_new<SellOrder>();
		} else {
			// cancel
			// opposite probability distribution from place
			!buysell ? cancel<BuyOrder>() : cancel<SellOrder>();
		}
	}
private:
	Handler &handler_;
	const std::uint64_t book_size_;
	std::mt19937_64 gen_;
	std::pair<Orders<BuyOrder>, Orders<SellOrder>> orders_;
	Order::IdType id_ = 0;

	Order::IdType next_id() {
		return id_++;
	}

	// generate a random integer
	// probability: p(x) = poi(mean + x) + poi(mean - 1 - x)
	//     where poi is the probability density function of possion distribution
	Order::PriceType random_half_poisson(Order::PriceType mean) {
		std::poisson_distribution<Order::PriceType> dis(mean);
		Order::PriceType ret = dis(gen_);
		if (ret < mean) {
			ret = 2 * mean - ret - 1;
		}
		return ret - mean;
	}

	// generate random trend of order numbers: increase(true) or decrease(false)
	// probability: p(increase) / p(decrease) = book_size_ / (buy_book_size + sell_book_size)
	int random_action() {
		auto buy_size = std::get<Orders<BuyOrder>>(orders_).size();
		auto sell_size = std::get<Orders<SellOrder>>(orders_).size();
		std::uniform_int_distribution<unsigned> dis(0, buy_size + sell_size + book_size_);
		return dis(gen_) < book_size_;
	}

	// generate random action whether to put a immediately matched order
	// probability: p(match) = 1/4
	bool random_action_match() {
		static std::uniform_int_distribution<> dis(0, 3);
		return dis(gen_) == 0;
	}

	// generate random order type: buy(true) or sell(false)
	// probability: p(buy) / p(sell) = sell_book_size / buy_book_size
	bool random_buysell() {
		auto buy_size = std::get<Orders<BuyOrder>>(orders_).size();
		auto sell_size = std::get<Orders<SellOrder>>(orders_).size();
		std::uniform_int_distribution<unsigned> dis(0, buy_size + sell_size);
		return dis(gen_) < sell_size;
	}

	// generate random order quantity
	// probability: uniform in [1, 100]
	Order::QuantityType random_quantity() {
		static std::uniform_int_distribution<Order::QuantityType> dis(1, 100);
		return dis(gen_);
	}

	// generate random order price that will not be matched immediately
	// T can be BuyOrder or SellOrder
	template <class O>
	Order::PriceType random_price() {
		constexpr static Order::PriceType MIN_PRICE = 100;
		Order::PriceType diff = random_half_poisson(INITIAL_PRICE);
		Order::PriceType price = std::get<Orders<O>>(orders_).top_price();
		return std::is_same<O, BuyOrder>() ? std::max(price - diff, MIN_PRICE) : price + diff;
	}

	// generate a order price that will be matched immediately
	// T can be BuyOrder or SellOrder
	template <class O>
	Order::PriceType match_price() {
		return std::is_same<O, BuyOrder>()
			? std::get<Orders<SellOrder>>(orders_).top_price()
			: std::get<Orders<BuyOrder>>(orders_).top_price();
	}

	// generate random order price as an indication to cancel orders_
	template <class O>
	Order::PriceType random_cancel_price() {
		Order::PriceType bottom_price = std::get<Orders<O>>(orders_).rbegin()->price();
		Order::PriceType top_price = std::get<Orders<O>>(orders_).top_price();
		return std::is_same<O, BuyOrder>()
			? std::min(top_price, bottom_price + random_half_poisson(top_price - bottom_price))
			: std::max(top_price, bottom_price - random_half_poisson(bottom_price - top_price));
	}

	template <class O>
	void match(O &order) {
		auto &opposite_book = std::get<Orders<typename OppositeOrder<O>::Type>>(orders_);
		while (!opposite_book.empty() && opposite_book.begin()->is_compatible_with(order) && order.quantity() > 0) {
			auto matched_order = opposite_book.begin();
			if (order.quantity() >= matched_order->quantity()) {
				order.quantity() -= matched_order->quantity();
				opposite_book.erase(matched_order);
			} else {
				matched_order->quantity() -= order.quantity();
				order.quantity() = 0;
			}
		}
	}

	template <class O>
	void place(const O &order) {
		Request::Place request(
			std::is_same<O, BuyOrder>() ? Request::BUY : Request::SELL,
			order.id(),
			order.price(),
			order.quantity());
		O matched = order;
		match(matched);
		if (matched.quantity() > 0) {
			std::get<Orders<O>>(orders_).insert(matched);
		}
		handler_.process(request);
	}

	template <class O>
	void place_new() {
		place<O>({next_id(), random_price<O>(), random_quantity()});
	}

	template <class O>
	void place_match() {
		place<O>({next_id(), match_price<O>(), random_quantity()});
	}

	template <class O>
	void cancel() {
		auto &book = std::get<Orders<O>>(orders_);
		if (book.empty()) {
			return;
		}
		Order::PriceType price = random_cancel_price<O>();
		auto it = book.lower_bound({0, price, 0});
		Order::IdType id = it->id();
		book.erase(it);
		Request::Cancel request(
			std::is_same<O, BuyOrder>() ? Request::BUY : Request::SELL,
			id);
		handler_.process(request);
	}
};

}

using generator::Generator;

}
}
}

#endif
