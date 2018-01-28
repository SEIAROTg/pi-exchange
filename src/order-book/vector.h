#include <vector>
#include <algorithm>
#include "src/order/order.h"

namespace piex {
// not thread safe
template <class T>
class OrderBook {
public:
	using OrderType = T;
	using SizeType = typename std::vector<OrderType>::size_type;

	bool empty() const {
		return orders_.empty();
	}
	SizeType size() const {
		return orders_.size();
	}
	OrderType &top() {
		// no boundary check for performance
		return *orders_.rbegin();
	}
	const OrderType &top() const {
		return *orders_.rbegin();
	}
	// O(n)
	bool insert(const OrderType &order) {
		auto iter = std::lower_bound(orders_.rbegin(), orders_.rend(), order);
		orders_.insert(iter.base(), order);
		return true;
	}
	// O(1)
	void pop() {
		orders_.pop_back();
	}
	// O(n)
	bool remove(const typename OrderType::IdType &id) {
		auto iter = std::find_if(orders_.begin(), orders_.end(), [&id](const OrderType& order) {
			return order.id() == id;
		});
		if (iter != orders_.end()) {
			orders_.erase(iter);
			return true;
		}
		return false;
	}

private:
	std::vector<OrderType> orders_;
};
}
