/*
      This file is part of GNUnet
      (C) 2008--2012 Christian Grothoff (and other contributing authors)

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
 * @file testbed/testbed_api_peers.h
 * @brief internal API to access the 'peers' subsystem
 * @author Christian Grothoff
 * @author Sree Harsha Totakura
 */

#ifndef NEW_TESTING_API_PEERS_H
#define NEW_TESTING_API_PEERS_H

#include "gnunet_testbed_service.h"
#include "gnunet_helper_lib.h"


/**
 * Enumeration of possible states a peer could be in
 */
enum PeerState
{
    /**
     * State to signify that this peer is invalid
     */
  PS_INVALID,

    /**
     * The peer has been created
     */
  PS_CREATED,

    /**
     * The peer is running
     */
  PS_STARTED,

    /**
     * The peer is stopped
     */
  PS_STOPPED,
};


/**
 * A peer controlled by the testing framework.  A peer runs
 * at a particular host.
 */
struct GNUNET_TESTBED_Peer
{
  /**
   * Our controller context (not necessarily the controller
   * that is responsible for starting/running the peer!).
   */
  struct GNUNET_TESTBED_Controller *controller;

  /**
   * Which host does this peer run on?
   */
  struct GNUNET_TESTBED_Host *host;

  /**
   * Globally unique ID of the peer.
   */
  uint32_t unique_id;

  /**
   * Peer's state
   */
  enum PeerState state;
};


/**
 * Data for the OperationType OP_PEER_CREATE
 */
struct PeerCreateData
{
  /**
   * The host where the peer has to be created
   */
  struct GNUNET_TESTBED_Host *host;

  /**
   * The template configuration of the peer
   */
  const struct GNUNET_CONFIGURATION_Handle *cfg;

  /**
   * The call back to call when we receive peer create success message
   */
  GNUNET_TESTBED_PeerCreateCallback cb;

  /**
   * The closure for the above callback
   */
  void *cls;

  /**
   * The peer structure to return when we get success message
   */
  struct GNUNET_TESTBED_Peer *peer;

};


/**
 * Data for OperationType OP_PEER_START and OP_PEER_STOP
 */
struct PeerEventData
{
  /**
   * The handle of the peer to start
   */
  struct GNUNET_TESTBED_Peer *peer;
  
  /**
   * The Peer churn callback to call when this operation is completed
   */
  GNUNET_TESTBED_PeerChurnCallback pcc;
   
  /**
   * Closure for the above callback
   */
  void *pcc_cls;
    
};


/**
 * Data for the OperationType OP_PEER_DESTROY;
 */
struct PeerDestroyData
{
  /**
   * The peer structure
   */
  struct GNUNET_TESTBED_Peer *peer;

  //PEERDESTROYDATA
};


/**
 * Data for the OperationType OP_PEER_INFO
 */
struct PeerInfoData
{
  /**
   * The peer whose information has been requested
   */
  struct GNUNET_TESTBED_Peer *peer;

  /**
   * The Peer info callback to call when this operation has completed
   */
  GNUNET_TESTBED_PeerInfoCallback cb;
    
  /**
   * The closure for peer info callback
   */
  void *cb_cls;

  /**
   * The type of peer information requested
   */
  enum GNUNET_TESTBED_PeerInformationType pit;
};


/**
 * Data structure for OperationType OP_OVERLAY_CONNECT
 */
struct OverlayConnectData
{

  /**
   * Peer A to connect to peer B
   */
  struct GNUNET_TESTBED_Peer *p1;

  /**
   * Peer B
   */
  struct GNUNET_TESTBED_Peer *p2;

  /**
   * The operation completion callback to call once this operation is done
   */
  GNUNET_TESTBED_OperationCompletionCallback cb;
  
  /**
   * The closure for the above callback
   */
  void *cb_cls;

  /**
   * OperationContext for forwarded operations generated when peer1's controller doesn't have the
   * configuration of peer2's controller for linking laterally to attemp an
   * overlay connection between peer 1 and peer 2.
   */
  struct OperationContext *sub_opc;

  /**
   * State information for this context data
   */
  enum OCDState {
    
    /**
     * The initial state
     */
    OCD_INIT,

    /**
     * State where we attempt to acquire peer2's controller's configuration
     */
    OCD_CFG_ACQUIRE,

    /**
     * State where we link peer1's controller to peer2's controller
     */
    OCD_LINK_CONTROLLERS,
    
    /**
     * State where we re-ask controller of peer1 to attempt an overlay connect
     * between peer1 and peer2
     */
    OCD_REATTEMPT_OVERLAY_CONNECT
  } state;

};



/**
 * Create the given peer at the specified host using the given
 * controller.  If the given controller is not running on the target
 * host, it should find or create a controller at the target host and
 * delegate creating the peer.  Explicit delegation paths can be setup
 * using 'GNUNET_TESTBED_controller_link'.  If no explicit delegation
 * path exists, a direct link with a subordinate controller is setup
 * for the first delegated peer to a particular host; the subordinate
 * controller is then destroyed once the last peer that was delegated
 * to the remote host is stopped.  This function is used in particular
 * if some other controller has already assigned a unique ID to the
 * peer.
 *
 * Creating the peer only creates the handle to manipulate and further
 * configure the peer; use "GNUNET_TESTBED_peer_start" and
 * "GNUNET_TESTBED_peer_stop" to actually start/stop the peer's
 * processes.
 *
 * Note that the given configuration will be adjusted by the
 * controller to avoid port/path conflicts with other peers.
 * The "final" configuration can be obtained using
 * 'GNUNET_TESTBED_peer_get_information'.
 *
 * @param unique_id unique ID for this peer
 * @param controller controller process to use
 * @param host host to run the peer on
 * @param cfg configuration to use for the peer
 * @param cb the callback to call when the peer has been created
 * @param cls the closure to the above callback
 * @return the operation handle
 */
struct GNUNET_TESTBED_Operation *
GNUNET_TESTBED_peer_create_with_id_ (uint32_t unique_id,
                                     struct GNUNET_TESTBED_Controller
                                     *controller,
                                     struct GNUNET_TESTBED_Host *host,
                                     const struct GNUNET_CONFIGURATION_Handle
                                     *cfg, GNUNET_TESTBED_PeerCreateCallback cb,
                                     void *cls);


/**
 * Generate PeerGetConfigurationMessage
 *
 * @param peer_id the id of the peer whose information we have to get
 * @param operation_id the ip of the operation that should be represented in
 *          the message
 * @return the PeerGetConfigurationMessage
 */
struct GNUNET_TESTBED_PeerGetConfigurationMessage *
GNUNET_TESTBED_generate_peergetconfig_msg_ (uint32_t peer_id,
                                            uint64_t operation_id);

#endif
/* end of testbed_api_peers.h */
