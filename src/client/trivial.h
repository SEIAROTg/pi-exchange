#include <cerrno>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include "src/socket/socket.h"
#include "src/packets/packets.h"
#include "src/order/order.h"

namespace piex {
template <class T>
class Client {
public:
	using EventHandlerType = T;
	explicit Client(EventHandlerType &handler) : handler_(handler) {}
	void connect(const char *host, const char *port) {
		socket.connect(host, port);
	}
	void close() {
		socket.close();
	}
	template <class U>
	void place(const U &order) {
		Request::Place request(
			std::is_same<U, BuyOrder>() ? Request::BUY : Request::SELL,
			order.id(),
			order.price(),
			order.quantity());
		socket.write(&request, sizeof(request));
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
		socket.write(&request, sizeof(request));
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
		while (socket.read_ready()) {
			int len = socket.read(&response.data(), sizeof(Response::Header));
			if (len == 0) {
				throw std::runtime_error("Connection lost");
			}
			switch (response.data().header.type()) {
			case Response::PLACE:
				len = socket.read(&response.data(), sizeof(Response::Place), sizeof(Response::Header));
				handler_.on_place(response.data().place);
				break;
			case Response::CANCEL:
				len = socket.read(&response.data(), sizeof(Response::Cancel), sizeof(Response::Header));
				handler_.on_cancel(response.data().cancel);
				break;
			case Response::MATCH:
				len = socket.read(&response.data(), sizeof(Response::Match), sizeof(Response::Header));
				handler_.on_match(response.data().match);
				break;
			}
		}
	}
private:
	EventHandlerType &handler_;
	Socket socket;
};
}
