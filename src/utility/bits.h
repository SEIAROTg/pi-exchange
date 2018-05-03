#ifndef PIEX_HEADER_UTILITY_BITS
#define PIEX_HEADER_UTILITY_BITS

#include <cstdint>

namespace piex {
namespace utility {
namespace bits {

// return size of type in bits
template <class T>
constexpr std::size_t bitwidth() {
	return sizeof(T) * 8;
}

// set bits[OFFSET..OFFSET + N - 1] to 0
// bits[0] is the least significant bit
template <std::size_t N, std::size_t OFFSET = 0, class T>
T clear_bits(T data) {
	constexpr T MASK = (static_cast<T>(-1) >> (bitwidth<T>() - OFFSET - N)) & (static_cast<T>(-1) << OFFSET);
	return data & (~MASK);
}

// extract bits[OFFSET..OFFSET + N - 1]
// bits[0] is the least significant bit
template <std::size_t N, std::size_t OFFSET = 0, class T>
T extract_bits(T data) {
	return clear_bits<bitwidth<T>() - OFFSET - N, N>(data >> OFFSET);
}

// extract bits[N..]
// bits[0] is the least significant bit
template <std::size_t N, class T>
T discard_bits(T data) {
	return data >> N;
}

// replace bits[OFFSET..OFFSET + N - 1] with repl
// the behavior is undefined if repl has more bits than N
// bits[0] is the least significant bit
template <std::size_t N, std::size_t OFFSET = 0, class T>
T set_bits(T data, T repl) {
	return clear_bits<N, OFFSET>(data) | (repl << OFFSET);
}

}
}
}

#endif
