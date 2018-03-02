#ifndef PIEX_HEADER_ORDERBOOK
#define PIEX_HEADER_ORDERBOOK

#include "config.h"

#if PIEX_OPTION_ORDER_BOOK == PIEX_OPTION_ORDER_BOOK_RBTREE
	#include "src/order-book/rbtree.h"
#else
	#include "src/order-book/vector.h"
#endif

#endif
