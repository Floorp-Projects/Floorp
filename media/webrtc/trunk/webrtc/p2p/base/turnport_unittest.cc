/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#if defined(WEBRTC_POSIX)
#include <dirent.h>
#endif

#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/constants.h"
#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/p2p/base/tcpport.h"
#include "webrtc/p2p/base/testturnserver.h"
#include "webrtc/p2p/base/turnport.h"
#include "webrtc/p2p/base/udpport.h"
#include "webrtc/base/asynctcpsocket.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/firewallsocketserver.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/virtualsocketserver.h"

using rtc::SocketAddress;
using cricket::Connection;
using cricket::Port;
using cricket::PortInterface;
using cricket::TurnPort;
using cricket::UDPPort;

static const SocketAddress kLocalAddr1("11.11.11.11", 0);
static const SocketAddress kLocalAddr2("22.22.22.22", 0);
static const SocketAddress kLocalIPv6Addr(
    "2401:fa00:4:1000:be30:5bff:fee5:c3", 0);
static const SocketAddress kTurnUdpIntAddr("99.99.99.3",
                                           cricket::TURN_SERVER_PORT);
static const SocketAddress kTurnTcpIntAddr("99.99.99.4",
                                           cricket::TURN_SERVER_PORT);
static const SocketAddress kTurnUdpExtAddr("99.99.99.5", 0);
static const SocketAddress kTurnAlternateIntAddr("99.99.99.6",
                                                 cricket::TURN_SERVER_PORT);
static const SocketAddress kTurnIntAddr("99.99.99.7",
                                        cricket::TURN_SERVER_PORT);
static const SocketAddress kTurnIPv6IntAddr(
    "2400:4030:2:2c00:be30:abcd:efab:cdef",
    cricket::TURN_SERVER_PORT);
static const SocketAddress kTurnUdpIPv6IntAddr(
    "2400:4030:1:2c00:be30:abcd:efab:cdef", cricket::TURN_SERVER_PORT);
static const SocketAddress kTurnUdpIPv6ExtAddr(
  "2620:0:1000:1b03:2e41:38ff:fea6:f2a4", 0);

static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIceUfrag2[] = "TESTICEUFRAG0002";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
static const char kIcePwd2[] = "TESTICEPWD00000000000002";
static const char kTurnUsername[] = "test";
static const char kTurnPassword[] = "test";
static const char kTestOrigin[] = "http://example.com";
static const unsigned int kTimeout = 1000;

static const cricket::ProtocolAddress kTurnUdpProtoAddr(
    kTurnUdpIntAddr, cricket::PROTO_UDP);
static const cricket::ProtocolAddress kTurnTcpProtoAddr(
    kTurnTcpIntAddr, cricket::PROTO_TCP);
static const cricket::ProtocolAddress kTurnUdpIPv6ProtoAddr(
    kTurnUdpIPv6IntAddr, cricket::PROTO_UDP);

static const unsigned int MSG_TESTFINISH = 0;

#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
static int GetFDCount() {
  struct dirent *dp;
  int fd_count = 0;
  DIR *dir = opendir("/proc/self/fd/");
  while ((dp = readdir(dir)) != NULL) {
    if (dp->d_name[0] == '.')
      continue;
    ++fd_count;
  }
  closedir(dir);
  return fd_count;
}
#endif

class TurnPortTestVirtualSocketServer : public rtc::VirtualSocketServer {
 public:
  explicit TurnPortTestVirtualSocketServer(SocketServer* ss)
      : VirtualSocketServer(ss) {}

  using rtc::VirtualSocketServer::LookupBinding;
};

class TestConnectionWrapper : public sigslot::has_slots<> {
 public:
  TestConnectionWrapper(Connection* conn) : connection_(conn) {
    conn->SignalDestroyed.connect(
        this, &TestConnectionWrapper::OnConnectionDestroyed);
  }

  Connection* connection() { return connection_; }

 private:
  void OnConnectionDestroyed(Connection* conn) {
    ASSERT_TRUE(conn == connection_);
    connection_ = nullptr;
  }

  Connection* connection_;
};

