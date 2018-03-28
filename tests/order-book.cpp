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
	buys.insert({3, 10, 1});
	buys.insert({4, 10, 1});
	buys.insert({5, 20, 1});
	buys.insert({6, 20, 1});
	buys.insert({7, 10, 1});
	buys.insert({8, 10, 1});
	buys.insert({9, 10, 1});
	buys.insert({10, 10, 1});
	buys.insert({11, 10, 1});
	buys.insert({12, 10, 1});
	buys.insert({13, 15, 1});
	ASSERT_EQ(buys.size(), 14);
	EXPECT_EQ(buys.top().id(), 1);
	success = buys.remove(8);
	EXPECT_TRUE(success);
	ASSERT_EQ(buys.size(), 13);
	EXPECT_EQ(buys.top().id(), 1);
	buys.remove(5);
	ASSERT_EQ(buys.size(), 12);
	EXPECT_EQ(buys.top().id(), 1);
	buys.remove(1);
	ASSERT_EQ(buys.size(), 11);
	EXPECT_EQ(buys.top().id(), 6);
	success = buys.remove(100);
	EXPECT_FALSE(success);
	ASSERT_EQ(buys.size(), 11);
	EXPECT_EQ(buys.top().id(), 6);
	buys.remove(6);
	ASSERT_EQ(buys.size(), 10);
	EXPECT_EQ(buys.top().id(), 13);
	buys.remove(13);
	buys.remove(0);
	ASSERT_EQ(buys.size(), 8);
	EXPECT_EQ(buys.top().id(), 2);
	success = buys.remove(0);
	EXPECT_FALSE(success);
	ASSERT_EQ(buys.size(), 8);
	EXPECT_EQ(buys.top().id(), 2);
	buys.remove(2);
	buys.remove(3);
	buys.remove(4);
	buys.remove(7);
	buys.remove(8);
	buys.remove(9);
	buys.remove(10);
	buys.remove(11);
	buys.remove(12);
	ASSERT_EQ(buys.size(), 0);
	ASSERT_TRUE(buys.empty());
}
