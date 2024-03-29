INCLUDES = -I$(top_srcdir)/src/include

if MINGW
  WINFLAGS = -Wl,--no-undefined -Wl,--export-all-symbols
endif

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
  XLIB = -lgcov
endif

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

pkgcfg_DATA = \
  mesh.conf

plugindir = $(libdir)/gnunet

AM_CLFAGS = -g

libexec_PROGRAMS = \
 gnunet-service-mesh gnunet-service-mesh-new

bin_PROGRAMS = \
 gnunet-mesh

lib_LTLIBRARIES = \
  libgnunetmesh.la \
  libgnunetmeshblock.la

plugin_LTLIBRARIES = \
 libgnunet_plugin_block_mesh.la

libgnunet_plugin_block_mesh_la_SOURCES = \
 plugin_block_mesh.c
libgnunet_plugin_block_mesh_la_LIBADD = \
 $(top_builddir)/src/mesh/libgnunetmeshblock.la \
 $(top_builddir)/src/block/libgnunetblock.la \
 $(top_builddir)/src/util/libgnunetutil.la
libgnunet_plugin_block_mesh_la_LDFLAGS = \
 $(GN_PLUGIN_LDFLAGS)
libgnunet_plugin_block_mesh_la_DEPENDENCIES = \
 libgnunetmeshblock.la \
 $(top_builddir)/src/block/libgnunetblock.la \
 $(top_builddir)/src/util/libgnunetutil.la

libgnunetmesh_la_SOURCES = \
  mesh_api.c mesh_common.c
libgnunetmesh_la_LIBADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(XLIB) \
  $(LTLIBINTL)
libgnunetmesh_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS) $(WINFLAGS) \
  -version-info 1:0:0

libgnunetmeshblock_la_SOURCES = \
  mesh_block_lib.c mesh_block_lib.h
libgnunetmeshblock_la_LIBADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(XLIB) \
  $(LTLIBINTL)
libgnunetmeshblock_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS) $(WINFLAGS) \
  -version-info 2:0:0

gnunet_service_mesh_SOURCES = \
 gnunet-service-mesh.c \
 mesh_tunnel_tree.c mesh_tunnel_tree.h \
 mesh_common.c
gnunet_service_mesh_CFLAGS = $(AM_CFLAGS)
gnunet_service_mesh_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/core/libgnunetcore.la \
  $(top_builddir)/src/dht/libgnunetdht.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/block/libgnunetblock.la \
  $(top_builddir)/src/mesh/libgnunetmeshblock.la \
  $(top_builddir)/src/regex/libgnunetregex.la
gnunet_service_mesh_DEPENDENCIES = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/core/libgnunetcore.la \
  $(top_builddir)/src/dht/libgnunetdht.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/block/libgnunetblock.la \
  libgnunetmeshblock.la \
  $(top_builddir)/src/regex/libgnunetregex.la
if LINUX
gnunet_service_mesh_LDFLAGS = -lrt
endif

gnunet_mesh_SOURCES = \
 gnunet-mesh.c         
gnunet_mesh_LDADD = \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/util/libgnunetutil.la
gnunet_mesh_DEPENDENCIES = \
  libgnunetmesh.la

gnunet_service_mesh_new_SOURCES = \
 gnunet-service-mesh-new.c \
 mesh_tunnel_tree.c \
 mesh_common.c
gnunet_service_mesh_new_CFLAGS = $(AM_CFLAGS)
gnunet_service_mesh_new_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/core/libgnunetcore.la \
  $(top_builddir)/src/dht/libgnunetdht.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/block/libgnunetblock.la \
  $(top_builddir)/src/mesh/libgnunetmeshblock.la \
  $(top_builddir)/src/regex/libgnunetregex.la
gnunet_service_mesh_new_DEPENDENCIES = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/core/libgnunetcore.la \
  $(top_builddir)/src/dht/libgnunetdht.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/block/libgnunetblock.la \
  libgnunetmeshblock.la \
  $(top_builddir)/src/regex/libgnunetregex.la
if LINUX
gnunet_service_mesh_new_LDFLAGS = -lrt
endif


noinst_PROGRAMS = \
  gnunet-regex-profiler \
  gnunet-daemon-regexprofiler

gnunet_regex_profiler_SOURCES = \
  gnunet-regex-profiler.c
gnunet_regex_profiler_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testbed/libgnunettestbed.la \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/regex/libgnunetregex.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la
gnunet_regex_profiler_DEPENDENCIES = \
  libgnunetmesh.la

gnunet_daemon_regexprofiler_SOURCES = \
  gnunet-daemon-regexprofiler.c
gnunet_daemon_regexprofiler_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la
gnunet_daemon_regexprofiler_DEPENDENCIES = \
  libgnunetmesh.la

check_PROGRAMS = \
 test_mesh_api \
 test_mesh_tree_api \
 test_mesh_local_1 \
 test_mesh_local_2 \
 test_mesh_local_traffic_fwd \
 test_mesh_local_traffic_bck \
 test_mesh_local_traffic_both \
 test_mesh_2dtorus \
 test_mesh_regex \
 test_mesh_small_unicast \
 test_mesh_small_multicast \
 test_mesh_small_speed \
 test_mesh_small_speed_nobuf \
 test_mesh_small_speed_min \
 test_mesh_small_speed_backwards \
 test_mesh_small_speed_nobuf_backwards \
 test_mesh_small_speed_min_backwards \
 test_mesh_small_speed_ack

