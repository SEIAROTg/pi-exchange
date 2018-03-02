#include <cstdint>

namespace piex {
class Order {
public:
	using IdType = std::uint64_t;
	using PriceType = std::uint64_t;
	using QuantityType = std::uint64_t;

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
	bool operator<(const BuyOrder &other) const {
		return (price() > other.price()) || (price() == other.price() && id() < other.id());
	}
	inline bool is_compatible_with(const SellOrder &) const;
};

class SellOrder : public Order {
public:
	using Order::Order;
	bool operator<(const SellOrder &other) const {
		return (price() < other.price()) || (price() == other.price() && id() < other.id());
	}
	inline bool is_compatible_with(const BuyOrder &) const;
};

inline bool BuyOrder::is_compatible_with(const SellOrder &sell) const {
	return price() >= sell.price();
}

inline bool SellOrder::is_compatible_with(const BuyOrder &buy) const {
	return price() <= buy.price();
}
}
