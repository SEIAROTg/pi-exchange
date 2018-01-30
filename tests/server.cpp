#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <vector>
#include <variant>
#include <chrono>
#include <thread>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "src/server/server.h"
#include "src/client/client.h"
#include "src/packets/packets.h"

class Server : public ::testing::Test {
public:
	Server() : client(*this) {}
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
	virtual void SetUp() {
		pid = fork();
		if (!pid) {
			piex::Server server;
			server.listen("127.0.0.1", "3000");
		} else {
			wait();
			client.connect("127.0.0.1", "3000");
		}
	}
	virtual void TearDown() {
		kill(pid, SIGINT);
		waitpid(pid, NULL, 0);
		client.close();
	}
	void wait() {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	pid_t pid = -1;
	piex::Client<Server> client;
	std::vector<std::variant<piex::Response::Place, piex::Response::Cancel, piex::Response::Match>> responses;
};

TEST_F(Server, place) {
	client.buy({0, 100, 1});
	client.sell({1, 200, 1});
	wait();
	client.receive_responses();

	ASSERT_THAT(responses, testing::ElementsAre(
		piex::Response::Place(true, 0),
		piex::Response::Place(true, 1)
	));
}

TEST_F(Server, match) {
	client.buy({0, 200, 1});
	client.sell({1, 100, 2});
	wait();
	client.receive_responses();

	ASSERT_THAT(responses, testing::ElementsAre(
		piex::Response::Place(true, 0),
		piex::Response::Match(0, 1, 200, 1, 0, 100),
		piex::Response::Place(true, 1)
	));
}

TEST_F(Server, cancel) {
	client.sell({0, 100, 1});
	client.cancel_sell(0);
	client.cancel_sell(0);
	wait();
	client.receive_responses();

	ASSERT_THAT(responses, testing::ElementsAre(
		piex::Response::Place(true, 0),
		piex::Response::Cancel(true, 0),
		piex::Response::Cancel(false, 0)
	));
}