test_mesh_api_SOURCES = \
 test_mesh_api.c
test_mesh_api_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/testing/libgnunettesting.la \
 $(top_builddir)/src/mesh/libgnunetmesh.la
test_mesh_api_DEPENDENCIES = \
  libgnunetmesh.la \
   $(top_builddir)/src/util/libgnunetutil.la

test_mesh_tree_api_SOURCES = \
 test_mesh_tree_api.c
test_mesh_tree_api_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/dht/libgnunetdht.la
test_mesh_tree_api_DEPENDENCIES = \
  libgnunetmesh.la \
  $(top_builddir)/src/dht/libgnunetdht.la

test_mesh_local_1_SOURCES = \
 test_mesh_local_1.c
test_mesh_local_1_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/testing/libgnunettesting.la \
 $(top_builddir)/src/mesh/libgnunetmesh.la
test_mesh_local_1_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_local_2_SOURCES = \
 test_mesh_local_2.c
test_mesh_local_2_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/testing/libgnunettesting.la \
 $(top_builddir)/src/mesh/libgnunetmesh.la
test_mesh_local_2_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_local_traffic_fwd_SOURCES = \
 test_mesh_local_traffic.c
test_mesh_local_traffic_fwd_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/testing/libgnunettesting.la \
 $(top_builddir)/src/mesh/libgnunetmesh.la
test_mesh_local_traffic_fwd_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_local_traffic_bck_SOURCES = \
 test_mesh_local_traffic.c
test_mesh_local_traffic_bck_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/testing/libgnunettesting.la \
 $(top_builddir)/src/mesh/libgnunetmesh.la
test_mesh_local_traffic_bck_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_local_traffic_both_SOURCES = \
 test_mesh_local_traffic.c
test_mesh_local_traffic_both_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/testing/libgnunettesting.la \
 $(top_builddir)/src/mesh/libgnunetmesh.la
test_mesh_local_traffic_both_DEPENDENCIES = \
  libgnunetmesh.la


test_mesh_2dtorus_SOURCES = \
 test_mesh_2dtorus.c
test_mesh_2dtorus_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing_old/libgnunettesting_old.la

test_mesh_regex_SOURCES = \
 test_mesh_regex.c
test_mesh_regex_LDADD = \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing_old/libgnunettesting_old.la
test_mesh_regex_DEPENDENCIES = \
  libgnunetmesh.la


test_mesh_small_unicast_SOURCES = \
 test_mesh_small.c
test_mesh_small_unicast_LDADD = \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing_old/libgnunettesting_old.la
test_mesh_small_unicast_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_small_multicast_SOURCES = \
 test_mesh_small.c
test_mesh_small_multicast_LDADD = \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing_old/libgnunettesting_old.la
test_mesh_small_multicast_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_small_speed_ack_SOURCES = \
 test_mesh_small.c
test_mesh_small_speed_ack_LDADD = \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing_old/libgnunettesting_old.la
test_mesh_small_speed_ack_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_small_speed_SOURCES = \
 test_mesh_small.c
test_mesh_small_speed_LDADD = \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing_old/libgnunettesting_old.la
test_mesh_small_speed_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_small_speed_min_SOURCES = \
 test_mesh_small.c
test_mesh_small_speed_min_LDADD = \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing_old/libgnunettesting_old.la
test_mesh_small_speed_min_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_small_speed_nobuf_SOURCES = \
 test_mesh_small.c
test_mesh_small_speed_nobuf_LDADD = \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing_old/libgnunettesting_old.la
test_mesh_small_speed_nobuf_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_small_speed_backwards_SOURCES = \
 test_mesh_small.c
test_mesh_small_speed_backwards_LDADD = \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing_old/libgnunettesting_old.la
test_mesh_small_speed_backwards_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_small_speed_min_backwards_SOURCES = \
 test_mesh_small.c
test_mesh_small_speed_min_backwards_LDADD = \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing_old/libgnunettesting_old.la
test_mesh_small_speed_min_backwards_DEPENDENCIES = \
  libgnunetmesh.la

test_mesh_small_speed_nobuf_backwards_SOURCES = \
 test_mesh_small.c
test_mesh_small_speed_nobuf_backwards_LDADD = \
  $(top_builddir)/src/mesh/libgnunetmesh.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing_old/libgnunettesting_old.la
test_mesh_small_speed_nobuf_backwards_DEPENDENCIES = \
  libgnunetmesh.la


if ENABLE_TEST_RUN
TESTS = test_mesh_api \
 test_mesh_tree_api \
 test_mesh_local_1 test_mesh_local_2 \
 test_mesh_local_traffic_fwd \
 test_mesh_local_traffic_bck \
 test_mesh_local_traffic_both \
 test_mesh_2dtorus test_mesh_regex \
 test_mesh_small_unicast test_mesh_small_multicast \
 test_mesh_small_speed \
 test_mesh_small_speed_min \
 test_mesh_small_speed_nobuf \
 test_mesh_small_speed_backwards \
 test_mesh_small_speed_min_backwards
endif

EXTRA_DIST = \
  mesh.h mesh_protocol.h \
  test_mesh.conf \
  test_mesh_2dtorus.conf \
  test_mesh_small.conf
