#ifndef PIEX_HEADER_UTILITY_POOL
#define PIEX_HEADER_UTILITY_POOL

#include <foonathan/memory/config.hpp>
#undef FOONATHAN_MEMORY_THREAD_SAFE_REFERENCE
#define FOONATHAN_MEMORY_THREAD_SAFE_REFERENCE 0
#include <foonathan/memory/container.hpp>
#include <foonathan/memory/memory_pool.hpp>
#include <foonathan/memory/namespace_alias.hpp>

namespace piex {
namespace utility {
namespace pool {

using RawAllocator = memory::memory_pool<>;
template <class T>
using StdAllocator = memory::std_allocator<T, RawAllocator>;

template <class K, class V>
class unordered_map {
private:
	RawAllocator allocator_;
	memory::unordered_map<K, V, RawAllocator> container_;
public:
	explicit unordered_map(std::size_t size) :
		allocator_(memory::unordered_map_node_size<std::pair<const K, V>>::value, size),
		container_(allocator_) {}
	decltype(container_) &container() {
		return container_;
	}
};

template <class K>
class set {
private:
	RawAllocator allocator_;
	memory::set<K, RawAllocator> container_;
public:
	explicit set(std::size_t size) :
		allocator_(memory::set_node_size<K>::value, size),
		container_(allocator_) {}
	decltype(container_) &container() {
		return container_;
	}
};

template <class K, class V>
class map {
private:
	RawAllocator allocator_;
	memory::map<K, V, RawAllocator> container_;
public:
	explicit map(std::size_t size) :
		allocator_(memory::map_node_size<std::pair<const K, V>>::value, size),
		container_(allocator_) {}
	decltype(container_) &container() {
		return container_;
	}
};

template <class T>
class deque {
private:
	RawAllocator allocator_;
	memory::deque<T, RawAllocator> container_;
public:
	explicit deque(std::size_t size) :
		allocator_(sizeof(T), size),
		container_(allocator_) {}
	decltype(container_) &container() {
		return container_;
	}
};

}
}
}

#endif
