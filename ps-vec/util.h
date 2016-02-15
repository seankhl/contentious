#ifndef PS_VEC_UTIL
#define PS_VEC_UTIL

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
inline T implicit_cast (typename detail::icast_identity<T>::type x) {
    return x;
}


#endif // PS_VEC_UTIL

