/*
     This file is part of GNUnet.
     (C) 2011 Christian Grothoff (and other contributing authors)

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/
/**
 * @file mesh/test_mesh_small.c
 *
 * @brief Test for the mesh service: retransmission of traffic.
 */
#include <stdio.h>
#include "platform.h"
#include "gnunet_testing_lib.h"
#include "gnunet_mesh_service.h"
#include <gauger.h>


#define REMOVE_DIR GNUNET_YES

struct MeshPeer
{
  struct MeshPeer *prev;

  struct MeshPeer *next;

  struct GNUNET_TESTING_Daemon *daemon;

  struct GNUNET_MESH_Handle *mesh_handle;
};

/**
 * How namy messages to send
 */
#define TOTAL_PACKETS 1000

/**
 * How long until we give up on connecting the peers?
 */
#define TIMEOUT GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 120)

/**
 * Time to wait for stuff that should be rather fast
 */
#define SHORT_TIME GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_SECONDS, 20)

/**
 * DIFFERENT TESTS TO RUN
 */
#define SETUP 0
#define UNICAST 1
#define MULTICAST 2
#define SPEED 3
#define SPEED_ACK 4
#define SPEED_MIN 5
#define SPEED_NOBUF 6

/**
 * Which test are we running?
 */
static int test;

/**
 * String with test name
 */
char *test_name;

/**
 * Flag to send traffic leaf->root in speed tests to test BCK_ACK logic.
 */
static int test_backwards = GNUNET_NO;

/**
 * How many events have happened
 */
static int ok;

 /**
  * Each peer is supposed to generate the following callbacks:
  * 1 incoming tunnel (@dest)
  * 1 connected peer (@orig)
  * 1 received data packet (@dest)
  * 1 received data packet (@orig)
  * 1 received tunnel destroy (@dest)
  * _________________________________
  * 5 x ok expected per peer
  */
int ok_goal;


/**
 * Size of each test packet
 */
size_t size_payload = sizeof (struct GNUNET_MessageHeader) + sizeof (uint32_t);

/**
 * Is the setup initialized?
 */
static int initialized;

/**
 * Peers that have been connected
 */
static int peers_in_tunnel;

/**
 * Peers that have responded
 */
static int peers_responded;

/**
 * Number of payload packes sent
 */
static int data_sent;

/**
 * Number of payload packets received
 */
static int data_received;

/**
 * Number of payload packed explicitly (app level) acknowledged
 */
static int data_ack;

/**
 * Be verbose
 */
static int verbose;

/**
 * Total number of peers in the test.
 */
static unsigned long long num_peers;

/**
 * Global configuration file
 */
static struct GNUNET_CONFIGURATION_Handle *testing_cfg;

/**
 * Total number of currently running peers.
 */
static unsigned long long peers_running;

/**
 * Total number of connections in the whole network.
 */
static unsigned int total_connections;

/**
 * The currently running peer group.
 */
static struct GNUNET_TESTING_PeerGroup *pg;

/**
 * File to report results to.
 */
static struct GNUNET_DISK_FileHandle *output_file;

/**
 * File to log connection info, statistics to.
 */
static struct GNUNET_DISK_FileHandle *data_file;

/**
 * Wait time
 */
static struct GNUNET_TIME_Relative wait_time;

/**
 * Task called to disconnect peers.
 */
static GNUNET_SCHEDULER_TaskIdentifier disconnect_task;

/**
 * Task To perform tests
 */
static GNUNET_SCHEDULER_TaskIdentifier test_task;

/**
 * Task called to shutdown test.
 */
static GNUNET_SCHEDULER_TaskIdentifier shutdown_handle;

/**
 * Filename of the file containing the topology.
 */
static char *topology_file;

/**
 * Testbed handle for the root peer
 */
static struct GNUNET_TESTING_Daemon *d1;

/**
 * Testbed handle for the first leaf peer
 */
static struct GNUNET_TESTING_Daemon *d2;

/**
 * Testbed handle for the second leaf peer
 */
static struct GNUNET_TESTING_Daemon *d3;

/**
 * Mesh handle for the root peer
 */
static struct GNUNET_MESH_Handle *h1;

/**
 * Mesh handle for the first leaf peer
 */
static struct GNUNET_MESH_Handle *h2;

