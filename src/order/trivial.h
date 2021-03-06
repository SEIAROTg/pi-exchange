#include <cstdint>

namespace piex {
class Order {
public:
	using IdType = std::uint64_t;
	using PriceType = std::uint32_t;
	using QuantityType = std::uint32_t;

	Order(const IdType &id, const PriceType &price, const QuantityType &quantity) :
		id_(id),
		price_(price),
		quantity_(quantity) {
	}
	Order(const Order &) = default;
	bool operator== (const Order &other) const {
		return id_ == other.id_
			&& price_ == other.price_
			&& quantity_ == other.quantity_;
	}

	const IdType &id() const {
		return id_;
	}
	const PriceType &price() const {
		return price_;
	}
	QuantityType &quantity() const {
		return quantity_;
	}
private:
	IdType id_;
	PriceType price_;
	mutable QuantityType quantity_;
};

class BuyOrder;
class SellOrder;

class BuyOrder : public Order {
public:
	using Order::Order;
	/// \effects Compare the priority with another order
	/// \returns bool indicating this order has higher priority
	bool operator<(const BuyOrder &other) const {
		return (price() > other.price()) || (price() == other.price() && id() < other.id());
	}
	/// \effects Check if two this order is compatiable with another
	/// \returns bool indicating whether the orders are compatiable
	inline bool is_compatible_with(const SellOrder &) const;
};

class SellOrder : public Order {
public:
	using Order::Order;
	/// \effects Compare the priority with another order
	/// \returns bool indicating this order has higher priority
	bool operator<(const SellOrder &other) const {
		return (price() < other.price()) || (price() == other.price() && id() < other.id());
	}
	/// \effects Check if two this order is compatiable with another
	/// \returns bool indicating whether the orders are compatiable
	inline bool is_compatible_with(const BuyOrder &) const;
};

inline bool BuyOrder::is_compatible_with(const SellOrder &sell) const {
	return price() >= sell.price();
}

inline bool SellOrder::is_compatible_with(const BuyOrder &buy) const {
	return price() <= buy.price();
}
}
