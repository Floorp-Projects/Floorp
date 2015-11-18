/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/p2ptransportchannel.h"
#include "webrtc/p2p/base/testrelayserver.h"
#include "webrtc/p2p/base/teststunserver.h"
#include "webrtc/p2p/base/testturnserver.h"
#include "webrtc/p2p/client/basicportallocator.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/fakenetwork.h"
#include "webrtc/base/firewallsocketserver.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/natserver.h"
#include "webrtc/base/natsocketfactory.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/proxyserver.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/virtualsocketserver.h"

using cricket::kDefaultPortAllocatorFlags;
using cricket::kMinimumStepDelay;
using cricket::kDefaultStepDelay;
using cricket::PORTALLOCATOR_ENABLE_BUNDLE;
using cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG;
using cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET;
using cricket::ServerAddresses;
using rtc::SocketAddress;

static const int kDefaultTimeout = 1000;
static const int kOnlyLocalPorts = cricket::PORTALLOCATOR_DISABLE_STUN |
                                   cricket::PORTALLOCATOR_DISABLE_RELAY |
                                   cricket::PORTALLOCATOR_DISABLE_TCP;
// Addresses on the public internet.
static const SocketAddress kPublicAddrs[2] =
    { SocketAddress("11.11.11.11", 0), SocketAddress("22.22.22.22", 0) };
// IPv6 Addresses on the public internet.
static const SocketAddress kIPv6PublicAddrs[2] = {
    SocketAddress("2400:4030:1:2c00:be30:abcd:efab:cdef", 0),
    SocketAddress("2620:0:1000:1b03:2e41:38ff:fea6:f2a4", 0)
};
// For configuring multihomed clients.
static const SocketAddress kAlternateAddrs[2] =
    { SocketAddress("11.11.11.101", 0), SocketAddress("22.22.22.202", 0) };
// Addresses for HTTP proxy servers.
static const SocketAddress kHttpsProxyAddrs[2] =
    { SocketAddress("11.11.11.1", 443), SocketAddress("22.22.22.1", 443) };
// Addresses for SOCKS proxy servers.
static const SocketAddress kSocksProxyAddrs[2] =
    { SocketAddress("11.11.11.1", 1080), SocketAddress("22.22.22.1", 1080) };
// Internal addresses for NAT boxes.
static const SocketAddress kNatAddrs[2] =
    { SocketAddress("192.168.1.1", 0), SocketAddress("192.168.2.1", 0) };
// Private addresses inside the NAT private networks.
static const SocketAddress kPrivateAddrs[2] =
    { SocketAddress("192.168.1.11", 0), SocketAddress("192.168.2.22", 0) };
// For cascaded NATs, the internal addresses of the inner NAT boxes.
static const SocketAddress kCascadedNatAddrs[2] =
    { SocketAddress("192.168.10.1", 0), SocketAddress("192.168.20.1", 0) };
// For cascaded NATs, private addresses inside the inner private networks.
static const SocketAddress kCascadedPrivateAddrs[2] =
    { SocketAddress("192.168.10.11", 0), SocketAddress("192.168.20.22", 0) };
// The address of the public STUN server.
static const SocketAddress kStunAddr("99.99.99.1", cricket::STUN_SERVER_PORT);
// The addresses for the public relay server.
static const SocketAddress kRelayUdpIntAddr("99.99.99.2", 5000);
static const SocketAddress kRelayUdpExtAddr("99.99.99.3", 5001);
static const SocketAddress kRelayTcpIntAddr("99.99.99.2", 5002);
static const SocketAddress kRelayTcpExtAddr("99.99.99.3", 5003);
static const SocketAddress kRelaySslTcpIntAddr("99.99.99.2", 5004);
static const SocketAddress kRelaySslTcpExtAddr("99.99.99.3", 5005);
// The addresses for the public turn server.
static const SocketAddress kTurnUdpIntAddr("99.99.99.4",
                                           cricket::STUN_SERVER_PORT);
static const SocketAddress kTurnUdpExtAddr("99.99.99.5", 0);
static const cricket::RelayCredentials kRelayCredentials("test", "test");

// Based on ICE_UFRAG_LENGTH
static const char* kIceUfrag[4] = {"TESTICEUFRAG0000", "TESTICEUFRAG0001",
                                   "TESTICEUFRAG0002", "TESTICEUFRAG0003"};
// Based on ICE_PWD_LENGTH
static const char* kIcePwd[4] = {"TESTICEPWD00000000000000",
                                 "TESTICEPWD00000000000001",
                                 "TESTICEPWD00000000000002",
                                 "TESTICEPWD00000000000003"};

static const uint64 kTiebreaker1 = 11111;
static const uint64 kTiebreaker2 = 22222;

enum {
  MSG_CANDIDATE
};