/**
 * Mesh handle for the second leaf peer
 */
static struct GNUNET_MESH_Handle *h3;

/**
 * Tunnel handle for the root peer
 */
static struct GNUNET_MESH_Tunnel *t;

/**
 * Tunnel handle for the first leaf peer
 */
static struct GNUNET_MESH_Tunnel *incoming_t;

/**
 * Tunnel handle for the second leaf peer
 */
static struct GNUNET_MESH_Tunnel *incoming_t2;

/**
 * Time we started the data transmission (after tunnel has been established
 * and initilized).
 */
static struct GNUNET_TIME_Absolute start_time;


/**
 * Show the results of the test (banwidth acheived) and log them to GAUGER
 */
static void
show_end_data (void)
{
  static struct GNUNET_TIME_Absolute end_time;
  static struct GNUNET_TIME_Relative total_time;

  end_time = GNUNET_TIME_absolute_get();
  total_time = GNUNET_TIME_absolute_get_difference(start_time, end_time);
  FPRINTF (stderr, "\nResults of test \"%s\"\n", test_name);
  FPRINTF (stderr, "Test time %llu ms\n",
            (unsigned long long) total_time.rel_value);
  FPRINTF (stderr, "Test bandwidth: %f kb/s\n",
            4 * TOTAL_PACKETS * 1.0 / total_time.rel_value); // 4bytes * ms
  FPRINTF (stderr, "Test throughput: %f packets/s\n\n",
            TOTAL_PACKETS * 1000.0 / total_time.rel_value); // packets * ms
  GAUGER ("MESH", test_name,
          TOTAL_PACKETS * 1000.0 / total_time.rel_value,
          "packets/s");
}


/**
 * Check whether peers successfully shut down.
 * 
 * @param cls Closure (unused).
 * @param emsg Error message.
 */
static void
shutdown_callback (void *cls, const char *emsg)
{
  if (emsg != NULL)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Shutdown of peers failed!\n");
    ok--;
  }
  else
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "All peers successfully shut down!\n");
  }
  GNUNET_CONFIGURATION_destroy (testing_cfg);
}


/**
 * Shut down peergroup, clean up.
 * 
 * @param cls Closure (unused).
 * @param tc Task Context.
 */
static void
shutdown_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Ending test.\n");
  if (disconnect_task != GNUNET_SCHEDULER_NO_TASK)
  {
    GNUNET_SCHEDULER_cancel (disconnect_task);
    disconnect_task = GNUNET_SCHEDULER_NO_TASK;
  }

  if (NULL != h1)
  {
    GNUNET_MESH_disconnect (h1);
    h1 = NULL;
  }
  if (NULL != h2)
  {
    GNUNET_MESH_disconnect (h2);
    h2 = NULL;
  }
  if (test == MULTICAST && NULL != h3)
  {
    GNUNET_MESH_disconnect (h3);
    h3 = NULL;
  }
  
  if (data_file != NULL)
    GNUNET_DISK_file_close (data_file);
  GNUNET_TESTING_daemons_stop (pg, TIMEOUT, &shutdown_callback, NULL);
}


/**
 * Disconnect from mesh services af all peers, call shutdown.
 * 
 * @param cls Closure (unused).
 * @param tc Task Context.
 */
static void
disconnect_mesh_peers (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  int line = (int) (long ) cls;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "disconnecting mesh service of peers, called from line %d\n",
              line);
  disconnect_task = GNUNET_SCHEDULER_NO_TASK;
  if (NULL != t)
  {
    GNUNET_MESH_tunnel_destroy(t);
    t = NULL;
  }
  if (NULL != incoming_t)
  {
    GNUNET_MESH_tunnel_destroy(incoming_t);
    incoming_t = NULL;
  }
  if (NULL != incoming_t2)
  {
    GNUNET_MESH_tunnel_destroy(incoming_t2);
    incoming_t2 = NULL;
  }
  GNUNET_MESH_disconnect (h1);
  GNUNET_MESH_disconnect (h2);
  h1 = h2 = NULL;
  if (test == MULTICAST)
  {
    GNUNET_MESH_disconnect (h3);
    h3 = NULL;
  }
  if (GNUNET_SCHEDULER_NO_TASK != shutdown_handle)
  {
    GNUNET_SCHEDULER_cancel (shutdown_handle);
    shutdown_handle = GNUNET_SCHEDULER_add_now (&shutdown_task, NULL);
  }
}


