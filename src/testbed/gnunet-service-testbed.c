/*
  This file is part of GNUnet.
  (C) 2012 Christian Grothoff (and other contributing authors)

  GNUnet is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 2, or (at your
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
 * @file testbed/gnunet-service-testbed.c
 * @brief implementation of the TESTBED service
 * @author Sree Harsha Totakura
 */

#include "platform.h"
#include "gnunet_service_lib.h"
#include "gnunet_server_lib.h"
#include "gnunet_transport_service.h"
#include "gnunet_core_service.h"
#include "gnunet_hello_lib.h"
#include <zlib.h>

#include "gnunet_testbed_service.h"
#include "testbed.h"
#include "testbed_api.h"
#include "testbed_api_hosts.h"
#include "gnunet_testing_lib-new.h"

/**
 * Generic logging
 */
#define LOG(kind,...)                           \
  GNUNET_log (kind, __VA_ARGS__)

/**
 * Debug logging
 */
#define LOG_DEBUG(...)                          \
  LOG (GNUNET_ERROR_TYPE_DEBUG, __VA_ARGS__)

/**
 * By how much should the arrays lists grow
 */
#define LIST_GROW_STEP 10

/**
 * Default timeout for operations which may take some time
 */
#define TIMEOUT GNUNET_TIME_relative_multiply(GNUNET_TIME_UNIT_SECONDS, 30)

/**
 * The main context information associated with the client which started us
 */
struct Context
{
  /**
   * The client handle associated with this context
   */
  struct GNUNET_SERVER_Client *client;

  /**
   * The network address of the master controller
   */
  char *master_ip;

  /**
   * The TESTING system handle for starting peers locally
   */
  struct GNUNET_TESTING_System *system;
  
  /**
   * Our host id according to this context
   */
  uint32_t host_id;
};


/**
 * The message queue for sending messages to clients
 */
struct MessageQueue
{
  /**
   * The message to be sent
   */
  struct GNUNET_MessageHeader *msg;

  /**
   * The client to send the message to
   */
  struct GNUNET_SERVER_Client *client;

  /**
   * next pointer for DLL
   */
  struct MessageQueue *next;

  /**
   * prev pointer for DLL
   */
  struct MessageQueue *prev;
};


/**
 * The structure for identifying a shared service
 */
struct SharedService
{
  /**
   * The name of the shared service
   */
  char *name;

  /**
   * Number of shared peers per instance of the shared service
   */
  uint32_t num_shared;

  /**
   * Number of peers currently sharing the service
   */
  uint32_t num_sharing;
};


/**
 * A routing entry
 */
struct Route
{
  /**
   * destination host
   */
  uint32_t dest;

  /**
   * The destination host is reachable thru
   */
  uint32_t thru;
};


/**
 * Context information used while linking controllers
 */
struct LinkControllersContext;


/**
 * A DLL of host registrations to be made
 */
struct HostRegistration
{
  /**
   * next registration in the DLL
   */
  struct HostRegistration *next;

  /**
   * previous registration in the DLL
   */
  struct HostRegistration *prev;

  /**
   * The callback to call after this registration's status is available
   */
  GNUNET_TESTBED_HostRegistrationCompletion cb;

  /**
   * The closure for the above callback
   */
  void *cb_cls;

  /**
   * The host that has to be registered
   */
  struct GNUNET_TESTBED_Host *host;
};


/**
 * This context information will be created for each host that is registered at
 * slave controllers during overlay connects.
 */
struct RegisteredHostContext
{
  /**
   * The host which is being registered
   */
  struct GNUNET_TESTBED_Host *reg_host;

  /**
   * The host of the controller which has to connect to the above rhost
   */
  struct GNUNET_TESTBED_Host *host;

  /**
   * The gateway to which this operation is forwarded to
   */
  struct Slave *gateway;

  /**
   * The gateway through which peer2's controller can be reached
   */
  struct Slave *gateway2;

  /**
   * Handle for sub-operations
   */
  struct GNUNET_TESTBED_Operation *sub_op;

  /**
   * The client which initiated the link controller operation
   */
  struct GNUNET_SERVER_Client *client;

  /**
   * Head of the ForwardedOverlayConnectContext DLL
   */
  struct ForwardedOverlayConnectContext *focc_dll_head;

  /**
   * Tail of the ForwardedOverlayConnectContext DLL
   */
  struct ForwardedOverlayConnectContext *focc_dll_tail;
  
  /**
   * Enumeration of states for this context
   */
  enum RHCState {

    /**
     * The initial state
     */
    RHC_INIT = 0,

    /**
     * State where we attempt to get peer2's controller configuration
     */
    RHC_GET_CFG,

    /**
     * State where we attempt to link the controller of peer 1 to the controller
     * of peer2
     */
    RHC_LINK,

    /**
     * State where we attempt to do the overlay connection again
     */
    RHC_OL_CONNECT
    
  } state;

};


/**
 * Structure representing a connected(directly-linked) controller
 */
struct Slave
{
  /**
   * The controller process handle if we had started the controller
   */
  struct GNUNET_TESTBED_ControllerProc *controller_proc;

  /**
   * The controller handle
   */
  struct GNUNET_TESTBED_Controller *controller;

  /**
   * The configuration of the slave. Cannot be NULL
   */
  struct GNUNET_CONFIGURATION_Handle *cfg;

  /**
   * handle to lcc which is associated with this slave startup. Should be set to
   * NULL when the slave has successfully started up
   */
  struct LinkControllersContext *lcc;

  /**
   * Head of the host registration DLL
   */
  struct HostRegistration *hr_dll_head;

  /**
   * Tail of the host registration DLL
   */
  struct HostRegistration *hr_dll_tail;

  /**
   * The current host registration handle
   */
  struct GNUNET_TESTBED_HostRegistrationHandle *rhandle;

  /**
   * Hashmap to hold Registered host contexts
   */
  struct GNUNET_CONTAINER_MultiHashMap *reghost_map;

  /**
   * The id of the host this controller is running on
   */
  uint32_t host_id;

};


/**
 * States of LCFContext
 */
enum LCFContextState
{
  /**
   * The Context has been initialized; Nothing has been done on it
   */
  INIT,

  /**
   * Delegated host has been registered at the forwarding controller
   */
  DELEGATED_HOST_REGISTERED,
  
  /**
   * The slave host has been registred at the forwarding controller
   */
  SLAVE_HOST_REGISTERED,
  
  /**
   * The context has been finished (may have error)
   */
  FINISHED
};


/**
 * Link controllers request forwarding context
 */
struct LCFContext
{
  /**
   * The gateway which will pass the link message to delegated host
   */
  struct Slave *gateway;

  /**
   * The controller link message that has to be forwarded to
   */
  struct GNUNET_TESTBED_ControllerLinkMessage *msg;

  /**
   * The client which has asked to perform this operation
   */
  struct GNUNET_SERVER_Client *client;

  /**
   * Handle for operations which are forwarded while linking controllers
   */
  struct ForwardedOperationContext *fopc;

  /**
   * The id of the operation which created this context
   */
  uint64_t operation_id;

  /**
   * The state of this context
   */
  enum LCFContextState state;

  /**
   * The delegated host
   */
  uint32_t delegated_host_id;

  /**
   * The slave host
   */
  uint32_t slave_host_id;

};


/**
 * Structure of a queue entry in LCFContext request queue
 */
struct LCFContextQueue
{
  /**
   * The LCFContext
   */
  struct LCFContext *lcf;

  /**
   * Head prt for DLL
   */
  struct LCFContextQueue *next;

  /**
   * Tail ptr for DLL
   */
  struct LCFContextQueue *prev;
};


/**
 * A peer
 */
struct Peer
{
  
  union
  {
    struct
    {
      /**
       * The peer handle from testing API
       */
      struct GNUNET_TESTING_Peer *peer;

      /**
       * The modified (by GNUNET_TESTING_peer_configure) configuration this
       * peer is configured with
       */
      struct GNUNET_CONFIGURATION_Handle *cfg;
      
      /**
       * Is the peer running
       */
      int is_running;

    } local;

    struct
    {
      /**
       * The slave this peer is started through
       */
      struct Slave *slave;

      /**
       * The id of the remote host this peer is running on
       */
      uint32_t remote_host_id;

    } remote;

  } details;

  /**
   * Is this peer locally created?
   */
  int is_remote;

  /**
   * Our local reference id for this peer
   */
  uint32_t id;

  /**
   * References to peers are using in forwarded overlay contexts and remote
   * overlay connect contexts. A peer can only be destroyed after all such
   * contexts are destroyed. For this, we maintain a reference counter. When we
   * use a peer in any such context, we increment this counter. We decrement it
   * when we are destroying these contexts
   */
  uint32_t reference_cnt;

  /**
   * While destroying a peer, due to the fact that there could be references to
   * this peer, we delay the peer destroy to a further time. We do this by using
   * this flag to destroy the peer while destroying a context in which this peer
   * has been used. When the flag is set to 1 and reference_cnt = 0 we destroy
   * the peer
   */
  uint32_t destroy_flag;

};


/**
 * Context information for transport try connect
 */
struct TryConnectContext
{
  /**
   * The identity of the peer to which the transport has to attempt a connection
   */
  struct GNUNET_PeerIdentity *pid;

  /**
   * The transport handle
   */
  struct GNUNET_TRANSPORT_Handle *th;

  /**
   * the try connect handle
   */
  struct GNUNET_TRANSPORT_TryConnectHandle *tch;

  /**
   * The task handle
   */
  GNUNET_SCHEDULER_TaskIdentifier task;

  /**
   * The number of times we attempted to connect
   */
  unsigned int retries;

};


/**
 * Context information for connecting 2 peers in overlay
 */
struct OverlayConnectContext
{
  /**
   * The next pointer for maintaining a DLL
   */
  struct OverlayConnectContext *next;

  /**
   * The prev pointer for maintaining a DLL
   */
  struct OverlayConnectContext *prev;
  
  /**
   * The client which has requested for overlay connection
   */
  struct GNUNET_SERVER_Client *client;

  /**
   * the peer which has to connect to the other peer
   */
  struct Peer *peer;

  /**
   * Transport handle of the first peer to get its HELLO
   */
  struct GNUNET_TRANSPORT_Handle *p1th;

  /**
   * Core handles of the first peer; used to notify when second peer connects to it
   */
  struct GNUNET_CORE_Handle *ch;

  /**
   * HELLO of the other peer
   */
  struct GNUNET_MessageHeader *hello;

  /**
   * Get hello handle to acquire HELLO of first peer
   */
  struct GNUNET_TRANSPORT_GetHelloHandle *ghh;

  /**
   * The handle for offering HELLO
   */
  struct GNUNET_TRANSPORT_OfferHelloHandle *ohh;

  /**
   * The error message we send if this overlay connect operation has timed out
   */
  char *emsg;

  /**
   * Operation context for suboperations
   */
  struct OperationContext *opc;

  /**
   * Controller of peer 2; NULL if the peer is local
   */
  struct GNUNET_TESTBED_Controller *peer2_controller;

  /**
   * The transport try connect context
   */
  struct TryConnectContext tcc;

  /**
   * The peer identity of the first peer
   */
  struct GNUNET_PeerIdentity peer_identity;

  /**
   * The peer identity of the other peer
   */
  struct GNUNET_PeerIdentity other_peer_identity;

  /**
   * The id of the operation responsible for creating this context
   */
  uint64_t op_id;

  /**
   * The id of the task for sending HELLO of peer 2 to peer 1 and ask peer 1 to
   * connect to peer 2
   */
  GNUNET_SCHEDULER_TaskIdentifier send_hello_task;

  /**
   * The id of the overlay connect timeout task
   */
  GNUNET_SCHEDULER_TaskIdentifier timeout_task;

  /**
   * The id of the cleanup task
   */
  GNUNET_SCHEDULER_TaskIdentifier cleanup_task;

  /**
   * The id of peer A
   */
  uint32_t peer_id;

  /**
   * The id of peer B
   */
  uint32_t other_peer_id;

};


/**
 * Context information for RequestOverlayConnect
 * operations. RequestOverlayConnect is used when peers A, B reside on different
 * hosts and the host controller for peer B is asked by the host controller of
 * peer A to make peer B connect to peer A
 */
struct RequestOverlayConnectContext
{
  /**
   * the next pointer for DLL
   */
  struct RequestOverlayConnectContext *next;

  /**
   * the prev pointer for DLL
   */
  struct RequestOverlayConnectContext *prev;

  /**
   * The peer handle of peer B
   */
  struct Peer *peer;
  
  /**
   * Peer A's HELLO
   */
  struct GNUNET_MessageHeader *hello;

  /**
   * The handle for offering HELLO
   */
  struct GNUNET_TRANSPORT_OfferHelloHandle *ohh;

  /**
   * The transport try connect context
   */
  struct TryConnectContext tcc;

  /**
   * The peer identity of peer A
   */
  struct GNUNET_PeerIdentity a_id;

  /**
   * Task for offering HELLO of A to B and doing try_connect
   */
  GNUNET_SCHEDULER_TaskIdentifier attempt_connect_task_id;
  
  /**
   * Task to timeout RequestOverlayConnect
   */
  GNUNET_SCHEDULER_TaskIdentifier timeout_rocc_task_id;
  
};


/**
 * Context information for operations forwarded to subcontrollers
 */
struct ForwardedOperationContext
{
  /**
   * The next pointer for DLL
   */
  struct ForwardedOperationContext *next;

  /**
   * The prev pointer for DLL
   */
  struct ForwardedOperationContext *prev;
  
  /**
   * The generated operation context
   */
  struct OperationContext *opc;

  /**
   * The client to which we have to reply
   */
  struct GNUNET_SERVER_Client *client;

  /**
   * Closure pointer
   */
  void *cls;

  /**
   * Task ID for the timeout task
   */
  GNUNET_SCHEDULER_TaskIdentifier timeout_task;

  /**
   * The id of the operation that has been forwarded
   */
  uint64_t operation_id;

  /**
   * The type of the operation which is forwarded
   */
  enum OperationType type;

};


/**
 * Context information used while linking controllers
 */
struct LinkControllersContext
{
  /**
   * The client which initiated the link controller operation
   */
  struct GNUNET_SERVER_Client *client;

  /**
   * The ID of the operation
   */
  uint64_t operation_id;

};


/**
 * Context information to used during operations which forward the overlay
 * connect message
 */
struct ForwardedOverlayConnectContext
{
  /**
   * next ForwardedOverlayConnectContext in the DLL
   */
  struct ForwardedOverlayConnectContext *next;

  /**
   * previous ForwardedOverlayConnectContext in the DLL
   */
  struct ForwardedOverlayConnectContext *prev;

  /**
   * A copy of the original overlay connect message
   */
  struct GNUNET_MessageHeader *orig_msg;

  /**
   * The id of the operation which created this context information
   */
  uint64_t operation_id;

  /**
   * the id of peer 1
   */
  uint32_t peer1;
  
  /**
   * The id of peer 2
   */
  uint32_t peer2;
  
  /**
   * Id of the host where peer2 is running
   */
  uint32_t peer2_host_id;
};



/**
 * The master context; generated with the first INIT message
 */
static struct Context *master_context;

/**
 * Our hostname; we give this to all the peers we start
 */
static char *hostname;


/***********/
/* Handles */
/***********/

/**
 * Our configuration
 */
static struct GNUNET_CONFIGURATION_Handle *our_config;

/**
 * Current Transmit Handle; NULL if no notify transmit exists currently
 */
static struct GNUNET_SERVER_TransmitHandle *transmit_handle;

/****************/
/* Lists & Maps */
/****************/

/**
 * The head for the LCF queue
 */
static struct LCFContextQueue *lcfq_head;

/**
 * The tail for the LCF queue
 */
static struct LCFContextQueue *lcfq_tail;

/**
 * The message queue head
 */
static struct MessageQueue *mq_head;

/**
 * The message queue tail
 */
static struct MessageQueue *mq_tail;

/**
 * DLL head for OverlayConnectContext DLL - to be used to clean up during shutdown
 */
static struct OverlayConnectContext *occq_head;

/**
 * DLL tail for OverlayConnectContext DLL
 */
static struct OverlayConnectContext *occq_tail;

/**
 * DLL head for RequectOverlayConnectContext DLL - to be used to clean up during
 * shutdown
 */
static struct RequestOverlayConnectContext *roccq_head;

/**
 * DLL tail for RequectOverlayConnectContext DLL
 */
static struct RequestOverlayConnectContext *roccq_tail;

