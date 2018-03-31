#include <map>
#include <algorithm>

namespace piex {
namespace benchmark {

template <class K, class V, std::size_t N>
class static_map_base {
public:
	static_map_base() {
		std::fill(std::begin(keys_), std::end(keys_), EMPTY);
	}
protected:
	static constexpr K EMPTY = static_cast<K>(-1);
	K keys_[N];
	char values_buf_[N * sizeof(V)];
	V (&values_)[N] = *reinterpret_cast<V(*)[N]>(values_buf_);
	std::map<K, V> archived_;

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
	void insert(const K &key, const V &value) {
		std::size_t pos = this->hash(key);
		if (this->keys_[pos] != this->EMPTY) {
			this->archived_.emplace(this->keys_[pos], this->values_[pos]);
		}
		this->keys_[pos] = key;
		this->values_[pos] = value;
		cursor_ = key + 1;
	}
	V erase(const K &key) {
		std::size_t pos = this->hash(key);
		if (this->keys_[pos] == key) {
			this->keys_[pos] = this->EMPTY;
			return this->values_[pos];
		}
		if (key < cursor_ - N) {
			auto it = this->archived_.find(key);
			V value = it->second;
			this->archived_.erase(it);
			return value;
		}
		throw std::out_of_range("key not found");
	}
private:
	K cursor_ = 0;
};

// non-strict version
template <class K, class V, std::size_t N>
class static_map<K, V, N, false> : public static_map_base<K, V, N> {
public:
	void insert(const K &key, const V &value) {
		std::size_t pos = this->hash(key);
		if (this->keys_[pos] != this->EMPTY) {
			if (this->keys_[pos] > key) {
				this->archived_.emplace(key, value);
				return;
			}
			this->archived_.emplace(this->keys_[pos], this->values_[pos]);
		}
		this->keys_[pos] = key;
		this->values_[pos] = value;
	}
	V erase(const K &key) {
		std::size_t pos = this->hash(key);
		if (this->keys_[pos] == key) {
			this->keys_[pos] = this->EMPTY;
			return this->values_[pos];
		}
		if (this->keys_[pos] != this->EMPTY && this->keys_[pos] > key) {
			auto it = this->archived_.find(key);
			V value = it->second;
			this->archived_.erase(it);
			return value;
		}
		throw std::out_of_range("key not found");
	}
};

}
}
