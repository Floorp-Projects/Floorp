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
#include "webrtc/p2p/base/constants.h"
#include "webrtc/p2p/base/p2ptransportchannel.h"
#include "webrtc/p2p/base/portallocatorsessionproxy.h"
#include "webrtc/p2p/base/testrelayserver.h"
#include "webrtc/p2p/base/teststunserver.h"
#include "webrtc/p2p/base/testturnserver.h"
#include "webrtc/p2p/client/basicportallocator.h"
#include "webrtc/p2p/client/httpportallocator.h"
#include "webrtc/base/fakenetwork.h"
#include "webrtc/base/firewallsocketserver.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/natserver.h"
#include "webrtc/base/natsocketfactory.h"
#include "webrtc/base/network.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/virtualsocketserver.h"

using cricket::ServerAddresses;
using rtc::SocketAddress;
using rtc::Thread;

static const SocketAddress kClientAddr("11.11.11.11", 0);
static const SocketAddress kPrivateAddr("192.168.1.11", 0);
static const SocketAddress kPrivateAddr2("192.168.1.12", 0);
static const SocketAddress kClientIPv6Addr(
    "2401:fa00:4:1000:be30:5bff:fee5:c3", 0);
static const SocketAddress kClientAddr2("22.22.22.22", 0);
static const SocketAddress kNatAddr("77.77.77.77", rtc::NAT_SERVER_PORT);
static const SocketAddress kRemoteClientAddr("22.22.22.22", 0);
static const SocketAddress kStunAddr("99.99.99.1", cricket::STUN_SERVER_PORT);
static const SocketAddress kRelayUdpIntAddr("99.99.99.2", 5000);
static const SocketAddress kRelayUdpExtAddr("99.99.99.3", 5001);
static const SocketAddress kRelayTcpIntAddr("99.99.99.2", 5002);
static const SocketAddress kRelayTcpExtAddr("99.99.99.3", 5003);
static const SocketAddress kRelaySslTcpIntAddr("99.99.99.2", 5004);
static const SocketAddress kRelaySslTcpExtAddr("99.99.99.3", 5005);
static const SocketAddress kTurnUdpIntAddr("99.99.99.4", 3478);
static const SocketAddress kTurnTcpIntAddr("99.99.99.5", 3478);
static const SocketAddress kTurnUdpExtAddr("99.99.99.6", 0);

// Minimum and maximum port for port range tests.
static const int kMinPort = 10000;
static const int kMaxPort = 10099;

// Based on ICE_UFRAG_LENGTH
static const char kIceUfrag0[] = "TESTICEUFRAG0000";
// Based on ICE_PWD_LENGTH
static const char kIcePwd0[] = "TESTICEPWD00000000000000";

static const char kContentName[] = "test content";

static const int kDefaultAllocationTimeout = 1000;
static const char kTurnUsername[] = "test";
static const char kTurnPassword[] = "test";

namespace cricket {

// Helper for dumping candidates
std::ostream& operator<<(std::ostream& os, const cricket::Candidate& c) {
  os << c.ToString();
  return os;
}

}  // namespace cricket

class PortAllocatorTest : public testing::Test, public sigslot::has_slots<> {
 public:
  PortAllocatorTest()
      : pss_(new rtc::PhysicalSocketServer),
        vss_(new rtc::VirtualSocketServer(pss_.get())),
        fss_(new rtc::FirewallSocketServer(vss_.get())),
        ss_scope_(fss_.get()),
        nat_factory_(vss_.get(), kNatAddr),
        nat_socket_factory_(&nat_factory_),
        stun_server_(cricket::TestStunServer::Create(Thread::Current(),
                                                     kStunAddr)),
        relay_server_(Thread::Current(), kRelayUdpIntAddr, kRelayUdpExtAddr,
                      kRelayTcpIntAddr, kRelayTcpExtAddr,
                      kRelaySslTcpIntAddr, kRelaySslTcpExtAddr),
        turn_server_(Thread::Current(), kTurnUdpIntAddr, kTurnUdpExtAddr),
        candidate_allocation_done_(false) {
    cricket::ServerAddresses stun_servers;
    stun_servers.insert(kStunAddr);
    // Passing the addresses of GTURN servers will enable GTURN in
    // Basicportallocator.
    allocator_.reset(new cricket::BasicPortAllocator(
        &network_manager_,
        stun_servers,
        kRelayUdpIntAddr, kRelayTcpIntAddr, kRelaySslTcpIntAddr));
    allocator_->set_step_delay(cricket::kMinimumStepDelay);
  }

  void AddInterface(const SocketAddress& addr) {
    network_manager_.AddInterface(addr);
  }
  bool SetPortRange(int min_port, int max_port) {
    return allocator_->SetPortRange(min_port, max_port);
  }
  void ResetWithNatServer(const rtc::SocketAddress& stun_server) {
    nat_server_.reset(new rtc::NATServer(
        rtc::NAT_OPEN_CONE, vss_.get(), kNatAddr, vss_.get(), kNatAddr));

    ServerAddresses stun_servers;
    stun_servers.insert(stun_server);
    allocator_.reset(new cricket::BasicPortAllocator(
        &network_manager_, &nat_socket_factory_, stun_servers));
    allocator().set_step_delay(cricket::kMinimumStepDelay);
  }

  // Create a BasicPortAllocator without GTURN and add the TURN servers.
  void ResetWithTurnServers(const rtc::SocketAddress& udp_turn,
                            const rtc::SocketAddress& tcp_turn) {
    allocator_.reset(new cricket::BasicPortAllocator(&network_manager_));
    allocator().set_step_delay(cricket::kMinimumStepDelay);
    AddTurnServers(udp_turn, tcp_turn);
  }