/**
 * DLL head for forwarded operation contexts
 */
static struct ForwardedOperationContext *fopcq_head;

/**
 * DLL tail for forwarded operation contexts
 */
static struct ForwardedOperationContext *fopcq_tail;

/**
 * Array of hosts
 */
static struct GNUNET_TESTBED_Host **host_list;

/**
 * A list of routes
 */
static struct Route **route_list;

/**
 * A list of directly linked neighbours
 */
static struct Slave **slave_list;

/**
 * A list of peers we know about
 */
static struct Peer **peer_list;

/**
 * The hashmap of shared services
 */
static struct GNUNET_CONTAINER_MultiHashMap *ss_map;

/**
 * The event mask for the events we listen from sub-controllers
 */
static uint64_t event_mask;

/**
 * The size of the host list
 */
static uint32_t host_list_size;

/**
 * The size of the route list
 */
static uint32_t route_list_size;

/**
 * The size of directly linked neighbours list
 */
static uint32_t slave_list_size;

/**
 * The size of the peer list
 */
static uint32_t peer_list_size;

/*********/
/* Tasks */
/*********/

/**
 * The lcf_task handle
 */
static GNUNET_SCHEDULER_TaskIdentifier lcf_proc_task_id;

/**
 * The shutdown task handle
 */
static GNUNET_SCHEDULER_TaskIdentifier shutdown_task_id;


/**
 * Function called to notify a client about the connection begin ready to queue
 * more data.  "buf" will be NULL and "size" zero if the connection was closed
 * for writing in the meantime.
 *
 * @param cls NULL
 * @param size number of bytes available in buf
 * @param buf where the callee should write the message
 * @return number of bytes written to buf
 */
static size_t
transmit_ready_notify (void *cls, size_t size, void *buf)
{
  struct MessageQueue *mq_entry;

  transmit_handle = NULL;
  mq_entry = mq_head;
  GNUNET_assert (NULL != mq_entry);
  if (0 == size)
    return 0;
  GNUNET_assert (ntohs (mq_entry->msg->size) <= size);
  size = ntohs (mq_entry->msg->size);
  memcpy (buf, mq_entry->msg, size);
  GNUNET_free (mq_entry->msg);
  GNUNET_SERVER_client_drop (mq_entry->client);
  GNUNET_CONTAINER_DLL_remove (mq_head, mq_tail, mq_entry);
  GNUNET_free (mq_entry);
  mq_entry = mq_head;
  if (NULL != mq_entry)
    transmit_handle =
        GNUNET_SERVER_notify_transmit_ready (mq_entry->client,
                                             ntohs (mq_entry->msg->size),
                                             GNUNET_TIME_UNIT_FOREVER_REL,
                                             &transmit_ready_notify, NULL);
  return size;
}


/**
 * Queues a message in send queue for sending to the service
 *
 * @param client the client to whom the queued message has to be sent
 * @param msg the message to queue
 */
static void
queue_message (struct GNUNET_SERVER_Client *client,
               struct GNUNET_MessageHeader *msg)
{
  struct MessageQueue *mq_entry;
  uint16_t type;
  uint16_t size;

  type = ntohs (msg->type);
  size = ntohs (msg->size);
  GNUNET_assert ((GNUNET_MESSAGE_TYPE_TESTBED_INIT <= type) &&
                 (GNUNET_MESSAGE_TYPE_TESTBED_MAX > type));
  mq_entry = GNUNET_malloc (sizeof (struct MessageQueue));
  mq_entry->msg = msg;
  mq_entry->client = client;
  GNUNET_SERVER_client_keep (client);
  LOG_DEBUG ("Queueing message of type %u, size %u for sending\n", type,
             ntohs (msg->size));
  GNUNET_CONTAINER_DLL_insert_tail (mq_head, mq_tail, mq_entry);
  if (NULL == transmit_handle)
    transmit_handle =
        GNUNET_SERVER_notify_transmit_ready (client, size,
                                             GNUNET_TIME_UNIT_FOREVER_REL,
                                             &transmit_ready_notify, NULL);
}


/**
 * Similar to GNUNET_realloc; however clears tail part of newly allocated memory
 *
 * @param ptr the memory block to realloc
 * @param size the size of ptr
 * @param new_size the size to which ptr has to be realloc'ed
 * @return the newly reallocated memory block
 */
static void *
TESTBED_realloc (void *ptr, size_t size, size_t new_size)
{
  ptr = GNUNET_realloc (ptr, new_size);
  if (new_size > size)
    (void) memset (ptr + size, 0, new_size - size);
  return ptr;
}


/**
 * Function to add a host to the current list of known hosts
 *
 * @param host the host to add
 * @return GNUNET_OK on success; GNUNET_SYSERR on failure due to host-id
 *           already in use
 */
static int
host_list_add (struct GNUNET_TESTBED_Host *host)
{
  uint32_t host_id;
  uint32_t orig_size;

  host_id = GNUNET_TESTBED_host_get_id_ (host);
  orig_size = host_list_size;  
  if (host_list_size <= host_id)
  {
    while (host_list_size <= host_id)
      host_list_size += LIST_GROW_STEP;
    host_list =
        TESTBED_realloc (host_list,
                         sizeof (struct GNUNET_TESTBED_Host *) * orig_size,
                         sizeof (struct GNUNET_TESTBED_Host *)
			 * host_list_size);
  }
  if (NULL != host_list[host_id])
  {
    LOG_DEBUG ("A host with id: %u already exists\n", host_id);
    return GNUNET_SYSERR;
  }
  host_list[host_id] = host;
  return GNUNET_OK;
}


/**
 * Adds a route to the route list
 *
 * @param route the route to add
 */
static void
route_list_add (struct Route *route)
{
  uint32_t orig_size;

  orig_size = route_list_size;  
  if (route->dest >= route_list_size)
  {
    while (route->dest >= route_list_size)
      route_list_size += LIST_GROW_STEP;
    route_list =
        TESTBED_realloc (route_list,
			 sizeof (struct Route *) * orig_size,
                         sizeof (struct Route *) * route_list_size);
  }
  GNUNET_assert (NULL == route_list[route->dest]);
  route_list[route->dest] = route;
}


/**
 * Adds a slave to the slave array
 *
 * @param slave the slave controller to add
 */
static void
slave_list_add (struct Slave *slave)
{
  uint32_t orig_size;

  orig_size = slave_list_size;  
  if (slave->host_id >= slave_list_size)
  {
    while (slave->host_id >= slave_list_size)
      slave_list_size += LIST_GROW_STEP;
    slave_list =
        TESTBED_realloc (slave_list, sizeof (struct Slave *) * orig_size,
                         sizeof (struct Slave *) * slave_list_size);
  }
  GNUNET_assert (NULL == slave_list[slave->host_id]);
  slave_list[slave->host_id] = slave;
}


/**
 * Adds a peer to the peer array
 *
 * @param peer the peer to add
 */
static void
peer_list_add (struct Peer *peer)
{
  uint32_t orig_size;

  orig_size = peer_list_size;
  if (peer->id >= peer_list_size)
  {
    while (peer->id >= peer_list_size)
      peer_list_size += LIST_GROW_STEP;
    peer_list =
        TESTBED_realloc (peer_list, sizeof (struct Peer *) * orig_size,
                         sizeof (struct Peer *) * peer_list_size);
  }  
  GNUNET_assert (NULL == peer_list[peer->id]);
  peer_list[peer->id] = peer;
}


/**
 * Removes a the give peer from the peer array
 *
 * @param peer the peer to be removed
 */
static void
peer_list_remove (struct Peer *peer)
{
  uint32_t id;
  uint32_t orig_size;

  peer_list[peer->id] = NULL;
  orig_size = peer_list_size;
  while (peer_list_size >= LIST_GROW_STEP)
  {
    for (id = peer_list_size - 1;
         (id >= peer_list_size - LIST_GROW_STEP) && (id != UINT32_MAX); id--)
      if (NULL != peer_list[id])
        break;
    if (id != ((peer_list_size - LIST_GROW_STEP) - 1))
      break;
    peer_list_size -= LIST_GROW_STEP;
  }
  if (orig_size == peer_list_size)
    return;
  peer_list =
      GNUNET_realloc (peer_list, sizeof (struct Peer *) * peer_list_size);
}


/**
 * Finds the route with directly connected host as destination through which
 * the destination host can be reached
 *
 * @param host_id the id of the destination host
 * @return the route with directly connected destination host; NULL if no route
 *           is found
 */
static struct Route *
find_dest_route (uint32_t host_id)
{
  struct Route *route;

  if (route_list_size <= host_id)
    return NULL;
  while (NULL != (route = route_list[host_id]))
  {
    if (route->thru == master_context->host_id)
      break;
    host_id = route->thru;
  }
  return route;
}


/**
 * Routes message to a host given its host_id
 *
 * @param host_id the id of the destination host
 * @param msg the message to be routed
 */
static void
route_message (uint32_t host_id, const struct GNUNET_MessageHeader *msg)
{
  GNUNET_break (0);
}


/**
 * Send operation failure message to client
 *
 * @param client the client to which the failure message has to be sent to
 * @param operation_id the id of the failed operation
 * @param emsg the error message; can be NULL
 */
static void
send_operation_fail_msg (struct GNUNET_SERVER_Client *client,
                         uint64_t operation_id, const char *emsg)
{
  struct GNUNET_TESTBED_OperationFailureEventMessage *msg;
  uint16_t msize;
  uint16_t emsg_len;

  msize = sizeof (struct GNUNET_TESTBED_OperationFailureEventMessage);
  emsg_len = (NULL == emsg) ? 0 : strlen (emsg) + 1;
  msize += emsg_len;
  msg = GNUNET_malloc (msize);
  msg->header.size = htons (msize);
  msg->header.type = htons (GNUNET_MESSAGE_TYPE_TESTBED_OPERATIONFAILEVENT);
  msg->event_type = htonl (GNUNET_TESTBED_ET_OPERATION_FINISHED);
  msg->operation_id = GNUNET_htonll (operation_id);
  if (0 != emsg_len)
    memcpy (&msg[1], emsg, emsg_len);
  queue_message (client, &msg->header);
}


/**
 * Function to send generic operation success message to given client
 *
 * @param client the client to send the message to
 * @param operation_id the id of the operation which was successful
 */
static void
send_operation_success_msg (struct GNUNET_SERVER_Client *client,
                            uint64_t operation_id)
{
  struct GNUNET_TESTBED_GenericOperationSuccessEventMessage *msg;
  uint16_t msize;

  msize = sizeof (struct GNUNET_TESTBED_GenericOperationSuccessEventMessage);
  msg = GNUNET_malloc (msize);
  msg->header.size = htons (msize);
  msg->header.type = htons (GNUNET_MESSAGE_TYPE_TESTBED_GENERICOPSUCCESS);
  msg->operation_id = GNUNET_htonll (operation_id);
  msg->event_type = htonl (GNUNET_TESTBED_ET_OPERATION_FINISHED);
  queue_message (client, &msg->header);
}


/**
 * Callback which will be called to after a host registration succeeded or failed
 *
 * @param cls the handle to the slave at which the registration is completed
 * @param emsg the error message; NULL if host registration is successful
 */
static void 
hr_completion (void *cls, const char *emsg);


/**
 * Attempts to register the next host in the host registration queue
 *
 * @param slave the slave controller whose host registration queue is checked
 *          for host registrations
 */
static void
register_next_host (struct Slave *slave)
{
  struct HostRegistration *hr;
  
  hr = slave->hr_dll_head;
  GNUNET_assert (NULL != hr);
  GNUNET_assert (NULL == slave->rhandle);
  LOG (GNUNET_ERROR_TYPE_DEBUG,
       "Registering host %u at %u\n",
       GNUNET_TESTBED_host_get_id_ (hr->host),
       GNUNET_TESTBED_host_get_id_ (host_list[slave->host_id]));
  slave->rhandle = GNUNET_TESTBED_register_host (slave->controller,
                                                 hr->host,
                                                 hr_completion,
                                                 slave);
}


/**
 * Callback which will be called to after a host registration succeeded or failed
 *
 * @param cls the handle to the slave at which the registration is completed
 * @param emsg the error message; NULL if host registration is successful
 */
static void 
hr_completion (void *cls, const char *emsg)
{
  struct Slave *slave = cls;
  struct HostRegistration *hr;

  slave->rhandle = NULL;
  hr = slave->hr_dll_head;
  GNUNET_assert (NULL != hr);
  LOG (GNUNET_ERROR_TYPE_DEBUG,
       "Registering host %u at %u successful\n",
       GNUNET_TESTBED_host_get_id_ (hr->host),
       GNUNET_TESTBED_host_get_id_ (host_list[slave->host_id]));
  GNUNET_CONTAINER_DLL_remove (slave->hr_dll_head,
                               slave->hr_dll_tail,
                               hr);
  if (NULL != hr->cb)
    hr->cb (hr->cb_cls, emsg);
  GNUNET_free (hr);
  if (NULL != slave->hr_dll_head)
    register_next_host (slave);
}


/**
 * Adds a host registration's request to a slave's registration queue
 *
 * @param slave the slave controller at which the given host has to be
 *          registered 
 * @param cb the host registration completion callback
 * @param cb_cls the closure for the host registration completion callback
 * @param host the host which has to be registered
 */
static void
queue_host_registration (struct Slave *slave,
                         GNUNET_TESTBED_HostRegistrationCompletion cb,
                         void *cb_cls,
                         struct GNUNET_TESTBED_Host *host)
{
  struct HostRegistration *hr;
  int call_register;

  LOG (GNUNET_ERROR_TYPE_DEBUG,
       "Queueing host registration for host %u at %u\n",
       GNUNET_TESTBED_host_get_id_ (host),
       GNUNET_TESTBED_host_get_id_ (host_list[slave->host_id]));
  hr = GNUNET_malloc (sizeof (struct HostRegistration));
  hr->cb = cb;
  hr->cb_cls = cb_cls;
  hr->host = host;
  call_register = (NULL == slave->hr_dll_head) ? GNUNET_YES : GNUNET_NO;
  GNUNET_CONTAINER_DLL_insert_tail (slave->hr_dll_head,
                                    slave->hr_dll_tail,
                                    hr);
  if (GNUNET_YES == call_register)
    register_next_host (slave);
}


/**
 * The  Link Controller forwarding task
 *
 * @param cls the LCFContext
 * @param tc the Task context from scheduler
 */
static void
lcf_proc_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc);


/**
 * Completion callback for host registrations while forwarding Link Controller messages
 *
 * @param cls the LCFContext
 * @param emsg the error message; NULL if host registration is successful
 */
static void
lcf_proc_cc (void *cls, const char *emsg)
{
  struct LCFContext *lcf = cls;

  GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == lcf_proc_task_id);
  switch (lcf->state)
  {
  case INIT:
    if (NULL != emsg)
      goto registration_error;
    lcf->state = DELEGATED_HOST_REGISTERED;
    lcf_proc_task_id = GNUNET_SCHEDULER_add_now (&lcf_proc_task, lcf);
    break;
  case DELEGATED_HOST_REGISTERED:
    if (NULL != emsg)
      goto registration_error;
    lcf->state = SLAVE_HOST_REGISTERED;
    lcf_proc_task_id = GNUNET_SCHEDULER_add_now (&lcf_proc_task, lcf);
    break;
  default:
    GNUNET_assert (0);          /* Shouldn't reach here */
  }
  return;

 registration_error:
  LOG (GNUNET_ERROR_TYPE_WARNING, "Host registration failed with message: %s\n",
       emsg);
  lcf->state = FINISHED;
  lcf_proc_task_id = GNUNET_SCHEDULER_add_now (&lcf_proc_task, lcf);
}


/**
 * Callback to relay the reply msg of a forwarded operation back to the client
 *
 * @param cls ForwardedOperationContext
 * @param msg the message to relay
 */
static void
forwarded_operation_reply_relay (void *cls,
                                 const struct GNUNET_MessageHeader *msg)
{
  struct ForwardedOperationContext *fopc = cls;
  struct GNUNET_MessageHeader *dup_msg;
  uint16_t msize;

  msize = ntohs (msg->size);
  LOG_DEBUG ("Relaying message with type: %u, size: %u\n", ntohs (msg->type),
             msize);
  dup_msg = GNUNET_copy_message (msg);
  queue_message (fopc->client, dup_msg);
  GNUNET_SERVER_client_drop (fopc->client);
  GNUNET_SCHEDULER_cancel (fopc->timeout_task);
  GNUNET_CONTAINER_DLL_remove (fopcq_head, fopcq_tail, fopc);
  GNUNET_free (fopc);
}


