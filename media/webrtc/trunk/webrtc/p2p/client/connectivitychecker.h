/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_CLIENT_CONNECTIVITYCHECKER_H_
#define WEBRTC_P2P_CLIENT_CONNECTIVITYCHECKER_H_

#include <map>
#include <string>

#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/client/httpportallocator.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/network.h"
#include "webrtc/base/proxyinfo.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/socketaddress.h"

namespace rtc {
class AsyncHttpRequest;
class AutoDetectProxy;
class BasicPacketSocketFactory;
class NetworkManager;
class PacketSocketFactory;
class SignalThread;
class TestHttpPortAllocatorSession;
class Thread;
}

namespace cricket {
class HttpPortAllocator;
class Port;
class PortAllocatorSession;
struct PortConfiguration;
class RelayPort;
class StunPort;

// Contains details about a discovered firewall that are of interest
// when debugging call failures.
struct FirewallInfo {
  std::string brand;
  std::string model;

  // TODO: List of current port mappings.
};

// Contains details about a specific connect attempt.
struct ConnectInfo {
  ConnectInfo()
      : rtt(-1), error(0) {}
  // Time when the connection was initiated. Needed for calculating
  // the round trip time.
  uint32 start_time_ms;
  // Round trip time in milliseconds or -1 for failed connection.
  int32 rtt;
  // Error code representing low level errors like socket errors.
  int error;
};

// Identifier for a network interface and proxy address pair.
struct NicId {
  NicId(const rtc::IPAddress& ip,
        const rtc::SocketAddress& proxy_address)
      : ip(ip),
        proxy_address(proxy_address) {
  }
  rtc::IPAddress ip;
  rtc::SocketAddress proxy_address;
};

// Comparator implementation identifying unique network interface and
// proxy address pairs.
class NicIdComparator {
 public:
  int compare(const NicId &first, const NicId &second) const {
    if (first.ip == second.ip) {
      // Compare proxy address.
      if (first.proxy_address == second.proxy_address) {
        return 0;
      } else {
        return first.proxy_address < second.proxy_address? -1 : 1;
      }
    }
    return first.ip < second.ip ? -1 : 1;
  }

  bool operator()(const NicId &first, const NicId &second) const {
    return (compare(first, second) < 0);
  }
};

// Contains information of a network interface and proxy address pair.
struct NicInfo {
  NicInfo() {}
  rtc::IPAddress ip;
  rtc::ProxyInfo proxy_info;
  rtc::SocketAddress external_address;
  ServerAddresses stun_server_addresses;
  rtc::SocketAddress media_server_address;
  ConnectInfo stun;
  ConnectInfo http;
  ConnectInfo https;
  ConnectInfo udp;
  ConnectInfo tcp;
  ConnectInfo ssltcp;
  FirewallInfo firewall;
};

// Holds the result of the connectivity check.
class NicMap : public std::map<NicId, NicInfo, NicIdComparator> {
};

class TestHttpPortAllocatorSession : public HttpPortAllocatorSession {
 public:
  TestHttpPortAllocatorSession(
      HttpPortAllocator* allocator,
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd,
      const std::vector<rtc::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay_token,
      const std::string& user_agent)
      : HttpPortAllocatorSession(
          allocator, content_name, component, ice_ufrag, ice_pwd, stun_hosts,
          relay_hosts, relay_token, user_agent) {
  }
  void set_proxy(const rtc::ProxyInfo& proxy) {
    proxy_ = proxy;
  }

  void ConfigReady(PortConfiguration* config);

  void OnRequestDone(rtc::SignalThread* data);

  sigslot::signal4<const std::string&, const std::string&,
                   const PortConfiguration*,
                   const rtc::ProxyInfo&> SignalConfigReady;
  sigslot::signal1<rtc::AsyncHttpRequest*> SignalRequestDone;

