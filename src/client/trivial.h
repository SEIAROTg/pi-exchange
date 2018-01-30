#include <unistd.h>
#include <cerrno>
#include <sys/socket.h>
#include <sys/select.h>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include "src/utilities/socket.h"
#include "src/packets/packets.h"
#include "src/order/order.h"

namespace piex {
template <class T>
class Client {
public:
	using EventHandlerType = T;
	explicit Client(EventHandlerType &handler) : handler_(handler) {}
	void connect(const char *host, const char *port) {
		fd_ = utilities::get_ready_socket(host, port, SOCK_STREAM, false);
	}
	void close() {
		::close(fd_);
	}
	template <class U>
	void place(const U &order) {
		Request::Place request(
			std::is_same<U, BuyOrder>() ? Request::BUY : Request::SELL,
			order.id(),
			order.price(),
			order.quantity());
		write(fd_, &request, sizeof(request));
	}
	void buy(const BuyOrder &order) {
		place(order);
	}
	void sell(const SellOrder &order) {
		place(order);
	}
	template <class U>
	void cancel(const Order::IdType &id) {
		Request::Cancel request(
			std::is_same<U, BuyOrder>() ? Request::BUY : Request::SELL,
			id);
		write(fd_, &request, sizeof(request));
	}
	void cancel_buy(const Order::IdType &id) {
		cancel<BuyOrder>(id);
	}
	void cancel_sell(const Order::IdType &id) {
		cancel<SellOrder>(id);
	}
	void receive_responses() {
		std::uint8_t buf[sizeof(Response)];
		Response &response = *reinterpret_cast<Response *>(buf);
		int ret;
		do {
			struct timeval tv = {0, 0};
			FD_ZERO(&rfds);
			FD_SET(fd_, &rfds);
			ret = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
			if (ret > 0) {
				int len;
				len = read(fd_, &response.data(), sizeof(Response::Header));
				if (len == 0) {
					throw std::runtime_error("Connection lost");
				}
				switch (response.data().header.type()) {
				case Response::PLACE:
					len = read(
						fd_,
						reinterpret_cast<std::uint8_t *>(&response.data()) + sizeof(Response::Header),
						sizeof(Response::Place) - sizeof(Response::Header));
					handler_.on_place(response.data().place);
					break;
				case Response::CANCEL:
					len = read(
						fd_,
						reinterpret_cast<std::uint8_t *>(&response.data()) + sizeof(Response::Header),
						sizeof(Response::Cancel) - sizeof(Response::Header));
					handler_.on_cancel(response.data().cancel);
					break;
				case Response::MATCH:
					len = read(
						fd_,
						reinterpret_cast<std::uint8_t *>(&response.data()) + sizeof(Response::Header),
						sizeof(Response::Match) - sizeof(Response::Header));
					handler_.on_match(response.data().match);
					break;
				}
			}
		} while (ret > 0);
	}

private:
	fd_set rfds;
	EventHandlerType &handler_;
	int fd_ = -1;
};
}
