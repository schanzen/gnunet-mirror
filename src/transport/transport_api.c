/*
     This file is part of GNUnet.
     (C) 2009, 2010, 2011 Christian Grothoff (and other contributing authors)

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
 * @file transport/transport_api.c
 * @brief library to access the low-level P2P IO service
 * @author Christian Grothoff
 *
 * TODO:
 * - adjust testcases to use new 'try connect' style (should be easy, breaks API compatibility!)
 * - adjust core service to use new 'try connect' style (should be MUCH nicer there as well!)
 * - test test test
 */
#include "platform.h"
#include "gnunet_constants.h"
#include "gnunet_bandwidth_lib.h"
#include "gnunet_client_lib.h"
#include "gnunet_constants.h"
#include "gnunet_container_lib.h"
#include "gnunet_arm_service.h"
#include "gnunet_hello_lib.h"
#include "gnunet_protocols.h"
#include "gnunet_server_lib.h"
#include "gnunet_time_lib.h"
#include "gnunet_transport_service.h"
#include "transport.h"

#define LOG(kind,...) GNUNET_log_from (kind, "transport-api",__VA_ARGS__)

/**
 * How large to start with for the hashmap of neighbours.
 */
#define STARTING_NEIGHBOURS_SIZE 16

/**
 * Handle for a message that should be transmitted to the service.
 * Used for both control messages and normal messages.
 */
struct GNUNET_TRANSPORT_TransmitHandle
{

  /**
   * We keep all requests in a DLL.
   */
  struct GNUNET_TRANSPORT_TransmitHandle *next;

  /**
   * We keep all requests in a DLL.
   */
  struct GNUNET_TRANSPORT_TransmitHandle *prev;

  /**
   * Neighbour for this handle, NULL for control messages.
   */
  struct Neighbour *neighbour;

  /**
   * Function to call when notify_size bytes are available
   * for transmission.
   */
  GNUNET_CONNECTION_TransmitReadyNotify notify;

  /**
   * Closure for notify.
   */
  void *notify_cls;

  /**
   * Timeout for this request, 0 for control messages.
   */
  struct GNUNET_TIME_Absolute timeout;

  /**
   * Task to trigger request timeout if the request is stalled due to
   * congestion.
   */
  GNUNET_SCHEDULER_TaskIdentifier timeout_task;

  /**
   * How many bytes is our notify callback waiting for?
   */
  size_t notify_size;

  /**
   * How important is this message? Not used for control messages.
   */
  uint32_t priority;

};


/**
 * Entry in hash table of all of our current neighbours.
 */
struct Neighbour
{
  /**
   * Overall transport handle.
   */
  struct GNUNET_TRANSPORT_Handle *h;

  /**
   * Active transmit handle or NULL.
   */
  struct GNUNET_TRANSPORT_TransmitHandle *th;

  /**
   * Identity of this neighbour.
   */
  struct GNUNET_PeerIdentity id;

  /**
   * Outbound bandwidh tracker.
   */
  struct GNUNET_BANDWIDTH_Tracker out_tracker;

  /**
   * Entry in our readyness heap (which is sorted by 'next_ready'
   * value).  NULL if there is no pending transmission request for
   * this neighbour or if we're waiting for 'is_ready' to become
   * true AFTER the 'out_tracker' suggested that this peer's quota
   * has been satisfied (so once 'is_ready' goes to GNUNET_YES,
   * we should immediately go back into the heap).
   */
  struct GNUNET_CONTAINER_HeapNode *hn;

  /**
   * Is this peer currently ready to receive a message?
   */
  int is_ready;

  /**
   * Sending consumed more bytes on wire than payload was announced
   * This overhead is added to the delay of next sending operation
   */
  size_t traffic_overhead;
};


/**
 * Linked list of functions to call whenever our HELLO is updated.
 */
struct GNUNET_TRANSPORT_GetHelloHandle
{

  /**
   * This is a doubly linked list.
   */
  struct GNUNET_TRANSPORT_GetHelloHandle *next;

  /**
   * This is a doubly linked list.
   */
  struct GNUNET_TRANSPORT_GetHelloHandle *prev;

  /**
   * Transport handle.
   */
  struct GNUNET_TRANSPORT_Handle *handle;

  /**
   * Callback to call once we got our HELLO.
   */
  GNUNET_TRANSPORT_HelloUpdateCallback rec;

  /**
   * Closure for rec.
   */
  void *rec_cls;

};

/**
 * Linked list for all try-connect requests
 */
struct GNUNET_TRANSPORT_TryConnectHandle
{
  struct GNUNET_TRANSPORT_TryConnectHandle *prev;
  struct GNUNET_TRANSPORT_TryConnectHandle *next;

  struct GNUNET_PeerIdentity pid;

  struct GNUNET_TRANSPORT_Handle *th;
  struct GNUNET_TRANSPORT_TransmitHandle *tth;
  GNUNET_TRANSPORT_TryConnectCallback cb;
  void *cb_cls;
};


/**
 * Linked list for all try-connect requests
 */
struct GNUNET_TRANSPORT_OfferHelloHandle
{
  struct GNUNET_TRANSPORT_OfferHelloHandle *prev;
  struct GNUNET_TRANSPORT_OfferHelloHandle *next;

  struct GNUNET_TRANSPORT_Handle *th;

  struct GNUNET_TRANSPORT_TransmitHandle *tth;
  GNUNET_SCHEDULER_Task cont;

  void *cls;

  struct GNUNET_MessageHeader *msg;
};


/**
 * Handle for the transport service (includes all of the
 * state for the transport service).
 */
struct GNUNET_TRANSPORT_Handle
{

  /**
   * Closure for the callbacks.
   */
  void *cls;

  /**
   * Function to call for received data.
   */
  GNUNET_TRANSPORT_ReceiveCallback rec;

  /**
   * function to call on connect events
   */
  GNUNET_TRANSPORT_NotifyConnect nc_cb;

  /**
   * function to call on disconnect events
   */
  GNUNET_TRANSPORT_NotifyDisconnect nd_cb;

  /**
   * Head of DLL of control messages.
   */
  struct GNUNET_TRANSPORT_TransmitHandle *control_head;

  /**
   * Tail of DLL of control messages.
   */
  struct GNUNET_TRANSPORT_TransmitHandle *control_tail;

  /**
   * The current HELLO message for this peer.  Updated
   * whenever transports change their addresses.
   */
  struct GNUNET_HELLO_Message *my_hello;

  /**
   * My client connection to the transport service.
   */
  struct GNUNET_CLIENT_Connection *client;

