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
#include "webrtc/p2p/base/testrelayserver.h"
#include "webrtc/p2p/base/teststunserver.h"
#include "webrtc/p2p/base/testturnserver.h"
#include "webrtc/p2p/client/basicportallocator.h"
#include "webrtc/p2p/client/httpportallocator.h"
#include "webrtc/base/fakenetwork.h"
#include "webrtc/base/firewallsocketserver.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/ipaddress.h"
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
using rtc::IPAddress;
using rtc::SocketAddress;
using rtc::Thread;

static const SocketAddress kClientAddr("11.11.11.11", 0);
static const SocketAddress kLoopbackAddr("127.0.0.1", 0);
static const SocketAddress kPrivateAddr("192.168.1.11", 0);
static const SocketAddress kPrivateAddr2("192.168.1.12", 0);
static const SocketAddress kClientIPv6Addr(
    "2401:fa00:4:1000:be30:5bff:fee5:c3", 0);
static const SocketAddress kClientAddr2("22.22.22.22", 0);
static const SocketAddress kNatUdpAddr("77.77.77.77", rtc::NAT_SERVER_UDP_PORT);
static const SocketAddress kNatTcpAddr("77.77.77.77", rtc::NAT_SERVER_TCP_PORT);
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
        nat_factory_(vss_.get(), kNatUdpAddr, kNatTcpAddr),
        nat_socket_factory_(new rtc::BasicPacketSocketFactory(&nat_factory_)),
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
  void AddInterface(const SocketAddress& addr, const std::string& if_name) {
    network_manager_.AddInterface(addr, if_name);
  }
  void AddInterface(const SocketAddress& addr,
                    const std::string& if_name,
                    rtc::AdapterType type) {
    network_manager_.AddInterface(addr, if_name, type);
  }
  // The default route is the public address that STUN server will observe when
  // the endpoint is sitting on the public internet and the local port is bound
  // to the "any" address. This may be different from the default local address
  // which the endpoint observes. This can occur if the route to the public
  // endpoint like 8.8.8.8 (specified as the default local address) is
  // different from the route to the STUN server (the default route).
  void AddInterfaceAsDefaultRoute(const SocketAddress& addr) {
    AddInterface(addr);
    // When a binding comes from the any address, the |addr| will be used as the
    // srflx address.
    vss_->SetDefaultRoute(addr.ipaddr());
  }
  void RemoveInterface(const SocketAddress& addr) {
    network_manager_.RemoveInterface(addr);
  }
  bool SetPortRange(int min_port, int max_port) {
    return allocator_->SetPortRange(min_port, max_port);
  }
  // Endpoint is on the public network. No STUN or TURN.
  void ResetWithNoServersOrNat() {
    allocator_.reset(new cricket::BasicPortAllocator(&network_manager_));
    allocator_->set_step_delay(cricket::kMinimumStepDelay);
  }
  // Endpoint is behind a NAT, with STUN specified.
  void ResetWithStunServerAndNat(const rtc::SocketAddress& stun_server) {
    ResetWithStunServer(stun_server, true);
  }
  // Endpoint is on the public network, with STUN specified.
  void ResetWithStunServerNoNat(const rtc::SocketAddress& stun_server) {
    ResetWithStunServer(stun_server, false);
  }
  // Endpoint is on the public network, with TURN specified.
  void ResetWithTurnServersNoNat(const rtc::SocketAddress& udp_turn,
                                 const rtc::SocketAddress& tcp_turn) {
    ResetWithNoServersOrNat();
    AddTurnServers(udp_turn, tcp_turn);
  }

  void AddTurnServers(const rtc::SocketAddress& udp_turn,
                      const rtc::SocketAddress& tcp_turn) {
    cricket::RelayServerConfig turn_server(cricket::RELAY_TURN);
    cricket::RelayCredentials credentials(kTurnUsername, kTurnPassword);
    turn_server.credentials = credentials;

    if (!udp_turn.IsNil()) {
      turn_server.ports.push_back(
          cricket::ProtocolAddress(kTurnUdpIntAddr, cricket::PROTO_UDP, false));
    }
    if (!tcp_turn.IsNil()) {
      turn_server.ports.push_back(
          cricket::ProtocolAddress(kTurnTcpIntAddr, cricket::PROTO_TCP, false));
    }
    allocator_->AddTurnServer(turn_server);
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

  // This function starts the port/address gathering and check the existence of
  // candidates as specified. When |expect_stun_candidate| is true,
  // |stun_candidate_addr| carries the expected reflective address, which is
  // also the related address for TURN candidate if it is expected. Otherwise,
  // it should be ignore.
  void CheckDisableAdapterEnumeration(
      uint32_t total_ports,
      const rtc::IPAddress& host_candidate_addr,
      const rtc::IPAddress& stun_candidate_addr,
      const rtc::IPAddress& relay_candidate_udp_transport_addr,
      const rtc::IPAddress& relay_candidate_tcp_transport_addr) {
    network_manager_.set_default_local_addresses(kPrivateAddr.ipaddr(),
                                                 rtc::IPAddress());
    if (!session_) {
      EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
    }
    session_->set_flags(session_->flags() |
                        cricket::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
    allocator().set_allow_tcp_listen(false);
    session_->StartGettingPorts();
    EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);

    uint32_t total_candidates = 0;
    if (!host_candidate_addr.IsNil()) {
      EXPECT_PRED5(CheckCandidate, candidates_[total_candidates],
                   cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp",
                   rtc::SocketAddress(kPrivateAddr.ipaddr(), 0));
      ++total_candidates;
    }
    if (!stun_candidate_addr.IsNil()) {
      EXPECT_PRED5(CheckCandidate, candidates_[total_candidates],
                   cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp",
                   rtc::SocketAddress(stun_candidate_addr, 0));
      rtc::IPAddress related_address = host_candidate_addr;
      if (host_candidate_addr.IsNil()) {
        related_address =
            rtc::GetAnyIP(candidates_[total_candidates].address().family());
      }
      EXPECT_EQ(related_address,
                candidates_[total_candidates].related_address().ipaddr());
      ++total_candidates;
    }
    if (!relay_candidate_udp_transport_addr.IsNil()) {
      EXPECT_PRED5(CheckCandidate, candidates_[total_candidates],
                   cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp",
                   rtc::SocketAddress(relay_candidate_udp_transport_addr, 0));
      EXPECT_EQ(stun_candidate_addr,
                candidates_[total_candidates].related_address().ipaddr());
      ++total_candidates;
    }
    if (!relay_candidate_tcp_transport_addr.IsNil()) {
      EXPECT_PRED5(CheckCandidate, candidates_[total_candidates],
                   cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp",
                   rtc::SocketAddress(relay_candidate_tcp_transport_addr, 0));
      EXPECT_EQ(stun_candidate_addr,
                candidates_[total_candidates].related_address().ipaddr());
      ++total_candidates;
    }

    EXPECT_EQ(total_candidates, candidates_.size());
    EXPECT_EQ(total_ports, ports_.size());
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
    for (size_t i = 0; i < allocator_->turn_servers().size(); ++i) {
      cricket::RelayServerConfig server_config = allocator_->turn_servers()[i];
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

  void ResetWithStunServer(const rtc::SocketAddress& stun_server,
                           bool with_nat) {
    if (with_nat) {
      nat_server_.reset(new rtc::NATServer(
          rtc::NAT_OPEN_CONE, vss_.get(), kNatUdpAddr, kNatTcpAddr, vss_.get(),
          rtc::SocketAddress(kNatUdpAddr.ipaddr(), 0)));
    } else {
      nat_socket_factory_.reset(new rtc::BasicPacketSocketFactory());
    }

    ServerAddresses stun_servers;
    if (!stun_server.IsNil()) {
      stun_servers.insert(stun_server);
    }
    allocator_.reset(new cricket::BasicPortAllocator(
        &network_manager_, nat_socket_factory_.get(), stun_servers));
    allocator().set_step_delay(cricket::kMinimumStepDelay);
  }

  rtc::scoped_ptr<rtc::PhysicalSocketServer> pss_;
  rtc::scoped_ptr<rtc::VirtualSocketServer> vss_;
  rtc::scoped_ptr<rtc::FirewallSocketServer> fss_;
  rtc::SocketServerScope ss_scope_;
  rtc::scoped_ptr<rtc::NATServer> nat_server_;
  rtc::NATSocketFactory nat_factory_;
  rtc::scoped_ptr<rtc::BasicPacketSocketFactory> nat_socket_factory_;
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
  ASSERT_EQ(1u, allocator().turn_servers().size());
  EXPECT_EQ(cricket::RELAY_GTURN, allocator().turn_servers()[0].type);
  // Empty relay credentials are used for GTURN.
  EXPECT_TRUE(allocator().turn_servers()[0].credentials.username.empty());
  EXPECT_TRUE(allocator().turn_servers()[0].credentials.password.empty());
  EXPECT_TRUE(HasRelayAddress(cricket::ProtocolAddress(
      kRelayUdpIntAddr, cricket::PROTO_UDP)));
  EXPECT_TRUE(HasRelayAddress(cricket::ProtocolAddress(
      kRelayTcpIntAddr, cricket::PROTO_TCP)));
  EXPECT_TRUE(HasRelayAddress(cricket::ProtocolAddress(
      kRelaySslTcpIntAddr, cricket::PROTO_SSLTCP)));
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
}

// Tests that our network filtering works properly.
TEST_F(PortAllocatorTest, TestIgnoreOnlyLoopbackNetworkByDefault) {
  AddInterface(SocketAddress(IPAddress(0x12345600U), 0), "test_eth0",
               rtc::ADAPTER_TYPE_ETHERNET);
  AddInterface(SocketAddress(IPAddress(0x12345601U), 0), "test_wlan0",
               rtc::ADAPTER_TYPE_WIFI);
  AddInterface(SocketAddress(IPAddress(0x12345602U), 0), "test_cell0",
               rtc::ADAPTER_TYPE_CELLULAR);
  AddInterface(SocketAddress(IPAddress(0x12345603U), 0), "test_vpn0",
               rtc::ADAPTER_TYPE_VPN);
  AddInterface(SocketAddress(IPAddress(0x12345604U), 0), "test_lo",
               rtc::ADAPTER_TYPE_LOOPBACK);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->set_flags(cricket::PORTALLOCATOR_DISABLE_STUN |
                      cricket::PORTALLOCATOR_DISABLE_RELAY |
                      cricket::PORTALLOCATOR_DISABLE_TCP);
  session_->StartGettingPorts();
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_EQ(4U, candidates_.size());
  for (cricket::Candidate candidate : candidates_) {
    EXPECT_LT(candidate.address().ip(), 0x12345604U);
  }
}

TEST_F(PortAllocatorTest, TestIgnoreNetworksAccordingToIgnoreMask) {
  AddInterface(SocketAddress(IPAddress(0x12345600U), 0), "test_eth0",
               rtc::ADAPTER_TYPE_ETHERNET);
  AddInterface(SocketAddress(IPAddress(0x12345601U), 0), "test_wlan0",
               rtc::ADAPTER_TYPE_WIFI);
  AddInterface(SocketAddress(IPAddress(0x12345602U), 0), "test_cell0",
               rtc::ADAPTER_TYPE_CELLULAR);
  allocator_->SetNetworkIgnoreMask(rtc::ADAPTER_TYPE_ETHERNET |
                                   rtc::ADAPTER_TYPE_LOOPBACK |
                                   rtc::ADAPTER_TYPE_WIFI);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->set_flags(cricket::PORTALLOCATOR_DISABLE_STUN |
                      cricket::PORTALLOCATOR_DISABLE_RELAY |
                      cricket::PORTALLOCATOR_DISABLE_TCP);
  session_->StartGettingPorts();
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_EQ(1U, candidates_.size());
  EXPECT_EQ(0x12345602U, candidates_[0].address().ip());
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

// Test that we could use loopback interface as host candidate.
TEST_F(PortAllocatorTest, TestLoopbackNetworkInterface) {
  AddInterface(kLoopbackAddr, "test_loopback", rtc::ADAPTER_TYPE_LOOPBACK);
  allocator_->SetNetworkIgnoreMask(0);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->set_flags(cricket::PORTALLOCATOR_DISABLE_STUN |
                      cricket::PORTALLOCATOR_DISABLE_RELAY |
                      cricket::PORTALLOCATOR_DISABLE_TCP);
  session_->StartGettingPorts();
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_EQ(1U, candidates_.size());
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

// Test that when the same network interface is brought down and up, the
// port allocator session will restart a new allocation sequence if
// it is not stopped.
TEST_F(PortAllocatorTest, TestSameNetworkDownAndUpWhenSessionNotStopped) {
  std::string if_name("test_net0");
  AddInterface(kClientAddr, if_name);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(7U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(4U, ports_.size());
  EXPECT_TRUE(candidate_allocation_done_);
  candidate_allocation_done_ = false;
  candidates_.clear();
  ports_.clear();

  RemoveInterface(kClientAddr);
  ASSERT_EQ_WAIT(0U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(0U, ports_.size());
  EXPECT_FALSE(candidate_allocation_done_);

  // When the same interfaces are added again, new candidates/ports should be
  // generated.
  AddInterface(kClientAddr, if_name);
  ASSERT_EQ_WAIT(7U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(4U, ports_.size());
  EXPECT_TRUE(candidate_allocation_done_);
}

// Test that when the same network interface is brought down and up, the
// port allocator session will not restart a new allocation sequence if
// it is stopped.
TEST_F(PortAllocatorTest, TestSameNetworkDownAndUpWhenSessionStopped) {
  std::string if_name("test_net0");
  AddInterface(kClientAddr, if_name);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(7U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(4U, ports_.size());
  EXPECT_TRUE(candidate_allocation_done_);
  session_->StopGettingPorts();
  candidates_.clear();
  ports_.clear();

  RemoveInterface(kClientAddr);
  ASSERT_EQ_WAIT(0U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(0U, ports_.size());

  // When the same interfaces are added again, new candidates/ports should not
  // be generated because the session has stopped.
  AddInterface(kClientAddr, if_name);
  ASSERT_EQ_WAIT(0U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(0U, ports_.size());
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

// Test that when enumeration is disabled, we should not have any ports when
// candidate_filter() is set to CF_RELAY and no relay is specified.
TEST_F(PortAllocatorTest,
       TestDisableAdapterEnumerationWithoutNatRelayTransportOnly) {
  ResetWithStunServerNoNat(kStunAddr);
  allocator().set_candidate_filter(cricket::CF_RELAY);
  // Expect to see no ports and no candidates.
  CheckDisableAdapterEnumeration(0U, rtc::IPAddress(), rtc::IPAddress(),
                                 rtc::IPAddress(), rtc::IPAddress());
}

// Test that even with multiple interfaces, the result should still be a single
// default private, one STUN and one TURN candidate since we bind to any address
// (i.e. all 0s).
TEST_F(PortAllocatorTest,
       TestDisableAdapterEnumerationBehindNatMultipleInterfaces) {
  AddInterface(kPrivateAddr);
  AddInterface(kPrivateAddr2);
  ResetWithStunServerAndNat(kStunAddr);
  AddTurnServers(kTurnUdpIntAddr, rtc::SocketAddress());

  // Enable IPv6 here. Since the network_manager doesn't have IPv6 default
  // address set and we have no IPv6 STUN server, there should be no IPv6
  // candidates.
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->set_flags(cricket::PORTALLOCATOR_ENABLE_IPV6);

  // Expect to see 3 ports for IPv4: HOST/STUN, TURN/UDP and TCP ports, 2 ports
  // for IPv6: HOST, and TCP. Only IPv4 candidates: a default private, STUN and
  // TURN/UDP candidates.
  CheckDisableAdapterEnumeration(5U, kPrivateAddr.ipaddr(),
                                 kNatUdpAddr.ipaddr(), kTurnUdpExtAddr.ipaddr(),
                                 rtc::IPAddress());
}

// Test that we should get a default private, STUN, TURN/UDP and TURN/TCP
// candidates when both TURN/UDP and TURN/TCP servers are specified.
TEST_F(PortAllocatorTest, TestDisableAdapterEnumerationBehindNatWithTcp) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  AddInterface(kPrivateAddr);
  ResetWithStunServerAndNat(kStunAddr);
  AddTurnServers(kTurnUdpIntAddr, kTurnTcpIntAddr);
  // Expect to see 4 ports - STUN, TURN/UDP, TURN/TCP and TCP port. A default
  // private, STUN, TURN/UDP, and TURN/TCP candidates.
  CheckDisableAdapterEnumeration(4U, kPrivateAddr.ipaddr(),
                                 kNatUdpAddr.ipaddr(), kTurnUdpExtAddr.ipaddr(),
                                 kTurnUdpExtAddr.ipaddr());
}

// Test that when adapter enumeration is disabled, for endpoints without
// STUN/TURN specified, a default private candidate is still generated.
TEST_F(PortAllocatorTest, TestDisableAdapterEnumerationWithoutNatOrServers) {
  ResetWithNoServersOrNat();
  // Expect to see 2 ports: STUN and TCP ports, one default private candidate.
  CheckDisableAdapterEnumeration(2U, kPrivateAddr.ipaddr(), rtc::IPAddress(),
                                 rtc::IPAddress(), rtc::IPAddress());
}

// Test that when adapter enumeration is disabled, with
// PORTALLOCATOR_DISABLE_LOCALHOST_CANDIDATE specified, for endpoints not behind
// a NAT, there is no local candidate.
TEST_F(PortAllocatorTest,
       TestDisableAdapterEnumerationWithoutNatLocalhostCandidateDisabled) {
  ResetWithStunServerNoNat(kStunAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->set_flags(cricket::PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE);
  // Expect to see 2 ports: STUN and TCP ports, localhost candidate and STUN
  // candidate.
  CheckDisableAdapterEnumeration(2U, rtc::IPAddress(), rtc::IPAddress(),
                                 rtc::IPAddress(), rtc::IPAddress());
}

// Test that when adapter enumeration is disabled, with
// PORTALLOCATOR_DISABLE_LOCALHOST_CANDIDATE specified, for endpoints not behind
// a NAT, there is no local candidate. However, this specified default route
// (kClientAddr) which was discovered when sending STUN requests, will become
// the srflx addresses.
TEST_F(
    PortAllocatorTest,
    TestDisableAdapterEnumerationWithoutNatLocalhostCandidateDisabledWithDifferentDefaultRoute) {
  ResetWithStunServerNoNat(kStunAddr);
  AddInterfaceAsDefaultRoute(kClientAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->set_flags(cricket::PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE);
  // Expect to see 2 ports: STUN and TCP ports, localhost candidate and STUN
  // candidate.
  CheckDisableAdapterEnumeration(2U, rtc::IPAddress(), kClientAddr.ipaddr(),
                                 rtc::IPAddress(), rtc::IPAddress());
}

// Test that when adapter enumeration is disabled, with
// PORTALLOCATOR_DISABLE_LOCALHOST_CANDIDATE specified, for endpoints behind a
// NAT, there is only one STUN candidate.
TEST_F(PortAllocatorTest,
       TestDisableAdapterEnumerationWithNatLocalhostCandidateDisabled) {
  ResetWithStunServerAndNat(kStunAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->set_flags(cricket::PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE);
  // Expect to see 2 ports: STUN and TCP ports, and single STUN candidate.
  CheckDisableAdapterEnumeration(2U, rtc::IPAddress(), kNatUdpAddr.ipaddr(),
                                 rtc::IPAddress(), rtc::IPAddress());
}

// Test that we disable relay over UDP, and only TCP is used when connecting to
// the relay server.
TEST_F(PortAllocatorTest, TestDisableUdpTurn) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  AddInterface(kClientAddr);
  ResetWithStunServerAndNat(kStunAddr);
  AddTurnServers(kTurnUdpIntAddr, kTurnTcpIntAddr);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->set_flags(cricket::PORTALLOCATOR_DISABLE_UDP_RELAY |
                      cricket::PORTALLOCATOR_DISABLE_UDP |
                      cricket::PORTALLOCATOR_DISABLE_STUN |
                      cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);

  session_->StartGettingPorts();
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);

  // Expect to see 2 ports and 2 candidates - TURN/TCP and TCP ports, TCP and
  // TURN/TCP candidates.
  EXPECT_EQ(2U, ports_.size());
  EXPECT_EQ(2U, candidates_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
               cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp",
               kTurnUdpExtAddr);
  // The TURN candidate should use TCP to contact the TURN server.
  EXPECT_EQ(cricket::TCP_PROTOCOL_NAME, candidates_[0].relay_protocol());
  EXPECT_PRED5(CheckCandidate, candidates_[1],
               cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "tcp",
               kClientAddr);
}

// Disable for asan, see
// https://code.google.com/p/webrtc/issues/detail?id=4743 for details.
#if !defined(ADDRESS_SANITIZER)

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

#endif // if !defined(ADDRESS_SANITIZER)

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
  ResetWithTurnServersNoNat(kTurnUdpIntAddr, rtc::SocketAddress());
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
  allocator().set_flags(cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
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
  ResetWithStunServerAndNat(kStunAddr);

  allocator().set_flags(cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
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
  allocator().set_flags(cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
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

// Test that we get the same ufrag and pwd for all candidates.
TEST_F(PortAllocatorTest, TestEnableSharedUfrag) {
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

// Test that when PORTALLOCATOR_ENABLE_SHARED_SOCKET is enabled only one port
// is allocated for udp and stun. Also verify there is only one candidate
// (local) if stun candidate is same as local candidate, which will be the case
// in a public network like the below test.
TEST_F(PortAllocatorTest, TestSharedSocketWithoutNat) {
  AddInterface(kClientAddr);
  allocator_->set_flags(allocator().flags() |
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
  ResetWithStunServerAndNat(kStunAddr);

  allocator_->set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(3U, candidates_.size(), kDefaultAllocationTimeout);
  ASSERT_EQ(2U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp",
      rtc::SocketAddress(kNatUdpAddr.ipaddr(), 0));
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_EQ(3U, candidates_.size());
}

// Test TURN port in shared socket mode with UDP and TCP TURN server addresses.
TEST_F(PortAllocatorTest, TestSharedSocketWithoutNatUsingTurn) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  AddInterface(kClientAddr);
  allocator_.reset(new cricket::BasicPortAllocator(&network_manager_));

  AddTurnServers(kTurnUdpIntAddr, kTurnTcpIntAddr);

  allocator_->set_step_delay(cricket::kMinimumStepDelay);
  allocator_->set_flags(allocator().flags() |
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
  cricket::RelayServerConfig turn_server(cricket::RELAY_TURN);
  cricket::RelayCredentials credentials(kTurnUsername, kTurnPassword);
  turn_server.credentials = credentials;
  turn_server.ports.push_back(cricket::ProtocolAddress(
      rtc::SocketAddress("localhost", 3478), cricket::PROTO_UDP, false));
  allocator_->AddTurnServer(turn_server);

  allocator_->set_step_delay(cricket::kMinimumStepDelay);
  allocator_->set_flags(allocator().flags() |
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
  ResetWithStunServerAndNat(kStunAddr);

  AddTurnServers(kTurnUdpIntAddr, rtc::SocketAddress());

  allocator_->set_flags(allocator().flags() |
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
      rtc::SocketAddress(kNatUdpAddr.ipaddr(), 0));
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
  // Use an empty SocketAddress to add a NAT without STUN server.
  ResetWithStunServerAndNat(SocketAddress());
  AddTurnServers(kTurnUdpIntAddr, rtc::SocketAddress());

  // Must set the step delay to 0 to make sure the relay allocation phase is
  // started before the STUN candidates are obtained, so that the STUN binding
  // response is processed when both StunPort and TurnPort exist to reproduce
  // webrtc issue 3537.
  allocator_->set_step_delay(0);
  allocator_->set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                        cricket::PORTALLOCATOR_DISABLE_TCP);

  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();

  ASSERT_EQ_WAIT(3U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_PRED5(CheckCandidate, candidates_[0],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp", kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
      cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp",
      rtc::SocketAddress(kNatUdpAddr.ipaddr(), 0));
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

// Test that when only a TCP TURN server is available, we do NOT use it as
// a UDP STUN server, as this could leak our IP address. Thus we should only
// expect two ports, a UDPPort and TurnPort.
TEST_F(PortAllocatorTest, TestSharedSocketWithNatUsingTurnTcpOnly) {
  turn_server_.AddInternalSocket(kTurnTcpIntAddr, cricket::PROTO_TCP);
  AddInterface(kClientAddr);
  ResetWithStunServerAndNat(rtc::SocketAddress());
  AddTurnServers(rtc::SocketAddress(), kTurnTcpIntAddr);

  allocator_->set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                        cricket::PORTALLOCATOR_DISABLE_TCP);

  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();

  ASSERT_EQ_WAIT(2U, candidates_.size(), kDefaultAllocationTimeout);
  ASSERT_EQ(2U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
               cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp",
               kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
               cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp",
               rtc::SocketAddress(kTurnUdpExtAddr.ipaddr(), 0));
  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_EQ(2U, candidates_.size());
  EXPECT_EQ(1U, ports_[0]->Candidates().size());
  EXPECT_EQ(1U, ports_[1]->Candidates().size());
}

// Test that even when PORTALLOCATOR_ENABLE_SHARED_SOCKET is NOT enabled, the
// TURN server is used as the STUN server and we get 'local', 'stun', and
// 'relay' candidates.
// TODO(deadbeef): Remove this test when support for non-shared socket mode
// is removed.
TEST_F(PortAllocatorTest, TestNonSharedSocketWithNatUsingTurnAsStun) {
  AddInterface(kClientAddr);
  // Use an empty SocketAddress to add a NAT without STUN server.
  ResetWithStunServerAndNat(SocketAddress());
  AddTurnServers(kTurnUdpIntAddr, rtc::SocketAddress());

  allocator_->set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_DISABLE_TCP);

  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();

  ASSERT_EQ_WAIT(3U, candidates_.size(), kDefaultAllocationTimeout);
  ASSERT_EQ(3U, ports_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
               cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp",
               kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
               cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp",
               rtc::SocketAddress(kNatUdpAddr.ipaddr(), 0));
  EXPECT_PRED5(CheckCandidate, candidates_[2],
               cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp",
               rtc::SocketAddress(kTurnUdpExtAddr.ipaddr(), 0));
  // Not using shared socket, so the STUN request's server reflexive address
  // should be different than the TURN request's server reflexive address.
  EXPECT_NE(candidates_[2].related_address(), candidates_[1].address());

  EXPECT_TRUE_WAIT(candidate_allocation_done_, kDefaultAllocationTimeout);
  EXPECT_EQ(3U, candidates_.size());
  EXPECT_EQ(1U, ports_[0]->Candidates().size());
  EXPECT_EQ(1U, ports_[1]->Candidates().size());
  EXPECT_EQ(1U, ports_[2]->Candidates().size());
}

// Test that even when both a STUN and TURN server are configured, the TURN
// server is used as a STUN server and we get a 'stun' candidate.
TEST_F(PortAllocatorTest, TestSharedSocketWithNatUsingTurnAndStun) {
  AddInterface(kClientAddr);
  // Configure with STUN server but destroy it, so we can ensure that it's
  // the TURN server actually being used as a STUN server.
  ResetWithStunServerAndNat(kStunAddr);
  stun_server_.reset();
  AddTurnServers(kTurnUdpIntAddr, rtc::SocketAddress());

  allocator_->set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                        cricket::PORTALLOCATOR_DISABLE_TCP);

  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();

  ASSERT_EQ_WAIT(3U, candidates_.size(), kDefaultAllocationTimeout);
  EXPECT_PRED5(CheckCandidate, candidates_[0],
               cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp",
               kClientAddr);
  EXPECT_PRED5(CheckCandidate, candidates_[1],
               cricket::ICE_CANDIDATE_COMPONENT_RTP, "stun", "udp",
               rtc::SocketAddress(kNatUdpAddr.ipaddr(), 0));
  EXPECT_PRED5(CheckCandidate, candidates_[2],
               cricket::ICE_CANDIDATE_COMPONENT_RTP, "relay", "udp",
               rtc::SocketAddress(kTurnUdpExtAddr.ipaddr(), 0));
  EXPECT_EQ(candidates_[2].related_address(), candidates_[1].address());

  // Don't bother waiting for STUN timeout, since we already verified
  // that we got a STUN candidate from the TURN server.
}

// This test verifies when PORTALLOCATOR_ENABLE_SHARED_SOCKET flag is enabled
// and fail to generate STUN candidate, local UDP candidate is generated
// properly.
TEST_F(PortAllocatorTest, TestSharedSocketNoUdpAllowed) {
  allocator().set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_DISABLE_RELAY |
                        cricket::PORTALLOCATOR_DISABLE_TCP |
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

// Test that when the NetworkManager doesn't have permission to enumerate
// adapters, the PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION is specified
// automatically.
TEST_F(PortAllocatorTest, TestNetworkPermissionBlocked) {
  network_manager_.set_default_local_addresses(kPrivateAddr.ipaddr(),
                                               rtc::IPAddress());
  network_manager_.set_enumeration_permission(
      rtc::NetworkManager::ENUMERATION_BLOCKED);
  allocator().set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_DISABLE_RELAY |
                        cricket::PORTALLOCATOR_DISABLE_TCP |
                        cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET);
  EXPECT_EQ(0U, allocator_->flags() &
                    cricket::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  EXPECT_EQ(0U, session_->flags() &
                    cricket::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION);
  session_->StartGettingPorts();
  EXPECT_EQ_WAIT(1U, ports_.size(), kDefaultAllocationTimeout);
  EXPECT_EQ(1U, candidates_.size());
  EXPECT_PRED5(CheckCandidate, candidates_[0],
               cricket::ICE_CANDIDATE_COMPONENT_RTP, "local", "udp",
               kPrivateAddr);
  EXPECT_TRUE((session_->flags() &
               cricket::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION) != 0);
}

// This test verifies allocator can use IPv6 addresses along with IPv4.
TEST_F(PortAllocatorTest, TestEnableIPv6Addresses) {
  allocator().set_flags(allocator().flags() |
                        cricket::PORTALLOCATOR_DISABLE_RELAY |
                        cricket::PORTALLOCATOR_ENABLE_IPV6 |
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

TEST_F(PortAllocatorTest, TestStopGettingPorts) {
  AddInterface(kClientAddr);
  allocator_->set_step_delay(cricket::kDefaultStepDelay);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(2U, candidates_.size(), 1000);
  EXPECT_EQ(2U, ports_.size());
  session_->StopGettingPorts();
  EXPECT_TRUE_WAIT(candidate_allocation_done_, 1000);

  // After stopping getting ports, adding a new interface will not start
  // getting ports again.
  candidates_.clear();
  ports_.clear();
  candidate_allocation_done_ = false;
  network_manager_.AddInterface(kClientAddr2);
  rtc::Thread::Current()->ProcessMessages(1000);
  EXPECT_EQ(0U, candidates_.size());
  EXPECT_EQ(0U, ports_.size());
}

TEST_F(PortAllocatorTest, TestClearGettingPorts) {
  AddInterface(kClientAddr);
  allocator_->set_step_delay(cricket::kDefaultStepDelay);
  EXPECT_TRUE(CreateSession(cricket::ICE_CANDIDATE_COMPONENT_RTP));
  session_->StartGettingPorts();
  ASSERT_EQ_WAIT(2U, candidates_.size(), 1000);
  EXPECT_EQ(2U, ports_.size());
  session_->ClearGettingPorts();
  WAIT(candidate_allocation_done_, 1000);
  EXPECT_FALSE(candidate_allocation_done_);

  // After clearing getting ports, adding a new interface will start getting
  // ports again.
  candidates_.clear();
  ports_.clear();
  candidate_allocation_done_ = false;
  network_manager_.AddInterface(kClientAddr2);
  ASSERT_EQ_WAIT(2U, candidates_.size(), 1000);
  EXPECT_EQ(2U, ports_.size());
}
