INCLUDES = -I$(top_srcdir)/src/include -I$(top_builddir)/src/include

plugindir = $(libdir)/gnunet

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

pkgcfg_DATA = \
  transport.conf


if HAVE_MHD
 GN_LIBMHD = -lmicrohttpd
 HTTP_SERVER_PLUGIN_LA = libgnunet_plugin_transport_http_server.la
 HTTPS_SERVER_PLUGIN_LA = libgnunet_plugin_transport_https_server.la
 HTTP_SERVER_PLUGIN_TEST = test_plugin_http_server
 HTTPS_SERVER_PLUGIN_TEST = test_plugin_https_server
endif

if HAVE_LIBCURL
 HTTP_CLIENT_PLUGIN_TEST = test_plugin_http_client
 HTTPS_CLIENT_PLUGIN_TEST = test_plugin_https_client
 HTTP_CLIENT_PLUGIN_LA = libgnunet_plugin_transport_http_client.la
 HTTPS_CLIENT_PLUGIN_LA = libgnunet_plugin_transport_https_client.la
endif

if HAVE_MHD 
if HAVE_LIBCURL
 HTTP_API_TEST = test_transport_api_http
 HTTP_REVERSE_API_TEST = test_transport_api_http_reverse
 HTTP_API_TIMEOUT_TEST = test_transport_api_timeout_http
 HTTP_REL_TEST = test_transport_api_reliability_http
 HTTP_QUOTA_TEST = test_quota_compliance_http \
		   test_quota_compliance_http_asymmetric
 HTTPS_API_TEST = test_transport_api_https
 HTTPS_API_TIMEOUT_TEST = test_transport_api_timeout_https
 HTTPS_REL_TEST = test_transport_api_reliability_https
 HTTPS_QUOTA_TEST = test_quota_compliance_https \
		test_quota_compliance_https_asymmetric 
endif
endif 

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
endif

if LINUX
 WLAN_BIN = gnunet-helper-transport-wlan
 WLAN_BIN_DUMMY = gnunet-helper-transport-wlan-dummy
 WLAN_BIN_SENDER = gnunet-transport-wlan-sender
 WLAN_BIN_RECEIVER = gnunet-transport-wlan-receiver
 WLAN_PLUGIN_LA = libgnunet_plugin_transport_wlan.la
 WLAN_PLUGIN_TEST = test_plugin_wlan
 WLAN_API_TEST = test_transport_api_wlan
 WLAN_REL_TEST = test_transport_api_reliability_wlan
 WLAN_UREL_TEST = test_transport_api_unreliability_wlan
 WLAN_QUOTA_TEST = test_quota_compliance_wlan \
		test_quota_compliance_wlan_asymmetric
endif


if LINUX
install-exec-hook:
	$(top_srcdir)/src/transport/install-wlan-helper.sh $(libexecdir) $(SUDO_BINARY) || true
else
install-exec-hook:
endif

if !MINGW
UNIX_PLUGIN_LA = libgnunet_plugin_transport_unix.la
UNIX_PLUGIN_TEST = test_transport_api_unix 
UNIX_TEST = test_plugin_unix
UNIX_PLUGIN_TIMEOUT_TEST = test_transport_api_timeout_unix
UNIX_REL_TEST = test_transport_api_unreliability_unix
UNIX_QUOTA_TEST = test_quota_compliance_unix \
     test_quota_compliance_unix_asymmetric
endif

noinst_PROGRAMS = \
 $(WLAN_BIN_SENDER) \
 $(WLAN_BIN_RECEIVER)

lib_LTLIBRARIES = \
  libgnunettransport.la \
  libgnunettransporttesting.la

libgnunettransporttesting_la_SOURCES = \
  transport-testing.c transport-testing.h
libgnunettransporttesting_la_LIBADD = \
  $(top_builddir)/src/transport/libgnunettransport.la \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(top_builddir)/src/testing/libgnunettesting.la \
  $(GN_LIBINTL) 
libgnunettransporttesting_la_DEPENDENCIES = \
  libgnunettransport.la