  void AddTurnServers(const rtc::SocketAddress& udp_turn,
                      const rtc::SocketAddress& tcp_turn) {
    cricket::RelayServerConfig relay_server(cricket::RELAY_TURN);
    cricket::RelayCredentials credentials(kTurnUsername, kTurnPassword);
    relay_server.credentials = credentials;

    if (!udp_turn.IsNil()) {
      relay_server.ports.push_back(cricket::ProtocolAddress(
          kTurnUdpIntAddr, cricket::PROTO_UDP, false));
    }
    if (!tcp_turn.IsNil()) {
      relay_server.ports.push_back(cricket::ProtocolAddress(
          kTurnTcpIntAddr, cricket::PROTO_TCP, false));
    }
    allocator_->AddRelay(relay_server);
  }

  bool CreateSession(int component) {
    session_.reset(CreateSession("session", component));
    if (!session_)
      return false;
    return true;
  }

  bool CreateSession(int component, const std::string& content_name) {
    session_.reset(CreateSession("session", content_name, component));
    if (!session_)
      return false;
    return true;
  }

  cricket::PortAllocatorSession* CreateSession(
      const std::string& sid, int component) {
    return CreateSession(sid, kContentName, component);
  }

  cricket::PortAllocatorSession* CreateSession(
      const std::string& sid, const std::string& content_name, int component) {
    return CreateSession(sid, content_name, component, kIceUfrag0, kIcePwd0);
  }

  cricket::PortAllocatorSession* CreateSession(
      const std::string& sid, const std::string& content_name, int component,
      const std::string& ice_ufrag, const std::string& ice_pwd) {
    cricket::PortAllocatorSession* session =
        allocator_->CreateSession(
            sid, content_name, component, ice_ufrag, ice_pwd);
    session->SignalPortReady.connect(this,
            &PortAllocatorTest::OnPortReady);
    session->SignalCandidatesReady.connect(this,
        &PortAllocatorTest::OnCandidatesReady);
    session->SignalCandidatesAllocationDone.connect(this,
        &PortAllocatorTest::OnCandidatesAllocationDone);
    return session;
  }

  static bool CheckCandidate(const cricket::Candidate& c,
                             int component, const std::string& type,
                             const std::string& proto,
                             const SocketAddress& addr) {
    return (c.component() == component && c.type() == type &&
        c.protocol() == proto && c.address().ipaddr() == addr.ipaddr() &&
        ((addr.port() == 0 && (c.address().port() != 0)) ||
        (c.address().port() == addr.port())));
  }
  static bool CheckPort(const rtc::SocketAddress& addr,
                        int min_port, int max_port) {
    return (addr.port() >= min_port && addr.port() <= max_port);
  }

  void OnCandidatesAllocationDone(cricket::PortAllocatorSession* session) {
    // We should only get this callback once, except in the mux test where
    // we have multiple port allocation sessions.
    if (session == session_.get()) {
      ASSERT_FALSE(candidate_allocation_done_);
      candidate_allocation_done_ = true;
    }
  }

  // Check if all ports allocated have send-buffer size |expected|. If
  // |expected| == -1, check if GetOptions returns SOCKET_ERROR.
  void CheckSendBufferSizesOfAllPorts(int expected) {
    std::vector<cricket::PortInterface*>::iterator it;
    for (it = ports_.begin(); it < ports_.end(); ++it) {
      int send_buffer_size;
      if (expected == -1) {
        EXPECT_EQ(SOCKET_ERROR,
                  (*it)->GetOption(rtc::Socket::OPT_SNDBUF,
                                   &send_buffer_size));
      } else {
        EXPECT_EQ(0, (*it)->GetOption(rtc::Socket::OPT_SNDBUF,
                                      &send_buffer_size));
        ASSERT_EQ(expected, send_buffer_size);
      }
    }
  }

  void CheckDisableAdapterEnumeration() {
    EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
    session_->set_flags(cricket::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION);
    session_->StartGettingPorts();
    EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);

    // Only 2 candidates as local UDP/TCP are all 0s and get trimmed out.
    EXPECT_EQ(2U, candidates_.size());
    EXPECT_EQ(2U, ports_.size());  // One stunport and one turnport.

    EXPECT_PRED5(CheckCandidate, candidates_[0],
                 cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp",
                 rtc::SocketAddress(kNatAddr.ipaddr(), 0));
    EXPECT_EQ(
        rtc::EmptySocketAddressWithFamily(candidates_[0].address().family()),
        candidates_[0].related_address());