// This test simulates 2 P2P endpoints that want to establish connectivity
// with each other over various network topologies and conditions, which can be
// specified in each individial test.
// A virtual network (via VirtualSocketServer) along with virtual firewalls and
// NATs (via Firewall/NATSocketServer) are used to simulate the various network
// conditions. We can configure the IP addresses of the endpoints,
// block various types of connectivity, or add arbitrary levels of NAT.
// We also run a STUN server and a relay server on the virtual network to allow
// our typical P2P mechanisms to do their thing.
// For each case, we expect the P2P stack to eventually settle on a specific
// form of connectivity to the other side. The test checks that the P2P
// negotiation successfully establishes connectivity within a certain time,
// and that the result is what we expect.
// Note that this class is a base class for use by other tests, who will provide
// specialized test behavior.
class P2PTransportChannelTestBase : public testing::Test,
                                    public rtc::MessageHandler,
                                    public sigslot::has_slots<> {
 public:
  P2PTransportChannelTestBase()
      : main_(rtc::Thread::Current()),
        pss_(new rtc::PhysicalSocketServer),
        vss_(new rtc::VirtualSocketServer(pss_.get())),
        nss_(new rtc::NATSocketServer(vss_.get())),
        ss_(new rtc::FirewallSocketServer(nss_.get())),
        ss_scope_(ss_.get()),
        stun_server_(cricket::TestStunServer::Create(main_, kStunAddr)),
        turn_server_(main_, kTurnUdpIntAddr, kTurnUdpExtAddr),
        relay_server_(main_, kRelayUdpIntAddr, kRelayUdpExtAddr,
                      kRelayTcpIntAddr, kRelayTcpExtAddr,
                      kRelaySslTcpIntAddr, kRelaySslTcpExtAddr),
        socks_server1_(ss_.get(), kSocksProxyAddrs[0],
                       ss_.get(), kSocksProxyAddrs[0]),
        socks_server2_(ss_.get(), kSocksProxyAddrs[1],
                       ss_.get(), kSocksProxyAddrs[1]),
        clear_remote_candidates_ufrag_pwd_(false),
        force_relay_(false) {
    ep1_.role_ = cricket::ICEROLE_CONTROLLING;
    ep2_.role_ = cricket::ICEROLE_CONTROLLED;

    ServerAddresses stun_servers;
    stun_servers.insert(kStunAddr);
    ep1_.allocator_.reset(new cricket::BasicPortAllocator(
        &ep1_.network_manager_,
        stun_servers, kRelayUdpIntAddr, kRelayTcpIntAddr, kRelaySslTcpIntAddr));
    ep2_.allocator_.reset(new cricket::BasicPortAllocator(
        &ep2_.network_manager_,
        stun_servers, kRelayUdpIntAddr, kRelayTcpIntAddr, kRelaySslTcpIntAddr));
  }

 protected:
  enum Config {
    OPEN,                           // Open to the Internet
    NAT_FULL_CONE,                  // NAT, no filtering
    NAT_ADDR_RESTRICTED,            // NAT, must send to an addr to recv
    NAT_PORT_RESTRICTED,            // NAT, must send to an addr+port to recv
    NAT_SYMMETRIC,                  // NAT, endpoint-dependent bindings
    NAT_DOUBLE_CONE,                // Double NAT, both cone
    NAT_SYMMETRIC_THEN_CONE,        // Double NAT, symmetric outer, cone inner
    BLOCK_UDP,                      // Firewall, UDP in/out blocked
    BLOCK_UDP_AND_INCOMING_TCP,     // Firewall, UDP in/out and TCP in blocked
    BLOCK_ALL_BUT_OUTGOING_HTTP,    // Firewall, only TCP out on 80/443
    PROXY_HTTPS,                    // All traffic through HTTPS proxy
    PROXY_SOCKS,                    // All traffic through SOCKS proxy
    NUM_CONFIGS
  };

  struct Result {
    Result(const std::string& lt, const std::string& lp,
           const std::string& rt, const std::string& rp,
           const std::string& lt2, const std::string& lp2,
           const std::string& rt2, const std::string& rp2, int wait)
        : local_type(lt), local_proto(lp), remote_type(rt), remote_proto(rp),
          local_type2(lt2), local_proto2(lp2), remote_type2(rt2),
          remote_proto2(rp2), connect_wait(wait) {
    }
    std::string local_type;
    std::string local_proto;
    std::string remote_type;
    std::string remote_proto;
    std::string local_type2;
    std::string local_proto2;
    std::string remote_type2;
    std::string remote_proto2;
    int connect_wait;
  };

  struct ChannelData {
    bool CheckData(const char* data, int len) {
      bool ret = false;
      if (!ch_packets_.empty()) {
        std::string packet =  ch_packets_.front();
        ret = (packet == std::string(data, len));
        ch_packets_.pop_front();
      }
      return ret;
    }

    std::string name_;  // TODO - Currently not used.
    std::list<std::string> ch_packets_;
    rtc::scoped_ptr<cricket::P2PTransportChannel> ch_;
  };

  struct CandidateData : public rtc::MessageData {
    CandidateData(cricket::TransportChannel* ch, const cricket::Candidate& c)
        : channel(ch), candidate(c) {
    }
    cricket::TransportChannel* channel;
    cricket::Candidate candidate;
  };

  struct Endpoint {
    Endpoint()
        : role_(cricket::ICEROLE_UNKNOWN),
          tiebreaker_(0),
          role_conflict_(false),
          save_candidates_(false),
          protocol_type_(cricket::ICEPROTO_GOOGLE) {}
    bool HasChannel(cricket::TransportChannel* ch) {
      return (ch == cd1_.ch_.get() || ch == cd2_.ch_.get());
    }
    ChannelData* GetChannelData(cricket::TransportChannel* ch) {
      if (!HasChannel(ch)) return NULL;
      if (cd1_.ch_.get() == ch)
        return &cd1_;
      else
        return &cd2_;
    }

    void SetIceRole(cricket::IceRole role) { role_ = role; }
    cricket::IceRole ice_role() { return role_; }
    void SetIceProtocolType(cricket::IceProtocolType type) {
      protocol_type_ = type;
    }
    cricket::IceProtocolType protocol_type() { return protocol_type_; }
    void SetIceTiebreaker(uint64 tiebreaker) { tiebreaker_ = tiebreaker; }
    uint64 GetIceTiebreaker() { return tiebreaker_; }
    void OnRoleConflict(bool role_conflict) { role_conflict_ = role_conflict; }
    bool role_conflict() { return role_conflict_; }
    void SetAllocationStepDelay(uint32 delay) {
      allocator_->set_step_delay(delay);
    }
    void SetAllowTcpListen(bool allow_tcp_listen) {
      allocator_->set_allow_tcp_listen(allow_tcp_listen);
    }

    rtc::FakeNetworkManager network_manager_;
    rtc::scoped_ptr<cricket::BasicPortAllocator> allocator_;
    ChannelData cd1_;
    ChannelData cd2_;
    cricket::IceRole role_;
    uint64 tiebreaker_;
    bool role_conflict_;
    bool save_candidates_;
    cricket::IceProtocolType protocol_type_;
    std::vector<CandidateData*> saved_candidates_;
  };

  ChannelData* GetChannelData(cricket::TransportChannel* channel) {
    if (ep1_.HasChannel(channel))
      return ep1_.GetChannelData(channel);
    else
      return ep2_.GetChannelData(channel);
  }

  void CreateChannels(int num) {
    std::string ice_ufrag_ep1_cd1_ch = kIceUfrag[0];
    std::string ice_pwd_ep1_cd1_ch = kIcePwd[0];
    std::string ice_ufrag_ep2_cd1_ch = kIceUfrag[1];
    std::string ice_pwd_ep2_cd1_ch = kIcePwd[1];
    ep1_.cd1_.ch_.reset(CreateChannel(
        0, cricket::ICE_CANDIDATE_COMPONENT_DEFAULT,
        ice_ufrag_ep1_cd1_ch, ice_pwd_ep1_cd1_ch,
        ice_ufrag_ep2_cd1_ch, ice_pwd_ep2_cd1_ch));
    ep2_.cd1_.ch_.reset(CreateChannel(
        1, cricket::ICE_CANDIDATE_COMPONENT_DEFAULT,
        ice_ufrag_ep2_cd1_ch, ice_pwd_ep2_cd1_ch,
        ice_ufrag_ep1_cd1_ch, ice_pwd_ep1_cd1_ch));
    if (num == 2) {
      std::string ice_ufrag_ep1_cd2_ch = kIceUfrag[2];
      std::string ice_pwd_ep1_cd2_ch = kIcePwd[2];
      std::string ice_ufrag_ep2_cd2_ch = kIceUfrag[3];
      std::string ice_pwd_ep2_cd2_ch = kIcePwd[3];
      // In BUNDLE each endpoint must share common ICE credentials.
      if (ep1_.allocator_->flags() & PORTALLOCATOR_ENABLE_BUNDLE) {
        ice_ufrag_ep1_cd2_ch = ice_ufrag_ep1_cd1_ch;
        ice_pwd_ep1_cd2_ch = ice_pwd_ep1_cd1_ch;
      }
      if (ep2_.allocator_->flags() & PORTALLOCATOR_ENABLE_BUNDLE) {
        ice_ufrag_ep2_cd2_ch = ice_ufrag_ep2_cd1_ch;
        ice_pwd_ep2_cd2_ch = ice_pwd_ep2_cd1_ch;
      }
      ep1_.cd2_.ch_.reset(CreateChannel(
          0, cricket::ICE_CANDIDATE_COMPONENT_DEFAULT,
          ice_ufrag_ep1_cd2_ch, ice_pwd_ep1_cd2_ch,
          ice_ufrag_ep2_cd2_ch, ice_pwd_ep2_cd2_ch));
      ep2_.cd2_.ch_.reset(CreateChannel(
          1, cricket::ICE_CANDIDATE_COMPONENT_DEFAULT,
          ice_ufrag_ep2_cd2_ch, ice_pwd_ep2_cd2_ch,
          ice_ufrag_ep1_cd2_ch, ice_pwd_ep1_cd2_ch));
    }
  }
  cricket::P2PTransportChannel* CreateChannel(
      int endpoint,
      int component,
      const std::string& local_ice_ufrag,
      const std::string& local_ice_pwd,
      const std::string& remote_ice_ufrag,
      const std::string& remote_ice_pwd) {
    cricket::P2PTransportChannel* channel = new cricket::P2PTransportChannel(
        "test content name", component, NULL, GetAllocator(endpoint));
    channel->SignalRequestSignaling.connect(
        this, &P2PTransportChannelTestBase::OnChannelRequestSignaling);
    channel->SignalCandidateReady.connect(this,
        &P2PTransportChannelTestBase::OnCandidate);
    channel->SignalReadPacket.connect(
        this, &P2PTransportChannelTestBase::OnReadPacket);
    channel->SignalRoleConflict.connect(
        this, &P2PTransportChannelTestBase::OnRoleConflict);
    channel->SetIceProtocolType(GetEndpoint(endpoint)->protocol_type());
    channel->SetIceCredentials(local_ice_ufrag, local_ice_pwd);
    if (clear_remote_candidates_ufrag_pwd_) {
      // This only needs to be set if we're clearing them from the
      // candidates.  Some unit tests rely on this not being set.
      channel->SetRemoteIceCredentials(remote_ice_ufrag, remote_ice_pwd);
    }
    channel->SetIceRole(GetEndpoint(endpoint)->ice_role());
    channel->SetIceTiebreaker(GetEndpoint(endpoint)->GetIceTiebreaker());
    channel->Connect();
    return channel;
  }
  void DestroyChannels() {
    ep1_.cd1_.ch_.reset();
    ep2_.cd1_.ch_.reset();
    ep1_.cd2_.ch_.reset();
    ep2_.cd2_.ch_.reset();
  }
  cricket::P2PTransportChannel* ep1_ch1() { return ep1_.cd1_.ch_.get(); }
  cricket::P2PTransportChannel* ep1_ch2() { return ep1_.cd2_.ch_.get(); }
  cricket::P2PTransportChannel* ep2_ch1() { return ep2_.cd1_.ch_.get(); }
  cricket::P2PTransportChannel* ep2_ch2() { return ep2_.cd2_.ch_.get(); }

  // Common results.
  static const Result kLocalUdpToLocalUdp;
  static const Result kLocalUdpToStunUdp;
  static const Result kLocalUdpToPrflxUdp;
  static const Result kPrflxUdpToLocalUdp;
  static const Result kStunUdpToLocalUdp;
  static const Result kStunUdpToStunUdp;
  static const Result kPrflxUdpToStunUdp;
  static const Result kLocalUdpToRelayUdp;
  static const Result kPrflxUdpToRelayUdp;
  static const Result kLocalTcpToLocalTcp;
  static const Result kLocalTcpToPrflxTcp;
  static const Result kPrflxTcpToLocalTcp;

  rtc::NATSocketServer* nat() { return nss_.get(); }
  rtc::FirewallSocketServer* fw() { return ss_.get(); }

  Endpoint* GetEndpoint(int endpoint) {
    if (endpoint == 0) {
      return &ep1_;
    } else if (endpoint == 1) {
      return &ep2_;
    } else {
      return NULL;
    }
  }
  cricket::PortAllocator* GetAllocator(int endpoint) {
    return GetEndpoint(endpoint)->allocator_.get();
  }
  void AddAddress(int endpoint, const SocketAddress& addr) {
    GetEndpoint(endpoint)->network_manager_.AddInterface(addr);
  }
  void RemoveAddress(int endpoint, const SocketAddress& addr) {
    GetEndpoint(endpoint)->network_manager_.RemoveInterface(addr);
  }
  void SetProxy(int endpoint, rtc::ProxyType type) {
    rtc::ProxyInfo info;
    info.type = type;
    info.address = (type == rtc::PROXY_HTTPS) ?
        kHttpsProxyAddrs[endpoint] : kSocksProxyAddrs[endpoint];
    GetAllocator(endpoint)->set_proxy("unittest/1.0", info);
  }
  void SetAllocatorFlags(int endpoint, int flags) {
    GetAllocator(endpoint)->set_flags(flags);
  }
  void SetIceProtocol(int endpoint, cricket::IceProtocolType type) {
    GetEndpoint(endpoint)->SetIceProtocolType(type);
  }
  void SetIceRole(int endpoint, cricket::IceRole role) {
    GetEndpoint(endpoint)->SetIceRole(role);
  }
  void SetIceTiebreaker(int endpoint, uint64 tiebreaker) {
    GetEndpoint(endpoint)->SetIceTiebreaker(tiebreaker);
  }
  bool GetRoleConflict(int endpoint) {
    return GetEndpoint(endpoint)->role_conflict();
  }
  void SetAllocationStepDelay(int endpoint, uint32 delay) {
    return GetEndpoint(endpoint)->SetAllocationStepDelay(delay);
  }
  void SetAllowTcpListen(int endpoint, bool allow_tcp_listen) {
    return GetEndpoint(endpoint)->SetAllowTcpListen(allow_tcp_listen);
  }

  void Test(const Result& expected) {
    int32 connect_start = rtc::Time(), connect_time;

    // Create the channels and wait for them to connect.
    CreateChannels(1);
    EXPECT_TRUE_WAIT_MARGIN(ep1_ch1() != NULL &&
                            ep2_ch1() != NULL &&
                            ep1_ch1()->readable() &&
                            ep1_ch1()->writable() &&
                            ep2_ch1()->readable() &&
                            ep2_ch1()->writable(),
                            expected.connect_wait,
                            1000);
    connect_time = rtc::TimeSince(connect_start);
    if (connect_time < expected.connect_wait) {
      LOG(LS_INFO) << "Connect time: " << connect_time << " ms";
    } else {
      LOG(LS_INFO) << "Connect time: " << "TIMEOUT ("
                   << expected.connect_wait << " ms)";
    }

    // Allow a few turns of the crank for the best connections to emerge.
    // This may take up to 2 seconds.
    if (ep1_ch1()->best_connection() &&
        ep2_ch1()->best_connection()) {
      int32 converge_start = rtc::Time(), converge_time;
      int converge_wait = 2000;
      EXPECT_TRUE_WAIT_MARGIN(
          LocalCandidate(ep1_ch1())->type() == expected.local_type &&
          LocalCandidate(ep1_ch1())->protocol() == expected.local_proto &&
          RemoteCandidate(ep1_ch1())->type() == expected.remote_type &&
          RemoteCandidate(ep1_ch1())->protocol() == expected.remote_proto,
          converge_wait,
          converge_wait);

      // Also do EXPECT_EQ on each part so that failures are more verbose.
      EXPECT_EQ(expected.local_type, LocalCandidate(ep1_ch1())->type());
      EXPECT_EQ(expected.local_proto, LocalCandidate(ep1_ch1())->protocol());
      EXPECT_EQ(expected.remote_type, RemoteCandidate(ep1_ch1())->type());
      EXPECT_EQ(expected.remote_proto, RemoteCandidate(ep1_ch1())->protocol());

      // Verifying remote channel best connection information. This is done
      // only for the RFC 5245 as controlled agent will use USE-CANDIDATE
      // from controlling (ep1) agent. We can easily predict from EP1 result
      // matrix.
      if (ep2_.protocol_type_ == cricket::ICEPROTO_RFC5245) {
        // Checking for best connection candidates information at remote.
        EXPECT_TRUE_WAIT(
            LocalCandidate(ep2_ch1())->type() == expected.local_type2 &&
            LocalCandidate(ep2_ch1())->protocol() == expected.local_proto2 &&
            RemoteCandidate(ep2_ch1())->protocol() == expected.remote_proto2,
            kDefaultTimeout);

        // For verbose
        EXPECT_EQ(expected.local_type2, LocalCandidate(ep2_ch1())->type());
        EXPECT_EQ(expected.local_proto2, LocalCandidate(ep2_ch1())->protocol());
        EXPECT_EQ(expected.remote_proto2,
                  RemoteCandidate(ep2_ch1())->protocol());
        // Removed remote_type comparision aginst best connection remote
        // candidate. This is done to handle remote type discrepancy from
        // local to stun based on the test type.
        // For example in case of Open -> NAT, ep2 channels will have LULU
        // and in other cases like NAT -> NAT it will be LUSU. To avoid these
        // mismatches and we are doing comparision in different way.
        // i.e. when don't match its remote type is either local or stun.
        // TODO(ronghuawu): Refine the test criteria.
        // https://code.google.com/p/webrtc/issues/detail?id=1953
        if (expected.remote_type2 != RemoteCandidate(ep2_ch1())->type()) {
          EXPECT_TRUE(expected.remote_type2 == cricket::LOCAL_PORT_TYPE ||
                      expected.remote_type2 == cricket::STUN_PORT_TYPE);
          EXPECT_TRUE(
              RemoteCandidate(ep2_ch1())->type() == cricket::LOCAL_PORT_TYPE ||
              RemoteCandidate(ep2_ch1())->type() == cricket::STUN_PORT_TYPE ||
              RemoteCandidate(ep2_ch1())->type() == cricket::PRFLX_PORT_TYPE);
        }
      }

      converge_time = rtc::TimeSince(converge_start);
      if (converge_time < converge_wait) {
        LOG(LS_INFO) << "Converge time: " << converge_time << " ms";
      } else {
        LOG(LS_INFO) << "Converge time: " << "TIMEOUT ("
                     << converge_wait << " ms)";
      }
    }
    // Try sending some data to other end.
    TestSendRecv(1);

    // Destroy the channels, and wait for them to be fully cleaned up.
    DestroyChannels();
  }

  void TestSendRecv(int channels) {
    for (int i = 0; i < 10; ++i) {
    const char* data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
      int len = static_cast<int>(strlen(data));
      // local_channel1 <==> remote_channel1
      EXPECT_EQ_WAIT(len, SendData(ep1_ch1(), data, len), 1000);
      EXPECT_TRUE_WAIT(CheckDataOnChannel(ep2_ch1(), data, len), 1000);
      EXPECT_EQ_WAIT(len, SendData(ep2_ch1(), data, len), 1000);
      EXPECT_TRUE_WAIT(CheckDataOnChannel(ep1_ch1(), data, len), 1000);
      if (channels == 2 && ep1_ch2() && ep2_ch2()) {
        // local_channel2 <==> remote_channel2
        EXPECT_EQ_WAIT(len, SendData(ep1_ch2(), data, len), 1000);
        EXPECT_TRUE_WAIT(CheckDataOnChannel(ep2_ch2(), data, len), 1000);
        EXPECT_EQ_WAIT(len, SendData(ep2_ch2(), data, len), 1000);
        EXPECT_TRUE_WAIT(CheckDataOnChannel(ep1_ch2(), data, len), 1000);
      }
    }
  }

  // This test waits for the transport to become readable and writable on both
  // end points. Once they are, the end points set new local ice credentials to
  // restart the ice gathering. Finally it waits for the transport to select a
  // new connection using the newly generated ice candidates.
  // Before calling this function the end points must be configured.
  void TestHandleIceUfragPasswordChanged() {
    ep1_ch1()->SetRemoteIceCredentials(kIceUfrag[1], kIcePwd[1]);
    ep2_ch1()->SetRemoteIceCredentials(kIceUfrag[0], kIcePwd[0]);
    EXPECT_TRUE_WAIT_MARGIN(ep1_ch1()->readable() && ep1_ch1()->writable() &&
                            ep2_ch1()->readable() && ep2_ch1()->writable(),
                            1000, 1000);

    const cricket::Candidate* old_local_candidate1 = LocalCandidate(ep1_ch1());
    const cricket::Candidate* old_local_candidate2 = LocalCandidate(ep2_ch1());
    const cricket::Candidate* old_remote_candidate1 =
        RemoteCandidate(ep1_ch1());
    const cricket::Candidate* old_remote_candidate2 =
        RemoteCandidate(ep2_ch1());

    ep1_ch1()->SetIceCredentials(kIceUfrag[2], kIcePwd[2]);
    ep1_ch1()->SetRemoteIceCredentials(kIceUfrag[3], kIcePwd[3]);
    ep2_ch1()->SetIceCredentials(kIceUfrag[3], kIcePwd[3]);
    ep2_ch1()->SetRemoteIceCredentials(kIceUfrag[2], kIcePwd[2]);

    EXPECT_TRUE_WAIT_MARGIN(LocalCandidate(ep1_ch1())->generation() !=
                            old_local_candidate1->generation(),
                            1000, 1000);
    EXPECT_TRUE_WAIT_MARGIN(LocalCandidate(ep2_ch1())->generation() !=
                            old_local_candidate2->generation(),
                            1000, 1000);
    EXPECT_TRUE_WAIT_MARGIN(RemoteCandidate(ep1_ch1())->generation() !=
                            old_remote_candidate1->generation(),
                            1000, 1000);
    EXPECT_TRUE_WAIT_MARGIN(RemoteCandidate(ep2_ch1())->generation() !=
                            old_remote_candidate2->generation(),
                            1000, 1000);
    EXPECT_EQ(1u, RemoteCandidate(ep2_ch1())->generation());
    EXPECT_EQ(1u, RemoteCandidate(ep1_ch1())->generation());
  }

  void TestSignalRoleConflict() {
    SetIceProtocol(0, cricket::ICEPROTO_RFC5245);
    SetIceTiebreaker(0, kTiebreaker1);  // Default EP1 is in controlling state.

    SetIceProtocol(1, cricket::ICEPROTO_RFC5245);
    SetIceRole(1, cricket::ICEROLE_CONTROLLING);
    SetIceTiebreaker(1, kTiebreaker2);

    // Creating channels with both channels role set to CONTROLLING.
    CreateChannels(1);
    // Since both the channels initiated with controlling state and channel2
    // has higher tiebreaker value, channel1 should receive SignalRoleConflict.
    EXPECT_TRUE_WAIT(GetRoleConflict(0), 1000);
    EXPECT_FALSE(GetRoleConflict(1));

    EXPECT_TRUE_WAIT(ep1_ch1()->readable() &&
                     ep1_ch1()->writable() &&
                     ep2_ch1()->readable() &&
                     ep2_ch1()->writable(),
                     1000);

    EXPECT_TRUE(ep1_ch1()->best_connection() &&
                ep2_ch1()->best_connection());

    TestSendRecv(1);
  }

  void TestHybridConnectivity(cricket::IceProtocolType proto) {
    AddAddress(0, kPublicAddrs[0]);
    AddAddress(1, kPublicAddrs[1]);

    SetAllocationStepDelay(0, kMinimumStepDelay);
    SetAllocationStepDelay(1, kMinimumStepDelay);

    SetIceRole(0, cricket::ICEROLE_CONTROLLING);
    SetIceProtocol(0, cricket::ICEPROTO_HYBRID);
    SetIceTiebreaker(0, kTiebreaker1);
    SetIceRole(1, cricket::ICEROLE_CONTROLLED);
    SetIceProtocol(1, proto);
    SetIceTiebreaker(1, kTiebreaker2);

    CreateChannels(1);
    // When channel is in hybrid and it's controlling agent, channel will
    // receive ping request from the remote. Hence connection is readable.
    // Since channel is in hybrid, it will not send any pings, so no writable
    // connection. Since channel2 is in controlled state, it will not have
    // any connections which are readable or writable, as it didn't received
    // pings (or none) with USE-CANDIDATE attribute.
    EXPECT_TRUE_WAIT(ep1_ch1()->readable(), 1000);

    // Set real protocol type.
    ep1_ch1()->SetIceProtocolType(proto);

    // Channel should able to send ping requests and connections become writable
    // in both directions.
    EXPECT_TRUE_WAIT(ep1_ch1()->readable() && ep1_ch1()->writable() &&
                     ep2_ch1()->readable() && ep2_ch1()->writable(),
                     1000);
    EXPECT_TRUE(
        ep1_ch1()->best_connection() && ep2_ch1()->best_connection() &&
        LocalCandidate(ep1_ch1())->address().EqualIPs(kPublicAddrs[0]) &&
        RemoteCandidate(ep1_ch1())->address().EqualIPs(kPublicAddrs[1]));

    TestSendRecv(1);
    DestroyChannels();
  }

  void OnChannelRequestSignaling(cricket::TransportChannelImpl* channel) {
    channel->OnSignalingReady();
  }
  // We pass the candidates directly to the other side.
  void OnCandidate(cricket::TransportChannelImpl* ch,
                   const cricket::Candidate& c) {
    if (force_relay_ && c.type() != cricket::RELAY_PORT_TYPE)
      return;

    if (GetEndpoint(ch)->save_candidates_) {
      GetEndpoint(ch)->saved_candidates_.push_back(new CandidateData(ch, c));
    } else {
      main_->Post(this, MSG_CANDIDATE, new CandidateData(ch, c));
    }
  }

  void PauseCandidates(int endpoint) {
    GetEndpoint(endpoint)->save_candidates_ = true;
  }

  void ResumeCandidates(int endpoint) {
    Endpoint* ed = GetEndpoint(endpoint);
    std::vector<CandidateData*>::iterator it = ed->saved_candidates_.begin();
    for (; it != ed->saved_candidates_.end(); ++it) {
      main_->Post(this, MSG_CANDIDATE, *it);
    }
    ed->saved_candidates_.clear();
    ed->save_candidates_ = false;
  }

  void OnMessage(rtc::Message* msg) {
    switch (msg->message_id) {
      case MSG_CANDIDATE: {
        rtc::scoped_ptr<CandidateData> data(
            static_cast<CandidateData*>(msg->pdata));
        cricket::P2PTransportChannel* rch = GetRemoteChannel(data->channel);
        cricket::Candidate c = data->candidate;
        if (clear_remote_candidates_ufrag_pwd_) {
          c.set_username("");
          c.set_password("");
        }
        LOG(LS_INFO) << "Candidate(" << data->channel->component() << "->"
                     << rch->component() << "): " << c.ToString();
        rch->OnCandidate(c);
        break;
      }
    }
  }
  void OnReadPacket(cricket::TransportChannel* channel, const char* data,
                    size_t len, const rtc::PacketTime& packet_time,
                    int flags) {
    std::list<std::string>& packets = GetPacketList(channel);
    packets.push_front(std::string(data, len));
  }
  void OnRoleConflict(cricket::TransportChannelImpl* channel) {
    GetEndpoint(channel)->OnRoleConflict(true);
    cricket::IceRole new_role =
        GetEndpoint(channel)->ice_role() == cricket::ICEROLE_CONTROLLING ?
            cricket::ICEROLE_CONTROLLED : cricket::ICEROLE_CONTROLLING;
    channel->SetIceRole(new_role);
  }
  int SendData(cricket::TransportChannel* channel,
               const char* data, size_t len) {
    rtc::PacketOptions options;
    return channel->SendPacket(data, len, options, 0);
  }
  bool CheckDataOnChannel(cricket::TransportChannel* channel,
                          const char* data, int len) {
    return GetChannelData(channel)->CheckData(data, len);
  }
  static const cricket::Candidate* LocalCandidate(
      cricket::P2PTransportChannel* ch) {
    return (ch && ch->best_connection()) ?
        &ch->best_connection()->local_candidate() : NULL;
  }
  static const cricket::Candidate* RemoteCandidate(
      cricket::P2PTransportChannel* ch) {
    return (ch && ch->best_connection()) ?
        &ch->best_connection()->remote_candidate() : NULL;
  }
  Endpoint* GetEndpoint(cricket::TransportChannel* ch) {
    if (ep1_.HasChannel(ch)) {
      return &ep1_;
    } else if (ep2_.HasChannel(ch)) {
      return &ep2_;
    } else {
      return NULL;
    }
  }
  cricket::P2PTransportChannel* GetRemoteChannel(
      cricket::TransportChannel* ch) {
    if (ch == ep1_ch1())
      return ep2_ch1();
    else if (ch == ep1_ch2())
      return ep2_ch2();
    else if (ch == ep2_ch1())
      return ep1_ch1();
    else if (ch == ep2_ch2())
      return ep1_ch2();
    else
      return NULL;
  }
  std::list<std::string>& GetPacketList(cricket::TransportChannel* ch) {
    return GetChannelData(ch)->ch_packets_;
  }

  void set_clear_remote_candidates_ufrag_pwd(bool clear) {
    clear_remote_candidates_ufrag_pwd_ = clear;
  }

  void set_force_relay(bool relay) {
    force_relay_ = relay;
  }

 private:
  rtc::Thread* main_;
  rtc::scoped_ptr<rtc::PhysicalSocketServer> pss_;
  rtc::scoped_ptr<rtc::VirtualSocketServer> vss_;
  rtc::scoped_ptr<rtc::NATSocketServer> nss_;
  rtc::scoped_ptr<rtc::FirewallSocketServer> ss_;
  rtc::SocketServerScope ss_scope_;
  rtc::scoped_ptr<cricket::TestStunServer> stun_server_;
  cricket::TestTurnServer turn_server_;
  cricket::TestRelayServer relay_server_;
  rtc::SocksProxyServer socks_server1_;
  rtc::SocksProxyServer socks_server2_;
  Endpoint ep1_;
  Endpoint ep2_;
  bool clear_remote_candidates_ufrag_pwd_;
  bool force_relay_;
};