  /**
   * Handle to our registration with the client for notification.
   */
  struct GNUNET_CLIENT_TransmitHandle *cth;

  /**
   * Linked list of pending requests for our HELLO.
   */
  struct GNUNET_TRANSPORT_GetHelloHandle *hwl_head;

  /**
   * Linked list of pending requests for our HELLO.
   */
  struct GNUNET_TRANSPORT_GetHelloHandle *hwl_tail;

  /**
   * Linked list of pending try connect requests head
   */
  struct GNUNET_TRANSPORT_TryConnectHandle *tc_head;

  /**
   * Linked list of pending try connect requests tail
   */
  struct GNUNET_TRANSPORT_TryConnectHandle *tc_tail;

  /**
   * Linked list of pending offer HELLO requests head
   */
  struct GNUNET_TRANSPORT_OfferHelloHandle *oh_head;

  /**
   * Linked list of pending offer HELLO requests tail
   */
  struct GNUNET_TRANSPORT_OfferHelloHandle *oh_tail;

  /**
   * My configuration.
   */
  const struct GNUNET_CONFIGURATION_Handle *cfg;

  /**
   * Hash map of the current connected neighbours of this peer.
   * Maps peer identities to 'struct Neighbour' entries.
   */
  struct GNUNET_CONTAINER_MultiHashMap *neighbours;

  /**
   * Heap sorting peers with pending messages by the timestamps that
   * specify when we could next send a message to the respective peer.
   * Excludes control messages (which can always go out immediately).
   * Maps time stamps to 'struct Neighbour' entries.
   */
  struct GNUNET_CONTAINER_Heap *ready_heap;

  /**
   * Peer identity as assumed by this process, or all zeros.
   */
  struct GNUNET_PeerIdentity self;

  /**
   * ID of the task trying to reconnect to the service.
   */
  GNUNET_SCHEDULER_TaskIdentifier reconnect_task;

  /**
   * ID of the task trying to trigger transmission for a peer while
   * maintaining bandwidth quotas.  In use if there are no control
   * messages and the smallest entry in the 'ready_heap' has a time
   * stamp in the future.
   */
  GNUNET_SCHEDULER_TaskIdentifier quota_task;

  /**
   * Delay until we try to reconnect.
   */
  struct GNUNET_TIME_Relative reconnect_delay;

  /**
   * Should we check that 'self' matches what the service thinks?
   * (if GNUNET_NO, then 'self' is all zeros!).
   */
  int check_self;

  /**
   * Reconnect in progress
   */
  int reconnecting;
};



/**
 * Schedule the task to send one message, either from the control
 * list or the peer message queues  to the service.
 *
 * @param h transport service to schedule a transmission for
 */
static void
schedule_transmission (struct GNUNET_TRANSPORT_Handle *h);


/**
 * Function that will schedule the job that will try
 * to connect us again to the client.
 *
 * @param h transport service to reconnect
 */
static void
disconnect_and_schedule_reconnect (struct GNUNET_TRANSPORT_Handle *h);


/**
 * Get the neighbour list entry for the given peer
 *
 * @param h our context
 * @param peer peer to look up
 * @return NULL if no such peer entry exists
 */
static struct Neighbour *
neighbour_find (struct GNUNET_TRANSPORT_Handle *h,
                const struct GNUNET_PeerIdentity *peer)
{
  return GNUNET_CONTAINER_multihashmap_get (h->neighbours, &peer->hashPubKey);
}


/**
 * Add neighbour to our list
 *
 * @return NULL if this API is currently disconnecting from the service
 */
static struct Neighbour *
neighbour_add (struct GNUNET_TRANSPORT_Handle *h,
               const struct GNUNET_PeerIdentity *pid)
{
  struct Neighbour *n;

  LOG (GNUNET_ERROR_TYPE_DEBUG, "Creating entry for neighbour `%4s'.\n",
       GNUNET_i2s (pid));
  n = GNUNET_malloc (sizeof (struct Neighbour));
  n->id = *pid;
  n->h = h;
  n->is_ready = GNUNET_YES;
  n->traffic_overhead = 0;
  GNUNET_BANDWIDTH_tracker_init (&n->out_tracker,
                                 GNUNET_CONSTANTS_DEFAULT_BW_IN_OUT,
                                 MAX_BANDWIDTH_CARRY_S);
  GNUNET_assert (GNUNET_OK ==
                 GNUNET_CONTAINER_multihashmap_put (h->neighbours,
                                                    &n->id.hashPubKey, n,
                                                    GNUNET_CONTAINER_MULTIHASHMAPOPTION_UNIQUE_ONLY));
  return n;
}


/**
 * Iterator over hash map entries, for deleting state of a neighbour.
 *
 * @param cls the 'struct GNUNET_TRANSPORT_Handle*'
 * @param key peer identity
 * @param value value in the hash map, the neighbour entry to delete
 * @return GNUNET_YES if we should continue to
 *         iterate,
 *         GNUNET_NO if not.
 */
static int
neighbour_delete (void *cls, const struct GNUNET_HashCode * key, void *value)
{
  struct GNUNET_TRANSPORT_Handle *handle = cls;
  struct Neighbour *n = value;

  if (NULL != handle->nd_cb)
    handle->nd_cb (handle->cls, &n->id);
  GNUNET_assert (NULL == n->th);
  GNUNET_assert (NULL == n->hn);
  GNUNET_assert (GNUNET_YES ==
                 GNUNET_CONTAINER_multihashmap_remove (handle->neighbours, key,
                                                       n));
  GNUNET_free (n);
  return GNUNET_YES;
}


/**
 * Function we use for handling incoming messages.
 *
 * @param cls closure (struct GNUNET_TRANSPORT_Handle *)
 * @param msg message received, NULL on timeout or fatal error
 */
