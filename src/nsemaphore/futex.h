#include <linux/futex.h>
#include <sys/syscall.h>
#include <atomic>

namespace piex {
namespace nsemaphore {

inline int futex(int *uaddr, int futex_op, int val) {
    return syscall(SYS_futex, uaddr, futex_op, val, NULL, NULL, 0);
}

template <class SizeT>
class Base {
public:
	void terminate() {
		terminated_ = true;
		futex(reinterpret_cast<int *>(&size_), FUTEX_WAKE_PRIVATE, 1);
	}
protected:
	Base(const SizeT &size) : size_(size) {}
	volatile bool terminated_ = false;
	volatile std::size_t waiting_size_ = 0;
	std::atomic<SizeT> size_;
};

class Strict : public Base<unsigned> {
public:
	Strict(std::size_t size) : Base(size) {}
	std::size_t load() {
		return size_.load();
	}
	void post(std::size_t n) {
		unsigned before = size_.fetch_add(n);
		std::size_t waiting_size = waiting_size_;
		if (before < waiting_size && before + n >= waiting_size) {
			futex(reinterpret_cast<int *>(&size_), FUTEX_WAKE_PRIVATE, 1);
		}
	}
	void wait(std::size_t n) {
		unsigned size = size_.load();
		if (size < n && !terminated_) {
			waiting_size_ = n;
			do {
				futex(reinterpret_cast<int *>(&size_), FUTEX_WAIT_PRIVATE, size);
				size = size_.load();
			} while (size < n && !terminated_);
			waiting_size_ = 0;
		}
	}
	void consume(std::size_t n) {
		size_ -= n;
	}
};

struct LooseSize {
	// defined as unsigned for compiler optimization
	unsigned size_plus_flush_size;
	unsigned flush_size;
	unsigned size() const {
		return size_plus_flush_size - flush_size;
	}
};

class Loose : public Base<LooseSize> {
public:
	Loose(std::size_t size) : Base({static_cast<unsigned>(size), 0}) {}
	std::size_t load() {
		return size_.load().size();
	}
	void post(std::size_t n) {
		LooseSize size = size_.load();
		while (!size_.compare_exchange_weak(size, { size.size_plus_flush_size + static_cast<unsigned>(n), size.flush_size }));
		std::size_t waiting_size = waiting_size_;
		if (size.size() < waiting_size && size.size() + n >= waiting_size) {
			futex(reinterpret_cast<int *>(&size_), FUTEX_WAKE_PRIVATE, 1);
		}
	}
	void flush() {
		LooseSize size = size_.load();
		while (!size_.compare_exchange_weak(size, { size.size() * 2, size.size() }));
		futex(reinterpret_cast<int *>(&size_), FUTEX_WAKE_PRIVATE, 1);
	}
	void wait(std::size_t n) {
		LooseSize size = size_.load();
		if (size.size_plus_flush_size < n && size.flush_size == 0 && !terminated_) {
			waiting_size_ = n;
			do {
				futex(reinterpret_cast<int *>(&size_), FUTEX_WAIT_PRIVATE, size.size_plus_flush_size);
				size = size_.load();
			} while (size.size_plus_flush_size < n && size.flush_size == 0 && !terminated_);
			waiting_size_ = 0;
		}
	}
	void consume(std::size_t n) {
		LooseSize size = size_.load();
		while (!size_.compare_exchange_weak(size, {
			size.size_plus_flush_size - static_cast<unsigned>(n) - (size.flush_size <= n ? size.flush_size : static_cast<unsigned>(n)),
			size.flush_size <= n ? 0 : size.flush_size - static_cast<unsigned>(n),
		}));
	}
};

}
}
