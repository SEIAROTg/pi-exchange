#include "gtest/gtest.h"
#include "src/order-book/order-book.h"
#include "src/order/order.h"

class OrderBook : public ::testing::Test {
protected:
	piex::OrderBook<piex::BuyOrder> buys;
	piex::OrderBook<piex::SellOrder> sells;
};

TEST_F(OrderBook, empty) {
	EXPECT_TRUE(buys.empty());
	EXPECT_TRUE(sells.empty());
	EXPECT_EQ(buys.size(), 0);
	EXPECT_EQ(sells.size(), 0);
}

TEST_F(OrderBook, insert) {
	buys.insert({0, 10, 1});
	EXPECT_FALSE(buys.empty());
	ASSERT_EQ(buys.size(), 1);
	EXPECT_EQ(buys.top().id(), 0);
	buys.insert({1, 20, 1});
	ASSERT_EQ(buys.size(), 2);
	EXPECT_EQ(buys.top().id(), 1);
	buys.insert({2, 10, 1});
	ASSERT_EQ(buys.size(), 3);
	EXPECT_EQ(buys.top().id(), 1);
	buys.insert({3, 20, 1});
	ASSERT_EQ(buys.size(), 4);
	EXPECT_EQ(buys.top().id(), 1);
}

TEST_F(OrderBook, pop) {
	sells.insert({0, 10, 1});
	sells.insert({1, 20, 1});
	sells.insert({2, 10, 1});
	sells.insert({3, 20, 1});
	EXPECT_EQ(sells.top().id(), 0);
	sells.pop();
	ASSERT_EQ(sells.size(), 3);
	EXPECT_EQ(sells.top().id(), 2);
	sells.pop();
	ASSERT_EQ(sells.size(), 2);
	EXPECT_EQ(sells.top().id(), 1);
	sells.pop();
	ASSERT_EQ(sells.size(), 1);
	EXPECT_EQ(sells.top().id(), 3);
	sells.pop();
	ASSERT_EQ(sells.size(), 0);
	EXPECT_TRUE(sells.empty());
}

TEST_F(OrderBook, remove) {
	bool success;
	buys.insert({0, 10, 1});
	buys.insert({1, 20, 1});
	buys.insert({2, 10, 1});
	buys.insert({3, 20, 1});
	EXPECT_EQ(buys.top().id(), 1);
	buys.remove(3);
	ASSERT_EQ(buys.size(), 3);
	EXPECT_EQ(buys.top().id(), 1);
	buys.remove(1);
	ASSERT_EQ(buys.size(), 2);
	EXPECT_EQ(buys.top().id(), 0);
	success = buys.remove(10);
	EXPECT_FALSE(success);
	ASSERT_EQ(buys.size(), 2);
	EXPECT_EQ(buys.top().id(), 0);
	buys.remove(0);
	ASSERT_EQ(buys.size(), 1);
	EXPECT_EQ(buys.top().id(), 2);
	buys.remove(2);
	ASSERT_EQ(buys.size(), 0);
	EXPECT_TRUE(buys.empty());
}
