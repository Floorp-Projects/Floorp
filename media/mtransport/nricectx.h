/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

// Some of this code is cut-and-pasted from nICEr. Copyright is:

/*
Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

// This is a wrapper around the nICEr ICE stack
#ifndef nricectx_h__
#define nricectx_h__

#include <memory>
#include <string>
#include <vector>
#include <map>

#include "sigslot.h"

#include "prnetdb.h"

#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoPtr.h"
#include "nsIEventTarget.h"
#include "nsITimer.h"
#include "nsTArray.h"

#include "m_cpp_utils.h"
#include "nricestunaddr.h"
#include "nricemediastream.h"

typedef struct nr_ice_ctx_ nr_ice_ctx;
typedef struct nr_ice_peer_ctx_ nr_ice_peer_ctx;
typedef struct nr_ice_media_stream_ nr_ice_media_stream;
typedef struct nr_ice_handler_ nr_ice_handler;
typedef struct nr_ice_handler_vtbl_ nr_ice_handler_vtbl;
typedef struct nr_ice_candidate_ nr_ice_candidate;
typedef struct nr_ice_cand_pair_ nr_ice_cand_pair;
typedef struct nr_ice_stun_server_ nr_ice_stun_server;
typedef struct nr_ice_turn_server_ nr_ice_turn_server;
typedef struct nr_resolver_ nr_resolver;
typedef struct nr_proxy_tunnel_config_ nr_proxy_tunnel_config;

typedef void* NR_SOCKET;

namespace mozilla {

class NrSocketProxyConfig;

// Timestamps set whenever a packet is dropped due to global rate limiting
// (see nr_socket_prsock.cpp)
TimeStamp nr_socket_short_term_violation_time();
TimeStamp nr_socket_long_term_violation_time();

class NrIceMediaStream;

extern const char kNrIceTransportUdp[];
extern const char kNrIceTransportTcp[];
extern const char kNrIceTransportTls[];

class NrIceStunServer {
 public:
  explicit NrIceStunServer(const PRNetAddr& addr) : has_addr_(true) {
    memcpy(&addr_, &addr, sizeof(addr));
  }

  // The main function to use. Will take either an address or a hostname.
  static UniquePtr<NrIceStunServer> Create(
      const std::string& addr, uint16_t port,
      const char* transport = kNrIceTransportUdp) {
    UniquePtr<NrIceStunServer> server(new NrIceStunServer(transport));

    nsresult rv = server->Init(addr, port);
    if (NS_FAILED(rv)) return nullptr;

    return server;
  }

  nsresult ToNicerStunStruct(nr_ice_stun_server* server) const;

 protected:
  explicit NrIceStunServer(const char* transport)
      : addr_(), transport_(transport) {}

  nsresult Init(const std::string& addr, uint16_t port) {
    PRStatus status = PR_StringToNetAddr(addr.c_str(), &addr_);
    if (status == PR_SUCCESS) {
      // Parseable as an address
      addr_.inet.port = PR_htons(port);
      port_ = port;
      has_addr_ = true;
      return NS_OK;
    } else if (addr.size() < 256) {
      // Apparently this is a hostname.
      host_ = addr;
      port_ = port;
      has_addr_ = false;
      return NS_OK;
    }

    return NS_ERROR_FAILURE;
  }

  bool has_addr_;
  std::string host_;
  uint16_t port_;
  PRNetAddr addr_;
  std::string transport_;
};

class NrIceTurnServer : public NrIceStunServer {
 public:
  static UniquePtr<NrIceTurnServer> Create(
      const std::string& addr, uint16_t port, const std::string& username,
      const std::vector<unsigned char>& password,
      const char* transport = kNrIceTransportUdp) {
    UniquePtr<NrIceTurnServer> server(
        new NrIceTurnServer(username, password, transport));

    nsresult rv = server->Init(addr, port);
    if (NS_FAILED(rv)) return nullptr;

    return server;
  }

  nsresult ToNicerTurnStruct(nr_ice_turn_server* server) const;

 private:
  NrIceTurnServer(const std::string& username,
                  const std::vector<unsigned char>& password,
                  const char* transport)
      : NrIceStunServer(transport), username_(username), password_(password) {}

  std::string username_;
  std::vector<unsigned char> password_;
};

class TestNat;

class NrIceStats {
 public:
  uint16_t stun_retransmits = 0;
  uint16_t turn_401s = 0;
  uint16_t turn_403s = 0;
  uint16_t turn_438s = 0;
};

class NrIceCtx {
 public:
  enum ConnectionState {
    ICE_CTX_INIT,
    ICE_CTX_CHECKING,
    ICE_CTX_CONNECTED,
    ICE_CTX_COMPLETED,
    ICE_CTX_FAILED,
    ICE_CTX_DISCONNECTED,
    ICE_CTX_CLOSED
  };

  enum GatheringState {
    ICE_CTX_GATHER_INIT,
    ICE_CTX_GATHER_STARTED,
    ICE_CTX_GATHER_COMPLETE
  };

  enum Controlling { ICE_CONTROLLING, ICE_CONTROLLED };

  enum Policy { ICE_POLICY_RELAY, ICE_POLICY_NO_HOST, ICE_POLICY_ALL };

  static RefPtr<NrIceCtx> Create(
      const std::string& name, bool allow_loopback = false,
      bool tcp_enabled = true, bool allow_link_local = false,
      NrIceCtx::Policy policy = NrIceCtx::ICE_POLICY_ALL);

  RefPtr<NrIceMediaStream> CreateStream(const std::string& id,
                                        const std::string& name,
                                        int components);
  void DestroyStream(const std::string& id);

  // initialize ICE globals, crypto, and logging
  static void InitializeGlobals(bool allow_loopback = false,
                                bool tcp_enabled = true,
                                bool allow_link_local = false);

  // static GetStunAddrs for use in parent process to support
  // sandboxing restrictions
  static nsTArray<NrIceStunAddr> GetStunAddrs();
  void SetStunAddrs(const nsTArray<NrIceStunAddr>& addrs);

  bool Initialize();

  int SetNat(const RefPtr<TestNat>& aNat);

  // Deinitialize all ICE global state. Used only for testing.
  static void internal_DeinitializeGlobal();

  // Divide some timers to faster testing. Used only for testing.
  void internal_SetTimerAccelarator(int divider);

  nr_ice_ctx* ctx() { return ctx_; }
  nr_ice_peer_ctx* peer() { return peer_; }

  // Testing only.
  void destroy_peer_ctx();

  RefPtr<NrIceMediaStream> GetStream(const std::string& id) {
    auto it = streams_.find(id);
    if (it != streams_.end()) {
      return it->second;
    }
    return nullptr;
  }

  std::vector<RefPtr<NrIceMediaStream>> GetStreams() const {
    std::vector<RefPtr<NrIceMediaStream>> result;
    for (auto& idAndStream : streams_) {
      result.push_back(idAndStream.second);
    }
    return result;
  }

  bool HasStreamsToConnect() const;

  // The name of the ctx
  const std::string& name() const { return name_; }

  // Current state
  ConnectionState connection_state() const { return connection_state_; }

  // Current state
  GatheringState gathering_state() const { return gathering_state_; }

  // Get the global attributes
  std::vector<std::string> GetGlobalAttributes();

  // Set the other side's global attributes
  nsresult ParseGlobalAttributes(std::vector<std::string> attrs);

  // Set whether we are controlling or not.
  nsresult SetControlling(Controlling controlling);

  Controlling GetControlling();

  // Set whether we're allowed to produce none, relay or all candidates.
  // TODO(jib@mozilla.com): Work out what change means mid-connection (1181768)
  nsresult SetPolicy(Policy policy);

  Policy policy() const { return policy_; }

  // Set the STUN servers. Must be called before StartGathering
  // (if at all).
  nsresult SetStunServers(const std::vector<NrIceStunServer>& stun_servers);

  // Set the TURN servers. Must be called before StartGathering
  // (if at all).
  nsresult SetTurnServers(const std::vector<NrIceTurnServer>& turn_servers);

  // Provide the resolution provider. Must be called before
  // StartGathering.
  nsresult SetResolver(nr_resolver* resolver);

  // Provide the proxy address. Must be called before
  // StartGathering.
  nsresult SetProxyServer(NrSocketProxyConfig&& config);

  const std::shared_ptr<NrSocketProxyConfig>& GetProxyConfig() {
    return proxy_config_;
  }

  void SetCtxFlags(bool default_route_only, bool proxy_only);

  // Start ICE gathering
  nsresult StartGathering(bool default_route_only, bool proxy_only);

  // Start checking
  nsresult StartChecks(bool offerer);

  // Notify that the network has gone online/offline
  void UpdateNetworkState(bool online);

  // Finalize the ICE negotiation. I.e., there will be no
  // more forking.
  nsresult Finalize();

  void AccumulateStats(const NrIceStats& stats);
  NrIceStats Destroy();

  // Are we trickling?
  bool generating_trickle() const { return trickle_; }

  // Signals to indicate events. API users can (and should)
  // register for these.
  sigslot::signal2<NrIceCtx*, NrIceCtx::GatheringState>
      SignalGatheringStateChange;
  sigslot::signal2<NrIceCtx*, NrIceCtx::ConnectionState>
      SignalConnectionStateChange;

  // The thread to direct method calls to
  nsCOMPtr<nsIEventTarget> thread() { return sts_target_; }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NrIceCtx)

 private:
  NrIceCtx(const std::string& name, Policy policy);

  virtual ~NrIceCtx();

  DISALLOW_COPY_ASSIGN(NrIceCtx);

  // Callbacks for nICEr
  static void gather_cb(NR_SOCKET s, int h, void* arg);  // ICE gather complete

  // Handler implementation
  static int select_pair(void* obj, nr_ice_media_stream* stream,
                         int component_id, nr_ice_cand_pair** potentials,
                         int potential_ct);
  static int stream_ready(void* obj, nr_ice_media_stream* stream);
  static int stream_failed(void* obj, nr_ice_media_stream* stream);
  static int ice_checking(void* obj, nr_ice_peer_ctx* pctx);
  static int ice_connected(void* obj, nr_ice_peer_ctx* pctx);
  static int ice_disconnected(void* obj, nr_ice_peer_ctx* pctx);
  static int msg_recvd(void* obj, nr_ice_peer_ctx* pctx,
                       nr_ice_media_stream* stream, int component_id,
                       unsigned char* msg, int len);
  static void trickle_cb(void* arg, nr_ice_ctx* ctx,
                         nr_ice_media_stream* stream, int component_id,
                         nr_ice_candidate* candidate);

  // Find a media stream by stream ptr. Gross
  RefPtr<NrIceMediaStream> FindStream(nr_ice_media_stream* stream);

  // Set the state
  void SetConnectionState(ConnectionState state);

  // Set the state
  void SetGatheringState(GatheringState state);

  ConnectionState connection_state_;
  GatheringState gathering_state_;
  const std::string name_;
  bool offerer_;
  TimeStamp ice_start_time_;
  bool ice_controlling_set_;
  std::map<std::string, RefPtr<NrIceMediaStream>> streams_;
  nr_ice_ctx* ctx_;
  nr_ice_peer_ctx* peer_;
  nr_ice_handler_vtbl* ice_handler_vtbl_;  // Must be pointer
  nr_ice_handler* ice_handler_;            // Must be pointer
  bool trickle_;
  nsCOMPtr<nsIEventTarget> sts_target_;  // The thread to run on
  Policy policy_;
  RefPtr<TestNat> nat_;
  std::shared_ptr<NrSocketProxyConfig> proxy_config_;
};

}  // namespace mozilla
#endif