static void
demultiplexer (void *cls, const struct GNUNET_MessageHeader *msg)
{
  struct GNUNET_TRANSPORT_Handle *h = cls;
  const struct DisconnectInfoMessage *dim;
  const struct ConnectInfoMessage *cim;
  const struct InboundMessage *im;
  const struct GNUNET_MessageHeader *imm;
  const struct SendOkMessage *okm;
  const struct QuotaSetMessage *qm;
  const struct GNUNET_ATS_Information *ats;
  struct GNUNET_TRANSPORT_GetHelloHandle *hwl;
  struct GNUNET_TRANSPORT_GetHelloHandle *next_hwl;
  struct Neighbour *n;
  struct GNUNET_PeerIdentity me;
  uint16_t size;
  uint32_t ats_count;
  uint32_t bytes_msg;
  uint32_t bytes_physical;

  GNUNET_assert (h->client != NULL);
  if (msg == NULL)
  {
    LOG (GNUNET_ERROR_TYPE_DEBUG,
         "Error receiving from transport service, disconnecting temporarily.\n");
    disconnect_and_schedule_reconnect (h);
    return;
  }
  GNUNET_CLIENT_receive (h->client, &demultiplexer, h,
                         GNUNET_TIME_UNIT_FOREVER_REL);
  size = ntohs (msg->size);
  switch (ntohs (msg->type))
  {
  case GNUNET_MESSAGE_TYPE_HELLO:
    if (GNUNET_OK !=
        GNUNET_HELLO_get_id ((const struct GNUNET_HELLO_Message *) msg, &me))
    {
      GNUNET_break (0);
      break;
    }
    LOG (GNUNET_ERROR_TYPE_DEBUG,
         "Receiving (my own) `%s' message, I am `%4s'.\n", "HELLO",
         GNUNET_i2s (&me));
    GNUNET_free_non_null (h->my_hello);
    h->my_hello = NULL;
    if (size < sizeof (struct GNUNET_MessageHeader))
    {
      GNUNET_break (0);
      break;
    }
    h->my_hello = GNUNET_malloc (size);
    memcpy (h->my_hello, msg, size);
    hwl = h->hwl_head;
    while (NULL != hwl)
    {
      next_hwl = hwl->next;
      hwl->rec (hwl->rec_cls,
                (const struct GNUNET_MessageHeader *) h->my_hello);
      hwl = next_hwl;
    }
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_CONNECT:
    if (size < sizeof (struct ConnectInfoMessage))
    {
      GNUNET_break (0);
      break;
    }
    cim = (const struct ConnectInfoMessage *) msg;
    ats_count = ntohl (cim->ats_count);
    if (size !=
        sizeof (struct ConnectInfoMessage) +
        ats_count * sizeof (struct GNUNET_ATS_Information))
    {
      GNUNET_break (0);
      break;
    }
    ats = (const struct GNUNET_ATS_Information *) &cim[1];
    LOG (GNUNET_ERROR_TYPE_DEBUG, "Receiving `%s' message for `%4s'.\n",
         "CONNECT", GNUNET_i2s (&cim->id));
    n = neighbour_find (h, &cim->id);
    if (n != NULL)
    {
      GNUNET_break (0);
      break;
    }
    n = neighbour_add (h, &cim->id);
    LOG (GNUNET_ERROR_TYPE_DEBUG, "Receiving `%s' message for `%4s' with quota %u\n",
         "CONNECT", GNUNET_i2s (&cim->id), ntohl (cim->quota_out.value__));
    GNUNET_BANDWIDTH_tracker_update_quota (&n->out_tracker, cim->quota_out);
    if (h->nc_cb != NULL)
      h->nc_cb (h->cls, &n->id, ats, ats_count);
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_DISCONNECT:
    if (size != sizeof (struct DisconnectInfoMessage))
    {
      GNUNET_break (0);
      break;
    }
    dim = (const struct DisconnectInfoMessage *) msg;
    GNUNET_break (ntohl (dim->reserved) == 0);
    LOG (GNUNET_ERROR_TYPE_DEBUG, "Receiving `%s' message for `%4s'.\n",
         "DISCONNECT", GNUNET_i2s (&dim->peer));
    n = neighbour_find (h, &dim->peer);
    if (n == NULL)
    {
      GNUNET_break (0);
      break;
    }
    neighbour_delete (h, &dim->peer.hashPubKey, n);
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_SEND_OK:
    if (size != sizeof (struct SendOkMessage))
    {
      GNUNET_break (0);
      break;
    }
    okm = (const struct SendOkMessage *) msg;
    bytes_msg = ntohl (okm->bytes_msg);
    bytes_physical = ntohl (okm->bytes_physical);
    LOG (GNUNET_ERROR_TYPE_DEBUG, "Receiving `%s' message, transmission %s.\n",
         "SEND_OK", ntohl (okm->success) == GNUNET_OK ? "succeeded" : "failed");

    n = neighbour_find (h, &okm->peer);
    if (n == NULL)
      break;

    if (bytes_physical >= bytes_msg)
    {
        LOG (GNUNET_ERROR_TYPE_DEBUG, "Overhead for %u byte message: %u \n",
            bytes_msg, bytes_physical - bytes_msg);
      n->traffic_overhead += bytes_physical - bytes_msg;
    }
    GNUNET_break (GNUNET_NO == n->is_ready);
    n->is_ready = GNUNET_YES;
    if ((n->th != NULL) && (n->hn == NULL))
    {
      GNUNET_assert (GNUNET_SCHEDULER_NO_TASK != n->th->timeout_task);
      GNUNET_SCHEDULER_cancel (n->th->timeout_task);
      n->th->timeout_task = GNUNET_SCHEDULER_NO_TASK;
      /* we've been waiting for this (congestion, not quota,
       * caused delayed transmission) */
      n->hn = GNUNET_CONTAINER_heap_insert (h->ready_heap, n, 0);
      schedule_transmission (h);
    }
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_RECV:
    LOG (GNUNET_ERROR_TYPE_DEBUG, "Receiving `%s' message.\n", "RECV");
    if (size <
        sizeof (struct InboundMessage) + sizeof (struct GNUNET_MessageHeader))
    {
      GNUNET_break (0);
      break;
    }
    im = (const struct InboundMessage *) msg;
    ats_count = ntohl (im->ats_count);
    ats = (const struct GNUNET_ATS_Information *) &im[1];
    imm = (const struct GNUNET_MessageHeader *) &ats[ats_count];
    if (ntohs (imm->size) + sizeof (struct InboundMessage) +
        ats_count * sizeof (struct GNUNET_ATS_Information) != size)
    {
      GNUNET_break (0);
      break;
    }
    LOG (GNUNET_ERROR_TYPE_DEBUG, "Received message of type %u from `%4s'.\n",
         ntohs (imm->type), GNUNET_i2s (&im->peer));
    n = neighbour_find (h, &im->peer);
    if (n == NULL)
    {
      GNUNET_break (0);
      break;
    }
    if (h->rec != NULL)
      h->rec (h->cls, &im->peer, imm, ats, ats_count);
    break;
  case GNUNET_MESSAGE_TYPE_TRANSPORT_SET_QUOTA:
    LOG (GNUNET_ERROR_TYPE_DEBUG, "Receiving `%s' message.\n", "SET_QUOTA");
    if (size != sizeof (struct QuotaSetMessage))
    {
      GNUNET_break (0);
      break;
    }
    qm = (const struct QuotaSetMessage *) msg;
    n = neighbour_find (h, &qm->peer);
    if (n == NULL)
      break;
    LOG (GNUNET_ERROR_TYPE_DEBUG, "Receiving `%s' message for `%4s' with quota %u\n",
         "SET_QUOTA", GNUNET_i2s (&qm->peer), ntohl (qm->quota.value__));
    GNUNET_BANDWIDTH_tracker_update_quota (&n->out_tracker, qm->quota);
    break;
  default:
    LOG (GNUNET_ERROR_TYPE_ERROR,
         _("Received unexpected message of type %u in %s:%u\n"),
         ntohs (msg->type), __FILE__, __LINE__);
    GNUNET_break (0);
    break;
  }
}


