#include "config/config.h"

#ifdef PIEX_OPTION_ORDER_BOOK_INIT_SIZE
	#undef PIEX_OPTION_ORDER_BOOK_INIT_SIZE
	#define PIEX_OPTION_ORDER_BOOK_INIT_SIZE (1 << 16)
#endif
