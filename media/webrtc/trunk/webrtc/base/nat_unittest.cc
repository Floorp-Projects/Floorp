/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/natserver.h"
#include "webrtc/base/natsocketfactory.h"
#include "webrtc/base/nethelpers.h"
#include "webrtc/base/network.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/testclient.h"
#include "webrtc/base/virtualsocketserver.h"
#include "webrtc/test/testsupport/gtest_disable.h"

using namespace rtc;

bool CheckReceive(
    TestClient* client, bool should_receive, const char* buf, size_t size) {
  return (should_receive) ?
      client->CheckNextPacket(buf, size, 0) :
      client->CheckNoPacket();
}

TestClient* CreateTestClient(
      SocketFactory* factory, const SocketAddress& local_addr) {
  AsyncUDPSocket* socket = AsyncUDPSocket::Create(factory, local_addr);
  return new TestClient(socket);
}

// Tests that when sending from internal_addr to external_addrs through the
// NAT type specified by nat_type, all external addrs receive the sent packet
// and, if exp_same is true, all use the same mapped-address on the NAT.
void TestSend(
      SocketServer* internal, const SocketAddress& internal_addr,
      SocketServer* external, const SocketAddress external_addrs[4],
      NATType nat_type, bool exp_same) {
  Thread th_int(internal);
  Thread th_ext(external);

  SocketAddress server_addr = internal_addr;
  server_addr.SetPort(0);  // Auto-select a port
  NATServer* nat = new NATServer(
      nat_type, internal, server_addr, external, external_addrs[0]);
  NATSocketFactory* natsf = new NATSocketFactory(internal,
                                                 nat->internal_address());

  TestClient* in = CreateTestClient(natsf, internal_addr);
  TestClient* out[4];
  for (int i = 0; i < 4; i++)
    out[i] = CreateTestClient(external, external_addrs[i]);

  th_int.Start();
  th_ext.Start();

  const char* buf = "filter_test";
  size_t len = strlen(buf);

  in->SendTo(buf, len, out[0]->address());
  SocketAddress trans_addr;
  EXPECT_TRUE(out[0]->CheckNextPacket(buf, len, &trans_addr));

  for (int i = 1; i < 4; i++) {
    in->SendTo(buf, len, out[i]->address());
    SocketAddress trans_addr2;
    EXPECT_TRUE(out[i]->CheckNextPacket(buf, len, &trans_addr2));
    bool are_same = (trans_addr == trans_addr2);
    ASSERT_EQ(are_same, exp_same) << "same translated address";
    ASSERT_NE(AF_UNSPEC, trans_addr.family());
    ASSERT_NE(AF_UNSPEC, trans_addr2.family());
  }

  th_int.Stop();
  th_ext.Stop();

  delete nat;
  delete natsf;
  delete in;
  for (int i = 0; i < 4; i++)
    delete out[i];
}

// Tests that when sending from external_addrs to internal_addr, the packet
// is delivered according to the specified filter_ip and filter_port rules.
void TestRecv(
      SocketServer* internal, const SocketAddress& internal_addr,
      SocketServer* external, const SocketAddress external_addrs[4],
      NATType nat_type, bool filter_ip, bool filter_port) {
  Thread th_int(internal);
  Thread th_ext(external);

  SocketAddress server_addr = internal_addr;
  server_addr.SetPort(0);  // Auto-select a port
  NATServer* nat = new NATServer(
      nat_type, internal, server_addr, external, external_addrs[0]);
  NATSocketFactory* natsf = new NATSocketFactory(internal,
                                                 nat->internal_address());

  TestClient* in = CreateTestClient(natsf, internal_addr);
  TestClient* out[4];
  for (int i = 0; i < 4; i++)
    out[i] = CreateTestClient(external, external_addrs[i]);

  th_int.Start();
  th_ext.Start();

  const char* buf = "filter_test";
  size_t len = strlen(buf);

  in->SendTo(buf, len, out[0]->address());
  SocketAddress trans_addr;
  EXPECT_TRUE(out[0]->CheckNextPacket(buf, len, &trans_addr));

  out[1]->SendTo(buf, len, trans_addr);
  EXPECT_TRUE(CheckReceive(in, !filter_ip, buf, len));

  out[2]->SendTo(buf, len, trans_addr);
  EXPECT_TRUE(CheckReceive(in, !filter_port, buf, len));

  out[3]->SendTo(buf, len, trans_addr);
  EXPECT_TRUE(CheckReceive(in, !filter_ip && !filter_port, buf, len));

  th_int.Stop();
  th_ext.Stop();

  delete nat;
  delete natsf;
  delete in;
  for (int i = 0; i < 4; i++)
    delete out[i];
}