class TurnPortTest : public testing::Test,
                     public sigslot::has_slots<>,
                     public rtc::MessageHandler {
 public:
  TurnPortTest()
      : main_(rtc::Thread::Current()),
        pss_(new rtc::PhysicalSocketServer),
        ss_(new TurnPortTestVirtualSocketServer(pss_.get())),
        ss_scope_(ss_.get()),
        network_("unittest", "unittest", rtc::IPAddress(INADDR_ANY), 32),
        socket_factory_(rtc::Thread::Current()),
        turn_server_(main_, kTurnUdpIntAddr, kTurnUdpExtAddr),
        turn_ready_(false),
        turn_error_(false),
        turn_unknown_address_(false),
        turn_create_permission_success_(false),
        udp_ready_(false),
        test_finish_(false) {
    network_.AddIP(rtc::IPAddress(INADDR_ANY));
  }

  virtual void OnMessage(rtc::Message* msg) {
    ASSERT(msg->message_id == MSG_TESTFINISH);
    if (msg->message_id == MSG_TESTFINISH)
      test_finish_ = true;
  }

  void ConnectSignalAddressReadyToSetLocalhostAsAltenertativeLocalAddress() {
    rtc::AsyncPacketSocket* socket = turn_port_->socket();
    rtc::VirtualSocket* virtual_socket =
        ss_->LookupBinding(socket->GetLocalAddress());
    virtual_socket->SignalAddressReady.connect(
        this, &TurnPortTest::SetLocalhostAsAltenertativeLocalAddress);
  }

  void SetLocalhostAsAltenertativeLocalAddress(
      rtc::VirtualSocket* socket,
      const rtc::SocketAddress& address) {
    SocketAddress local_address("127.0.0.1", 2000);
    socket->SetAlternativeLocalAddress(local_address);
  }

  void OnTurnPortComplete(Port* port) {
    turn_ready_ = true;
  }
  void OnTurnPortError(Port* port) {
    turn_error_ = true;
  }
  void OnTurnUnknownAddress(PortInterface* port, const SocketAddress& addr,
                            cricket::ProtocolType proto,
                            cricket::IceMessage* msg, const std::string& rf,
                            bool /*port_muxed*/) {
    turn_unknown_address_ = true;
  }
  void OnTurnCreatePermissionResult(TurnPort* port,
                                    const SocketAddress& addr,
                                    int code) {
    // Ignoring the address.
    turn_create_permission_success_ = (code == 0);
  }

  void OnTurnRefreshResult(TurnPort* port, int code) {
    turn_refresh_success_ = (code == 0);
  }
  void OnTurnReadPacket(Connection* conn, const char* data, size_t size,
                        const rtc::PacketTime& packet_time) {
    turn_packets_.push_back(rtc::Buffer(data, size));
  }
  void OnUdpPortComplete(Port* port) {
    udp_ready_ = true;
  }
  void OnUdpReadPacket(Connection* conn, const char* data, size_t size,
                       const rtc::PacketTime& packet_time) {
    udp_packets_.push_back(rtc::Buffer(data, size));
  }
  void OnConnectionDestroyed(Connection* conn) { connection_destroyed_ = true; }
  void OnSocketReadPacket(rtc::AsyncPacketSocket* socket,
                          const char* data, size_t size,
                          const rtc::SocketAddress& remote_addr,
                          const rtc::PacketTime& packet_time) {
    turn_port_->HandleIncomingPacket(socket, data, size, remote_addr,
                                     packet_time);
  }
  rtc::AsyncSocket* CreateServerSocket(const SocketAddress addr) {
    rtc::AsyncSocket* socket = ss_->CreateAsyncSocket(SOCK_STREAM);
    EXPECT_GE(socket->Bind(addr), 0);
    EXPECT_GE(socket->Listen(5), 0);
    return socket;
  }

  void CreateTurnPort(const std::string& username,
                      const std::string& password,
                      const cricket::ProtocolAddress& server_address) {
    CreateTurnPort(kLocalAddr1, username, password, server_address);
  }
  void CreateTurnPort(const rtc::SocketAddress& local_address,
                      const std::string& username,
                      const std::string& password,
                      const cricket::ProtocolAddress& server_address) {
    cricket::RelayCredentials credentials(username, password);
    turn_port_.reset(TurnPort::Create(main_, &socket_factory_, &network_,
                                 local_address.ipaddr(), 0, 0,
                                 kIceUfrag1, kIcePwd1,
                                 server_address, credentials, 0,
                                 std::string()));
    // This TURN port will be the controlling.
    turn_port_->SetIceRole(cricket::ICEROLE_CONTROLLING);
    ConnectSignals();
  }

  // Should be identical to CreateTurnPort but specifies an origin value
  // when creating the instance of TurnPort.
  void CreateTurnPortWithOrigin(const rtc::SocketAddress& local_address,
                                const std::string& username,
                                const std::string& password,
                                const cricket::ProtocolAddress& server_address,
                                const std::string& origin) {
    cricket::RelayCredentials credentials(username, password);
    turn_port_.reset(TurnPort::Create(main_, &socket_factory_, &network_,
                                 local_address.ipaddr(), 0, 0,
                                 kIceUfrag1, kIcePwd1,
                                 server_address, credentials, 0,
                                 origin));
    // This TURN port will be the controlling.
    turn_port_->SetIceRole(cricket::ICEROLE_CONTROLLING);
    ConnectSignals();
  }

  void CreateSharedTurnPort(const std::string& username,
                            const std::string& password,
                            const cricket::ProtocolAddress& server_address) {
    ASSERT(server_address.proto == cricket::PROTO_UDP);

    if (!socket_) {
      socket_.reset(socket_factory_.CreateUdpSocket(
          rtc::SocketAddress(kLocalAddr1.ipaddr(), 0), 0, 0));
      ASSERT_TRUE(socket_ != NULL);
      socket_->SignalReadPacket.connect(
          this, &TurnPortTest::OnSocketReadPacket);
    }

    cricket::RelayCredentials credentials(username, password);
    turn_port_.reset(cricket::TurnPort::Create(
        main_, &socket_factory_, &network_, socket_.get(),
        kIceUfrag1, kIcePwd1, server_address, credentials, 0, std::string()));
    // This TURN port will be the controlling.
    turn_port_->SetIceRole(cricket::ICEROLE_CONTROLLING);
    ConnectSignals();
  }

  void ConnectSignals() {
    turn_port_->SignalPortComplete.connect(this,
        &TurnPortTest::OnTurnPortComplete);
    turn_port_->SignalPortError.connect(this,
        &TurnPortTest::OnTurnPortError);
    turn_port_->SignalUnknownAddress.connect(this,
        &TurnPortTest::OnTurnUnknownAddress);
    turn_port_->SignalCreatePermissionResult.connect(this,
        &TurnPortTest::OnTurnCreatePermissionResult);
    turn_port_->SignalTurnRefreshResult.connect(
        this, &TurnPortTest::OnTurnRefreshResult);
  }
  void ConnectConnectionDestroyedSignal(Connection* conn) {
    conn->SignalDestroyed.connect(this, &TurnPortTest::OnConnectionDestroyed);
  }

  void CreateUdpPort() { CreateUdpPort(kLocalAddr2); }

  void CreateUdpPort(const SocketAddress& address) {
    udp_port_.reset(UDPPort::Create(main_, &socket_factory_, &network_,
                                    address.ipaddr(), 0, 0, kIceUfrag2,
                                    kIcePwd2, std::string(), false));
    // UDP port will be controlled.
    udp_port_->SetIceRole(cricket::ICEROLE_CONTROLLED);
    udp_port_->SignalPortComplete.connect(
        this, &TurnPortTest::OnUdpPortComplete);
  }

  void PrepareTurnAndUdpPorts() {
    // turn_port_ should have been created.
    ASSERT_TRUE(turn_port_ != nullptr);
    turn_port_->PrepareAddress();
    ASSERT_TRUE_WAIT(turn_ready_, kTimeout);

    CreateUdpPort();
    udp_port_->PrepareAddress();
    ASSERT_TRUE_WAIT(udp_ready_, kTimeout);
  }

  bool CheckConnectionDestroyed() {
    turn_port_->FlushRequests(cricket::kAllRequests);
    rtc::Thread::Current()->ProcessMessages(50);
    return connection_destroyed_;
  }

  void TestTurnAlternateServer(cricket::ProtocolType protocol_type) {
    std::vector<rtc::SocketAddress> redirect_addresses;
    redirect_addresses.push_back(kTurnAlternateIntAddr);

    cricket::TestTurnRedirector redirector(redirect_addresses);

    turn_server_.AddInternalSocket(kTurnIntAddr, protocol_type);
    turn_server_.AddInternalSocket(kTurnAlternateIntAddr, protocol_type);
    turn_server_.set_redirect_hook(&redirector);
    CreateTurnPort(kTurnUsername, kTurnPassword,
                   cricket::ProtocolAddress(kTurnIntAddr, protocol_type));

    // Retrieve the address before we run the state machine.
    const SocketAddress old_addr = turn_port_->server_address().address;

    turn_port_->PrepareAddress();
    EXPECT_TRUE_WAIT(turn_ready_, kTimeout * 100);
    // Retrieve the address again, the turn port's address should be
    // changed.
    const SocketAddress new_addr = turn_port_->server_address().address;
    EXPECT_NE(old_addr, new_addr);
    ASSERT_EQ(1U, turn_port_->Candidates().size());
    EXPECT_EQ(kTurnUdpExtAddr.ipaddr(),
              turn_port_->Candidates()[0].address().ipaddr());
    EXPECT_NE(0, turn_port_->Candidates()[0].address().port());
  }

  void TestTurnAlternateServerV4toV6(cricket::ProtocolType protocol_type) {
    std::vector<rtc::SocketAddress> redirect_addresses;
    redirect_addresses.push_back(kTurnIPv6IntAddr);

    cricket::TestTurnRedirector redirector(redirect_addresses);
    turn_server_.AddInternalSocket(kTurnIntAddr, protocol_type);
    turn_server_.set_redirect_hook(&redirector);
    CreateTurnPort(kTurnUsername, kTurnPassword,
                   cricket::ProtocolAddress(kTurnIntAddr, protocol_type));
    turn_port_->PrepareAddress();
    EXPECT_TRUE_WAIT(turn_error_, kTimeout);
  }

  void TestTurnAlternateServerPingPong(cricket::ProtocolType protocol_type) {
    std::vector<rtc::SocketAddress> redirect_addresses;
    redirect_addresses.push_back(kTurnAlternateIntAddr);
    redirect_addresses.push_back(kTurnIntAddr);

    cricket::TestTurnRedirector redirector(redirect_addresses);

    turn_server_.AddInternalSocket(kTurnIntAddr, protocol_type);
    turn_server_.AddInternalSocket(kTurnAlternateIntAddr, protocol_type);
    turn_server_.set_redirect_hook(&redirector);
    CreateTurnPort(kTurnUsername, kTurnPassword,
                   cricket::ProtocolAddress(kTurnIntAddr, protocol_type));

    turn_port_->PrepareAddress();
    EXPECT_TRUE_WAIT(turn_error_, kTimeout);
    ASSERT_EQ(0U, turn_port_->Candidates().size());
    rtc::SocketAddress address;
    // Verify that we have exhausted all alternate servers instead of
    // failure caused by other errors.
    EXPECT_FALSE(redirector.ShouldRedirect(address, &address));
  }

  void TestTurnAlternateServerDetectRepetition(
      cricket::ProtocolType protocol_type) {
    std::vector<rtc::SocketAddress> redirect_addresses;
    redirect_addresses.push_back(kTurnAlternateIntAddr);
    redirect_addresses.push_back(kTurnAlternateIntAddr);

    cricket::TestTurnRedirector redirector(redirect_addresses);

    turn_server_.AddInternalSocket(kTurnIntAddr, protocol_type);
    turn_server_.AddInternalSocket(kTurnAlternateIntAddr, protocol_type);
    turn_server_.set_redirect_hook(&redirector);
    CreateTurnPort(kTurnUsername, kTurnPassword,
                   cricket::ProtocolAddress(kTurnIntAddr, protocol_type));

    turn_port_->PrepareAddress();
    EXPECT_TRUE_WAIT(turn_error_, kTimeout);
    ASSERT_EQ(0U, turn_port_->Candidates().size());
  }

  void TestTurnConnection() {
    // Create ports and prepare addresses.
    PrepareTurnAndUdpPorts();

    // Send ping from UDP to TURN.
    Connection* conn1 = udp_port_->CreateConnection(
                    turn_port_->Candidates()[0], Port::ORIGIN_MESSAGE);
    ASSERT_TRUE(conn1 != NULL);
    conn1->Ping(0);
    WAIT(!turn_unknown_address_, kTimeout);
    EXPECT_FALSE(turn_unknown_address_);
    EXPECT_FALSE(conn1->receiving());
    EXPECT_EQ(Connection::STATE_WRITE_INIT, conn1->write_state());

    // Send ping from TURN to UDP.
    Connection* conn2 = turn_port_->CreateConnection(
                    udp_port_->Candidates()[0], Port::ORIGIN_MESSAGE);
    ASSERT_TRUE(conn2 != NULL);
    ASSERT_TRUE_WAIT(turn_create_permission_success_, kTimeout);
    conn2->Ping(0);

    EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, conn2->write_state(), kTimeout);
    EXPECT_TRUE(conn1->receiving());
    EXPECT_TRUE(conn2->receiving());
    EXPECT_EQ(Connection::STATE_WRITE_INIT, conn1->write_state());

    // Send another ping from UDP to TURN.
    conn1->Ping(0);
    EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, conn1->write_state(), kTimeout);
    EXPECT_TRUE(conn2->receiving());
  }

  void TestDestroyTurnConnection() {
    PrepareTurnAndUdpPorts();

    // Create connections on both ends.
    Connection* conn1 = udp_port_->CreateConnection(turn_port_->Candidates()[0],
                                                    Port::ORIGIN_MESSAGE);
    Connection* conn2 = turn_port_->CreateConnection(udp_port_->Candidates()[0],
                                                     Port::ORIGIN_MESSAGE);
    ASSERT_TRUE(conn2 != NULL);
    ASSERT_TRUE_WAIT(turn_create_permission_success_, kTimeout);
    // Make sure turn connection can receive.
    conn1->Ping(0);
    EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, conn1->write_state(), kTimeout);
    EXPECT_FALSE(turn_unknown_address_);

    // Destroy the connection on the turn port. The TurnEntry is still
    // there. So the turn port gets ping from unknown address if it is pinged.
    conn2->Destroy();
    conn1->Ping(0);
    EXPECT_TRUE_WAIT(turn_unknown_address_, kTimeout);

    // Flush all requests in the invoker to destroy the TurnEntry.
    // Now the turn port cannot receive the ping.
    turn_unknown_address_ = false;
    turn_port_->invoker()->Flush(rtc::Thread::Current());
    conn1->Ping(0);
    rtc::Thread::Current()->ProcessMessages(500);
    EXPECT_FALSE(turn_unknown_address_);

    // If the connection is created again, it will start to receive pings.
    conn2 = turn_port_->CreateConnection(udp_port_->Candidates()[0],
                                         Port::ORIGIN_MESSAGE);
    conn1->Ping(0);
    EXPECT_TRUE_WAIT(conn2->receiving(), kTimeout);
    EXPECT_FALSE(turn_unknown_address_);
  }

  void TestTurnSendData() {
    PrepareTurnAndUdpPorts();

    // Create connections and send pings.
    Connection* conn1 = turn_port_->CreateConnection(
        udp_port_->Candidates()[0], Port::ORIGIN_MESSAGE);
    Connection* conn2 = udp_port_->CreateConnection(
        turn_port_->Candidates()[0], Port::ORIGIN_MESSAGE);
    ASSERT_TRUE(conn1 != NULL);
    ASSERT_TRUE(conn2 != NULL);
    conn1->SignalReadPacket.connect(static_cast<TurnPortTest*>(this),
                                    &TurnPortTest::OnTurnReadPacket);
    conn2->SignalReadPacket.connect(static_cast<TurnPortTest*>(this),
                                    &TurnPortTest::OnUdpReadPacket);
    conn1->Ping(0);
    EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, conn1->write_state(), kTimeout);
    conn2->Ping(0);
    EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, conn2->write_state(), kTimeout);

    // Send some data.
    size_t num_packets = 256;
    for (size_t i = 0; i < num_packets; ++i) {
      unsigned char buf[256] = { 0 };
      for (size_t j = 0; j < i + 1; ++j) {
        buf[j] = 0xFF - static_cast<unsigned char>(j);
      }
      conn1->Send(buf, i + 1, options);
      conn2->Send(buf, i + 1, options);
      main_->ProcessMessages(0);
    }

    // Check the data.
    ASSERT_EQ_WAIT(num_packets, turn_packets_.size(), kTimeout);
    ASSERT_EQ_WAIT(num_packets, udp_packets_.size(), kTimeout);
    for (size_t i = 0; i < num_packets; ++i) {
      EXPECT_EQ(i + 1, turn_packets_[i].size());
      EXPECT_EQ(i + 1, udp_packets_[i].size());
      EXPECT_EQ(turn_packets_[i], udp_packets_[i]);
    }
  }

 protected:
  rtc::Thread* main_;
  rtc::scoped_ptr<rtc::PhysicalSocketServer> pss_;
  rtc::scoped_ptr<TurnPortTestVirtualSocketServer> ss_;
  rtc::SocketServerScope ss_scope_;
  rtc::Network network_;
  rtc::BasicPacketSocketFactory socket_factory_;
  rtc::scoped_ptr<rtc::AsyncPacketSocket> socket_;
  cricket::TestTurnServer turn_server_;
  rtc::scoped_ptr<TurnPort> turn_port_;
  rtc::scoped_ptr<UDPPort> udp_port_;
  bool turn_ready_;
  bool turn_error_;
  bool turn_unknown_address_;
  bool turn_create_permission_success_;
  bool udp_ready_;
  bool test_finish_;
  bool turn_refresh_success_ = false;
  bool connection_destroyed_ = false;
  std::vector<rtc::Buffer> turn_packets_;
  std::vector<rtc::Buffer> udp_packets_;
  rtc::PacketOptions options;
};

