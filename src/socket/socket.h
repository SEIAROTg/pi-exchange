#ifndef PIEX_HEADER_SOCKET
#define PIEX_HEADER_SOCKET

#include "config.h"

#if PIEX_OPTION_SOCKET == PIEX_OPTION_SOCKET_BUFFERED
	#include "src/socket/buffered.h"
#elif PIEX_OPTION_SOCKET == PIEX_OPTION_SOCKET_MULTITHREADED
	#include "src/socket/multithreaded.h"
#else
	#include "src/socket/trivial.h"
#endif

#endif
