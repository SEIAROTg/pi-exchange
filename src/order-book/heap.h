#include <deque>
#include "src/order/order.h"
#include "src/utility/pool.h"

namespace piex {
// not thread safe
template <class T>
class OrderBook {
public:
	using OrderType = T;
	using SizeType = typename std::deque<OrderType>::size_type;

	OrderBook() :
		pooled_orders_(PIEX_OPTION_ORDER_BOOK_INIT_SIZE / 2),
		orders_(pooled_orders_.container()),
		pooled_order_pos_(PIEX_OPTION_ORDER_BOOK_INIT_SIZE / 2),
		order_pos_(pooled_order_pos_.container()) {}
	bool empty() const {
		return orders_.empty();
	}
	SizeType size() const {
		return orders_.size();
	}

	/// \returns The order with highest priority
	/// \remarks The behavior is undefined if the order book is empty
	/// \complexity O(1)
	const OrderType &top() const {
		return *orders_.begin();
	}
	
	/// \effects Insert an order into the book
	/// \param order The order to insert
	/// \complexity O(log n)
	/// \returns bool indicating whether the insertion is successful
	bool insert(const OrderType &order) {
		if (!push_heap(orders_.size(), order)) {
			order_pos_[order.id()] = orders_.size();
			orders_.push_back(order);
		}
		return true;
	}

	/// \effects Pop the order with highest priority from the book
	/// \remarks The behavior is undefined if the order book is empty
	/// \complexity O(log n)
	void pop() {
		pop_heap(0);
	}

	/// \effects Remove an order by id from the book
	/// \param id The id of the order to remove
	/// \complexity O(log n)
	/// \returns bool indicating whether the order is found and removed
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
	utility::pool::deque<OrderType> pooled_orders_;
	decltype(pooled_orders_.container()) &orders_;
	utility::pool::unordered_map<Order::IdType, SizeType> pooled_order_pos_;
	decltype(pooled_order_pos_.container()) &order_pos_;

	/// \effects Push a order into indexed subheap
	/// \param size The size of the subheap [0..size - 1]
	/// \param order The order to insert
	/// \complexity O(log n)
	/// \returns Returns false if the order has lower priority then the last element in the heap. Otherwise returns true.
	/// \remarks The order is not inserted into the book if the return value is false
	bool push_heap(SizeType size, const OrderType &order) {
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
	/// \effects Pop the order at the peak of indexed subheap
	/// \param root The root of the subheap [root..orders_.size() - 1]
	/// \complexity O(log n)
	void pop_heap(SizeType root) {
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