// Do a normal TURN allocation.
TEST_F(TurnPortTest, TestTurnAllocate) {
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  EXPECT_EQ(0, turn_port_->SetOption(rtc::Socket::OPT_SNDBUF, 10*1024));
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);
  ASSERT_EQ(1U, turn_port_->Candidates().size());
  EXPECT_EQ(kTurnUdpExtAddr.ipaddr(),
            turn_port_->Candidates()[0].address().ipaddr());
  EXPECT_NE(0, turn_port_->Candidates()[0].address().port());
}

// Testing a normal UDP allocation using TCP connection.
TEST_F(TurnPortTest, TestTurnTcpAllocate) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnTcpProtoAddr);
  EXPECT_EQ(0, turn_port_->SetOption(rtc::Socket::OPT_SNDBUF, 10*1024));
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);
  ASSERT_EQ(1U, turn_port_->Candidates().size());
  EXPECT_EQ(kTurnUdpExtAddr.ipaddr(),
            turn_port_->Candidates()[0].address().ipaddr());
  EXPECT_NE(0, turn_port_->Candidates()[0].address().port());
}

// Test case for WebRTC issue 3927 where a proxy binds to the local host address
// instead the address that TurnPort originally bound to. The candidate pair
// impacted by this behavior should still be used.
TEST_F(TurnPortTest, TestTurnTcpAllocationWhenProxyChangesAddressToLocalHost) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnTcpProtoAddr);
  EXPECT_EQ(0, turn_port_->SetOption(rtc::Socket::OPT_SNDBUF, 10 * 1024));
  turn_port_->PrepareAddress();
  ConnectSignalAddressReadyToSetLocalhostAsAltenertativeLocalAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);
  ASSERT_EQ(1U, turn_port_->Candidates().size());
  EXPECT_EQ(kTurnUdpExtAddr.ipaddr(),
            turn_port_->Candidates()[0].address().ipaddr());
  EXPECT_NE(0, turn_port_->Candidates()[0].address().port());
}

