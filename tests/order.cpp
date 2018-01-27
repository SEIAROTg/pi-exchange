#include "gtest/gtest.h"
#include "src/order/order.h"

TEST(Order, data) {
	piex::Order a(111, 222, 333);
	EXPECT_EQ(a.id(), 111);
	EXPECT_EQ(a.price(), 222);
	EXPECT_EQ(a.quantity(), 333);
}

TEST(Order, priority_buy_price_diff) {
	piex::BuyOrder a(0, 200, 1), b(1, 100, 1);
	EXPECT_LT(a, b);
	EXPECT_FALSE(b < a);
}

TEST(Order, priority_buy_price_same) {
	piex::BuyOrder a(0, 100, 1), b(1, 100, 1);
	EXPECT_LT(a, b);
	EXPECT_FALSE(b < a);
}

TEST(Order, priority_sell_price_diff) {
	piex::SellOrder a(0, 100, 1), b(1, 200, 1);
	EXPECT_LT(a, b);
	EXPECT_FALSE(b < a);
}

TEST(Order, priority_sell_price_same) {
	piex::SellOrder a(0, 100, 1), b(1, 100, 1);
	EXPECT_LT(a, b);
	EXPECT_FALSE(b < a);
}

TEST(Order, compatible) {
	piex::BuyOrder buy_50(0, 50, 5), buy_100(1, 100, 1), buy_200(2, 200, 2);
	piex::SellOrder sell_50(3, 50, 5), sell_100(4, 100, 1), sell_200(5, 200, 2);

	EXPECT_TRUE(buy_100.is_compatible_with(sell_50));
	EXPECT_TRUE(buy_100.is_compatible_with(sell_100));
	EXPECT_FALSE(buy_100.is_compatible_with(sell_200));

	EXPECT_TRUE(sell_100.is_compatible_with(buy_200));
	EXPECT_TRUE(sell_100.is_compatible_with(buy_100));
	EXPECT_FALSE(sell_100.is_compatible_with(buy_50));
}