/**
 * Transmit ready callback.
 * 
 * @param cls Closure (message type).
 * @param size Size of the tranmist buffer.
 * @param buf Pointer to the beginning of the buffer.
 * 
 * @return Number of bytes written to buf.
 */
static size_t
tmt_rdy (void *cls, size_t size, void *buf);


/**
 * Task to schedule a new data transmission.
 * 
 * @param cls Closure (peer #).
 * @param tc Task Context.
 */
static void
data_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct GNUNET_MESH_TransmitHandle *th;
  struct GNUNET_MESH_Tunnel *tunnel;
  struct GNUNET_PeerIdentity *destination;

  if ((GNUNET_SCHEDULER_REASON_SHUTDOWN & tc->reason) != 0)
    return;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Data task\n");
  if (GNUNET_YES == test_backwards)
  {
    tunnel = incoming_t;
    destination = &d1->id;
  }
  else
  {
    tunnel = t;
    destination = &d2->id;
  }
  th = GNUNET_MESH_notify_transmit_ready (tunnel, GNUNET_NO,
                                          GNUNET_TIME_UNIT_FOREVER_REL,
                                          destination,
                                            size_payload,
                                          &tmt_rdy, (void *) 1L);
  if (NULL == th)
  {
    unsigned long i = (unsigned long) cls;

    GNUNET_log (GNUNET_ERROR_TYPE_INFO, "Retransmission\n");
    if (0 == i)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_INFO, "  in 1 ms\n");
      GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_MILLISECONDS,
                                    &data_task, (void *)1UL);
    }
    else
    {
      i++;
      GNUNET_log (GNUNET_ERROR_TYPE_INFO, "in %u ms\n", i);
      GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_relative_multiply(
                                      GNUNET_TIME_UNIT_MILLISECONDS,
                                      i),
                                    &data_task, (void *)i);
    }
  }
}


/**
 * Transmit ready callback
 *
 * @param cls Closure (message type).
 * @param size Size of the buffer we have.
 * @param buf Buffer to copy data to.
 */
size_t
tmt_rdy (void *cls, size_t size, void *buf)
{
  struct GNUNET_MessageHeader *msg = buf;
  uint32_t *data;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              " tmt_rdy called\n");
  if (size < size_payload || NULL == buf)
  {
    GNUNET_break (0);
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                "size %u, buf %p, data_sent %u, data_received %u\n",
                size,
                buf,
                data_sent,
                data_received);
    return 0;
  }
  msg->size = htons (size);
  msg->type = htons ((long) cls);
  data = (uint32_t *) &msg[1];
  *data = htonl (data_sent);
  if (SPEED == test && GNUNET_YES == initialized)
  {
    data_sent++;
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              " Sent packet %d\n", data_sent);
    if (data_sent < TOTAL_PACKETS)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              " Scheduling packet %d\n", data_sent + 1);
      GNUNET_SCHEDULER_add_now(&data_task, NULL);
    }
  }
  return size_payload;
}


/**
 * Function is called whenever a message is received.
 *
 * @param cls closure (set from GNUNET_MESH_connect)
 * @param tunnel connection to the other end
 * @param tunnel_ctx place to store local state associated with the tunnel
 * @param sender who sent the message
 * @param message the actual message
 * @param atsi performance data for the connection
 * @return GNUNET_OK to keep the connection open,
 *         GNUNET_SYSERR to close it (signal serious error)
 */