// Testing turn port will attempt to create TCP socket on address resolution
// failure.
TEST_F(TurnPortTest, DISABLED_TestTurnTcpOnAddressResolveFailure) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  CreateTurnPort(kTurnUsername, kTurnPassword, cricket::ProtocolAddress(
      rtc::SocketAddress("www.webrtc-blah-blah.com", 3478),
      cricket::PROTO_TCP));
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_error_, kTimeout);
  // As VSS doesn't provide a DNS resolution, name resolve will fail. TurnPort
  // will proceed in creating a TCP socket which will fail as there is no
  // server on the above domain and error will be set to SOCKET_ERROR.
  EXPECT_EQ(SOCKET_ERROR, turn_port_->error());
}

// In case of UDP on address resolve failure, TurnPort will not create socket
// and return allocate failure.
TEST_F(TurnPortTest, DISABLED_TestTurnUdpOnAdressResolveFailure) {
  CreateTurnPort(kTurnUsername, kTurnPassword, cricket::ProtocolAddress(
      rtc::SocketAddress("www.webrtc-blah-blah.com", 3478),
      cricket::PROTO_UDP));
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_error_, kTimeout);
  // Error from turn port will not be socket error.
  EXPECT_NE(SOCKET_ERROR, turn_port_->error());
}

// Try to do a TURN allocation with an invalid password.
TEST_F(TurnPortTest, TestTurnAllocateBadPassword) {
  CreateTurnPort(kTurnUsername, "bad", kTurnUdpProtoAddr);
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_error_, kTimeout);
  ASSERT_EQ(0U, turn_port_->Candidates().size());
}