 private:
  rtc::ProxyInfo proxy_;
};

// Runs a request/response check on all network interface and proxy
// address combinations. The check is considered done either when all
// checks has been successful or when the check times out.
class ConnectivityChecker
    : public rtc::MessageHandler, public sigslot::has_slots<> {
 public:
  ConnectivityChecker(rtc::Thread* worker,
                      const std::string& jid,
                      const std::string& session_id,
                      const std::string& user_agent,
                      const std::string& relay_token,
                      const std::string& connection);
  virtual ~ConnectivityChecker();

  // Virtual for gMock.
  virtual bool Initialize();
  virtual void Start();

  // MessageHandler implementation.
  virtual void OnMessage(rtc::Message *msg);

  // Instruct checker to stop and wait until that's done.
  // Virtual for gMock.
  virtual void Stop() {
    worker_->Stop();
  }

  const NicMap& GetResults() const {
    return nics_;
  }

  void set_timeout_ms(uint32 timeout) {
    timeout_ms_ = timeout;
  }

  void set_stun_address(const rtc::SocketAddress& stun_address) {
    stun_address_ = stun_address;
  }

  const std::string& connection() const {
    return connection_;
  }

  const std::string& jid() const {
    return jid_;
  }

  const std::string& session_id() const {
    return session_id_;
  }

  // Context: Main Thread. Signalled when the connectivity check is complete.
  sigslot::signal1<ConnectivityChecker*> SignalCheckDone;

 protected:
  // Can be overridden for test.
  virtual rtc::NetworkManager* CreateNetworkManager() {
    return new rtc::BasicNetworkManager();
  }
  virtual rtc::BasicPacketSocketFactory* CreateSocketFactory(
      rtc::Thread* thread) {
    return new rtc::BasicPacketSocketFactory(thread);
  }
  virtual HttpPortAllocator* CreatePortAllocator(
      rtc::NetworkManager* network_manager,
      const std::string& user_agent,
      const std::string& relay_token);
  virtual StunPort* CreateStunPort(
      const std::string& username, const std::string& password,
      const PortConfiguration* config, rtc::Network* network);
  virtual RelayPort* CreateRelayPort(
      const std::string& username, const std::string& password,
      const PortConfiguration* config, rtc::Network* network);
  virtual void InitiateProxyDetection();
  virtual void SetProxyInfo(const rtc::ProxyInfo& info);
  virtual rtc::ProxyInfo GetProxyInfo() const;

  rtc::Thread* worker() {
    return worker_;
  }

 private:
  bool AddNic(const rtc::IPAddress& ip,
              const rtc::SocketAddress& proxy_address);
  void AllocatePorts();
  void AllocateRelayPorts();
  void CheckNetworks();
  void CreateRelayPorts(
      const std::string& username, const std::string& password,
      const PortConfiguration* config, const rtc::ProxyInfo& proxy_info);

  // Must be called by the worker thread.
  void CleanUp();

  void OnRequestDone(rtc::AsyncHttpRequest* request);
  void OnRelayPortComplete(Port* port);
  void OnStunPortComplete(Port* port);
  void OnRelayPortError(Port* port);
  void OnStunPortError(Port* port);
  void OnNetworksChanged();
  void OnProxyDetect(rtc::SignalThread* thread);
  void OnConfigReady(
      const std::string& username, const std::string& password,
      const PortConfiguration* config, const rtc::ProxyInfo& proxy);
  void OnConfigWithProxyReady(const PortConfiguration*);
  void RegisterHttpStart(int port);
  rtc::Thread* worker_;
  std::string jid_;
  std::string session_id_;
  std::string user_agent_;
  std::string relay_token_;
  std::string connection_;
  rtc::AutoDetectProxy* proxy_detect_;
  rtc::scoped_ptr<rtc::NetworkManager> network_manager_;
  rtc::scoped_ptr<rtc::BasicPacketSocketFactory> socket_factory_;
  rtc::scoped_ptr<HttpPortAllocator> port_allocator_;
  NicMap nics_;
  std::vector<Port*> ports_;
  std::vector<PortAllocatorSession*> sessions_;
  uint32 timeout_ms_;
  rtc::SocketAddress stun_address_;
  rtc::Thread* main_;
  bool started_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_CLIENT_CONNECTIVITYCHECKER_H_
