#include "src/order/order.h"
#include "src/utility/pool.h"

namespace piex {
// not thread safe
template <class T>
class OrderBook {
public:
	using OrderType = T;
	using SizeType = typename std::set<OrderType>::size_type;

	OrderBook() :
		pooled_orders_(PIEX_OPTION_ORDER_BOOK_INIT_SIZE / 2),
		pooled_order_prices_(PIEX_OPTION_ORDER_BOOK_INIT_SIZE/ 2) {}
	bool empty() const {
		return orders_.empty();
	}
	SizeType size() const {
		return orders_.size();
	}
	const OrderType &top() const {
		// no boundary check for performance
		return *orders_.begin();
	}
	// O(log n)
	bool insert(const OrderType &order) {
		order_prices_.insert({order.id(), order.price()});
		orders_.insert(order);
		return true;
	}
	// O(log n)
	void pop() {
		Order order = top();
		orders_.erase(orders_.begin());
		order_prices_.erase(order.id());
	}
	// O(log n)
	bool remove(const typename OrderType::IdType &id) {
		auto it = order_prices_.find(id);
		if (it == order_prices_.end()) {
			return false;
		}
		Order::PriceType price = it->second;
		order_prices_.erase(it);
		orders_.erase({id, price, 0});
		return true;
	}

private:
	utility::pool::set<OrderType> pooled_orders_;
	decltype(pooled_orders_.container()) &orders_ = pooled_orders_.container();
	utility::pool::unordered_map<Order::IdType, Order::PriceType> pooled_order_prices_;
	decltype(pooled_order_prices_.container()) &order_prices_ = pooled_order_prices_.container();
};

}
