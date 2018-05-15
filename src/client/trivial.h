#include <cerrno>
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include "src/socket/socket.h"
#include "src/packets/packets.h"
#include "src/order/order.h"

namespace piex {

/// \requires Type `T` shall satisfy `EventHandler`
template <class T>
class Client {
public:
	using EventHandlerType = T;
	explicit Client(EventHandlerType &handler) : handler_(handler) {}

	/// \effects Connect to server
	/// \param host Exchange server host
	/// \param port Exchange server port
	void connect(const char *host, const char *port) {
		socket.connect(host, port);
	}

	/// \effects Close connection to server
	void close() {
		socket.close();
	}

	/// \effects Process request
	/// \requires Type `T` shall satisfy `Request`
	/// \param request The request to process
	template <class R>
	void process(const R &request) {
		socket.write(&request, sizeof(request));
	}

	/// \effects Send requests in buffer to server immediately
	void flush() {
		Request::Flush request;
		process(request);
		socket.flush();
	}

	/// \effects Place an order
	/// \requires Type `U` shall be `BuyOrder` or `SellOrder`
	template <class U>
	void place(const U &order) {
		Request::Place request(
			std::is_same<U, BuyOrder>() ? Request::BUY : Request::SELL,
			order.id(),
			order.price(),
			order.quantity());
		process(request);
	}

	/// \effects Place a buy order
	/// \param order Buy order to place
	void buy(const BuyOrder &order) {
		place(order);
	}

	/// \effects Place a sell order
	/// \param order Sell order to place
	void sell(const SellOrder &order) {
		place(order);
	}

	/// \effects Cancel an order
	/// \requires Type `T` shall satisfy `Order` and is the correct type of the order to cancel
	/// \param id The id of the order to cancel
	template <class U>
	void cancel(const Order::IdType &id) {
		Request::Cancel request(
			std::is_same<U, BuyOrder>() ? Request::BUY : Request::SELL,
			id);
		process(request);
	}

	/// \effects Cancel a buy order
	/// \param id The id of the order to cancel
	void cancel_buy(const Order::IdType &id) {
		cancel<BuyOrder>(id);
	}

	/// \effects Cancel a sell order
	/// \param id The id of the order to cancel
	void cancel_sell(const Order::IdType &id) {
		cancel<SellOrder>(id);
	}

	/// \effects Read responses from socket and notify `handler_`
	/// \remarks This function may block when there is no sufficient data in socket
	void receive_response() {
		std::uint8_t buf[sizeof(Response)];
		Response &response = *reinterpret_cast<Response *>(buf);
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

	/// \effects Read responses from socket if available and notify `handler_`
	void try_receive_responses() {
		while (socket.read_ready()) {
			receive_response();
		}
	}
private:
	EventHandlerType &handler_;
	Socket socket;
};
}
