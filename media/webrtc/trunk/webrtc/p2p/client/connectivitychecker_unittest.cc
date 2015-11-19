/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/relayport.h"
#include "webrtc/p2p/base/stunport.h"
#include "webrtc/p2p/client/connectivitychecker.h"
#include "webrtc/p2p/client/httpportallocator.h"
#include "webrtc/base/asynchttprequest.h"
#include "webrtc/base/fakenetwork.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/socketaddress.h"

namespace cricket {

static const rtc::SocketAddress kClientAddr1("11.11.11.11", 0);
static const rtc::SocketAddress kClientAddr2("22.22.22.22", 0);
static const rtc::SocketAddress kExternalAddr("33.33.33.33", 3333);
static const rtc::SocketAddress kStunAddr("44.44.44.44", 4444);
static const rtc::SocketAddress kRelayAddr("55.55.55.55", 5555);
static const rtc::SocketAddress kProxyAddr("66.66.66.66", 6666);
static const rtc::ProxyType kProxyType = rtc::PROXY_HTTPS;
static const char kRelayHost[] = "relay.google.com";
static const char kRelayToken[] =
    "CAESFwoOb2phQGdvb2dsZS5jb20Q043h47MmGhBTB1rbfIXkhuarDCZe+xF6";
static const char kBrowserAgent[] = "browser_test";
static const char kJid[] = "a.b@c";
static const char kUserName[] = "testuser";
static const char kPassword[] = "testpassword";
static const char kMagicCookie[] = "testcookie";
static const char kRelayUdpPort[] = "4444";
static const char kRelayTcpPort[] = "5555";
static const char kRelaySsltcpPort[] = "6666";
static const char kSessionId[] = "testsession";
static const char kConnection[] = "testconnection";
static const int kMinPort = 1000;
static const int kMaxPort = 2000;

// Fake implementation to mock away real network usage.
class FakeRelayPort : public RelayPort {
 public:
  FakeRelayPort(rtc::Thread* thread,
                rtc::PacketSocketFactory* factory,
                rtc::Network* network, const rtc::IPAddress& ip,
                int min_port, int max_port,
                const std::string& username, const std::string& password)
      : RelayPort(thread, factory, network, ip, min_port, max_port,
                  username, password) {
  }

  // Just signal that we are done.
  virtual void PrepareAddress() {
    SignalPortComplete(this);
  }
};

// Fake implementation to mock away real network usage.
class FakeStunPort : public StunPort {
 public:
  FakeStunPort(rtc::Thread* thread,
               rtc::PacketSocketFactory* factory,
               rtc::Network* network,
               const rtc::IPAddress& ip,
               int min_port, int max_port,
               const std::string& username, const std::string& password,
               const ServerAddresses& server_addr)
      : StunPort(thread, factory, network, ip, min_port, max_port,
                 username, password, server_addr, std::string()) {
  }

  // Just set external address and signal that we are done.
  virtual void PrepareAddress() {
    AddAddress(kExternalAddr, kExternalAddr, rtc::SocketAddress(), "udp", "",
               STUN_PORT_TYPE, ICE_TYPE_PREFERENCE_SRFLX, 0, true);
    SignalPortComplete(this);
  }
};

// Fake implementation to mock away real network usage by responding
// to http requests immediately.
class FakeHttpPortAllocatorSession : public TestHttpPortAllocatorSession {
 public:
  FakeHttpPortAllocatorSession(
      HttpPortAllocator* allocator,
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag, const std::string& ice_pwd,
      const std::vector<rtc::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay_token,
      const std::string& agent)
      : TestHttpPortAllocatorSession(allocator,
                                     content_name,
                                     component,
                                     ice_ufrag,
                                     ice_pwd,
                                     stun_hosts,
                                     relay_hosts,
                                     relay_token,
                                     agent) {
  }
  virtual void SendSessionRequest(const std::string& host, int port) {
    FakeReceiveSessionResponse(host, port);
  }

  // Pass results to the real implementation.
  void FakeReceiveSessionResponse(const std::string& host, int port) {
    rtc::AsyncHttpRequest* response = CreateAsyncHttpResponse(port);
    TestHttpPortAllocatorSession::OnRequestDone(response);
    response->Destroy(true);
  }

