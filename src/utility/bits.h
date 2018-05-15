#ifndef PIEX_HEADER_UTILITY_BITS
#define PIEX_HEADER_UTILITY_BITS

#include <cstdint>

namespace piex {
namespace utility {
namespace bits {

/// \returns size of type in bits
template <class T>
constexpr std::size_t bitwidth() {
	return sizeof(T) * 8;
}

/// \effects set bits[OFFSET..OFFSET + N - 1] to 0
/// \remarks  bits[0] is the least significant bit
template <std::size_t N, std::size_t OFFSET = 0, class T>
T clear_bits(T data) {
	constexpr T MASK = (static_cast<T>(-1) >> (bitwidth<T>() - OFFSET - N)) & (static_cast<T>(-1) << OFFSET);
	return data & (~MASK);
}

/// \effects extract bits[OFFSET..OFFSET + N - 1]
/// \remarks bits[0] is the least significant bit
template <std::size_t N, std::size_t OFFSET = 0, class T>
T extract_bits(T data) {
	return clear_bits<bitwidth<T>() - OFFSET - N, N>(data >> OFFSET);
}

/// \effects extract bits[N..]
/// \remarks bits[0] is the least significant bit
template <std::size_t N, class T>
T discard_bits(T data) {
	return data >> N;
}

/// \effects replace bits[OFFSET..OFFSET + N - 1] with repl
/// \remarks the behavior is undefined if repl has more bits than N
/// \remarks bits[0] is the least significant bit
template <std::size_t N, std::size_t OFFSET = 0, class T>
T set_bits(T data, T repl) {
	return clear_bits<N, OFFSET>(data) | (repl << OFFSET);
}

}
}
}

#endif