// Tests that a new local address is created after
// STUN_ERROR_ALLOCATION_MISMATCH.
TEST_F(TurnPortTest, TestTurnAllocateMismatch) {
  // Do a normal allocation first.
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);
  rtc::SocketAddress first_addr(turn_port_->socket()->GetLocalAddress());

  // Clear connected_ flag on turnport to suppress the release of
  // the allocation.
  turn_port_->OnSocketClose(turn_port_->socket(), 0);

  // Forces the socket server to assign the same port.
  ss_->SetNextPortForTesting(first_addr.port());

  turn_ready_ = false;
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  turn_port_->PrepareAddress();

  // Verifies that the new port has the same address.
  EXPECT_EQ(first_addr, turn_port_->socket()->GetLocalAddress());

  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);

  // Verifies that the new port has a different address now.
  EXPECT_NE(first_addr, turn_port_->socket()->GetLocalAddress());
}

// Tests that a shared-socket-TurnPort creates its own socket after
// STUN_ERROR_ALLOCATION_MISMATCH.
TEST_F(TurnPortTest, TestSharedSocketAllocateMismatch) {
  // Do a normal allocation first.
  CreateSharedTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);
  rtc::SocketAddress first_addr(turn_port_->socket()->GetLocalAddress());

  // Clear connected_ flag on turnport to suppress the release of
  // the allocation.
  turn_port_->OnSocketClose(turn_port_->socket(), 0);

  turn_ready_ = false;
  CreateSharedTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);

  // Verifies that the new port has the same address.
  EXPECT_EQ(first_addr, turn_port_->socket()->GetLocalAddress());
  EXPECT_TRUE(turn_port_->SharedSocket());

  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);

  // Verifies that the new port has a different address now.
  EXPECT_NE(first_addr, turn_port_->socket()->GetLocalAddress());
  EXPECT_FALSE(turn_port_->SharedSocket());
}