int
data_callback (void *cls, struct GNUNET_MESH_Tunnel *tunnel, void **tunnel_ctx,
               const struct GNUNET_PeerIdentity *sender,
               const struct GNUNET_MessageHeader *message,
               const struct GNUNET_ATS_Information *atsi)
{
  long client = (long) cls;
  long expected_target_client;
  uint32_t *data;

  ok++;

  if ((ok % 20) == 0)
  {
    if (GNUNET_SCHEDULER_NO_TASK != disconnect_task)
    {
      GNUNET_SCHEDULER_cancel (disconnect_task);
    }
    disconnect_task =
              GNUNET_SCHEDULER_add_delayed (SHORT_TIME, &disconnect_mesh_peers,
                                            (void *) __LINE__);
  }

  switch (client)
  {
  case 1L:
    GNUNET_log (GNUNET_ERROR_TYPE_INFO, "Root client got a message!\n");
    peers_responded++;
    if (test == MULTICAST && peers_responded < 2)
      return GNUNET_OK;
    break;
  case 2L:
  case 3L:
    GNUNET_log (GNUNET_ERROR_TYPE_INFO,
                "Leaf client %li got a message.\n",
                client);
    client = 2L;
    break;
  default:
    GNUNET_assert (0);
    break;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_INFO, " ok: (%d/%d)\n", ok, ok_goal);
  data = (uint32_t *) &message[1];
  GNUNET_log (GNUNET_ERROR_TYPE_INFO, " payload: (%u)\n", ntohl (*data));
  if (SPEED == test && GNUNET_YES == test_backwards)
  {
    expected_target_client = 1L;
  }
  else
  {
    expected_target_client = 2L;
  }

  if (GNUNET_NO == initialized)
  {
    initialized = GNUNET_YES;
    start_time = GNUNET_TIME_absolute_get ();
    if (SPEED == test)
    {
      GNUNET_assert (2L == client);
      GNUNET_SCHEDULER_add_now (&data_task, NULL);
      return GNUNET_OK;
    }
  }

  if (client == expected_target_client) // Normally 2 or 3
  {
    data_received++;
    GNUNET_log (GNUNET_ERROR_TYPE_INFO,
                " received data %u\n", data_received);
    if (SPEED != test || (ok_goal - 2) == ok)
    {
      GNUNET_MESH_notify_transmit_ready (tunnel, GNUNET_NO,
                                        GNUNET_TIME_UNIT_FOREVER_REL, sender,
                                               size_payload,
                                        &tmt_rdy, (void *) 1L);
      return GNUNET_OK;
    }
    else
    {
      if (data_received < TOTAL_PACKETS)
        return GNUNET_OK;
    }
  }
  else // Normally 1
  {
    if (test == SPEED_ACK || test == SPEED)
    {
      data_ack++;
      GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              " received ack %u\n", data_ack);
      GNUNET_MESH_notify_transmit_ready (tunnel, GNUNET_NO,
                                        GNUNET_TIME_UNIT_FOREVER_REL, sender,
                                               size_payload,
                                        &tmt_rdy, (void *) 1L);
      if (data_ack < TOTAL_PACKETS && SPEED != test)
        return GNUNET_OK;
      if (ok == 2 && SPEED == test)
        return GNUNET_OK;
      show_end_data();
    }
    GNUNET_MESH_tunnel_destroy (t);
    t = NULL;
  }

  if (GNUNET_SCHEDULER_NO_TASK != disconnect_task)
  {
    GNUNET_SCHEDULER_cancel (disconnect_task);
  }
  disconnect_task =
        GNUNET_SCHEDULER_add_delayed (SHORT_TIME, &disconnect_mesh_peers,
                                      (void *) __LINE__);

  return GNUNET_OK;
}


/**
 * Handlers, for diverse services
 */
static struct GNUNET_MESH_MessageHandler handlers[] = {
  {&data_callback, 1, sizeof (struct GNUNET_MessageHeader)},
  {NULL, 0, 0}
};


/**
 * Method called whenever another peer has added us to a tunnel
 * the other peer initiated.
 *
 * @param cls closure
 * @param tunnel new handle to the tunnel
 * @param initiator peer that started the tunnel
 * @param atsi performance information for the tunnel
 * @return initial tunnel context for the tunnel
 *         (can be NULL -- that's not an error)
 */
static void *
incoming_tunnel (void *cls, struct GNUNET_MESH_Tunnel *tunnel,
                 const struct GNUNET_PeerIdentity *initiator,
                 const struct GNUNET_ATS_Information *atsi)
{
  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "Incoming tunnel from %s to peer %d\n",
              GNUNET_i2s (initiator), (long) cls);
  ok++;
  GNUNET_log (GNUNET_ERROR_TYPE_INFO, " ok: %d\n", ok);
  if ((long) cls == 2L)
    incoming_t = tunnel;
  else if ((long) cls == 3L)
    incoming_t2 = tunnel;
  else
  {
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                "Incoming tunnel for unknown client %lu\n", (long) cls);
    GNUNET_break(0);
  }
  if (GNUNET_SCHEDULER_NO_TASK != disconnect_task)
  {
    GNUNET_SCHEDULER_cancel (disconnect_task);
  }
  disconnect_task =
        GNUNET_SCHEDULER_add_delayed (SHORT_TIME, &disconnect_mesh_peers,
                                      (void *) __LINE__);

  return NULL;
}

