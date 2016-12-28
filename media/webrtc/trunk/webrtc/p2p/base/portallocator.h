/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_PORTALLOCATOR_H_
#define WEBRTC_P2P_BASE_PORTALLOCATOR_H_

#include <string>
#include <vector>

#include "webrtc/p2p/base/port.h"
#include "webrtc/p2p/base/portinterface.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/proxyinfo.h"
#include "webrtc/base/sigslot.h"

namespace cricket {

// PortAllocator is responsible for allocating Port types for a given
// P2PSocket. It also handles port freeing.
//
// Clients can override this class to control port allocation, including
// what kinds of ports are allocated.

enum {
  // Disable local UDP ports. This doesn't impact how we connect to relay
  // servers.
  PORTALLOCATOR_DISABLE_UDP = 0x01,
  PORTALLOCATOR_DISABLE_STUN = 0x02,
  PORTALLOCATOR_DISABLE_RELAY = 0x04,
  // Disable local TCP ports. This doesn't impact how we connect to relay
  // servers.
  PORTALLOCATOR_DISABLE_TCP = 0x08,
  PORTALLOCATOR_ENABLE_SHAKER = 0x10,
  PORTALLOCATOR_ENABLE_IPV6 = 0x40,
  // TODO(pthatcher): Remove this once it's no longer used in:
  // remoting/client/plugin/pepper_port_allocator.cc
  // remoting/protocol/chromium_port_allocator.cc
  // remoting/test/fake_port_allocator.cc
  // It's a no-op and is no longer needed.
  PORTALLOCATOR_ENABLE_SHARED_UFRAG = 0x80,
  PORTALLOCATOR_ENABLE_SHARED_SOCKET = 0x100,
  PORTALLOCATOR_ENABLE_STUN_RETRANSMIT_ATTRIBUTE = 0x200,
  // When specified, we'll only allocate the STUN candidate for the public
  // interface as seen by regular http traffic and the HOST candidate associated
  // with the default local interface.
  PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION = 0x400,
  // When specified along with PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION, the
  // default local candidate mentioned above will not be allocated. Only the
  // STUN candidate will be.
  PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE = 0x800,
  // Disallow use of UDP when connecting to a relay server. Since proxy servers
  // usually don't handle UDP, using UDP will leak the IP address.
  PORTALLOCATOR_DISABLE_UDP_RELAY = 0x1000,
};

const uint32_t kDefaultPortAllocatorFlags = 0;

const uint32_t kDefaultStepDelay = 1000;  // 1 sec step delay.
// As per RFC 5245 Appendix B.1, STUN transactions need to be paced at certain
// internal. Less than 20ms is not acceptable. We choose 50ms as our default.
const uint32_t kMinimumStepDelay = 50;

// CF = CANDIDATE FILTER
enum {
  CF_NONE = 0x0,
  CF_HOST = 0x1,
  CF_REFLEXIVE = 0x2,
  CF_RELAY = 0x4,
  CF_ALL = 0x7,
};

// TODO(deadbeef): Rename to TurnCredentials (and username to ufrag).
struct RelayCredentials {
  RelayCredentials() {}
  RelayCredentials(const std::string& username, const std::string& password)
      : username(username), password(password) {}

  std::string username;
  std::string password;
};

typedef std::vector<ProtocolAddress> PortList;
// TODO(deadbeef): Rename to TurnServerConfig.
struct RelayServerConfig {
  RelayServerConfig(RelayType type) : type(type), priority(0) {}

  RelayServerConfig(const std::string& address,
                    int port,
                    const std::string& username,
                    const std::string& password,
                    ProtocolType proto,
                    bool secure)
      : type(RELAY_TURN), credentials(username, password) {
    ports.push_back(
        ProtocolAddress(rtc::SocketAddress(address, port), proto, secure));
  }

  RelayType type;
  PortList ports;
  RelayCredentials credentials;
  int priority;
};

class PortAllocatorSession : public sigslot::has_slots<> {
 public:
  // Content name passed in mostly for logging and debugging.
  PortAllocatorSession(const std::string& content_name,
                       int component,
                       const std::string& ice_ufrag,
                       const std::string& ice_pwd,
                       uint32_t flags);

  // Subclasses should clean up any ports created.
  virtual ~PortAllocatorSession() {}

