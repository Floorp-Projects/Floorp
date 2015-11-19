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
  PORTALLOCATOR_DISABLE_UDP = 0x01,
  PORTALLOCATOR_DISABLE_STUN = 0x02,
  PORTALLOCATOR_DISABLE_RELAY = 0x04,
  PORTALLOCATOR_DISABLE_TCP = 0x08,
  PORTALLOCATOR_ENABLE_SHAKER = 0x10,
  PORTALLOCATOR_ENABLE_BUNDLE = 0x20,
  PORTALLOCATOR_ENABLE_IPV6 = 0x40,
  PORTALLOCATOR_ENABLE_SHARED_UFRAG = 0x80,
  PORTALLOCATOR_ENABLE_SHARED_SOCKET = 0x100,
  PORTALLOCATOR_ENABLE_STUN_RETRANSMIT_ATTRIBUTE = 0x200,
  PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION = 0x400,
};

const uint32 kDefaultPortAllocatorFlags = 0;

const uint32 kDefaultStepDelay = 1000;  // 1 sec step delay.
// As per RFC 5245 Appendix B.1, STUN transactions need to be paced at certain
// internal. Less than 20ms is not acceptable. We choose 50ms as our default.
const uint32 kMinimumStepDelay = 50;

// CF = CANDIDATE FILTER
enum {
  CF_NONE = 0x0,
  CF_HOST = 0x1,
  CF_REFLEXIVE = 0x2,
  CF_RELAY = 0x4,
  CF_ALL = 0x7,
};

class PortAllocatorSessionMuxer;

class PortAllocatorSession : public sigslot::has_slots<> {
 public:
  // Content name passed in mostly for logging and debugging.
  // TODO(mallinath) - Change username and password to ice_ufrag and ice_pwd.
  PortAllocatorSession(const std::string& content_name,
                       int component,
                       const std::string& username,
                       const std::string& password,
                       uint32 flags);

  // Subclasses should clean up any ports created.
  virtual ~PortAllocatorSession() {}

  uint32 flags() const { return flags_; }
  void set_flags(uint32 flags) { flags_ = flags; }
  std::string content_name() const { return content_name_; }
  int component() const { return component_; }

  // Starts gathering STUN and Relay configurations.
  virtual void StartGettingPorts() = 0;
  virtual void StopGettingPorts() = 0;
  virtual bool IsGettingPorts() = 0;

  sigslot::signal2<PortAllocatorSession*, PortInterface*> SignalPortReady;
  sigslot::signal2<PortAllocatorSession*,
                   const std::vector<Candidate>&> SignalCandidatesReady;
  sigslot::signal1<PortAllocatorSession*> SignalCandidatesAllocationDone;

  virtual uint32 generation() { return generation_; }
  virtual void set_generation(uint32 generation) { generation_ = generation; }
  sigslot::signal1<PortAllocatorSession*> SignalDestroyed;

 protected:
  const std::string& username() const { return username_; }
  const std::string& password() const { return password_; }

  std::string content_name_;
  int component_;

 private:
  uint32 flags_;
  uint32 generation_;
  std::string username_;
  std::string password_;
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
  virtual ~PortAllocator();

  PortAllocatorSession* CreateSession(
      const std::string& sid,
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd);

  PortAllocatorSessionMuxer* GetSessionMuxer(const std::string& key) const;
  void OnSessionMuxerDestroyed(PortAllocatorSessionMuxer* session);

  uint32 flags() const { return flags_; }
  void set_flags(uint32 flags) { flags_ = flags; }

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

  uint32 step_delay() const { return step_delay_; }
  void set_step_delay(uint32 delay) {
    step_delay_ = delay;
  }

  bool allow_tcp_listen() const { return allow_tcp_listen_; }
  void set_allow_tcp_listen(bool allow_tcp_listen) {
    allow_tcp_listen_ = allow_tcp_listen;
  }

  uint32 candidate_filter() { return candidate_filter_; }
  bool set_candidate_filter(uint32 filter) {
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

  typedef std::map<std::string, PortAllocatorSessionMuxer*> SessionMuxerMap;

  uint32 flags_;
  std::string agent_;
  rtc::ProxyInfo proxy_;
  int min_port_;
  int max_port_;
  uint32 step_delay_;
  SessionMuxerMap muxers_;
  bool allow_tcp_listen_;
  uint32 candidate_filter_;
  std::string origin_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_PORTALLOCATOR_H_