/**
 * Task to free resources when forwarded operation has been timedout
 *
 * @param cls the ForwardedOperationContext
 * @param tc the task context from scheduler
 */
static void
forwarded_operation_timeout (void *cls,
                             const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct ForwardedOperationContext *fopc = cls;

  GNUNET_TESTBED_forward_operation_msg_cancel_ (fopc->opc);
  LOG (GNUNET_ERROR_TYPE_WARNING, "A forwarded operation has timed out\n");
  send_operation_fail_msg (fopc->client, fopc->operation_id, "Timeout");
  GNUNET_SERVER_client_drop (fopc->client);
  GNUNET_CONTAINER_DLL_remove (fopcq_head, fopcq_tail, fopc);
  GNUNET_free (fopc);
}


/**
 * The  Link Controller forwarding task
 *
 * @param cls the LCFContext
 * @param tc the Task context from scheduler
 */
static void
lcf_proc_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc);


/**
 * Callback to be called when forwarded link controllers operation is
 * successfull. We have to relay the reply msg back to the client
 *
 * @param cls the LCFContext
 * @param msg the message to relay
 */
static void
lcf_forwarded_operation_reply_relay (void *cls,
                                     const struct GNUNET_MessageHeader *msg)
{
  struct LCFContext *lcf = cls;

  GNUNET_assert (NULL != lcf->fopc);
  forwarded_operation_reply_relay (lcf->fopc, msg);
  lcf->fopc = NULL;
  GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == lcf_proc_task_id);
  lcf_proc_task_id = GNUNET_SCHEDULER_add_now (&lcf_proc_task, lcf);
}


/**
 * Task to free resources when forwarded link controllers has been timedout
 *
 * @param cls the LCFContext
 * @param tc the task context from scheduler
 */
static void
lcf_forwarded_operation_timeout (void *cls,
                                 const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct LCFContext *lcf = cls;

  GNUNET_assert (NULL != lcf->fopc);
  lcf->fopc->timeout_task = GNUNET_SCHEDULER_NO_TASK;
  forwarded_operation_timeout (lcf->fopc, tc);
  lcf->fopc = NULL;
  GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == lcf_proc_task_id);
  lcf_proc_task_id = GNUNET_SCHEDULER_add_now (&lcf_proc_task, lcf);
}


/**
 * The  Link Controller forwarding task
 *
 * @param cls the LCFContext
 * @param tc the Task context from scheduler
 */
static void
lcf_proc_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct LCFContext *lcf = cls;
  struct LCFContextQueue *lcfq;

  lcf_proc_task_id = GNUNET_SCHEDULER_NO_TASK;
  switch (lcf->state)
  {
  case INIT:
    if (GNUNET_NO ==
        GNUNET_TESTBED_is_host_registered_ (host_list[lcf->delegated_host_id],
                                            lcf->gateway->controller))
    {
      queue_host_registration (lcf->gateway,
                               lcf_proc_cc, lcf,
                               host_list[lcf->delegated_host_id]);
    }
    else
    {
      lcf->state = DELEGATED_HOST_REGISTERED;
      lcf_proc_task_id = GNUNET_SCHEDULER_add_now (&lcf_proc_task, lcf);
    }
    break;
  case DELEGATED_HOST_REGISTERED:
    if (GNUNET_NO ==
        GNUNET_TESTBED_is_host_registered_ (host_list[lcf->slave_host_id],
                                            lcf->gateway->controller))
    {
      queue_host_registration (lcf->gateway,
                               lcf_proc_cc, lcf,
                               host_list[lcf->slave_host_id]);
    }
    else
    {
      lcf->state = SLAVE_HOST_REGISTERED;
      lcf_proc_task_id = GNUNET_SCHEDULER_add_now (&lcf_proc_task, lcf);
    }
    break;
  case SLAVE_HOST_REGISTERED:
    lcf->fopc = GNUNET_malloc (sizeof (struct ForwardedOperationContext));
    lcf->fopc->client = lcf->client;
    lcf->fopc->operation_id = lcf->operation_id;
    lcf->fopc->type = OP_LINK_CONTROLLERS;
    lcf->fopc->opc =
        GNUNET_TESTBED_forward_operation_msg_ (lcf->gateway->controller,
                                               lcf->operation_id,
                                               &lcf->msg->header,
                                               &lcf_forwarded_operation_reply_relay,
                                               lcf);
    lcf->fopc->timeout_task =
        GNUNET_SCHEDULER_add_delayed (TIMEOUT, &lcf_forwarded_operation_timeout,
                                      lcf);
    GNUNET_CONTAINER_DLL_insert_tail (fopcq_head, fopcq_tail, lcf->fopc);
    lcf->state = FINISHED;
    break;
  case FINISHED:
    lcfq = lcfq_head;
    GNUNET_assert (lcfq->lcf == lcf);
    GNUNET_free (lcf->msg);
    GNUNET_free (lcf);
    GNUNET_CONTAINER_DLL_remove (lcfq_head, lcfq_tail, lcfq);
    GNUNET_free (lcfq);
    if (NULL != lcfq_head)
      lcf_proc_task_id =
          GNUNET_SCHEDULER_add_now (&lcf_proc_task, lcfq_head->lcf);
  }
}


/**
 * Cleans up ForwardedOverlayConnectContext
 *
 * @param focc the ForwardedOverlayConnectContext to cleanup
 */
static void
cleanup_focc (struct ForwardedOverlayConnectContext *focc)
{
  GNUNET_free_non_null (focc->orig_msg);
  GNUNET_free (focc);
}


/**
 * Processes a forwarded overlay connect context in the queue of the given RegisteredHostContext
 *
 * @param rhc the RegisteredHostContext
 */
static void
process_next_focc (struct RegisteredHostContext *rhc);


/**
 * Timeout task for cancelling a forwarded overlay connect connect
 *
 * @param cls the ForwardedOverlayConnectContext
 * @param tc the task context from the scheduler
 */
static void
forwarded_overlay_connect_timeout (void *cls,
                                   const struct GNUNET_SCHEDULER_TaskContext
                                   *tc)
{
  struct ForwardedOperationContext *fopc = cls;
  struct RegisteredHostContext *rhc;
  struct ForwardedOverlayConnectContext *focc;
  
  rhc = fopc->cls;
  focc = rhc->focc_dll_head;
  GNUNET_CONTAINER_DLL_remove (rhc->focc_dll_head, rhc->focc_dll_tail, focc);
  cleanup_focc (focc);
  LOG_DEBUG ("Overlay linking between peers %u and %u failed\n",
	     focc->peer1, focc->peer2);
  forwarded_operation_timeout (cls, tc);
  if (NULL != rhc->focc_dll_head)
    process_next_focc (rhc);
}


/**
 * Callback to be called when forwarded overlay connection operation has a reply
 * from the sub-controller successfull. We have to relay the reply msg back to
 * the client
 *
 * @param cls ForwardedOperationContext
 * @param msg the peer create success message
 */
static void
forwarded_overlay_connect_listener (void *cls,
                                    const struct GNUNET_MessageHeader *msg)
{
  struct ForwardedOperationContext *fopc = cls;
  struct RegisteredHostContext *rhc;
  struct ForwardedOverlayConnectContext *focc;
  
  rhc = fopc->cls;
  forwarded_operation_reply_relay (cls, msg);
  focc = rhc->focc_dll_head;
  GNUNET_CONTAINER_DLL_remove (rhc->focc_dll_head, rhc->focc_dll_tail, focc);
  cleanup_focc (focc);
  if (NULL != rhc->focc_dll_head)
    process_next_focc (rhc);
}


/**
 * Processes a forwarded overlay connect context in the queue of the given RegisteredHostContext
 *
 * @param rhc the RegisteredHostContext
 */
static void
process_next_focc (struct RegisteredHostContext *rhc)
{
  struct ForwardedOperationContext *fopc;
  struct ForwardedOverlayConnectContext *focc;

  focc = rhc->focc_dll_head;
  GNUNET_assert (NULL != focc);
  GNUNET_assert (RHC_OL_CONNECT == rhc->state);
  fopc = GNUNET_malloc (sizeof (struct ForwardedOperationContext));
  GNUNET_SERVER_client_keep (rhc->client);
  fopc->client = rhc->client;  
  fopc->operation_id = focc->operation_id;
  fopc->cls = rhc;
  fopc->type = OP_OVERLAY_CONNECT;
  fopc->opc =
        GNUNET_TESTBED_forward_operation_msg_ (rhc->gateway->controller,
                                               focc->operation_id, focc->orig_msg,
                                               &forwarded_overlay_connect_listener,
                                               fopc);
  GNUNET_free (focc->orig_msg);
  focc->orig_msg = NULL;
  fopc->timeout_task =
      GNUNET_SCHEDULER_add_delayed (TIMEOUT, &forwarded_overlay_connect_timeout,
                                    fopc);
  GNUNET_CONTAINER_DLL_insert_tail (fopcq_head, fopcq_tail, fopc);
}


/**
 * Callback for event from slave controllers
 *
 * @param cls struct Slave *
 * @param event information about the event
 */
static void
slave_event_callback (void *cls,
                      const struct GNUNET_TESTBED_EventInformation *event)
{
  struct RegisteredHostContext *rhc;
  struct GNUNET_CONFIGURATION_Handle *cfg;
  struct GNUNET_TESTBED_Operation *old_op;
  
  /* We currently only get here when working on RegisteredHostContexts */
  GNUNET_assert (GNUNET_TESTBED_ET_OPERATION_FINISHED == event->type);
  rhc = event->details.operation_finished.op_cls;
  GNUNET_assert (rhc->sub_op == event->details.operation_finished.operation);
  switch (rhc->state)
  {
  case RHC_GET_CFG:
    cfg = event->details.operation_finished.generic;
    old_op = rhc->sub_op;
    rhc->state = RHC_LINK;
    rhc->sub_op =
        GNUNET_TESTBED_controller_link (rhc,
                                        rhc->gateway->controller,
                                        rhc->reg_host,
                                        rhc->host,
                                        cfg,
                                        GNUNET_NO);
    GNUNET_TESTBED_operation_done (old_op);
    break;
  case RHC_LINK:
    LOG_DEBUG ("OL: Linking controllers successfull\n");
    GNUNET_TESTBED_operation_done (rhc->sub_op);
    rhc->sub_op = NULL;
    rhc->state = RHC_OL_CONNECT;
    process_next_focc (rhc);
    break;
  default:
    GNUNET_assert (0);
  }
}


/**
 * Callback to signal successfull startup of the controller process
 *
 * @param cls the handle to the slave whose status is to be found here
 * @param cfg the configuration with which the controller has been started;
 *          NULL if status is not GNUNET_OK
 * @param status GNUNET_OK if the startup is successfull; GNUNET_SYSERR if not,
 *          GNUNET_TESTBED_controller_stop() shouldn't be called in this case
 */
static void
slave_status_callback (void *cls, const struct GNUNET_CONFIGURATION_Handle *cfg,
                       int status)
{
  struct Slave *slave = cls;
  struct LinkControllersContext *lcc;

  lcc = slave->lcc;
  if (GNUNET_SYSERR == status)
  {
    slave->controller_proc = NULL;
    slave_list[slave->host_id] = NULL;
    if (NULL != slave->cfg)
      GNUNET_CONFIGURATION_destroy (slave->cfg);
    GNUNET_free (slave);
    slave = NULL;
    LOG (GNUNET_ERROR_TYPE_WARNING, "Unexpected slave shutdown\n");
    GNUNET_SCHEDULER_shutdown ();       /* We too shutdown */
    goto clean_lcc;
  }
  slave->controller =
      GNUNET_TESTBED_controller_connect (cfg, host_list[slave->host_id],
                                         event_mask,
                                         &slave_event_callback, slave);
  if (NULL != slave->controller)
  {
    send_operation_success_msg (lcc->client, lcc->operation_id);
    slave->cfg = GNUNET_CONFIGURATION_dup (cfg);
  }
  else
  {
    send_operation_fail_msg (lcc->client, lcc->operation_id,
                             "Could not connect to delegated controller");
    GNUNET_TESTBED_controller_stop (slave->controller_proc);
    slave_list[slave->host_id] = NULL;
    GNUNET_free (slave);
    slave = NULL;
  }

 clean_lcc:
  if (NULL != lcc)
  {
    if (NULL != lcc->client)
    {
      GNUNET_SERVER_receive_done (lcc->client, GNUNET_OK);
      GNUNET_SERVER_client_drop (lcc->client);
      lcc->client = NULL;
    }
    GNUNET_free (lcc);
  }
  if (NULL != slave)
    slave->lcc = NULL;
}


/**
 * Message handler for GNUNET_MESSAGE_TYPE_TESTBED_INIT messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_init (void *cls, struct GNUNET_SERVER_Client *client,
             const struct GNUNET_MessageHeader *message)
{
  const struct GNUNET_TESTBED_InitMessage *msg;
  struct GNUNET_TESTBED_Host *host;
  const char *controller_hostname;
  uint16_t msize;

  if (NULL != master_context)
  {
    LOG_DEBUG ("We are being connected to laterally\n");
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  msg = (const struct GNUNET_TESTBED_InitMessage *) message;
  msize = ntohs (message->size);
  if (msize <= sizeof (struct GNUNET_TESTBED_InitMessage))
  {
    GNUNET_break (0);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  msize -= sizeof (struct GNUNET_TESTBED_InitMessage);
  controller_hostname = (const char *) &msg[1];
  if ('\0' != controller_hostname[msize - 1])
  {
    GNUNET_break (0);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  master_context = GNUNET_malloc (sizeof (struct Context));
  GNUNET_SERVER_client_keep (client);
  master_context->client = client;
  master_context->host_id = ntohl (msg->host_id);
  master_context->master_ip = GNUNET_strdup (controller_hostname);
  LOG_DEBUG ("Our IP: %s\n", master_context->master_ip);
  master_context->system =
      GNUNET_TESTING_system_create ("testbed", master_context->master_ip, hostname);
  host =
      GNUNET_TESTBED_host_create_with_id (master_context->host_id,
                                          master_context->master_ip,
                                          NULL,
                                          0);
  host_list_add (host);
  LOG_DEBUG ("Created master context with host ID: %u\n",
             master_context->host_id);
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * Message handler for GNUNET_MESSAGE_TYPE_TESTBED_ADDHOST messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_add_host (void *cls, struct GNUNET_SERVER_Client *client,
                 const struct GNUNET_MessageHeader *message)
{
  struct GNUNET_TESTBED_Host *host;
  const struct GNUNET_TESTBED_AddHostMessage *msg;
  struct GNUNET_TESTBED_HostConfirmedMessage *reply;
  char *username;
  char *hostname;
  char *emsg;
  uint32_t host_id;
  uint16_t username_length;
  uint16_t hostname_length;
  uint16_t reply_size;
  uint16_t msize;

  msg = (const struct GNUNET_TESTBED_AddHostMessage *) message;
  msize = ntohs (msg->header.size);
  username = (char *) &msg[1];
  username_length = ntohs (msg->user_name_length);
  if (0 != username_length)
    username_length++;
  /* msg must contain hostname */
  GNUNET_assert (msize > (sizeof (struct GNUNET_TESTBED_AddHostMessage) +
			  username_length + 1));
  if (0 != username_length)
    GNUNET_assert ('\0' == username[username_length - 1]);
  hostname = username + username_length;
  hostname_length =
      msize - (sizeof (struct GNUNET_TESTBED_AddHostMessage) + username_length);
  GNUNET_assert ('\0' == hostname[hostname_length - 1]);
  GNUNET_assert (strlen (hostname) == hostname_length - 1);
  host_id = ntohl (msg->host_id);
  LOG_DEBUG ("Received ADDHOST %u message\n", host_id);
  LOG_DEBUG ("-------host id: %u\n", host_id);
  LOG_DEBUG ("-------hostname: %s\n", hostname);
  if (0 != username_length)
    LOG_DEBUG ("-------username: %s\n", username);
  else
  {
    LOG_DEBUG ("-------username: NULL\n");
    username = NULL;
  }
  LOG_DEBUG ("-------ssh port: %u\n", ntohs (msg->ssh_port));
  host =
      GNUNET_TESTBED_host_create_with_id (host_id, hostname, username,
                                          ntohs (msg->ssh_port));
  GNUNET_assert (NULL != host);
  reply_size = sizeof (struct GNUNET_TESTBED_HostConfirmedMessage);
  if (GNUNET_OK != host_list_add (host))
  {
    /* We are unable to add a host */
    emsg = "A host exists with given host-id";
    LOG_DEBUG ("%s: %u", emsg, host_id);
    GNUNET_TESTBED_host_destroy (host);
    reply_size += strlen (emsg) + 1;
    reply = GNUNET_malloc (reply_size);
    memcpy (&reply[1], emsg, strlen (emsg) + 1);
  }
  else
  {
    LOG_DEBUG ("Added host %u at %u\n",
	       host_id, master_context->host_id);
    reply = GNUNET_malloc (reply_size);
  }
  reply->header.type = htons (GNUNET_MESSAGE_TYPE_TESTBED_ADDHOSTCONFIRM);
  reply->header.size = htons (reply_size);
  reply->host_id = htonl (host_id);
  queue_message (client, &reply->header);
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * Iterator over hash map entries.
 *
 * @param cls closure
 * @param key current key code
 * @param value value in the hash map
 * @return GNUNET_YES if we should continue to
 *         iterate,
 *         GNUNET_NO if not.
 */