/**
 * Function called whenever an inbound tunnel is destroyed.  Should clean up
 * any associated state.
 *
 * @param cls closure (set from GNUNET_MESH_connect)
 * @param tunnel connection to the other end (henceforth invalid)
 * @param tunnel_ctx place where local state associated
 *                   with the tunnel is stored
 */
static void
tunnel_cleaner (void *cls, const struct GNUNET_MESH_Tunnel *tunnel,
                void *tunnel_ctx)
{
  long i = (long) cls;

  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "Incoming tunnel disconnected at peer %d\n",
              i);
  if (2L == i)
  {
    ok++;
    incoming_t = NULL;
  }
  else if (3L == i)
  {
    ok++;
    incoming_t2 = NULL;
  }
  else
    GNUNET_log (GNUNET_ERROR_TYPE_WARNING,
                "Unknown peer! %d\n", i);
  GNUNET_log (GNUNET_ERROR_TYPE_INFO, " ok: %d\n", ok);
  peers_in_tunnel--;
  if (peers_in_tunnel > 0)
    return;

  if (GNUNET_SCHEDULER_NO_TASK != disconnect_task)
  {
    GNUNET_SCHEDULER_cancel (disconnect_task);
  }
  disconnect_task = GNUNET_SCHEDULER_add_now (&disconnect_mesh_peers,
                                              (void *) __LINE__);

  return;
}


/**
 * Method called whenever a tunnel falls apart.
 *
 * @param cls closure
 * @param peer peer identity the tunnel stopped working with
 */
static void
dh (void *cls, const struct GNUNET_PeerIdentity *peer)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "peer %s disconnected\n",
              GNUNET_i2s (peer));
  return;
}


/**
 * Method called whenever a peer connects to a tunnel.
 *
 * @param cls closure
 * @param peer peer identity the tunnel was created to, NULL on timeout
 * @param atsi performance data for the connection
 */
static void
ch (void *cls, const struct GNUNET_PeerIdentity *peer,
    const struct GNUNET_ATS_Information *atsi)
{
  struct GNUNET_PeerIdentity *dest;

  GNUNET_log (GNUNET_ERROR_TYPE_INFO,
              "peer %s connected\n", GNUNET_i2s (peer));

  if (0 == memcmp (&d2->id, peer, sizeof (d2->id)) && (long) cls == 1L)
  {
    ok++;
  }
  if (test == MULTICAST && 0 == memcmp (&d3->id, peer, sizeof (d3->id)) &&
      (long) cls == 1L)
  {
    ok++;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_INFO, " ok: %d\n", ok);
  switch (test)
  {
    case UNICAST:
    case SPEED:
    case SPEED_ACK:
      // incoming_t is NULL unless we send a relevant data packet
      dest = &d2->id;
      break;
    case MULTICAST:
      peers_in_tunnel++;
      if (peers_in_tunnel < 2)
        return;
      dest = NULL;
      break;
    default:
      GNUNET_assert (0);
      return;
  }
  if (GNUNET_SCHEDULER_NO_TASK != disconnect_task)
  {
    GNUNET_SCHEDULER_cancel (disconnect_task);
    disconnect_task =
        GNUNET_SCHEDULER_add_delayed (SHORT_TIME, &disconnect_mesh_peers,
                                      (void *) __LINE__);
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Sending data initializer...\n");
    peers_responded = 0;
    data_ack = 0;
    data_received = 0;
    data_sent = 0;
    GNUNET_MESH_notify_transmit_ready (t, GNUNET_NO,
                                       GNUNET_TIME_UNIT_FOREVER_REL, dest,
                                           size_payload,
                                       &tmt_rdy, (void *) 1L);
  }
  else
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Disconnect already run?\n");
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Aborting...\n");
  }
  return;
}


