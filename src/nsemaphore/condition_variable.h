#include <mutex>
#include <atomic>
#include <condition_variable>

namespace piex {
namespace nsemaphore {

class Base {
public:
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

class Strict : public Base {
public:
	Strict(std::size_t size) : size_(size) {}
	std::size_t load() {
		return size_.load();
	}
	void post(std::size_t n) {
		std::size_t before = size_.fetch_add(n);
		std::size_t waiting_size = waiting_size_;
		if (before < waiting_size && before + n >= waiting_size) {
			std::unique_lock<std::mutex> lock(mutex_);
			cv_.notify_one();
		}
	}
	void wait(std::size_t n) {
		if (size_.load() < n && !terminated_) {
			std::unique_lock<std::mutex> lock(mutex_);
			waiting_size_ = n;
			cv_.wait(lock, [this, n]() { return size_.load() >= n || terminated_; });
			waiting_size_ = 0;
		}
	}
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

class Loose : public Base {
public:
	Loose(std::size_t size) : size_({size, 0}) {}
	std::size_t load() {
		return size_.load().size;
	}
	void post(std::size_t n) {
		LooseSize size = size_.load();
		while (!size_.compare_exchange_weak(size, { size.size + n, size.flush_size }));
		std::size_t waiting_size = waiting_size_;
		if (size.size < waiting_size && size.size + n >= waiting_size) {
			std::unique_lock<std::mutex> lock(mutex_);
			cv_.notify_one();
		}
	}
	void flush() {
		LooseSize size = size_.load();
		while (!size_.compare_exchange_weak(size, { size.size, size.size }));
		cv_.notify_one();
	}
	void wait(std::size_t n) {
		std::unique_lock<std::mutex> lock(mutex_);
		waiting_size_ = n;
		cv_.wait(lock, [this, n]() {
			LooseSize size = size_.load();
			return size.size >= n || size.flush_size > 0 || terminated_;
		});
		waiting_size_ = 0;
	}
	void consume(std::size_t n) {
		LooseSize size = size_.load();
		while (!size_.compare_exchange_weak(size, { size.size - n, size.flush_size <= n ? 0 : size.flush_size - n }));
	}
private:
	std::atomic<LooseSize> size_;
};

}
}
