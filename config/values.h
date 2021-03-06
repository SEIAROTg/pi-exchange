#ifndef PIEX_HEADER_CONFIG_VALUES
#define PIEX_HEADER_CONFIG_VALUES

// to avoid false positive comparsion when an option is misspelled
#define PIEX_OPTION_TRIVIAL -1

// socket

#define PIEX_OPTION_SOCKET_BUFFERED 1
#define PIEX_OPTION_SOCKET_MULTITHREADED 2
#define PIEX_OPTION_SOCKET_MULTITHREADED_ATOMIC 3
#define PIEX_OPTION_SOCKET_MULTITHREADED_ATOMIC_FLUSH 4

// order-book

#define PIEX_OPTION_ORDER_BOOK_RBTREE 1
#define PIEX_OPTION_ORDER_BOOK_HEAP 2
#define PIEX_OPTION_ORDER_BOOK_TREAP 3

// packets

#define PIEX_OPTION_PACKETS_COMPACT 1

// nsemaphore

#define PIEX_OPTION_NSEMAPHORE_FUTEX 1

#endif
