#ifndef BP_VECTOR_CONSTANTS_H
#define BP_VECTOR_CONSTANTS_H

#include <cstdint>

constexpr uint8_t BP_BITS = 14;
constexpr uint16_t BP_WIDTH = 1 << BP_BITS;
constexpr uint16_t BP_MASK = BP_WIDTH - 1;

#endif