    EXPECT_PRED5(CheckCandidate, candidates_[1],
                 cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp",
                 rtc::SocketAddress(kTurnUdpExtAddr.ipaddr(), 0));
    EXPECT_EQ(kNatAddr.ipaddr(), candidates_[1].related_address().ipaddr());
  }

 protected:
  cricket::BasicPortAllocator& allocator() {
    return *allocator_;
  }

  void OnPortReady(cricket::PortAllocatorSession* ses,
                   cricket::PortInterface* port) {
    LOG(LS_INFO) << "OnPortReady: " << port->ToString();
    ports_.push_back(port);
  }
  void OnCandidatesReady(cricket::PortAllocatorSession* ses,
                         const std::vector<cricket::Candidate>& candidates) {
    for (size_t i = 0; i < candidates.size(); ++i) {
      LOG(LS_INFO) << "OnCandidatesReady: " << candidates[i].ToString();
      candidates_.push_back(candidates[i]);
    }
  }

  bool HasRelayAddress(const cricket::ProtocolAddress& proto_addr) {
    for (size_t i = 0; i < allocator_->relays().size(); ++i) {
      cricket::RelayServerConfig server_config = allocator_->relays()[i];
      cricket::PortList::const_iterator relay_port;
      for (relay_port = server_config.ports.begin();
          relay_port != server_config.ports.end(); ++relay_port) {
        if (proto_addr.address == relay_port->address &&
            proto_addr.proto == relay_port->proto)
          return true;
      }
    }
    return false;
  }

  rtc::scoped_ptr<rtc::PhysicalSocketServer> pss_;
  rtc::scoped_ptr<rtc::VirtualSocketServer> vss_;
  rtc::scoped_ptr<rtc::FirewallSocketServer> fss_;
  rtc::SocketServerScope ss_scope_;
  rtc::scoped_ptr<rtc::NATServer> nat_server_;
  rtc::NATSocketFactory nat_factory_;
  rtc::BasicPacketSocketFactory nat_socket_factory_;
  rtc::scoped_ptr<cricket::TestStunServer> stun_server_;
  cricket::TestRelayServer relay_server_;
  cricket::TestTurnServer turn_server_;
  rtc::FakeNetworkManager network_manager_;
  rtc::scoped_ptr<cricket::BasicPortAllocator> allocator_;
  rtc::scoped_ptr<cricket::PortAllocatorSession> session_;
  std::vector<cricket::PortInterface*> ports_;
  std::vector<cricket::Candidate> candidates_;
  bool candidate_allocation_done_;
};

// Tests that we can init the port allocator and create a session.
TEST_F(PortAllocatorTest, TestBasic) {
  EXPECT_EQ(&network_manager_, allocator().network_manager());
  EXPECT_EQ(kStunAddr, *allocator().stun_servers().begin());
  ASSERT_EQ(1u, allocator().relays().size());
  EXPECT_EQ(cricket::RELAY_GTURN, allocator().relays()[0].type);
  // Empty relay credentials are used for GTURN.
  EXPECT_TRUE(allocator().relays()[0].credentials.username.empty());
  EXPECT_TRUE(allocator().relays()[0].credentials.password.empty());
  EXPECT_TRUE(HasRelayAddress(cricket::ProtocolAddress(
      kRelayUdpIntAddr, cricket::PROTO_UDP)));
  EXPECT_TRUE(HasRelayAddress(cricket::ProtocolAddress(
      kRelayTcpIntAddr, cricket::PROTO_TCP)));
  EXPECT_TRUE(HasRelayAddress(cricket::ProtocolAddress(
      kRelaySslTcpIntAddr, cricket::PROTO_SSLTCP)));
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
}

// Tests that we allocator session not trying to allocate ports for every 250ms.
TEST_F(PortAllocatorTest, TestNoNetworkInterface) {
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  // Waiting for one second to make sure BasicPortAllocatorSession has not
  // called OnAllocate multiple times. In old behavior it's called every 250ms.
  // When there are no network interfaces, each execution of OnAllocate will
  // result in SignalCandidatesAllocationDone signal.
  rtc::Thread::Current()->ProcessMessages(1000);
  EXPECT_TRUE(candidate_allocation_done_);
  EXPECT_EQ(0U, candidates_.size());
}

