#ifndef BP_VECTOR_CONSTANTS_H
#define BP_VECTOR_CONSTANTS_H

#include <cstdint>

#ifdef CTTS_BPBITS
constexpr uint8_t BP_BITS = CTTS_BPBITS;
#else
constexpr uint8_t BP_BITS = 10;
#endif
constexpr uint16_t BP_WIDTH = 1 << BP_BITS;
constexpr uint16_t BP_MASK = BP_WIDTH - 1;

#endif
