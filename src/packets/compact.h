#include <cstdint>
#include "src/order/order.h"
#include "src/utility/bits.h"

// must be little-endian

namespace piex {

class Request {
public:
	enum Type : std::uint8_t {
		PLACE = 0b00,
		CANCEL = 0b01,
		FLUSH = 0b10,
	};
	enum OrderType : std::uint8_t {
		BUY = 0b0,
		SELL = 0b1,
	};

	class Header {
	public:
		Header() = delete;
		bool operator== (const Header &other) const {
			return type() == other.type();
		}
		Type type() const {
			return extract_type(data_);
		}
	private:
		std::uint8_t data_;
	};

	class Place {
	public:
		Place(
			const OrderType &order_type,
			const Order::IdType &id,
			const Order::PriceType &price,
			const Order::QuantityType &quantity)
		: order_(construct_id(PLACE, order_type, id), price, quantity) {}
		Place(const Place &) = default;
		bool operator== (const Place &other) const {
			return order_ == other.order_;
		}
		OrderType order_type() const {
			return extract_order_type(order_.id());
		}
		Order order() const {
			return {
				extract_id(order_.id()),
				order_.price(),
				order_.quantity()
			};
		}
	private:
		Order order_;
	};

	class Cancel {
	public:
		Cancel(const OrderType &order_type, const Order::IdType &id)
			: id_(construct_id(CANCEL, order_type, id)) {}
		Cancel(const Cancel &) = default;
		bool operator== (const Cancel &other) const {
			return id_ == other.id_;
		}
		OrderType order_type() const {
			return extract_order_type(id_);
		}
		Order::IdType id() const {
			return extract_id(id_);
		}
	private:
		Order::IdType id_;
	};

	class Flush {
	public:
		Flush() : header_(static_cast<Order::IdType>(FLUSH)) {};
	private:
		[[maybe_unused]] std::uint8_t header_;
	};

	using Data = union {
		Header header;
		Place place;
		Cancel cancel;
		Flush flush;
	};

	Data &data() {
		return data_;
	}
private:
	Data data_;
	static constexpr std::size_t TYPE_BITWIDTH = 2;
	static constexpr std::size_t ORDER_TYPE_BITWIDTH = 1;
	static Order::IdType extract_id(Order::IdType id) {
		return utility::bits::discard_bits<TYPE_BITWIDTH + ORDER_TYPE_BITWIDTH>(id);
	}
	static Type extract_type(Order::IdType id) {
		return static_cast<Type>(utility::bits::extract_bits<TYPE_BITWIDTH>(id));
	}
	static OrderType extract_order_type(Order::IdType id) {
		return static_cast<OrderType>(utility::bits::extract_bits<ORDER_TYPE_BITWIDTH, TYPE_BITWIDTH>(id));
	}
	static Order::IdType construct_id(Type type, OrderType order_type, Order::IdType id) {
		id <<= ORDER_TYPE_BITWIDTH;
		id |= static_cast<Order::IdType>(order_type);
		id <<= TYPE_BITWIDTH;
		id |= static_cast<Order::IdType>(type);
		return id;
	}
};

class Response {
public:
	enum Type : std::uint8_t {
		PLACE = 0b00,
		CANCEL = 0b01,
		MATCH = 0b10,
	};

	class Header {
	public:
		Header() = delete;
		bool operator== (const Header &other) const {
			return type() == other.type();
		}
		Type type() const {
			return extract_type(data_);
		}
	private:
		std::uint8_t data_;
	};

	class Place {
	public:
		Place(bool success, const Order::IdType &id)
			: id_(construct_id(PLACE, success, id)) {}
		Place(const Place &) = default;
		bool operator== (const Place &other) const {
			return id_ == other.id_;
		}
		bool success() const {
			return extract_success(id_);
		}
		Order::IdType id() const {
			return extract_id(id_);
		}
	private:
		Order::IdType id_;
	};

	class Cancel {
	public:
		Cancel(bool success, const Order::IdType &id)
			: id_(construct_id(CANCEL, success, id)) {}
		Cancel(const Cancel &) = default;
		bool operator== (const Cancel &other) const {
			return id_ == other.id_;
		}
		bool success() const {
			return extract_success(id_);
		}
		Order::IdType id() const {
			return extract_id(id_);
		}
	private:
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
			buy_id_(construct_id(MATCH, true, buy_id)),
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
		Order::IdType buy_id() const {
			return extract_id(buy_id_);
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
	static constexpr std::size_t TYPE_BITWIDTH = 2;
	static constexpr std::size_t SUCCESS_BITWIDTH = 1;
	static Order::IdType extract_id(Order::IdType id) {
		return utility::bits::discard_bits<TYPE_BITWIDTH + SUCCESS_BITWIDTH>(id);
	}
	static Type extract_type(Order::IdType id) {
		return static_cast<Type>(utility::bits::extract_bits<TYPE_BITWIDTH>(id));
	}
	static bool extract_success(Order::IdType id) {
		return static_cast<bool>(utility::bits::extract_bits<SUCCESS_BITWIDTH, TYPE_BITWIDTH>(id));
	}
	static Order::IdType construct_id(Type type, bool success, Order::IdType id) {
		id <<= SUCCESS_BITWIDTH;
		id |= static_cast<Order::IdType>(success);
		id <<= TYPE_BITWIDTH;
		id |= static_cast<Order::IdType>(type);
		return id;
	}
};

}
