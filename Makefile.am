ACLOCAL_AMFLAGS = -I./m4

# the subdirectories of the project to go into
#SUBDIRS = \
#          include/folly \
#          contentious \
#          tests
#libmylib_a_HEADERS = \
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
