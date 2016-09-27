# -*- autoconf -*-
AC_PREREQ([2.64])
AC_INIT([contentious], [0.3], [slaguna@cs.uchicago.edu])

AC_CONFIG_HEADERS([config.h])
AX_PREFIX_CONFIG_H(
        [${srcdir}/include/folly/folly-config.h],
        [folly], [config.h])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.10 -Wall -Werror foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES]([yes]))

# Checks for programs.
AC_PROG_CXXCPP
AC_PROG_CXX
AC_PROG_RANLIB
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_LANG([C++])

AX_CHECK_COMPILE_FLAG(["-std=c++1z"], [
  AM_CXXFLAGS="$AM_CXXFLAGS -std=c++1z"
], [
  echo "Your compiler does not support c++1z!"
  exit -1
])
AX_CHECK_COMPILE_FLAG(["-Wall"], [
  AM_CPPFLAGS="$AM_CPPFLAGS -Wall"
], [
  echo "No warning flags, compiling anyway"
])
AX_CHECK_COMPILE_FLAG(["-Wextra"], [
  AM_CPPFLAGS="$AM_CPPFLAGS -Wextra"
], [
  echo "No extra warning flags, compiling anyway"
])
# AX_CHECK_COMPILE_FLAG(["-march=native"], [
#   AM_CPPFLAGS="$AM_CPPFLAGS -march=native"
# ], [
#   echo "No native arch build, compiling anyway"
# ])
# AX_CHECK_COMPILE_FLAG(["-mtune=generic"], [
#   AM_CPPFLAGS="$AM_CPPFLAGS -mtune=generic"
# ], [
#   echo "No native tuned build, compiling anyway"
# ])

AC_OPENMP

# Aeems like we don't actually need this
AX_PTHREAD

AX_BOOST_BASE([1.56.0], [],
              [AC_MSG_ERROR(
                  [Please install boost >= 1.56.0 (system, thread)])])
AX_BOOST_SYSTEM
AX_BOOST_THREAD

# FIXME: Replace `main' with a function in `-lprofiler':
#AC_CHECK_LIB([profiler], [main])
# FIXME: Replace `main' with a function in `-ltcmalloc':
#AC_CHECK_LIB([tcmalloc], [main])

# folly header checks
AC_HEADER_STDC
AC_CHECK_HEADERS([ \
        fcntl.h features.h inttypes.h limits.h sched.h stdint.h stdlib.h \
        string.h sys/time.h  unistd.h mutex.h malloc.h byteswap.h \
        bits/functexcept.h bits/c++config.h])

# folly checks for library functions.
AC_CHECK_FUNCS([ \
        getdelim gettimeofday memmove memset pow strerror sched_yield \
        malloc_size malloc_usable_size memrchr pipe2 preadv pwritev])

# folly check for clock_gettime(2), link with rt if necessary.
AC_SEARCH_LIBS([clock_gettime], [rt],
  AC_DEFINE(
    [HAVE_CLOCK_GETTIME],
    [1],
    [Define to 1 if we support clock_gettime(2).]),
  [])

# folly check for pthread_atfork(3), include pthread.h if necessary.
AC_CACHE_CHECK(
  [for pthread_atfork support],
  [folly_cv_prog_cc_pthread_atfork],
  [AC_COMPILE_IFELSE(
    [AC_LANG_SOURCE[
      #include <pthread.h>
      void func() {pthread_atfork(NULL, NULL, NULL);}]
    ],
    [folly_cv_prog_cc_pthread_atfork=yes],
    [folly_cv_prog_cc_pthread_atfork=no])])
if test "$folly_cv_prog_cc_pthread_atfork" = "yes"; then
  AC_DEFINE([HAVE_PTHREAD_ATFORK], [1],
            [Define to 1 if the compiler supports pthread_atfork])
fi

AM_CONDITIONAL([HAVE_STD_THREAD],
               [test "$ac_cv_header_features" = "yes"])
AM_CONDITIONAL([HAVE_X86_64],
               [test "$build_cpu" = "x86_64"])
AM_CONDITIONAL([HAVE_PPC64],
               [test "$build_cpu" = "powerpc64le"])
AM_CONDITIONAL([HAVE_LINUX],
               [test "$build_os" == "linux-gnu"])
AM_CONDITIONAL([HAVE_BITS_FUNCTEXCEPT_H],
               [test "$ac_cv_header_bits_functexcept_h" = "yes"])
AM_CONDITIONAL([HAVE_EXTRANDOM_SFMT19937],
               [test "$folly_cv_prog_cc_have_extrandom_sfmt19937" = "yes"])

# some checks for things contentious uses... but we already checked for c++1z
AC_TYPE_UINT8_T
AC_TYPE_UINT64_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES([ptrdiff_t])
AC_CHECK_FUNCS([floor ceil])

AM_CPPFLAGS="$AM_CPPFLAGS $BOOST_CPPFLAGS"
AM_LDFLAGS="$AM_LDFLAGS $BOOST_THREAD_LIB $BOOST_SYSTEM_LIB"
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