 private:
  // Helper method for creating a response to a relay session request.
  rtc::AsyncHttpRequest* CreateAsyncHttpResponse(int port) {
    rtc::AsyncHttpRequest* request =
        new rtc::AsyncHttpRequest(kBrowserAgent);
    std::stringstream ss;
    ss << "username=" << kUserName << std::endl
       << "password=" << kPassword << std::endl
       << "magic_cookie=" << kMagicCookie << std::endl
       << "relay.ip=" << kRelayAddr.ipaddr().ToString() << std::endl
       << "relay.udp_port=" << kRelayUdpPort << std::endl
       << "relay.tcp_port=" << kRelayTcpPort << std::endl
       << "relay.ssltcp_port=" << kRelaySsltcpPort << std::endl;
    request->response().document.reset(
        new rtc::MemoryStream(ss.str().c_str()));
    request->response().set_success();
    request->set_port(port);
    request->set_secure(port == rtc::HTTP_SECURE_PORT);
    return request;
  }
};

// Fake implementation for creating fake http sessions.
class FakeHttpPortAllocator : public HttpPortAllocator {
 public:
  FakeHttpPortAllocator(rtc::NetworkManager* network_manager,
                        const std::string& user_agent)
      : HttpPortAllocator(network_manager, user_agent) {
  }

  virtual PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name, int component,
      const std::string& ice_ufrag, const std::string& ice_pwd) {
    std::vector<rtc::SocketAddress> stun_hosts;
    stun_hosts.push_back(kStunAddr);
    std::vector<std::string> relay_hosts;
    relay_hosts.push_back(kRelayHost);
    return new FakeHttpPortAllocatorSession(this,
                                            content_name,
                                            component,
                                            ice_ufrag,
                                            ice_pwd,
                                            stun_hosts,
                                            relay_hosts,
                                            kRelayToken,
                                            kBrowserAgent);
  }
};

class ConnectivityCheckerForTest : public ConnectivityChecker {
 public:
  ConnectivityCheckerForTest(rtc::Thread* worker,
                             const std::string& jid,
                             const std::string& session_id,
                             const std::string& user_agent,
                             const std::string& relay_token,
                             const std::string& connection)
      : ConnectivityChecker(worker,
                            jid,
                            session_id,
                            user_agent,
                            relay_token,
                            connection),
        proxy_initiated_(false) {
  }

  rtc::FakeNetworkManager* network_manager() const {
    return network_manager_;
  }

  FakeHttpPortAllocator* port_allocator() const {
    return fake_port_allocator_;
  }

 protected:
  // Overridden methods for faking a real network.
  virtual rtc::NetworkManager* CreateNetworkManager() {
    network_manager_ = new rtc::FakeNetworkManager();
    return network_manager_;
  }
  virtual rtc::BasicPacketSocketFactory* CreateSocketFactory(
      rtc::Thread* thread) {
    // Create socket factory, for simplicity, let it run on the current thread.
    socket_factory_ =
        new rtc::BasicPacketSocketFactory(rtc::Thread::Current());
    return socket_factory_;
  }
  virtual HttpPortAllocator* CreatePortAllocator(
      rtc::NetworkManager* network_manager,
      const std::string& user_agent,
      const std::string& relay_token) {
    fake_port_allocator_ =
        new FakeHttpPortAllocator(network_manager, user_agent);
    return fake_port_allocator_;
  }
  virtual StunPort* CreateStunPort(
      const std::string& username, const std::string& password,
      const PortConfiguration* config, rtc::Network* network) {
    return new FakeStunPort(worker(),
                            socket_factory_,
                            network,
                            network->GetBestIP(),
                            kMinPort,
                            kMaxPort,
                            username,
                            password,
                            config->stun_servers);
  }
  virtual RelayPort* CreateRelayPort(
      const std::string& username, const std::string& password,
      const PortConfiguration* config, rtc::Network* network) {
    return new FakeRelayPort(worker(),
                             socket_factory_,
                             network,
                             network->GetBestIP(),
                             kMinPort,
                             kMaxPort,
                             username,
                             password);
  }
  virtual void InitiateProxyDetection() {
    if (!proxy_initiated_) {
      proxy_initiated_ = true;
      proxy_info_.address = kProxyAddr;
      proxy_info_.type = kProxyType;
      SetProxyInfo(proxy_info_);
    }
  }

  virtual rtc::ProxyInfo GetProxyInfo() const {
    return proxy_info_;
  }

