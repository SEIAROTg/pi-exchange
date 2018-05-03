#ifndef PIEX_HEADER_PACKETS
#define PIEX_HEADER_PACKETS

#include "config/config.h"

#if PIEX_OPTION_PACKETS == PIEX_OPTION_PACKETS_COMPACT
	#include "src/packets/compact.h"
#elif PIEX_OPTION_PACKETS == PIEX_OPTION_TRIVIAL
	#include "src/packets/trivial.h"
#else
	#error "Invalid PIEX_OPTION_PACKETS"
#endif

#endif