// Tests that NATServer allocates bindings properly.
void TestBindings(
    SocketServer* internal, const SocketAddress& internal_addr,
    SocketServer* external, const SocketAddress external_addrs[4]) {
  TestSend(internal, internal_addr, external, external_addrs,
           NAT_OPEN_CONE, true);
  TestSend(internal, internal_addr, external, external_addrs,
           NAT_ADDR_RESTRICTED, true);
  TestSend(internal, internal_addr, external, external_addrs,
           NAT_PORT_RESTRICTED, true);
  TestSend(internal, internal_addr, external, external_addrs,
           NAT_SYMMETRIC, false);
}

// Tests that NATServer filters packets properly.
void TestFilters(
    SocketServer* internal, const SocketAddress& internal_addr,
    SocketServer* external, const SocketAddress external_addrs[4]) {
  TestRecv(internal, internal_addr, external, external_addrs,
           NAT_OPEN_CONE, false, false);
  TestRecv(internal, internal_addr, external, external_addrs,
           NAT_ADDR_RESTRICTED, true, false);
  TestRecv(internal, internal_addr, external, external_addrs,
           NAT_PORT_RESTRICTED, true, true);
  TestRecv(internal, internal_addr, external, external_addrs,
           NAT_SYMMETRIC, true, true);
}

bool TestConnectivity(const SocketAddress& src, const IPAddress& dst) {
  // The physical NAT tests require connectivity to the selected ip from the
  // internal address used for the NAT. Things like firewalls can break that, so
  // check to see if it's worth even trying with this ip.
  scoped_ptr<PhysicalSocketServer> pss(new PhysicalSocketServer());
  scoped_ptr<AsyncSocket> client(pss->CreateAsyncSocket(src.family(),
                                                        SOCK_DGRAM));
  scoped_ptr<AsyncSocket> server(pss->CreateAsyncSocket(src.family(),
                                                        SOCK_DGRAM));
  if (client->Bind(SocketAddress(src.ipaddr(), 0)) != 0 ||
      server->Bind(SocketAddress(dst, 0)) != 0) {
    return false;
  }
  const char* buf = "hello other socket";
  size_t len = strlen(buf);
  int sent = client->SendTo(buf, len, server->GetLocalAddress());
  SocketAddress addr;
  const size_t kRecvBufSize = 64;
  char recvbuf[kRecvBufSize];
  Thread::Current()->SleepMs(100);
  int received = server->RecvFrom(recvbuf, kRecvBufSize, &addr);
  return received == sent && ::memcmp(buf, recvbuf, len) == 0;
}

void TestPhysicalInternal(const SocketAddress& int_addr) {
  BasicNetworkManager network_manager;
  network_manager.set_ipv6_enabled(true);
  network_manager.StartUpdating();
  // Process pending messages so the network list is updated.
  Thread::Current()->ProcessMessages(0);

  std::vector<Network*> networks;
  network_manager.GetNetworks(&networks);
  if (networks.empty()) {
    LOG(LS_WARNING) << "Not enough network adapters for test.";
    return;
  }

  SocketAddress ext_addr1(int_addr);
  SocketAddress ext_addr2;
  // Find an available IP with matching family. The test breaks if int_addr
  // can't talk to ip, so check for connectivity as well.
  for (std::vector<Network*>::iterator it = networks.begin();
      it != networks.end(); ++it) {
    const IPAddress& ip = (*it)->GetBestIP();
    if (ip.family() == int_addr.family() && TestConnectivity(int_addr, ip)) {
      ext_addr2.SetIP(ip);
      break;
    }
  }
  if (ext_addr2.IsNil()) {
    LOG(LS_WARNING) << "No available IP of same family as " << int_addr;
    return;
  }

  LOG(LS_INFO) << "selected ip " << ext_addr2.ipaddr();

  SocketAddress ext_addrs[4] = {
      SocketAddress(ext_addr1),
      SocketAddress(ext_addr2),
      SocketAddress(ext_addr1),
      SocketAddress(ext_addr2)
  };

  scoped_ptr<PhysicalSocketServer> int_pss(new PhysicalSocketServer());
  scoped_ptr<PhysicalSocketServer> ext_pss(new PhysicalSocketServer());

  TestBindings(int_pss.get(), int_addr, ext_pss.get(), ext_addrs);
  TestFilters(int_pss.get(), int_addr, ext_pss.get(), ext_addrs);
}