libgnunettransporttesting_la_LDFLAGS = \
 $(GN_LIB_LDFLAGS)

libgnunettransport_la_SOURCES = \
  transport_api.c transport.h \
  transport_api_blacklist.c \
  transport_api_address_to_string.c \
  transport_api_address_lookup.c
libgnunettransport_la_LIBADD = \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL) 
libgnunettransport_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS) $(WINFLAGS) \
  -version-info 1:1:0

libexec_PROGRAMS = \
 $(WLAN_BIN) \
 $(WLAN_BIN_DUMMY) \
 gnunet-service-transport 

bin_PROGRAMS = \
 gnunet-transport \
 gnunet-transport-certificate-creation

#bin_SCRIPTS = \
# gnunet-transport-certificate-creation

gnunet_transport_certificate_creation_SOURCES = \
 gnunet-transport-certificate-creation.c
gnunet_transport_certificate_creation_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la

gnunet_helper_transport_wlan_SOURCES = \
 gnunet-helper-transport-wlan.c

gnunet_helper_transport_wlan_dummy_SOURCES = \
 gnunet-helper-transport-wlan-dummy.c
gnunet_helper_transport_wlan_dummy_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la 

gnunet_transport_wlan_sender_SOURCES = \
 gnunet-transport-wlan-sender.c
gnunet_transport_wlan_sender_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la 

gnunet_transport_wlan_receiver_SOURCES = \
 gnunet-transport-wlan-receiver.c
gnunet_transport_wlan_receiver_LDADD = \
  $(top_builddir)/src/util/libgnunetutil.la 

gnunet_transport_SOURCES = \
 gnunet-transport.c         
gnunet_transport_LDADD = \
  $(top_builddir)/src/transport/libgnunettransport.la \
  $(top_builddir)/src/nat/libgnunetnat.la \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_LIBINTL)
gnunet_transport_DEPENDENCIES = \
  libgnunettransport.la				

gnunet_service_transport_SOURCES = \
 gnunet-service-transport.c gnunet-service-transport.h \
 gnunet-service-transport_blacklist.h gnunet-service-transport_blacklist.c \
 gnunet-service-transport_clients.h gnunet-service-transport_clients.c \
 gnunet-service-transport_hello.h gnunet-service-transport_hello.c \
 gnunet-service-transport_neighbours.h gnunet-service-transport_neighbours.c \
 gnunet-service-transport_plugins.h gnunet-service-transport_plugins.c \
 gnunet-service-transport_validation.h gnunet-service-transport_validation.c 
gnunet_service_transport_LDADD = \
  $(top_builddir)/src/ats/libgnunetats.la \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/peerinfo/libgnunetpeerinfo.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(GN_GLPK) \
  $(GN_LIBINTL)
gnunet_service_transport_CFLAGS = \
  $(CFLAGS)
# -DANALYZE

plugin_LTLIBRARIES = \
  libgnunet_plugin_transport_tcp.la \
  libgnunet_plugin_transport_udp.la \
  $(UNIX_PLUGIN_LA) \
  $(HTTP_CLIENT_PLUGIN_LA) \
  $(HTTPS_CLIENT_PLUGIN_LA) \
  $(HTTP_SERVER_PLUGIN_LA) \
  $(HTTPS_SERVER_PLUGIN_LA) \
  $(WLAN_PLUGIN_LA) \
  libgnunet_plugin_transport_template.la

libgnunet_plugin_transport_tcp_la_SOURCES = \
  plugin_transport_tcp.c
libgnunet_plugin_transport_tcp_la_LIBADD = \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/peerinfo/libgnunetpeerinfo.la \
  $(top_builddir)/src/nat/libgnunetnat.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(LTLIBINTL)
libgnunet_plugin_transport_tcp_la_LDFLAGS = \
 $(GN_PLUGIN_LDFLAGS)

libgnunet_plugin_transport_template_la_SOURCES = \
  plugin_transport_template.c
