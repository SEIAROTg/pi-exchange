#include <cstdint>
#include "src/order/order.h"

namespace piex {
class Request {
public:
	enum Type : std::uint8_t {
		PLACE,
		CANCEL,
	};

	enum OrderType : std::uint8_t {
		BUY,
		SELL,
	};

	class Header {
	public:
		explicit Header(const Type &type) : type_(type) {}
		bool operator== (const Header &other) const {
			return type_ == other.type_;
		}
		const Type &type() const {
			return type_;
		}
	private:
		Type type_;
	};

	class Place {
	public:
		Place(
			const OrderType &order_type,
			const Order::IdType &id,
			const Order::PriceType &price,
			const Order::QuantityType &quantity)
		: header_(PLACE), order_type_(order_type), order_(id, price, quantity) {}
		Place(const Place &) = default;
		bool operator== (const Place &other) const {
			return order_type_ == other.order_type_
				&& order_ == other.order_;
		}
		const OrderType &order_type() const {
			return order_type_;
		}
		const Order &order() const {
			return order_;
		}
	private:
		Header header_;
		OrderType order_type_;
		Order order_;
	};

	class Cancel {
	public:
		Cancel(const OrderType &order_type, const Order::IdType &id) :
			header_(CANCEL), order_type_(order_type), id_(id) {
		}
		Cancel(const Cancel &) = default;
		bool operator== (const Cancel &other) const {
			return order_type_ == other.order_type_
				&& id_ == other.id_;
		}
		const OrderType &order_type() const {
			return order_type_;
		}
		const Order::IdType &id() const {
			return id_;
		}
	private:
		Header header_;
		OrderType order_type_;
		Order::IdType id_;
	};

	using Data = union {
		Header header;
		Place place;
		Cancel cancel;
	};

	Data &data() {
		return data_;
	}
private:
	Data data_;
};

class Response {
public:
	enum Type : std::uint8_t {
		PLACE,
		CANCEL,
		MATCH,
	};

	class Header {
	public:
		explicit Header(const Type &type) : type_(type) {}
		bool operator== (const Header &other) const {
			return type_ == other.type_;
		}
		const Type &type() const {
			return type_;
		}
	private:
		Type type_;
	};

	class Place {
	public:
		Place(bool success, const Order::IdType &id) :
			header_(PLACE),
			success_(success),
			id_(id) {
		}
		Place(const Place &) = default;
		bool operator== (const Place &other) const {
			return success_ == other.success_
				&& id_ == other.id_;
		}
		bool success() const {
			return success_;
		}
		const Order::IdType &id() const {
			return id_;
		}
	private:
		Header header_;
		bool success_;
		Order::IdType id_;
	};

	class Cancel {
	public:
		Cancel(bool success, const Order::IdType &id) :
			header_(CANCEL),
			success_(success),
			id_(id) {
		}
		Cancel(const Cancel &) = default;
		bool operator== (const Cancel &other) const {
			return success_ == other.success_
				&& id_ == other.id_;
		}
		bool success() const {
			return success_;
		}
		const Order::IdType &id() const {
			return id_;
		}
	private:
		Header header_;
		bool success_;
		Order::IdType id_;
	};

	class Match {
	public:
		Match(
			const Order::IdType &buy_id,
			const Order::IdType &sell_id,
			const Order::PriceType &price,
			const Order::QuantityType &quantity,
			const Order::PriceType &top_buy_price,
			const Order::PriceType &top_sell_price)
		:
			header_(MATCH),
			buy_id_(buy_id),
			sell_id_(sell_id),
			price_(price),
			quantity_(quantity),
			top_buy_price_(top_buy_price),
			top_sell_price_(top_sell_price) {
		}
		Match(
			const BuyOrder &first,
			const SellOrder &second,
			const Order::QuantityType &quantity,
			const Order::PriceType &top_buy_price,
			const Order::PriceType &top_sell_price)
		: Match(first.id(), second.id(), first.price(), quantity, top_buy_price, top_sell_price) {}
		Match(
			const SellOrder &first,
			const BuyOrder &second,
			const Order::QuantityType &quantity,
			const Order::PriceType &top_sell_price,
			const Order::PriceType &top_buy_price)
		: Match(second.id(), first.id(), first.price(), quantity, top_buy_price, top_sell_price) {}
		Match(const Match &) = default;
		bool operator== (const Match &other) const {
			return buy_id_ == other.buy_id_
				&& sell_id_ == other.sell_id_
				&& price_ == other.price_
				&& quantity_ == other.quantity_
				&& top_buy_price_ == other.top_buy_price_
				&& top_sell_price_ == other.top_sell_price_;
		}
		const Order::IdType &buy_id() const {
			return buy_id_;
		}
		const Order::IdType &sell_id() const {
			return sell_id_;
		}
		const Order::PriceType &price() const {
			return price_;
		}
		const Order::QuantityType &quantity() const {
			return quantity_;
		}
		const Order::PriceType &top_buy_price() const {
			return top_buy_price_;
		}
		const Order::PriceType &top_sell_price() const {
			return top_sell_price_;
		}
	private:
		Header header_;
		Order::IdType buy_id_, sell_id_;
		Order::PriceType price_;
		Order::QuantityType quantity_;
		Order::PriceType top_buy_price_;
		Order::PriceType top_sell_price_;
	};

	using Data = union {
		Header header;
		Place place;
		Cancel cancel;
		Match match;
	};

	Data &data() {
		return data_;
	}
private:
	Data data_;
};
}
