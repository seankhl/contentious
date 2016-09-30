#ifndef CONT_CONSTANTS_H
#define CONT_CONSTANTS_H

#include <cstdint>

namespace contentious {

#ifdef CTTS_HWCONC
constexpr uint16_t HWCONC = CTTS_HWCONC;
#else
constexpr uint16_t HWCONC = 1;
#endif

}

#endif
