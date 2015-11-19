/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_NATSERVER_H_
#define WEBRTC_BASE_NATSERVER_H_

#include <map>
#include <set>

#include "webrtc/base/asyncudpsocket.h"
#include "webrtc/base/socketaddresspair.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/socketfactory.h"
#include "webrtc/base/nattypes.h"

namespace rtc {

// Change how routes (socketaddress pairs) are compared based on the type of
// NAT.  The NAT server maintains a hashtable of the routes that it knows
// about.  So these affect which routes are treated the same.
struct RouteCmp {
  explicit RouteCmp(NAT* nat);
  size_t operator()(const SocketAddressPair& r) const;
  bool operator()(
      const SocketAddressPair& r1, const SocketAddressPair& r2) const;

  bool symmetric;
};

// Changes how addresses are compared based on the filtering rules of the NAT.
struct AddrCmp {
  explicit AddrCmp(NAT* nat);
  size_t operator()(const SocketAddress& r) const;
  bool operator()(const SocketAddress& r1, const SocketAddress& r2) const;

  bool use_ip;
  bool use_port;
};

// Implements the NAT device.  It listens for packets on the internal network,
// translates them, and sends them out over the external network.

const int NAT_SERVER_PORT = 4237;

class NATServer : public sigslot::has_slots<> {
 public:
  NATServer(
      NATType type, SocketFactory* internal, const SocketAddress& internal_addr,
      SocketFactory* external, const SocketAddress& external_ip);
  ~NATServer() override;

  SocketAddress internal_address() const {
    return server_socket_->GetLocalAddress();
  }

  // Packets received on one of the networks.
  void OnInternalPacket(AsyncPacketSocket* socket, const char* buf,
                        size_t size, const SocketAddress& addr,
                        const PacketTime& packet_time);
  void OnExternalPacket(AsyncPacketSocket* socket, const char* buf,
                        size_t size, const SocketAddress& remote_addr,
                        const PacketTime& packet_time);

 private:
  typedef std::set<SocketAddress, AddrCmp> AddressSet;

  /* Records a translation and the associated external socket. */
  struct TransEntry {
    TransEntry(const SocketAddressPair& r, AsyncUDPSocket* s, NAT* nat);
    ~TransEntry();

    void WhitelistInsert(const SocketAddress& addr);
    bool WhitelistContains(const SocketAddress& ext_addr);

    SocketAddressPair route;
    AsyncUDPSocket* socket;
    AddressSet* whitelist;
    CriticalSection crit_;
  };

  typedef std::map<SocketAddressPair, TransEntry*, RouteCmp> InternalMap;
  typedef std::map<SocketAddress, TransEntry*> ExternalMap;

  /* Creates a new entry that translates the given route. */
  void Translate(const SocketAddressPair& route);

  /* Determines whether the NAT would filter out a packet from this address. */
  bool ShouldFilterOut(TransEntry* entry, const SocketAddress& ext_addr);

  NAT* nat_;
  SocketFactory* internal_;
  SocketFactory* external_;
  SocketAddress external_ip_;
  AsyncUDPSocket* server_socket_;
  AsyncSocket* tcp_server_socket_;
  InternalMap* int_map_;
  ExternalMap* ext_map_;
  DISALLOW_EVIL_CONSTRUCTORS(NATServer);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_NATSERVER_H_
