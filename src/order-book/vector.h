#include <vector>
#include <algorithm>
#include "src/order/order.h"

namespace piex {

/// \remark Not thread safe
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
	
	/// \remarks The behavior is undefined if the order book is empty
	/// \complexity O(1)
	OrderType &top() {
		return *orders_.rbegin();
	}
	const OrderType &top() const {
		return *orders_.rbegin();
	}

	/// \complexity O(n)
	bool insert(const OrderType &order) {
		auto iter = std::lower_bound(orders_.rbegin(), orders_.rend(), order);
		orders_.insert(iter.base(), order);
		return true;
	}

	/// \complexity O(1)
	void pop() {
		orders_.pop_back();
	}

	/// \complexity O(n)
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