/**
 * A transmission request could not be satisfied because of
 * network congestion.  Notify the initiator and clean up.
 *
 * @param cls the 'struct GNUNET_TRANSPORT_TransmitHandle'
 * @param tc scheduler context
 */
static void
timeout_request_due_to_congestion (void *cls,
                                   const struct GNUNET_SCHEDULER_TaskContext
                                   *tc)
{
  struct GNUNET_TRANSPORT_TransmitHandle *th = cls;
  struct Neighbour *n = th->neighbour;

  n->th->timeout_task = GNUNET_SCHEDULER_NO_TASK;
  GNUNET_assert (th == n->th);
  GNUNET_assert (NULL == n->hn);
  n->th = NULL;
  th->notify (th->notify_cls, 0, NULL);
  GNUNET_free (th);
}


/**
 * Transmit message(s) to service.
 *
 * @param cls handle to transport
 * @param size number of bytes available in buf
 * @param buf where to copy the message
 * @return number of bytes copied to buf
 */
static size_t
transport_notify_ready (void *cls, size_t size, void *buf)
{
  struct GNUNET_TRANSPORT_Handle *h = cls;
  struct GNUNET_TRANSPORT_TransmitHandle *th;
  struct Neighbour *n;
  char *cbuf;
  struct OutboundMessage obm;
  size_t ret;
  size_t nret;
  size_t mret;

  GNUNET_assert (NULL != h->client);
  h->cth = NULL;
  if (NULL == buf)
  {
    /* transmission failed */
    disconnect_and_schedule_reconnect (h);
    return 0;
  }

  cbuf = buf;
  ret = 0;
  /* first send control messages */
  while ((NULL != (th = h->control_head)) && (th->notify_size <= size))
  {
    GNUNET_CONTAINER_DLL_remove (h->control_head, h->control_tail, th);
    nret = th->notify (th->notify_cls, size, &cbuf[ret]);
    LOG (GNUNET_ERROR_TYPE_DEBUG, "Added %u bytes of control message at %u\n",
         nret, ret);
    GNUNET_free (th);
    ret += nret;
    size -= nret;
  }

  /* then, if possible and no control messages pending, send data messages */
  while ((NULL == h->control_head) &&
         (NULL != (n = GNUNET_CONTAINER_heap_peek (h->ready_heap))))
  {
    if (GNUNET_YES != n->is_ready)
    {
      /* peer not ready, wait for notification! */
      GNUNET_assert (n == GNUNET_CONTAINER_heap_remove_root (h->ready_heap));
      n->hn = NULL;
      GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == n->th->timeout_task);
      n->th->timeout_task =
          GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_absolute_get_remaining
                                        (n->th->timeout),
                                        &timeout_request_due_to_congestion,
                                        n->th);
      continue;
    }
    th = n->th;
    if (th->notify_size + sizeof (struct OutboundMessage) > size)
      break;                    /* does not fit */
    if (GNUNET_BANDWIDTH_tracker_get_delay
        (&n->out_tracker, th->notify_size).rel_value > 0)
      break;                    /* too early */
    GNUNET_assert (n == GNUNET_CONTAINER_heap_remove_root (h->ready_heap));
    n->hn = NULL;
    n->th = NULL;
    n->is_ready = GNUNET_NO;
    GNUNET_assert (size >= sizeof (struct OutboundMessage));
    mret =
        th->notify (th->notify_cls, size - sizeof (struct OutboundMessage),
                    &cbuf[ret + sizeof (struct OutboundMessage)]);
    GNUNET_assert (mret <= size - sizeof (struct OutboundMessage));
    if (mret != 0)
    {
      GNUNET_assert (mret + sizeof (struct OutboundMessage) <
                     GNUNET_SERVER_MAX_MESSAGE_SIZE);
      obm.header.type = htons (GNUNET_MESSAGE_TYPE_TRANSPORT_SEND);
      obm.header.size = htons (mret + sizeof (struct OutboundMessage));
      obm.priority = htonl (th->priority);
      obm.timeout =
          GNUNET_TIME_relative_hton (GNUNET_TIME_absolute_get_remaining
                                     (th->timeout));
      obm.peer = n->id;
      memcpy (&cbuf[ret], &obm, sizeof (struct OutboundMessage));
      ret += (mret + sizeof (struct OutboundMessage));
      size -= (mret + sizeof (struct OutboundMessage));
      GNUNET_BANDWIDTH_tracker_consume (&n->out_tracker, mret);
    }
    GNUNET_free (th);
  }
  /* if there are more pending messages, try to schedule those */
  schedule_transmission (h);
  LOG (GNUNET_ERROR_TYPE_DEBUG, "Transmitting %u bytes to transport service\n",
       ret);
  return ret;
}


/**
 * Schedule the task to send one message, either from the control
 * list or the peer message queues  to the service.
 *
 * @param cls transport service to schedule a transmission for
 * @param tc scheduler context
 */
