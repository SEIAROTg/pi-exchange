#include <cstring>
#include "gtest/gtest.h"
#include "src/packets/packets.h"

template <class T>
class PacketTest : public ::testing::Test {
protected:
	template <class U>
	void reinterpret_header(const U &packet) {
		std::memcpy(&buf.data(), &packet, sizeof(typename T::Header));
	}
	template <class U>
	void reinterpret_body(const U &packet) {
		std::memcpy(
			reinterpret_cast<std::uint8_t *>(&buf.data()) + sizeof(typename T::Header),
			reinterpret_cast<const std::uint8_t *>(&packet) + sizeof(typename T::Header),
			sizeof(U) - sizeof(typename T::Header)
		);
	}
	T &buf = *reinterpret_cast<T *>(raw_buf);
private:
	std::uint8_t raw_buf[sizeof(T)];
};

class Request : public PacketTest<piex::Request> {};
class Response : public PacketTest<piex::Response> {};

TEST_F(Request, place_reinterpret) {
	piex::Request::Place request(piex::Request::BUY, 111, 222, 333);
	reinterpret_header(request);
	EXPECT_EQ(buf.data().header.type(), piex::Request::PLACE);
	reinterpret_body(request);
	piex::Request::Place &reinterpreted_request = buf.data().place;
	EXPECT_EQ(reinterpreted_request, request);
}

TEST_F(Request, cancel_reinterpret) {
	piex::Request::Cancel request(piex::Request::SELL, 111);
	reinterpret_header(request);
	EXPECT_EQ(buf.data().header.type(), piex::Request::CANCEL);
	reinterpret_body(request);
	piex::Request::Cancel &reinterpreted_request = buf.data().cancel;
	EXPECT_EQ(reinterpreted_request, request);
}

TEST_F(Request, flush_reinterpret) {
	piex::Request::Flush request;
	reinterpret_header(request);
	EXPECT_EQ(buf.data().header.type(), piex::Request::FLUSH);
}

TEST_F(Response, place_reinterpret) {
	piex::Response::Place response(true, 222);
	reinterpret_header(response);
	EXPECT_EQ(buf.data().header.type(), piex::Response::PLACE);
	reinterpret_body(response);
	piex::Response::Place &reinterpreted_response = buf.data().place;
	EXPECT_EQ(reinterpreted_response, response);
}

TEST_F(Response, cancel_reinterpret) {
	piex::Response::Cancel response(false, 333);
	reinterpret_header(response);
	EXPECT_EQ(buf.data().header.type(), piex::Response::CANCEL);
	reinterpret_body(response);
	piex::Response::Cancel &reinterpreted_response = buf.data().cancel;
	EXPECT_EQ(reinterpreted_response, response);
}

TEST_F(Response, match_reinterpret) {
	piex::Response::Match response(999, 888, 777, 666, 555, 444);
	reinterpret_header(response);
	EXPECT_EQ(buf.data().header.type(), piex::Response::MATCH);
	reinterpret_body(response);
	piex::Response::Match &reinterpreted_response = buf.data().match;
	EXPECT_EQ(reinterpreted_response, response);
}
