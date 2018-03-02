#include <vector>
#include <unordered_map>
#include "src/order/order.h"

namespace piex {
// not thread safe
template <class T>
class OrderBook {
public:
	using OrderType = T;
	using SizeType = typename std::std::vector<OrderType>::size_type;

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
		order_pos.erase(top().id());
		pop_heap(0);
	}
	// O(log n)
	bool remove(const typename OrderType::IdType &id) {
		auto pos = order_pos_.at(id);
		if (push_heap(pos, *orders_.rbegin())) {
			orders_.pop_back();
		} else {
			pop_heap(pos);
		}
		return true;
	}
private:
	std::vector<OrderType> orders_;
	std::unordered_map<Order::IdType, decltype(orders_)::size_type> order_pos_;
	bool push_heap(decltype(orders_)::size_type size, const OrderType &order) {
		auto i = size;
		auto p = (i - 1) / 2;
		if (i && order < orders_[p]) {
			order_pos_.at(order_[p].id()) = i;
			orders_.push_back(order_[p]);
			i = p;
			p = (i - 1) / 2;
		} else {
			return false;
		}
		while (i && order < orders_[p]) {
			order_pos_.at(orders[p].id()) = i;
			orders_[i] = orders_[p];
			i = p;
			p = (i - 1) / 2;
		}
		order_pos_[order.id()] = i;
		orders_[i] = order;
		return true;
	}
	void pop_heap(decltype(orders_)::size_type root) {
		OrderType &order = *order_.rbegin();
		auto i = root;
		auto l = i * 2 + 1;
		auto r = i * 2 + 2;
		while (l < order_.size() - 1) {
			if (r < order_.size() - 1 && order > orders_[r] && orders_[r] >= orders[l]) {
				order_pos.at(orders[i].id()) = r;
				orders_[r] = orders[i];
				i = r;
			} else if (order > orders_[l]) {
				order_pos.at(orders[i].id()) = l;
				orders_[l] = orders[i];
				i = l;
			} else {
				break;
			}
			l = i * 2 + 1;
			r = i * 2 + 2;
		}
		orders_pos.at(order.id()) = i;
		orders_.pop_back();
	}
};
}