static void
schedule_transmission_task (void *cls,
                            const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct GNUNET_TRANSPORT_Handle *h = cls;
  size_t size;
  struct GNUNET_TRANSPORT_TransmitHandle *th;
  struct Neighbour *n;

  h->quota_task = GNUNET_SCHEDULER_NO_TASK;
  GNUNET_assert (NULL != h->client);
  /* destroy all requests that have timed out */
  while ((NULL != (n = GNUNET_CONTAINER_heap_peek (h->ready_heap))) &&
         (GNUNET_TIME_absolute_get_remaining (n->th->timeout).rel_value == 0))
  {
    /* notify client that the request could not be satisfied within
     * the given time constraints */
    th = n->th;
    n->th = NULL;
    GNUNET_assert (n == GNUNET_CONTAINER_heap_remove_root (h->ready_heap));
    n->hn = NULL;
    LOG (GNUNET_ERROR_TYPE_DEBUG,
         "Signalling timeout for transmission to peer %s due to congestion\n",
         GNUNET_i2s (&n->id));
    GNUNET_assert (0 == th->notify (th->notify_cls, 0, NULL));
    GNUNET_free (th);
  }
  if (NULL != h->cth)
    return;
  if (NULL != h->control_head)
  {
    size = h->control_head->notify_size;
  }
  else
  {
    n = GNUNET_CONTAINER_heap_peek (h->ready_heap);
    if (NULL == n)
      return;                   /* no pending messages */
    size = n->th->notify_size + sizeof (struct OutboundMessage);
  }
  LOG (GNUNET_ERROR_TYPE_DEBUG, "Calling notify_transmit_ready\n");
  h->cth =
      GNUNET_CLIENT_notify_transmit_ready (h->client, size,
                                           GNUNET_TIME_UNIT_FOREVER_REL,
                                           GNUNET_NO, &transport_notify_ready,
                                           h);
  GNUNET_assert (NULL != h->cth);
}


/**
 * Schedule the task to send one message, either from the control
 * list or the peer message queues  to the service.
 *
 * @param h transport service to schedule a transmission for
 */
static void
schedule_transmission (struct GNUNET_TRANSPORT_Handle *h)
{
  struct GNUNET_TIME_Relative delay;
  struct Neighbour *n;

  GNUNET_assert (NULL != h->client);
  if (h->quota_task != GNUNET_SCHEDULER_NO_TASK)
  {
    GNUNET_SCHEDULER_cancel (h->quota_task);
    h->quota_task = GNUNET_SCHEDULER_NO_TASK;
  }
  if (NULL != h->control_head)
    delay = GNUNET_TIME_UNIT_ZERO;
  else if (NULL != (n = GNUNET_CONTAINER_heap_peek (h->ready_heap)))
  {
    delay =
        GNUNET_BANDWIDTH_tracker_get_delay (&n->out_tracker,
                                            n->th->notify_size + n->traffic_overhead);
    n->traffic_overhead = 0;
  }
  else
    return;                     /* no work to be done */
  LOG (GNUNET_ERROR_TYPE_DEBUG,
       "Scheduling next transmission to service in %llu ms\n",
       (unsigned long long) delay.rel_value);
  h->quota_task =
      GNUNET_SCHEDULER_add_delayed (delay, &schedule_transmission_task, h);
}


/**
 * Queue control request for transmission to the transport
 * service.
 *
 * @param h handle to the transport service
 * @param size number of bytes to be transmitted
 * @param notify function to call to get the content
 * @param notify_cls closure for notify
 * @return a GNUNET_TRANSPORT_TransmitHandle
 */
static struct GNUNET_TRANSPORT_TransmitHandle *
schedule_control_transmit (struct GNUNET_TRANSPORT_Handle *h, size_t size,
                           GNUNET_CONNECTION_TransmitReadyNotify notify,
                           void *notify_cls)
{
  struct GNUNET_TRANSPORT_TransmitHandle *th;

  LOG (GNUNET_ERROR_TYPE_DEBUG, "Control transmit of %u bytes requested\n",
       size);
  th = GNUNET_malloc (sizeof (struct GNUNET_TRANSPORT_TransmitHandle));
  th->notify = notify;
  th->notify_cls = notify_cls;
  th->notify_size = size;
  GNUNET_CONTAINER_DLL_insert_tail (h->control_head, h->control_tail, th);
  schedule_transmission (h);
  return th;
}


/**
 * Transmit START message to service.
 *
 * @param cls unused
 * @param size number of bytes available in buf
 * @param buf where to copy the message
 * @return number of bytes copied to buf
 */
static size_t
send_start (void *cls, size_t size, void *buf)
{
  struct GNUNET_TRANSPORT_Handle *h = cls;
  struct StartMessage s;
  uint32_t options;

  if (buf == NULL)
  {
    /* Can only be shutdown, just give up */
    LOG (GNUNET_ERROR_TYPE_DEBUG,
         "Shutdown while trying to transmit `%s' request.\n", "START");
    return 0;
  }
  LOG (GNUNET_ERROR_TYPE_DEBUG, "Transmitting `%s' request.\n", "START");
  GNUNET_assert (size >= sizeof (struct StartMessage));
  s.header.size = htons (sizeof (struct StartMessage));
  s.header.type = htons (GNUNET_MESSAGE_TYPE_TRANSPORT_START);
  options = 0;
  if (h->check_self)
    options |= 1;
  if (h->rec != NULL)
    options |= 2;
  s.options = htonl (options);
  s.self = h->self;
  memcpy (buf, &s, sizeof (struct StartMessage));
  GNUNET_CLIENT_receive (h->client, &demultiplexer, h,
                         GNUNET_TIME_UNIT_FOREVER_REL);
  return sizeof (struct StartMessage);
}


/**
 * Try again to connect to transport service.
 *
 * @param cls the handle to the transport service
 * @param tc scheduler context
 */
static void
reconnect (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct GNUNET_TRANSPORT_Handle *h = cls;

  h->reconnect_task = GNUNET_SCHEDULER_NO_TASK;
  if ((tc->reason & GNUNET_SCHEDULER_REASON_SHUTDOWN) != 0)
  {
    /* shutdown, just give up */
    return;
  }
  LOG (GNUNET_ERROR_TYPE_DEBUG, "Connecting to transport service.\n");
  GNUNET_assert (h->client == NULL);
  GNUNET_assert (h->control_head == NULL);
  GNUNET_assert (h->control_tail == NULL);
  h->client = GNUNET_CLIENT_connect ("transport", h->cfg);
  GNUNET_assert (h->client != NULL);
  schedule_control_transmit (h, sizeof (struct StartMessage), &send_start, h);
}


/**
 * Function that will schedule the job that will try
 * to connect us again to the client.
 *
 * @param h transport service to reconnect
 */
