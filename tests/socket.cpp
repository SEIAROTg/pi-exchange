#include <cstdlib>
#include <utility>
#include <memory>
#include <vector>
#include "gtest/gtest.h"
#include "src/socket/socket.h"
#include "config.h"

class Socket : public ::testing::Test {
protected:
	piex::Socket listen, client;
	std::unique_ptr<piex::Socket> server;
	virtual void SetUp() {
		listen.listen("127.0.0.1", "3000");
		client.connect("127.0.0.1", "3000");
		server = std::make_unique<piex::Socket>(listen.accept());
	}
};

TEST_F(Socket, simple) {
	const char request[] = "foobar";
	const char response[] = "bazqux123";
	char buffer[100];
	client.write(request, sizeof(request));
	client.flush();
	server->read(buffer, sizeof(request), 0);
	ASSERT_STREQ(request, buffer);
	server->write(response, sizeof(response));
	server->flush();
	client.read(buffer, sizeof(response), 0);
	ASSERT_STREQ(response, buffer);
}

TEST_F(Socket, read_from_half) {
	const char request[] = "foobar";
	const char response[] = "bazqux123";
	char buffer[100];
	client.write(request, sizeof(request));
	client.flush();
	server->read(buffer, 1, 0);
	server->read(buffer, sizeof(request), 1);
	ASSERT_STREQ(request, buffer);
	server->write(response, sizeof(response));
	server->flush();
	client.read(buffer, 2, 0);
	client.read(buffer, sizeof(response), 2);
	ASSERT_STREQ(response, buffer);
}

TEST_F(Socket, batch) {
	#ifdef PIEX_OPTION_SOCKET_BUFFER_SIZE
		size_t to_transmit = 2 * PIEX_OPTION_SOCKET_BUFFER_SIZE + 10;
	#else
		size_t to_transmit = 2 * 4096 + 10;
	#endif
	std::srand(0);
	int i = 0;
	for (size_t total = 0; total <= to_transmit; ++i, total += i) {
		std::vector<unsigned char> data;
		for (int j = 0; j <= i; ++j) {
			data.push_back(rand() % 0x100);
		}
		client.write(&data[0], i + 1);
	}
	client.flush();
	std::srand(0);
	unsigned char buffer[i];
	for (; i > 0; --i) {
		server->read(buffer, i, 0);
		for (int j = 0; j < i; ++j) {
			ASSERT_EQ(buffer[j], std::rand() % 0x100);
		}
	}
}
