#ifndef PIEX_HEADER_NSEMAPHORE
#define PIEX_HEADER_NSEMAPHORE

#include "config/config.h"

#if PIEX_OPTION_NSEMAPHORE == PIEX_OPTION_TRIVIAL
	#include "src/nsemaphore/condition_variable.h"
#else
	#error "Invalid PIEX_OPTION_NSEMAPHORE"
#endif

#endif