static void
disconnect_and_schedule_reconnect (struct GNUNET_TRANSPORT_Handle *h)
{
  struct GNUNET_TRANSPORT_TransmitHandle *th;

  GNUNET_assert (h->reconnect_task == GNUNET_SCHEDULER_NO_TASK);
  if (NULL != h->cth)
  {
    GNUNET_CLIENT_notify_transmit_ready_cancel (h->cth);
    h->cth = NULL;
  }
  if (NULL != h->client)
  {
    GNUNET_CLIENT_disconnect (h->client);
    h->client = NULL;
  }
  /* Forget about all neighbours that we used to be connected to */
  GNUNET_CONTAINER_multihashmap_iterate (h->neighbours, &neighbour_delete, h);
  if (h->quota_task != GNUNET_SCHEDULER_NO_TASK)
  {
    GNUNET_SCHEDULER_cancel (h->quota_task);
    h->quota_task = GNUNET_SCHEDULER_NO_TASK;
  }
  while ((NULL != (th = h->control_head)))
  {
    GNUNET_CONTAINER_DLL_remove (h->control_head, h->control_tail, th);
    th->notify (th->notify_cls, 0, NULL);
    GNUNET_free (th);
  }
  LOG (GNUNET_ERROR_TYPE_DEBUG,
       "Scheduling task to reconnect to transport service in %llu ms.\n",
       h->reconnect_delay.rel_value);
  h->reconnect_task =
      GNUNET_SCHEDULER_add_delayed (h->reconnect_delay, &reconnect, h);
  h->reconnect_delay = GNUNET_TIME_STD_BACKOFF (h->reconnect_delay);
}


/**
 * Cancel control request for transmission to the transport service.
 *
 * @param th handle to the transport service
 * @param tth transmit handle to cancel
 */
static void
cancel_control_transmit (struct GNUNET_TRANSPORT_Handle *th, struct GNUNET_TRANSPORT_TransmitHandle *tth)
{
  GNUNET_assert (NULL != th);
  GNUNET_assert (NULL != tth);

  LOG (GNUNET_ERROR_TYPE_DEBUG, "Canceling transmit of contral transmission requested\n");

  GNUNET_CONTAINER_DLL_remove (th->control_head, th->control_tail, tth);
  GNUNET_free (tth);
}



/**
 * Send REQUEST_CONNECT message to the service.
 *
 * @param cls the 'struct GNUNET_PeerIdentity'
 * @param size number of bytes available in buf
 * @param buf where to copy the message
 * @return number of bytes copied to buf
 */
static size_t
send_try_connect (void *cls, size_t size, void *buf)
{
  struct GNUNET_TRANSPORT_TryConnectHandle *tch = cls;
  struct TransportRequestConnectMessage msg;

  if (buf == NULL)
  {
    if (NULL != tch->cb)
      tch->cb (tch->cb_cls, GNUNET_SYSERR);
    GNUNET_CONTAINER_DLL_remove (tch->th->tc_head, tch->th->tc_tail, tch);
    GNUNET_free (tch);
    return 0;
  }
  LOG (GNUNET_ERROR_TYPE_DEBUG,
       "Transmitting `%s' request with respect to `%4s'.\n", "REQUEST_CONNECT",
       GNUNET_i2s (&tch->pid));
  GNUNET_assert (size >= sizeof (struct TransportRequestConnectMessage));
  msg.header.size = htons (sizeof (struct TransportRequestConnectMessage));
  msg.header.type = htons (GNUNET_MESSAGE_TYPE_TRANSPORT_REQUEST_CONNECT);
  msg.reserved = htonl (0);
  msg.peer = tch->pid;
  memcpy (buf, &msg, sizeof (msg));
  if (NULL != tch->cb)
    tch->cb (tch->cb_cls, GNUNET_OK);
  GNUNET_CONTAINER_DLL_remove (tch->th->tc_head, tch->th->tc_tail, tch);
  GNUNET_free (tch);
  return sizeof (struct TransportRequestConnectMessage);
}

/**
 * Ask the transport service to establish a connection to
 * the given peer.
 *
 * @param handle connection to transport service
 * @param target who we should try to connect to
 * @param cb callback to be called when request was transmitted to transport
 *         service
 * @param cb_cls closure for the callback
 * @return a GNUNET_TRANSPORT_TryConnectHandle handle or
 *         NULL on failure (cb will not be called)
 */
struct GNUNET_TRANSPORT_TryConnectHandle *
GNUNET_TRANSPORT_try_connect (struct GNUNET_TRANSPORT_Handle *handle,
                              const struct GNUNET_PeerIdentity *target,
                              GNUNET_TRANSPORT_TryConnectCallback cb,
                              void *cb_cls)
{
  struct GNUNET_TRANSPORT_TryConnectHandle *tch = NULL;

  if (NULL == handle->client)
      return NULL;

  tch = GNUNET_malloc (sizeof (struct GNUNET_TRANSPORT_TryConnectHandle));
  tch->th = handle;
  tch->pid = *(target);
  tch->cb = cb;
  tch->cb_cls = cb_cls;
  tch->tth = schedule_control_transmit (handle,
                             sizeof (struct TransportRequestConnectMessage),
                             &send_try_connect, tch);
  GNUNET_CONTAINER_DLL_insert(handle->tc_head, handle->tc_tail, tch);
  return tch;
}


/**
 * Cancel the request to transport to try a connect
 * Callback will not be called
 *
 * @param tch GNUNET_TRANSPORT_TryConnectHandle handle to cancel
 */
void
GNUNET_TRANSPORT_try_connect_cancel (struct GNUNET_TRANSPORT_TryConnectHandle *tch)
{
  struct GNUNET_TRANSPORT_Handle *th;
  GNUNET_assert (NULL != tch);

  th = tch->th;
  cancel_control_transmit (th, tch->tth);
  GNUNET_CONTAINER_DLL_remove (th->tc_head, th->tc_tail, tch);
  GNUNET_free (tch);
}

/**
 * Send HELLO message to the service.
 *
 * @param cls the HELLO message to send
 * @param size number of bytes available in buf
 * @param buf where to copy the message
 * @return number of bytes copied to buf
 */
static size_t
send_hello (void *cls, size_t size, void *buf)
{
  struct GNUNET_TRANSPORT_OfferHelloHandle *ohh = cls;
  struct GNUNET_MessageHeader *msg = ohh->msg;
  uint16_t ssize;
  struct GNUNET_SCHEDULER_TaskContext tc;
  tc.read_ready = NULL;
  tc.write_ready = NULL;
  tc.reason = GNUNET_SCHEDULER_REASON_TIMEOUT;

  if (buf == NULL)
  {
    LOG (GNUNET_ERROR_TYPE_DEBUG,
         "Timeout while trying to transmit `%s' request.\n", "HELLO");
    if (NULL != ohh->cont)
      ohh->cont (ohh->cls, &tc);
    GNUNET_free (msg);
    GNUNET_CONTAINER_DLL_remove (ohh->th->oh_head, ohh->th->oh_tail, ohh);
    GNUNET_free (ohh);
    return 0;
  }
  LOG (GNUNET_ERROR_TYPE_DEBUG, "Transmitting `%s' request.\n", "HELLO");
  ssize = ntohs (msg->size);
  GNUNET_assert (size >= ssize);
  memcpy (buf, msg, ssize);
  GNUNET_free (msg);
  tc.reason = GNUNET_SCHEDULER_REASON_READ_READY;
  if (NULL != ohh->cont)
    ohh->cont (ohh->cls, &tc);
  GNUNET_CONTAINER_DLL_remove (ohh->th->oh_head, ohh->th->oh_tail, ohh);
  GNUNET_free (ohh);
  return ssize;
}