TEST(NatTest, TestPhysicalIPv4) {
  TestPhysicalInternal(SocketAddress("127.0.0.1", 0));
}

TEST(NatTest, TestPhysicalIPv6) {
  if (HasIPv6Enabled()) {
    TestPhysicalInternal(SocketAddress("::1", 0));
  } else {
    LOG(LS_WARNING) << "No IPv6, skipping";
  }
}

class TestVirtualSocketServer : public VirtualSocketServer {
 public:
  explicit TestVirtualSocketServer(SocketServer* ss)
      : VirtualSocketServer(ss),
        ss_(ss) {}
  // Expose this publicly
  IPAddress GetNextIP(int af) { return VirtualSocketServer::GetNextIP(af); }

 private:
  scoped_ptr<SocketServer> ss_;
};

void TestVirtualInternal(int family) {
  scoped_ptr<TestVirtualSocketServer> int_vss(new TestVirtualSocketServer(
      new PhysicalSocketServer()));
  scoped_ptr<TestVirtualSocketServer> ext_vss(new TestVirtualSocketServer(
      new PhysicalSocketServer()));

  SocketAddress int_addr;
  SocketAddress ext_addrs[4];
  int_addr.SetIP(int_vss->GetNextIP(family));
  ext_addrs[0].SetIP(ext_vss->GetNextIP(int_addr.family()));
  ext_addrs[1].SetIP(ext_vss->GetNextIP(int_addr.family()));
  ext_addrs[2].SetIP(ext_addrs[0].ipaddr());
  ext_addrs[3].SetIP(ext_addrs[1].ipaddr());

  TestBindings(int_vss.get(), int_addr, ext_vss.get(), ext_addrs);
  TestFilters(int_vss.get(), int_addr, ext_vss.get(), ext_addrs);
}

TEST(NatTest, TestVirtualIPv4) {
  TestVirtualInternal(AF_INET);
}

TEST(NatTest, TestVirtualIPv6) {
  if (HasIPv6Enabled()) {
    TestVirtualInternal(AF_INET6);
  } else {
    LOG(LS_WARNING) << "No IPv6, skipping";
  }
}

// TODO: Finish this test
class NatTcpTest : public testing::Test, public sigslot::has_slots<> {
 public:
  NatTcpTest() : connected_(false) {}
  virtual void SetUp() {
    int_vss_ = new TestVirtualSocketServer(new PhysicalSocketServer());
    ext_vss_ = new TestVirtualSocketServer(new PhysicalSocketServer());
    nat_ = new NATServer(NAT_OPEN_CONE, int_vss_, SocketAddress(),
                         ext_vss_, SocketAddress());
    natsf_ = new NATSocketFactory(int_vss_, nat_->internal_address());
  }
  void OnConnectEvent(AsyncSocket* socket) {
    connected_ = true;
  }
  void OnAcceptEvent(AsyncSocket* socket) {
    accepted_ = server_->Accept(NULL);
  }
  void OnCloseEvent(AsyncSocket* socket, int error) {
  }
  void ConnectEvents() {
    server_->SignalReadEvent.connect(this, &NatTcpTest::OnAcceptEvent);
    client_->SignalConnectEvent.connect(this, &NatTcpTest::OnConnectEvent);
  }
  TestVirtualSocketServer* int_vss_;
  TestVirtualSocketServer* ext_vss_;
  NATServer* nat_;
  NATSocketFactory* natsf_;
  AsyncSocket* client_;
  AsyncSocket* server_;
  AsyncSocket* accepted_;
  bool connected_;
};

TEST_F(NatTcpTest, DISABLED_TestConnectOut) {
  server_ = ext_vss_->CreateAsyncSocket(SOCK_STREAM);
  server_->Bind(SocketAddress());
  server_->Listen(5);

  client_ = int_vss_->CreateAsyncSocket(SOCK_STREAM);
  EXPECT_GE(0, client_->Bind(SocketAddress()));
  EXPECT_GE(0, client_->Connect(server_->GetLocalAddress()));


  ConnectEvents();

  EXPECT_TRUE_WAIT(connected_, 1000);
  EXPECT_EQ(client_->GetRemoteAddress(), server_->GetLocalAddress());
  EXPECT_EQ(client_->GetRemoteAddress(), accepted_->GetLocalAddress());
  EXPECT_EQ(client_->GetLocalAddress(), accepted_->GetRemoteAddress());

  client_->Close();
}
//#endif
