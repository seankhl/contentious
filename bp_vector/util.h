#ifndef BP_VECTOR_UTIL
#define BP_VECTOR_UTIL

#include <cassert>
#include <cstdint>

/* debug printing */

#ifdef DEBUG_EXTREME
#define DEBUG_CERR(x) do { std::cerr << __FILE__ << ":" << __func__ << ":" << __LINE__ << " " << x << std::endl; } while (0)
#else
#define DEBUG_CERR(x) do {} while (0)
#endif

/* from boost */

// Copyright David Abrahams 2003.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

namespace detail {

template<class T> struct icast_identity
{
    typedef T type;
};

} // namespace detail


// implementation originally suggested by C. Green in
// http://lists.boost.org/MailArchives/boost/msg00886.php

// The use of identity creates a non-deduced form, so that the
// explicit template argument must be supplied
template <typename T>
inline T implicit_cast(typename detail::icast_identity<T>::type x)
{
    return x;
}

/* from http://stackoverflow.com/a/9194117 */
constexpr inline int64_t next_multiple(int64_t i, int64_t m)
{
    assert(m && ((m & (m-1)) == 0));
    return (i + m-1) & ~(m-1);
}

#endif  // BP_VECTOR_UTIL