/**
 * Offer the transport service the HELLO of another peer.  Note that
 * the transport service may just ignore this message if the HELLO is
 * malformed or useless due to our local configuration.
 *
 * @param handle connection to transport service
 * @param hello the hello message
 * @param cont continuation to call when HELLO has been sent,
 * 	tc reason GNUNET_SCHEDULER_REASON_TIMEOUT for fail
 * 	tc reasong GNUNET_SCHEDULER_REASON_READ_READY for success
 * @param cls closure for continuation
 * @return a GNUNET_TRANSPORT_OfferHelloHandle handle or NULL on failure,
 *      in case of failure cont will not be called
 *
 */
struct GNUNET_TRANSPORT_OfferHelloHandle *
GNUNET_TRANSPORT_offer_hello (struct GNUNET_TRANSPORT_Handle *handle,
                              const struct GNUNET_MessageHeader *hello,
                              GNUNET_SCHEDULER_Task cont, void *cls)
{
  struct GNUNET_TRANSPORT_OfferHelloHandle *ohh;
  struct GNUNET_MessageHeader *msg;
  struct GNUNET_PeerIdentity peer;
  uint16_t size;

  GNUNET_assert (NULL != handle);
  GNUNET_assert (NULL != hello);

  if (NULL == handle->client)
    return NULL;

  GNUNET_break (ntohs (hello->type) == GNUNET_MESSAGE_TYPE_HELLO);
  size = ntohs (hello->size);
  GNUNET_break (size >= sizeof (struct GNUNET_MessageHeader));
  if (GNUNET_OK !=
      GNUNET_HELLO_get_id ((const struct GNUNET_HELLO_Message *) hello, &peer))
  {
    GNUNET_break (0);
    return NULL;
  }

  msg = GNUNET_malloc (size);
  memcpy (msg, hello, size);
  LOG (GNUNET_ERROR_TYPE_DEBUG,
       "Offering `%s' message of `%4s' to transport for validation.\n", "HELLO",
       GNUNET_i2s (&peer));

  ohh = GNUNET_malloc (sizeof (struct GNUNET_TRANSPORT_OfferHelloHandle));
  ohh->th = handle;
  ohh->cont = cont;
  ohh->cls = cls;
  ohh->msg = msg;
  ohh->tth = schedule_control_transmit (handle, size, &send_hello, ohh);
  GNUNET_CONTAINER_DLL_insert (handle->oh_head, handle->oh_tail, ohh);
  return ohh;
}


/**
 * Cancel the request to transport to offer the HELLO message
 *
 * @param ohh the GNUNET_TRANSPORT_OfferHelloHandle to cancel
 */
void
GNUNET_TRANSPORT_offer_hello_cancel (struct GNUNET_TRANSPORT_OfferHelloHandle *ohh)
{
  struct GNUNET_TRANSPORT_Handle *th = ohh->th;
  GNUNET_assert (NULL != ohh);

  cancel_control_transmit (ohh->th, ohh->tth);
  GNUNET_CONTAINER_DLL_remove (th->oh_head, th->oh_tail, ohh);
  GNUNET_free (ohh->msg);
  GNUNET_free (ohh);
}



/**
 * Obtain the HELLO message for this peer.
 *
 * @param handle connection to transport service
 * @param rec function to call with the HELLO, sender will be our peer
 *            identity; message and sender will be NULL on timeout
 *            (handshake with transport service pending/failed).
 *             cost estimate will be 0.
 * @param rec_cls closure for rec
 * @return handle to cancel the operation
 */
struct GNUNET_TRANSPORT_GetHelloHandle *
GNUNET_TRANSPORT_get_hello (struct GNUNET_TRANSPORT_Handle *handle,
                            GNUNET_TRANSPORT_HelloUpdateCallback rec,
                            void *rec_cls)
{
  struct GNUNET_TRANSPORT_GetHelloHandle *hwl;

  hwl = GNUNET_malloc (sizeof (struct GNUNET_TRANSPORT_GetHelloHandle));
  hwl->rec = rec;
  hwl->rec_cls = rec_cls;
  hwl->handle = handle;
  GNUNET_CONTAINER_DLL_insert (handle->hwl_head, handle->hwl_tail, hwl);
  if (handle->my_hello != NULL)
    rec (rec_cls, (const struct GNUNET_MessageHeader *) handle->my_hello);
  return hwl;
}


/**
 * Stop receiving updates about changes to our HELLO message.
 *
 * @param ghh handle to cancel
 */
void
GNUNET_TRANSPORT_get_hello_cancel (struct GNUNET_TRANSPORT_GetHelloHandle *ghh)
{
  struct GNUNET_TRANSPORT_Handle *handle = ghh->handle;

  GNUNET_CONTAINER_DLL_remove (handle->hwl_head, handle->hwl_tail, ghh);
  GNUNET_free (ghh);
}


/**
 * Connect to the transport service.  Note that the connection may
 * complete (or fail) asynchronously.
 *
 * @param cfg configuration to use
 * @param self our own identity (API should check that it matches
 *             the identity found by transport), or NULL (no check)
 * @param cls closure for the callbacks
 * @param rec receive function to call
 * @param nc function to call on connect events
 * @param nd function to call on disconnect events
 */
