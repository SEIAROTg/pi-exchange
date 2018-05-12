#include <deque>
#include <boost/intrusive/treap_set.hpp>
#include "src/order/order.h"
#include "src/utility/pool.h"

namespace piex {
namespace order_book {

template <class T>
struct OrderIdComp {
	bool operator()(Order::IdType id, const T &hook) const {
		return id < hook.order().id();
	}
	bool operator()(const T &hook, Order::IdType id) const {
		return hook.order().id() < id;
	}
};

template <class T>
class Hook : public boost::intrusive::bs_set_base_hook<> {
public:
	Hook(const T &order) : order_(order) {}
	friend bool operator<(const Hook &a, const Hook &b) {
		return a.order_.id() < b.order_.id();
	}
	friend bool priority_order(const Hook &a, const Hook &b) {
		return a.order_ < b.order_;
	}
	const T &order() const {
		return order_;
	}
	T &order() {
		return order_;
	}
private:
	T order_;
};

// not thread safe
template <class OrderType>
class OrderBook {
private:
	utility::pool::RawAllocator allocator_;
	utility::pool::StdAllocator<Hook<OrderType>> std_allocator_;
	boost::intrusive::treap_set<Hook<OrderType>> orders_;
public:
	using SizeType = typename decltype(orders_)::size_type;
	OrderBook() :
		allocator_(alignof(Hook<OrderType>) + sizeof(Hook<OrderType>), PIEX_OPTION_ORDER_BOOK_INIT_SIZE),
		std_allocator_(allocator_) {}
	bool empty() const {
		return orders_.empty();
	}
	SizeType size() const {
		return orders_.size();
	}
	const OrderType &top() const {
		// no boundary check for performance
		return orders_.top()->order();
	}
	// O(log n)
	bool insert(const OrderType &order) {
		Hook<OrderType> *ptr = std_allocator_.allocate(1);
		new(ptr) Hook<OrderType>(order);
		orders_.insert(*ptr);
		return true;
	}
	// O(log n)
	void pop() {
		auto it = orders_.top();
		auto &data = *it;
		orders_.erase(it);
		std_allocator_.deallocate(&data, 1);
	}
	// O(log n)
	bool remove(const typename OrderType::IdType &id) {
		auto it = orders_.find(id, OrderIdComp<Hook<OrderType>>());
		if (it == orders_.end()) {
			return false;
		}
		auto &data = *it;
		orders_.erase(it);
		std_allocator_.deallocate(&data, 1);
		return true;
	}
};

}

template <class ...Args>
using OrderBook = order_book::OrderBook<Args...>;

}
