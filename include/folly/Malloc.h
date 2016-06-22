/*
 * Copyright 2016 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Functions to provide smarter use of jemalloc, if jemalloc is being used.
// http://www.canonware.com/download/jemalloc/jemalloc-latest/doc/jemalloc.html

#pragma once
#define FOLLY_MALLOC_H_

/**
 * Define various MALLOCX_* macros normally provided by jemalloc.  We define
 * them so that we don't have to include jemalloc.h, in case the program is
 * built without jemalloc support.
 */
#ifndef MALLOCX_LG_ALIGN
#define MALLOCX_LG_ALIGN(la) (la)
#endif
#ifndef MALLOCX_ZERO
#define MALLOCX_ZERO (static_cast<int>(0x40))
#endif

// If using fbstring from libstdc++ (see comment in FBString.h), then
// just define stub code here to typedef the fbstring type into the
// folly namespace.
// This provides backwards compatibility for code that explicitly
// includes and uses fbstring.
#if defined(_GLIBCXX_USE_FB) && !defined(_LIBSTDCXX_FBSTRING)

#include <string>
#include <stdlib.h>
#include <folly/Portability.h>

namespace folly {
  using std::goodMallocSize;
  using std::smartRealloc;
  using std::checkedMalloc;
  using std::checkedCalloc;
  using std::checkedRealloc;
}

#else // !defined(_GLIBCXX_USE_FB) || defined(_LIBSTDCXX_FBSTRING)

#ifdef _LIBSTDCXX_FBSTRING
#pragma GCC system_header

#include <bits/functexcept.h>
#define FOLLY_HAVE_MALLOC_H 1
#else
#include <string>
#include <stdlib.h>
#include <folly/Portability.h>

#endif

// for malloc_usable_size
// NOTE: FreeBSD 9 doesn't have malloc.h.  It's defitions
// are found in stdlib.h.
#if FOLLY_HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <new>

#ifdef _LIBSTDCXX_FBSTRING
namespace std _GLIBCXX_VISIBILITY(default) {
_GLIBCXX_BEGIN_NAMESPACE_VERSION
#else
namespace folly {
#endif

// Cannot depend on Portability.h when _LIBSTDCXX_FBSTRING.
// Disabled for nvcc because it fails on attributes on lambdas.
#if defined(__GNUC__) && !defined(__NVCC__)
#define FOLLY_MALLOC_NOINLINE __attribute__((__noinline__))
#else
#define FOLLY_MALLOC_NOINLINE
#endif

inline size_t goodMallocSize(size_t minSize) noexcept {
  if (minSize == 0) {
    return 0;
  }

  // Not using jemalloc - no smarts
  return minSize;
}

/**
 * Trivial wrappers around malloc, calloc, realloc that check for allocation
 * failure and throw std::bad_alloc in that case.
 */
inline void* checkedMalloc(size_t size) {
  void* p = malloc(size);
  if (!p) std::__throw_bad_alloc();
  return p;
}

inline void* checkedCalloc(size_t n, size_t size) {
  void* p = calloc(n, size);
  if (!p) std::__throw_bad_alloc();
  return p;
}

inline void* checkedRealloc(void* ptr, size_t size) {
  void* p = realloc(ptr, size);
  if (!p) std::__throw_bad_alloc();
  return p;
}

/**
 * This function tries to reallocate a buffer of which only the first
 * currentSize bytes are used. The problem with using realloc is that
 * if currentSize is relatively small _and_ if realloc decides it
 * needs to move the memory chunk to a new buffer, then realloc ends
 * up copying data that is not used. It's impossible to hook into
 * GNU's malloc to figure whether expansion will occur in-place or as
 * a malloc-copy-free troika. (If an expand_in_place primitive would
 * be available, smartRealloc would use it.) As things stand, this
 * routine just tries to call realloc() (thus benefitting of potential
 * copy-free coalescing) unless there's too much slack memory.
 */
inline void* smartRealloc(void* p,
                          const size_t currentSize,
                          const size_t currentCapacity,
                          const size_t newCapacity) {
  assert(p);
  assert(currentSize <= currentCapacity &&
         currentCapacity < newCapacity);

  // No jemalloc no honey
  auto const slack = currentCapacity - currentSize;
  if (slack * 2 > currentSize) {
    // Too much slack, malloc-copy-free cycle:
    auto const result = checkedMalloc(newCapacity);
    std::memcpy(result, p, currentSize);
    free(p);
    return result;
  }
  // If there's not too much slack, we realloc in hope of coalescing
  return checkedRealloc(p, newCapacity);
}

#ifdef _LIBSTDCXX_FBSTRING
_GLIBCXX_END_NAMESPACE_VERSION
#endif

} // folly

#endif // !defined(_GLIBCXX_USE_FB) || defined(_LIBSTDCXX_FBSTRING)