struct GNUNET_TRANSPORT_Handle *
GNUNET_TRANSPORT_connect (const struct GNUNET_CONFIGURATION_Handle *cfg,
                          const struct GNUNET_PeerIdentity *self, void *cls,
                          GNUNET_TRANSPORT_ReceiveCallback rec,
                          GNUNET_TRANSPORT_NotifyConnect nc,
                          GNUNET_TRANSPORT_NotifyDisconnect nd)
{
  struct GNUNET_TRANSPORT_Handle *ret;

  ret = GNUNET_malloc (sizeof (struct GNUNET_TRANSPORT_Handle));
  if (self != NULL)
  {
    ret->self = *self;
    ret->check_self = GNUNET_YES;
  }
  ret->cfg = cfg;
  ret->cls = cls;
  ret->rec = rec;
  ret->nc_cb = nc;
  ret->nd_cb = nd;
  ret->reconnect_delay = GNUNET_TIME_UNIT_ZERO;
  ret->neighbours =
    GNUNET_CONTAINER_multihashmap_create (STARTING_NEIGHBOURS_SIZE, GNUNET_YES);
  ret->ready_heap =
      GNUNET_CONTAINER_heap_create (GNUNET_CONTAINER_HEAP_ORDER_MIN);
  LOG (GNUNET_ERROR_TYPE_DEBUG, "Connecting to transport service.\n");
  ret->client = GNUNET_CLIENT_connect ("transport", cfg);
  if (ret->client == NULL)
  {
    GNUNET_free (ret);
    return NULL;
  }
  schedule_control_transmit (ret, sizeof (struct StartMessage), &send_start, ret);
  return ret;
}


/**
 * Disconnect from the transport service.
 *
 * @param handle handle to the service as returned from GNUNET_TRANSPORT_connect
 */
void
GNUNET_TRANSPORT_disconnect (struct GNUNET_TRANSPORT_Handle *handle)
{
  LOG (GNUNET_ERROR_TYPE_DEBUG, "Transport disconnect called!\n");
  /* this disconnects all neighbours... */
  if (handle->reconnect_task == GNUNET_SCHEDULER_NO_TASK)
    disconnect_and_schedule_reconnect (handle);
  /* and now we stop trying to connect again... */
  if (handle->reconnect_task != GNUNET_SCHEDULER_NO_TASK)
  {
    GNUNET_SCHEDULER_cancel (handle->reconnect_task);
    handle->reconnect_task = GNUNET_SCHEDULER_NO_TASK;
  }
  GNUNET_CONTAINER_multihashmap_destroy (handle->neighbours);
  handle->neighbours = NULL;
  if (handle->quota_task != GNUNET_SCHEDULER_NO_TASK)
  {
    GNUNET_SCHEDULER_cancel (handle->quota_task);
    handle->quota_task = GNUNET_SCHEDULER_NO_TASK;
  }
  GNUNET_free_non_null (handle->my_hello);
  handle->my_hello = NULL;
  GNUNET_assert (handle->tc_head == NULL);
  GNUNET_assert (handle->tc_tail == NULL);
  GNUNET_assert (handle->hwl_head == NULL);
  GNUNET_assert (handle->hwl_tail == NULL);
  GNUNET_CONTAINER_heap_destroy (handle->ready_heap);
  handle->ready_heap = NULL;
  GNUNET_free (handle);
}


/**
 * Check if we could queue a message of the given size for
 * transmission.  The transport service will take both its
 * internal buffers and bandwidth limits imposed by the
 * other peer into consideration when answering this query.
 *
 * @param handle connection to transport service
 * @param target who should receive the message
 * @param size how big is the message we want to transmit?
 * @param priority how important is the message?
 * @param timeout after how long should we give up (and call
 *        notify with buf NULL and size 0)?
 * @param notify function to call when we are ready to
 *        send such a message
 * @param notify_cls closure for notify
 * @return NULL if someone else is already waiting to be notified
 *         non-NULL if the notify callback was queued (can be used to cancel
 *         using GNUNET_TRANSPORT_notify_transmit_ready_cancel)
 */
struct GNUNET_TRANSPORT_TransmitHandle *
GNUNET_TRANSPORT_notify_transmit_ready (struct GNUNET_TRANSPORT_Handle *handle,
                                        const struct GNUNET_PeerIdentity
                                        *target, size_t size, uint32_t priority,
                                        struct GNUNET_TIME_Relative timeout,
                                        GNUNET_CONNECTION_TransmitReadyNotify
                                        notify, void *notify_cls)
{
  struct Neighbour *n;
  struct GNUNET_TRANSPORT_TransmitHandle *th;
  struct GNUNET_TIME_Relative delay;

  n = neighbour_find (handle, target);
  if (NULL == n)
  {
    /* use GNUNET_TRANSPORT_try_connect first, only use this function
     * once a connection has been established */
    GNUNET_assert (0);
    return NULL;
  }
  if (NULL != n->th)
  {
    /* attempt to send two messages at the same time to the same peer */
    GNUNET_assert (0);
    return NULL;
  }
  GNUNET_assert (NULL == n->hn);
  th = GNUNET_malloc (sizeof (struct GNUNET_TRANSPORT_TransmitHandle));
  th->neighbour = n;
  th->notify = notify;
  th->notify_cls = notify_cls;
  th->timeout = GNUNET_TIME_relative_to_absolute (timeout);
  th->notify_size = size;
  th->priority = priority;
  n->th = th;
  /* calculate when our transmission should be ready */
  delay = GNUNET_BANDWIDTH_tracker_get_delay (&n->out_tracker, size + n->traffic_overhead);
  n->traffic_overhead = 0;
  if (delay.rel_value > timeout.rel_value)
    delay.rel_value = 0;        /* notify immediately (with failure) */
  LOG (GNUNET_ERROR_TYPE_DEBUG,
       "Bandwidth tracker allows next transmission to peer %s in %llu ms\n",
       GNUNET_i2s (target), (unsigned long long) delay.rel_value);
  n->hn = GNUNET_CONTAINER_heap_insert (handle->ready_heap, n, delay.rel_value);
  schedule_transmission (handle);
  return th;
}


/**
 * Cancel the specified transmission-ready notification.
 *
 * @param th handle returned from GNUNET_TRANSPORT_notify_transmit_ready
 */
void
GNUNET_TRANSPORT_notify_transmit_ready_cancel (struct
                                               GNUNET_TRANSPORT_TransmitHandle
                                               *th)
{
  struct Neighbour *n;

  GNUNET_assert (NULL == th->next);
  GNUNET_assert (NULL == th->prev);
  n = th->neighbour;
  GNUNET_assert (th == n->th);
  n->th = NULL;
  if (n->hn != NULL)
  {
    GNUNET_CONTAINER_heap_remove_node (n->hn);
    n->hn = NULL;
  }
  else
  {
    GNUNET_assert (GNUNET_SCHEDULER_NO_TASK != th->timeout_task);
    GNUNET_SCHEDULER_cancel (th->timeout_task);
    th->timeout_task = GNUNET_SCHEDULER_NO_TASK;
  }
  GNUNET_free (th);
}


/* end of transport_api.c */