// The tests have only a few outcomes, which we predefine.
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kLocalUdpToLocalUdp("local", "udp", "local", "udp",
                        "local", "udp", "local", "udp", 1000);
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kLocalUdpToStunUdp("local", "udp", "stun", "udp",
                       "local", "udp", "stun", "udp", 1000);
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kLocalUdpToPrflxUdp("local", "udp", "prflx", "udp",
                        "prflx", "udp", "local", "udp", 1000);
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kPrflxUdpToLocalUdp("prflx", "udp", "local", "udp",
                        "local", "udp", "prflx", "udp", 1000);
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kStunUdpToLocalUdp("stun", "udp", "local", "udp",
                       "local", "udp", "stun", "udp", 1000);
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kStunUdpToStunUdp("stun", "udp", "stun", "udp",
                      "stun", "udp", "stun", "udp", 1000);
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kPrflxUdpToStunUdp("prflx", "udp", "stun", "udp",
                       "local", "udp", "prflx", "udp", 1000);
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kLocalUdpToRelayUdp("local", "udp", "relay", "udp",
                        "relay", "udp", "local", "udp", 2000);
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kPrflxUdpToRelayUdp("prflx", "udp", "relay", "udp",
                        "relay", "udp", "prflx", "udp", 2000);
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kLocalTcpToLocalTcp("local", "tcp", "local", "tcp",
                        "local", "tcp", "local", "tcp", 3000);
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kLocalTcpToPrflxTcp("local", "tcp", "prflx", "tcp",
                        "prflx", "tcp", "local", "tcp", 3000);