TEST_F(TurnPortTest, TestTurnTcpAllocateMismatch) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnTcpProtoAddr);

  // Do a normal allocation first.
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);
  rtc::SocketAddress first_addr(turn_port_->socket()->GetLocalAddress());

  // Clear connected_ flag on turnport to suppress the release of
  // the allocation.
  turn_port_->OnSocketClose(turn_port_->socket(), 0);

  // Forces the socket server to assign the same port.
  ss_->SetNextPortForTesting(first_addr.port());

  turn_ready_ = false;
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnTcpProtoAddr);
  turn_port_->PrepareAddress();

  // Verifies that the new port has the same address.
  EXPECT_EQ(first_addr, turn_port_->socket()->GetLocalAddress());

  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);

  // Verifies that the new port has a different address now.
  EXPECT_NE(first_addr, turn_port_->socket()->GetLocalAddress());
}

TEST_F(TurnPortTest, TestRefreshRequestGetsErrorResponse) {
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  PrepareTurnAndUdpPorts();
  turn_port_->CreateConnection(udp_port_->Candidates()[0],
                               Port::ORIGIN_MESSAGE);
  // Set bad credentials.
  cricket::RelayCredentials bad_credentials("bad_user", "bad_pwd");
  turn_port_->set_credentials(bad_credentials);
  turn_refresh_success_ = false;
  // This sends out the first RefreshRequest with correct credentials.
  // When this succeeds, it will schedule a new RefreshRequest with the bad
  // credential.
  turn_port_->FlushRequests(cricket::TURN_REFRESH_REQUEST);
  EXPECT_TRUE_WAIT(turn_refresh_success_, kTimeout);
  // Flush it again, it will receive a bad response.
  turn_port_->FlushRequests(cricket::TURN_REFRESH_REQUEST);
  EXPECT_TRUE_WAIT(!turn_refresh_success_, kTimeout);
  EXPECT_TRUE_WAIT(!turn_port_->connected(), kTimeout);
  EXPECT_TRUE_WAIT(turn_port_->connections().empty(), kTimeout);
  EXPECT_FALSE(turn_port_->HasRequests());
}

// Test that CreateConnection will return null if port becomes disconnected.
TEST_F(TurnPortTest, TestCreateConnectionWhenSocketClosed) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnTcpProtoAddr);
  PrepareTurnAndUdpPorts();
  // Create a connection.
  Connection* conn1 = turn_port_->CreateConnection(udp_port_->Candidates()[0],
                                                   Port::ORIGIN_MESSAGE);
  ASSERT_TRUE(conn1 != NULL);

  // Close the socket and create a connection again.
  turn_port_->OnSocketClose(turn_port_->socket(), 1);
  conn1 = turn_port_->CreateConnection(udp_port_->Candidates()[0],
                                       Port::ORIGIN_MESSAGE);
  ASSERT_TRUE(conn1 == NULL);
}

// Test try-alternate-server feature.
TEST_F(TurnPortTest, TestTurnAlternateServerUDP) {
  TestTurnAlternateServer(cricket::PROTO_UDP);
}

TEST_F(TurnPortTest, TestTurnAlternateServerTCP) {
  TestTurnAlternateServer(cricket::PROTO_TCP);
}

// Test that we fail when we redirect to an address different from
// current IP family.
TEST_F(TurnPortTest, TestTurnAlternateServerV4toV6UDP) {
  TestTurnAlternateServerV4toV6(cricket::PROTO_UDP);
}

TEST_F(TurnPortTest, TestTurnAlternateServerV4toV6TCP) {
  TestTurnAlternateServerV4toV6(cricket::PROTO_TCP);
}

// Test try-alternate-server catches the case of pingpong.
TEST_F(TurnPortTest, TestTurnAlternateServerPingPongUDP) {
  TestTurnAlternateServerPingPong(cricket::PROTO_UDP);
}

TEST_F(TurnPortTest, TestTurnAlternateServerPingPongTCP) {
  TestTurnAlternateServerPingPong(cricket::PROTO_TCP);
}

// Test try-alternate-server catch the case of repeated server.
TEST_F(TurnPortTest, TestTurnAlternateServerDetectRepetitionUDP) {
  TestTurnAlternateServerDetectRepetition(cricket::PROTO_UDP);
}

