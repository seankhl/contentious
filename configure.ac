# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([contentious], [0.3], [slaguna@cs.uchicago.edu])
AM_INIT_AUTOMAKE([1.11 -Wall -Werror foreign subdir-objects])
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXXCPP
AC_PROG_CXX
AC_PROG_RANLIB
AM_PROG_AR

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
AC_CONFIG_FILES([include/folly/Makefile])
AC_CONFIG_FILES([contentious/Makefile])
AC_CONFIG_FILES([tests/Makefile])

AC_OUTPUT
