#ifndef PIEX_HEADER_ORDERBOOK
#define PIEX_HEADER_ORDERBOOK

#include "config.h"

#if PIEX_OPTION_ORDER_BOOK == PIEX_OPTION_ORDER_BOOK_RBTREE
	#include "src/order-book/rbtree.h"
#elif PIEX_OPTION_ORDER_BOOK == PIEX_OPTION_ORDER_BOOK_HEAP
	#include "src/order-book/heap.h"
#elif PIEX_OPTION_ORDER_BOOK == PIEX_OPTION_TRIVIAL
	#include "src/order-book/vector.h"
#else
	#error "Invalid PIEX_OPTION_ORDER_BOOK"
#endif

#endif