const P2PTransportChannelTestBase::Result P2PTransportChannelTestBase::
    kPrflxTcpToLocalTcp("prflx", "tcp", "local", "tcp",
                        "local", "tcp", "prflx", "tcp", 3000);

// Test the matrix of all the connectivity types we expect to see in the wild.
// Just test every combination of the configs in the Config enum.
class P2PTransportChannelTest : public P2PTransportChannelTestBase {
 protected:
  static const Result* kMatrix[NUM_CONFIGS][NUM_CONFIGS];
  static const Result* kMatrixSharedUfrag[NUM_CONFIGS][NUM_CONFIGS];
  static const Result* kMatrixSharedSocketAsGice[NUM_CONFIGS][NUM_CONFIGS];
  static const Result* kMatrixSharedSocketAsIce[NUM_CONFIGS][NUM_CONFIGS];
  void ConfigureEndpoints(Config config1, Config config2,
      int allocator_flags1, int allocator_flags2,
      int delay1, int delay2,
      cricket::IceProtocolType type) {
    // Ideally we want to use TURN server for both GICE and ICE, but in case
    // of GICE, TURN server usage is not producing results reliabally.
    // TODO(mallinath): Remove Relay and use TURN server for all tests.
    ServerAddresses stun_servers;
    stun_servers.insert(kStunAddr);
    GetEndpoint(0)->allocator_.reset(
        new cricket::BasicPortAllocator(&(GetEndpoint(0)->network_manager_),
        stun_servers,
        rtc::SocketAddress(), rtc::SocketAddress(),
        rtc::SocketAddress()));
    GetEndpoint(1)->allocator_.reset(
        new cricket::BasicPortAllocator(&(GetEndpoint(1)->network_manager_),
        stun_servers,
        rtc::SocketAddress(), rtc::SocketAddress(),
        rtc::SocketAddress()));

    cricket::RelayServerConfig relay_server(cricket::RELAY_GTURN);
    if (type == cricket::ICEPROTO_RFC5245) {
      relay_server.type = cricket::RELAY_TURN;
      relay_server.credentials = kRelayCredentials;
      relay_server.ports.push_back(cricket::ProtocolAddress(
          kTurnUdpIntAddr, cricket::PROTO_UDP, false));
    } else {
      relay_server.ports.push_back(cricket::ProtocolAddress(
          kRelayUdpIntAddr, cricket::PROTO_UDP, false));
      relay_server.ports.push_back(cricket::ProtocolAddress(
          kRelayTcpIntAddr, cricket::PROTO_TCP, false));
      relay_server.ports.push_back(cricket::ProtocolAddress(
          kRelaySslTcpIntAddr, cricket::PROTO_SSLTCP, false));
    }
    GetEndpoint(0)->allocator_->AddRelay(relay_server);
    GetEndpoint(1)->allocator_->AddRelay(relay_server);

    ConfigureEndpoint(0, config1);
    SetIceProtocol(0, type);
    SetAllocatorFlags(0, allocator_flags1);
    SetAllocationStepDelay(0, delay1);
    ConfigureEndpoint(1, config2);
    SetIceProtocol(1, type);
    SetAllocatorFlags(1, allocator_flags2);
    SetAllocationStepDelay(1, delay2);

    if (type == cricket::ICEPROTO_RFC5245) {
      set_clear_remote_candidates_ufrag_pwd(true);
    }
  }
  void ConfigureEndpoint(int endpoint, Config config) {
    switch (config) {
      case OPEN:
        AddAddress(endpoint, kPublicAddrs[endpoint]);
        break;
      case NAT_FULL_CONE:
      case NAT_ADDR_RESTRICTED:
      case NAT_PORT_RESTRICTED:
      case NAT_SYMMETRIC:
        AddAddress(endpoint, kPrivateAddrs[endpoint]);
        // Add a single NAT of the desired type
        nat()->AddTranslator(kPublicAddrs[endpoint], kNatAddrs[endpoint],
            static_cast<rtc::NATType>(config - NAT_FULL_CONE))->
            AddClient(kPrivateAddrs[endpoint]);
        break;
      case NAT_DOUBLE_CONE:
      case NAT_SYMMETRIC_THEN_CONE:
        AddAddress(endpoint, kCascadedPrivateAddrs[endpoint]);
        // Add a two cascaded NATs of the desired types
        nat()->AddTranslator(kPublicAddrs[endpoint], kNatAddrs[endpoint],
            (config == NAT_DOUBLE_CONE) ?
                rtc::NAT_OPEN_CONE : rtc::NAT_SYMMETRIC)->
            AddTranslator(kPrivateAddrs[endpoint], kCascadedNatAddrs[endpoint],
                rtc::NAT_OPEN_CONE)->
                AddClient(kCascadedPrivateAddrs[endpoint]);
        break;
      case BLOCK_UDP:
      case BLOCK_UDP_AND_INCOMING_TCP:
      case BLOCK_ALL_BUT_OUTGOING_HTTP:
      case PROXY_HTTPS:
      case PROXY_SOCKS:
        AddAddress(endpoint, kPublicAddrs[endpoint]);
        // Block all UDP
        fw()->AddRule(false, rtc::FP_UDP, rtc::FD_ANY,
                      kPublicAddrs[endpoint]);
        if (config == BLOCK_UDP_AND_INCOMING_TCP) {
          // Block TCP inbound to the endpoint
          fw()->AddRule(false, rtc::FP_TCP, SocketAddress(),
                        kPublicAddrs[endpoint]);
        } else if (config == BLOCK_ALL_BUT_OUTGOING_HTTP) {
          // Block all TCP to/from the endpoint except 80/443 out
          fw()->AddRule(true, rtc::FP_TCP, kPublicAddrs[endpoint],
                        SocketAddress(rtc::IPAddress(INADDR_ANY), 80));
          fw()->AddRule(true, rtc::FP_TCP, kPublicAddrs[endpoint],
                        SocketAddress(rtc::IPAddress(INADDR_ANY), 443));
          fw()->AddRule(false, rtc::FP_TCP, rtc::FD_ANY,
                        kPublicAddrs[endpoint]);
        } else if (config == PROXY_HTTPS) {
          // Block all TCP to/from the endpoint except to the proxy server
          fw()->AddRule(true, rtc::FP_TCP, kPublicAddrs[endpoint],
                        kHttpsProxyAddrs[endpoint]);
          fw()->AddRule(false, rtc::FP_TCP, rtc::FD_ANY,
                        kPublicAddrs[endpoint]);
          SetProxy(endpoint, rtc::PROXY_HTTPS);
        } else if (config == PROXY_SOCKS) {
          // Block all TCP to/from the endpoint except to the proxy server
          fw()->AddRule(true, rtc::FP_TCP, kPublicAddrs[endpoint],
                        kSocksProxyAddrs[endpoint]);
          fw()->AddRule(false, rtc::FP_TCP, rtc::FD_ANY,
                        kPublicAddrs[endpoint]);
          SetProxy(endpoint, rtc::PROXY_SOCKS5);
        }
        break;
      default:
        break;
    }
  }
};

// Shorthands for use in the test matrix.
#define LULU &kLocalUdpToLocalUdp
#define LUSU &kLocalUdpToStunUdp
#define LUPU &kLocalUdpToPrflxUdp
#define PULU &kPrflxUdpToLocalUdp
#define SULU &kStunUdpToLocalUdp
#define SUSU &kStunUdpToStunUdp
#define PUSU &kPrflxUdpToStunUdp
#define LURU &kLocalUdpToRelayUdp
#define PURU &kPrflxUdpToRelayUdp
#define LTLT &kLocalTcpToLocalTcp
#define LTPT &kLocalTcpToPrflxTcp
#define PTLT &kPrflxTcpToLocalTcp
// TODO: Enable these once TestRelayServer can accept external TCP.
#define LTRT NULL
#define LSRS NULL

// Test matrix. Originator behavior defined by rows, receiever by columns.

// Currently the p2ptransportchannel.cc (specifically the
// P2PTransportChannel::OnUnknownAddress) operates in 2 modes depend on the
// remote candidates - ufrag per port or shared ufrag.
// For example, if the remote candidates have the shared ufrag, for the unknown
// address reaches the OnUnknownAddress, we will try to find the matched
// remote candidate based on the address and protocol, if not found, a new
// remote candidate will be created for this address. But if the remote
// candidates have different ufrags, we will try to find the matched remote
// candidate by comparing the ufrag. If not found, an error will be returned.
// Because currently the shared ufrag feature is under the experiment and will
// be rolled out gradually. We want to test the different combinations of peers
// with/without the shared ufrag enabled. And those different combinations have
// different expectation of the best connection. For example in the OpenToCONE
// case, an unknown address will be updated to a "host" remote candidate if the
// remote peer uses different ufrag per port. But in the shared ufrag case,
// a "stun" (should be peer-reflexive eventually) candidate will be created for
// that. So the expected best candidate will be LUSU instead of LULU.
// With all these, we have to keep 2 test matrixes for the tests:
// kMatrix - for the tests that the remote peer uses different ufrag per port.
// kMatrixSharedUfrag - for the tests that remote peer uses shared ufrag.
// The different between the two matrixes are on:
// OPToCONE, OPTo2CON,
// COToCONE, COToADDR, COToPORT, COToSYMM, COTo2CON, COToSCON,
// ADToCONE, ADToADDR, ADTo2CON,
// POToADDR,
// SYToADDR,
// 2CToCONE, 2CToADDR, 2CToPORT, 2CToSYMM, 2CTo2CON, 2CToSCON,
// SCToADDR,