libgnunet_plugin_transport_template_la_LIBADD = \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(LTLIBINTL)
libgnunet_plugin_transport_template_la_LDFLAGS = \
 $(GN_PLUGIN_LDFLAGS)

libgnunet_plugin_transport_wlan_la_SOURCES = \
  plugin_transport_wlan.c plugin_transport_wlan.h
libgnunet_plugin_transport_wlan_la_LIBADD = \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/peerinfo/libgnunetpeerinfo.la \
  $(top_builddir)/src/fragmentation/libgnunetfragmentation.la \
  $(top_builddir)/src/util/libgnunetutil.la 
libgnunet_plugin_transport_wlan_la_LDFLAGS = \
  $(GN_PLUGIN_LDFLAGS)

libgnunet_plugin_transport_udp_la_SOURCES = \
  plugin_transport_udp.c plugin_transport_udp.h \
  plugin_transport_udp_broadcasting.c
libgnunet_plugin_transport_udp_la_LIBADD = \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/fragmentation/libgnunetfragmentation.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/peerinfo/libgnunetpeerinfo.la \
  $(top_builddir)/src/nat/libgnunetnat.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(LTLIBINTL)
libgnunet_plugin_transport_udp_la_LDFLAGS = \
 $(GN_PLUGIN_LDFLAGS)

libgnunet_plugin_transport_unix_la_SOURCES = \
  plugin_transport_unix.c
libgnunet_plugin_transport_unix_la_LIBADD = \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/peerinfo/libgnunetpeerinfo.la \
  $(top_builddir)/src/util/libgnunetutil.la \
  $(LTLIBINTL)
libgnunet_plugin_transport_unix_la_LDFLAGS = \
 $(GN_PLUGIN_LDFLAGS)


libgnunet_plugin_transport_http_client_la_SOURCES = \
  plugin_transport_http_client.c plugin_transport_http_common.c plugin_transport_http_common.h
libgnunet_plugin_transport_http_client_la_LIBADD = \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/peerinfo/libgnunetpeerinfo.la \
  @LIBCURL@ \
  $(top_builddir)/src/nat/libgnunetnat.la \
  $(top_builddir)/src/util/libgnunetutil.la 
libgnunet_plugin_transport_http_client_la_LDFLAGS = \
 $(GN_PLUGIN_LDFLAGS)
libgnunet_plugin_transport_http_client_la_CFLAGS = \
 $(CFLAGS) 
libgnunet_plugin_transport_http_client_la_CPPFLAGS = \
 @LIBCURL_CPPFLAGS@


libgnunet_plugin_transport_http_server_la_SOURCES = \
  plugin_transport_http_server.c plugin_transport_http_common.c 
libgnunet_plugin_transport_http_server_la_LIBADD = \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/peerinfo/libgnunetpeerinfo.la \
  $(top_builddir)/src/nat/libgnunetnat.la \
  $(top_builddir)/src/util/libgnunetutil.la 
libgnunet_plugin_transport_http_server_la_LDFLAGS = \
 $(GN_LIBMHD) \
 $(GN_PLUGIN_LDFLAGS)
libgnunet_plugin_transport_http_server_la_CFLAGS = \
 $(CFLAGS) 

libgnunet_plugin_transport_https_client_la_SOURCES = \
  plugin_transport_http_client.c plugin_transport_http_common.c
libgnunet_plugin_transport_https_client_la_LIBADD = \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/peerinfo/libgnunetpeerinfo.la \
  @LIBCURL@ \
  $(top_builddir)/src/nat/libgnunetnat.la \
  $(top_builddir)/src/util/libgnunetutil.la 
libgnunet_plugin_transport_https_client_la_LDFLAGS = \
 $(GN_PLUGIN_LDFLAGS)
libgnunet_plugin_transport_https_client_la_CFLAGS = \
 $(CFLAGS) -DBUILD_HTTPS 
libgnunet_plugin_transport_https_client_la_CPPFLAGS = \
 @LIBCURL_CPPFLAGS@


