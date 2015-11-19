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
  void OnTurnCreatePermissionResult(TurnPort* port, const SocketAddress& addr,
                                     int code) {
    // Ignoring the address.
    if (code == 0) {
      turn_create_permission_success_ = true;
    }
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
    // Set ICE protocol type to ICEPROTO_RFC5245, as port by default will be
    // in Hybrid mode. Protocol type is necessary to send correct type STUN ping
    // messages.
    // This TURN port will be the controlling.
    turn_port_->SetIceProtocolType(cricket::ICEPROTO_RFC5245);
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
    // Set ICE protocol type to ICEPROTO_RFC5245, as port by default will be
    // in Hybrid mode. Protocol type is necessary to send correct type STUN ping
    // messages.
    // This TURN port will be the controlling.
    turn_port_->SetIceProtocolType(cricket::ICEPROTO_RFC5245);
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
    // Set ICE protocol type to ICEPROTO_RFC5245, as port by default will be
    // in Hybrid mode. Protocol type is necessary to send correct type STUN ping
    // messages.
    // This TURN port will be the controlling.
    turn_port_->SetIceProtocolType(cricket::ICEPROTO_RFC5245);
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
  }
  void CreateUdpPort() {
    udp_port_.reset(UDPPort::Create(main_, &socket_factory_, &network_,
                                    kLocalAddr2.ipaddr(), 0, 0,
                                    kIceUfrag2, kIcePwd2,
                                    std::string()));
    // Set protocol type to RFC5245, as turn port is also in same mode.
    // UDP port will be controlled.
    udp_port_->SetIceProtocolType(cricket::ICEPROTO_RFC5245);
    udp_port_->SetIceRole(cricket::ICEROLE_CONTROLLED);
    udp_port_->SignalPortComplete.connect(
        this, &TurnPortTest::OnUdpPortComplete);
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
    ASSERT_TRUE(turn_port_ != NULL);
    turn_port_->PrepareAddress();
    ASSERT_TRUE_WAIT(turn_ready_, kTimeout);
    CreateUdpPort();
    udp_port_->PrepareAddress();
    ASSERT_TRUE_WAIT(udp_ready_, kTimeout);

    // Send ping from UDP to TURN.
    Connection* conn1 = udp_port_->CreateConnection(
                    turn_port_->Candidates()[0], Port::ORIGIN_MESSAGE);
    ASSERT_TRUE(conn1 != NULL);
    conn1->Ping(0);
    WAIT(!turn_unknown_address_, kTimeout);
    EXPECT_FALSE(turn_unknown_address_);
    EXPECT_EQ(Connection::STATE_READ_INIT, conn1->read_state());
    EXPECT_EQ(Connection::STATE_WRITE_INIT, conn1->write_state());

    // Send ping from TURN to UDP.
    Connection* conn2 = turn_port_->CreateConnection(
                    udp_port_->Candidates()[0], Port::ORIGIN_MESSAGE);
    ASSERT_TRUE(conn2 != NULL);
    ASSERT_TRUE_WAIT(turn_create_permission_success_, kTimeout);
    conn2->Ping(0);

    EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, conn2->write_state(), kTimeout);
    EXPECT_EQ(Connection::STATE_READABLE, conn1->read_state());
    EXPECT_EQ(Connection::STATE_READ_INIT, conn2->read_state());
    EXPECT_EQ(Connection::STATE_WRITE_INIT, conn1->write_state());

    // Send another ping from UDP to TURN.
    conn1->Ping(0);
    EXPECT_EQ_WAIT(Connection::STATE_WRITABLE, conn1->write_state(), kTimeout);
    EXPECT_EQ(Connection::STATE_READABLE, conn2->read_state());
  }

  void TestTurnSendData() {
    turn_port_->PrepareAddress();
    EXPECT_TRUE_WAIT(turn_ready_, kTimeout);
    CreateUdpPort();
    udp_port_->PrepareAddress();
    EXPECT_TRUE_WAIT(udp_ready_, kTimeout);
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

// Do a TURN allocation, establish a UDP connection, and send some data.
TEST_F(TurnPortTest, TestTurnSendDataTurnUdpToUdp) {
  // Create ports and prepare addresses.
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnUdpProtoAddr);
  TestTurnSendData();
}

// Do a TURN allocation, establish a TCP connection, and send some data.
TEST_F(TurnPortTest, TestTurnSendDataTurnTcpToUdp) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  // Create ports and prepare addresses.
  CreateTurnPort(kTurnUsername, kTurnPassword, kTurnTcpProtoAddr);
  TestTurnSendData();
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
TEST_F(TurnPortTest, TestResolverShutdown) {
  turn_server_.AddInternalSocket(kTurnUdpIPv6IntAddr, cricket::PROTO_UDP);
  int last_fd_count = GetFDCount();
  // Need to supply unresolved address to kick off resolver.
  CreateTurnPort(kLocalIPv6Addr, kTurnUsername, kTurnPassword,
                 cricket::ProtocolAddress(rtc::SocketAddress(
                    "stun.l.google.com", 3478), cricket::PROTO_UDP));
  turn_port_->PrepareAddress();
  ASSERT_TRUE_WAIT(turn_error_, kTimeout);
  EXPECT_TRUE(turn_port_->Candidates().empty());
  turn_port_.reset();
  rtc::Thread::Current()->Post(this, MSG_TESTFINISH);
  // Waiting for above message to be processed.
  ASSERT_TRUE_WAIT(test_finish_, kTimeout);
  EXPECT_EQ(last_fd_count, GetFDCount());
}
#endif