TEST_F(TurnPortTest, TestTurnAlternateServerDetectRepetitionTCP) {
  TestTurnAlternateServerDetectRepetition(cricket::PROTO_TCP);
}

// Do a TURN allocation and try to send a packet to it from the outside.
// The packet should be dropped. Then, try to send a packet from TURN to the
// outside. It should reach its destination. Finally, try again from the
// outside. It should now work as well.
TEST_F(TurnPortTest, TestTurnConnection) {
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  TestTurnConnection();
}

// Similar to above, except that this test will use the shared socket.
TEST_F(TurnPortTest, TestTurnConnectionUsingSharedSocket) {
  CreateSharedTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  TestTurnConnection();
}

// Test that we can establish a TCP connection with TURN server.
TEST_F(TurnPortTest, TestTurnTcpConnection) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnTcpProtoAddr);
  TestTurnConnection();
}

// Test that if a connection on a TURN port is destroyed, the TURN port can
// still receive ping on that connection as if it is from an unknown address.
// If the connection is created again, it will be used to receive ping.
TEST_F(TurnPortTest, TestDestroyTurnConnection) {
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  TestDestroyTurnConnection();
}

// Similar to above, except that this test will use the shared socket.
TEST_F(TurnPortTest, TestDestroyTurnConnectionUsingSharedSocket) {
  CreateSharedTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  TestDestroyTurnConnection();
}

// Test that we fail to create a connection when we want to use TLS over TCP.
// This test should be removed once we have TLS support.
TEST_F(TurnPortTest, TestTurnTlsTcpConnectionFails) {
  cricket::ProtocolAddress secure_addr(kTurnTcpProtoAddr.address,
                                       kTurnTcpProtoAddr.proto,
                                       true);
  CreateTurnPort(kTurnUsername, kTurnPassword, secure_addr);
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_error_, kTimeout);
  ASSERT_EQ(0U, turn_port_->Candidates().size());
}

// Run TurnConnectionTest with one-time-use nonce feature.
// Here server will send a 438 STALE_NONCE error message for
// every TURN transaction.
TEST_F(TurnPortTest, TestTurnConnectionUsingOTUNonce) {
  turn_server_.set_enable_otu_nonce(true);
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  TestTurnConnection();
}

// Test that CreatePermissionRequest will be scheduled after the success
// of the first create permission request and the request will get an
// ErrorResponse if the ufrag and pwd are incorrect.
TEST_F(TurnPortTest, TestRefreshCreatePermissionRequest) {
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  PrepareTurnAndUdpPorts();

  Connection* conn = turn_port_->CreateConnection(udp_port_->Candidates()[0],
                                                  Port::ORIGIN_MESSAGE);
  ConnectConnectionDestroyedSignal(conn);
  ASSERT_TRUE(conn != NULL);
  ASSERT_TRUE_WAIT(turn_create_permission_success_, kTimeout);
  turn_create_permission_success_ = false;
  // A create-permission-request should be pending.
  // After the next create-permission-response is received, it will schedule
  // another request with bad_ufrag and bad_pwd.
  cricket::RelayCredentials bad_credentials("bad_user", "bad_pwd");
  turn_port_->set_credentials(bad_credentials);
  turn_port_->FlushRequests(cricket::kAllRequests);
  ASSERT_TRUE_WAIT(turn_create_permission_success_, kTimeout);
  // Flush the requests again; the create-permission-request will fail.
  turn_port_->FlushRequests(cricket::kAllRequests);
  EXPECT_TRUE_WAIT(!turn_create_permission_success_, kTimeout);
  EXPECT_TRUE_WAIT(connection_destroyed_, kTimeout);
}

TEST_F(TurnPortTest, TestChannelBindGetErrorResponse) {
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  PrepareTurnAndUdpPorts();
  Connection* conn1 = turn_port_->CreateConnection(udp_port_->Candidates()[0],
                                                   Port::ORIGIN_MESSAGE);
  ASSERT_TRUE(conn1 != nullptr);
  Connection* conn2 = udp_port_->CreateConnection(turn_port_->Candidates()[0],
                                                  Port::ORIGIN_MESSAGE);
  ASSERT_TRUE(conn2 != nullptr);
  ConnectConnectionDestroyedSignal(conn1);
  conn1->Ping(0);
  ASSERT_TRUE_WAIT(conn1->writable(), kTimeout);

  std::string data = "ABC";
  conn1->Send(data.data(), data.length(), options);
  bool success =
      turn_port_->SetEntryChannelId(udp_port_->Candidates()[0].address(), -1);
  ASSERT_TRUE(success);
  // Next time when the binding request is sent, it will get an ErrorResponse.
  EXPECT_TRUE_WAIT(CheckConnectionDestroyed(), kTimeout);
}

// Do a TURN allocation, establish a UDP connection, and send some data.
TEST_F(TurnPortTest, TestTurnSendDataTurnUdpToUdp) {
  // Create ports and prepare addresses.
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  TestTurnSendData();
  EXPECT_EQ(cricket::UDP_PROTOCOL_NAME,
            turn_port_->Candidates()[0].relay_protocol());
}

// Do a TURN allocation, establish a TCP connection, and send some data.
TEST_F(TurnPortTest, TestTurnSendDataTurnTcpToUdp) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  // Create ports and prepare addresses.
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnTcpProtoAddr);
  TestTurnSendData();
  EXPECT_EQ(cricket::TCP_PROTOCOL_NAME,
            turn_port_->Candidates()[0].relay_protocol());
}