libgnunet_plugin_transport_https_server_la_SOURCES = \
  plugin_transport_http_server.c plugin_transport_http_common.c
libgnunet_plugin_transport_https_server_la_LIBADD = \
  $(top_builddir)/src/hello/libgnunethello.la \
  $(top_builddir)/src/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/peerinfo/libgnunetpeerinfo.la \
  $(top_builddir)/src/nat/libgnunetnat.la \
  $(top_builddir)/src/util/libgnunetutil.la 
libgnunet_plugin_transport_https_server_la_LDFLAGS = \
 $(GN_LIBMHD) \
 $(GN_PLUGIN_LDFLAGS)
libgnunet_plugin_transport_https_server_la_CFLAGS = \
 $(CFLAGS) -DBUILD_HTTPS 

check_PROGRAMS = \
 test_transport_testing_startstop \
 test_transport_testing_restart \
 test_transport_testing \
 test_transport_startonly \
 test_plugin_tcp \
 test_plugin_udp \
 $(UNIX_TEST) \
 $(WLAN_PLUGIN_TEST) \
 $(HTTP_CLIENT_PLUGIN_TEST) \
 $(HTTPS_CLIENT_PLUGIN_TEST) \
 $(HTTP_SERVER_PLUGIN_TEST) \
 $(HTTPS_SERVER_PLUGIN_TEST) \
 test_transport_api_blacklisting \
 test_transport_api_disconnect_tcp \
 test_transport_api_bidirectional_connect \
 test_transport_api_tcp \
 test_transport_api_restart_1peer \
 test_transport_api_restart_2peers \
 test_transport_api_timeout_tcp \
 test_transport_api_limited_sockets_tcp \
 test_transport_api_tcp_nat \
 test_transport_api_udp \
 test_transport_api_timeout_udp \
 $(UNIX_PLUGIN_TEST) \
 $(UNIX_PLUGIN_TIMEOUT_TEST) \
 test_transport_api_udp_nat \
 $(HTTP_API_TEST) \
 $(HTTP_REVERSE_API_TEST) \
 $(HTTP_API_TIMEOUT_TEST) \
 $(HTTPS_API_TEST) \
 $(HTTPS_API_TIMEOUT_TEST) \
 $(WLAN_API_TEST) \
 test_transport_api_multi \
 test_transport_api_reliability_tcp \
 test_transport_api_reliability_tcp_nat \
 test_transport_api_unreliability_udp \
 test_transport_api_unreliability_constant_udp \
 $(UNIX_REL_TEST) \
 $(HTTP_REL_TEST) \
 $(HTTPS_REL_TEST) \
 $(WLAN_REL_TEST) \
 $(WLAN_UREL_TEST) \
 test_quota_compliance_tcp \
 test_quota_compliance_tcp_asymmetric \
 test_quota_compliance_udp \
 $(UNIX_QUOTA_TEST) \
 $(HTTP_QUOTA_TEST) \
 $(HTTPS_QUOTA_TEST) \
 $(WLAN_QUOTA_TEST)

if ENABLE_TEST_RUN
TESTS = \
 test_transport_testing_startstop \
 test_transport_testing_restart \
 test_transport_testing \
 test_transport_startonly \
 test_plugin_tcp \
 test_plugin_udp \
 $(UNIX_TEST) \
 $(WLAN_PLUGIN_TEST) \
 test_transport_api_blacklisting \
 test_transport_api_disconnect_tcp \
 test_transport_api_bidirectional_connect \
 test_transport_api_tcp \
 test_transport_api_restart_1peer \
 test_transport_api_restart_2peers \
 test_transport_api_timeout_tcp \
 test_transport_api_limited_sockets_tcp \
 test_transport_api_tcp_nat \
 test_transport_api_udp \
 test_transport_api_timeout_udp \
 $(UNIX_PLUGIN_TEST) \
 $(UNIX_PLUGIN_TIMEOUT_TEST) \
 test_transport_api_udp_nat \
 $(HTTP_API_TEST) \
 $(HTTP_API_TIMEOUT_TEST) \
 $(HTTPS_API_TEST) \
 $(HTTPS_API_TIMEOUT_TEST) \
 $(WLAN_API_TEST) \
 test_transport_api_multi \
 test_transport_api_reliability_tcp \
 test_transport_api_reliability_tcp_nat \
 test_transport_api_unreliability_udp \
 test_transport_api_unreliability_constant_udp \
 $(UNIX_REL_TEST) \
 $(HTTP_REL_TEST) \
 $(HTTPS_REL_TEST) \
 $(WLAN_REL_TEST) \
 $(WLAN_UREL_TEST) \
 test_quota_compliance_tcp \
 test_quota_compliance_tcp_asymmetric \
 test_quota_compliance_udp \
 $(UNIX_QUOTA_TEST) \
 $(HTTP_QUOTA_TEST) \
 $(HTTPS_QUOTA_TEST)