// TODO: Fix NULLs caused by lack of TCP support in NATSocket.
// TODO: Fix NULLs caused by no HTTP proxy support.
// TODO: Rearrange rows/columns from best to worst.
// TODO(ronghuawu): Keep only one test matrix once the shared ufrag is enabled.
const P2PTransportChannelTest::Result*
    P2PTransportChannelTest::kMatrix[NUM_CONFIGS][NUM_CONFIGS] = {
//      OPEN  CONE  ADDR  PORT  SYMM  2CON  SCON  !UDP  !TCP  HTTP  PRXH  PRXS
/*OP*/ {LULU, LULU, LULU, LULU, LULU, LULU, LULU, LTLT, LTLT, LSRS, NULL, LTLT},
/*CO*/ {LULU, LULU, LULU, SULU, SULU, LULU, SULU, NULL, NULL, LSRS, NULL, LTRT},
/*AD*/ {LULU, LULU, LULU, SUSU, SUSU, LULU, SUSU, NULL, NULL, LSRS, NULL, LTRT},
/*PO*/ {LULU, LUSU, LUSU, SUSU, LURU, LUSU, LURU, NULL, NULL, LSRS, NULL, LTRT},
/*SY*/ {LULU, LUSU, LUSU, LURU, LURU, LUSU, LURU, NULL, NULL, LSRS, NULL, LTRT},
/*2C*/ {LULU, LULU, LULU, SULU, SULU, LULU, SULU, NULL, NULL, LSRS, NULL, LTRT},
/*SC*/ {LULU, LUSU, LUSU, LURU, LURU, LUSU, LURU, NULL, NULL, LSRS, NULL, LTRT},
/*!U*/ {LTLT, NULL, NULL, NULL, NULL, NULL, NULL, LTLT, LTLT, LSRS, NULL, LTRT},
/*!T*/ {LTRT, NULL, NULL, NULL, NULL, NULL, NULL, LTLT, LTRT, LSRS, NULL, LTRT},
/*HT*/ {LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, NULL, LSRS},
/*PR*/ {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
/*PR*/ {LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LSRS, NULL, LTRT},
};
const P2PTransportChannelTest::Result*
    P2PTransportChannelTest::kMatrixSharedUfrag[NUM_CONFIGS][NUM_CONFIGS] = {
//      OPEN  CONE  ADDR  PORT  SYMM  2CON  SCON  !UDP  !TCP  HTTP  PRXH  PRXS
/*OP*/ {LULU, LUSU, LULU, LULU, LULU, LUSU, LULU, LTLT, LTLT, LSRS, NULL, LTLT},
/*CO*/ {LULU, LUSU, LUSU, SUSU, SUSU, LUSU, SUSU, NULL, NULL, LSRS, NULL, LTRT},
/*AD*/ {LULU, LUSU, LUSU, SUSU, SUSU, LUSU, SUSU, NULL, NULL, LSRS, NULL, LTRT},
/*PO*/ {LULU, LUSU, LUSU, SUSU, LURU, LUSU, LURU, NULL, NULL, LSRS, NULL, LTRT},
/*SY*/ {LULU, LUSU, LUSU, LURU, LURU, LUSU, LURU, NULL, NULL, LSRS, NULL, LTRT},
/*2C*/ {LULU, LUSU, LUSU, SUSU, SUSU, LUSU, SUSU, NULL, NULL, LSRS, NULL, LTRT},
/*SC*/ {LULU, LUSU, LUSU, LURU, LURU, LUSU, LURU, NULL, NULL, LSRS, NULL, LTRT},
/*!U*/ {LTLT, NULL, NULL, NULL, NULL, NULL, NULL, LTLT, LTLT, LSRS, NULL, LTRT},
/*!T*/ {LTRT, NULL, NULL, NULL, NULL, NULL, NULL, LTLT, LTRT, LSRS, NULL, LTRT},
/*HT*/ {LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, NULL, LSRS},
/*PR*/ {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
/*PR*/ {LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LSRS, NULL, LTRT},
};
const P2PTransportChannelTest::Result*
    P2PTransportChannelTest::kMatrixSharedSocketAsGice
        [NUM_CONFIGS][NUM_CONFIGS] = {
//      OPEN  CONE  ADDR  PORT  SYMM  2CON  SCON  !UDP  !TCP  HTTP  PRXH  PRXS
/*OP*/ {LULU, LUSU, LUSU, LUSU, LUSU, LUSU, LUSU, LTLT, LTLT, LSRS, NULL, LTLT},
/*CO*/ {LULU, LUSU, LUSU, LUSU, LUSU, LUSU, LUSU, NULL, NULL, LSRS, NULL, LTRT},
/*AD*/ {LULU, LUSU, LUSU, LUSU, LUSU, LUSU, LUSU, NULL, NULL, LSRS, NULL, LTRT},
/*PO*/ {LULU, LUSU, LUSU, LUSU, LURU, LUSU, LURU, NULL, NULL, LSRS, NULL, LTRT},
/*SY*/ {LULU, LUSU, LUSU, LURU, LURU, LUSU, LURU, NULL, NULL, LSRS, NULL, LTRT},
/*2C*/ {LULU, LUSU, LUSU, LUSU, LUSU, LUSU, LUSU, NULL, NULL, LSRS, NULL, LTRT},
/*SC*/ {LULU, LUSU, LUSU, LURU, LURU, LUSU, LURU, NULL, NULL, LSRS, NULL, LTRT},
/*!U*/ {LTLT, NULL, NULL, NULL, NULL, NULL, NULL, LTLT, LTLT, LSRS, NULL, LTRT},
/*!T*/ {LTRT, NULL, NULL, NULL, NULL, NULL, NULL, LTLT, LTRT, LSRS, NULL, LTRT},
/*HT*/ {LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, NULL, LSRS},
/*PR*/ {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
/*PR*/ {LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LSRS, NULL, LTRT},
};
const P2PTransportChannelTest::Result*
    P2PTransportChannelTest::kMatrixSharedSocketAsIce
        [NUM_CONFIGS][NUM_CONFIGS] = {
//      OPEN  CONE  ADDR  PORT  SYMM  2CON  SCON  !UDP  !TCP  HTTP  PRXH  PRXS
/*OP*/ {LULU, LUSU, LUSU, LUSU, LUPU, LUSU, LUPU, PTLT, LTPT, LSRS, NULL, PTLT},
/*CO*/ {LULU, LUSU, LUSU, LUSU, LUPU, LUSU, LUPU, NULL, NULL, LSRS, NULL, LTRT},
/*AD*/ {LULU, LUSU, LUSU, LUSU, LUPU, LUSU, LUPU, NULL, NULL, LSRS, NULL, LTRT},
/*PO*/ {LULU, LUSU, LUSU, LUSU, LURU, LUSU, LURU, NULL, NULL, LSRS, NULL, LTRT},
/*SY*/ {PULU, PUSU, PUSU, PURU, PURU, PUSU, PURU, NULL, NULL, LSRS, NULL, LTRT},
/*2C*/ {LULU, LUSU, LUSU, LUSU, LUPU, LUSU, LUPU, NULL, NULL, LSRS, NULL, LTRT},
/*SC*/ {PULU, PUSU, PUSU, PURU, PURU, PUSU, PURU, NULL, NULL, LSRS, NULL, LTRT},
/*!U*/ {PTLT, NULL, NULL, NULL, NULL, NULL, NULL, PTLT, LTPT, LSRS, NULL, LTRT},
/*!T*/ {LTRT, NULL, NULL, NULL, NULL, NULL, NULL, PTLT, LTRT, LSRS, NULL, LTRT},
/*HT*/ {LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, LSRS, NULL, LSRS},
/*PR*/ {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
/*PR*/ {LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LTRT, LSRS, NULL, LTRT},
};

// The actual tests that exercise all the various configurations.
// Test names are of the form P2PTransportChannelTest_TestOPENToNAT_FULL_CONE
// Same test case is run in both GICE and ICE mode.
// kDefaultStepDelay - is used for all Gice cases.
// kMinimumStepDelay - is used when both end points have
//                     PORTALLOCATOR_ENABLE_SHARED_UFRAG flag enabled.
// Technically we should be able to use kMinimumStepDelay irrespective of
// protocol type. But which might need modifications to current result matrices
// for tests in this file.
#define P2P_TEST_DECLARATION(x, y, z) \
  TEST_F(P2PTransportChannelTest, z##Test##x##To##y##AsGiceNoneSharedUfrag) { \
    ConfigureEndpoints(x, y, kDefaultPortAllocatorFlags, \
                       kDefaultPortAllocatorFlags, \
                       kDefaultStepDelay, kDefaultStepDelay, \
                       cricket::ICEPROTO_GOOGLE); \
    if (kMatrix[x][y] != NULL) \
      Test(*kMatrix[x][y]); \
    else \
      LOG(LS_WARNING) << "Not yet implemented"; \
  } \
  TEST_F(P2PTransportChannelTest, z##Test##x##To##y##AsGiceP0SharedUfrag) { \
    ConfigureEndpoints(x, y, PORTALLOCATOR_ENABLE_SHARED_UFRAG, \
                       kDefaultPortAllocatorFlags, \
                       kDefaultStepDelay, kDefaultStepDelay, \
                       cricket::ICEPROTO_GOOGLE); \
    if (kMatrix[x][y] != NULL) \
      Test(*kMatrix[x][y]); \
    else \
      LOG(LS_WARNING) << "Not yet implemented"; \
  } \
  TEST_F(P2PTransportChannelTest, z##Test##x##To##y##AsGiceP1SharedUfrag) { \
    ConfigureEndpoints(x, y, kDefaultPortAllocatorFlags, \
                       PORTALLOCATOR_ENABLE_SHARED_UFRAG, \
                       kDefaultStepDelay, kDefaultStepDelay, \
                       cricket::ICEPROTO_GOOGLE); \
    if (kMatrixSharedUfrag[x][y] != NULL) \
      Test(*kMatrixSharedUfrag[x][y]); \
    else \
      LOG(LS_WARNING) << "Not yet implemented"; \
  } \
  TEST_F(P2PTransportChannelTest, z##Test##x##To##y##AsGiceBothSharedUfrag) { \
    ConfigureEndpoints(x, y, PORTALLOCATOR_ENABLE_SHARED_UFRAG, \
                       PORTALLOCATOR_ENABLE_SHARED_UFRAG, \
                       kDefaultStepDelay, kDefaultStepDelay, \
                       cricket::ICEPROTO_GOOGLE); \
    if (kMatrixSharedUfrag[x][y] != NULL) \
      Test(*kMatrixSharedUfrag[x][y]); \
    else \
      LOG(LS_WARNING) << "Not yet implemented"; \
  } \
  TEST_F(P2PTransportChannelTest, \
         z##Test##x##To##y##AsGiceBothSharedUfragWithMinimumStepDelay) { \
    ConfigureEndpoints(x, y, PORTALLOCATOR_ENABLE_SHARED_UFRAG, \
                       PORTALLOCATOR_ENABLE_SHARED_UFRAG, \
                       kMinimumStepDelay, kMinimumStepDelay, \
                       cricket::ICEPROTO_GOOGLE); \
    if (kMatrixSharedUfrag[x][y] != NULL) \
      Test(*kMatrixSharedUfrag[x][y]); \
    else \
      LOG(LS_WARNING) << "Not yet implemented"; \
  } \
  TEST_F(P2PTransportChannelTest, \
         z##Test##x##To##y##AsGiceBothSharedUfragSocket) { \
    ConfigureEndpoints(x, y, PORTALLOCATOR_ENABLE_SHARED_UFRAG | \
                       PORTALLOCATOR_ENABLE_SHARED_SOCKET, \
                       PORTALLOCATOR_ENABLE_SHARED_UFRAG | \
                       PORTALLOCATOR_ENABLE_SHARED_SOCKET, \
                       kMinimumStepDelay, kMinimumStepDelay, \
                       cricket::ICEPROTO_GOOGLE); \
    if (kMatrixSharedSocketAsGice[x][y] != NULL) \
      Test(*kMatrixSharedSocketAsGice[x][y]); \
    else \
    LOG(LS_WARNING) << "Not yet implemented"; \
  } \
  TEST_F(P2PTransportChannelTest, z##Test##x##To##y##AsIce) { \
    ConfigureEndpoints(x, y, PORTALLOCATOR_ENABLE_SHARED_UFRAG | \
                       PORTALLOCATOR_ENABLE_SHARED_SOCKET, \
                       PORTALLOCATOR_ENABLE_SHARED_UFRAG | \
                       PORTALLOCATOR_ENABLE_SHARED_SOCKET, \
                       kMinimumStepDelay, kMinimumStepDelay, \
                       cricket::ICEPROTO_RFC5245); \
    if (kMatrixSharedSocketAsIce[x][y] != NULL) \
      Test(*kMatrixSharedSocketAsIce[x][y]); \
    else \
    LOG(LS_WARNING) << "Not yet implemented"; \
  }

#define P2P_TEST(x, y) \
  P2P_TEST_DECLARATION(x, y,)

#define FLAKY_P2P_TEST(x, y) \
  P2P_TEST_DECLARATION(x, y, DISABLED_)

// TODO(holmer): Disabled due to randomly failing on webrtc buildbots.
// Issue: webrtc/2383
#define P2P_TEST_SET(x) \
  P2P_TEST(x, OPEN) \
  FLAKY_P2P_TEST(x, NAT_FULL_CONE) \
  FLAKY_P2P_TEST(x, NAT_ADDR_RESTRICTED) \
  FLAKY_P2P_TEST(x, NAT_PORT_RESTRICTED) \
  P2P_TEST(x, NAT_SYMMETRIC) \
  FLAKY_P2P_TEST(x, NAT_DOUBLE_CONE) \
  P2P_TEST(x, NAT_SYMMETRIC_THEN_CONE) \
  P2P_TEST(x, BLOCK_UDP) \
  P2P_TEST(x, BLOCK_UDP_AND_INCOMING_TCP) \
  P2P_TEST(x, BLOCK_ALL_BUT_OUTGOING_HTTP) \
  P2P_TEST(x, PROXY_HTTPS) \
  P2P_TEST(x, PROXY_SOCKS)

#define FLAKY_P2P_TEST_SET(x) \
  P2P_TEST(x, OPEN) \
  P2P_TEST(x, NAT_FULL_CONE) \
  P2P_TEST(x, NAT_ADDR_RESTRICTED) \
  P2P_TEST(x, NAT_PORT_RESTRICTED) \
  P2P_TEST(x, NAT_SYMMETRIC) \
  P2P_TEST(x, NAT_DOUBLE_CONE) \
  P2P_TEST(x, NAT_SYMMETRIC_THEN_CONE) \
  P2P_TEST(x, BLOCK_UDP) \
  P2P_TEST(x, BLOCK_UDP_AND_INCOMING_TCP) \
  P2P_TEST(x, BLOCK_ALL_BUT_OUTGOING_HTTP) \
  P2P_TEST(x, PROXY_HTTPS) \
  P2P_TEST(x, PROXY_SOCKS)

P2P_TEST_SET(OPEN)
P2P_TEST_SET(NAT_FULL_CONE)
P2P_TEST_SET(NAT_ADDR_RESTRICTED)
P2P_TEST_SET(NAT_PORT_RESTRICTED)
P2P_TEST_SET(NAT_SYMMETRIC)
P2P_TEST_SET(NAT_DOUBLE_CONE)
P2P_TEST_SET(NAT_SYMMETRIC_THEN_CONE)
P2P_TEST_SET(BLOCK_UDP)
P2P_TEST_SET(BLOCK_UDP_AND_INCOMING_TCP)
P2P_TEST_SET(BLOCK_ALL_BUT_OUTGOING_HTTP)
P2P_TEST_SET(PROXY_HTTPS)
P2P_TEST_SET(PROXY_SOCKS)

// Test that we restart candidate allocation when local ufrag&pwd changed.
// Standard Ice protocol is used.
TEST_F(P2PTransportChannelTest, HandleUfragPwdChangeAsIce) {
  ConfigureEndpoints(OPEN, OPEN,
                     PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     kMinimumStepDelay, kMinimumStepDelay,
                     cricket::ICEPROTO_RFC5245);
  CreateChannels(1);
  TestHandleIceUfragPasswordChanged();
  DestroyChannels();
}

// Test that we restart candidate allocation when local ufrag&pwd changed.
// Standard Ice protocol is used.
TEST_F(P2PTransportChannelTest, HandleUfragPwdChangeBundleAsIce) {
  ConfigureEndpoints(
      OPEN, OPEN,
      PORTALLOCATOR_ENABLE_BUNDLE | PORTALLOCATOR_ENABLE_SHARED_UFRAG,
      PORTALLOCATOR_ENABLE_BUNDLE | PORTALLOCATOR_ENABLE_SHARED_UFRAG,
      kMinimumStepDelay, kMinimumStepDelay,
      cricket::ICEPROTO_RFC5245);
  CreateChannels(2);
  TestHandleIceUfragPasswordChanged();
  DestroyChannels();
}

// Test that we restart candidate allocation when local ufrag&pwd changed.
// Google Ice protocol is used.
TEST_F(P2PTransportChannelTest, HandleUfragPwdChangeAsGice) {
  ConfigureEndpoints(OPEN, OPEN,
                     PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     kDefaultStepDelay, kDefaultStepDelay,
                     cricket::ICEPROTO_GOOGLE);
  CreateChannels(1);
  TestHandleIceUfragPasswordChanged();
  DestroyChannels();
}

// Test that ICE restart works when bundle is enabled.
// Google Ice protocol is used.
TEST_F(P2PTransportChannelTest, HandleUfragPwdChangeBundleAsGice) {
  ConfigureEndpoints(
      OPEN, OPEN,
      PORTALLOCATOR_ENABLE_BUNDLE | PORTALLOCATOR_ENABLE_SHARED_UFRAG,
      PORTALLOCATOR_ENABLE_BUNDLE | PORTALLOCATOR_ENABLE_SHARED_UFRAG,
      kDefaultStepDelay, kDefaultStepDelay,
      cricket::ICEPROTO_GOOGLE);
  CreateChannels(2);
  TestHandleIceUfragPasswordChanged();
  DestroyChannels();
}

// Test the operation of GetStats.
TEST_F(P2PTransportChannelTest, GetStats) {
  ConfigureEndpoints(OPEN, OPEN,
                     kDefaultPortAllocatorFlags,
                     kDefaultPortAllocatorFlags,
                     kDefaultStepDelay, kDefaultStepDelay,
                     cricket::ICEPROTO_GOOGLE);
  CreateChannels(1);
  EXPECT_TRUE_WAIT_MARGIN(ep1_ch1()->readable() && ep1_ch1()->writable() &&
                          ep2_ch1()->readable() && ep2_ch1()->writable(),
                          1000, 1000);
  TestSendRecv(1);
  cricket::ConnectionInfos infos;
  ASSERT_TRUE(ep1_ch1()->GetStats(&infos));
  ASSERT_EQ(1U, infos.size());
  EXPECT_TRUE(infos[0].new_connection);
  EXPECT_TRUE(infos[0].best_connection);
  EXPECT_TRUE(infos[0].readable);
  EXPECT_TRUE(infos[0].writable);
  EXPECT_FALSE(infos[0].timeout);
  EXPECT_EQ(10U, infos[0].sent_total_packets);
  EXPECT_EQ(0U, infos[0].sent_discarded_packets);
  EXPECT_EQ(10 * 36U, infos[0].sent_total_bytes);
  EXPECT_EQ(10 * 36U, infos[0].recv_total_bytes);
  EXPECT_GT(infos[0].rtt, 0U);
  DestroyChannels();
}

// Test that we properly create a connection on a STUN ping from unknown address
// when the signaling is slow.
TEST_F(P2PTransportChannelTest, PeerReflexiveCandidateBeforeSignaling) {
  ConfigureEndpoints(OPEN, OPEN,
                     PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     kDefaultStepDelay, kDefaultStepDelay,
                     cricket::ICEPROTO_RFC5245);
  // Emulate no remote credentials coming in.
  set_clear_remote_candidates_ufrag_pwd(false);
  CreateChannels(1);
  // Only have remote credentials come in for ep2, not ep1.
  ep2_ch1()->SetRemoteIceCredentials(kIceUfrag[3], kIcePwd[3]);

  // Pause sending ep2's candidates to ep1 until ep1 receives the peer reflexive
  // candidate.
  PauseCandidates(1);

  // The caller should have the best connection connected to the peer reflexive
  // candidate.
  const cricket::Connection* best_connection = NULL;
  WAIT((best_connection = ep1_ch1()->best_connection()) != NULL, 2000);
  EXPECT_EQ("prflx", ep1_ch1()->best_connection()->remote_candidate().type());

  // Because we don't have a remote pwd, we don't ping yet.
  EXPECT_EQ(kIceUfrag[1],
            ep1_ch1()->best_connection()->remote_candidate().username());
  EXPECT_EQ("", ep1_ch1()->best_connection()->remote_candidate().password());
  EXPECT_TRUE(nullptr == ep1_ch1()->FindNextPingableConnection());

  ep1_ch1()->SetRemoteIceCredentials(kIceUfrag[1], kIcePwd[1]);
  ResumeCandidates(1);

  EXPECT_EQ(kIcePwd[1],
            ep1_ch1()->best_connection()->remote_candidate().password());
  EXPECT_TRUE(nullptr != ep1_ch1()->FindNextPingableConnection());

  WAIT(ep2_ch1()->best_connection() != NULL, 2000);
  // Verify ep1's best connection is updated to use the 'local' candidate.
  EXPECT_EQ_WAIT(
      "local",
      ep1_ch1()->best_connection()->remote_candidate().type(),
      2000);
  EXPECT_EQ(best_connection, ep1_ch1()->best_connection());
  DestroyChannels();
}

// Test that we properly create a connection on a STUN ping from unknown address
// when the signaling is slow and the end points are behind NAT.
TEST_F(P2PTransportChannelTest, PeerReflexiveCandidateBeforeSignalingWithNAT) {
  ConfigureEndpoints(OPEN, NAT_SYMMETRIC,
                     PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     kDefaultStepDelay, kDefaultStepDelay,
                     cricket::ICEPROTO_RFC5245);
  // Emulate no remote credentials coming in.
  set_clear_remote_candidates_ufrag_pwd(false);
  CreateChannels(1);
  // Only have remote credentials come in for ep2, not ep1.
  ep2_ch1()->SetRemoteIceCredentials(kIceUfrag[3], kIcePwd[3]);
  // Pause sending ep2's candidates to ep1 until ep1 receives the peer reflexive
  // candidate.
  PauseCandidates(1);

  // The caller should have the best connection connected to the peer reflexive
  // candidate.
  WAIT(ep1_ch1()->best_connection() != NULL, 2000);
  EXPECT_EQ("prflx", ep1_ch1()->best_connection()->remote_candidate().type());

  // Because we don't have a remote pwd, we don't ping yet.
  EXPECT_EQ(kIceUfrag[1],
            ep1_ch1()->best_connection()->remote_candidate().username());
  EXPECT_EQ("", ep1_ch1()->best_connection()->remote_candidate().password());
  EXPECT_TRUE(nullptr == ep1_ch1()->FindNextPingableConnection());

  ep1_ch1()->SetRemoteIceCredentials(kIceUfrag[1], kIcePwd[1]);
  ResumeCandidates(1);

  EXPECT_EQ(kIcePwd[1],
            ep1_ch1()->best_connection()->remote_candidate().password());
  EXPECT_TRUE(nullptr != ep1_ch1()->FindNextPingableConnection());

  const cricket::Connection* best_connection = NULL;
  WAIT((best_connection = ep2_ch1()->best_connection()) != NULL, 2000);

  // Wait to verify the connection is not culled.
  WAIT(ep1_ch1()->writable(), 2000);
  EXPECT_EQ(ep2_ch1()->best_connection(), best_connection);
  EXPECT_EQ("prflx", ep1_ch1()->best_connection()->remote_candidate().type());
  DestroyChannels();
}

// Test that if remote candidates don't have ufrag and pwd, we still work.
TEST_F(P2PTransportChannelTest, RemoteCandidatesWithoutUfragPwd) {
  set_clear_remote_candidates_ufrag_pwd(true);
  ConfigureEndpoints(OPEN, OPEN,
                     PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     kMinimumStepDelay, kMinimumStepDelay,
                     cricket::ICEPROTO_GOOGLE);
  CreateChannels(1);
  const cricket::Connection* best_connection = NULL;
  // Wait until the callee's connections are created.
  WAIT((best_connection = ep2_ch1()->best_connection()) != NULL, 1000);
  // Wait to see if they get culled; they shouldn't.
  WAIT(ep2_ch1()->best_connection() != best_connection, 1000);
  EXPECT_TRUE(ep2_ch1()->best_connection() == best_connection);
  DestroyChannels();
}

// Test that a host behind NAT cannot be reached when incoming_only
// is set to true.
TEST_F(P2PTransportChannelTest, IncomingOnlyBlocked) {
  ConfigureEndpoints(NAT_FULL_CONE, OPEN,
                     kDefaultPortAllocatorFlags,
                     kDefaultPortAllocatorFlags,
                     kDefaultStepDelay, kDefaultStepDelay,
                     cricket::ICEPROTO_GOOGLE);

  SetAllocatorFlags(0, kOnlyLocalPorts);
  CreateChannels(1);
  ep1_ch1()->set_incoming_only(true);

  // Pump for 1 second and verify that the channels are not connected.
  rtc::Thread::Current()->ProcessMessages(1000);

  EXPECT_FALSE(ep1_ch1()->readable());
  EXPECT_FALSE(ep1_ch1()->writable());
  EXPECT_FALSE(ep2_ch1()->readable());
  EXPECT_FALSE(ep2_ch1()->writable());

  DestroyChannels();
}

// Test that a peer behind NAT can connect to a peer that has
// incoming_only flag set.
TEST_F(P2PTransportChannelTest, IncomingOnlyOpen) {
  ConfigureEndpoints(OPEN, NAT_FULL_CONE,
                     kDefaultPortAllocatorFlags,
                     kDefaultPortAllocatorFlags,
                     kDefaultStepDelay, kDefaultStepDelay,
                     cricket::ICEPROTO_GOOGLE);

  SetAllocatorFlags(0, kOnlyLocalPorts);
  CreateChannels(1);
  ep1_ch1()->set_incoming_only(true);

  EXPECT_TRUE_WAIT_MARGIN(ep1_ch1() != NULL && ep2_ch1() != NULL &&
                          ep1_ch1()->readable() && ep1_ch1()->writable() &&
                          ep2_ch1()->readable() && ep2_ch1()->writable(),
                          1000, 1000);

  DestroyChannels();
}

TEST_F(P2PTransportChannelTest, TestTcpConnectionsFromActiveToPassive) {
  AddAddress(0, kPublicAddrs[0]);
  AddAddress(1, kPublicAddrs[1]);

  SetAllocationStepDelay(0, kMinimumStepDelay);
  SetAllocationStepDelay(1, kMinimumStepDelay);

  int kOnlyLocalTcpPorts = cricket::PORTALLOCATOR_DISABLE_UDP |
                           cricket::PORTALLOCATOR_DISABLE_STUN |
                           cricket::PORTALLOCATOR_DISABLE_RELAY |
                           cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG;
  // Disable all protocols except TCP.
  SetAllocatorFlags(0, kOnlyLocalTcpPorts);
  SetAllocatorFlags(1, kOnlyLocalTcpPorts);

  SetAllowTcpListen(0, true);   // actpass.
  SetAllowTcpListen(1, false);  // active.

  CreateChannels(1);

  EXPECT_TRUE_WAIT(ep1_ch1()->readable() && ep1_ch1()->writable() &&
                   ep2_ch1()->readable() && ep2_ch1()->writable(),
                   1000);
  EXPECT_TRUE(
      ep1_ch1()->best_connection() && ep2_ch1()->best_connection() &&
      LocalCandidate(ep1_ch1())->address().EqualIPs(kPublicAddrs[0]) &&
      RemoteCandidate(ep1_ch1())->address().EqualIPs(kPublicAddrs[1]));

  std::string kTcpProtocol = "tcp";
  EXPECT_EQ(kTcpProtocol, RemoteCandidate(ep1_ch1())->protocol());
  EXPECT_EQ(kTcpProtocol, LocalCandidate(ep1_ch1())->protocol());
  EXPECT_EQ(kTcpProtocol, RemoteCandidate(ep2_ch1())->protocol());
  EXPECT_EQ(kTcpProtocol, LocalCandidate(ep2_ch1())->protocol());

  TestSendRecv(1);
  DestroyChannels();
}

TEST_F(P2PTransportChannelTest, TestBundleAllocatorToBundleAllocator) {
  AddAddress(0, kPublicAddrs[0]);
  AddAddress(1, kPublicAddrs[1]);
  SetAllocatorFlags(
      0, PORTALLOCATOR_ENABLE_BUNDLE | PORTALLOCATOR_ENABLE_SHARED_UFRAG);
  SetAllocatorFlags(
      1, PORTALLOCATOR_ENABLE_BUNDLE | PORTALLOCATOR_ENABLE_SHARED_UFRAG);

  CreateChannels(2);

  EXPECT_TRUE_WAIT(ep1_ch1()->readable() &&
                   ep1_ch1()->writable() &&
                   ep2_ch1()->readable() &&
                   ep2_ch1()->writable(),
                   1000);
  EXPECT_TRUE(ep1_ch1()->best_connection() &&
              ep2_ch1()->best_connection());

  EXPECT_FALSE(ep1_ch2()->readable());
  EXPECT_FALSE(ep1_ch2()->writable());
  EXPECT_FALSE(ep2_ch2()->readable());
  EXPECT_FALSE(ep2_ch2()->writable());

  TestSendRecv(1);  // Only 1 channel is writable per Endpoint.
  DestroyChannels();
}

TEST_F(P2PTransportChannelTest, TestBundleAllocatorToNonBundleAllocator) {
  AddAddress(0, kPublicAddrs[0]);
  AddAddress(1, kPublicAddrs[1]);
  // Enable BUNDLE flag at one side.
  SetAllocatorFlags(
      0, PORTALLOCATOR_ENABLE_BUNDLE | PORTALLOCATOR_ENABLE_SHARED_UFRAG);

  CreateChannels(2);

  EXPECT_TRUE_WAIT(ep1_ch1()->readable() &&
                   ep1_ch1()->writable() &&
                   ep2_ch1()->readable() &&
                   ep2_ch1()->writable(),
                   1000);
  EXPECT_TRUE_WAIT(ep1_ch2()->readable() &&
                   ep1_ch2()->writable() &&
                   ep2_ch2()->readable() &&
                   ep2_ch2()->writable(),
                   1000);

  EXPECT_TRUE(ep1_ch1()->best_connection() &&
              ep2_ch1()->best_connection());
  EXPECT_TRUE(ep1_ch2()->best_connection() &&
              ep2_ch2()->best_connection());

  TestSendRecv(2);
  DestroyChannels();
}

TEST_F(P2PTransportChannelTest, TestIceRoleConflictWithoutBundle) {
  AddAddress(0, kPublicAddrs[0]);
  AddAddress(1, kPublicAddrs[1]);
  TestSignalRoleConflict();
}

TEST_F(P2PTransportChannelTest, TestIceRoleConflictWithBundle) {
  AddAddress(0, kPublicAddrs[0]);
  AddAddress(1, kPublicAddrs[1]);
  SetAllocatorFlags(
      0, PORTALLOCATOR_ENABLE_BUNDLE | PORTALLOCATOR_ENABLE_SHARED_UFRAG);
  SetAllocatorFlags(
      1, PORTALLOCATOR_ENABLE_BUNDLE | PORTALLOCATOR_ENABLE_SHARED_UFRAG);
  TestSignalRoleConflict();
}

// Tests that the ice configs (protocol, tiebreaker and role) can be passed
// down to ports.
TEST_F(P2PTransportChannelTest, TestIceConfigWillPassDownToPort) {
  AddAddress(0, kPublicAddrs[0]);
  AddAddress(1, kPublicAddrs[1]);

  SetIceRole(0, cricket::ICEROLE_CONTROLLING);
  SetIceProtocol(0, cricket::ICEPROTO_GOOGLE);
  SetIceTiebreaker(0, kTiebreaker1);
  SetIceRole(1, cricket::ICEROLE_CONTROLLING);
  SetIceProtocol(1, cricket::ICEPROTO_RFC5245);
  SetIceTiebreaker(1, kTiebreaker2);

  CreateChannels(1);

  EXPECT_EQ_WAIT(2u, ep1_ch1()->ports().size(), 1000);

  const std::vector<cricket::PortInterface *> ports_before = ep1_ch1()->ports();
  for (size_t i = 0; i < ports_before.size(); ++i) {
    EXPECT_EQ(cricket::ICEROLE_CONTROLLING, ports_before[i]->GetIceRole());
    EXPECT_EQ(cricket::ICEPROTO_GOOGLE, ports_before[i]->IceProtocol());
    EXPECT_EQ(kTiebreaker1, ports_before[i]->IceTiebreaker());
  }

  ep1_ch1()->SetIceRole(cricket::ICEROLE_CONTROLLED);
  ep1_ch1()->SetIceProtocolType(cricket::ICEPROTO_RFC5245);
  ep1_ch1()->SetIceTiebreaker(kTiebreaker2);

  const std::vector<cricket::PortInterface *> ports_after = ep1_ch1()->ports();
  for (size_t i = 0; i < ports_after.size(); ++i) {
    EXPECT_EQ(cricket::ICEROLE_CONTROLLED, ports_before[i]->GetIceRole());
    EXPECT_EQ(cricket::ICEPROTO_RFC5245, ports_before[i]->IceProtocol());
    // SetIceTiebreaker after Connect() has been called will fail. So expect the
    // original value.
    EXPECT_EQ(kTiebreaker1, ports_before[i]->IceTiebreaker());
  }

  EXPECT_TRUE_WAIT(ep1_ch1()->readable() &&
                   ep1_ch1()->writable() &&
                   ep2_ch1()->readable() &&
                   ep2_ch1()->writable(),
                   1000);

  EXPECT_TRUE(ep1_ch1()->best_connection() &&
              ep2_ch1()->best_connection());

  TestSendRecv(1);
  DestroyChannels();
}

// This test verifies channel can handle ice messages when channel is in
// hybrid mode.
TEST_F(P2PTransportChannelTest, TestConnectivityBetweenHybridandIce) {
  TestHybridConnectivity(cricket::ICEPROTO_RFC5245);
}

// This test verifies channel can handle Gice messages when channel is in
// hybrid mode.
TEST_F(P2PTransportChannelTest, TestConnectivityBetweenHybridandGice) {
  TestHybridConnectivity(cricket::ICEPROTO_GOOGLE);
}

// Verify that we can set DSCP value and retrieve properly from P2PTC.
TEST_F(P2PTransportChannelTest, TestDefaultDscpValue) {
  AddAddress(0, kPublicAddrs[0]);
  AddAddress(1, kPublicAddrs[1]);

  CreateChannels(1);
  EXPECT_EQ(rtc::DSCP_NO_CHANGE,
            GetEndpoint(0)->cd1_.ch_->DefaultDscpValue());
  EXPECT_EQ(rtc::DSCP_NO_CHANGE,
            GetEndpoint(1)->cd1_.ch_->DefaultDscpValue());
  GetEndpoint(0)->cd1_.ch_->SetOption(
      rtc::Socket::OPT_DSCP, rtc::DSCP_CS6);
  GetEndpoint(1)->cd1_.ch_->SetOption(
      rtc::Socket::OPT_DSCP, rtc::DSCP_CS6);
  EXPECT_EQ(rtc::DSCP_CS6,
            GetEndpoint(0)->cd1_.ch_->DefaultDscpValue());
  EXPECT_EQ(rtc::DSCP_CS6,
            GetEndpoint(1)->cd1_.ch_->DefaultDscpValue());
  GetEndpoint(0)->cd1_.ch_->SetOption(
      rtc::Socket::OPT_DSCP, rtc::DSCP_AF41);
  GetEndpoint(1)->cd1_.ch_->SetOption(
      rtc::Socket::OPT_DSCP, rtc::DSCP_AF41);
  EXPECT_EQ(rtc::DSCP_AF41,
            GetEndpoint(0)->cd1_.ch_->DefaultDscpValue());
  EXPECT_EQ(rtc::DSCP_AF41,
            GetEndpoint(1)->cd1_.ch_->DefaultDscpValue());
}

// Verify IPv6 connection is preferred over IPv4.
TEST_F(P2PTransportChannelTest, TestIPv6Connections) {
  AddAddress(0, kIPv6PublicAddrs[0]);
  AddAddress(0, kPublicAddrs[0]);
  AddAddress(1, kIPv6PublicAddrs[1]);
  AddAddress(1, kPublicAddrs[1]);

  SetAllocationStepDelay(0, kMinimumStepDelay);
  SetAllocationStepDelay(1, kMinimumStepDelay);

  // Enable IPv6
  SetAllocatorFlags(0, cricket::PORTALLOCATOR_ENABLE_IPV6);
  SetAllocatorFlags(1, cricket::PORTALLOCATOR_ENABLE_IPV6);

  CreateChannels(1);

  EXPECT_TRUE_WAIT(ep1_ch1()->readable() && ep1_ch1()->writable() &&
                   ep2_ch1()->readable() && ep2_ch1()->writable(),
                   1000);
  EXPECT_TRUE(
      ep1_ch1()->best_connection() && ep2_ch1()->best_connection() &&
      LocalCandidate(ep1_ch1())->address().EqualIPs(kIPv6PublicAddrs[0]) &&
      RemoteCandidate(ep1_ch1())->address().EqualIPs(kIPv6PublicAddrs[1]));

  TestSendRecv(1);
  DestroyChannels();
}

// Testing forceful TURN connections.
TEST_F(P2PTransportChannelTest, TestForceTurn) {
  ConfigureEndpoints(NAT_PORT_RESTRICTED, NAT_SYMMETRIC,
                     kDefaultPortAllocatorFlags |
                         cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                         cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     kDefaultPortAllocatorFlags |
                         cricket::PORTALLOCATOR_ENABLE_SHARED_SOCKET |
                         cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG,
                     kDefaultStepDelay, kDefaultStepDelay,
                     cricket::ICEPROTO_RFC5245);
  set_force_relay(true);

  SetAllocationStepDelay(0, kMinimumStepDelay);
  SetAllocationStepDelay(1, kMinimumStepDelay);

  CreateChannels(1);

  EXPECT_TRUE_WAIT(ep1_ch1()->readable() &&
                   ep1_ch1()->writable() &&
                   ep2_ch1()->readable() &&
                   ep2_ch1()->writable(),
                   1000);

  EXPECT_TRUE(ep1_ch1()->best_connection() &&
              ep2_ch1()->best_connection());

  EXPECT_EQ("relay", RemoteCandidate(ep1_ch1())->type());
  EXPECT_EQ("relay", LocalCandidate(ep1_ch1())->type());
  EXPECT_EQ("relay", RemoteCandidate(ep2_ch1())->type());
  EXPECT_EQ("relay", LocalCandidate(ep2_ch1())->type());

  TestSendRecv(1);
  DestroyChannels();
}

// Test what happens when we have 2 users behind the same NAT. This can lead
// to interesting behavior because the STUN server will only give out the
// address of the outermost NAT.
class P2PTransportChannelSameNatTest : public P2PTransportChannelTestBase {
 protected:
  void ConfigureEndpoints(Config nat_type, Config config1, Config config2) {
    ASSERT(nat_type >= NAT_FULL_CONE && nat_type <= NAT_SYMMETRIC);
    rtc::NATSocketServer::Translator* outer_nat =
        nat()->AddTranslator(kPublicAddrs[0], kNatAddrs[0],
            static_cast<rtc::NATType>(nat_type - NAT_FULL_CONE));
    ConfigureEndpoint(outer_nat, 0, config1);
    ConfigureEndpoint(outer_nat, 1, config2);
  }
  void ConfigureEndpoint(rtc::NATSocketServer::Translator* nat,
                         int endpoint, Config config) {
    ASSERT(config <= NAT_SYMMETRIC);
    if (config == OPEN) {
      AddAddress(endpoint, kPrivateAddrs[endpoint]);
      nat->AddClient(kPrivateAddrs[endpoint]);
    } else {
      AddAddress(endpoint, kCascadedPrivateAddrs[endpoint]);
      nat->AddTranslator(kPrivateAddrs[endpoint], kCascadedNatAddrs[endpoint],
          static_cast<rtc::NATType>(config - NAT_FULL_CONE))->AddClient(
              kCascadedPrivateAddrs[endpoint]);
    }
  }
};

TEST_F(P2PTransportChannelSameNatTest, TestConesBehindSameCone) {
  ConfigureEndpoints(NAT_FULL_CONE, NAT_FULL_CONE, NAT_FULL_CONE);
  Test(kLocalUdpToStunUdp);
}

// Test what happens when we have multiple available pathways.
// In the future we will try different RTTs and configs for the different
// interfaces, so that we can simulate a user with Ethernet and VPN networks.
class P2PTransportChannelMultihomedTest : public P2PTransportChannelTestBase {
};

// Test that we can establish connectivity when both peers are multihomed.
TEST_F(P2PTransportChannelMultihomedTest, DISABLED_TestBasic) {
  AddAddress(0, kPublicAddrs[0]);
  AddAddress(0, kAlternateAddrs[0]);
  AddAddress(1, kPublicAddrs[1]);
  AddAddress(1, kAlternateAddrs[1]);
  Test(kLocalUdpToLocalUdp);
}

// Test that we can quickly switch links if an interface goes down.
TEST_F(P2PTransportChannelMultihomedTest, TestFailover) {
  AddAddress(0, kPublicAddrs[0]);
  // Adding alternate address will make sure |kPublicAddrs| has the higher
  // priority than others. This is due to FakeNetwork::AddInterface method.
  AddAddress(1, kAlternateAddrs[1]);
  AddAddress(1, kPublicAddrs[1]);

  // Use only local ports for simplicity.
  SetAllocatorFlags(0, kOnlyLocalPorts);
  SetAllocatorFlags(1, kOnlyLocalPorts);

  // Create channels and let them go writable, as usual.
  CreateChannels(1);
  EXPECT_TRUE_WAIT(ep1_ch1()->readable() && ep1_ch1()->writable() &&
                   ep2_ch1()->readable() && ep2_ch1()->writable(),
                   1000);
  EXPECT_TRUE(
      ep1_ch1()->best_connection() && ep2_ch1()->best_connection() &&
      LocalCandidate(ep1_ch1())->address().EqualIPs(kPublicAddrs[0]) &&
      RemoteCandidate(ep1_ch1())->address().EqualIPs(kPublicAddrs[1]));

  // Blackhole any traffic to or from the public addrs.
  LOG(LS_INFO) << "Failing over...";
  fw()->AddRule(false, rtc::FP_ANY, rtc::FD_ANY,
                kPublicAddrs[1]);

  // We should detect loss of connectivity within 5 seconds or so.
  EXPECT_TRUE_WAIT(!ep1_ch1()->writable(), 7000);

  // We should switch over to use the alternate addr immediately
  // when we lose writability.
  EXPECT_TRUE_WAIT(
      ep1_ch1()->best_connection() && ep2_ch1()->best_connection() &&
      LocalCandidate(ep1_ch1())->address().EqualIPs(kPublicAddrs[0]) &&
      RemoteCandidate(ep1_ch1())->address().EqualIPs(kAlternateAddrs[1]),
      3000);

  DestroyChannels();
}

// Test that we can switch links in a coordinated fashion.
TEST_F(P2PTransportChannelMultihomedTest, TestDrain) {
  AddAddress(0, kPublicAddrs[0]);
  AddAddress(1, kPublicAddrs[1]);
  // Use only local ports for simplicity.
  SetAllocatorFlags(0, kOnlyLocalPorts);
  SetAllocatorFlags(1, kOnlyLocalPorts);

  // Create channels and let them go writable, as usual.
  CreateChannels(1);
  EXPECT_TRUE_WAIT(ep1_ch1()->readable() && ep1_ch1()->writable() &&
                   ep2_ch1()->readable() && ep2_ch1()->writable(),
                   1000);
  EXPECT_TRUE(
      ep1_ch1()->best_connection() && ep2_ch1()->best_connection() &&
      LocalCandidate(ep1_ch1())->address().EqualIPs(kPublicAddrs[0]) &&
      RemoteCandidate(ep1_ch1())->address().EqualIPs(kPublicAddrs[1]));

  // Remove the public interface, add the alternate interface, and allocate
  // a new generation of candidates for the new interface (via Connect()).
  LOG(LS_INFO) << "Draining...";
  AddAddress(1, kAlternateAddrs[1]);
  RemoveAddress(1, kPublicAddrs[1]);
  ep2_ch1()->Connect();

  // We should switch over to use the alternate address after
  // an exchange of pings.
  EXPECT_TRUE_WAIT(
      ep1_ch1()->best_connection() && ep2_ch1()->best_connection() &&
      LocalCandidate(ep1_ch1())->address().EqualIPs(kPublicAddrs[0]) &&
      RemoteCandidate(ep1_ch1())->address().EqualIPs(kAlternateAddrs[1]),
      3000);

  DestroyChannels();
}