// Tests that we can get all the desired addresses successfully.
TEST_F(PortAllocatorTest, TestGetAllPortsWithMinimumStepDelay) {
  AddInterface(kClientAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(7U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(4U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[2],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp", kRelayUdpIntAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[3],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp", kRelayUdpExtAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[4],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "tcp", kRelayTcpIntAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[5],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "tcp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[6],
      cricket::ICE_CANDIDATE_COMPONENT_RTP,
      "relay", "ssltcp", kRelaySslTcpIntAddr);
  EXPECT_TRUE(candidate_allocation_done_);
}

// Verify candidates with default step delay of 1sec.
TEST_F(PortAllocatorTest, TestGetAllPortsWithOneSecondStepDelay) {
  AddInterface(kClientAddr);
  allocator_->set_step_delay(cricket::kDefaultStepDelay);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(2U, candidates_.size(), 1000);
  EXPECT_EQ(2U, ports_.size());
  ASSERT_EQ_WAIT(4U, candidates_.size(), 2000);
  EXPECT_EQ(3U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[2],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp", kRelayUdpIntAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[3],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp", kRelayUdpExtAddr);
  ASSERT_EQ_WAIT(6U, candidates_.size(), 1500);
  EXPECT_PRED5(CheckCandidate, candidates_[4],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "tcp", kRelayTcpIntAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[5],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "tcp", kClientAddr);
  EXPECT_EQ(4U, ports_.size());
  ASSERT_EQ_WAIT(7U, candidates_.size(), 2000);
  EXPECT_PRED5(CheckCandidate, candidates_[6],
      cricket::ICE_CANDIDATE_COMPONENT_RTP,
               "relay", "ssltcp", kRelaySslTcpIntAddr);
  EXPECT_EQ(4U, ports_.size());
  EXPECT_TRUE(candidate_allocation_done_);
  // If we Stop gathering now, we shouldn't get a second "done" callback.
  session_->StopGettingPorts();
}

TEST_F(PortAllocatorTest, TestSetupVideoRtpPortsWithNormalSendBuffers) {
  AddInterface(kClientAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP,
                            cricket::CN_VIDEO));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(7U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_TRUE(candidate_allocation_done_);
  // If we Stop gathering now, we shouldn't get a second "done" callback.
  session_->StopGettingPorts();

  // All ports should have unset send-buffer sizes.
  CheckSendBufferSizesOfAllPorts(-1);
}

// Tests that we can get callback after StopGetAllPorts.
TEST_F(PortAllocatorTest, TestStopGetAllPorts) {
  AddInterface(kClientAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(2U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(2U, ports_.size());
  session_->StopGettingPorts();
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
}

// Test that we restrict client ports appropriately when a port range is set.
// We check the candidates for udp/stun/tcp ports, and the from address
// for relay ports.
TEST_F(PortAllocatorTest, TestGetAllPortsPortRange) {
  AddInterface(kClientAddr);
  // Check that an invalid port range fails.
  EXPECT_FALSE(SetPortRange(kMaxPort, kMinPort));
  // Check that a null port range succeeds.
  EXPECT_TRUE(SetPortRange(0, 0));
  // Check that a valid port range succeeds.
  EXPECT_TRUE(SetPortRange(kMinPort, kMaxPort));
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(7U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(4U, ports_.size());
  // Check the port number for the UDP port object.
  EXPECT_PRED3(CheckPort, candidates_[0].address(), kMinPort, kMaxPort);
  // Check the port number for the STUN port object.
  EXPECT_PRED3(CheckPort, candidates_[1].address(), kMinPort, kMaxPort);
  // Check the port number used to connect to the relay server.
  EXPECT_PRED3(CheckPort, relay_server_.GetConnection(0).source(),
               kMinPort, kMaxPort);
  // Check the port number for the TCP port object.
  EXPECT_PRED3(CheckPort, candidates_[5].address(), kMinPort, kMaxPort);
  EXPECT_TRUE(candidate_allocation_done_);
}

// Test that we don't crash or malfunction if we have no network adapters.
TEST_F(PortAllocatorTest, TestGetAllPortsNoAdapters) {
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  rtc::Thread::Current()->ProcessMessages(100);
  // Without network adapter, we should not get any candidate.
  EXPECT_EQ(0U, candidates_.size());
  EXPECT_TRUE(candidate_allocation_done_);
}

// Test that we should only get STUN and TURN candidates when adapter
// enumeration is disabled.
TEST_F(PortAllocatorTest, TestDisableAdapterEnumeration) {
  AddInterface(kClientAddr);
  // GTURN is not configured here.
  ResetWithNatServer(kStunAddr);
  AddTurnServers(kTurnUdpIntAddr, rtc::SocketAddress());

  CheckDisableAdapterEnumeration();
}

// Test that even with multiple interfaces, the result should be only 1 Stun
// candidate since we bind to any address (i.e. all 0s).
TEST_F(PortAllocatorTest, TestDisableAdapterEnumerationMultipleInterfaces) {
  AddInterface(kPrivateAddr);
  AddInterface(kPrivateAddr2);
  ResetWithNatServer(kStunAddr);
  AddTurnServers(kTurnUdpIntAddr, rtc::SocketAddress());

  CheckDisableAdapterEnumeration();
}

// Test that we can get OnCandidatesAllocationDone callback when all the ports
// are disabled.
TEST_F(PortAllocatorTest, TestDisableAllPorts) {
  AddInterface(kClientAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->set_flags(cricket::PORTALLOCATOR_DISABLE_UDP |
                      cricket::PORTALLOCATOR_DISABLE_STUN |
                      cricket::PORTALLOCATOR_DISABLE_RELAY |
                      cricket::PORTALLOCATOR_DISABLE_TCP);
  session_->StartGettingPorts();
  rtc::Thread::Current()->ProcessMessages(100);
  EXPECT_EQ(0U, candidates_.size());
  EXPECT_TRUE(candidate_allocation_done_);
}

// Test that we don't crash or malfunction if we can't create UDP sockets.
TEST_F(PortAllocatorTest, TestGetAllPortsNoUdpSockets) {
  AddInterface(kClientAddr);
  fss_->set_udp_sockets_enabled(false);
  EXPECT_TRUE(CreateSession(1));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(5U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(2U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp", kRelayUdpIntAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp", kRelayUdpExtAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[2],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "tcp", kRelayTcpIntAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[3],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "tcp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[4],
      cricket::ICE_CANDIDATE_COMPONENT_RTP,
      "relay", "ssltcp", kRelaySslTcpIntAddr);
  EXPECT_TRUE(candidate_allocation_done_);
}

// Test that we don't crash or malfunction if we can't create UDP sockets or
// listen on TCP sockets. We still give out a local TCP address, since
// apparently this is needed for the remote side to accept our connection.
TEST_F(PortAllocatorTest, TestGetAllPortsNoUdpSocketsNoTcpListen) {
  AddInterface(kClientAddr);
  fss_->set_udp_sockets_enabled(false);
  fss_->set_tcp_listen_enabled(false);
  EXPECT_TRUE(CreateSession(1));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(5U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(2U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      1, "relay", "udp", kRelayUdpIntAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      1, "relay", "udp", kRelayUdpExtAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[2],
      1, "relay", "tcp", kRelayTcpIntAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[3],
      1, "local", "tcp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[4],
      1, "relay", "ssltcp", kRelaySslTcpIntAddr);
  EXPECT_TRUE(candidate_allocation_done_);
}

// Test that we don't crash or malfunction if we can't create any sockets.
// TODO: Find a way to exit early here.
TEST_F(PortAllocatorTest, TestGetAllPortsNoSockets) {
  AddInterface(kClientAddr);
  fss_->set_tcp_sockets_enabled(false);
  fss_->set_udp_sockets_enabled(false);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  WAIT(candidates_.size() > 0, 2000);
  // TODO - Check candidate_allocation_done signal.
  // In case of Relay, ports creation will succeed but sockets will fail.
  // There is no error reporting from RelayEntry to handle this failure.
}

// Testing STUN timeout.
TEST_F(PortAllocatorTest, TestGetAllPortsNoUdpAllowed) {
  fss_->AddRule(false, rtc::FP_UDP, rtc::FD_ANY, kClientAddr);
  AddInterface(kClientAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  EXPECT_EQ_WAIT(2U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(2U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "tcp", kClientAddr);
  // RelayPort connection timeout is 3sec. TCP connection with RelayServer
  // will be tried after 3 seconds.
  EXPECT_EQ_WAIT(6U, candidates_.size(), 4000);
  EXPECT_EQ(3U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[2],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp", kRelayUdpIntAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[3],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "tcp", kRelayTcpIntAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[4],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "ssltcp",
      kRelaySslTcpIntAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[5],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp", kRelayUdpExtAddr);
  // Stun Timeout is 9sec.
  EXPECT_TRUE_WAIT(candidate_allocation_done_, 9000);
}

TEST_F(PortAllocatorTest, TestCandidatePriorityOfMultipleInterfaces) {
  AddInterface(kClientAddr);
  AddInterface(kClientAddr2);
  // Allocating only host UDP ports. This is done purely for testing
  // convenience.
  allocator().set_flags(cricket::PORTALLOCATOR_DISABLE_TCP |
                        cricket::PORTALLOCATOR_DISABLE_STUN |
                        cricket::PORTALLOCATOR_DISABLE_RELAY);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  ASSERT_EQ(2U, candidates_.size());
  EXPECT_EQ(2U, ports_.size());
  // Candidates priorities should be different.
  EXPECT_NE(candidates_[0].priority(), candidates_[1].priority());
}

// Test to verify ICE restart process.
TEST_F(PortAllocatorTest, TestGetAllPortsRestarts) {
  AddInterface(kClientAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  EXPECT_EQ_WAIT(7U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(4U, ports_.size());
  EXPECT_TRUE(candidate_allocation_done_);
  // TODO - Extend this to verify ICE restart.
}

// Test ICE candidate filter mechanism with options Relay/Host/Reflexive.
// This test also verifies that when the allocator is only allowed to use
// relay (i.e. IceTransportsType is relay), the raddr is an empty
// address with the correct family. This is to prevent any local
// reflective address leakage in the sdp line.
TEST_F(PortAllocatorTest, TestCandidateFilterWithRelayOnly) {
  AddInterface(kClientAddr);
  // GTURN is not configured here.
  ResetWithTurnServers(kTurnUdpIntAddr, rtc::SocketAddress());
  allocator().set_candidate_filter(cricket::CF_RELAY);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_PRED5(CheckCandidate,
               candidates_[0],
               cricket::ICE_CANDIDATE_COMPONENT_RTP,
               "relay",
               "udp",
               rtc::SocketAddress(kTurnUdpExtAddr.ipaddr(), 0));

  EXPECT_EQ(1U, candidates_.size());
  EXPECT_EQ(1U, ports_.size());  // Only Relay port will be in ready state.
  for (size_t i = 0; i < candidates_.size(); ++i) {
    EXPECT_EQ(std::string(cricket::RELAY_PORT_TYPE), candidates_[i].type());
    EXPECT_EQ(
        candidates_[0].related_address(),
        rtc::EmptySocketAddressWithFamily(candidates_[0].address().family()));
  }
}

TEST_F(PortAllocatorTest, TestCandidateFilterWithHostOnly) {
  AddInterface(kClientAddr);
  allocator().set_flags(cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
  allocator().set_candidate_filter(cricket::CF_HOST);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_EQ(2U, candidates_.size()); // Host UDP/TCP candidates only.
  EXPECT_EQ(2U, ports_.size()); // UDP/TCP ports only.
  for (size_t i = 0; i < candidates_.size(); ++i) {
    EXPECT_EQ(std::string(cricket::LOCAL_PORT_TYPE), candidates_[i].type());
  }
}

// Host is behind the NAT.
TEST_F(PortAllocatorTest, TestCandidateFilterWithReflexiveOnly) {
  AddInterface(kPrivateAddr);
  ResetWithNatServer(kStunAddr);

  allocator().set_flags(cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
  allocator().set_candidate_filter(cricket::CF_REFLEXIVE);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  // Host is behind NAT, no private address will be exposed. Hence only UDP
  // port with STUN candidate will be sent outside.
  EXPECT_EQ(1U, candidates_.size()); // Only STUN candidate.
  EXPECT_EQ(1U, ports_.size());  // Only UDP port will be in ready state.
  for (size_t i = 0; i < candidates_.size(); ++i) {
    EXPECT_EQ(std::string(cricket::STUN_PORT_TYPE), candidates_[i].type());
    EXPECT_EQ(
        candidates_[0].related_address(),
        rtc::EmptySocketAddressWithFamily(candidates_[0].address().family()));
  }
}

// Host is not behind the NAT.
TEST_F(PortAllocatorTest, TestCandidateFilterWithReflexiveOnlyAndNoNAT) {
  AddInterface(kClientAddr);
  allocator().set_flags(cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
  allocator().set_candidate_filter(cricket::CF_REFLEXIVE);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  // Host has a public address, both UDP and TCP candidates will be exposed.
  EXPECT_EQ(2U, candidates_.size()); // Local UDP + TCP candidate.
  EXPECT_EQ(2U, ports_.size());  //  UDP and TCP ports will be in ready state.
  for (size_t i = 0; i < candidates_.size(); ++i) {
    EXPECT_EQ(std::string(cricket::LOCAL_PORT_TYPE), candidates_[i].type());
  }
}

TEST_F(PortAllocatorTest, TestBasicMuxFeatures) {
  AddInterface(kClientAddr);
  allocator().set_flags(cricket::PORTALLOCATOR_ENABLE_BUNDLE |
      cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG);
  // Session ID - session1.
  rtc::scoped_ptr<cricket::PortAllocatorSession> session1(
      CreateSession("session1", cricket::ICE_CANDIDATE_COMPONENT_RTP));
  rtc::scoped_ptr<cricket::PortAllocatorSession> session2(
      CreateSession("session1", cricket::ICE_CANDIDATE_COMPONENT_RTCP));
  session1->StartGettingPorts();
  session2->StartGettingPorts();
  // Each session should receive two proxy ports of local and stun.
  ASSERT_EQ_WAIT(14U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(8U, ports_.size());

  rtc::scoped_ptr<cricket::PortAllocatorSession> session3(
      CreateSession("session1", cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session3->StartGettingPorts();
  // Already allocated candidates and ports will be sent to the newly
  // allocated proxy session.
  ASSERT_EQ_WAIT(21U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(12U, ports_.size());
}

// This test verifies by changing ice_ufrag and/or ice_pwd
// will result in different set of candidates when BUNDLE is enabled.
// If BUNDLE is disabled, CreateSession will always allocate new
// set of candidates.
TEST_F(PortAllocatorTest, TestBundleIceRestart) {
  AddInterface(kClientAddr);
  allocator().set_flags(cricket::PORTALLOCATOR_ENABLE_BUNDLE |
      cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG);
  // Session ID - session1.
  rtc::scoped_ptr<cricket::PortAllocatorSession> session1(
      CreateSession("session1", kContentName,
                    cricket::ICE_CANDIDATE_COMPONENT_RTP,
                    kIceUfrag0, kIcePwd0));
  session1->StartGettingPorts();
  ASSERT_EQ_WAIT(7U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(4U, ports_.size());

  // Allocate a different session with sid |session1| and different ice_ufrag.
  rtc::scoped_ptr<cricket::PortAllocatorSession> session2(
      CreateSession("session1", kContentName,
                    cricket::ICE_CANDIDATE_COMPONENT_RTP,
                    "TestIceUfrag", kIcePwd0));
  session2->StartGettingPorts();
  ASSERT_EQ_WAIT(14U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(8U, ports_.size());
  // Verifying the candidate address different from previously allocated
  // address.
  // Skipping verification of component id and candidate type.
  EXPECT_NE(candidates_[0].address(), candidates_[7].address());
  EXPECT_NE(candidates_[1].address(), candidates_[8].address());

  // Allocating a different session with sid |session1| and
  // different ice_pwd.
  rtc::scoped_ptr<cricket::PortAllocatorSession> session3(
      CreateSession("session1", kContentName,
                    cricket::ICE_CANDIDATE_COMPONENT_RTP,
                    kIceUfrag0, "TestIcePwd"));
  session3->StartGettingPorts();
  ASSERT_EQ_WAIT(21U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(12U, ports_.size());
  // Verifying the candidate address different from previously
  // allocated address.
  EXPECT_NE(candidates_[7].address(), candidates_[14].address());
  EXPECT_NE(candidates_[8].address(), candidates_[15].address());

  // Allocating a session with by changing both ice_ufrag and ice_pwd.
  rtc::scoped_ptr<cricket::PortAllocatorSession> session4(
      CreateSession("session1", kContentName,
                    cricket::ICE_CANDIDATE_COMPONENT_RTP,
                    "TestIceUfrag", "TestIcePwd"));
  session4->StartGettingPorts();
  ASSERT_EQ_WAIT(28U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(16U, ports_.size());
  // Verifying the candidate address different from previously
  // allocated address.
  EXPECT_NE(candidates_[14].address(), candidates_[21].address());
  EXPECT_NE(candidates_[15].address(), candidates_[22].address());
}

// Test that when the PORTALLOCATOR_ENABLE_SHARED_UFRAG is enabled we got same
// ufrag and pwd for the collected candidates.
TEST_F(PortAllocatorTest, TestEnableSharedUfrag) {
  allocator().set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG);
  AddInterface(kClientAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(7U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[5],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "tcp", kClientAddr);
  EXPECT_EQ(4U, ports_.size());
  EXPECT_EQ(kIceUfrag0, candidates_[0].username());
  EXPECT_EQ(kIceUfrag0, candidates_[1].username());
  EXPECT_EQ(kIceUfrag0, candidates_[2].username());
  EXPECT_EQ(kIcePwd0, candidates_[0].password());
  EXPECT_EQ(kIcePwd0, candidates_[1].password());
  EXPECT_TRUE(candidate_allocation_done_);
}

// Test that when the PORTALLOCATOR_ENABLE_SHARED_UFRAG isn't enabled we got
// different ufrag and pwd for the collected candidates.
TEST_F(PortAllocatorTest, TestDisableSharedUfrag) {
  allocator().set_flags(allocator().flags() &
                        ~cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG);
  AddInterface(kClientAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(7U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp", kClientAddr);
  EXPECT_EQ(4U, ports_.size());
  // Port should generate random ufrag and pwd.
  EXPECT_NE(kIceUfrag0, candidates_[0].username());
  EXPECT_NE(kIceUfrag0, candidates_[1].username());
  EXPECT_NE(candidates_[0].username(), candidates_[1].username());
  EXPECT_NE(kIcePwd0, candidates_[0].password());
  EXPECT_NE(kIcePwd0, candidates_[1].password());
  EXPECT_NE(candidates_[0].password(), candidates_[1].password());
  EXPECT_TRUE(candidate_allocation_done_);
}

// Test that when PORTALLOCATOR_ENABLE_SHARED_SOCKET is enabled only one port
// is allocated for udp and stun. Also verify there is only one candidate
// (local) if stun candidate is same as local candidate, which will be the case
// in a public network like the below test.
TEST_F(PortAllocatorTest, TestSharedSocketWithoutNat) {
  AddInterface(kClientAddr);
  allocator_->set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(6U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(3U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
}

// Test that when PORTALLOCATOR_ENABLE_SHARED_SOCKET is enabled only one port
// is allocated for udp and stun. In this test we should expect both stun and
// local candidates as client behind a nat.
TEST_F(PortAllocatorTest, TestSharedSocketWithNat) {
  AddInterface(kClientAddr);
  ResetWithNatServer(kStunAddr);

  allocator_->set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(3U, candidates_.size(), kDefaultAllocationTimeout);
  ASSERT_EQ(2U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp",
      rtc::SocketAddress(kNatAddr.ipaddr(), 0));
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_EQ(3U, candidates_.size());
}

// Test TURN port in shared socket mode with UDP and TCP TURN server adderesses.
TEST_F(PortAllocatorTest, TestSharedSocketWithoutNatUsingTurn) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  AddInterface(kClientAddr);
  allocator_.reset(new cricket::BasicPortAllocator(&network_manager_));

  AddTurnServers(kTurnUdpIntAddr, kTurnTcpIntAddr);

  allocator_->set_step_delay(cricket::kMinimumStepDelay);
  allocator_->set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                        cricket::PORTALLOCATOR_DISABLE_TCP);

  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();

  ASSERT_EQ_WAIT(3U, candidates_.size(), kDefaultAllocationTimeout);
  ASSERT_EQ(3U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp",
      rtc::SocketAddress(kTurnUdpExtAddr.ipaddr(), 0));
  EXPECT_PRED5(CheckCandidate, candidates_[2],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp",
      rtc::SocketAddress(kTurnUdpExtAddr.ipaddr(), 0));
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_EQ(3U, candidates_.size());
}

// Testing DNS resolve for the TURN server, this will test AllocationSequence
// handling the unresolved address signal from TurnPort.
TEST_F(PortAllocatorTest, TestSharedSocketWithServerAddressResolve) {
  turn_server_.AddInternalSocket(rtc::SocketAddress("127.0.0.1", 3478),
                                 cricket::PROTO_UDP);
  AddInterface(kClientAddr);
  allocator_.reset(new cricket::BasicPortAllocator(&network_manager_));
  cricket::RelayServerConfig relay_server(cricket::RELAY_TURN);
  cricket::RelayCredentials credentials(kTurnUsername, kTurnPassword);
  relay_server.credentials = credentials;
  relay_server.ports.push_back(cricket::ProtocolAddress(
      rtc::SocketAddress("localhost", 3478),
      cricket::PROTO_UDP, false));
  allocator_->AddRelay(relay_server);

  allocator_->set_step_delay(cricket::kMinimumStepDelay);
  allocator_->set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                        cricket::PORTALLOCATOR_DISABLE_TCP);

  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();

  EXPECT_EQ_WAIT(2U, ports_.size(), kDefaultAllocationTimeout);
}

// Test that when PORTALLOCATOR_ENABLE_SHARED_SOCKET is enabled only one port
// is allocated for udp/stun/turn. In this test we should expect all local,
// stun and turn candidates.
TEST_F(PortAllocatorTest, TestSharedSocketWithNatUsingTurn) {
  AddInterface(kClientAddr);
  ResetWithNatServer(kStunAddr);

  AddTurnServers(kTurnUdpIntAddr, rtc::SocketAddress());

  allocator_->set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                        cricket::PORTALLOCATOR_DISABLE_TCP);

  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();

  ASSERT_EQ_WAIT(3U, candidates_.size(), kDefaultAllocationTimeout);
  ASSERT_EQ(2U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp",
      rtc::SocketAddress(kNatAddr.ipaddr(), 0));
  EXPECT_PRED5(CheckCandidate, candidates_[2],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp",
      rtc::SocketAddress(kTurnUdpExtAddr.ipaddr(), 0));
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_EQ(3U, candidates_.size());
  // Local port will be created first and then TURN port.
  EXPECT_EQ(2U, ports_[0]->Candidates().size());
  EXPECT_EQ(1U, ports_[1]->Candidates().size());
}

// Test that when PORTALLOCATOR_ENABLE_SHARED_SOCKET is enabled and the TURN
// server is also used as the STUN server, we should get 'local', 'stun', and
// 'relay' candidates.
TEST_F(PortAllocatorTest, TestSharedSocketWithNatUsingTurnAsStun) {
  AddInterface(kClientAddr);
  ResetWithNatServer(kTurnUdpIntAddr);
  AddTurnServers(kTurnUdpIntAddr, rtc::SocketAddress());

  // Must set the step delay to 0 to make sure the relay allocation phase is
  // started before the STUN candidates are obtained, so that the STUN binding
  // response is processed when both StunPort and TurnPort exist to reproduce
  // webrtc issue 3537.
  allocator_->set_step_delay(0);
  allocator_->set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                        cricket::PORTALLOCATOR_DISABLE_TCP);

  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();

  ASSERT_EQ_WAIT(3U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp",
      rtc::SocketAddress(kNatAddr.ipaddr(), 0));
  EXPECT_PRED5(CheckCandidate, candidates_[2],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp",
      rtc::SocketAddress(kTurnUdpExtAddr.ipaddr(), 0));
  EXPECT_EQ(candidates_[2].related_address(), candidates_[1].address());

  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_EQ(3U, candidates_.size());
  // Local port will be created first and then TURN port.
  EXPECT_EQ(2U, ports_[0]->Candidates().size());
  EXPECT_EQ(1U, ports_[1]->Candidates().size());
}

// This test verifies when PORTALLOCATOR_ENABLE_SHARED_SOCKET flag is enabled
// and fail to generate STUN candidate, local UDP candidate is generated
// properly.
TEST_F(PortAllocatorTest, TestSharedSocketNoUdpAllowed) {
  allocator().set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_DISABLE_RELAY |
                        cricket::PORTALLOCATOR_DISABLE_TCP |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
  fss_->AddRule(false, rtc::FP_UDP, rtc::FD_ANY, kClientAddr);
  AddInterface(kClientAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(1U, ports_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(1U, candidates_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  // STUN timeout is 9sec. We need to wait to get candidate done signal.
  EXPECT_TRUE_WAIT(candidate_allocation_done_, 10000);
  EXPECT_EQ(1U, candidates_.size());
}

// This test verifies allocator can use IPv6 addresses along with IPv4.
TEST_F(PortAllocatorTest, TestEnableIPv6Addresses) {
  allocator().set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_DISABLE_RELAY |
                        cricket::PORTALLOCATOR_ENABLE_IPV6 |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
  AddInterface(kClientIPv6Addr);
  AddInterface(kClientAddr);
  allocator_->set_step_delay(cricket::kMinimumStepDelay);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(4U, ports_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(4U, candidates_.size());
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp",
      kClientIPv6Addr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp",
      kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[2],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "tcp",
      kClientIPv6Addr);
  EXPECT_PRED5(CheckCandidate, candidates_[3],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "tcp",
      kClientAddr);
  EXPECT_EQ(4U, candidates_.size());
}

// Test that the httpportallocator correctly maintains its lists of stun and
// relay servers, by never allowing an empty list.
TEST(HttpPortAllocatorTest, TestHttpPortAllocatorHostLists) {
  rtc::FakeNetworkManager network_manager;
  cricket::HttpPortAllocator alloc(&network_manager, "unit test agent");
  EXPECT_EQ(1U, alloc.relay_hosts().size());
  EXPECT_EQ(1U, alloc.stun_hosts().size());

  std::vector<std::string> relay_servers;
  std::vector<rtc::SocketAddress> stun_servers;

  alloc.SetRelayHosts(relay_servers);
  alloc.SetStunHosts(stun_servers);
  EXPECT_EQ(1U, alloc.relay_hosts().size());
  EXPECT_EQ(1U, alloc.stun_hosts().size());

  relay_servers.push_back("1.unittest.corp.google.com");
  relay_servers.push_back("2.unittest.corp.google.com");
  stun_servers.push_back(
      rtc::SocketAddress("1.unittest.corp.google.com", 0));
  stun_servers.push_back(
      rtc::SocketAddress("2.unittest.corp.google.com", 0));

  alloc.SetRelayHosts(relay_servers);
  alloc.SetStunHosts(stun_servers);
  EXPECT_EQ(2U, alloc.relay_hosts().size());
  EXPECT_EQ(2U, alloc.stun_hosts().size());
}

// Test that the HttpPortAllocator uses correct URL to create sessions.
TEST(HttpPortAllocatorTest, TestSessionRequestUrl) {
  rtc::FakeNetworkManager network_manager;
  cricket::HttpPortAllocator alloc(&network_manager, "unit test agent");

  // Disable PORTALLOCATOR_ENABLE_SHARED_UFRAG.
  alloc.set_flags(alloc.flags() & ~cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG);
  rtc::scoped_ptr<cricket::HttpPortAllocatorSessionBase> session(
      static_cast<cricket::HttpPortAllocatorSession*>(
          alloc.CreateSessionInternal(
              "test content", 0, kIceUfrag0, kIcePwd0)));
  std::string url = session->GetSessionRequestUrl();
  LOG(LS_INFO) << "url: " << url;
  EXPECT_EQ(std::string(cricket::HttpPortAllocator::kCreateSessionURL), url);

  // Enable PORTALLOCATOR_ENABLE_SHARED_UFRAG.
  alloc.set_flags(alloc.flags() | cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG);
  session.reset(static_cast<cricket::HttpPortAllocatorSession*>(
      alloc.CreateSessionInternal("test content", 0, kIceUfrag0, kIcePwd0)));
  url = session->GetSessionRequestUrl();
  LOG(LS_INFO) << "url: " << url;
  std::vector<std::string> parts;
  rtc::split(url, '?', &parts);
  ASSERT_EQ(2U, parts.size());

  std::vector<std::string> args_parts;
  rtc::split(parts[1], '&', &args_parts);

  std::map<std::string, std::string> args;
  for (std::vector<std::string>::iterator it = args_parts.begin();
       it != args_parts.end(); ++it) {
    std::vector<std::string> parts;
    rtc::split(*it, '=', &parts);
    ASSERT_EQ(2U, parts.size());
    args[rtc::s_url_decode(parts[0])] = rtc::s_url_decode(parts[1]);
  }

  EXPECT_EQ(kIceUfrag0, args["username"]);
  EXPECT_EQ(kIcePwd0, args["password"]);
}

// Tests that destroying ports with non-shared sockets does not crash.
// b/19074679.
TEST_F(PortAllocatorTest, TestDestroyPortsNonSharedSockets) {
  AddInterface(kClientAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(7U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(4U, ports_.size());

  auto it = ports_.begin();
  for (; it != ports_.end(); ++it) {
    (reinterpret_cast<cricket::Port*>(*it))->Destroy();
  }
}