endif

test_transport_testing_startstop_SOURCES = \
 test_transport_testing_startstop.c
test_transport_testing_startstop_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_testing_restart_SOURCES = \
 test_transport_testing_restart.c
test_transport_testing_restart_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_testing_SOURCES = \
 test_transport_testing.c
test_transport_testing_LDADD = \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 


test_transport_api_blacklisting_SOURCES = \
 test_transport_api_blacklisting.c
test_transport_api_blacklisting_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_disconnect_tcp_SOURCES = \
 test_transport_api_disconnect.c
test_transport_api_disconnect_tcp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_startonly_SOURCES = \
 test_transport_startonly.c
test_transport_startonly_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_plugin_tcp_SOURCES = \
 test_plugin_transport.c
test_plugin_tcp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la
 
test_plugin_udp_SOURCES = \
 test_plugin_transport.c
test_plugin_udp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_plugin_unix_SOURCES = \
 test_plugin_transport.c
test_plugin_unix_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_plugin_wlan_SOURCES = \
 test_plugin_transport.c
test_plugin_wlan_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_plugin_http_server_SOURCES = \
 test_plugin_transport.c
test_plugin_http_server_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_plugin_https_server_SOURCES = \
 test_plugin_transport.c
test_plugin_https_server_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_plugin_http_client_SOURCES = \
 test_plugin_transport.c
test_plugin_http_client_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_plugin_https_client_SOURCES = \
 test_plugin_transport.c
test_plugin_https_client_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la  

test_transport_api_tcp_SOURCES = \
 test_transport_api.c
test_transport_api_tcp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_transport_api_bidirectional_connect_SOURCES = \
 test_transport_api_bidirectional_connect.c
test_transport_api_bidirectional_connect_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_transport_api_restart_1peer_SOURCES = \
 test_transport_api_restart_1peer.c
test_transport_api_restart_1peer_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_restart_2peers_SOURCES = \
 test_transport_api_restart_2peers.c
test_transport_api_restart_2peers_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/statistics/libgnunetstatistics.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_limited_sockets_tcp_SOURCES = \
 test_transport_api_limited_sockets.c
test_transport_api_limited_sockets_tcp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_transport_api_tcp_nat_SOURCES = \
 test_transport_api.c
test_transport_api_tcp_nat_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_reliability_tcp_SOURCES = \
 test_transport_api_reliability.c
test_transport_api_reliability_tcp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_timeout_tcp_SOURCES = \
 test_transport_api_timeout.c
test_transport_api_timeout_tcp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_timeout_unix_SOURCES = \
 test_transport_api_timeout.c
test_transport_api_timeout_unix_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_reliability_tcp_nat_SOURCES = \
 test_transport_api_reliability.c
test_transport_api_reliability_tcp_nat_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_reliability_wlan_SOURCES = \
 test_transport_api_reliability.c
test_transport_api_reliability_wlan_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_unreliability_wlan_SOURCES = \
 test_transport_api_unreliability.c
test_transport_api_unreliability_wlan_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_udp_SOURCES = \
 test_transport_api.c
test_transport_api_udp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_transport_api_timeout_udp_SOURCES = \
 test_transport_api_timeout.c
