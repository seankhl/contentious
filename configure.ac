# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([contentious], [0.3], [slaguna@cs.uchicago.edu])

AM_INIT_AUTOMAKE([1.10 -Wall -Werror foreign subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES]([yes]))

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AX_PREFIX_CONFIG_H(
		[${srcdir}/include/folly/folly-config.h],
		[folly], [config.h])

# Checks for programs.
AC_PROG_CXXCPP
AC_PROG_CXX
AC_PROG_RANLIB
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_LANG([C++])

AX_CHECK_COMPILE_FLAG(["-std=c++14"], [
  AM_CXXFLAGS="$AM_CXXFLAGS -std=c++14"
], [
  echo "Your compiler does not support c++14!"
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
AX_CHECK_COMPILE_FLAG(["-march=native"], [
  AM_CPPFLAGS="$AM_CPPFLAGS -march=native"
], [
  echo "No native arch build, compiling anyway"
])
AX_CHECK_COMPILE_FLAG(["-mtune=generic"], [
  AM_CPPFLAGS="$AM_CPPFLAGS -mtune=generic"
], [
  echo "No native tuned build, compiling anyway"
])

AC_OPENMP

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lpthread':
AC_CHECK_LIB([pthread], [main])
AC_CHECK_LIB([jemalloc], [xallocx])
# FIXME: Replace `main' with a function in `-lboost_context':
AC_CHECK_LIB([boost_context], [main])
# FIXME: Replace `main' with a function in `-lboost_system':
AC_CHECK_LIB([boost_system], [main])
# FIXME: Replace `main' with a function in `-lboost_thread':
AC_CHECK_LIB([boost_thread], [main])
# FIXME: Replace `main' with a function in `-lprofiler':
#AC_CHECK_LIB([profiler], [main])
# FIXME: Replace `main' with a function in `-ltcmalloc':
#AC_CHECK_LIB([tcmalloc], [main])

# folly header checks
AC_HEADER_STDC
AC_CHECK_HEADERS([ \
		fcntl.h features.h inttypes.h limits.h \
		sched.h stdint.h stdlib.h string.h sys/time.h \
		unistd.h mutex.h malloc.h \
		byteswap.h bits/functexcept.h bits/c++config.h])

# folly checks for library functions.
AC_CHECK_FUNCS([getdelim gettimeofday \
                memmove memset \
                pow strerror sched_yield \
                malloc_size malloc_usable_size memrchr \
                pipe2 preadv pwritev])

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

# folly figure out if we support weak symbols, if not, we will link in some
# null stubs for functions that would otherwise be weak.
AC_CACHE_CHECK(
  [for weak symbol support], 
  [folly_cv_prog_cc_weak_symbols],
  [AC_LINK_IFELSE(
    [AC_LANG_SOURCE[
      extern "C" void configure_link_extern_weak_test() __attribute__((weak));
      int main(int argc, char** argv) {
          return configure_link_extern_weak_test == nullptr;
      }]],     
    [folly_cv_prog_cc_weak_symbols="yes"],
    [folly_cv_prog_cc_weak_symbols="no"])])
if test "$folly_cv_prog_cc_weak_symbols" = yes; then
  AC_DEFINE([HAVE_WEAK_SYMBOLS], [1],
            [Define to 1 if the linker supports weak symbols.])
fi               

AM_CONDITIONAL([HAVE_STD_THREAD], [test "$ac_cv_header_features" = "yes"])
AM_CONDITIONAL([HAVE_X86_64], [test "$build_cpu" = "x86_64"])
AM_CONDITIONAL([HAVE_PPC64], [test "$build_cpu" = "powerpc64le"])
AM_CONDITIONAL([RUN_ARCH_SPECIFIC_TESTS], [test "$build_cpu" = "x86_64" || test "$build_cpu" = "powerpc64le"])
AM_CONDITIONAL([HAVE_LINUX], [test "$build_os" == "linux-gnu"])
AM_CONDITIONAL([HAVE_WEAK_SYMBOLS],
               [test "$folly_cv_prog_cc_weak_symbols" = "yes"])
AM_CONDITIONAL([HAVE_BITS_FUNCTEXCEPT_H], [test "$ac_cv_header_bits_functexcept_h" = "yes"])
AM_CONDITIONAL([HAVE_EXTRANDOM_SFMT19937],
               [test "$folly_cv_prog_cc_have_extrandom_sfmt19937" = "yes"])
AM_CONDITIONAL([FOLLY_TESTMAIN], [test "x${use_follytestmain}" = "xyes"])
AM_CONDITIONAL([HAVE_BOOST_CONTEXT], [test "x${ax_cv_boost_context}" = "xyes"])

# Checks for header files.
#AC_FUNC_ALLOCA
#AC_CHECK_HEADERS([arpa/inet.h fcntl.h fenv.h float.h inttypes.h libintl.h limits.h locale.h malloc.h memory.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/file.h sys/mount.h sys/param.h sys/socket.h sys/statvfs.h sys/time.h sys/vfs.h unistd.h utime.h wchar.h wctype.h])

# Checks for typedefs, structures, and compiler characteristics.
#AC_CHECK_HEADER_STDBOOL
#AC_C_INLINE

#AC_TYPE_INT8_T
#AC_TYPE_INT16_T
#AC_TYPE_INT32_T
#AC_TYPE_INT64_T
AC_TYPE_UINT8_T
#AC_TYPE_UINT16_T
#AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
#AC_TYPE_SSIZE_T
#AC_TYPE_SIZE_T
#AC_TYPE_MODE_T
#AC_TYPE_OFF_T
#AC_TYPE_PID_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
#AC_FUNC_ERROR_AT_LINE
#AC_FUNC_FORK
#AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
#AC_FUNC_MALLOC
#AC_FUNC_MKTIME
#AC_FUNC_MMAP
#AC_FUNC_REALLOC
#AC_FUNC_STRERROR_R
#AC_FUNC_STRTOD
#AC_CHECK_FUNCS([atexit clock_gettime dup2 fesetround floor ftruncate getcwd getpagesize gettimeofday iswprint localeconv localtime_r memchr memmove memset mkdir modf munmap pathconf pow regcomp rmdir select setlocale socket sqrt strchr strcspn strdup strerror strndup strrchr strtol strtoul uname utime])

AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