/**
 * START THE TESTCASE ITSELF, AS WE ARE CONNECTED TO THE MESH SERVICES.
 * 
 * Testcase continues when the root receives confirmation of connected peers,
 * on callback funtion ch.
 * 
 * @param cls Closure (unsued).
 * @param tc Task Context.
 */
static void
do_test (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "test_task\n");
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "add peer 2\n");
  GNUNET_MESH_peer_request_connect_add (t, &d2->id);

  if (test == MULTICAST)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "add peer 3\n");
    GNUNET_MESH_peer_request_connect_add (t, &d3->id);
  }

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "schedule timeout in TIMEOUT\n");
  if (GNUNET_SCHEDULER_NO_TASK != disconnect_task)
  {
    GNUNET_SCHEDULER_cancel (disconnect_task);
    disconnect_task =
        GNUNET_SCHEDULER_add_delayed (TIMEOUT, &disconnect_mesh_peers,
                                      (void *) __LINE__);
  }
}


/**
 * connect_mesh_service: connect to the mesh service of one of the peers
 *
 * @param cls Closure (unsued).
 * @param tc Task Context.
 */
static void
connect_mesh_service (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  GNUNET_MESH_ApplicationType app;

  if ((GNUNET_SCHEDULER_REASON_SHUTDOWN & tc->reason) != 0)
    return;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "connect_mesh_service\n");

  d2 = GNUNET_TESTING_daemon_get (pg, num_peers - 1);
  if (test == MULTICAST)
  {
    d3 = GNUNET_TESTING_daemon_get (pg, num_peers - 2);
  }
  app = (GNUNET_MESH_ApplicationType) 0;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "connecting to mesh service of peer %s\n",
              GNUNET_i2s (&d1->id));
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "connecting to mesh service of peer %s\n",
              GNUNET_i2s (&d2->id));
  if (test == MULTICAST)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "connecting to mesh service of peer %s\n",
                GNUNET_i2s (&d3->id));
  }
  h1 = GNUNET_MESH_connect (d1->cfg, (void *) 1L, NULL, &tunnel_cleaner,
                            handlers, &app);
  h2 = GNUNET_MESH_connect (d2->cfg, (void *) 2L, &incoming_tunnel,
                            &tunnel_cleaner, handlers, &app);
  if (test == MULTICAST)
  {
    h3 = GNUNET_MESH_connect (d3->cfg, (void *) 3L, &incoming_tunnel,
                              &tunnel_cleaner, handlers, &app);
  }
  t = GNUNET_MESH_tunnel_create (h1, NULL, &ch, &dh, (void *) 1L);
  if (SPEED_MIN == test)
  {
    GNUNET_MESH_tunnel_speed_min(t);
    test = SPEED;
  }
  if (SPEED_NOBUF == test)
  {
    GNUNET_MESH_tunnel_buffer(t, GNUNET_NO);
    test = SPEED;
  }
  peers_in_tunnel = 0;
  test_task =
      GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_relative_multiply
                                    (GNUNET_TIME_UNIT_SECONDS, 1), &do_test,
                                    NULL);
}



/**
 * peergroup_ready: start test when all peers are connected
 * 
 * @param cls closure
 * @param emsg error message
 */
