INCLUDES = -I$(top_srcdir)/src/include

if MINGW
  WINFLAGS = -Wl,--no-undefined -Wl,--export-all-symbols
  NATBIN = gnunet-helper-nat-server gnunet-helper-nat-client
  NATSERVER = gnunet-helper-nat-server-windows.c
  NATCLIENT = gnunet-helper-nat-client-windows.c
endif

libexecdir= $(pkglibdir)/libexec/

pkgcfgdir= $(pkgdatadir)/config.d/

dist_pkgcfg_DATA = \
  nat.conf

if LINUX
NATBIN = gnunet-helper-nat-server gnunet-helper-nat-client
NATSERVER = gnunet-helper-nat-server.c
NATCLIENT = gnunet-helper-nat-client.c
install-exec-hook:
	$(top_srcdir)/src/nat/install-nat-helper.sh $(libexecdir) $(SUDO_BINARY) || true
else
install-exec-hook:
endif

bin_PROGRAMS = \
 gnunet-nat-server 

libexec_PROGRAMS = \
 $(NATBIN) 

gnunet_nat_server_SOURCES = \
 gnunet-nat-server.c nat.h
gnunet_nat_server_LDADD = \
  $(top_builddir)/src/nat/libgnunetnat.la \
  $(top_builddir)/src/util/libgnunetutil.la
gnunet_nat_server_DEPENDENCIES = \
  libgnunetnat.la

gnunet_helper_nat_server_SOURCES = \
 $(NATSERVER)

gnunet_helper_nat_client_SOURCES = \
 $(NATCLIENT)         



if USE_COVERAGE
  AM_CFLAGS = -fprofile-arcs -ftest-coverage
endif

lib_LTLIBRARIES = libgnunetnat.la

libgnunetnat_la_SOURCES = \
  nat.c nat.h \
  nat_test.c \
  nat_mini.c

libgnunetnat_la_LIBADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL) @EXT_LIBS@ 

libgnunetnat_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS) $(WINFLAGS) \
  -version-info 0:1:0

check_PROGRAMS = \
  test_nat \
  test_nat_mini \
  test_nat_test

if ENABLE_TEST_RUN
 TESTS = $(check_PROGRAMS)
endif

test_nat_SOURCES = \
  test_nat.c
test_nat_LDADD = \
 $(top_builddir)/src/nat/libgnunetnat.la \
 $(top_builddir)/src/util/libgnunetutil.la 

test_nat_mini_SOURCES = \
  test_nat_mini.c
test_nat_mini_LDADD = \
 $(top_builddir)/src/nat/libgnunetnat.la \
 $(top_builddir)/src/util/libgnunetutil.la 


test_nat_test_SOURCES = \
  test_nat_test.c
test_nat_test_LDADD = \
 $(top_builddir)/src/nat/libgnunetnat.la \
 $(top_builddir)/src/util/libgnunetutil.la 


EXTRA_DIST = \
 test_nat_data.conf \
 test_nat_test_data.conf 