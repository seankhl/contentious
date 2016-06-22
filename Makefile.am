ACLOCAL_AMFLAGS = -I./m4

#libmylib_a_HEADERS = headers_to_install.h
#libmylib_adir = $(includedir)/mylib

AM_CPPFLAGS = @AM_CPPFLAGS@ -I$(top_srcdir)/include

lib_LIBRARIES = libfolly-minimal.a libcontentious.a

bin_PROGRAMS = runner

include $(top_srcdir)/subfiles.mk

libfolly_minimal_a_SOURCES = $(folly_sources)

libcontentious_a_SOURCES = $(contentious_sources)

runner_CXXFLAGS = @AM_CXXFLAGS@ -mavx @OPENMP_CXXFLAGS@
runner_SOURCES = $(runner_sources)
runner_LDADD =  $(AM_LDFLAGS) \
				$(top_builddir)/libcontentious.a \
				$(top_builddir)/libfolly-minimal.a
