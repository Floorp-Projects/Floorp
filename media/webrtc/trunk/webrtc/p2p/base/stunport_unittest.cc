/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/stunport.h"
#include "webrtc/p2p/base/teststunserver.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/virtualsocketserver.h"

using cricket::ServerAddresses;
using rtc::SocketAddress;

static const SocketAddress kLocalAddr("127.0.0.1", 0);
static const SocketAddress kStunAddr1("127.0.0.1", 5000);
static const SocketAddress kStunAddr2("127.0.0.1", 4000);
static const SocketAddress kBadAddr("0.0.0.1", 5000);
static const SocketAddress kStunHostnameAddr("localhost", 5000);
static const SocketAddress kBadHostnameAddr("not-a-real-hostname", 5000);
static const int kTimeoutMs = 10000;
// stun prio = 100 << 24 | 30 (IPV4) << 8 | 256 - 0
static const uint32_t kStunCandidatePriority = 1677729535;

// Tests connecting a StunPort to a fake STUN server (cricket::StunServer)
// TODO: Use a VirtualSocketServer here. We have to use a
// PhysicalSocketServer right now since DNS is not part of SocketServer yet.
class StunPortTest : public testing::Test,
                     public sigslot::has_slots<> {
 public:
  StunPortTest()
      : pss_(new rtc::PhysicalSocketServer),
        ss_(new rtc::VirtualSocketServer(pss_.get())),
        ss_scope_(ss_.get()),
        network_("unittest", "unittest", rtc::IPAddress(INADDR_ANY), 32),
        socket_factory_(rtc::Thread::Current()),
        stun_server_1_(cricket::TestStunServer::Create(
          rtc::Thread::Current(), kStunAddr1)),
        stun_server_2_(cricket::TestStunServer::Create(
          rtc::Thread::Current(), kStunAddr2)),
        done_(false), error_(false), stun_keepalive_delay_(0) {
  }

  const cricket::Port* port() const { return stun_port_.get(); }
  bool done() const { return done_; }
  bool error() const { return error_; }

  void CreateStunPort(const rtc::SocketAddress& server_addr) {
    ServerAddresses stun_servers;
    stun_servers.insert(server_addr);
    CreateStunPort(stun_servers);
  }

  void CreateStunPort(const ServerAddresses& stun_servers) {
    stun_port_.reset(cricket::StunPort::Create(
        rtc::Thread::Current(), &socket_factory_, &network_,
        kLocalAddr.ipaddr(), 0, 0, rtc::CreateRandomString(16),
        rtc::CreateRandomString(22), stun_servers, std::string()));
    stun_port_->set_stun_keepalive_delay(stun_keepalive_delay_);
    stun_port_->SignalPortComplete.connect(this,
        &StunPortTest::OnPortComplete);
    stun_port_->SignalPortError.connect(this,
        &StunPortTest::OnPortError);
  }

  void CreateSharedStunPort(const rtc::SocketAddress& server_addr) {
    socket_.reset(socket_factory_.CreateUdpSocket(
        rtc::SocketAddress(kLocalAddr.ipaddr(), 0), 0, 0));
    ASSERT_TRUE(socket_ != NULL);
    socket_->SignalReadPacket.connect(this, &StunPortTest::OnReadPacket);
    stun_port_.reset(cricket::UDPPort::Create(
        rtc::Thread::Current(), &socket_factory_,
        &network_, socket_.get(),
        rtc::CreateRandomString(16), rtc::CreateRandomString(22),
        std::string(), false));
    ASSERT_TRUE(stun_port_ != NULL);
    ServerAddresses stun_servers;
    stun_servers.insert(server_addr);
    stun_port_->set_server_addresses(stun_servers);
    stun_port_->SignalPortComplete.connect(this,
        &StunPortTest::OnPortComplete);
    stun_port_->SignalPortError.connect(this,
        &StunPortTest::OnPortError);
  }

  void PrepareAddress() {
    stun_port_->PrepareAddress();
  }

  void OnReadPacket(rtc::AsyncPacketSocket* socket, const char* data,
                    size_t size, const rtc::SocketAddress& remote_addr,
                    const rtc::PacketTime& packet_time) {
    stun_port_->HandleIncomingPacket(
        socket, data, size, remote_addr, rtc::PacketTime());
  }

  void SendData(const char* data, size_t len) {
    stun_port_->HandleIncomingPacket(
        socket_.get(), data, len, rtc::SocketAddress("22.22.22.22", 0),
        rtc::PacketTime());
  }

 protected:
  static void SetUpTestCase() {
    // Ensure the RNG is inited.
    rtc::InitRandom(NULL, 0);

  }

  void OnPortComplete(cricket::Port* port) {
    ASSERT_FALSE(done_);
    done_ = true;
    error_ = false;
  }
  void OnPortError(cricket::Port* port) {
    done_ = true;
    error_ = true;
  }
  void SetKeepaliveDelay(int delay) {
    stun_keepalive_delay_ = delay;
  }

  cricket::TestStunServer* stun_server_1() {
    return stun_server_1_.get();
  }
  cricket::TestStunServer* stun_server_2() {
    return stun_server_2_.get();
  }

 private:
  rtc::scoped_ptr<rtc::PhysicalSocketServer> pss_;
  rtc::scoped_ptr<rtc::VirtualSocketServer> ss_;
  rtc::SocketServerScope ss_scope_;
  rtc::Network network_;
  rtc::BasicPacketSocketFactory socket_factory_;
  rtc::scoped_ptr<cricket::UDPPort> stun_port_;
  rtc::scoped_ptr<cricket::TestStunServer> stun_server_1_;
  rtc::scoped_ptr<cricket::TestStunServer> stun_server_2_;
  rtc::scoped_ptr<rtc::AsyncPacketSocket> socket_;
  bool done_;
  bool error_;
  int stun_keepalive_delay_;
};