  uint32_t flags() const { return flags_; }
  void set_flags(uint32_t flags) { flags_ = flags; }
  std::string content_name() const { return content_name_; }
  int component() const { return component_; }

  // Starts gathering STUN and Relay configurations.
  virtual void StartGettingPorts() = 0;
  virtual void StopGettingPorts() = 0;
  // Only stop the existing gathering process but may start new ones if needed.
  virtual void ClearGettingPorts() = 0;
  // Whether the process of getting ports has been stopped.
  virtual bool IsGettingPorts() = 0;

  sigslot::signal2<PortAllocatorSession*, PortInterface*> SignalPortReady;
  sigslot::signal2<PortAllocatorSession*,
                   const std::vector<Candidate>&> SignalCandidatesReady;
  sigslot::signal1<PortAllocatorSession*> SignalCandidatesAllocationDone;

  virtual uint32_t generation() { return generation_; }
  virtual void set_generation(uint32_t generation) { generation_ = generation; }
  sigslot::signal1<PortAllocatorSession*> SignalDestroyed;

  const std::string& ice_ufrag() const { return ice_ufrag_; }
  const std::string& ice_pwd() const { return ice_pwd_; }

 protected:
  // TODO(deadbeef): Get rid of these when everyone switches to ice_ufrag and
  // ice_pwd.
  const std::string& username() const { return ice_ufrag_; }
  const std::string& password() const { return ice_pwd_; }

  std::string content_name_;
  int component_;

 private:
  uint32_t flags_;
  uint32_t generation_;
  std::string ice_ufrag_;
  std::string ice_pwd_;
};

class PortAllocator : public sigslot::has_slots<> {
 public:
  PortAllocator() :
      flags_(kDefaultPortAllocatorFlags),
      min_port_(0),
      max_port_(0),
      step_delay_(kDefaultStepDelay),
      allow_tcp_listen_(true),
      candidate_filter_(CF_ALL) {
    // This will allow us to have old behavior on non webrtc clients.
  }
  virtual ~PortAllocator() {}

  // Set STUN and TURN servers to be used in future sessions.
  virtual void SetIceServers(
      const ServerAddresses& stun_servers,
      const std::vector<RelayServerConfig>& turn_servers) = 0;

  // Sets the network types to ignore.
  // Values are defined by the AdapterType enum.
  // For instance, calling this with
  // ADAPTER_TYPE_ETHERNET | ADAPTER_TYPE_LOOPBACK will ignore Ethernet and
  // loopback interfaces.
  virtual void SetNetworkIgnoreMask(int network_ignore_mask) = 0;

  PortAllocatorSession* CreateSession(
      const std::string& sid,
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd);

  uint32_t flags() const { return flags_; }
  void set_flags(uint32_t flags) { flags_ = flags; }

  const std::string& user_agent() const { return agent_; }
  const rtc::ProxyInfo& proxy() const { return proxy_; }
  void set_proxy(const std::string& agent, const rtc::ProxyInfo& proxy) {
    agent_ = agent;
    proxy_ = proxy;
  }

  // Gets/Sets the port range to use when choosing client ports.
  int min_port() const { return min_port_; }
  int max_port() const { return max_port_; }
  bool SetPortRange(int min_port, int max_port) {
    if (min_port > max_port) {
      return false;
    }

    min_port_ = min_port;
    max_port_ = max_port;
    return true;
  }

  uint32_t step_delay() const { return step_delay_; }
  void set_step_delay(uint32_t delay) { step_delay_ = delay; }

  bool allow_tcp_listen() const { return allow_tcp_listen_; }
  void set_allow_tcp_listen(bool allow_tcp_listen) {
    allow_tcp_listen_ = allow_tcp_listen;
  }

  uint32_t candidate_filter() { return candidate_filter_; }
  bool set_candidate_filter(uint32_t filter) {
    // TODO(mallinath) - Do transition check?
    candidate_filter_ = filter;
    return true;
  }

  // Gets/Sets the Origin value used for WebRTC STUN requests.
  const std::string& origin() const { return origin_; }
  void set_origin(const std::string& origin) { origin_ = origin; }

 protected:
  virtual PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) = 0;

  uint32_t flags_;
  std::string agent_;
  rtc::ProxyInfo proxy_;
  int min_port_;
  int max_port_;
  uint32_t step_delay_;
  bool allow_tcp_listen_;
  uint32_t candidate_filter_;
  std::string origin_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_PORTALLOCATOR_H_