test_transport_api_timeout_udp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la  

test_transport_api_udp_nat_SOURCES = \
 test_transport_api.c
test_transport_api_udp_nat_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la  

test_transport_api_unix_SOURCES = \
 test_transport_api.c
test_transport_api_unix_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

# HTTP tests
test_transport_api_http_SOURCES = \
 test_transport_api.c
test_transport_api_http_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_transport_api_http_reverse_SOURCES = \
 test_transport_api.c
test_transport_api_http_reverse_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_transport_api_timeout_http_SOURCES = \
 test_transport_api_timeout.c
test_transport_api_timeout_http_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la  

test_transport_api_reliability_http_SOURCES = \
 test_transport_api_reliability.c
test_transport_api_reliability_http_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_quota_compliance_http_SOURCES = \
 test_quota_compliance.c
test_quota_compliance_http_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_quota_compliance_http_asymmetric_SOURCES = \
 test_quota_compliance.c
test_quota_compliance_http_asymmetric_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_quota_compliance_https_SOURCES = \
 test_quota_compliance.c
test_quota_compliance_https_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_quota_compliance_https_asymmetric_SOURCES = \
 test_quota_compliance.c
test_quota_compliance_https_asymmetric_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

# HTTPS tests
test_transport_api_https_SOURCES = \
 test_transport_api.c
test_transport_api_https_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la  \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_timeout_https_SOURCES = \
 test_transport_api_timeout.c
test_transport_api_timeout_https_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la  


test_transport_api_reliability_https_SOURCES = \
 test_transport_api_reliability.c
test_transport_api_reliability_https_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_unreliability_unix_SOURCES = \
 test_transport_api_unreliability.c
test_transport_api_unreliability_unix_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_unreliability_udp_SOURCES = \
 test_transport_api_unreliability.c
test_transport_api_unreliability_udp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_transport_api_unreliability_constant_udp_SOURCES = \
 test_transport_api_unreliability_constant.c
test_transport_api_unreliability_constant_udp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

if LINUX
test_transport_api_wlan_SOURCES = \
 test_transport_api.c
test_transport_api_wlan_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la  
endif

test_quota_compliance_tcp_SOURCES = \
 test_quota_compliance.c
test_quota_compliance_tcp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_quota_compliance_tcp_asymmetric_SOURCES = \
 test_quota_compliance.c
test_quota_compliance_tcp_asymmetric_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 

test_quota_compliance_udp_SOURCES = \
 test_quota_compliance.c
test_quota_compliance_udp_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_quota_compliance_unix_SOURCES = \
 test_quota_compliance.c
test_quota_compliance_unix_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_quota_compliance_unix_asymmetric_SOURCES = \
 test_quota_compliance.c
test_quota_compliance_unix_asymmetric_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_quota_compliance_wlan_SOURCES = \
 test_quota_compliance.c
test_quota_compliance_wlan_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_quota_compliance_wlan_asymmetric_SOURCES = \
 test_quota_compliance.c
test_quota_compliance_wlan_asymmetric_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la

test_transport_api_multi_SOURCES = \
 test_transport_api.c
test_transport_api_multi_LDADD = \
 $(top_builddir)/src/transport/libgnunettransport.la \
 $(top_builddir)/src/hello/libgnunethello.la \
 $(top_builddir)/src/util/libgnunetutil.la \
 $(top_builddir)/src/transport/libgnunettransporttesting.la 


