#include <vector>
#include <unordered_map>
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
	const OrderType &top() const {
		// no boundary check for performance
		return *orders_.begin();
	}
	// O(log n)
	bool insert(const OrderType &order) {
		if (!push_heap(orders_.size(), order)) {
			order_pos_[order.id()] = orders_.size();
			orders_.push_back(order);
		}
		return true;
	}
	// O(log n)
	void pop() {
		pop_heap(0);
	}
	// O(log n)
	bool remove(const typename OrderType::IdType &id) {
		auto it = order_pos_.find(id);
		if (it == order_pos_.end()) {
			return false;
		}
		auto pos = it->second;
		if (push_heap(pos, *orders_.rbegin())) {
			orders_.pop_back();
			order_pos_.erase(it);
		} else {
			pop_heap(pos);
		}
		return true;
	}
private:
	std::vector<OrderType> orders_;
	std::unordered_map<Order::IdType, typename decltype(orders_)::size_type> order_pos_;
	bool push_heap(typename decltype(orders_)::size_type size, const OrderType &order) {
		auto i = size;
		auto p = (i - 1) / 2;
		if (i && order < orders_[p]) {
			order_pos_.at(orders_[p].id()) = i;
			if (i == orders_.size()) {
				orders_.push_back(orders_[p]);
			} else {
				orders_[i] = orders_[p];
			}
			i = p;
			p = (i - 1) / 2;
		} else {
			return false;
		}
		while (i && order < orders_[p]) {
			order_pos_.at(orders_[p].id()) = i;
			orders_[i] = orders_[p];
			i = p;
			p = (i - 1) / 2;
		}
		order_pos_[order.id()] = i;
		orders_[i] = order;
		return true;
	}
	void pop_heap(typename decltype(orders_)::size_type root) {
		order_pos_.erase(orders_[root].id());
		OrderType &order = *orders_.rbegin();
		auto i = root;
		auto l = i * 2 + 1;
		auto r = i * 2 + 2;
		while (l < orders_.size() - 1) {
			if (r < orders_.size() - 1 && orders_[r] < order && !(orders_[l] < orders_[r])) {
				orders_[i] = orders_[r];
				order_pos_.at(orders_[i].id()) = i;
				i = r;
			} else if (orders_[l] < order) {
				orders_[i] = orders_[l];
				order_pos_.at(orders_[i].id()) = i;
				i = l;
			} else {
				break;
			}
			l = i * 2 + 1;
			r = i * 2 + 2;
		}
		orders_.pop_back();
		if (i != orders_.size()) {
			orders_[i] = order;
			order_pos_.at(order.id()) = i;
		}
	}
};
}
