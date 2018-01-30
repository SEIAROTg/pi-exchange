#include <iostream>
#include <stdexcept>
#include "src/server/server.h"

int main(int argc, const char *argv[]) {
	piex::Server server;

	const char *host = "127.0.0.1";
	const char *port = "3000";
	switch (argc) {
	case 3:
		host = argv[2];
		[[fallthrough]];
	case 2:
		port = argv[1];
		[[fallthrough]];
	case 1:
		break;
	default:
		std::cerr << "Usage: " << argv[0] << " [port] [host]" << std::endl;
		return 1;
	}
	try {
		server.listen(host, port);
	} catch (const std::runtime_error &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 2;
	}
}
