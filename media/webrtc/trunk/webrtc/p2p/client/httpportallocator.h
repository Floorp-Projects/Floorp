/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_CLIENT_HTTPPORTALLOCATOR_H_
#define WEBRTC_P2P_CLIENT_HTTPPORTALLOCATOR_H_

#include <list>
#include <string>
#include <vector>

#include "webrtc/p2p/client/basicportallocator.h"

class HttpPortAllocatorTest_TestSessionRequestUrl_Test;

namespace rtc {
class AsyncHttpRequest;
class SignalThread;
}

namespace cricket {

class HttpPortAllocatorBase : public BasicPortAllocator {
 public:
  // The number of HTTP requests we should attempt before giving up.
  static const int kNumRetries;

  // Records the URL that we will GET in order to create a session.
  static const char kCreateSessionURL[];

  HttpPortAllocatorBase(rtc::NetworkManager* network_manager,
                        const std::string& user_agent);
  HttpPortAllocatorBase(rtc::NetworkManager* network_manager,
                        rtc::PacketSocketFactory* socket_factory,
                        const std::string& user_agent);
  virtual ~HttpPortAllocatorBase();

  // CreateSession is defined in BasicPortAllocator but is
  // redefined here as pure virtual.
  virtual PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) = 0;

  void SetStunHosts(const std::vector<rtc::SocketAddress>& hosts) {
    if (!hosts.empty()) {
      stun_hosts_ = hosts;
    }
  }
  void SetRelayHosts(const std::vector<std::string>& hosts) {
    if (!hosts.empty()) {
      relay_hosts_ = hosts;
    }
  }
  void SetRelayToken(const std::string& relay) { relay_token_ = relay; }

  const std::vector<rtc::SocketAddress>& stun_hosts() const {
    return stun_hosts_;
  }

  const std::vector<std::string>& relay_hosts() const {
    return relay_hosts_;
  }

  const std::string& relay_token() const {
    return relay_token_;
  }

  const std::string& user_agent() const {
    return agent_;
  }

 private:
  std::vector<rtc::SocketAddress> stun_hosts_;
  std::vector<std::string> relay_hosts_;
  std::string relay_token_;
  std::string agent_;
};

class RequestData;

class HttpPortAllocatorSessionBase : public BasicPortAllocatorSession {
 public:
  HttpPortAllocatorSessionBase(
      HttpPortAllocatorBase* allocator,
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd,
      const std::vector<rtc::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay,
      const std::string& agent);
  virtual ~HttpPortAllocatorSessionBase();

  const std::string& relay_token() const {
    return relay_token_;
  }

  const std::string& user_agent() const {
      return agent_;
  }

  virtual void SendSessionRequest(const std::string& host, int port) = 0;
  virtual void ReceiveSessionResponse(const std::string& response);

  // Made public for testing. Should be protected.
  std::string GetSessionRequestUrl();

 protected:
  virtual void GetPortConfigurations();
  void TryCreateRelaySession();
  virtual HttpPortAllocatorBase* allocator() {
    return static_cast<HttpPortAllocatorBase*>(
        BasicPortAllocatorSession::allocator());
  }

 private:
  std::vector<std::string> relay_hosts_;
  std::vector<rtc::SocketAddress> stun_hosts_;
  std::string relay_token_;
  std::string agent_;
  int attempts_;
};

class HttpPortAllocator : public HttpPortAllocatorBase {
 public:
  HttpPortAllocator(rtc::NetworkManager* network_manager,
                    const std::string& user_agent);
  HttpPortAllocator(rtc::NetworkManager* network_manager,
                    rtc::PacketSocketFactory* socket_factory,
                    const std::string& user_agent);
  virtual ~HttpPortAllocator();
  virtual PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag, const std::string& ice_pwd);
};

class HttpPortAllocatorSession : public HttpPortAllocatorSessionBase {
 public:
  HttpPortAllocatorSession(
      HttpPortAllocator* allocator,
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd,
      const std::vector<rtc::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay,
      const std::string& agent);
  virtual ~HttpPortAllocatorSession();

  virtual void SendSessionRequest(const std::string& host, int port);

 protected:
  // Protected for diagnostics.
  virtual void OnRequestDone(rtc::SignalThread* request);

 private:
  std::list<rtc::AsyncHttpRequest*> requests_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_CLIENT_HTTPPORTALLOCATOR_H_
