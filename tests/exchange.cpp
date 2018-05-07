#include <vector>
#include <variant>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "tests/config_override.h"
#include "src/exchange/exchange.h"
#include "src/packets/packets.h"

class Exchange : public testing::Test {
public:
	Exchange() : exchange(*this) {}
	void on_place(const piex::Response::Place &response) {
		responses.emplace_back(response);
	}
	void on_cancel(const piex::Response::Cancel &response) {
		responses.emplace_back(response);
	}
	void on_match(const piex::Response::Match &response) {
		responses.emplace_back(response);
	}
protected:
	piex::Exchange<Exchange> exchange;
	std::vector<std::variant<piex::Response::Place, piex::Response::Cancel, piex::Response::Match>> responses;
};

TEST_F(Exchange, place) {
	exchange.process_request({piex::Request::BUY, 0, 100, 1});
	exchange.process_request({piex::Request::SELL, 1, 200, 1});

	ASSERT_THAT(responses, testing::ElementsAre(
		piex::Response::Place(true, 0),
		piex::Response::Place(true, 1)
	));
}

TEST_F(Exchange, match) {
	exchange.process_request({piex::Request::SELL, 0, 100, 1});
	exchange.process_request({piex::Request::BUY, 1, 200, 2});
	exchange.process_request({piex::Request::BUY, 2, 100, 2});
	exchange.process_request({piex::Request::SELL, 3, 50, 4});

	ASSERT_THAT(responses, testing::ElementsAre(
		piex::Response::Place(true, 0),
		piex::Response::Match(1, 0, 100, 1, 200, 0),
		piex::Response::Place(true, 1),
		piex::Response::Place(true, 2),
		piex::Response::Match(1, 3, 200, 1, 100, 50),
		piex::Response::Match(2, 3, 100, 2, 0, 50),
		piex::Response::Place(true, 3)
	));
}

TEST_F(Exchange, cancel) {
	exchange.process_request({piex::Request::SELL, 0, 100, 1});
	exchange.process_request({piex::Request::SELL, 0});
	exchange.process_request({piex::Request::SELL, 0});

	ASSERT_THAT(responses, testing::ElementsAre(
		piex::Response::Place(true, 0),
		piex::Response::Cancel(true, 0),
		piex::Response::Cancel(false, 0)
	));
}

TEST_F(Exchange, mixed) {
	exchange.process_request({piex::Request::BUY, 0, 100, 1});
	exchange.process_request({piex::Request::BUY, 0});
	exchange.process_request({piex::Request::SELL, 1, 100, 3});
	exchange.process_request({piex::Request::BUY, 2});
	exchange.process_request({piex::Request::BUY, 2, 100, 1});
	exchange.process_request({piex::Request::BUY, 2});
	exchange.process_request({piex::Request::BUY, 3, 100, 1});
	exchange.process_request({piex::Request::SELL, 1});
	exchange.process_request({piex::Request::BUY, 4, 100, 1});

	ASSERT_THAT(responses, testing::ElementsAreArray(std::vector<std::variant<piex::Response::Place, piex::Response::Cancel, piex::Response::Match>>{
		piex::Response::Place(true, 0),
		piex::Response::Cancel(true, 0),
		piex::Response::Place(true, 1),
		piex::Response::Cancel(false, 2),
		piex::Response::Match(2, 1, 100, 1, 0, 100),
		piex::Response::Place(true, 2),
		piex::Response::Cancel(false, 2),
		piex::Response::Match(3, 1, 100, 1, 0, 100),
		piex::Response::Place(true, 3),
		piex::Response::Cancel(true, 1),
		piex::Response::Place(true, 4)
	}));
}
