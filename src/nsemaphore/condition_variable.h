#include <mutex>
#include <atomic>
#include <condition_variable>

namespace piex {
namespace nsemaphore {

class Base {
public:
	/// \effects Wake up waiting threads
	void terminate() {
		terminated_ = true;
		cv_.notify_one();
	}
protected:
	Base() {}
	volatile bool terminated_ = false;
	volatile std::size_t waiting_size_ = 0;
	std::mutex mutex_;
	std::condition_variable cv_;
};

/// \remarks The strict version of nsemaphore with no flush facilities.
/// \remarks There may only be two threads using it for synchronization. One may only call `load` and `post`. The other may only call `load`, `wait` and `consume`.
class Strict : public Base {
public:
	Strict(std::size_t size) : size_(size) {}
	/// \returns The internal value held by the nsemaphore
	std::size_t load() {
		return size_.load();
	}
	/// \effects Atomically increase the internal value by `n`
	void post(std::size_t n) {
		std::size_t before = size_.fetch_add(n);
		std::size_t waiting_size = waiting_size_;
		if (before < waiting_size && before + n >= waiting_size) {
			std::unique_lock<std::mutex> lock(mutex_);
			cv_.notify_one();
		}
	}
	/// \effects Block until the internal value becomes greater than or equal to `n`
	void wait(std::size_t n) {
		if (size_.load() < n && !terminated_) {
			std::unique_lock<std::mutex> lock(mutex_);
			waiting_size_ = n;
			cv_.wait(lock, [this, n]() { return size_.load() >= n || terminated_; });
			waiting_size_ = 0;
		}
	}
	/// \effects Atomically decrease the internal value by `n`
	void consume(std::size_t n) {
		size_ -= n;
	}
private:
	std::atomic_size_t size_;
};

struct LooseSize {
	std::size_t size;
	std::size_t flush_size;
};

/// \remarks The loose version of nsemaphore with flush facilities.
/// \remarks There may only be two threads using it for synchronization. One may only call `load`, `post` and `flush`. The other may only call `load`, `wait` and `consume`.
class Loose : public Base {
public:
	Loose(std::size_t size) : size_({size, 0}) {}
	/// \returns The internal value held by the nsemaphore
	std::size_t load() {
		return size_.load().size;
	}
	/// \effects Atomically increase the internal value by `n`
	void post(std::size_t n) {
		LooseSize size = size_.load();
		while (!size_.compare_exchange_weak(size, { size.size + n, size.flush_size }));
		std::size_t waiting_size = waiting_size_;
		if (size.size < waiting_size && size.size + n >= waiting_size) {
			std::unique_lock<std::mutex> lock(mutex_);
			cv_.notify_one();
		}
	}
	/// \effects Wake up waiting thread until the current internal value is all consumed
	void flush() {
		LooseSize size = size_.load();
		while (!size_.compare_exchange_weak(size, { size.size, size.size }));
		cv_.notify_one();
	}
	/// \effects Block until the internal value becomes greater than or equal to `n` or `flush` is called
	void wait(std::size_t n) {
		LooseSize size = size_.load();
		if (size.size < n && size.flush_size == 0 && !terminated_) {
			std::unique_lock<std::mutex> lock(mutex_);
			waiting_size_ = n;
			cv_.wait(lock, [this, n]() {
				LooseSize size = size_.load();
				return size.size >= n || size.flush_size > 0 || terminated_;
			});
			waiting_size_ = 0;
		}
	}
	/// \effects Atomically decrease the internal value by `n`
	void consume(std::size_t n) {
		LooseSize size = size_.load();
		while (!size_.compare_exchange_weak(size, { size.size - n, size.flush_size <= n ? 0 : size.flush_size - n }));
	}
private:
	std::atomic<LooseSize> size_;
};

}
}
