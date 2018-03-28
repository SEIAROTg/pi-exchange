#ifndef PIEX_HEADER_SOCKET
#define PIEX_HEADER_SOCKET

#include "config.h"

#if PIEX_OPTION_SOCKET == PIEX_OPTION_SOCKET_BUFFERED
	#include "src/socket/buffered.h"
#elif PIEX_OPTION_SOCKET == PIEX_OPTION_SOCKET_MULTITHREADED
	#include "src/socket/multithreaded.h"
#elif PIEX_OPTION_SOCKET == PIEX_OPTION_SOCKET_MULTITHREADED_ATOMIC
	#include "src/socket/multithreaded_atomic.h"
#elif PIEX_OPTION_SOCKET == PIEX_OPTION_SOCKET_MULTITHREADED_ATOMIC_FLUSH
	#include "src/socket/multithreaded_atomic_flush.h"
#elif PIEX_OPTION_SOCKET == PIEX_OPTION_TRIVIAL
	#include "src/socket/trivial.h"
#else
	#error "Invalid PIEX_OPTION_SOCKET"
#endif

#endif
