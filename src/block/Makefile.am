INCLUDES = -I$(top_srcdir)/src/include

plugindir = $(libdir)/gnunet

if MINGW
  WINFLAGS = -Wl,--no-undefined -Wl,--export-all-symbols
endif

if USE_COVERAGE
  AM_CFLAGS = --coverage
endif

lib_LTLIBRARIES = libgnunetblock.la

plugin_LTLIBRARIES = \
  libgnunet_plugin_block_template.la \
  libgnunet_plugin_block_test.la 


libgnunet_plugin_block_template_la_SOURCES = \
  plugin_block_template.c
libgnunet_plugin_block_template_la_LIBADD = \
  $(top_builddir)/src/block/libgnunetblock.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(LTLIBINTL)
libgnunet_plugin_block_template_la_LDFLAGS = \
 $(GN_PLUGIN_LDFLAGS)
libgnunet_plugin_block_template_la_DEPENDENCIES = \
  libgnunetblock.la

libgnunet_plugin_block_test_la_SOURCES = \
  plugin_block_test.c
libgnunet_plugin_block_test_la_LIBADD = \
  $(top_builddir)/src/block/libgnunetblock.la \
  $(top_builddir)/src/util/libgnunetutil.la  \
  $(LTLIBINTL)
libgnunet_plugin_block_test_la_LDFLAGS = \
 $(GN_PLUGIN_LDFLAGS)
libgnunet_plugin_block_test_la_DEPENDENCIES = \
  libgnunetblock.la					      


libgnunetblock_la_SOURCES = \
  block.c 
libgnunetblock_la_LIBADD = \
 $(top_builddir)/src/util/libgnunetutil.la
libgnunetblock_la_DEPENDENCIES = \
 $(top_builddir)/src/util/libgnunetutil.la  
libgnunetblock_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS) \
  -version-info 0:0:0
