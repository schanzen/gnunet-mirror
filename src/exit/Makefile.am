INCLUDES = -I$(top_srcdir)/src/include

if MINGW
  WINFLAGS = -Wl,--no-undefined -Wl,--export-all-symbols
endif

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
endif

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

plugindir = $(libdir)/gnunet

dist_pkgcfg_DATA = \
  exit.conf

if LINUX
EXITBIN = gnunet-helper-exit
install-exec-hook:
	$(top_srcdir)/src/exit/install-exit-helper.sh $(libexecdir) $(SUDO_BINARY) || true
else
install-exec-hook:
endif


libexec_PROGRAMS = \
  gnunet-daemon-exit \
  $(EXITBIN) 

gnunet_helper_exit_SOURCES = \
 gnunet-helper-exit.c

gnunet_daemon_exit_SOURCES = \
 gnunet-daemon-exit.c exit.h 
gnunet_daemon_exit_LDADD = \
  $(top_builddir)/src/core/libgnunetcore.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/tun/libgnunettun.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/regex/libgnunetregex.la \
  $(GN_LIBINTL)