static void
peergroup_ready (void *cls, const char *emsg)
{
  char *buf;
  int buf_len;
  unsigned int i;

  if (emsg != NULL)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Peergroup callback called with error, aborting test!\n");
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Error from testing: `%s'\n", emsg);
    ok--;
    GNUNET_TESTING_daemons_stop (pg, TIMEOUT, &shutdown_callback, NULL);
    return;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Peer Group started successfully with %u connections\n",
              total_connections);
  if (data_file != NULL)
  {
    buf = NULL;
    buf_len = GNUNET_asprintf (&buf, "CONNECTIONS_0: %u\n", total_connections);
    if (buf_len > 0)
      GNUNET_DISK_file_write (data_file, buf, buf_len);
    GNUNET_free (buf);
  }
  peers_running = GNUNET_TESTING_daemons_running (pg);
  for (i = 0; i < num_peers; i++)
  {
    GNUNET_PEER_Id peer_id;

    d1 = GNUNET_TESTING_daemon_get (pg, i);
    peer_id = GNUNET_PEER_intern (&d1->id);
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "  %u: %s\n",
                peer_id, GNUNET_i2s (&d1->id));
  }
  d1 = GNUNET_TESTING_daemon_get (pg, 0);
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Peer looking: %s\n",
              GNUNET_i2s (&d1->id));

  GNUNET_SCHEDULER_add_now (&connect_mesh_service, NULL);
  disconnect_task =
      GNUNET_SCHEDULER_add_delayed (wait_time, &disconnect_mesh_peers,
                                    (void *) __LINE__);

}


/**
 * Function that will be called whenever two daemons are connected by
 * the testing library.
 *
 * @param cls closure
 * @param first peer id for first daemon
 * @param second peer id for the second daemon
 * @param distance distance between the connected peers
 * @param first_cfg config for the first daemon
 * @param second_cfg config for the second daemon
 * @param first_daemon handle for the first daemon
 * @param second_daemon handle for the second daemon
 * @param emsg error message (NULL on success)
 */
static void
connect_cb (void *cls, const struct GNUNET_PeerIdentity *first,
            const struct GNUNET_PeerIdentity *second, uint32_t distance,
            const struct GNUNET_CONFIGURATION_Handle *first_cfg,
            const struct GNUNET_CONFIGURATION_Handle *second_cfg,
            struct GNUNET_TESTING_Daemon *first_daemon,
            struct GNUNET_TESTING_Daemon *second_daemon, const char *emsg)
{
  if (emsg == NULL)
  {
    total_connections++;
  }
  else
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
                "Problem with new connection (%s)\n",
                emsg);
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, " (%s)\n", GNUNET_i2s (first));
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, " (%s)\n", GNUNET_i2s (second));
  }

}


/**
 * run: load configuration options and schedule test to run (start peergroup)
 * 
 * @param cls closure
 * @param args argv
 * @param cfgfile configuration file name (can be NULL)
 * @param cfg configuration handle
 */
static void
run (void *cls, char *const *args, const char *cfgfile,
     const struct GNUNET_CONFIGURATION_Handle *cfg)
{
  char *temp_str;
  struct GNUNET_TESTING_Host *hosts;
  char *data_filename;

  ok = 0;
  testing_cfg = GNUNET_CONFIGURATION_dup (cfg);

  GNUNET_log_setup ("test_mesh_small",
                    "WARNING",
                    NULL);

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG,
              "Starting daemons.\n");
  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_number (testing_cfg, "testing_old",
                                             "num_peers", &num_peers))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Option TESTING:NUM_PEERS is required!\n");
    return;
  }

  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_time (testing_cfg, "test_mesh_small",
                                           "WAIT_TIME", &wait_time))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Option test_mesh_small:wait_time is required!\n");
    return;
  }

  if (GNUNET_OK !=
      GNUNET_CONFIGURATION_get_value_string (testing_cfg, "testing_old",
                                             "topology_output_file",
                                             &topology_file))
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "Option test_mesh_small:topology_output_file is required!\n");
    return;
  }

  if (GNUNET_OK ==
      GNUNET_CONFIGURATION_get_value_string (testing_cfg, "test_mesh_small",
                                             "data_output_file",
                                             &data_filename))
  {
    data_file =
        GNUNET_DISK_file_open (data_filename,
                               GNUNET_DISK_OPEN_READWRITE |
                               GNUNET_DISK_OPEN_CREATE,
                               GNUNET_DISK_PERM_USER_READ |
                               GNUNET_DISK_PERM_USER_WRITE);
    if (data_file == NULL)
    {
      GNUNET_log (GNUNET_ERROR_TYPE_WARNING, "Failed to open %s for output!\n",
                  data_filename);
      GNUNET_free (data_filename);
    }
  }

  if (GNUNET_YES ==
      GNUNET_CONFIGURATION_get_value_string (cfg, "test_mesh_small",
                                             "output_file", &temp_str))
  {
    output_file =
        GNUNET_DISK_file_open (temp_str,
                               GNUNET_DISK_OPEN_READWRITE |
                               GNUNET_DISK_OPEN_CREATE,
                               GNUNET_DISK_PERM_USER_READ |
                               GNUNET_DISK_PERM_USER_WRITE);
    if (output_file == NULL)
      GNUNET_log (GNUNET_ERROR_TYPE_WARNING, "Failed to open %s for output!\n",
                  temp_str);
  }
  GNUNET_free_non_null (temp_str);

  hosts = GNUNET_TESTING_hosts_load (testing_cfg);

  pg = GNUNET_TESTING_peergroup_start (testing_cfg, num_peers, TIMEOUT,
                                       &connect_cb, &peergroup_ready, NULL,
                                       hosts);
  GNUNET_assert (pg != NULL);
  shutdown_handle =
    GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_UNIT_FOREVER_REL,
                                    &shutdown_task, NULL);
}



/**
 * test_mesh_small command line options
 */
static struct GNUNET_GETOPT_CommandLineOption options[] = {
  {'V', "verbose", NULL,
   gettext_noop ("be verbose (print progress information)"),
   0, &GNUNET_GETOPT_set_one, &verbose},
  GNUNET_GETOPT_OPTION_END
};


/**
 * Main: start test
 */
int
main (int argc, char *argv[])
{
  char * argv2[] = {
    argv[0],
    "-c",
    "test_mesh_small.conf",
    NULL
  };
  int argc2 = (sizeof (argv2) / sizeof (char *)) - 1;

  initialized = GNUNET_NO;

  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "Start\n");
  if (strstr (argv[0], "test_mesh_small_unicast") != NULL)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "UNICAST\n");
    test = UNICAST;
    test_name = "unicast";
    ok_goal = 5;
  }
  else if (strstr (argv[0], "test_mesh_small_multicast") != NULL)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "MULTICAST\n");
    test = MULTICAST;
    test_name = "multicast";
    ok_goal = 10;
  }
  else if (strstr (argv[0], "test_mesh_small_speed_ack") != NULL)
  {
   /* Each peer is supposed to generate the following callbacks:
    * 1 incoming tunnel (@dest)
    * 1 connected peer (@orig)
    * TOTAL_PACKETS received data packet (@dest)
    * TOTAL_PACKETS received data packet (@orig)
    * 1 received tunnel destroy (@dest)
    * _________________________________
    * 5 x ok expected per peer
    */
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "SPEED_ACK\n");
    test = SPEED_ACK;
    test_name = "speed ack";
    ok_goal = TOTAL_PACKETS * 2 + 3;
    argv2 [3] = NULL; // remove -L DEBUG
  }
  else if (strstr (argv[0], "test_mesh_small_speed") != NULL)
  {
   /* Each peer is supposed to generate the following callbacks:
    * 1 incoming tunnel (@dest)
    * 1 connected peer (@orig)
    * 1 initial packet (@dest)
    * TOTAL_PACKETS received data packet (@dest)
    * 1 received data packet (@orig)
    * 1 received tunnel destroy (@dest)
    * _________________________________
    */
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "SPEED\n");
    ok_goal = TOTAL_PACKETS + 5;
    if (strstr (argv[0], "_min") != NULL)
    {
      test = SPEED_MIN;
      test_name = "speed min";
    }
    else if (strstr (argv[0], "_nobuf") != NULL)
    {
      test = SPEED_NOBUF;
      test_name = "speed nobuf";
    }
    else
    {
      test = SPEED;
      test_name = "speed";
    }
  }
  else
  {
    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "UNKNOWN\n");
    test = SETUP;
    ok_goal = 0;
  }

  if (strstr (argv[0], "backwards") != NULL)
  {
    char *aux;

    GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "BACKWARDS (LEAF TO ROOT)\n");
    test_backwards = GNUNET_YES;
    aux = malloc (32); // "leaked"
    sprintf (aux, "backwards %s", test_name);
    test_name = aux;
  }

  GNUNET_PROGRAM_run (argc2, argv2,
                      "test_mesh_small",
                      gettext_noop ("Test mesh in a small network."), options,
                      &run, NULL);
#if REMOVE_DIR
  GNUNET_DISK_directory_remove ("/tmp/test_mesh_small");
#endif
  if (ok_goal > ok)
  {
    GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                "FAILED! (%d/%d)\n", ok, ok_goal);
    return 1;
  }
  GNUNET_log (GNUNET_ERROR_TYPE_DEBUG, "success\n");
  return 0;
}

/* end of test_mesh_small.c */