 private:
  rtc::BasicPacketSocketFactory* socket_factory_;
  FakeHttpPortAllocator* fake_port_allocator_;
  rtc::FakeNetworkManager* network_manager_;
  rtc::ProxyInfo proxy_info_;
  bool proxy_initiated_;
};

class ConnectivityCheckerTest : public testing::Test {
 protected:
  void VerifyNic(const NicInfo& info,
                 const rtc::SocketAddress& local_address) {
    // Verify that the external address has been set.
    EXPECT_EQ(kExternalAddr, info.external_address);

    // Verify that the stun server address has been set.
    EXPECT_EQ(1U, info.stun_server_addresses.size());
    EXPECT_EQ(kStunAddr, *(info.stun_server_addresses.begin()));

    // Verify that the media server address has been set. Don't care
    // about port since it is different for different protocols.
    EXPECT_EQ(kRelayAddr.ipaddr(), info.media_server_address.ipaddr());

    // Verify that local ip matches.
    EXPECT_EQ(local_address.ipaddr(), info.ip);

    // Verify that we have received responses for our
    // pings. Unsuccessful ping has rtt value -1, successful >= 0.
    EXPECT_GE(info.stun.rtt, 0);
    EXPECT_GE(info.udp.rtt, 0);
    EXPECT_GE(info.tcp.rtt, 0);
    EXPECT_GE(info.ssltcp.rtt, 0);

    // If proxy has been set, verify address and type.
    if (!info.proxy_info.address.IsNil()) {
      EXPECT_EQ(kProxyAddr, info.proxy_info.address);
      EXPECT_EQ(kProxyType, info.proxy_info.type);
    }
  }
};

// Tests a configuration with two network interfaces. Verifies that 4
// combinations of ip/proxy are created and that all protocols are
// tested on each combination.
TEST_F(ConnectivityCheckerTest, TestStart) {
  ConnectivityCheckerForTest connectivity_checker(rtc::Thread::Current(),
                                                  kJid,
                                                  kSessionId,
                                                  kBrowserAgent,
                                                  kRelayToken,
                                                  kConnection);
  connectivity_checker.Initialize();
  connectivity_checker.set_stun_address(kStunAddr);
  connectivity_checker.network_manager()->AddInterface(kClientAddr1);
  connectivity_checker.network_manager()->AddInterface(kClientAddr2);

  connectivity_checker.Start();
  rtc::Thread::Current()->ProcessMessages(1000);

  NicMap nics = connectivity_checker.GetResults();

  // There should be 4 nics in our map. 2 for each interface added,
  // one with proxy set and one without.
  EXPECT_EQ(4U, nics.size());

  // First verify interfaces without proxy.
  rtc::SocketAddress nilAddress;

  // First lookup the address of the first nic combined with no proxy.
  NicMap::iterator i = nics.find(NicId(kClientAddr1.ipaddr(), nilAddress));
  ASSERT(i != nics.end());
  NicInfo info = i->second;
  VerifyNic(info, kClientAddr1);

  // Then make sure the second device has been tested without proxy.
  i = nics.find(NicId(kClientAddr2.ipaddr(), nilAddress));
  ASSERT(i != nics.end());
  info = i->second;
  VerifyNic(info, kClientAddr2);

  // Now verify both interfaces with proxy.
  i = nics.find(NicId(kClientAddr1.ipaddr(), kProxyAddr));
  ASSERT(i != nics.end());
  info = i->second;
  VerifyNic(info, kClientAddr1);

  i = nics.find(NicId(kClientAddr2.ipaddr(), kProxyAddr));
  ASSERT(i != nics.end());
  info = i->second;
  VerifyNic(info, kClientAddr2);
};

// Tests that nothing bad happens if thera are no network interfaces
// available to check.
TEST_F(ConnectivityCheckerTest, TestStartNoNetwork) {
  ConnectivityCheckerForTest connectivity_checker(rtc::Thread::Current(),
                                                  kJid,
                                                  kSessionId,
                                                  kBrowserAgent,
                                                  kRelayToken,
                                                  kConnection);
  connectivity_checker.Initialize();
  connectivity_checker.Start();
  rtc::Thread::Current()->ProcessMessages(1000);

  NicMap nics = connectivity_checker.GetResults();

  // Verify that no nics where checked.
  EXPECT_EQ(0U, nics.size());
}

}  // namespace cricket
