#include <algorithm>
#include "src/utility/pool.h"

namespace piex {
namespace benchmark {

template <class K, class V, std::size_t N>
class static_map_base {
protected:
	static_map_base() :
		pooled_archived_(PIEX_OPTION_ORDER_BOOK_INIT_SIZE),
		archived_(pooled_archived_.container()) {}
	K keys_[N];
	char values_buf_[N * sizeof(V)];
	V (&values_)[N] = *reinterpret_cast<V(*)[N]>(values_buf_);
	utility::pool::map<K, V> pooled_archived_;
	decltype(pooled_archived_.container()) archived_;

	std::size_t hash(const K &key) {
		return key % N;
	}
};

template <class K, class V, std::size_t N, bool S>
class static_map {};

// strict version: require insertions have strictly ascending keys
template <class K, class V, std::size_t N>
class static_map<K, V, N, true> : public static_map_base<K, V, N> {
public:
	static_map() {
		std::fill(std::begin(this->keys_), std::end(this->keys_), EMPTY);
	}
	void insert(const K &key, const V &value) {
		std::size_t pos = this->hash(key);
		if (this->keys_[pos] != EMPTY) {
			this->archived_.emplace(this->keys_[pos], this->values_[pos]);
		}
		this->keys_[pos] = key;
		this->values_[pos] = value;
		cursor_ = key + 1;
	}
	V erase(const K &key) {
		std::size_t pos = this->hash(key);
		if (this->keys_[pos] == key) {
			this->keys_[pos] = EMPTY;
			return this->values_[pos];
		}
		if (key + N < cursor_) {
			auto it = this->archived_.find(key);
			V value = it->second;
			this->archived_.erase(it);
			return value;
		}
		throw std::out_of_range("key not found");
	}
private:
	static constexpr K EMPTY = static_cast<K>(-1);
	K cursor_ = 0;
};

// non-strict version
template <class K, class V, std::size_t N>
class static_map<K, V, N, false> : public static_map_base<K, V, N> {
public:
	static_map() {
		std::fill(std::begin(this->keys_), std::end(this->keys_), EMPTY);
	}
	void insert(const K &key, const V &value) {
		std::size_t pos = this->hash(key);
		if ((this->keys_[pos] & ~MSB) != EMPTY) {
			this->archived_.emplace(key, value);
			this->keys_[pos] |= MSB;
			return;
		}
		this->keys_[pos] = key;
		this->values_[pos] = value;
	}
	V erase(const K &key) {
		std::size_t pos = this->hash(key);
		if ((this->keys_[pos] & ~MSB) == key) {
			this->keys_[pos] &= ~key;
			return this->values_[pos];
		}
		if (this->keys_[pos] & MSB) {
			auto it = this->archived_.find(key);
			if (it == this->archived_.end()) {
				this->keys_[pos] &= ~MSB;
				throw std::out_of_range("key not found");
			}
			V value = it->second;
			this->archived_.erase(it);
			return value;
		}
		throw std::out_of_range("key not found");
	}
private:
	K get_key(std::size_t pos) {
		return this->keys_[pos] & EMPTY;
	}
	static constexpr K MSB = ~(static_cast<K>(-1) >> 1); // most significant bit
	static constexpr K EMPTY = static_cast<K>(-1) & (~MSB);
};

}
}