// Test that we can create a STUN port
TEST_F(StunPortTest, TestBasic) {
  CreateStunPort(kStunAddr1);
  EXPECT_EQ("stun", port()->Type());
  EXPECT_EQ(0U, port()->Candidates().size());
}

// Test that we can get an address from a STUN server.
TEST_F(StunPortTest, TestPrepareAddress) {
  CreateStunPort(kStunAddr1);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kLocalAddr.EqualIPs(port()->Candidates()[0].address()));

  // TODO: Add IPv6 tests here, once either physicalsocketserver supports
  // IPv6, or this test is changed to use VirtualSocketServer.
}

// Test that we fail properly if we can't get an address.
TEST_F(StunPortTest, TestPrepareAddressFail) {
  CreateStunPort(kBadAddr);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  EXPECT_TRUE(error());
  EXPECT_EQ(0U, port()->Candidates().size());
}

// Test that we can get an address from a STUN server specified by a hostname.
TEST_F(StunPortTest, TestPrepareAddressHostname) {
  CreateStunPort(kStunHostnameAddr);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kLocalAddr.EqualIPs(port()->Candidates()[0].address()));
  EXPECT_EQ(kStunCandidatePriority, port()->Candidates()[0].priority());
}

// Test that we handle hostname lookup failures properly.
TEST_F(StunPortTest, TestPrepareAddressHostnameFail) {
  CreateStunPort(kBadHostnameAddr);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  EXPECT_TRUE(error());
  EXPECT_EQ(0U, port()->Candidates().size());
}

// This test verifies keepalive response messages don't result in
// additional candidate generation.
TEST_F(StunPortTest, TestKeepAliveResponse) {
  SetKeepaliveDelay(500);  // 500ms of keepalive delay.
  CreateStunPort(kStunHostnameAddr);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kLocalAddr.EqualIPs(port()->Candidates()[0].address()));
  // Waiting for 1 seond, which will allow us to process
  // response for keepalive binding request. 500 ms is the keepalive delay.
  rtc::Thread::Current()->ProcessMessages(1000);
  ASSERT_EQ(1U, port()->Candidates().size());
}

// Test that a local candidate can be generated using a shared socket.
TEST_F(StunPortTest, TestSharedSocketPrepareAddress) {
  CreateSharedStunPort(kStunAddr1);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kLocalAddr.EqualIPs(port()->Candidates()[0].address()));
}

// Test that we still a get a local candidate with invalid stun server hostname.
// Also verifing that UDPPort can receive packets when stun address can't be
// resolved.
TEST_F(StunPortTest, TestSharedSocketPrepareAddressInvalidHostname) {
  CreateSharedStunPort(kBadHostnameAddr);
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  ASSERT_EQ(1U, port()->Candidates().size());
  EXPECT_TRUE(kLocalAddr.EqualIPs(port()->Candidates()[0].address()));

  // Send data to port after it's ready. This is to make sure, UDP port can
  // handle data with unresolved stun server address.
  std::string data = "some random data, sending to cricket::Port.";
  SendData(data.c_str(), data.length());
  // No crash is success.
}

// Test that the same address is added only once if two STUN servers are in use.
TEST_F(StunPortTest, TestNoDuplicatedAddressWithTwoStunServers) {
  ServerAddresses stun_servers;
  stun_servers.insert(kStunAddr1);
  stun_servers.insert(kStunAddr2);
  CreateStunPort(stun_servers);
  EXPECT_EQ("stun", port()->Type());
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  EXPECT_EQ(1U, port()->Candidates().size());
  EXPECT_EQ(port()->Candidates()[0].relay_protocol(), "");
}

// Test that candidates can be allocated for multiple STUN servers, one of which
// is not reachable.
TEST_F(StunPortTest, TestMultipleStunServersWithBadServer) {
  ServerAddresses stun_servers;
  stun_servers.insert(kStunAddr1);
  stun_servers.insert(kBadAddr);
  CreateStunPort(stun_servers);
  EXPECT_EQ("stun", port()->Type());
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  EXPECT_EQ(1U, port()->Candidates().size());
}

// Test that two candidates are allocated if the two STUN servers return
// different mapped addresses.
TEST_F(StunPortTest, TestTwoCandidatesWithTwoStunServersAcrossNat) {
  const SocketAddress kStunMappedAddr1("77.77.77.77", 0);
  const SocketAddress kStunMappedAddr2("88.77.77.77", 0);
  stun_server_1()->set_fake_stun_addr(kStunMappedAddr1);
  stun_server_2()->set_fake_stun_addr(kStunMappedAddr2);

  ServerAddresses stun_servers;
  stun_servers.insert(kStunAddr1);
  stun_servers.insert(kStunAddr2);
  CreateStunPort(stun_servers);
  EXPECT_EQ("stun", port()->Type());
  PrepareAddress();
  EXPECT_TRUE_WAIT(done(), kTimeoutMs);
  EXPECT_EQ(2U, port()->Candidates().size());
  EXPECT_EQ(port()->Candidates()[0].relay_protocol(), "");
  EXPECT_EQ(port()->Candidates()[1].relay_protocol(), "");
}