int
ss_exists_iterator (void *cls, const struct GNUNET_HashCode *key, void *value)
{
  struct SharedService *queried_ss = cls;
  struct SharedService *ss = value;

  if (0 == strcmp (ss->name, queried_ss->name))
    return GNUNET_NO;
  else
    return GNUNET_YES;
}


/**
 * Message handler for GNUNET_MESSAGE_TYPE_TESTBED_ADDHOST messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_configure_shared_service (void *cls, struct GNUNET_SERVER_Client *client,
                                 const struct GNUNET_MessageHeader *message)
{
  const struct GNUNET_TESTBED_ConfigureSharedServiceMessage *msg;
  struct SharedService *ss;
  char *service_name;
  struct GNUNET_HashCode hash;
  uint16_t msg_size;
  uint16_t service_name_size;

  msg = (const struct GNUNET_TESTBED_ConfigureSharedServiceMessage *) message;
  msg_size = ntohs (message->size);
  if (msg_size <= sizeof (struct GNUNET_TESTBED_ConfigureSharedServiceMessage))
  {
    GNUNET_break (0);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  service_name_size =
      msg_size - sizeof (struct GNUNET_TESTBED_ConfigureSharedServiceMessage);
  service_name = (char *) &msg[1];
  if ('\0' != service_name[service_name_size - 1])
  {
    GNUNET_break (0);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  LOG_DEBUG ("Received service sharing request for %s, with %d peers\n",
             service_name, ntohl (msg->num_peers));
  if (ntohl (msg->host_id) != master_context->host_id)
  {
    route_message (ntohl (msg->host_id), message);
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
  ss = GNUNET_malloc (sizeof (struct SharedService));
  ss->name = strdup (service_name);
  ss->num_shared = ntohl (msg->num_peers);
  GNUNET_CRYPTO_hash (ss->name, service_name_size, &hash);
  if (GNUNET_SYSERR ==
      GNUNET_CONTAINER_multihashmap_get_multiple (ss_map, &hash,
                                                  &ss_exists_iterator, ss))
  {
    LOG (GNUNET_ERROR_TYPE_WARNING,
         "Service %s already configured as a shared service. "
         "Ignoring service sharing request \n", ss->name);
    GNUNET_free (ss->name);
    GNUNET_free (ss);
    return;
  }
  GNUNET_CONTAINER_multihashmap_put (ss_map, &hash, ss,
                                     GNUNET_CONTAINER_MULTIHASHMAPOPTION_MULTIPLE);
}


/**
 * Message handler for GNUNET_MESSAGE_TYPE_TESTBED_LCONTROLLERS message
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_link_controllers (void *cls, struct GNUNET_SERVER_Client *client,
                         const struct GNUNET_MessageHeader *message)
{
  const struct GNUNET_TESTBED_ControllerLinkMessage *msg;
  struct GNUNET_CONFIGURATION_Handle *cfg;
  struct LCFContextQueue *lcfq;
  struct Route *route;
  struct Route *new_route;
  char *config;
  uLongf dest_size;
  size_t config_size;
  uint32_t delegated_host_id;
  uint32_t slave_host_id;
  uint16_t msize;

  if (NULL == master_context)
  {
    GNUNET_break (0);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  msize = ntohs (message->size);
  if (sizeof (struct GNUNET_TESTBED_ControllerLinkMessage) >= msize)
  {
    GNUNET_break (0);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  msg = (const struct GNUNET_TESTBED_ControllerLinkMessage *) message;
  delegated_host_id = ntohl (msg->delegated_host_id);
  if (delegated_host_id == master_context->host_id)
  {
    GNUNET_break (0);
    LOG (GNUNET_ERROR_TYPE_WARNING, "Trying to link ourselves\n");
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  if ((delegated_host_id >= host_list_size) ||
      (NULL == host_list[delegated_host_id]))
  {
    LOG (GNUNET_ERROR_TYPE_WARNING,
	 "Delegated host %u not registered with us\n", delegated_host_id);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  slave_host_id = ntohl (msg->slave_host_id);
  if ((slave_host_id >= host_list_size) || (NULL == host_list[slave_host_id]))
  {
    LOG (GNUNET_ERROR_TYPE_WARNING, "Slave host %u not registered with us\n",
	 slave_host_id);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  if (slave_host_id == delegated_host_id)
  {
    LOG (GNUNET_ERROR_TYPE_WARNING, "Slave and delegated host are same\n");
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }

  if (slave_host_id == master_context->host_id) /* Link from us */
  {
    struct Slave *slave;
    struct LinkControllersContext *lcc;

    msize -= sizeof (struct GNUNET_TESTBED_ControllerLinkMessage);
    config_size = ntohs (msg->config_size);
    if ((delegated_host_id < slave_list_size) && (NULL != slave_list[delegated_host_id]))       /* We have already added */
    {
      LOG (GNUNET_ERROR_TYPE_WARNING, "Host %u already connected\n",
           delegated_host_id);
      GNUNET_SERVER_receive_done (client, GNUNET_OK);
      return;
    }
    config = GNUNET_malloc (config_size);
    dest_size = (uLongf) config_size;
    if (Z_OK !=
        uncompress ((Bytef *) config, &dest_size, (const Bytef *) &msg[1],
                    (uLong) msize))
    {
      GNUNET_break (0);         /* Compression error */
      GNUNET_free (config);
      GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
      return;
    }
    if (config_size != dest_size)
    {
      LOG (GNUNET_ERROR_TYPE_WARNING, "Uncompressed config size mismatch\n");
      GNUNET_free (config);
      GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
      return;
    }
    cfg = GNUNET_CONFIGURATION_create ();       /* Free here or in lcfcontext */
    if (GNUNET_OK !=
        GNUNET_CONFIGURATION_deserialize (cfg, config, config_size, GNUNET_NO))
    {
      GNUNET_break (0);         /* Configuration parsing error */
      GNUNET_free (config);
      GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
      return;
    }
    GNUNET_free (config);
    if ((delegated_host_id < slave_list_size) &&
        (NULL != slave_list[delegated_host_id]))
    {
      GNUNET_break (0);         /* Configuration parsing error */
      GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
      return;
    }
    slave = GNUNET_malloc (sizeof (struct Slave));
    slave->host_id = delegated_host_id;
    slave->reghost_map = GNUNET_CONTAINER_multihashmap_create (100, GNUNET_NO);
    slave_list_add (slave);
    if (1 != msg->is_subordinate)
    {
      slave->controller =
          GNUNET_TESTBED_controller_connect (cfg, host_list[slave->host_id],
                                             event_mask,
                                             &slave_event_callback, slave);
      slave->cfg = cfg;
      if (NULL != slave->controller)
        send_operation_success_msg (client, GNUNET_ntohll (msg->operation_id));
      else
        send_operation_fail_msg (client, GNUNET_ntohll (msg->operation_id),
                                 "Could not connect to delegated controller");
      GNUNET_SERVER_receive_done (client, GNUNET_OK);
      return;
    }
    lcc = GNUNET_malloc (sizeof (struct LinkControllersContext));
    lcc->operation_id = GNUNET_ntohll (msg->operation_id);
    GNUNET_SERVER_client_keep (client);
    lcc->client = client;
    slave->lcc = lcc;
    slave->controller_proc =
	GNUNET_TESTBED_controller_start (master_context->master_ip,
					 host_list[slave->host_id], cfg,
					 &slave_status_callback, slave);
    GNUNET_CONFIGURATION_destroy (cfg);
    new_route = GNUNET_malloc (sizeof (struct Route));
    new_route->dest = delegated_host_id;
    new_route->thru = master_context->host_id;
    route_list_add (new_route);
    return;
  }

  /* Route the request */
  if (slave_host_id >= route_list_size)
  {
    LOG (GNUNET_ERROR_TYPE_WARNING, "No route towards slave host");
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  lcfq = GNUNET_malloc (sizeof (struct LCFContextQueue));
  lcfq->lcf = GNUNET_malloc (sizeof (struct LCFContext));
  lcfq->lcf->delegated_host_id = delegated_host_id;
  lcfq->lcf->slave_host_id = slave_host_id;
  route = find_dest_route (slave_host_id);
  GNUNET_assert (NULL != route);        /* because we add routes carefully */
  GNUNET_assert (route->dest < slave_list_size);
  GNUNET_assert (NULL != slave_list[route->dest]);
  lcfq->lcf->state = INIT;
  lcfq->lcf->operation_id = GNUNET_ntohll (msg->operation_id);
  lcfq->lcf->gateway = slave_list[route->dest];
  lcfq->lcf->msg = GNUNET_malloc (msize);
  (void) memcpy (lcfq->lcf->msg, msg, msize);
  GNUNET_SERVER_client_keep (client);
  lcfq->lcf->client = client;
  if (NULL == lcfq_head)
  {
    GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == lcf_proc_task_id);
    GNUNET_CONTAINER_DLL_insert_tail (lcfq_head, lcfq_tail, lcfq);
    lcf_proc_task_id = GNUNET_SCHEDULER_add_now (&lcf_proc_task, lcfq->lcf);
  }
  else
    GNUNET_CONTAINER_DLL_insert_tail (lcfq_head, lcfq_tail, lcfq);
  /* FIXME: Adding a new route should happen after the controllers are linked
   * successfully */
  if (1 != msg->is_subordinate)
  {
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  if ((delegated_host_id < route_list_size)
      && (NULL != route_list[delegated_host_id]))
  {
    GNUNET_break_op (0);	/* Are you trying to link delegated host twice
				   with is subordinate flag set to GNUNET_YES? */
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  new_route = GNUNET_malloc (sizeof (struct Route));
  new_route->dest = delegated_host_id;
  new_route->thru = route->dest;
  route_list_add (new_route);
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * The task to be executed if the forwarded peer create operation has been
 * timed out
 *
 * @param cls the FowardedOperationContext
 * @param tc the TaskContext from the scheduler
 */
static void
peer_create_forward_timeout (void *cls,
                             const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct ForwardedOperationContext *fopc = cls;

  GNUNET_free (fopc->cls);
  forwarded_operation_timeout (fopc, tc);
}


/**
 * Callback to be called when forwarded peer create operation is successfull. We
 * have to relay the reply msg back to the client
 *
 * @param cls ForwardedOperationContext
 * @param msg the peer create success message
 */
static void
peer_create_success_cb (void *cls, const struct GNUNET_MessageHeader *msg)
{
  struct ForwardedOperationContext *fopc = cls;
  struct Peer *remote_peer;

  if (ntohs (msg->type) == GNUNET_MESSAGE_TYPE_TESTBED_PEERCREATESUCCESS)
  {
    GNUNET_assert (NULL != fopc->cls);
    remote_peer = fopc->cls;
    peer_list_add (remote_peer);
  }
  forwarded_operation_reply_relay (fopc, msg);
}


/**
 * Function to destroy a peer
 *
 * @param peer the peer structure to destroy
 */
static void
destroy_peer (struct Peer *peer)
{
  GNUNET_break (0 == peer->reference_cnt);
  if (GNUNET_YES == peer->is_remote)
  {
    peer_list_remove (peer);
    GNUNET_free (peer);
    return;
  }
  if (GNUNET_YES == peer->details.local.is_running)
  {
    GNUNET_TESTING_peer_stop (peer->details.local.peer);
    peer->details.local.is_running = GNUNET_NO;
  }
  GNUNET_TESTING_peer_destroy (peer->details.local.peer);
  GNUNET_CONFIGURATION_destroy (peer->details.local.cfg);
  peer_list_remove (peer);
  GNUNET_free (peer);
}


/**
 * Callback to be called when forwarded peer destroy operation is successfull. We
 * have to relay the reply msg back to the client
 *
 * @param cls ForwardedOperationContext
 * @param msg the peer create success message
 */
static void
peer_destroy_success_cb (void *cls, const struct GNUNET_MessageHeader *msg)
{
  struct ForwardedOperationContext *fopc = cls;
  struct Peer *remote_peer;

  if (GNUNET_MESSAGE_TYPE_TESTBED_GENERICOPSUCCESS == ntohs (msg->type))
  {
    remote_peer = fopc->cls;
    GNUNET_assert (NULL != remote_peer);
    remote_peer->destroy_flag = GNUNET_YES;
    if (0 == remote_peer->reference_cnt)
      destroy_peer (remote_peer);
  }
  forwarded_operation_reply_relay (fopc, msg);
}



/**
 * Handler for GNUNET_MESSAGE_TYPE_TESTBED_CREATEPEER messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_peer_create (void *cls, struct GNUNET_SERVER_Client *client,
                    const struct GNUNET_MessageHeader *message)
{
  const struct GNUNET_TESTBED_PeerCreateMessage *msg;
  struct GNUNET_TESTBED_PeerCreateSuccessEventMessage *reply;
  struct GNUNET_CONFIGURATION_Handle *cfg;
  struct ForwardedOperationContext *fo_ctxt;
  struct Route *route;
  struct Peer *peer;
  char *config;
  size_t dest_size;
  int ret;
  uint32_t config_size;
  uint32_t host_id;
  uint32_t peer_id;
  uint16_t msize;


  msize = ntohs (message->size);
  if (msize <= sizeof (struct GNUNET_TESTBED_PeerCreateMessage))
  {
    GNUNET_break (0);           /* We need configuration */
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  msg = (const struct GNUNET_TESTBED_PeerCreateMessage *) message;
  host_id = ntohl (msg->host_id);
  peer_id = ntohl (msg->peer_id);
  if (UINT32_MAX == peer_id)
  {
    send_operation_fail_msg (client, GNUNET_ntohll (msg->operation_id),
                             "Cannot create peer with given ID");
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  if (host_id == master_context->host_id)
  {
    char *emsg;

    /* We are responsible for this peer */
    msize -= sizeof (struct GNUNET_TESTBED_PeerCreateMessage);
    config_size = ntohl (msg->config_size);
    config = GNUNET_malloc (config_size);
    dest_size = config_size;
    if (Z_OK !=
        (ret =
         uncompress ((Bytef *) config, (uLongf *) & dest_size,
                     (const Bytef *) &msg[1], (uLong) msize)))
    {
      GNUNET_break (0);         /* uncompression error */
      GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
      return;
    }
    if (config_size != dest_size)
    {
      GNUNET_break (0);         /* Uncompressed config size mismatch */
      GNUNET_free (config);
      GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
      return;
    }
    cfg = GNUNET_CONFIGURATION_create ();
    if (GNUNET_OK !=
        GNUNET_CONFIGURATION_deserialize (cfg, config, config_size, GNUNET_NO))
    {
      GNUNET_break (0);         /* Configuration parsing error */
      GNUNET_free (config);
      GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
      return;
    }
    GNUNET_free (config);
    peer = GNUNET_malloc (sizeof (struct Peer));
    peer->is_remote = GNUNET_NO;
    peer->details.local.cfg = cfg;
    peer->id = peer_id;
    LOG_DEBUG ("Creating peer with id: %u\n", peer->id);
    peer->details.local.peer =
        GNUNET_TESTING_peer_configure (master_context->system,
                                       peer->details.local.cfg, peer->id,
                                       NULL /* Peer id */ ,
                                       &emsg);
    if (NULL == peer->details.local.peer)
    {
      LOG (GNUNET_ERROR_TYPE_WARNING, "Configuring peer failed: %s\n", emsg);
      GNUNET_free (emsg);
      GNUNET_free (peer);
      GNUNET_break (0);
      GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
      return;
    }
    peer->details.local.is_running = GNUNET_NO;
    peer_list_add (peer);
    reply =
        GNUNET_malloc (sizeof
                       (struct GNUNET_TESTBED_PeerCreateSuccessEventMessage));
    reply->header.size =
        htons (sizeof (struct GNUNET_TESTBED_PeerCreateSuccessEventMessage));
    reply->header.type = htons (GNUNET_MESSAGE_TYPE_TESTBED_PEERCREATESUCCESS);
    reply->peer_id = msg->peer_id;
    reply->operation_id = msg->operation_id;
    queue_message (client, &reply->header);
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }

  /* Forward peer create request */
  route = find_dest_route (host_id);
  if (NULL == route)
  {
    GNUNET_break (0);
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }

  peer = GNUNET_malloc (sizeof (struct Peer));
  peer->is_remote = GNUNET_YES;
  peer->id = peer_id;
  peer->details.remote.slave = slave_list[route->dest];
  peer->details.remote.remote_host_id = host_id;
  fo_ctxt = GNUNET_malloc (sizeof (struct ForwardedOperationContext));
  GNUNET_SERVER_client_keep (client);
  fo_ctxt->client = client;
  fo_ctxt->operation_id = GNUNET_ntohll (msg->operation_id);
  fo_ctxt->cls = peer; //slave_list[route->dest]->controller;
  fo_ctxt->type = OP_PEER_CREATE;
  fo_ctxt->opc =
      GNUNET_TESTBED_forward_operation_msg_ (slave_list [route->dest]->controller,
                                             fo_ctxt->operation_id,
                                             &msg->header,
                                             peer_create_success_cb, fo_ctxt);
  fo_ctxt->timeout_task =
      GNUNET_SCHEDULER_add_delayed (TIMEOUT, &peer_create_forward_timeout,
                                    fo_ctxt);
  GNUNET_CONTAINER_DLL_insert_tail (fopcq_head, fopcq_tail, fo_ctxt);
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * Message handler for GNUNET_MESSAGE_TYPE_TESTBED_DESTROYPEER messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_peer_destroy (void *cls, struct GNUNET_SERVER_Client *client,
                     const struct GNUNET_MessageHeader *message)
{
  const struct GNUNET_TESTBED_PeerDestroyMessage *msg;
  struct ForwardedOperationContext *fopc;
  struct Peer *peer;
  uint32_t peer_id;

  msg = (const struct GNUNET_TESTBED_PeerDestroyMessage *) message;
  peer_id = ntohl (msg->peer_id);
  LOG_DEBUG ("Received peer destory on peer: %u and operation id: %ul\n",
             peer_id, GNUNET_ntohll (msg->operation_id));
  if ((peer_list_size <= peer_id) || (NULL == peer_list[peer_id]))
  {
    LOG (GNUNET_ERROR_TYPE_ERROR,
         "Asked to destroy a non existent peer with id: %u\n", peer_id);
    send_operation_fail_msg (client, GNUNET_ntohll (msg->operation_id),
                             "Peer doesn't exist");
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  peer = peer_list[peer_id];
  if (GNUNET_YES == peer->is_remote)
  {
    /* Forward the destory message to sub controller */
    fopc = GNUNET_malloc (sizeof (struct ForwardedOperationContext));
    GNUNET_SERVER_client_keep (client);
    fopc->client = client;
    fopc->cls = peer;
    fopc->type = OP_PEER_DESTROY;
    fopc->operation_id = GNUNET_ntohll (msg->operation_id);    
    fopc->opc = 
        GNUNET_TESTBED_forward_operation_msg_ (peer->details.remote.slave->controller,
                                               fopc->operation_id, &msg->header,
                                               &peer_destroy_success_cb,
                                               fopc);
    fopc->timeout_task =
        GNUNET_SCHEDULER_add_delayed (TIMEOUT, &forwarded_operation_timeout,
                                      fopc);
    GNUNET_CONTAINER_DLL_insert_tail (fopcq_head, fopcq_tail, fopc);
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  peer->destroy_flag = GNUNET_YES;
  if (0 == peer->reference_cnt)
    destroy_peer (peer);
  else
    LOG (GNUNET_ERROR_TYPE_DEBUG,
         "Delaying peer destroy as peer is currently in use\n");
  send_operation_success_msg (client, GNUNET_ntohll (msg->operation_id));
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * Message handler for GNUNET_MESSAGE_TYPE_TESTBED_DESTROYPEER messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_peer_start (void *cls, struct GNUNET_SERVER_Client *client,
                   const struct GNUNET_MessageHeader *message)
{
  const struct GNUNET_TESTBED_PeerStartMessage *msg;
  struct GNUNET_TESTBED_PeerEventMessage *reply;
  struct ForwardedOperationContext *fopc;
  struct Peer *peer;
  uint32_t peer_id;

  msg = (const struct GNUNET_TESTBED_PeerStartMessage *) message;
  peer_id = ntohl (msg->peer_id);
  if ((peer_id >= peer_list_size) || (NULL == peer_list[peer_id]))
  {
    GNUNET_break (0);
    LOG (GNUNET_ERROR_TYPE_ERROR,
         "Asked to start a non existent peer with id: %u\n", peer_id);
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  peer = peer_list[peer_id];
  if (GNUNET_YES == peer->is_remote)
  {
    fopc = GNUNET_malloc (sizeof (struct ForwardedOperationContext));
    GNUNET_SERVER_client_keep (client);
    fopc->client = client;
    fopc->operation_id = GNUNET_ntohll (msg->operation_id);
    fopc->type = OP_PEER_START;
    fopc->opc =
        GNUNET_TESTBED_forward_operation_msg_ (peer->details.remote.slave->controller,
                                               fopc->operation_id, &msg->header,
                                               &forwarded_operation_reply_relay,
                                               fopc);
    fopc->timeout_task =
        GNUNET_SCHEDULER_add_delayed (TIMEOUT, &forwarded_operation_timeout,
                                      fopc);
    GNUNET_CONTAINER_DLL_insert_tail (fopcq_head, fopcq_tail, fopc);
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  if (GNUNET_OK != GNUNET_TESTING_peer_start (peer->details.local.peer))
  {
    send_operation_fail_msg (client, GNUNET_ntohll (msg->operation_id),
                             "Failed to start");
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  peer->details.local.is_running = GNUNET_YES;
  reply = GNUNET_malloc (sizeof (struct GNUNET_TESTBED_PeerEventMessage));
  reply->header.type = htons (GNUNET_MESSAGE_TYPE_TESTBED_PEEREVENT);
  reply->header.size = htons (sizeof (struct GNUNET_TESTBED_PeerEventMessage));
  reply->event_type = htonl (GNUNET_TESTBED_ET_PEER_START);
  reply->host_id = htonl (master_context->host_id);
  reply->peer_id = msg->peer_id;
  reply->operation_id = msg->operation_id;
  queue_message (client, &reply->header);
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * Message handler for GNUNET_MESSAGE_TYPE_TESTBED_DESTROYPEER messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_peer_stop (void *cls, struct GNUNET_SERVER_Client *client,
                  const struct GNUNET_MessageHeader *message)
{
  const struct GNUNET_TESTBED_PeerStopMessage *msg;
  struct GNUNET_TESTBED_PeerEventMessage *reply;
  struct ForwardedOperationContext *fopc;
  struct Peer *peer;
  uint32_t peer_id;

  msg = (const struct GNUNET_TESTBED_PeerStopMessage *) message;
  peer_id = ntohl (msg->peer_id);
  if ((peer_id >= peer_list_size) || (NULL == peer_list[peer_id]))
  {
    send_operation_fail_msg (client, GNUNET_ntohll (msg->operation_id),
                             "Peer not found");
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  peer = peer_list[peer_id];
  if (GNUNET_YES == peer->is_remote)
  {
    fopc = GNUNET_malloc (sizeof (struct ForwardedOperationContext));
    GNUNET_SERVER_client_keep (client);
    fopc->client = client;
    fopc->operation_id = GNUNET_ntohll (msg->operation_id);
    fopc->type = OP_PEER_STOP;
    fopc->opc =
        GNUNET_TESTBED_forward_operation_msg_ (peer->details.remote.slave->controller,
                                               fopc->operation_id, &msg->header,
                                               &forwarded_operation_reply_relay,
                                               fopc);
    fopc->timeout_task =
        GNUNET_SCHEDULER_add_delayed (TIMEOUT, &forwarded_operation_timeout,
                                      fopc);
    GNUNET_CONTAINER_DLL_insert_tail (fopcq_head, fopcq_tail, fopc);
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  if (GNUNET_OK != GNUNET_TESTING_peer_stop (peer->details.local.peer))
  {
    send_operation_fail_msg (client, GNUNET_ntohll (msg->operation_id),
                             "Peer not running");
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  peer->details.local.is_running = GNUNET_NO;
  reply = GNUNET_malloc (sizeof (struct GNUNET_TESTBED_PeerEventMessage));
  reply->header.type = htons (GNUNET_MESSAGE_TYPE_TESTBED_PEEREVENT);
  reply->header.size = htons (sizeof (struct GNUNET_TESTBED_PeerEventMessage));
  reply->event_type = htonl (GNUNET_TESTBED_ET_PEER_STOP);
  reply->host_id = htonl (master_context->host_id);
  reply->peer_id = msg->peer_id;
  reply->operation_id = msg->operation_id;
  queue_message (client, &reply->header);
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * Handler for GNUNET_MESSAGE_TYPE_TESTBED_GETPEERCONFIG messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_peer_get_config (void *cls, struct GNUNET_SERVER_Client *client,
                        const struct GNUNET_MessageHeader *message)
{
  const struct GNUNET_TESTBED_PeerGetConfigurationMessage *msg;
  struct GNUNET_TESTBED_PeerConfigurationInformationMessage *reply;
  struct Peer *peer;
  char *config;
  char *xconfig;
  size_t c_size;
  size_t xc_size;
  uint32_t peer_id;
  uint16_t msize;

  msg = (const struct GNUNET_TESTBED_PeerGetConfigurationMessage *) message;
  peer_id = ntohl (msg->peer_id);
  if ((peer_id >= peer_list_size) || (NULL == peer_list[peer_id]))
  {
    send_operation_fail_msg (client, GNUNET_ntohll (msg->operation_id),
                             "Peer not found");
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  peer = peer_list[peer_id];
  if (GNUNET_YES == peer->is_remote)
  {
    struct ForwardedOperationContext *fopc;
    
    fopc = GNUNET_malloc (sizeof (struct ForwardedOperationContext));
    GNUNET_SERVER_client_keep (client);
    fopc->client = client;
    fopc->operation_id = GNUNET_ntohll (msg->operation_id);
    fopc->type = OP_PEER_INFO;
    fopc->opc =
        GNUNET_TESTBED_forward_operation_msg_ (peer->details.remote.slave->controller,
                                               fopc->operation_id, &msg->header,
                                               &forwarded_operation_reply_relay,
                                               fopc);
    fopc->timeout_task =
        GNUNET_SCHEDULER_add_delayed (TIMEOUT, &forwarded_operation_timeout,
                                      fopc);
    GNUNET_CONTAINER_DLL_insert_tail (fopcq_head, fopcq_tail, fopc); 
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  config =
      GNUNET_CONFIGURATION_serialize (peer_list[peer_id]->details.local.cfg,
                                      &c_size);
  xc_size = GNUNET_TESTBED_compress_config_ (config, c_size, &xconfig);
  GNUNET_free (config);
  msize =
      xc_size +
      sizeof (struct GNUNET_TESTBED_PeerConfigurationInformationMessage);
  reply = GNUNET_realloc (xconfig, msize);
  (void) memmove (&reply[1], reply, xc_size);
  reply->header.size = htons (msize);
  reply->header.type = htons (GNUNET_MESSAGE_TYPE_TESTBED_PEERCONFIG);
  reply->peer_id = msg->peer_id;
  reply->operation_id = msg->operation_id;
  GNUNET_TESTING_peer_get_identity (peer_list[peer_id]->details.local.peer,
                                    &reply->peer_identity);
  reply->config_size = htons ((uint16_t) c_size);
  queue_message (client, &reply->header);
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * Cleanup overlay connect context structure
 *
 * @param occ the overlay connect context
 */
static void
cleanup_occ (struct OverlayConnectContext *occ)
{
  LOG_DEBUG ("Cleaning up occ\n");
  GNUNET_free_non_null (occ->emsg);
  GNUNET_free_non_null (occ->hello);
  GNUNET_SERVER_client_drop (occ->client);
  if (NULL != occ->opc)
    GNUNET_TESTBED_forward_operation_msg_cancel_ (occ->opc);
  if (GNUNET_SCHEDULER_NO_TASK != occ->send_hello_task)
    GNUNET_SCHEDULER_cancel (occ->send_hello_task);
  if (GNUNET_SCHEDULER_NO_TASK != occ->cleanup_task)
    GNUNET_SCHEDULER_cancel (occ->cleanup_task);
  if (GNUNET_SCHEDULER_NO_TASK != occ->timeout_task)
    GNUNET_SCHEDULER_cancel (occ->timeout_task);
  if (NULL != occ->ch)
  {
    GNUNET_CORE_disconnect (occ->ch);
    occ->peer->reference_cnt--;
  }
  if (NULL != occ->ghh)
    GNUNET_TRANSPORT_get_hello_cancel (occ->ghh);
  if (NULL != occ->ohh)
    GNUNET_TRANSPORT_offer_hello_cancel (occ->ohh);
  if (GNUNET_SCHEDULER_NO_TASK != occ->tcc.task)
    GNUNET_SCHEDULER_cancel (occ->tcc.task);
  if (NULL != occ->tcc.tch)
    GNUNET_TRANSPORT_try_connect_cancel (occ->tcc.tch);
  if (NULL != occ->p1th)
  {
    GNUNET_TRANSPORT_disconnect (occ->p1th);
    occ->peer->reference_cnt--;
  }
  if (NULL != occ->tcc.th)
  {
    GNUNET_TRANSPORT_disconnect (occ->tcc.th);
    peer_list[occ->other_peer_id]->reference_cnt--;
  }
  if ((GNUNET_YES == occ->peer->destroy_flag)
      && (0 == occ->peer->reference_cnt))
    destroy_peer (occ->peer);
  if ((NULL == occ->peer2_controller)
      && (GNUNET_YES == peer_list[occ->other_peer_id]->destroy_flag)
        && (0 == peer_list[occ->other_peer_id]->reference_cnt))
      destroy_peer (peer_list[occ->other_peer_id]);  
  GNUNET_CONTAINER_DLL_remove (occq_head, occq_tail, occ);
  GNUNET_free (occ);
}


/**
 * Task for cleaing up overlay connect context structure
 *
 * @param cls the overlay connect context
 * @param tc the task context
 */
static void
do_cleanup_occ (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct OverlayConnectContext *occ = cls;
  
  occ->cleanup_task = GNUNET_SCHEDULER_NO_TASK;
  cleanup_occ (occ);
}


/**
 * Task which will be run when overlay connect request has been timed out
 *
 * @param cls the OverlayConnectContext
 * @param tc the TaskContext
 */
static void
timeout_overlay_connect (void *cls,
                         const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct OverlayConnectContext *occ = cls;

  occ->timeout_task = GNUNET_SCHEDULER_NO_TASK;
  LOG (GNUNET_ERROR_TYPE_WARNING,
       "Timeout while connecting peers %u and %u\n",
       occ->peer_id, occ->other_peer_id);
  send_operation_fail_msg (occ->client, occ->op_id, occ->emsg);
  cleanup_occ (occ);
}



/**
 * Function called to notify transport users that another
 * peer connected to us.
 *
 * @param cls closure
 * @param new_peer the peer that connected
 * @param ats performance data
 * @param ats_count number of entries in ats (excluding 0-termination)
 */
static void
overlay_connect_notify (void *cls, const struct GNUNET_PeerIdentity *new_peer,
                        const struct GNUNET_ATS_Information *ats,
                        unsigned int ats_count)
{
  struct OverlayConnectContext *occ = cls;
  struct GNUNET_TESTBED_ConnectionEventMessage *msg;
  char *new_peer_str;
  char *other_peer_str;

  LOG_DEBUG ("Overlay connect notify\n");
  if (0 ==
      memcmp (new_peer, &occ->peer_identity,
              sizeof (struct GNUNET_PeerIdentity)))
    return;
  new_peer_str = GNUNET_strdup (GNUNET_i2s (new_peer));
  other_peer_str = GNUNET_strdup (GNUNET_i2s (&occ->other_peer_identity));
  if (0 !=
      memcmp (new_peer, &occ->other_peer_identity,
              sizeof (struct GNUNET_PeerIdentity)))
  {
    LOG_DEBUG ("Unexpected peer %4s connected when expecting peer %4s\n",
	       new_peer_str, other_peer_str);
    GNUNET_free (new_peer_str);
    GNUNET_free (other_peer_str);
    return;
  }
  GNUNET_free (new_peer_str);
  LOG_DEBUG ("Peer %4s connected to peer %4s\n", other_peer_str, 
             GNUNET_i2s (&occ->peer_identity));
  GNUNET_free (other_peer_str);
  if (GNUNET_SCHEDULER_NO_TASK != occ->send_hello_task)
  {
    GNUNET_SCHEDULER_cancel (occ->send_hello_task);
    occ->send_hello_task = GNUNET_SCHEDULER_NO_TASK;
  }
  GNUNET_assert (GNUNET_SCHEDULER_NO_TASK != occ->timeout_task);
  GNUNET_SCHEDULER_cancel (occ->timeout_task);
  occ->timeout_task = GNUNET_SCHEDULER_NO_TASK;
  if (GNUNET_SCHEDULER_NO_TASK != occ->tcc.task)
  {
    GNUNET_SCHEDULER_cancel (occ->tcc.task);
    occ->tcc.task = GNUNET_SCHEDULER_NO_TASK;
  }
  GNUNET_free_non_null (occ->emsg);
  occ->emsg = NULL;
  LOG_DEBUG ("Peers connected - Sending overlay connect success\n");
  msg = GNUNET_malloc (sizeof (struct GNUNET_TESTBED_ConnectionEventMessage));
  msg->header.size =
      htons (sizeof (struct GNUNET_TESTBED_ConnectionEventMessage));
  msg->header.type = htons (GNUNET_MESSAGE_TYPE_TESTBED_PEERCONEVENT);
  msg->event_type = htonl (GNUNET_TESTBED_ET_CONNECT);
  msg->peer1 = htonl (occ->peer_id);
  msg->peer2 = htonl (occ->other_peer_id);
  msg->operation_id = GNUNET_htonll (occ->op_id);
  queue_message (occ->client, &msg->header);
  occ->cleanup_task = GNUNET_SCHEDULER_add_now (&do_cleanup_occ, occ);
  //cleanup_occ (occ);
}


/**
 * Task to ask transport of a peer to connect to another peer
 *
 * @param cls the TryConnectContext
 * @param tc the scheduler task context
 */
static void
try_connect_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc);


/**
 * Callback to be called with result of the try connect request.
 *
 * @param cls the overlay connect context
 * @param result GNUNET_OK if message was transmitted to transport service
 *               GNUNET_SYSERR if message was not transmitted to transport service
 */
static void 
try_connect_cb (void *cls, const int result)
{
  struct TryConnectContext *tcc = cls;

  tcc->tch = NULL;
  GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == tcc->task);
  tcc->task = GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_relative_multiply
                                            (GNUNET_TIME_UNIT_MILLISECONDS,
                                             pow (10, ++tcc->retries)),
                                            &try_connect_task, tcc);
}


/**
 * Task to ask transport of a peer to connect to another peer
 *
 * @param cls the TryConnectContext
 * @param tc the scheduler task context
 */
static void
try_connect_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct TryConnectContext *tcc = cls;

  tcc->task = GNUNET_SCHEDULER_NO_TASK;
  if (0 != (GNUNET_SCHEDULER_REASON_SHUTDOWN & tc->reason))
    return;
  GNUNET_assert (NULL == tcc->tch);
  GNUNET_assert (NULL != tcc->pid);
  GNUNET_assert (NULL != tcc->th);
  tcc->tch = GNUNET_TRANSPORT_try_connect (tcc->th, tcc->pid, &try_connect_cb, tcc);
}


/**
 * Task to offer HELLO of peer 1 to peer 2 and try to make peer 2 to connect to
 * peer 1.
 *
 * @param cls the OverlayConnectContext
 * @param tc the TaskContext from scheduler
 */
static void
send_hello (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc);


/**
 * Task that is run when hello has been sent
 *
 * @param cls the overlay connect context
 * @param tc the scheduler task context; if tc->reason =
 *          GNUNET_SCHEDULER_REASON_TIMEOUT then sending HELLO failed; if
 *          GNUNET_SCHEDULER_REASON_READ_READY is succeeded
 */
static void
occ_hello_sent_cb (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct OverlayConnectContext *occ = cls;

  occ->ohh = NULL;
  GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == occ->send_hello_task);
  if (GNUNET_SCHEDULER_REASON_TIMEOUT == tc->reason)
  {
    GNUNET_free_non_null (occ->emsg);
    occ->emsg = GNUNET_strdup ("Timeout while offering HELLO to other peer");
    occ->send_hello_task = GNUNET_SCHEDULER_add_now (&send_hello, occ);
    return;
  }
  if (GNUNET_SCHEDULER_REASON_READ_READY != tc->reason)
    return;
  GNUNET_free_non_null (occ->emsg);
  occ->emsg = GNUNET_strdup ("Timeout while try connect\n");
  occ->tcc.pid =  &occ->peer_identity;
  occ->tcc.task = GNUNET_SCHEDULER_add_now (&try_connect_task, &occ->tcc);
}


/**
 * Task to offer HELLO of peer 1 to peer 2 and try to make peer 2 to connect to
 * peer 1.
 *
 * @param cls the OverlayConnectContext
 * @param tc the TaskContext from scheduler
 */
static void
send_hello (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct OverlayConnectContext *occ = cls;
  char *other_peer_str;

  occ->send_hello_task = GNUNET_SCHEDULER_NO_TASK;
  if (0 != (tc->reason & GNUNET_SCHEDULER_REASON_SHUTDOWN))
    return;
  GNUNET_assert (NULL != occ->hello);
  other_peer_str = GNUNET_strdup (GNUNET_i2s (&occ->other_peer_identity));
  if (NULL != occ->peer2_controller)
  {
    struct GNUNET_TESTBED_RequestConnectMessage *msg;
    uint16_t msize;
    uint16_t hello_size;

    LOG_DEBUG ("Offering HELLO of %s to %s via Remote Overlay Request\n", 
	       GNUNET_i2s (&occ->peer_identity), other_peer_str);
    hello_size = ntohs (occ->hello->size);
    msize = sizeof (struct GNUNET_TESTBED_RequestConnectMessage) + hello_size;
    msg = GNUNET_malloc (msize);
    msg->header.type = htons (GNUNET_MESSAGE_TYPE_TESTBED_REQUESTCONNECT);
    msg->header.size = htons (msize);
    msg->peer = htonl (occ->other_peer_id);
    msg->operation_id = GNUNET_htonll (occ->op_id);
    (void) memcpy (&msg->peer_identity, &occ->peer_identity,
		   sizeof (struct GNUNET_PeerIdentity));
    memcpy (msg->hello, occ->hello, hello_size);
    GNUNET_TESTBED_queue_message_ (occ->peer2_controller, &msg->header);
  }
  else
  {
    LOG_DEBUG ("Offering HELLO of %s to %s\n", 
	       GNUNET_i2s (&occ->peer_identity), other_peer_str);
    occ->ohh = GNUNET_TRANSPORT_offer_hello (occ->tcc.th,
                                             occ->hello,
                                             occ_hello_sent_cb,
                                             occ);
    if (NULL == occ->ohh)
    {
      GNUNET_break (0);
      occ->send_hello_task =
          GNUNET_SCHEDULER_add_delayed
          (GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_MILLISECONDS,
                                          100 + GNUNET_CRYPTO_random_u32
                                          (GNUNET_CRYPTO_QUALITY_WEAK, 500)),
           &send_hello, occ);
    }
  }
  GNUNET_free (other_peer_str);  
}

/**
 * Test for checking whether HELLO message is empty
 *
 * @param cls empty flag to set
 * @param address the HELLO
 * @param expiration expiration of the HELLO
 * @return
 */
static int
test_address (void *cls, const struct GNUNET_HELLO_Address *address,
              struct GNUNET_TIME_Absolute expiration)
{
  int *empty = cls;

  *empty = GNUNET_NO;
  return GNUNET_OK;
}


/**
 * Function called whenever there is an update to the HELLO of peers in the
 * OverlayConnectClosure. If we have a valid HELLO, we connect to the peer 2's
 * transport and offer peer 1's HELLO and ask peer 2 to connect to peer 1
 *
 * @param cls closure
 * @param hello our updated HELLO
 */
static void
hello_update_cb (void *cls, const struct GNUNET_MessageHeader *hello)
{
  struct OverlayConnectContext *occ = cls;
  int empty;
  uint16_t msize;

  msize = ntohs (hello->size);
  empty = GNUNET_YES;
  (void) GNUNET_HELLO_iterate_addresses ((const struct GNUNET_HELLO_Message *)
                                         hello, GNUNET_NO, &test_address,
                                         &empty);
  if (GNUNET_YES == empty)
  {
    LOG_DEBUG ("HELLO of %s is empty\n", GNUNET_i2s (&occ->peer_identity));
    return;
  }
  LOG_DEBUG ("Received HELLO of %s\n", GNUNET_i2s (&occ->peer_identity));
  occ->hello = GNUNET_malloc (msize);
  memcpy (occ->hello, hello, msize);
  GNUNET_TRANSPORT_get_hello_cancel (occ->ghh);
  occ->ghh = NULL;
  GNUNET_TRANSPORT_disconnect (occ->p1th);
  occ->p1th = NULL;
  occ->peer->reference_cnt--;
  GNUNET_free_non_null (occ->emsg);
  if (NULL == occ->peer2_controller)
  {
    peer_list[occ->other_peer_id]->reference_cnt++;
    occ->tcc.th =
	GNUNET_TRANSPORT_connect (peer_list[occ->other_peer_id]->details.local.cfg,
				  &occ->other_peer_identity, NULL, NULL, NULL,
				  NULL);
    if (NULL == occ->tcc.th)
    {
      GNUNET_asprintf (&occ->emsg, "Cannot connect to TRANSPORT of %s\n",
		       GNUNET_i2s (&occ->other_peer_identity));
      GNUNET_SCHEDULER_cancel (occ->timeout_task);
      occ->timeout_task = GNUNET_SCHEDULER_add_now (&timeout_overlay_connect, occ);
      return;
    }
  }
  occ->emsg = GNUNET_strdup ("Timeout while offering HELLO to other peer");
  occ->send_hello_task = GNUNET_SCHEDULER_add_now (&send_hello, occ);
}


/**
 * Function called after GNUNET_CORE_connect has succeeded (or failed
 * for good).  Note that the private key of the peer is intentionally
 * not exposed here; if you need it, your process should try to read
 * the private key file directly (which should work if you are
 * authorized...).
 *
 * @param cls closure
 * @param server handle to the server, NULL if we failed
 * @param my_identity ID of this peer, NULL if we failed
 */
static void
core_startup_cb (void *cls, struct GNUNET_CORE_Handle *server,
                 const struct GNUNET_PeerIdentity *my_identity)
{
  struct OverlayConnectContext *occ = cls;

  GNUNET_free_non_null (occ->emsg);
  occ->emsg = GNUNET_strdup ("Failed to connect to CORE\n");
  if ((NULL == server) || (NULL == my_identity))
    goto error_return;
  GNUNET_free (occ->emsg);
  occ->ch = server;
  occ->emsg = NULL;
  memcpy (&occ->peer_identity, my_identity,
          sizeof (struct GNUNET_PeerIdentity));
  occ->peer->reference_cnt++;
  occ->p1th =
      GNUNET_TRANSPORT_connect (occ->peer->details.local.cfg,
                                &occ->peer_identity, NULL, NULL, NULL, NULL);
  if (NULL == occ->p1th)
  {
    GNUNET_asprintf (&occ->emsg, "Cannot connect to TRANSPORT of peers %4s",
		    GNUNET_i2s (&occ->peer_identity));
    goto error_return;
  }
  LOG_DEBUG ("Acquiring HELLO of peer %s\n", GNUNET_i2s (&occ->peer_identity));
  occ->emsg = GNUNET_strdup ("Timeout while acquiring HELLO message");
  occ->ghh = GNUNET_TRANSPORT_get_hello (occ->p1th, &hello_update_cb, occ);
  return;
  
 error_return:
  GNUNET_SCHEDULER_cancel (occ->timeout_task);
  occ->timeout_task = GNUNET_SCHEDULER_add_now (&timeout_overlay_connect, occ);
  return;
}


/**
 * Callback to be called when forwarded get peer config operation as part of
 * overlay connect is successfull. Connection to Peer 1's core is made and is
 * checked for new connection from peer 2
 *
 * @param cls ForwardedOperationContext
 * @param msg the peer create success message
 */
static void
overlay_connect_get_config (void *cls, const struct GNUNET_MessageHeader *msg)
{
  struct OverlayConnectContext *occ = cls;
  const struct GNUNET_TESTBED_PeerConfigurationInformationMessage *cmsg;
  const struct GNUNET_CORE_MessageHandler no_handlers[] = {
    {NULL, 0, 0}
  };

  occ->opc = NULL;
  if (GNUNET_MESSAGE_TYPE_TESTBED_PEERCONFIG != ntohs (msg->type))
    goto error_return;
  cmsg = (const struct GNUNET_TESTBED_PeerConfigurationInformationMessage *)
      msg;
  memcpy (&occ->other_peer_identity, &cmsg->peer_identity,
	  sizeof (struct GNUNET_PeerIdentity));
  GNUNET_free_non_null (occ->emsg);
  occ->emsg = GNUNET_strdup ("Timeout while connecting to CORE");
  occ->peer->reference_cnt++;
  occ->ch =
      GNUNET_CORE_connect (occ->peer->details.local.cfg, occ, &core_startup_cb,
                           &overlay_connect_notify, NULL, NULL, GNUNET_NO, NULL,
                           GNUNET_NO, no_handlers);
  if (NULL == occ->ch)
    goto error_return;
  return;

 error_return:
  GNUNET_SCHEDULER_cancel (occ->timeout_task);
  occ->timeout_task = 
      GNUNET_SCHEDULER_add_now (&timeout_overlay_connect, occ);
}


/**
 * Callback which will be called to after a host registration succeeded or failed
 *
 * @param cls the RegisteredHostContext
 * @param emsg the error message; NULL if host registration is successful
 */
static void 
registeredhost_registration_completion (void *cls, const char *emsg)
{
  struct RegisteredHostContext *rhc = cls;
  struct GNUNET_CONFIGURATION_Handle *cfg;
  uint32_t peer2_host_id;

  /* if (NULL != rhc->focc_dll_head) */
  /*   process_next_focc (rhc); */
  peer2_host_id = GNUNET_TESTBED_host_get_id_ (rhc->reg_host);
  GNUNET_assert (RHC_INIT == rhc->state);
  GNUNET_assert (NULL == rhc->sub_op);
  if ((NULL == rhc->gateway2)
      || ((peer2_host_id < slave_list_size) /* Check if we have the needed config */
          && (NULL != slave_list[peer2_host_id])))
  {
    rhc->state = RHC_LINK;
    cfg = (NULL == rhc->gateway2) ? our_config : slave_list[peer2_host_id]->cfg;
    rhc->sub_op =
        GNUNET_TESTBED_controller_link (rhc,
                                        rhc->gateway->controller,
                                        rhc->reg_host,
                                        rhc->host,
                                        cfg,
                                        GNUNET_NO);
    return;
  }
  rhc->state = RHC_GET_CFG;
  rhc->sub_op =  GNUNET_TESTBED_get_slave_config (rhc,
                                                  rhc->gateway2->controller,
                                                  rhc->reg_host);
}


/**
 * Iterator to match a registered host context
 *
 * @param cls pointer 2 pointer of RegisteredHostContext
 * @param key current key code
 * @param value value in the hash map
 * @return GNUNET_YES if we should continue to
 *         iterate,
 *         GNUNET_NO if not.
 */
static int 
reghost_match_iterator (void *cls,
                        const struct GNUNET_HashCode * key,
                        void *value)
{
  struct RegisteredHostContext **rh = cls;
  struct RegisteredHostContext *rh_val = value;

  if ((rh_val->host == (*rh)->host) && (rh_val->reg_host == (*rh)->reg_host))
  {
    GNUNET_free (*rh);
    *rh = rh_val;
    return GNUNET_NO;
  }
  return GNUNET_YES;
}


/**
 * Function to generate the hashcode corresponding to a RegisteredHostContext
 *
 * @param reg_host the host which is being registered in RegisteredHostContext
 * @param host the host of the controller which has to connect to the above rhost
 * @return the hashcode
 */
static struct GNUNET_HashCode
hash_hosts (struct GNUNET_TESTBED_Host *reg_host,
            struct GNUNET_TESTBED_Host *host)
{
  struct GNUNET_HashCode hash;
  uint32_t host_ids[2];

  host_ids[0] = GNUNET_TESTBED_host_get_id_ (reg_host);
  host_ids[1] = GNUNET_TESTBED_host_get_id_ (host);
  GNUNET_CRYPTO_hash (host_ids, sizeof (host_ids), &hash);
  return hash;
}


/**
 * Handler for GNUNET_MESSAGE_TYPE_TESTBED_OLCONNECT messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_overlay_connect (void *cls, struct GNUNET_SERVER_Client *client,
                        const struct GNUNET_MessageHeader *message)
{
  const struct GNUNET_TESTBED_OverlayConnectMessage *msg;
  const struct GNUNET_CORE_MessageHandler no_handlers[] = {
    {NULL, 0, 0}
  };
  struct Peer *peer;
  struct OverlayConnectContext *occ;
  struct GNUNET_TESTBED_Controller *peer2_controller;
  uint64_t operation_id;
  uint32_t p1;
  uint32_t p2; 
  uint32_t peer2_host_id;

  msg = (const struct GNUNET_TESTBED_OverlayConnectMessage *) message;
  p1 = ntohl (msg->peer1);
  p2 = ntohl (msg->peer2);
  peer2_host_id = ntohl (msg->peer2_host_id);
  GNUNET_assert (p1 < peer_list_size);
  GNUNET_assert (NULL != peer_list[p1]);
  peer = peer_list[p1];
  operation_id = GNUNET_ntohll (msg->operation_id);  
  LOG_DEBUG ("Received overlay connect for peers %u and %u with op id: 0x%lx\n",
	     p1, p2, operation_id);
  if (GNUNET_YES == peer->is_remote)
  {
    struct ForwardedOperationContext *fopc;
    struct Route *route_to_peer2_host;
    struct Route *route_to_peer1_host;

    LOG_DEBUG ("Forwarding overlay connect\n");
    route_to_peer2_host = NULL;
    route_to_peer1_host = NULL;
    route_to_peer2_host = find_dest_route (peer2_host_id);
    if ((NULL != route_to_peer2_host) 
        || (peer2_host_id == master_context->host_id))
    {
      /* Peer 2 either below us OR with us */
      route_to_peer1_host = 
          find_dest_route (peer_list[p1]->details.remote.remote_host_id);
      /* Because we get this message only if we know where peer 1 is */
      GNUNET_assert (NULL != route_to_peer1_host);
      if ((peer2_host_id == master_context->host_id) 
          || (route_to_peer2_host->dest != route_to_peer1_host->dest))
      {
        /* Peer2 is either with us OR peer1 and peer2 can be reached through
           different gateways */
        struct GNUNET_HashCode hash;
        struct RegisteredHostContext *rhc;
        int skip_focc;

        rhc = GNUNET_malloc (sizeof (struct RegisteredHostContext));
        if (NULL != route_to_peer2_host)
          rhc->reg_host = host_list[route_to_peer2_host->dest];
        else
          rhc->reg_host = host_list[master_context->host_id];
        rhc->host = host_list[route_to_peer1_host->dest];
        GNUNET_assert (NULL != rhc->reg_host);
        GNUNET_assert (NULL != rhc->host);
        rhc->gateway = peer->details.remote.slave;
        rhc->gateway2 = (NULL == route_to_peer2_host) ? NULL :
            slave_list[route_to_peer2_host->dest];
        rhc->state = RHC_INIT;
        GNUNET_SERVER_client_keep (client);
        rhc->client = client;
        hash = hash_hosts (rhc->reg_host, rhc->host);
        skip_focc = GNUNET_NO;
        if ((GNUNET_NO == 
             GNUNET_CONTAINER_multihashmap_contains
             (peer->details.remote.slave->reghost_map, &hash))
            || (GNUNET_SYSERR != GNUNET_CONTAINER_multihashmap_get_multiple
                (peer->details.remote.slave->reghost_map, &hash,
                 reghost_match_iterator, &rhc)))
        {
          /* create and add a new registerd host context */
          /* add the focc to its queue */
          GNUNET_CONTAINER_multihashmap_put
              (peer->details.remote.slave->reghost_map,
               &hash, rhc, GNUNET_CONTAINER_MULTIHASHMAPOPTION_MULTIPLE);
          GNUNET_assert (NULL != host_list[peer2_host_id]);
          queue_host_registration (peer->details.remote.slave,
                                   registeredhost_registration_completion, rhc,
                                   host_list[peer2_host_id]);
        }
        else {
          /* rhc is now set to the existing one from the hash map by
             reghost_match_iterator() */
          /* if queue is empty then ignore creating focc and proceed with
             normal forwarding */	  	  
          if (RHC_OL_CONNECT == rhc->state)
            skip_focc = GNUNET_YES;
        }
        if (GNUNET_NO == skip_focc)
        {
          struct ForwardedOverlayConnectContext *focc;

          focc = GNUNET_malloc (sizeof (struct ForwardedOverlayConnectContext));
          focc->peer1 = p1;
          focc->peer2 = p2;
          focc->peer2_host_id = peer2_host_id;
          focc->orig_msg = GNUNET_copy_message (message);
          focc->operation_id = operation_id;
          GNUNET_CONTAINER_DLL_insert_tail (rhc->focc_dll_head,
                                            rhc->focc_dll_tail,
                                            focc);
          GNUNET_SERVER_receive_done (client, GNUNET_OK);
          return;
        }
      }
    }
    fopc = GNUNET_malloc (sizeof (struct ForwardedOperationContext));
    GNUNET_SERVER_client_keep (client);
    fopc->client = client;
    fopc->operation_id = operation_id;
    fopc->type = OP_OVERLAY_CONNECT;
    fopc->opc = 
	GNUNET_TESTBED_forward_operation_msg_ (peer->details.remote.slave->controller,
					       operation_id, message,
					       &forwarded_operation_reply_relay,
					       fopc);
    fopc->timeout_task =
	GNUNET_SCHEDULER_add_delayed (TIMEOUT, &forwarded_operation_timeout,
				      fopc);
    GNUNET_CONTAINER_DLL_insert_tail (fopcq_head, fopcq_tail, fopc);
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }

  peer2_controller = NULL;
  if ((p2 >= peer_list_size) || (NULL == peer_list[p2]))
  {   
    if ((peer2_host_id >= slave_list_size)
	|| (NULL ==slave_list[peer2_host_id]))
    {
      LOG (GNUNET_ERROR_TYPE_WARNING,
           "Configuration of peer2's controller missing for connecting peers"
           "%u and %u\n", p1, p2);      
      GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
      return;
    }
    peer2_controller = slave_list[peer2_host_id]->controller;
    if (NULL == peer2_controller)
    {
      GNUNET_break (0);       /* What's going on? */
      GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
      return;
    }
  }
  else
  {
    if (GNUNET_YES == peer_list[p2]->is_remote)
      peer2_controller = peer_list[p2]->details.remote.slave->controller;
  }
  occ = GNUNET_malloc (sizeof (struct OverlayConnectContext));
  GNUNET_CONTAINER_DLL_insert_tail (occq_head, occq_tail, occ);
  GNUNET_SERVER_client_keep (client);
  occ->client = client;
  occ->peer_id = p1;
  occ->other_peer_id = p2;
  occ->peer = peer_list[p1];
  occ->op_id = GNUNET_ntohll (msg->operation_id);
  occ->peer2_controller = peer2_controller;
  /* Get the identity of the second peer */
  if (NULL != occ->peer2_controller)
  {
    struct GNUNET_TESTBED_PeerGetConfigurationMessage cmsg;

    cmsg.header.size = 
	htons (sizeof (struct GNUNET_TESTBED_PeerGetConfigurationMessage));
    cmsg.header.type = htons (GNUNET_MESSAGE_TYPE_TESTBED_GETPEERCONFIG);
    cmsg.peer_id = msg->peer2;
    cmsg.operation_id = msg->operation_id;
    occ->opc = 
	GNUNET_TESTBED_forward_operation_msg_ (occ->peer2_controller,
					       occ->op_id, &cmsg.header,
					       &overlay_connect_get_config,
					       occ);
    occ->emsg = 
	GNUNET_strdup ("Timeout while getting peer identity of peer B\n");
    occ->timeout_task =
	GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_relative_multiply
				      (GNUNET_TIME_UNIT_SECONDS, 30),
				      &timeout_overlay_connect, occ);
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  GNUNET_TESTING_peer_get_identity (peer_list[occ->other_peer_id]->details.local.peer,
				    &occ->other_peer_identity);
  /* Connect to the core of 1st peer and wait for the 2nd peer to connect */
  occ->emsg = GNUNET_strdup ("Timeout while connecting to CORE");
  occ->peer->reference_cnt++;
  occ->ch =
      GNUNET_CORE_connect (occ->peer->details.local.cfg, occ, &core_startup_cb,
                           &overlay_connect_notify, NULL, NULL, GNUNET_NO, NULL,
                           GNUNET_NO, no_handlers);
  if (NULL == occ->ch)
    occ->timeout_task = 
	GNUNET_SCHEDULER_add_now (&timeout_overlay_connect, occ);
  else
    occ->timeout_task =
	GNUNET_SCHEDULER_add_delayed (GNUNET_TIME_relative_multiply
				      (GNUNET_TIME_UNIT_SECONDS, 30),
				      &timeout_overlay_connect, occ);
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * Function to cleanup RequestOverlayConnectContext and any associated tasks
 * with it
 *
 * @param rocc the RequestOverlayConnectContext
 */
static void
cleanup_rocc (struct RequestOverlayConnectContext *rocc)
{
  LOG_DEBUG ("Cleaning up rocc\n");
  if (GNUNET_SCHEDULER_NO_TASK != rocc->attempt_connect_task_id)
    GNUNET_SCHEDULER_cancel (rocc->attempt_connect_task_id);
  if (GNUNET_SCHEDULER_NO_TASK != rocc->timeout_rocc_task_id)
    GNUNET_SCHEDULER_cancel (rocc->timeout_rocc_task_id);
  if (NULL != rocc->ohh)
    GNUNET_TRANSPORT_offer_hello_cancel (rocc->ohh);
  if (NULL != rocc->tcc.tch)
    GNUNET_TRANSPORT_try_connect_cancel (rocc->tcc.tch);
  if (GNUNET_SCHEDULER_NO_TASK != rocc->tcc.task)
    GNUNET_SCHEDULER_cancel (rocc->tcc.task);
  GNUNET_TRANSPORT_disconnect (rocc->tcc.th);
  rocc->peer->reference_cnt--;
  if ((GNUNET_YES == rocc->peer->destroy_flag)
      && (0 == rocc->peer->reference_cnt))
    destroy_peer (rocc->peer);
  GNUNET_free_non_null (rocc->hello);
  GNUNET_CONTAINER_DLL_remove (roccq_head, roccq_tail, rocc);
  GNUNET_free (rocc);
}


/**
 * Task to timeout rocc and cleanit up
 *
 * @param cls the RequestOverlayConnectContext
 * @param tc the TaskContext from scheduler
 */
static void
timeout_rocc_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct RequestOverlayConnectContext *rocc = cls;
  
  rocc->timeout_rocc_task_id = GNUNET_SCHEDULER_NO_TASK;
  cleanup_rocc (rocc);
}


/**
 * Function called to notify transport users that another
 * peer connected to us.
 *
 * @param cls closure
 * @param new_peer the peer that connected
 * @param ats performance data
 * @param ats_count number of entries in ats (excluding 0-termination)
 */
static void 
transport_connect_notify (void *cls, const struct GNUNET_PeerIdentity *new_peer,
                          const struct GNUNET_ATS_Information * ats,
                          uint32_t ats_count)
{
  struct RequestOverlayConnectContext *rocc = cls;

  LOG_DEBUG ("Request Overlay connect notify\n");
  if (0 != memcmp (new_peer, &rocc->a_id, sizeof (struct GNUNET_PeerIdentity)))
    return;
  LOG_DEBUG ("Peer %4s connected\n", GNUNET_i2s (&rocc->a_id));
  cleanup_rocc (rocc);
}


/**
 * Task to offer the HELLO message to the peer and ask it to connect to the peer
 * whose identity is in RequestOverlayConnectContext
 *
 * @param cls the RequestOverlayConnectContext
 * @param tc the TaskContext from scheduler
 */
static void
attempt_connect_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc);


/**
 * Task that is run when hello has been sent
 *
 * @param cls the overlay connect context
 * @param tc the scheduler task context; if tc->reason =
 *          GNUNET_SCHEDULER_REASON_TIMEOUT then sending HELLO failed; if
 *          GNUNET_SCHEDULER_REASON_READ_READY is succeeded
 */
static void
rocc_hello_sent_cb (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct RequestOverlayConnectContext *rocc = cls;
  
  rocc->ohh = NULL;
  GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == rocc->attempt_connect_task_id);
  if (GNUNET_SCHEDULER_REASON_TIMEOUT == tc->reason)
  {
    GNUNET_break (0);
    rocc->attempt_connect_task_id =
        GNUNET_SCHEDULER_add_now (&attempt_connect_task,
                                  rocc);
    return;
  }
  if (GNUNET_SCHEDULER_REASON_READ_READY != tc->reason)
    return;
  rocc->tcc.task = GNUNET_SCHEDULER_add_now (&try_connect_task, &rocc->tcc);
}


/**
 * Task to offer the HELLO message to the peer and ask it to connect to the peer
 * whose identity is in RequestOverlayConnectContext
 *
 * @param cls the RequestOverlayConnectContext
 * @param tc the TaskContext from scheduler
 */
static void
attempt_connect_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct RequestOverlayConnectContext *rocc = cls;

  rocc->attempt_connect_task_id = GNUNET_SCHEDULER_NO_TASK;
  rocc->ohh = GNUNET_TRANSPORT_offer_hello (rocc->tcc.th, rocc->hello,
                                            rocc_hello_sent_cb, rocc);
  if (NULL == rocc->ohh)
    rocc->attempt_connect_task_id = 
        GNUNET_SCHEDULER_add_delayed 
        (GNUNET_TIME_relative_multiply (GNUNET_TIME_UNIT_MILLISECONDS,
                                        100 + GNUNET_CRYPTO_random_u32
                                        (GNUNET_CRYPTO_QUALITY_WEAK, 500)),
         &attempt_connect_task, rocc);
}


/**
 * Handler for GNUNET_MESSAGE_TYPE_TESTBED_REQUESTCONNECT messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_overlay_request_connect (void *cls, struct GNUNET_SERVER_Client *client,
				const struct GNUNET_MessageHeader *message)
{
  const struct GNUNET_TESTBED_RequestConnectMessage *msg;
  struct RequestOverlayConnectContext *rocc;
  struct Peer *peer;
  uint32_t peer_id;
  uint16_t msize;
  uint16_t hsize;
  
  msize = ntohs (message->size);
  if (sizeof (struct GNUNET_TESTBED_RequestConnectMessage) >= msize)
  {
    GNUNET_break (0);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }  
  msg = (const struct GNUNET_TESTBED_RequestConnectMessage *) message;
  if ((NULL == msg->hello) || 
      (GNUNET_MESSAGE_TYPE_HELLO != ntohs (msg->hello->type)))
  {
    GNUNET_break (0);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  hsize = ntohs (msg->hello->size);
  if ((sizeof (struct GNUNET_TESTBED_RequestConnectMessage) + hsize) != msize)
  {
    GNUNET_break (0);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  peer_id = ntohl (msg->peer);
  if ((peer_id >= peer_list_size) || (NULL == (peer = peer_list[peer_id])))
  {
    GNUNET_break_op (0);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  if (GNUNET_YES == peer->is_remote)
  {
    struct GNUNET_MessageHeader *msg2;
    
    msg2 = GNUNET_copy_message (message);
    GNUNET_TESTBED_queue_message_ (peer->details.remote.slave->controller, msg2);
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  rocc = GNUNET_malloc (sizeof (struct RequestOverlayConnectContext));
  GNUNET_CONTAINER_DLL_insert_tail (roccq_head, roccq_tail, rocc);
  rocc->peer = peer;
  rocc->peer->reference_cnt++;
  rocc->tcc.th = GNUNET_TRANSPORT_connect (rocc->peer->details.local.cfg, NULL, rocc,
                                           NULL, &transport_connect_notify, NULL);
  if (NULL == rocc->tcc.th)
  {
    GNUNET_break (0);
    GNUNET_free (rocc);
    GNUNET_SERVER_receive_done (client, GNUNET_SYSERR);
    return;
  }
  memcpy (&rocc->a_id, &msg->peer_identity,
          sizeof (struct GNUNET_PeerIdentity));
  rocc->tcc.pid = &rocc->a_id;
  rocc->hello = GNUNET_malloc (hsize);
  memcpy (rocc->hello, msg->hello, hsize);
  rocc->attempt_connect_task_id =
      GNUNET_SCHEDULER_add_now (&attempt_connect_task, rocc);
  rocc->timeout_rocc_task_id =
      GNUNET_SCHEDULER_add_delayed (TIMEOUT, &timeout_rocc_task, rocc);
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * Handler for GNUNET_MESSAGE_TYPE_TESTBED_GETSLAVECONFIG messages
 *
 * @param cls NULL
 * @param client identification of the client
 * @param message the actual message
 */
static void
handle_slave_get_config (void *cls, struct GNUNET_SERVER_Client *client,
			 const struct GNUNET_MessageHeader *message)
{
  struct GNUNET_TESTBED_SlaveGetConfigurationMessage *msg;
  struct Slave *slave;  
  struct GNUNET_TESTBED_SlaveConfiguration *reply;
  char *config;
  char *xconfig;
  size_t config_size;
  size_t xconfig_size;
  size_t reply_size;
  uint64_t op_id;
  uint32_t slave_id;

  msg = (struct GNUNET_TESTBED_SlaveGetConfigurationMessage *) message;
  slave_id = ntohl (msg->slave_id);
  op_id = GNUNET_ntohll (msg->operation_id);
  if ((slave_list_size <= slave_id) || (NULL == slave_list[slave_id]))
  {
    /* FIXME: Add forwardings for this type of message here.. */
    send_operation_fail_msg (client, op_id, "Slave not found");
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  slave = slave_list[slave_id];
  if (NULL == slave->cfg)
  {
    send_operation_fail_msg (client, op_id,
			     "Configuration not found (slave not started by me)");
    GNUNET_SERVER_receive_done (client, GNUNET_OK);
    return;
  }
  config = GNUNET_CONFIGURATION_serialize (slave->cfg, &config_size);
  xconfig_size = GNUNET_TESTBED_compress_config_ (config, config_size, 
						  &xconfig);
  GNUNET_free (config);  
  reply_size = xconfig_size + sizeof (struct GNUNET_TESTBED_SlaveConfiguration);
  GNUNET_break (reply_size <= UINT16_MAX);
  GNUNET_break (config_size <= UINT16_MAX);
  reply = GNUNET_realloc (xconfig, reply_size);
  (void) memmove (&reply[1], reply, xconfig_size);
  reply->header.type = htons (GNUNET_MESSAGE_TYPE_TESTBED_SLAVECONFIG);
  reply->header.size = htons ((uint16_t) reply_size);
  reply->slave_id = msg->slave_id;
  reply->operation_id = msg->operation_id;
  reply->config_size = htons ((uint16_t) config_size);
  queue_message (client, &reply->header);
  GNUNET_SERVER_receive_done (client, GNUNET_OK);
}


/**
 * Iterator over hash map entries.
 *
 * @param cls closure
 * @param key current key code
 * @param value value in the hash map
 * @return GNUNET_YES if we should continue to
 *         iterate,
 *         GNUNET_NO if not.
 */
static int
ss_map_free_iterator (void *cls, const struct GNUNET_HashCode *key, void *value)
{
  struct SharedService *ss = value;

  GNUNET_assert (GNUNET_YES ==
                 GNUNET_CONTAINER_multihashmap_remove (ss_map, key, value));
  GNUNET_free (ss->name);
  GNUNET_free (ss);
  return GNUNET_YES;
}


/**
 * Iterator for freeing hash map entries in a slave's reghost_map
 *
 * @param cls handle to the slave
 * @param key current key code
 * @param value value in the hash map
 * @return GNUNET_YES if we should continue to
 *         iterate,
 *         GNUNET_NO if not.
 */
static int 
reghost_free_iterator (void *cls,
                       const struct GNUNET_HashCode * key,
                       void *value)
{
  struct Slave *slave = cls;
  struct RegisteredHostContext *rhc = value;
  struct ForwardedOverlayConnectContext *focc;

  GNUNET_assert (GNUNET_YES ==
                 GNUNET_CONTAINER_multihashmap_remove (slave->reghost_map,
                                                       key, value));
  while (NULL != (focc = rhc->focc_dll_head))
  {
    GNUNET_CONTAINER_DLL_remove (rhc->focc_dll_head,
                                 rhc->focc_dll_tail,
                                 focc);
    cleanup_focc (focc);
  }
  if (NULL != rhc->sub_op)
    GNUNET_TESTBED_operation_done (rhc->sub_op);
  if (NULL != rhc->client)
  GNUNET_SERVER_client_drop (rhc->client);
  GNUNET_free (value);
  return GNUNET_YES;
}


/**
 * Task to clean up and shutdown nicely
 *
 * @param cls NULL
 * @param tc the TaskContext from scheduler
 */
static void
shutdown_task (void *cls, const struct GNUNET_SCHEDULER_TaskContext *tc)
{
  struct LCFContextQueue *lcfq;
  struct OverlayConnectContext *occ;
  struct RequestOverlayConnectContext *rocc;
  struct ForwardedOperationContext *fopc;
  struct MessageQueue *mq_entry;
  uint32_t id;

  shutdown_task_id = GNUNET_SCHEDULER_NO_TASK;
  LOG (GNUNET_ERROR_TYPE_DEBUG, "Shutting down testbed service\n");
  (void) GNUNET_CONTAINER_multihashmap_iterate (ss_map, &ss_map_free_iterator,
                                                NULL);
  GNUNET_CONTAINER_multihashmap_destroy (ss_map);
  /* cleanup any remaining forwarded operations */
  while (NULL != (fopc = fopcq_head))
  {
    GNUNET_CONTAINER_DLL_remove (fopcq_head, fopcq_tail, fopc);
    GNUNET_TESTBED_forward_operation_msg_cancel_ (fopc->opc);
    if (GNUNET_SCHEDULER_NO_TASK != fopc->timeout_task)
      GNUNET_SCHEDULER_cancel (fopc->timeout_task);
    GNUNET_SERVER_client_drop (fopc->client);
    switch (fopc->type)
    {
    case OP_PEER_CREATE:
      GNUNET_free (fopc->cls);
      break;
    case OP_PEER_START:
    case OP_PEER_STOP:
    case OP_PEER_DESTROY:
    case OP_PEER_INFO:
    case OP_OVERLAY_CONNECT:
    case OP_LINK_CONTROLLERS:
    case OP_GET_SLAVE_CONFIG:
      break;
    case OP_FORWARDED:
      GNUNET_assert (0);
    };
    GNUNET_free (fopc);
  }
  if (NULL != lcfq_head)
  {
    if (GNUNET_SCHEDULER_NO_TASK != lcf_proc_task_id)
    {
      GNUNET_SCHEDULER_cancel (lcf_proc_task_id);
      lcf_proc_task_id = GNUNET_SCHEDULER_NO_TASK;
    }
  }
  GNUNET_assert (GNUNET_SCHEDULER_NO_TASK == lcf_proc_task_id);
  for (lcfq = lcfq_head; NULL != lcfq; lcfq = lcfq_head)
  {
    GNUNET_free (lcfq->lcf->msg);
    GNUNET_free (lcfq->lcf);
    GNUNET_CONTAINER_DLL_remove (lcfq_head, lcfq_tail, lcfq);
    GNUNET_free (lcfq);
  }
  while (NULL != (occ = occq_head))
    cleanup_occ (occ);
  while (NULL != (rocc = roccq_head))
    cleanup_rocc (rocc);
  /* Clear peer list */
  for (id = 0; id < peer_list_size; id++)
    if (NULL != peer_list[id])
    {
      /* If destroy flag is set it means that this peer should have been
         destroyed by a context which we destroy before */
      GNUNET_break (GNUNET_NO == peer_list[id]->destroy_flag);
      /* counter should be zero as we free all contexts before */
      GNUNET_break (0 == peer_list[id]->reference_cnt);      
      if ( (GNUNET_NO == peer_list[id]->is_remote)
           && (GNUNET_YES == peer_list[id]->details.local.is_running))
        GNUNET_TESTING_peer_kill (peer_list[id]->details.local.peer);
    }
  for (id = 0; id < peer_list_size; id++)
    if (NULL != peer_list[id])
    {
      if (GNUNET_NO == peer_list[id]->is_remote)
      {
	if (GNUNET_YES == peer_list[id]->details.local.is_running)
	  GNUNET_TESTING_peer_wait (peer_list[id]->details.local.peer);
        GNUNET_TESTING_peer_destroy (peer_list[id]->details.local.peer);
        GNUNET_CONFIGURATION_destroy (peer_list[id]->details.local.cfg);
      }
      GNUNET_free (peer_list[id]);
    }
  GNUNET_free_non_null (peer_list);
  /* Clear host list */
  for (id = 0; id < host_list_size; id++)
    if (NULL != host_list[id])
      GNUNET_TESTBED_host_destroy (host_list[id]);
  GNUNET_free_non_null (host_list);
  /* Clear route list */
  for (id = 0; id < route_list_size; id++)
    if (NULL != route_list[id])
      GNUNET_free (route_list[id]);
  GNUNET_free_non_null (route_list);
  /* Clear slave_list */
  for (id = 0; id < slave_list_size; id++)
    if (NULL != slave_list[id])
    {
      struct HostRegistration *hr_entry;
      
      while (NULL != (hr_entry = slave_list[id]->hr_dll_head))
      {
        GNUNET_CONTAINER_DLL_remove (slave_list[id]->hr_dll_head,
                                     slave_list[id]->hr_dll_tail,
                                     hr_entry);
        GNUNET_free (hr_entry);
      }
      if (NULL != slave_list[id]->rhandle)
        GNUNET_TESTBED_cancel_registration (slave_list[id]->rhandle);
      (void) GNUNET_CONTAINER_multihashmap_iterate (slave_list[id]->reghost_map,
                                                    reghost_free_iterator,
                                                    slave_list[id]);
      GNUNET_CONTAINER_multihashmap_destroy (slave_list[id]->reghost_map);
      if (NULL != slave_list[id]->cfg)
	GNUNET_CONFIGURATION_destroy (slave_list[id]->cfg);
      if (NULL != slave_list[id]->controller)
        GNUNET_TESTBED_controller_disconnect (slave_list[id]->controller);
      if (NULL != slave_list[id]->controller_proc)
        GNUNET_TESTBED_controller_stop (slave_list[id]->controller_proc);
      GNUNET_free (slave_list[id]);
    }
  GNUNET_free_non_null (slave_list);
  if (NULL != master_context)
  {
    GNUNET_free_non_null (master_context->master_ip);
    if (NULL != master_context->system)
      GNUNET_TESTING_system_destroy (master_context->system, GNUNET_YES);
    GNUNET_SERVER_client_drop (master_context->client);
    GNUNET_free (master_context);
    master_context = NULL;
  }
  if (NULL != transmit_handle)
    GNUNET_SERVER_notify_transmit_ready_cancel (transmit_handle);  
  while (NULL != (mq_entry = mq_head))
  {
    GNUNET_free (mq_entry->msg);
    GNUNET_SERVER_client_drop (mq_entry->client);
    GNUNET_CONTAINER_DLL_remove (mq_head, mq_tail, mq_entry);    
    GNUNET_free (mq_entry);
  }
  GNUNET_free_non_null (hostname);
  GNUNET_CONFIGURATION_destroy (our_config);
}


/**
 * Callback for client disconnect
 *
 * @param cls NULL
 * @param client the client which has disconnected
 */
static void
client_disconnect_cb (void *cls, struct GNUNET_SERVER_Client *client)
{
  if (NULL == master_context)
    return;
  if (client == master_context->client)
  {
    LOG (GNUNET_ERROR_TYPE_DEBUG, "Master client disconnected\n");
    /* should not be needed as we're terminated by failure to read
     * from stdin, but if stdin fails for some reason, this shouldn't
     * hurt for now --- might need to revise this later if we ever
     * decide that master connections might be temporarily down
     * for some reason */
    //GNUNET_SCHEDULER_shutdown ();
  }
}


/**
 * Testbed setup
 *
 * @param cls closure
 * @param server the initialized server
 * @param cfg configuration to use
 */
static void
testbed_run (void *cls, struct GNUNET_SERVER_Handle *server,
             const struct GNUNET_CONFIGURATION_Handle *cfg)
{
  static const struct GNUNET_SERVER_MessageHandler message_handlers[] = {
    {&handle_init, NULL, GNUNET_MESSAGE_TYPE_TESTBED_INIT, 0},
    {&handle_add_host, NULL, GNUNET_MESSAGE_TYPE_TESTBED_ADDHOST, 0},
    {&handle_configure_shared_service, NULL,
     GNUNET_MESSAGE_TYPE_TESTBED_SERVICESHARE, 0},
    {&handle_link_controllers, NULL,
     GNUNET_MESSAGE_TYPE_TESTBED_LCONTROLLERS, 0},
    {&handle_peer_create, NULL, GNUNET_MESSAGE_TYPE_TESTBED_CREATEPEER, 0},
    {&handle_peer_destroy, NULL, GNUNET_MESSAGE_TYPE_TESTBED_DESTROYPEER,
     sizeof (struct GNUNET_TESTBED_PeerDestroyMessage)},
    {&handle_peer_start, NULL, GNUNET_MESSAGE_TYPE_TESTBED_STARTPEER,
     sizeof (struct GNUNET_TESTBED_PeerStartMessage)},
    {&handle_peer_stop, NULL, GNUNET_MESSAGE_TYPE_TESTBED_STOPPEER,
     sizeof (struct GNUNET_TESTBED_PeerStopMessage)},
    {&handle_peer_get_config, NULL, GNUNET_MESSAGE_TYPE_TESTBED_GETPEERCONFIG,
     sizeof (struct GNUNET_TESTBED_PeerGetConfigurationMessage)},
    {&handle_overlay_connect, NULL, GNUNET_MESSAGE_TYPE_TESTBED_OLCONNECT,
     sizeof (struct GNUNET_TESTBED_OverlayConnectMessage)},
    {&handle_overlay_request_connect, NULL, GNUNET_MESSAGE_TYPE_TESTBED_REQUESTCONNECT,
     0},
    {handle_slave_get_config, NULL, GNUNET_MESSAGE_TYPE_TESTBED_GETSLAVECONFIG,
     sizeof (struct GNUNET_TESTBED_SlaveGetConfigurationMessage)},
    {NULL}
  };

  GNUNET_assert (GNUNET_OK == GNUNET_CONFIGURATION_get_value_string 
		 (cfg, "testbed", "HOSTNAME", &hostname));
  our_config = GNUNET_CONFIGURATION_dup (cfg);
  GNUNET_SERVER_add_handlers (server, message_handlers);
  GNUNET_SERVER_disconnect_notify (server, &client_disconnect_cb, NULL);
  ss_map = GNUNET_CONTAINER_multihashmap_create (5, GNUNET_NO);
  shutdown_task_id =
      GNUNET_SCHEDULER_add_delayed_with_priority (GNUNET_TIME_UNIT_FOREVER_REL,
                                                  GNUNET_SCHEDULER_PRIORITY_IDLE,
                                                  &shutdown_task, NULL);
  LOG_DEBUG ("Testbed startup complete\n");
  event_mask = 1LL << GNUNET_TESTBED_ET_OPERATION_FINISHED;
}


/**
 * The starting point of execution
 */
int
main (int argc, char *const *argv)
{
  //sleep (15);                 /* Debugging */
  return (GNUNET_OK ==
          GNUNET_SERVICE_run (argc, argv, "testbed", GNUNET_SERVICE_OPTION_NONE,
                              &testbed_run, NULL)) ? 0 : 1;
}

/* end of gnunet-service-testbed.c */
