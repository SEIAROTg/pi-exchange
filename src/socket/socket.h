#ifndef PIEX_HEADER_SOCKET
#define PIEX_HEADER_SOCKET

#include "config.h"

#if PIEX_OPTION_SOCKET == PIEX_OPTION_SOCKET_BUFFERED
	#include "src/socket/buffered.h"
#else
	#include "src/socket/trivial.h"
#endif

#endif
