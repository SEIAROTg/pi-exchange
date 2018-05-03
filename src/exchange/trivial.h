#include "src/packets/packets.h"
#include "src/order-book/order-book.h"
#include "src/order/order.h"

namespace piex {
template <class T>
class Exchange {
public:
	using EventHandlerType = T;
	explicit Exchange(EventHandlerType &handler) : handler_(handler) {}

	void process_request(const Request::Place &request) {
		if (request.order_type() == Request::BUY) {
			const Order &order = request.order();
			insert_order_to_book(static_cast<const BuyOrder &>(order), buy_book_, sell_book_);
		} else {
			const Order &order = request.order();
			insert_order_to_book(static_cast<const SellOrder &>(order), sell_book_, buy_book_);
		}
	}

	void process_request(const Request::Cancel &request) {
		if (request.order_type() == Request::BUY) {
			remove_order_from_book(request, buy_book_);
		} else {
			remove_order_from_book(request, sell_book_);
		}
	}

private:
	EventHandlerType &handler_;
	OrderBook<BuyOrder> buy_book_;
	OrderBook<SellOrder> sell_book_;
	template <class U, class V>
	void insert_order_to_book(const U &request_order, OrderBook<U> &order_book, OrderBook<V> &opposite_book) {
		U order = request_order;
		bool success = true;
		while (
			!opposite_book.empty()
			&& order.is_compatible_with(opposite_book.top())
			&& order.quantity() > 0
		) {
			if (order.quantity() < opposite_book.top().quantity()) {
				opposite_book.top().quantity() -= order.quantity();
				handler_.on_match({
					opposite_book.top(),
					order,
					order.quantity(),
					opposite_book.empty() ? 0 : opposite_book.top().price(),
					order_book.empty() ? 0 : order_book.top().price()});
				order.quantity() = 0;
				break;
			}
			V opposite_top = opposite_book.top();
			order.quantity() -= opposite_top.quantity();
			opposite_book.pop();
			handler_.on_match({
				opposite_top,
				order,
				opposite_top.quantity(),
				opposite_book.empty() ? 0 : opposite_book.top().price(),
				order.quantity() > 0 ? order.price() : (order_book.empty() ? 0 : order_book.top().price())
			});
		}
		if (order.quantity() > 0) {
			success = order_book.insert(order);
		}
		handler_.on_place({success, order.id()});
	}
	template <class U>
	void remove_order_from_book(const Request::Cancel &request, OrderBook<U> &order_book) {
		bool success = order_book.remove(request.id());
		handler_.on_cancel({success, request.id()});
	}
};
}