// Test TURN fails to make a connection from IPv6 address to a server which has
// IPv4 address.
TEST_F(TurnPortTest, TestTurnLocalIPv6AddressServerIPv4) {
  turn_server_.AddInternalSocket(kTurnUdpIPv6IntAddr, cricket::PROTO_UDP);
  CreateTurnPort(kLocalIPv6Addr, kTurnUsername, kTurnPassword,
                 kTurnUdpProtoAddr);
  turn_port_->PrepareAddress();
  ASSERT_TRUE_WAIT(turn_error_, kTimeout);
  EXPECT_TRUE(turn_port_->Candidates().empty());
}

// Test TURN make a connection from IPv6 address to a server which has
// IPv6 intenal address. But in this test external address is a IPv4 address,
// hence allocated address will be a IPv4 address.
TEST_F(TurnPortTest, TestTurnLocalIPv6AddressServerIPv6ExtenalIPv4) {
  turn_server_.AddInternalSocket(kTurnUdpIPv6IntAddr, cricket::PROTO_UDP);
  CreateTurnPort(kLocalIPv6Addr, kTurnUsername, kTurnPassword,
                 kTurnUdpIPv6ProtoAddr);
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);
  ASSERT_EQ(1U, turn_port_->Candidates().size());
  EXPECT_EQ(kTurnUdpExtAddr.ipaddr(),
            turn_port_->Candidates()[0].address().ipaddr());
  EXPECT_NE(0, turn_port_->Candidates()[0].address().port());
}

TEST_F(TurnPortTest, TestOriginHeader) {
  CreateTurnPortWithOrigin(kLocalAddr1, kTurnUsername, kTurnPassword,
                           kTurnUdpProtoAddr, kTestOrigin);
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);
  ASSERT_GT(turn_server_.server()->allocations().size(), 0U);
  SocketAddress local_address = turn_port_->GetLocalAddress();
  ASSERT_TRUE(turn_server_.FindAllocation(local_address) != NULL);
  EXPECT_EQ(kTestOrigin, turn_server_.FindAllocation(local_address)->origin());
}

// Test that a CreatePermission failure will result in the connection being
// destroyed.
TEST_F(TurnPortTest, TestConnectionDestroyedOnCreatePermissionFailure) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  turn_server_.server()->set_reject_private_addresses(true);
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnTcpProtoAddr);
  turn_port_->PrepareAddress();
  ASSERT_TRUE_WAIT(turn_ready_, kTimeout);

  CreateUdpPort(SocketAddress("10.0.0.10", 0));
  udp_port_->PrepareAddress();
  ASSERT_TRUE_WAIT(udp_ready_, kTimeout);
  // Create a connection.
  TestConnectionWrapper conn(turn_port_->CreateConnection(
      udp_port_->Candidates()[0], Port::ORIGIN_MESSAGE));
  ASSERT_TRUE(conn.connection() != nullptr);

  // Asynchronously, CreatePermission request should be sent and fail, closing
  // the connection.
  EXPECT_TRUE_WAIT(conn.connection() == nullptr, kTimeout);
  EXPECT_FALSE(turn_create_permission_success_);
}

// Test that a TURN allocation is released when the port is closed.
TEST_F(TurnPortTest, TestTurnReleaseAllocation) {
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);

  ASSERT_GT(turn_server_.server()->allocations().size(), 0U);
  turn_port_.reset();
  EXPECT_EQ_WAIT(0U, turn_server_.server()->allocations().size(), kTimeout);
}

// Test that a TURN TCP allocation is released when the port is closed.
TEST_F(TurnPortTest, DISABLED_TestTurnTCPReleaseAllocation) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnTcpProtoAddr);
  turn_port_->PrepareAddress();
  EXPECT_TRUE_WAIT(turn_ready_, kTimeout);

  ASSERT_GT(turn_server_.server()->allocations().size(), 0U);
  turn_port_.reset();
  EXPECT_EQ_WAIT(0U, turn_server_.server()->allocations().size(), kTimeout);
}

// This test verifies any FD's are not leaked after TurnPort is destroyed.
// https://code.google.com/p/webrtc/issues/detail?id=2651
#if defined(WEBRTC_LINUX) && !defined(WEBRTC_ANDROID)
// 1 second is not always enough for getaddrinfo().
// See: https://bugs.chromium.org/p/webrtc/issues/detail?id=5191
static const unsigned int kResolverTimeout = 10000;

TEST_F(TurnPortTest, TestResolverShutdown) {
  turn_server_.AddInternalSocket(kTurnUdpIPv6IntAddr, cricket::PROTO_UDP);
  int last_fd_count = GetFDCount();
  // Need to supply unresolved address to kick off resolver.
  CreateTurnPort(kLocalIPv6Addr, kTurnUsername, kTurnPassword,
                 cricket::ProtocolAddress(rtc::SocketAddress(
                    "www.google.invalid", 3478), cricket::PROTO_UDP));
  turn_port_->PrepareAddress();
  ASSERT_TRUE_WAIT(turn_error_, kResolverTimeout);
  EXPECT_TRUE(turn_port_->Candidates().empty());
  turn_port_.reset();
  rtc::Thread::Current()->Post(this, MSG_TESTFINISH);
  // Waiting for above message to be processed.
  ASSERT_TRUE_WAIT(test_finish_, kTimeout);
  EXPECT_EQ(last_fd_count, GetFDCount());
}
#endif