EXTRA_DIST = \
test_plugin_hostkey \
template_cfg_peer1.conf\
template_cfg_peer2.conf\
test_plugin_transport_data.conf\
test_plugin_transport_data_udp.conf\
test_quota_compliance_data.conf\
test_quota_compliance_http_peer1.conf\
test_quota_compliance_http_peer2.conf\
test_quota_compliance_https_peer1.conf\
test_quota_compliance_https_peer2.conf\
test_quota_compliance_tcp_peer1.conf\
test_quota_compliance_tcp_peer2.conf\
test_quota_compliance_udp_peer1.conf\
test_quota_compliance_udp_peer2.conf\
test_quota_compliance_unix_peer1.conf\
test_quota_compliance_unix_peer2.conf\
test_quota_compliance_wlan_peer1.conf\
test_quota_compliance_wlan_peer2.conf\
test_quota_compliance_http_asymmetric_peer1.conf\
test_quota_compliance_http_asymmetric_peer2.conf\
test_quota_compliance_https_asymmetric_peer1.conf\
test_quota_compliance_https_asymmetric_peer2.conf\
test_quota_compliance_tcp_asymmetric_peer1.conf\
test_quota_compliance_tcp_asymmetric_peer2.conf\
test_quota_compliance_unix_asymmetric_peer1.conf\
test_quota_compliance_unix_asymmetric_peer2.conf\
test_quota_compliance_wlan_asymmetric_peer1.conf\
test_quota_compliance_wlan_asymmetric_peer2.conf\
test_transport_api_data.conf\
test_transport_api_http_peer1.conf\
test_transport_api_http_peer2.conf\
test_transport_api_https_peer1.conf\
test_transport_api_https_peer2.conf\
test_transport_api_limited_sockets_tcp_peer1.conf\
test_transport_api_limited_sockets_tcp_peer2.conf\
test_transport_api_timeout_tcp_peer1.conf\
test_transport_api_timeout_tcp_peer2.conf\
test_transport_api_multi_peer1.conf\
test_transport_api_multi_peer2.conf\
test_transport_api_reliability_http_peer1.conf\
test_transport_api_reliability_http_peer2.conf\
test_transport_api_reliability_https_peer1.conf\
test_transport_api_reliability_https_peer2.conf\
test_transport_api_reliability_tcp_nat_peer1.conf\
test_transport_api_reliability_tcp_nat_peer2.conf\
test_transport_api_reliability_tcp_peer1.conf\
test_transport_api_reliability_tcp_peer2.conf\
test_transport_api_reliability_wlan_peer1.conf\
test_transport_api_reliability_wlan_peer2.conf\
test_transport_api_bidirectional_connect_peer1.conf\
test_transport_api_bidirectional_connect_peer2.conf\
test_transport_api_tcp_nat_peer1.conf\
test_transport_api_tcp_nat_peer2.conf\
test_transport_api_tcp_peer1.conf\
test_transport_api_tcp_peer2.conf\
test_transport_api_udp_nat_peer1.conf\
test_transport_api_udp_nat_peer2.conf\
test_transport_api_udp_peer1.conf\
test_transport_api_udp_peer2.conf\
test_transport_api_timeout_udp_peer1.conf\
test_transport_api_timeout_udp_peer2.conf\
test_transport_api_unix_peer1.conf\
test_transport_api_unix_peer2.conf\
test_transport_api_timeout_unix_peer1.conf\
test_transport_api_timeout_unix_peer2.conf\
test_transport_api_unreliability_udp_peer1.conf\
test_transport_api_unreliability_udp_peer2.conf\
test_transport_api_unreliability_unix_peer1.conf\
test_transport_api_unreliability_unix_peer2.conf\
test_transport_api_unreliability_wlan_peer1.conf\
test_transport_api_unreliability_wlan_peer2.conf\
test_transport_api_wlan_peer1.conf\
test_transport_api_wlan_peer2.conf\
test_transport_defaults.conf\
test_transport_startonly.conf\
test_transport_api_disconnect_tcp_peer1.conf\
test_transport_api_disconnect_tcp_peer2.conf\
test_transport_api_timeout_http_peer1.conf\
test_transport_api_timeout_http_peer2.conf\
test_transport_api_timeout_https_peer1.conf\
test_transport_api_timeout_https_peer2.conf\
test_transport_api_unreliability_constant_udp_peer1.conf\
test_transport_api_unreliability_constant_udp_peer2.conf\
test_transport_api_http_reverse_peer1.conf \
test_transport_api_http_reverse_peer2.conf
