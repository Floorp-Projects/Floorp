/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <algorithm>
#include <deque>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "sigslot.h"

#include "logging.h"
#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include "mozilla/Preferences.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"

#include "nricectxhandler.h"
#include "nricemediastream.h"
#include "nriceresolverfake.h"
#include "nriceresolver.h"
#include "nrinterfaceprioritizer.h"
#include "gtest_ringbuffer_dumper.h"
#include "rlogconnector.h"
#include "runnable_utils.h"
#include "stunserver.h"
#include "nr_socket_prsock.h"
#include "test_nr_socket.h"
#include "ice_ctx.h"
#include "stun_socket_filter.h"
#include "mozilla/net/DNS.h"

#include "ice_ctx.h"
#include "ice_peer_ctx.h"
#include "ice_media_stream.h"

extern "C" {
#include "async_timer.h"
#include "r_data.h"
#include "util.h"
#include "r_time.h"
}

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"


using namespace mozilla;

static unsigned int kDefaultTimeout = 7000;

//TODO(nils@mozilla.com): This should get replaced with some non-external
//solution like discussed in bug 860775.
const std::string kDefaultStunServerHostname(
    (char *)"global.stun.twilio.com");
const std::string kBogusStunServerHostname(
    (char *)"stun-server-nonexistent.invalid");
const uint16_t kDefaultStunServerPort=3478;
const std::string kBogusIceCandidate(
    (char *)"candidate:0 2 UDP 2113601790 192.168.178.20 50769 typ");

const std::string kUnreachableHostIceCandidate(
    (char *)"candidate:0 1 UDP 2113601790 192.168.178.20 50769 typ host");

namespace {

// DNS resolution helper code
static std::string
Resolve(const std::string& fqdn, int address_family)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = address_family;
  hints.ai_protocol = IPPROTO_UDP;
  struct addrinfo *res;
  int err = getaddrinfo(fqdn.c_str(), nullptr, &hints, &res);
  if (err) {
    std::cerr << "Error in getaddrinfo: " << err << std::endl;
    return "";
  }

  char str_addr[64] = {0};
  switch (res->ai_family) {
    case AF_INET:
      inet_ntop(
          AF_INET,
          &reinterpret_cast<struct sockaddr_in*>(res->ai_addr)->sin_addr,
          str_addr,
          sizeof(str_addr));
      break;
    case AF_INET6:
      inet_ntop(
          AF_INET6,
          &reinterpret_cast<struct sockaddr_in6*>(res->ai_addr)->sin6_addr,
          str_addr,
          sizeof(str_addr));
      break;
    default:
      std::cerr << "Got unexpected address family in DNS lookup: "
                << res->ai_family << std::endl;
      freeaddrinfo(res);
      return "";
  }

  if (!strlen(str_addr)) {
    std::cerr << "inet_ntop failed" << std::endl;
  }

  freeaddrinfo(res);
  return str_addr;
}

class StunTest : public MtransportTest {
public:
  StunTest() : MtransportTest() {
    stun_server_hostname_ = kDefaultStunServerHostname;
  }

  void SetUp() override {
    MtransportTest::SetUp();

    // If only a STUN server FQDN was provided, look up its IP address for the
    // address-only tests.
    if (stun_server_address_.empty() && !stun_server_hostname_.empty()) {
      stun_server_address_ = Resolve(stun_server_hostname_, AF_INET);
    }

    // Make sure NrIceCtx is in a testable state.
    test_utils_->sts_target()->Dispatch(
        WrapRunnableNM(&NrIceCtx::internal_DeinitializeGlobal),
        NS_DISPATCH_SYNC);

    // NB: NrIceCtx::internal_DeinitializeGlobal destroys the RLogConnector
    // singleton.
    RLogConnector::CreateInstance();

    test_utils_->sts_target()->Dispatch(
        WrapRunnableNM(&TestStunServer::GetInstance, AF_INET),
                       NS_DISPATCH_SYNC);
    test_utils_->sts_target()->Dispatch(
        WrapRunnableNM(&TestStunServer::GetInstance, AF_INET6),
                       NS_DISPATCH_SYNC);

    test_utils_->sts_target()->Dispatch(
        WrapRunnableNM(&TestStunTcpServer::GetInstance, AF_INET),
                       NS_DISPATCH_SYNC);
    test_utils_->sts_target()->Dispatch(
        WrapRunnableNM(&TestStunTcpServer::GetInstance, AF_INET6),
                       NS_DISPATCH_SYNC);
  }

  void TearDown() override {
    test_utils_->sts_target()->Dispatch(
        WrapRunnableNM(&NrIceCtx::internal_DeinitializeGlobal),
        NS_DISPATCH_SYNC);

    test_utils_->sts_target()->Dispatch(
        WrapRunnableNM(&TestStunServer::ShutdownInstance), NS_DISPATCH_SYNC);

    test_utils_->sts_target()->Dispatch(
        WrapRunnableNM(&TestStunTcpServer::ShutdownInstance), NS_DISPATCH_SYNC);

    RLogConnector::DestroyInstance();

    MtransportTest::TearDown();
  }
};

enum TrickleMode { TRICKLE_NONE, TRICKLE_SIMULATE, TRICKLE_REAL };

enum ConsentStatus { CONSENT_FRESH, CONSENT_STALE, CONSENT_EXPIRED};

const unsigned int ICE_TEST_PEER_OFFERER = (1 << 0);
const unsigned int ICE_TEST_PEER_ALLOW_LOOPBACK = (1 << 1);
const unsigned int ICE_TEST_PEER_ENABLED_TCP = (1 << 2);
const unsigned int ICE_TEST_PEER_ALLOW_LINK_LOCAL = (1 << 3);

typedef std::string (*CandidateFilter)(const std::string& candidate);

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

static std::string IsSrflxCandidate(const std::string& candidate) {
  std::vector<std::string> tokens = split(candidate, ' ');
  if ((tokens.at(6) == "typ") && (tokens.at(7) == "srflx")) {
    return candidate;
  }
  return std::string();
}

static std::string IsRelayCandidate(const std::string& candidate) {
  if (candidate.find("typ relay") != std::string::npos) {
    return candidate;
  }
  return std::string();
}

static std::string IsTcpCandidate(const std::string& candidate) {
  if (candidate.find("TCP") != std::string::npos) {
    return candidate;
  }
  return std::string();
}

static std::string IsTcpSoCandidate(const std::string& candidate) {
  if (candidate.find("tcptype so") != std::string::npos) {
      return candidate;
    }
  return std::string();
}

static std::string IsLoopbackCandidate(const std::string& candidate) {
  if (candidate.find("127.0.0.") != std::string::npos) {
    return candidate;
  }
  return std::string();
}

static std::string IsIpv4Candidate(const std::string& candidate) {
  std::vector<std::string> tokens = split(candidate, ' ');
  if (tokens.at(4).find(":") == std::string::npos) {
    return candidate;
  }
  return std::string();
}

static std::string SabotageHostCandidateAndDropReflexive(
    const std::string& candidate) {
  if (candidate.find("typ srflx") != std::string::npos) {
    return std::string();
  }

  if (candidate.find("typ host") != std::string::npos) {
    return kUnreachableHostIceCandidate;
  }

  return candidate;
}

bool ContainsSucceededPair(const std::vector<NrIceCandidatePair>& pairs) {
  for (size_t i = 0; i < pairs.size(); ++i) {
    if (pairs[i].state == NrIceCandidatePair::STATE_SUCCEEDED) {
      return true;
    }
  }
  return false;
}

// Note: Does not correspond to any notion of prioritization; this is just
// so we can use stl containers/algorithms that need a comparator
bool operator<(const NrIceCandidate& lhs,
               const NrIceCandidate& rhs) {
  if (lhs.cand_addr.host == rhs.cand_addr.host) {
    if (lhs.cand_addr.port == rhs.cand_addr.port) {
      if (lhs.cand_addr.transport == rhs.cand_addr.transport) {
        if (lhs.type == rhs.type) {
          return lhs.tcp_type < rhs.tcp_type;
        }
        return lhs.type < rhs.type;
      }
      return lhs.cand_addr.transport < rhs.cand_addr.transport;
    }
    return lhs.cand_addr.port < rhs.cand_addr.port;
  }
  return lhs.cand_addr.host < rhs.cand_addr.host;
}

bool operator==(const NrIceCandidate& lhs,
                const NrIceCandidate& rhs) {
  return !((lhs < rhs) || (rhs < lhs));
}

class IceCandidatePairCompare {
  public:
    bool operator()(const NrIceCandidatePair& lhs,
                    const NrIceCandidatePair& rhs) const {
      if (lhs.priority == rhs.priority) {
        if (lhs.local == rhs.local) {
          if (lhs.remote == rhs.remote) {
            return lhs.codeword < rhs.codeword;
          }
          return lhs.remote < rhs.remote;
        }
        return lhs.local < rhs.local;
      }
      return lhs.priority < rhs.priority;
    }
};

class IceTestPeer;

class SchedulableTrickleCandidate {
  public:
    SchedulableTrickleCandidate(IceTestPeer *peer,
                                size_t stream,
                                const std::string &candidate,
                                MtransportTestUtils* utils) :
      peer_(peer),
      stream_(stream),
      candidate_(candidate),
      timer_handle_(nullptr),
      test_utils_(utils) {
    }

    ~SchedulableTrickleCandidate() {
      if (timer_handle_)
        NR_async_timer_cancel(timer_handle_);
    }

    void Schedule(unsigned int ms) {
      test_utils_->sts_target()->Dispatch(
          WrapRunnable(this, &SchedulableTrickleCandidate::Schedule_s, ms),
          NS_DISPATCH_SYNC);
    }

    void Schedule_s(unsigned int ms) {
      MOZ_ASSERT(!timer_handle_);
      NR_ASYNC_TIMER_SET(ms, Trickle_cb, this, &timer_handle_);
    }

    static void Trickle_cb(NR_SOCKET s, int how, void *cb_arg) {
      static_cast<SchedulableTrickleCandidate*>(cb_arg)->Trickle();
    }

    void Trickle();

    std::string& Candidate() {
      return candidate_;
    }

    const std::string& Candidate() const {
      return candidate_;
    }

    size_t Stream() const {
      return stream_;
    }

    bool IsHost() const {
      return candidate_.find("typ host") != std::string::npos;
    }

    bool IsReflexive() const {
      return candidate_.find("typ srflx") != std::string::npos;
    }

    bool IsRelay() const {
      return candidate_.find("typ relay") != std::string::npos;
    }

  private:
    IceTestPeer *peer_;
    size_t stream_;
    std::string candidate_;
    void *timer_handle_;
    MtransportTestUtils* test_utils_;

    DISALLOW_COPY_ASSIGN(SchedulableTrickleCandidate);
};

class IceTestPeer : public sigslot::has_slots<> {
 public:
  // TODO(ekr@rtfm.com): Convert to flags when NrIceCtx::Create() does.
  // Bug 1193437.
  IceTestPeer(const std::string& name, MtransportTestUtils* utils,
              bool offerer,
              bool allow_loopback = false, bool enable_tcp = true,
              bool allow_link_local = false,
              NrIceCtx::Policy ice_policy = NrIceCtx::ICE_POLICY_ALL) :
      name_(name),
      ice_ctx_(NrIceCtxHandler::Create(name, offerer, allow_loopback,
                                       enable_tcp, allow_link_local,
                                       ice_policy)),
      candidates_(),
      shutting_down_(false),
      gathering_complete_(false),
      ready_ct_(0),
      ice_connected_(false),
      ice_failed_(false),
      ice_reached_checking_(false),
      received_(0),
      sent_(0),
      fake_resolver_(),
      dns_resolver_(new NrIceResolver()),
      remote_(nullptr),
      candidate_filter_(nullptr),
      expected_local_type_(NrIceCandidate::ICE_HOST),
      expected_local_transport_(kNrIceTransportUdp),
      expected_remote_type_(NrIceCandidate::ICE_HOST),
      trickle_mode_(TRICKLE_NONE),
      trickled_(0),
      simulate_ice_lite_(false),
      nat_(new TestNat),
      test_utils_(utils) {
    ice_ctx_->ctx()->SignalGatheringStateChange.connect(
        this,
        &IceTestPeer::GatheringStateChange);
    ice_ctx_->ctx()->SignalConnectionStateChange.connect(
        this,
        &IceTestPeer::ConnectionStateChange);

    consent_timestamp_.tv_sec = 0;
    consent_timestamp_.tv_usec = 0;
    int r = ice_ctx_->ctx()->SetNat(nat_);
    (void)r;
    MOZ_ASSERT(!r);
  }

  ~IceTestPeer() {
    test_utils_->sts_target()->Dispatch(WrapRunnable(this,
                                                    &IceTestPeer::Shutdown),
        NS_DISPATCH_SYNC);

    // Give the ICE destruction callback time to fire before
    // we destroy the resolver.
    PR_Sleep(1000);
  }

  void AddStream_s(int components) {
    char name[100];
    snprintf(name, sizeof(name), "%s:stream%d", name_.c_str(),
             (int)ice_ctx_->ctx()->GetStreamCount());

    RefPtr<NrIceMediaStream> stream =
        ice_ctx_->CreateStream(static_cast<char *>(name), components);
    ice_ctx_->ctx()->SetStream(ice_ctx_->ctx()->GetStreamCount(), stream);

    ASSERT_TRUE(stream);
    stream->SignalCandidate.connect(this, &IceTestPeer::CandidateInitialized);
    stream->SignalReady.connect(this, &IceTestPeer::StreamReady);
    stream->SignalFailed.connect(this, &IceTestPeer::StreamFailed);
    stream->SignalPacketReceived.connect(this, &IceTestPeer::PacketReceived);
  }

  void AddStream(int components)
  {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this, &IceTestPeer::AddStream_s, components),
        NS_DISPATCH_SYNC);
  }

  void RemoveStream_s(size_t index) {
    ice_ctx_->ctx()->SetStream(index, nullptr);
  }

  void RemoveStream(size_t index) {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this, &IceTestPeer::RemoveStream_s, index),
        NS_DISPATCH_SYNC);
  }

  void SetStunServer(const std::string addr, uint16_t port,
                     const char* transport = kNrIceTransportUdp) {
    if (addr.empty()) {
      // Happens when MOZ_DISABLE_NONLOCAL_CONNECTIONS is set
      return;
    }

    std::vector<NrIceStunServer> stun_servers;
    UniquePtr<NrIceStunServer> server(NrIceStunServer::Create(
        addr, port, transport));
    stun_servers.push_back(*server);
    SetStunServers(stun_servers);
  }

  void SetStunServers(const std::vector<NrIceStunServer> &servers) {
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->ctx()->SetStunServers(servers)));
  }

  void UseTestStunServer() {
    SetStunServer(TestStunServer::GetInstance(AF_INET)->addr(),
                  TestStunServer::GetInstance(AF_INET)->port());
  }

  void SetTurnServer(const std::string addr, uint16_t port,
                     const std::string username,
                     const std::string password,
                     const char* transport) {
    std::vector<unsigned char> password_vec(password.begin(), password.end());
    SetTurnServer(addr, port, username, password_vec, transport);
  }


  void SetTurnServer(const std::string addr, uint16_t port,
                     const std::string username,
                     const std::vector<unsigned char> password,
                     const char* transport) {
    std::vector<NrIceTurnServer> turn_servers;
    UniquePtr<NrIceTurnServer> server(NrIceTurnServer::Create(
        addr, port, username, password, transport));
    turn_servers.push_back(*server);
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->ctx()->SetTurnServers(turn_servers)));
  }

  void SetTurnServers(const std::vector<NrIceTurnServer> servers) {
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->ctx()->SetTurnServers(servers)));
  }

  void SetFakeResolver(const std::string& ip,
                       const std::string& fqdn) {
    ASSERT_TRUE(NS_SUCCEEDED(dns_resolver_->Init()));
    if (!ip.empty() && !fqdn.empty()) {
      PRNetAddr addr;
      PRStatus status = PR_StringToNetAddr(ip.c_str(), &addr);
      addr.inet.port = kDefaultStunServerPort;
      ASSERT_EQ(PR_SUCCESS, status);
      fake_resolver_.SetAddr(fqdn, addr);
    }
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->ctx()->SetResolver(
        fake_resolver_.AllocateResolver())));
  }

  void SetDNSResolver() {
    ASSERT_TRUE(NS_SUCCEEDED(dns_resolver_->Init()));
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->ctx()->SetResolver(
        dns_resolver_->AllocateResolver())));
  }

  void Gather(bool default_route_only = false) {
    nsresult res;

    test_utils_->sts_target()->Dispatch(
        WrapRunnableRet(&res,
                        ice_ctx_->ctx(),
                        &NrIceCtx::StartGathering,
                        default_route_only,
                        false),
        NS_DISPATCH_SYNC);

    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  void UseNat() {
    nat_->enabled_ = true;
  }

  void SetTimerDivider(int div) {
    ice_ctx_->ctx()->internal_SetTimerAccelarator(div);
  }

  void SetStunResponseDelay(uint32_t delay) {
    nat_->delay_stun_resp_ms_ = delay;
  }

  void SetFilteringType(TestNat::NatBehavior type) {
    MOZ_ASSERT(!nat_->has_port_mappings());
    nat_->filtering_type_ = type;
  }

  void SetMappingType(TestNat::NatBehavior type) {
    MOZ_ASSERT(!nat_->has_port_mappings());
    nat_->mapping_type_ = type;
  }

  void SetBlockUdp(bool block) {
    MOZ_ASSERT(!nat_->has_port_mappings());
    nat_->block_udp_ = block;
  }

  void SetBlockStun(bool block) {
    nat_->block_stun_ = block;
  }

  // Get various pieces of state
  std::vector<std::string> GetGlobalAttributes() {
    std::vector<std::string> attrs(ice_ctx_->ctx()->GetGlobalAttributes());
    if (simulate_ice_lite_) {
      attrs.push_back("ice-lite");
    }
    return attrs;
  }

  std::vector<std::string> GetCandidates(size_t stream) {
    std::vector<std::string> v;

    RUN_ON_THREAD(
        test_utils_->sts_target(),
        WrapRunnableRet(&v, this, &IceTestPeer::GetCandidates_s, stream));

    return v;
  }

  std::string FilterCandidate(const std::string& candidate) {
    if (candidate_filter_) {
      return candidate_filter_(candidate);
    }
    return candidate;
  }

  std::vector<std::string> GetCandidates_s(size_t stream) {
    std::vector<std::string> candidates;

    if (stream >= ice_ctx_->ctx()->GetStreamCount() ||
        !ice_ctx_->ctx()->GetStream(stream)) {
      EXPECT_TRUE(false) << "No such stream " << stream;
      return candidates;
    }

    std::vector<std::string> candidates_in =
      ice_ctx_->ctx()->GetStream(stream)->GetCandidates();

    for (size_t i=0; i < candidates_in.size(); i++) {
      std::string candidate(FilterCandidate(candidates_in[i]));
      if (!candidate.empty()) {
        std::cerr << name_ << " Returning candidate: "
                           << candidate << std::endl;
        candidates.push_back(candidate);
      }
    }

    return candidates;
  }

  void SetExpectedTypes(NrIceCandidate::Type local,
                        NrIceCandidate::Type remote,
                        std::string local_transport = kNrIceTransportUdp) {
    expected_local_type_ = local;
    expected_local_transport_ = local_transport;
    expected_remote_type_ = remote;
  }

  void SetExpectedRemoteCandidateAddr(const std::string& addr) {
    expected_remote_addr_ = addr;
  }

  int GetCandidatesPrivateIpv4Range(size_t stream) {
    std::vector<std::string> candidates = GetCandidates(stream);

    int host_net = 0;
    for (auto c : candidates) {
      if (c.find("typ host") != std::string::npos) {
        nr_transport_addr addr;
        std::vector<std::string> tokens = split(c, ' ');
        int r = nr_str_port_to_transport_addr(tokens.at(4).c_str(), 0, IPPROTO_UDP, &addr);
        MOZ_ASSERT(!r);
        if (!r && (addr.ip_version == NR_IPV4)) {
          int n = nr_transport_addr_get_private_addr_range(&addr);
          if (n) {
            if (host_net) {
              // TODO: add support for multiple private interfaces
              std::cerr << "This test doesn't support multiple private interfaces";
              return -1;
            }
            host_net = n;
          }
        }
      }
    }
    return host_net;
  }

  bool gathering_complete() { return gathering_complete_; }
  int ready_ct() { return ready_ct_; }
  bool is_ready_s(size_t stream) {
    RefPtr<NrIceMediaStream> media_stream = ice_ctx_->ctx()->GetStream(stream);
    if (!media_stream) {
      EXPECT_TRUE(false) << "No such stream " << stream;
      return false;
    }
    return media_stream->state() == NrIceMediaStream::ICE_OPEN;
  }
  bool is_ready(size_t stream)
  {
    bool result;
    test_utils_->sts_target()->Dispatch(
        WrapRunnableRet(&result, this, &IceTestPeer::is_ready_s, stream),
        NS_DISPATCH_SYNC);
    return result;
  }
  bool ice_connected() { return ice_connected_; }
  bool ice_failed() { return ice_failed_; }
  bool ice_reached_checking() { return ice_reached_checking_; }
  size_t received() { return received_; }
  size_t sent() { return sent_; }


  void RestartIce() {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this,
                     &IceTestPeer::RestartIce_s,
                     ice_ctx_->CreateCtx()),
        NS_DISPATCH_SYNC);
  }


  void RestartIce_s(RefPtr<NrIceCtx> new_ctx) {
    ice_ctx_->BeginIceRestart(new_ctx);

    // set signals for the newly restarted ctx
    ice_ctx_->ctx()->SignalGatheringStateChange.connect(
        this,
        &IceTestPeer::GatheringStateChange);
    ice_ctx_->ctx()->SignalConnectionStateChange.connect(
        this,
        &IceTestPeer::ConnectionStateChange);

    // take care of some local bookkeeping
    ready_ct_ = 0;
    gathering_complete_ = false;
    ice_connected_ = false;
    ice_failed_ = false;
    ice_reached_checking_ = false;
    remote_ = nullptr;
  }


  void FinalizeIceRestart() {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this, &IceTestPeer::FinalizeIceRestart_s),
        NS_DISPATCH_SYNC);
  }


  void FinalizeIceRestart_s() {
    ice_ctx_->FinalizeIceRestart();
  }


  void RollbackIceRestart() {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this, &IceTestPeer::RollbackIceRestart_s),
        NS_DISPATCH_SYNC);
  }


  void RollbackIceRestart_s() {
    ice_ctx_->RollbackIceRestart();
  }


  // Start connecting to another peer
  void Connect_s(IceTestPeer *remote, TrickleMode trickle_mode,
                 bool start = true) {
    nsresult res;

    remote_ = remote;

    trickle_mode_ = trickle_mode;
    ice_connected_ = false;
    ice_failed_ = false;
    ice_reached_checking_ = false;
    res = ice_ctx_->ctx()->ParseGlobalAttributes(remote->GetGlobalAttributes());
    ASSERT_TRUE(NS_SUCCEEDED(res));

    if (trickle_mode == TRICKLE_NONE ||
        trickle_mode == TRICKLE_REAL) {
      for (size_t i=0; i<ice_ctx_->ctx()->GetStreamCount(); ++i) {
        RefPtr<NrIceMediaStream> aStream = ice_ctx_->ctx()->GetStream(i);
        if (!aStream || aStream->HasParsedAttributes()) {
          continue;
        }
        std::vector<std::string> candidates =
            remote->GetCandidates(i);

        for (size_t j=0; j<candidates.size(); ++j) {
          std::cerr << name_ << " Adding remote candidate: " + candidates[j] << std::endl;
        }
        res = aStream->ParseAttributes(candidates);
        ASSERT_TRUE(NS_SUCCEEDED(res));
      }
    } else {
      // Parse empty attributes and then trickle them out later
      for (size_t i=0; i<ice_ctx_->ctx()->GetStreamCount(); ++i) {
        RefPtr<NrIceMediaStream> aStream = ice_ctx_->ctx()->GetStream(i);
        if (!aStream || aStream->HasParsedAttributes()) {
          continue;
        }
        std::vector<std::string> empty_attrs;
        std::cout << "Calling ParseAttributes on stream " << i << std::endl;
        res = aStream->ParseAttributes(empty_attrs);
        ASSERT_TRUE(NS_SUCCEEDED(res));
      }
    }

    if (start) {
      // Now start checks
      res = ice_ctx_->ctx()->StartChecks();
      ASSERT_TRUE(NS_SUCCEEDED(res));
    }
  }

  void Connect(IceTestPeer *remote, TrickleMode trickle_mode,
               bool start = true) {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(
            this, &IceTestPeer::Connect_s, remote, trickle_mode, start),
        NS_DISPATCH_SYNC);
  }

  void SimulateTrickle(size_t stream) {
    std::cerr << name_ << " Doing trickle for stream " << stream << std::endl;
    // If we are in trickle deferred mode, now trickle in the candidates
    // for |stream|

    // We should be safe here since stream changes happen on STS thread.
    ASSERT_GT(remote_->ice_ctx_->ctx()->GetStreamCount(), stream);
    ASSERT_TRUE(remote_->ice_ctx_->ctx()->GetStream(stream).get());

    std::vector<SchedulableTrickleCandidate*>& candidates =
      ControlTrickle(stream);

    for (auto i = candidates.begin(); i != candidates.end(); ++i) {
      (*i)->Schedule(0);
    }
  }

  // Allows test case to completely control when/if candidates are trickled
  // (test could also do things like insert extra trickle candidates, or
  // change existing ones, or insert duplicates, really anything is fair game)
  std::vector<SchedulableTrickleCandidate*>& ControlTrickle(size_t stream) {
    std::cerr << "Doing controlled trickle for stream " << stream << std::endl;

    std::vector<std::string> candidates =
      remote_->GetCandidates(stream);

    for (size_t j=0; j<candidates.size(); j++) {
      controlled_trickle_candidates_[stream].push_back(
          new SchedulableTrickleCandidate(
              this, stream, candidates[j], test_utils_));
    }

    return controlled_trickle_candidates_[stream];
  }

  nsresult TrickleCandidate_s(const std::string &candidate, size_t stream) {
    if (!ice_ctx_->ctx()->GetStream(stream)) {
      // stream might have gone away before the trickle timer popped
      return NS_OK;
    }
    return ice_ctx_->ctx()->GetStream(stream)->ParseTrickleCandidate(candidate);
  }

  void DumpCandidate(std::string which, const NrIceCandidate& cand) {
    std::string type;
    std::string tcp_type;

    std::string addr;
    int port;

    if (which.find("Remote") != std::string::npos) {
      addr = cand.cand_addr.host;
      port = cand.cand_addr.port;
    }
    else {
      addr = cand.local_addr.host;
      port = cand.local_addr.port;
    }
    switch(cand.type) {
      case NrIceCandidate::ICE_HOST:
        type = "host";
        break;
      case NrIceCandidate::ICE_SERVER_REFLEXIVE:
        type = "srflx";
        break;
      case NrIceCandidate::ICE_PEER_REFLEXIVE:
        type = "prflx";
        break;
      case NrIceCandidate::ICE_RELAYED:
        type = "relay";
        if (which.find("Local") != std::string::npos) {
          type += "(" + cand.local_addr.transport + ")";
        }
        break;
      default:
        FAIL();
    };

    switch(cand.tcp_type) {
      case NrIceCandidate::ICE_NONE:
        break;
      case NrIceCandidate::ICE_ACTIVE:
        tcp_type = " tcptype=active";
        break;
      case NrIceCandidate::ICE_PASSIVE:
        tcp_type = " tcptype=passive";
        break;
      case NrIceCandidate::ICE_SO:
        tcp_type = " tcptype=so";
        break;
      default:
        FAIL();
    };


    std::cerr << which
              << " --> "
              << type
              << " "
              << addr
              << ":"
              << port
              << "/"
              << cand.cand_addr.transport
              << tcp_type
              << " codeword="
              << cand.codeword
              << std::endl;
  }

  void DumpAndCheckActiveCandidates_s() {
    std::cerr << name_ << " Active candidates:" << std::endl;
    for (size_t i=0; i < ice_ctx_->ctx()->GetStreamCount(); ++i) {
      if (!ice_ctx_->ctx()->GetStream(i)) {
        continue;
      }

      for (size_t j=0; j < ice_ctx_->ctx()->GetStream(i)->components(); ++j) {
        std::cerr << name_ << " Stream " << i
                           << " component " << j+1 << std::endl;

        UniquePtr<NrIceCandidate> local;
        UniquePtr<NrIceCandidate> remote;

        nsresult res = ice_ctx_->ctx()->GetStream(i)->GetActivePair(j+1,
                                                                    &local,
                                                                    &remote);
        if (res == NS_ERROR_NOT_AVAILABLE) {
          std::cerr << "Component unpaired or disabled." << std::endl;
        } else {
          ASSERT_TRUE(NS_SUCCEEDED(res));
          DumpCandidate("Local  ", *local);
          /* Depending on timing, and the whims of the network
           * stack/configuration we're running on top of, prflx is always a
           * possibility. */
          if (expected_local_type_ == NrIceCandidate::ICE_HOST) {
            ASSERT_NE(NrIceCandidate::ICE_SERVER_REFLEXIVE, local->type);
            ASSERT_NE(NrIceCandidate::ICE_RELAYED, local->type);
          } else {
            ASSERT_EQ(expected_local_type_, local->type);
          }
          ASSERT_EQ(expected_local_transport_, local->local_addr.transport);
          DumpCandidate("Remote ", *remote);
          /* Depending on timing, and the whims of the network
           * stack/configuration we're running on top of, prflx is always a
           * possibility. */
          if (expected_remote_type_ == NrIceCandidate::ICE_HOST) {
            ASSERT_NE(NrIceCandidate::ICE_SERVER_REFLEXIVE, remote->type);
            ASSERT_NE(NrIceCandidate::ICE_RELAYED, remote->type);
          } else {
            ASSERT_EQ(expected_remote_type_, remote->type);
          }
          if (!expected_remote_addr_.empty()) {
            ASSERT_EQ(expected_remote_addr_, remote->cand_addr.host);
          }
        }
      }
    }
  }

  void DumpAndCheckActiveCandidates() {
    test_utils_->sts_target()->Dispatch(
      WrapRunnable(this, &IceTestPeer::DumpAndCheckActiveCandidates_s),
      NS_DISPATCH_SYNC);
  }

  void Close() {
    test_utils_->sts_target()->Dispatch(
      WrapRunnable(ice_ctx_->ctx(), &NrIceCtx::destroy_peer_ctx),
      NS_DISPATCH_SYNC);
  }

  void Shutdown() {
    std::cerr << name_ << " Shutdown" << std::endl;
    shutting_down_ = true;
    for (auto s = controlled_trickle_candidates_.begin();
         s != controlled_trickle_candidates_.end();
         ++s) {
      for (auto cand = s->second.begin(); cand != s->second.end(); ++cand) {
        delete *cand;
      }
    }

    ice_ctx_ = nullptr;

    if (remote_) {
      remote_->UnsetRemote();
      remote_ = nullptr;
    }
  }

  void UnsetRemote()
  {
    remote_ = nullptr;
  }

  void StartChecks() {
    nsresult res;

    // Now start checks
    test_utils_->sts_target()->Dispatch(
        WrapRunnableRet(&res, ice_ctx_->ctx(), &NrIceCtx::StartChecks),
        NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  // Handle events
  void GatheringStateChange(NrIceCtx* ctx,
                            NrIceCtx::GatheringState state) {
    if (shutting_down_) {
      return;
    }
    if (state != NrIceCtx::ICE_CTX_GATHER_COMPLETE) {
      return;
    }

    std::cerr << name_ << " Gathering complete" << std::endl;
    gathering_complete_ = true;

    std::cerr << name_ << " CANDIDATES:" << std::endl;
    for (size_t i=0; i<ice_ctx_->ctx()->GetStreamCount(); ++i) {
      std::cerr << "Stream " << name_ << std::endl;

      if (!ice_ctx_->ctx()->GetStream(i)) {
        std::cerr << "DISABLED" << std::endl;
        continue;
      }

      std::vector<std::string> candidates =
          ice_ctx_->ctx()->GetStream(i)->GetCandidates();

      for(size_t j=0; j<candidates.size(); ++j) {
        std::cerr << candidates[j] << std::endl;
      }
    }
    std::cerr << std::endl;

  }

  void CandidateInitialized(NrIceMediaStream *stream, const std::string &raw_candidate) {
    std::string candidate(FilterCandidate(raw_candidate));
    if (candidate.empty()) {
      return;
    }
    std::cerr << "Candidate for stream " << stream->name() << " initialized: "
      << candidate << std::endl;
    candidates_[stream->name()].push_back(candidate);

    // If we are connected, then try to trickle to the other side.
    if (remote_ && remote_->remote_ && (trickle_mode_ != TRICKLE_SIMULATE)) {
      // first, find the index of the stream we've been given so
      // we can get the corresponding stream on the remote side
      for (size_t i=0; i<ice_ctx_->ctx()->GetStreamCount(); ++i) {
        if (ice_ctx_->ctx()->GetStream(i) == stream) {
          RefPtr<NrIceCtx> ctx = remote_->ice_ctx_->ctx();
          ASSERT_GT(ctx->GetStreamCount(), i);
          nsresult res = ctx->GetStream(i)->ParseTrickleCandidate(candidate);
          ASSERT_TRUE(NS_SUCCEEDED(res));
          ++trickled_;
          return;
        }
      }
      ADD_FAILURE() << "No matching stream found for " << stream;
    }
  }

  nsresult GetCandidatePairs_s(size_t stream_index,
                               std::vector<NrIceCandidatePair>* pairs)
  {
    MOZ_ASSERT(pairs);
    if (stream_index >= ice_ctx_->ctx()->GetStreamCount() ||
        !ice_ctx_->ctx()->GetStream(stream_index)) {
      // Is there a better error for "no such index"?
      ADD_FAILURE() << "No such media stream index: " << stream_index;
      return NS_ERROR_INVALID_ARG;
    }

    return ice_ctx_->ctx()->GetStream(stream_index)->GetCandidatePairs(pairs);
  }

  nsresult GetCandidatePairs(size_t stream_index,
                             std::vector<NrIceCandidatePair>* pairs) {
    nsresult v;
    test_utils_->sts_target()->Dispatch(
        WrapRunnableRet(&v, this,
                        &IceTestPeer::GetCandidatePairs_s,
                        stream_index,
                        pairs),
        NS_DISPATCH_SYNC);
    return v;
  }

  void DumpCandidatePair(const NrIceCandidatePair& pair) {
      std::cerr << std::endl;
      DumpCandidate("Local", pair.local);
      DumpCandidate("Remote", pair.remote);
      std::cerr << "state = " << pair.state
                << " priority = " << pair.priority
                << " nominated = " << pair.nominated
                << " selected = " << pair.selected
                << " codeword = " << pair.codeword << std::endl;
  }

  void DumpCandidatePairs_s(NrIceMediaStream *stream) {
    std::vector<NrIceCandidatePair> pairs;
    nsresult res = stream->GetCandidatePairs(&pairs);
    ASSERT_TRUE(NS_SUCCEEDED(res));

    std::cerr << "Begin list of candidate pairs [" << std::endl;

    for (std::vector<NrIceCandidatePair>::iterator p = pairs.begin();
         p != pairs.end(); ++p) {
      DumpCandidatePair(*p);
    }
    std::cerr << "]" << std::endl;
  }

  void DumpCandidatePairs_s() {
    std::cerr << "Dumping candidate pairs for all streams [" << std::endl;
    for (size_t s = 0; s < ice_ctx_->ctx()->GetStreamCount(); ++s) {
      if (!ice_ctx_->ctx()->GetStream(s)) {
        continue;
      }
      DumpCandidatePairs_s(ice_ctx_->ctx()->GetStream(s).get());
    }
    std::cerr << "]" << std::endl;
  }

  bool CandidatePairsPriorityDescending(const std::vector<NrIceCandidatePair>&
                                        pairs) {
    // Verify that priority is descending
    uint64_t priority = std::numeric_limits<uint64_t>::max();

    for (size_t p = 0; p < pairs.size(); ++p) {
      if (priority < pairs[p].priority) {
        std::cerr << "Priority increased in subsequent pairs:" << std::endl;
        DumpCandidatePair(pairs[p-1]);
        DumpCandidatePair(pairs[p]);
        return false;
      } else if (priority == pairs[p].priority) {
        if (!IceCandidatePairCompare()(pairs[p], pairs[p-1]) &&
            !IceCandidatePairCompare()(pairs[p-1], pairs[p])) {
          std::cerr << "Ignoring identical pair from trigger check" << std::endl;
        } else {
          std::cerr << "Duplicate priority in subseqent pairs:" << std::endl;
          DumpCandidatePair(pairs[p-1]);
          DumpCandidatePair(pairs[p]);
          return false;
        }
      }
      priority = pairs[p].priority;
    }
    return true;
  }

  void UpdateAndValidateCandidatePairs(size_t stream_index,
                                       std::vector<NrIceCandidatePair>*
                                       new_pairs) {
    std::vector<NrIceCandidatePair> old_pairs = *new_pairs;
    GetCandidatePairs(stream_index, new_pairs);
    ASSERT_TRUE(CandidatePairsPriorityDescending(*new_pairs)) << "New list of "
            "candidate pairs is either not sorted in priority order, or has "
            "duplicate priorities.";
    ASSERT_TRUE(CandidatePairsPriorityDescending(old_pairs)) << "Old list of "
            "candidate pairs is either not sorted in priority order, or has "
            "duplicate priorities. This indicates some bug in the test case.";
    std::vector<NrIceCandidatePair> added_pairs;
    std::vector<NrIceCandidatePair> removed_pairs;

    // set_difference computes the set of elements that are present in the
    // first set, but not the second
    // NrIceCandidatePair::operator< compares based on the priority, local
    // candidate, and remote candidate in that order. This means this will
    // catch cases where the priority has remained the same, but one of the
    // candidates has changed.
    std::set_difference((*new_pairs).begin(),
                        (*new_pairs).end(),
                        old_pairs.begin(),
                        old_pairs.end(),
                        std::inserter(added_pairs, added_pairs.begin()),
                        IceCandidatePairCompare());

    std::set_difference(old_pairs.begin(),
                        old_pairs.end(),
                        (*new_pairs).begin(),
                        (*new_pairs).end(),
                        std::inserter(removed_pairs, removed_pairs.begin()),
                        IceCandidatePairCompare());

    for (std::vector<NrIceCandidatePair>::iterator a = added_pairs.begin();
         a != added_pairs.end(); ++a) {
        std::cerr << "Found new candidate pair." << std::endl;
        DumpCandidatePair(*a);
    }

    for (std::vector<NrIceCandidatePair>::iterator r = removed_pairs.begin();
         r != removed_pairs.end(); ++r) {
        std::cerr << "Pre-existing candidate pair is now missing:" << std::endl;
        DumpCandidatePair(*r);
    }

    ASSERT_TRUE(removed_pairs.empty()) << "At least one candidate pair has "
                                          "gone missing.";
  }

  void StreamReady(NrIceMediaStream *stream) {
    ++ready_ct_;
    std::cerr << name_ << " Stream ready for " << stream->name()
                       << " ct=" << ready_ct_ << std::endl;
    DumpCandidatePairs_s(stream);
  }
  void StreamFailed(NrIceMediaStream *stream) {
    std::cerr << name_ << " Stream failed for " << stream->name()
                       << " ct=" << ready_ct_ << std::endl;
    DumpCandidatePairs_s(stream);
  }

  void ConnectionStateChange(NrIceCtx* ctx,
                             NrIceCtx::ConnectionState state) {
    (void)ctx;
    switch (state) {
      case NrIceCtx::ICE_CTX_INIT:
        break;
      case NrIceCtx::ICE_CTX_CHECKING:
        std::cerr << name_ << " ICE reached checking" << std::endl;
        ice_reached_checking_ = true;
        break;
      case NrIceCtx::ICE_CTX_CONNECTED:
        std::cerr << name_ << " ICE connected" << std::endl;
        ice_connected_ = true;
        break;
      case NrIceCtx::ICE_CTX_COMPLETED:
        std::cerr << name_ << " ICE completed" << std::endl;
        break;
      case NrIceCtx::ICE_CTX_FAILED:
        std::cerr << name_ << " ICE failed" << std::endl;
        ice_failed_ = true;
        break;
      case NrIceCtx::ICE_CTX_DISCONNECTED:
        std::cerr << name_ << " ICE disconnected" << std::endl;
        ice_connected_ = false;
        break;
      default:
        MOZ_CRASH();
    }
  }

  void PacketReceived(NrIceMediaStream *stream, int component, const unsigned char *data,
                      int len) {
    std::cerr << name_ << ": received " << len << " bytes" << std::endl;
    ++received_;
  }

  void SendPacket(int stream, int component, const unsigned char *data,
                  int len) {
    RefPtr<NrIceMediaStream> media_stream = ice_ctx_->ctx()->GetStream(stream);
    if (!media_stream) {
      ADD_FAILURE() << "No such stream " << stream;
      return;
    }

    ASSERT_TRUE(NS_SUCCEEDED(media_stream->SendPacket(component, data, len)));

    ++sent_;
    std::cerr << name_ << ": sent " << len << " bytes" << std::endl;
  }

  void SendFailure(int stream, int component) {
    RefPtr<NrIceMediaStream> media_stream = ice_ctx_->ctx()->GetStream(stream);
    if (!media_stream) {
      ADD_FAILURE() << "No such stream " << stream;
      return;
    }

    const std::string d("FAIL");
    ASSERT_TRUE(NS_FAILED(media_stream->SendPacket(component,
      reinterpret_cast<const unsigned char *>(d.c_str()), d.length())));

    std::cerr << name_ << ": send failed as expected" << std::endl;
  }

  void SetCandidateFilter(CandidateFilter filter) {
    candidate_filter_ = filter;
  }

  void ParseCandidate_s(size_t i, const std::string& candidate) {
    ASSERT_TRUE(ice_ctx_->ctx()->GetStream(i).get()) << "No such stream " << i;

    std::vector<std::string> attributes;

    attributes.push_back(candidate);
    ice_ctx_->ctx()->GetStream(i)->ParseAttributes(attributes);
  }

  void ParseCandidate(size_t i, const std::string& candidate)
  {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this,
                        &IceTestPeer::ParseCandidate_s,
                        i,
                        candidate),
        NS_DISPATCH_SYNC);
  }

  void DisableComponent_s(size_t stream, int component_id) {
    ASSERT_LT(stream, ice_ctx_->ctx()->GetStreamCount());
    ASSERT_TRUE(ice_ctx_->ctx()->GetStream(stream).get()) << "No such stream "
                                                          << stream;
    nsresult res =
      ice_ctx_->ctx()->GetStream(stream)->DisableComponent(component_id);
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  void DisableComponent(size_t stream, int component_id)
  {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this,
                        &IceTestPeer::DisableComponent_s,
                        stream,
                        component_id),
        NS_DISPATCH_SYNC);
  }

  void AssertConsentRefresh_s(size_t stream, int component_id, ConsentStatus status) {
    ASSERT_LT(stream, ice_ctx_->ctx()->GetStreamCount());
    ASSERT_TRUE(ice_ctx_->ctx()->GetStream(stream).get()) << "No such stream "
                                                          << stream;
    bool can_send;
    struct timeval timestamp;
    nsresult res = ice_ctx_->ctx()->GetStream(stream)->
                    GetConsentStatus(component_id, &can_send, &timestamp);
    ASSERT_TRUE(NS_SUCCEEDED(res));
    if (status == CONSENT_EXPIRED) {
      ASSERT_EQ(can_send, 0);
    } else {
      ASSERT_EQ(can_send, 1);
    }
    if (consent_timestamp_.tv_sec) {
      if (status == CONSENT_FRESH) {
        ASSERT_EQ(r_timeval_cmp(&timestamp, &consent_timestamp_), 1);
      } else {
        ASSERT_EQ(r_timeval_cmp(&timestamp, &consent_timestamp_), 0);
      }
    }
    consent_timestamp_.tv_sec = timestamp.tv_sec;
    consent_timestamp_.tv_usec = timestamp.tv_usec;
    std::cerr << name_ << ": new consent timestamp = " <<
      consent_timestamp_.tv_sec << "." << consent_timestamp_.tv_usec <<
      std::endl;
  }

  void AssertConsentRefresh(ConsentStatus status) {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this,
                        &IceTestPeer::AssertConsentRefresh_s,
                        0,
                        1,
                        status),
        NS_DISPATCH_SYNC);
  }

  void ChangeNetworkState_s(bool online) {
    ice_ctx_->ctx()->UpdateNetworkState(online);
  }

  void ChangeNetworkStateToOffline() {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this,
                        &IceTestPeer::ChangeNetworkState_s,
                        false),
        NS_DISPATCH_SYNC);
  }

  void ChangeNetworkStateToOnline() {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this,
                        &IceTestPeer::ChangeNetworkState_s,
                        true),
        NS_DISPATCH_SYNC);
  }

  int trickled() { return trickled_; }

  void SetControlling(NrIceCtx::Controlling controlling) {
    nsresult res;
    test_utils_->sts_target()->Dispatch(
        WrapRunnableRet(&res, ice_ctx_->ctx(),
                        &NrIceCtx::SetControlling,
                        controlling),
        NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  NrIceCtx::Controlling GetControlling() {
    return ice_ctx_->ctx()->GetControlling();
  }

  void SetTiebreaker(uint64_t tiebreaker) {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(this,
                     &IceTestPeer::SetTiebreaker_s,
                     tiebreaker),
        NS_DISPATCH_SYNC);
  }

  void SetTiebreaker_s(uint64_t tiebreaker) {
    ice_ctx_->ctx()->peer()->tiebreaker = tiebreaker;
  }

  void SimulateIceLite() {
    simulate_ice_lite_ = true;
    SetControlling(NrIceCtx::ICE_CONTROLLED);
  }

  nsresult GetDefaultCandidate(unsigned int stream, NrIceCandidate* cand) {
    nsresult rv;

    test_utils_->sts_target()->Dispatch(
        WrapRunnableRet(&rv, this,
                        &IceTestPeer::GetDefaultCandidate_s,
                        stream, cand),
        NS_DISPATCH_SYNC);

    return rv;
  }

  nsresult GetDefaultCandidate_s(unsigned int stream, NrIceCandidate* cand) {
    return ice_ctx_->ctx()->GetStream(stream)->GetDefaultCandidate(1, cand);
  }

 private:
  std::string name_;
  RefPtr<NrIceCtxHandler> ice_ctx_;
  std::map<std::string, std::vector<std::string> > candidates_;
  // Maps from stream id to list of remote trickle candidates
  std::map<size_t, std::vector<SchedulableTrickleCandidate*> >
    controlled_trickle_candidates_;
  bool shutting_down_;
  bool gathering_complete_;
  int ready_ct_;
  bool ice_connected_;
  bool ice_failed_;
  bool ice_reached_checking_;
  size_t received_;
  size_t sent_;
  struct timeval consent_timestamp_;
  NrIceResolverFake fake_resolver_;
  RefPtr<NrIceResolver> dns_resolver_;
  IceTestPeer *remote_;
  CandidateFilter candidate_filter_;
  NrIceCandidate::Type expected_local_type_;
  std::string expected_local_transport_;
  NrIceCandidate::Type expected_remote_type_;
  std::string expected_remote_addr_;
  TrickleMode trickle_mode_;
  int trickled_;
  bool simulate_ice_lite_;
  RefPtr<mozilla::TestNat> nat_;
  MtransportTestUtils* test_utils_;
};

void SchedulableTrickleCandidate::Trickle() {
  timer_handle_ = nullptr;
  nsresult res = peer_->TrickleCandidate_s(candidate_, stream_);
  ASSERT_TRUE(NS_SUCCEEDED(res));
}

class WebRtcIceGatherTest : public StunTest {
 public:
  void SetUp() override {
    StunTest::SetUp();

    Preferences::SetInt("media.peerconnection.ice.tcp_so_sock_count", 3);

    test_utils_->sts_target()->Dispatch(
        WrapRunnable(TestStunServer::GetInstance(AF_INET),
                     &TestStunServer::Reset),
        NS_DISPATCH_SYNC);
    if (TestStunServer::GetInstance(AF_INET6)) {
      test_utils_->sts_target()->Dispatch(
          WrapRunnable(TestStunServer::GetInstance(AF_INET6),
                       &TestStunServer::Reset),
          NS_DISPATCH_SYNC);
    }
  }

  void TearDown() override {
    peer_ = nullptr;
    StunTest::TearDown();
  }

  void EnsurePeer(const unsigned int flags = ICE_TEST_PEER_OFFERER) {
    if (!peer_) {
      peer_ = MakeUnique<IceTestPeer>("P1", test_utils_,
                                      flags & ICE_TEST_PEER_OFFERER,
                                      flags & ICE_TEST_PEER_ALLOW_LOOPBACK,
                                      flags & ICE_TEST_PEER_ENABLED_TCP,
                                      flags & ICE_TEST_PEER_ALLOW_LINK_LOCAL);
      peer_->AddStream(1);
    }
  }

  void Gather(unsigned int waitTime = kDefaultTimeout) {
    EnsurePeer();
    peer_->Gather();

    if (waitTime) {
      WaitForGather(waitTime);
    }
  }

  void WaitForGather(unsigned int waitTime = kDefaultTimeout) {
    ASSERT_TRUE_WAIT(peer_->gathering_complete(), waitTime);
  }

  void AddStunServerWithResponse(
      const std::string& fake_addr,
      uint16_t fake_port,
      const std::string& fqdn,
      const std::string& proto,
      std::vector<NrIceStunServer>* stun_servers) {
    int family;
    if (fake_addr.find(':') != std::string::npos) {
      family = AF_INET6;
    } else {
      family = AF_INET;
    }

    std::string stun_addr;
    uint16_t stun_port;
    if (proto == kNrIceTransportUdp) {
      TestStunServer::GetInstance(family)->SetResponseAddr(fake_addr,
                                                           fake_port);
      stun_addr = TestStunServer::GetInstance(family)->addr();
      stun_port = TestStunServer::GetInstance(family)->port();
    } else if (proto == kNrIceTransportTcp) {
      TestStunTcpServer::GetInstance(family)->SetResponseAddr(fake_addr,
                                                              fake_port);
      stun_addr = TestStunTcpServer::GetInstance(family)->addr();
      stun_port = TestStunTcpServer::GetInstance(family)->port();
    } else {
      MOZ_CRASH();
    }

    if (!fqdn.empty()) {
      peer_->SetFakeResolver(stun_addr, fqdn);
      stun_addr = fqdn;
    }

    stun_servers->push_back(*NrIceStunServer::Create(stun_addr,
                                                     stun_port,
                                                     proto.c_str()));
  }

  void UseFakeStunUdpServerWithResponse(
      const std::string& fake_addr,
      uint16_t fake_port,
      const std::string& fqdn = std::string()) {
    EnsurePeer();
    std::vector<NrIceStunServer> stun_servers;
    AddStunServerWithResponse(fake_addr, fake_port, fqdn, "udp", &stun_servers);
    peer_->SetStunServers(stun_servers);
  }

  void UseFakeStunTcpServerWithResponse(
      const std::string& fake_addr,
      uint16_t fake_port,
      const std::string& fqdn = std::string()) {
    EnsurePeer(ICE_TEST_PEER_OFFERER | ICE_TEST_PEER_ENABLED_TCP);
    std::vector<NrIceStunServer> stun_servers;
    AddStunServerWithResponse(fake_addr, fake_port, fqdn, "tcp", &stun_servers);
    peer_->SetStunServers(stun_servers);
  }

  void UseFakeStunUdpTcpServersWithResponse(
      const std::string& fake_udp_addr,
      uint16_t fake_udp_port,
      const std::string& fake_tcp_addr,
      uint16_t fake_tcp_port) {
    EnsurePeer(ICE_TEST_PEER_OFFERER | ICE_TEST_PEER_ENABLED_TCP);
    std::vector<NrIceStunServer> stun_servers;
    AddStunServerWithResponse(fake_udp_addr,
                              fake_udp_port,
                              "", // no fqdn
                              "udp",
                              &stun_servers);
    AddStunServerWithResponse(fake_tcp_addr,
                              fake_tcp_port,
                              "", // no fqdn
                              "tcp",
                              &stun_servers);

    peer_->SetStunServers(stun_servers);
  }

  void UseTestStunServer() {
    TestStunServer::GetInstance(AF_INET)->Reset();
    peer_->SetStunServer(TestStunServer::GetInstance(AF_INET)->addr(),
                         TestStunServer::GetInstance(AF_INET)->port());
  }

  // NB: Only does substring matching, watch out for stuff like "1.2.3.4"
  // matching "21.2.3.47". " 1.2.3.4 " should not have false positives.
  bool StreamHasMatchingCandidate(unsigned int stream,
                                  const std::string& match,
                                  const std::string& match2 = "") {
    std::vector<std::string> candidates = peer_->GetCandidates(stream);
    for (size_t c = 0; c < candidates.size(); ++c) {
      if (std::string::npos != candidates[c].find(match)) {
        if (!match2.length() ||
            std::string::npos != candidates[c].find(match2)) {
          return true;
        }
      }
    }
    return false;
  }

  void DumpCandidates(unsigned int stream) {
    std::vector<std::string> candidates = peer_->GetCandidates(stream);

    std::cerr << "Candidates for stream " << stream << "->"
              << candidates.size() << std::endl;

    for (auto c : candidates) {
      std::cerr << "Candidate: " << c << std::endl;
    }
  }

 protected:
  mozilla::UniquePtr<IceTestPeer> peer_;
};

class WebRtcIceConnectTest : public StunTest {
 public:
  WebRtcIceConnectTest() :
    initted_(false),
    test_stun_server_inited_(false),
    use_nat_(false),
    filtering_type_(TestNat::ENDPOINT_INDEPENDENT),
    mapping_type_(TestNat::ENDPOINT_INDEPENDENT),
    block_udp_(false) {}

  void SetUp() override {
    StunTest::SetUp();

    nsresult rv;
    target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    ASSERT_TRUE(NS_SUCCEEDED(rv));
  }

  void TearDown() override {
    p1_ = nullptr;
    p2_ = nullptr;

    StunTest::TearDown();
  }

  void AddStream(int components) {
    Init(false, false);
    p1_->AddStream(components);
    p2_->AddStream(components);
  }

  void RemoveStream(size_t index) {
    p1_->RemoveStream(index);
    p2_->RemoveStream(index);
  }

  void Init(bool allow_loopback,
            bool enable_tcp,
            bool setup_stun_servers = true,
            NrIceCtx::Policy ice_policy = NrIceCtx::ICE_POLICY_ALL) {
    if (initted_) {
      return;
    }

    p1_ = MakeUnique<IceTestPeer>("P1", test_utils_, true, allow_loopback,
                                  enable_tcp, false, ice_policy);
    p2_ = MakeUnique<IceTestPeer>("P2", test_utils_, false, allow_loopback,
                                  enable_tcp, false, ice_policy);
    InitPeer(p1_.get(), setup_stun_servers);
    InitPeer(p2_.get(), setup_stun_servers);

    initted_ = true;
  }

  void InitPeer(IceTestPeer* peer, bool setup_stun_servers = true) {
    if (use_nat_) {
      // If we enable nat simulation, but still use a real STUN server somewhere
      // on the internet, we will see failures if there is a real NAT in
      // addition to our simulated one, particularly if it disallows
      // hairpinning.
      if (setup_stun_servers) {
        InitTestStunServer();
        peer->UseTestStunServer();
      }
      peer->UseNat();
      peer->SetFilteringType(filtering_type_);
      peer->SetMappingType(mapping_type_);
      peer->SetBlockUdp(block_udp_);
    } else if (setup_stun_servers) {
      std::vector<NrIceStunServer> stun_servers;

      stun_servers.push_back(*NrIceStunServer::Create(stun_server_address_,
                                                      kDefaultStunServerPort, kNrIceTransportUdp));
      stun_servers.push_back(*NrIceStunServer::Create(stun_server_address_,
                                                      kDefaultStunServerPort, kNrIceTransportTcp));

      peer->SetStunServers(stun_servers);
    }
  }

  bool Gather(unsigned int waitTime = kDefaultTimeout,
              bool default_route_only = false) {
    Init(false, false);

    return GatherCallerAndCallee(p1_.get(),
                                 p2_.get(),
                                 waitTime,
                                 default_route_only);
  }

  bool GatherCallerAndCallee(IceTestPeer* caller,
                             IceTestPeer* callee,
                             unsigned int waitTime = kDefaultTimeout,
                             bool default_route_only = false) {
    caller->Gather(default_route_only);
    callee->Gather(default_route_only);

    if (waitTime) {
      EXPECT_TRUE_WAIT(caller->gathering_complete(), waitTime);
      if (!caller->gathering_complete())
        return false;
      EXPECT_TRUE_WAIT(callee->gathering_complete(), waitTime);
      if (!callee->gathering_complete())
        return false;
    }
    return true;
  }

  void UseNat() {
    // to be useful, this method should be called before Init
    ASSERT_FALSE(initted_);
    use_nat_ = true;
  }

  void SetFilteringType(TestNat::NatBehavior type) {
    // to be useful, this method should be called before Init
    ASSERT_FALSE(initted_);
    filtering_type_ = type;
  }

  void SetMappingType(TestNat::NatBehavior type) {
    // to be useful, this method should be called before Init
    ASSERT_FALSE(initted_);
    mapping_type_ = type;
  }

  void BlockUdp() {
    // note: |block_udp_| is used only in InitPeer.
    // Use IceTestPeer::SetBlockUdp to act on the peer directly.
    block_udp_ = true;
  }

  void SetupAndCheckConsent() {
    p1_->SetTimerDivider(10);
    p2_->SetTimerDivider(10);
    ASSERT_TRUE(Gather());
    Connect();
    p1_->AssertConsentRefresh(CONSENT_FRESH);
    p2_->AssertConsentRefresh(CONSENT_FRESH);
    SendReceive();
  }

  void AssertConsentRefresh(ConsentStatus status = CONSENT_FRESH) {
    p1_->AssertConsentRefresh(status);
    p2_->AssertConsentRefresh(status);
  }

  void InitTestStunServer() {
    if (test_stun_server_inited_) {
      return;
    }

    std::cerr << "Resetting TestStunServer" << std::endl;
    TestStunServer::GetInstance(AF_INET)->Reset();
    test_stun_server_inited_ = true;
  }

  void UseTestStunServer() {
    InitTestStunServer();
    p1_->UseTestStunServer();
    p2_->UseTestStunServer();
  }

  void SetTurnServer(const std::string addr, uint16_t port,
                     const std::string username,
                     const std::string password,
                     const char* transport = kNrIceTransportUdp) {
    p1_->SetTurnServer(addr, port, username, password, transport);
    p2_->SetTurnServer(addr, port, username, password, transport);
  }

  void SetTurnServers(const std::vector<NrIceTurnServer>& servers) {
    p1_->SetTurnServers(servers);
    p2_->SetTurnServers(servers);
  }

  void SetCandidateFilter(CandidateFilter filter, bool both=true) {
    p1_->SetCandidateFilter(filter);
    if (both) {
      p2_->SetCandidateFilter(filter);
    }
  }

  void Connect() {
    ConnectCallerAndCallee(p1_.get(), p2_.get());
  }

  void ConnectCallerAndCallee(IceTestPeer* caller, IceTestPeer* callee) {
    ASSERT_TRUE(caller->ready_ct() == 0);
    ASSERT_TRUE(caller->ice_connected() == 0);
    ASSERT_TRUE(caller->ice_reached_checking() == 0);
    ASSERT_TRUE(callee->ready_ct() == 0);
    ASSERT_TRUE(callee->ice_connected() == 0);
    ASSERT_TRUE(callee->ice_reached_checking() == 0);

    // IceTestPeer::Connect grabs attributes from the first arg, and
    // gives them to |this|, meaning that callee->Connect(caller, ...)
    // simulates caller sending an offer to callee. Order matters here
    // because it determines which peer is controlling.
    callee->Connect(caller, TRICKLE_NONE);
    caller->Connect(callee, TRICKLE_NONE);

    ASSERT_TRUE_WAIT(caller->ready_ct() == 1 && callee->ready_ct() == 1,
                     kDefaultTimeout);
    ASSERT_TRUE_WAIT(caller->ice_connected() && callee->ice_connected(),
                     kDefaultTimeout);

    ASSERT_TRUE(caller->ice_reached_checking());
    ASSERT_TRUE(callee->ice_reached_checking());

    caller->DumpAndCheckActiveCandidates();
    callee->DumpAndCheckActiveCandidates();
  }

  void SetExpectedTypes(NrIceCandidate::Type local, NrIceCandidate::Type remote,
                        std::string transport = kNrIceTransportUdp) {
    p1_->SetExpectedTypes(local, remote, transport);
    p2_->SetExpectedTypes(local, remote, transport);
  }

  void SetExpectedTypes(NrIceCandidate::Type local1, NrIceCandidate::Type remote1,
                        NrIceCandidate::Type local2, NrIceCandidate::Type remote2) {
    p1_->SetExpectedTypes(local1, remote1);
    p2_->SetExpectedTypes(local2, remote2);
  }

  void SetExpectedRemoteCandidateAddr(const std::string& addr) {
    p1_->SetExpectedRemoteCandidateAddr(addr);
    p2_->SetExpectedRemoteCandidateAddr(addr);
  }

  void ConnectP1(TrickleMode mode = TRICKLE_NONE) {
    p1_->Connect(p2_.get(), mode);
  }

  void ConnectP2(TrickleMode mode = TRICKLE_NONE) {
    p2_->Connect(p1_.get(), mode);
  }

  void WaitForConnectedStreams(int expected_streams = 1) {
    ASSERT_TRUE_WAIT(p1_->ready_ct() == expected_streams &&
                     p2_->ready_ct() == expected_streams, kDefaultTimeout);
    ASSERT_TRUE_WAIT(p1_->ice_connected() && p2_->ice_connected(),
                     kDefaultTimeout);
  }

  void AssertCheckingReached() {
    ASSERT_TRUE(p1_->ice_reached_checking());
    ASSERT_TRUE(p2_->ice_reached_checking());
  }

  void WaitForConnected(unsigned int timeout = kDefaultTimeout) {
    ASSERT_TRUE_WAIT(p1_->ice_connected(), timeout);
    ASSERT_TRUE_WAIT(p2_->ice_connected(), timeout);
  }

  void WaitForGather() {
    ASSERT_TRUE_WAIT(p1_->gathering_complete(), kDefaultTimeout);
    ASSERT_TRUE_WAIT(p2_->gathering_complete(), kDefaultTimeout);
  }

  void WaitForDisconnected(unsigned int timeout = kDefaultTimeout) {
    ASSERT_TRUE(p1_->ice_connected());
    ASSERT_TRUE(p2_->ice_connected());
    ASSERT_TRUE_WAIT(p1_->ice_connected() == 0 &&
                     p2_->ice_connected() == 0,
                     timeout);
  }

  void WaitForFailed(unsigned int timeout = kDefaultTimeout) {
    ASSERT_TRUE_WAIT(p1_->ice_failed() &&
                     p2_->ice_failed(),
                     timeout);
  }

  void ConnectTrickle(TrickleMode trickle = TRICKLE_SIMULATE) {
    p2_->Connect(p1_.get(), trickle);
    p1_->Connect(p2_.get(), trickle);
  }

  void SimulateTrickle(size_t stream) {
    p1_->SimulateTrickle(stream);
    p2_->SimulateTrickle(stream);
    ASSERT_TRUE_WAIT(p1_->is_ready(stream), kDefaultTimeout);
    ASSERT_TRUE_WAIT(p2_->is_ready(stream), kDefaultTimeout);
  }

  void SimulateTrickleP1(size_t stream) {
    p1_->SimulateTrickle(stream);
  }

  void SimulateTrickleP2(size_t stream) {
    p2_->SimulateTrickle(stream);
  }

  void CloseP1() {
    p1_->Close();
  }

  void ConnectThenDelete() {
    p2_->Connect(p1_.get(), TRICKLE_NONE, false);
    p1_->Connect(p2_.get(), TRICKLE_NONE, true);
    test_utils_->sts_target()->Dispatch(WrapRunnable(this,
                                                    &WebRtcIceConnectTest::CloseP1),
                                       NS_DISPATCH_SYNC);
    p2_->StartChecks();

    // Wait to see if we crash
    PR_Sleep(PR_MillisecondsToInterval(kDefaultTimeout));
  }

  // default is p1_ sending to p2_
  void SendReceive() {
    SendReceive(p1_.get(), p2_.get());
  }

  void SendReceive(IceTestPeer *p1, IceTestPeer *p2,
                   bool expect_tx_failure = false,
                   bool expect_rx_failure = false) {
    size_t previousSent = p1->sent();
    size_t previousReceived = p2->received();

    test_utils_->sts_target()->Dispatch(
        WrapRunnable(p1,
                     &IceTestPeer::SendPacket, 0, 1,
                     reinterpret_cast<const unsigned char *>("TEST"), 4),
        NS_DISPATCH_SYNC);

    if (expect_tx_failure) {
      ASSERT_EQ(previousSent, p1->sent());
    } else {
      ASSERT_EQ(previousSent+1, p1->sent());
    }
    if (expect_rx_failure) {
      usleep(1000);
      ASSERT_EQ(previousReceived, p2->received());
    } else {
      ASSERT_TRUE_WAIT(p2->received() == previousReceived+1, 1000);
    }
  }

  void SendFailure() {
    test_utils_->sts_target()->Dispatch(
        WrapRunnable(p1_.get(),
                     &IceTestPeer::SendFailure, 0, 1),
        NS_DISPATCH_SYNC);
  }

 protected:
  bool initted_;
  bool test_stun_server_inited_;
  nsCOMPtr<nsIEventTarget> target_;
  mozilla::UniquePtr<IceTestPeer> p1_;
  mozilla::UniquePtr<IceTestPeer> p2_;
  bool use_nat_;
  TestNat::NatBehavior filtering_type_;
  TestNat::NatBehavior mapping_type_;
  bool block_udp_;
};

class WebRtcIcePrioritizerTest : public StunTest {
 public:
  WebRtcIcePrioritizerTest():
    prioritizer_(nullptr) {}

  ~WebRtcIcePrioritizerTest() {
    if (prioritizer_) {
      nr_interface_prioritizer_destroy(&prioritizer_);
    }
  }

  void SetPriorizer(nr_interface_prioritizer *prioritizer) {
    prioritizer_ = prioritizer;
  }

  void AddInterface(const std::string& num, int type, int estimated_speed) {
    std::string str_addr = "10.0.0." + num;
    std::string ifname = "eth" + num;
    nr_local_addr local_addr;
    local_addr.interface.type = type;
    local_addr.interface.estimated_speed = estimated_speed;

    int r = nr_str_port_to_transport_addr(str_addr.c_str(), 0,
                                          IPPROTO_UDP, &(local_addr.addr));
    ASSERT_EQ(0, r);
    strncpy(local_addr.addr.ifname, ifname.c_str(), MAXIFNAME);

    r = nr_interface_prioritizer_add_interface(prioritizer_, &local_addr);
    ASSERT_EQ(0, r);
    r = nr_interface_prioritizer_sort_preference(prioritizer_);
    ASSERT_EQ(0, r);
  }

  void HasLowerPreference(const std::string& num1, const std::string& num2) {
    std::string key1 = "eth" + num1 + ":10.0.0." + num1;
    std::string key2 = "eth" + num2 + ":10.0.0." + num2;
    UCHAR pref1, pref2;
    int r = nr_interface_prioritizer_get_priority(prioritizer_, key1.c_str(), &pref1);
    ASSERT_EQ(0, r);
    r = nr_interface_prioritizer_get_priority(prioritizer_, key2.c_str(), &pref2);
    ASSERT_EQ(0, r);
    ASSERT_LE(pref1, pref2);
  }

 private:
  nr_interface_prioritizer *prioritizer_;
};

class WebRtcIcePacketFilterTest : public StunTest {
 public:
  WebRtcIcePacketFilterTest(): udp_filter_(nullptr),
                               tcp_filter_(nullptr) {}

  void SetUp() {
    StunTest::SetUp();

    // Set up enough of the ICE ctx to allow the packet filter to work
    ice_ctx_ = NrIceCtxHandler::Create("test", true);

    nsCOMPtr<nsISocketFilterHandler> udp_handler =
      do_GetService(NS_STUN_UDP_SOCKET_FILTER_HANDLER_CONTRACTID);
    ASSERT_TRUE(udp_handler);
    udp_handler->NewFilter(getter_AddRefs(udp_filter_));

    nsCOMPtr<nsISocketFilterHandler> tcp_handler =
      do_GetService(NS_STUN_TCP_SOCKET_FILTER_HANDLER_CONTRACTID);
    ASSERT_TRUE(tcp_handler);
    tcp_handler->NewFilter(getter_AddRefs(tcp_filter_));
  }

  void TearDown() {
    test_utils_->sts_target()->Dispatch(WrapRunnable(this,
                                       &WebRtcIcePacketFilterTest::TearDown_s),
                                       NS_DISPATCH_SYNC);
    StunTest::TearDown();
  }

  void TearDown_s() {
    ice_ctx_ = nullptr;
  }

  void TestIncoming(const uint8_t* data, uint32_t len,
                    uint8_t from_addr, int from_port,
                    bool expected_result) {
    mozilla::net::NetAddr addr;
    MakeNetAddr(&addr, from_addr, from_port);
    bool result;
    nsresult rv = udp_filter_->FilterPacket(&addr, data, len,
                                            nsISocketFilter::SF_INCOMING,
                                            &result);
    ASSERT_EQ(NS_OK, rv);
    ASSERT_EQ(expected_result, result);
  }

  void TestIncomingTcp(const uint8_t* data, uint32_t len,
                       bool expected_result) {
    mozilla::net::NetAddr addr;
    bool result;
    nsresult rv = tcp_filter_->FilterPacket(&addr, data, len,
                                            nsISocketFilter::SF_INCOMING,
                                            &result);
    ASSERT_EQ(NS_OK, rv);
    ASSERT_EQ(expected_result, result);
  }

  void TestIncomingTcpFramed(const uint8_t* data, uint32_t len,
                             bool expected_result) {
    mozilla::net::NetAddr addr;
    bool result;
    uint8_t* framed_data = new uint8_t[len+2];
    framed_data[0] = htons(len);
    memcpy(&framed_data[2], data, len);
    nsresult rv = tcp_filter_->FilterPacket(&addr, framed_data, len+2,
                                            nsISocketFilter::SF_INCOMING,
                                            &result);
    ASSERT_EQ(NS_OK, rv);
    ASSERT_EQ(expected_result, result);
    delete[] framed_data;
  }

  void TestOutgoing(const uint8_t* data, uint32_t len,
                    uint8_t to_addr, int to_port,
                    bool expected_result) {
    mozilla::net::NetAddr addr;
    MakeNetAddr(&addr, to_addr, to_port);
    bool result;
    nsresult rv = udp_filter_->FilterPacket(&addr, data, len,
                                            nsISocketFilter::SF_OUTGOING,
                                            &result);
    ASSERT_EQ(NS_OK, rv);
    ASSERT_EQ(expected_result, result);
  }

  void TestOutgoingTcp(const uint8_t* data, uint32_t len,
                       bool expected_result) {
    mozilla::net::NetAddr addr;
    bool result;
    nsresult rv = tcp_filter_->FilterPacket(&addr, data, len,
                                            nsISocketFilter::SF_OUTGOING,
                                            &result);
    ASSERT_EQ(NS_OK, rv);
    ASSERT_EQ(expected_result, result);
  }

  void TestOutgoingTcpFramed(const uint8_t* data, uint32_t len,
                             bool expected_result) {
    mozilla::net::NetAddr addr;
    bool result;
    uint8_t* framed_data = new uint8_t[len+2];
    framed_data[0] = htons(len);
    memcpy(&framed_data[2], data, len);
    nsresult rv = tcp_filter_->FilterPacket(&addr, framed_data, len+2,
                                            nsISocketFilter::SF_OUTGOING,
                                            &result);
    ASSERT_EQ(NS_OK, rv);
    ASSERT_EQ(expected_result, result);
    delete[] framed_data;
  }

 private:
  void MakeNetAddr(mozilla::net::NetAddr* net_addr,
                   uint8_t last_digit, uint16_t port) {
    net_addr->inet.family = AF_INET;
    net_addr->inet.ip = 192 << 24 | 168 << 16 | 1 << 8 | last_digit;
    net_addr->inet.port = port;
  }

  nsCOMPtr<nsISocketFilter> udp_filter_;
  nsCOMPtr<nsISocketFilter> tcp_filter_;
  RefPtr<NrIceCtxHandler> ice_ctx_;
};
}  // end namespace

TEST_F(WebRtcIceGatherTest, TestGatherFakeStunServerHostnameNoResolver) {
  if (stun_server_hostname_.empty()) {
    return;
  }

  EnsurePeer();
  peer_->SetStunServer(stun_server_hostname_, kDefaultStunServerPort);
  Gather();
}

TEST_F(WebRtcIceGatherTest, TestGatherFakeStunServerTcpHostnameNoResolver) {
  if (stun_server_hostname_.empty()) {
    return;
  }

  EnsurePeer(ICE_TEST_PEER_OFFERER | ICE_TEST_PEER_ENABLED_TCP);
  peer_->SetStunServer(stun_server_hostname_, kDefaultStunServerPort,
    kNrIceTransportTcp);
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " TCP "));
}

TEST_F(WebRtcIceGatherTest, TestGatherFakeStunServerIpAddress) {
  if (stun_server_address_.empty()) {
    return;
  }

  EnsurePeer();
  peer_->SetStunServer(stun_server_address_, kDefaultStunServerPort);
  peer_->SetFakeResolver(stun_server_address_, stun_server_hostname_);
  Gather();
}

TEST_F(WebRtcIceGatherTest, TestGatherStunServerIpAddressNoHost) {
  if (stun_server_address_.empty()) {
    return;
  }

  peer_ = MakeUnique<IceTestPeer>("P1", test_utils_, true, false, false, false, NrIceCtx::ICE_POLICY_NO_HOST);
  peer_->AddStream(1);
  peer_->SetStunServer(stun_server_address_, kDefaultStunServerPort);
  peer_->SetFakeResolver(stun_server_address_, stun_server_hostname_);
  Gather();
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " host "));
}

TEST_F(WebRtcIceGatherTest, TestGatherFakeStunServerHostname) {
  if (stun_server_hostname_.empty()) {
    return;
  }

  EnsurePeer();
  peer_->SetStunServer(stun_server_hostname_, kDefaultStunServerPort);
  peer_->SetFakeResolver(stun_server_address_, stun_server_hostname_);
  Gather();
}

TEST_F(WebRtcIceGatherTest, TestGatherFakeStunBogusHostname) {
  EnsurePeer();
  peer_->SetStunServer(kBogusStunServerHostname, kDefaultStunServerPort);
  peer_->SetFakeResolver(stun_server_address_, stun_server_hostname_);
  Gather();
}

TEST_F(WebRtcIceGatherTest, TestGatherDNSStunServerIpAddress) {
  if (stun_server_address_.empty()) {
    return;
  }

  EnsurePeer();
  peer_->SetStunServer(stun_server_address_, kDefaultStunServerPort);
  peer_->SetDNSResolver();
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " UDP "));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "typ srflx raddr"));
}

TEST_F(WebRtcIceGatherTest, TestGatherDNSStunServerIpAddressTcp) {
  if (stun_server_address_.empty()) {
    return;
  }

  EnsurePeer(ICE_TEST_PEER_OFFERER | ICE_TEST_PEER_ENABLED_TCP);
  peer_->SetStunServer(stun_server_address_, kDefaultStunServerPort,
    kNrIceTransportTcp);
  peer_->SetDNSResolver();
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "tcptype passive"));
  ASSERT_FALSE(StreamHasMatchingCandidate(0, "tcptype passive", " 9 "));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "tcptype so"));
  ASSERT_FALSE(StreamHasMatchingCandidate(0, "tcptype so", " 9 "));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "tcptype active", " 9 "));
}

TEST_F(WebRtcIceGatherTest, TestGatherDNSStunServerHostname) {
  if (stun_server_hostname_.empty()) {
    return;
  }

  EnsurePeer();
  peer_->SetStunServer(stun_server_hostname_, kDefaultStunServerPort);
  peer_->SetDNSResolver();
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " UDP "));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "typ srflx raddr"));
}

TEST_F(WebRtcIceGatherTest, TestGatherDNSStunServerHostnameTcp) {
  EnsurePeer(ICE_TEST_PEER_OFFERER | ICE_TEST_PEER_ENABLED_TCP);
  peer_->SetStunServer(stun_server_hostname_, kDefaultStunServerPort,
    kNrIceTransportTcp);
  peer_->SetDNSResolver();
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "tcptype passive"));
  ASSERT_FALSE(StreamHasMatchingCandidate(0, "tcptype passive", " 9 "));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "tcptype so"));
  ASSERT_FALSE(StreamHasMatchingCandidate(0, "tcptype so", " 9 "));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "tcptype active", " 9 "));
}

TEST_F(WebRtcIceGatherTest, TestGatherDNSStunServerHostnameBothUdpTcp) {
  if (stun_server_hostname_.empty()) {
    return;
  }

  std::vector<NrIceStunServer> stun_servers;

  EnsurePeer(ICE_TEST_PEER_OFFERER | ICE_TEST_PEER_ENABLED_TCP);
  stun_servers.push_back(*NrIceStunServer::Create(stun_server_hostname_,
    kDefaultStunServerPort, kNrIceTransportUdp));
  stun_servers.push_back(*NrIceStunServer::Create(stun_server_hostname_,
    kDefaultStunServerPort, kNrIceTransportTcp));
  peer_->SetStunServers(stun_servers);
  peer_->SetDNSResolver();
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " UDP "));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " TCP "));
}

TEST_F(WebRtcIceGatherTest, TestGatherDNSStunServerIpAddressBothUdpTcp) {
  if (stun_server_address_.empty()) {
    return;
  }

  std::vector<NrIceStunServer> stun_servers;

  EnsurePeer(ICE_TEST_PEER_OFFERER | ICE_TEST_PEER_ENABLED_TCP);
  stun_servers.push_back(*NrIceStunServer::Create(stun_server_address_,
    kDefaultStunServerPort, kNrIceTransportUdp));
  stun_servers.push_back(*NrIceStunServer::Create(stun_server_address_,
    kDefaultStunServerPort, kNrIceTransportTcp));
  peer_->SetStunServers(stun_servers);
  peer_->SetDNSResolver();
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " UDP "));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " TCP "));
}

TEST_F(WebRtcIceGatherTest, TestGatherDNSStunBogusHostname) {
  EnsurePeer();
  peer_->SetStunServer(kBogusStunServerHostname, kDefaultStunServerPort);
  peer_->SetDNSResolver();
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " UDP "));
}

TEST_F(WebRtcIceGatherTest, TestGatherDNSStunBogusHostnameTcp) {
  EnsurePeer(ICE_TEST_PEER_OFFERER | ICE_TEST_PEER_ENABLED_TCP);
  peer_->SetStunServer(kBogusStunServerHostname, kDefaultStunServerPort,
    kNrIceTransportTcp);
  peer_->SetDNSResolver();
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " TCP "));
}

TEST_F(WebRtcIceGatherTest, TestDefaultCandidate) {
  EnsurePeer();
  peer_->SetStunServer(stun_server_hostname_, kDefaultStunServerPort);
  Gather();
  NrIceCandidate default_candidate;
  ASSERT_TRUE(NS_SUCCEEDED(peer_->GetDefaultCandidate(0, &default_candidate)));
}

TEST_F(WebRtcIceGatherTest, TestGatherTurn) {
  EnsurePeer();
  if (turn_server_.empty())
    return;
  peer_->SetTurnServer(turn_server_, kDefaultStunServerPort,
                       turn_user_, turn_password_, kNrIceTransportUdp);
  Gather();
}

TEST_F(WebRtcIceGatherTest, TestGatherTurnTcp) {
  EnsurePeer();
  if (turn_server_.empty())
    return;
  peer_->SetTurnServer(turn_server_, kDefaultStunServerPort,
                       turn_user_, turn_password_, kNrIceTransportTcp);
  Gather();
}

TEST_F(WebRtcIceGatherTest, TestGatherDisableComponent) {
  if (stun_server_hostname_.empty()) {
    return;
  }

  EnsurePeer();
  peer_->SetStunServer(stun_server_hostname_, kDefaultStunServerPort);
  peer_->AddStream(2);
  peer_->DisableComponent(1, 2);
  Gather();
  std::vector<std::string> candidates =
    peer_->GetCandidates(1);

  for (size_t i=0; i<candidates.size(); ++i) {
    size_t sp1 = candidates[i].find(' ');
    ASSERT_EQ(0, candidates[i].compare(sp1+1, 1, "1", 1));
  }
}

TEST_F(WebRtcIceGatherTest, TestGatherVerifyNoLoopback) {
  Gather();
  ASSERT_FALSE(StreamHasMatchingCandidate(0, "127.0.0.1"));
}

TEST_F(WebRtcIceGatherTest, TestGatherAllowLoopback) {
  // Set up peer with loopback allowed.
  peer_ = MakeUnique<IceTestPeer>("P1", test_utils_, true, true);
  peer_->AddStream(1);
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "127.0.0.1"));
}

TEST_F(WebRtcIceGatherTest, TestGatherTcpDisabled) {
  // Set up peer with tcp disabled.
  peer_ = MakeUnique<IceTestPeer>("P1", test_utils_, true, false, false);
  peer_->AddStream(1);
  Gather();
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " TCP "));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " UDP "));
}

// Verify that a bogus candidate doesn't cause crashes on the
// main thread. See bug 856433.
TEST_F(WebRtcIceGatherTest, TestBogusCandidate) {
  Gather();
  peer_->ParseCandidate(0, kBogusIceCandidate);
}

TEST_F(WebRtcIceGatherTest, VerifyTestStunServer) {
  UseFakeStunUdpServerWithResponse("192.0.2.133", 3333);
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " 192.0.2.133 3333 "));
}

TEST_F(WebRtcIceGatherTest, VerifyTestStunTcpServer) {
  UseFakeStunTcpServerWithResponse("192.0.2.233", 3333);
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " 192.0.2.233 3333 typ srflx",
    " tcptype "));
}

TEST_F(WebRtcIceGatherTest, VerifyTestStunServerV6) {
  if (!TestStunServer::GetInstance(AF_INET6)) {
    // No V6 addresses
    return;
  }
  UseFakeStunUdpServerWithResponse("beef::", 3333);
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " beef:: 3333 "));
}

TEST_F(WebRtcIceGatherTest, VerifyTestStunServerFQDN) {
  UseFakeStunUdpServerWithResponse("192.0.2.133", 3333, "stun.example.com");
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " 192.0.2.133 3333 "));
}

TEST_F(WebRtcIceGatherTest, VerifyTestStunServerV6FQDN) {
  if (!TestStunServer::GetInstance(AF_INET6)) {
    // No V6 addresses
    return;
  }
  UseFakeStunUdpServerWithResponse("beef::", 3333, "stun.example.com");
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " beef:: 3333 "));
}

TEST_F(WebRtcIceGatherTest, TestStunServerReturnsWildcardAddr) {
  UseFakeStunUdpServerWithResponse("0.0.0.0", 3333);
  Gather(kDefaultTimeout * 3);
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " 0.0.0.0 "));
}

TEST_F(WebRtcIceGatherTest, TestStunServerReturnsWildcardAddrV6) {
  if (!TestStunServer::GetInstance(AF_INET6)) {
    // No V6 addresses
    return;
  }
  UseFakeStunUdpServerWithResponse("::", 3333);
  Gather(kDefaultTimeout * 3);
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " :: "));
}

TEST_F(WebRtcIceGatherTest, TestStunServerReturnsPort0) {
  UseFakeStunUdpServerWithResponse("192.0.2.133", 0);
  Gather(kDefaultTimeout * 3);
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " 192.0.2.133 0 "));
}

TEST_F(WebRtcIceGatherTest, TestStunServerReturnsLoopbackAddr) {
  UseFakeStunUdpServerWithResponse("127.0.0.133", 3333);
  Gather(kDefaultTimeout * 3);
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " 127.0.0.133 "));
}

TEST_F(WebRtcIceGatherTest, TestStunServerReturnsLoopbackAddrV6) {
  if (!TestStunServer::GetInstance(AF_INET6)) {
    // No V6 addresses
    return;
  }
  UseFakeStunUdpServerWithResponse("::1", 3333);
  Gather(kDefaultTimeout * 3);
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " ::1 "));
}

TEST_F(WebRtcIceGatherTest, TestStunServerTrickle) {
  UseFakeStunUdpServerWithResponse("192.0.2.1", 3333);
  TestStunServer::GetInstance(AF_INET)->SetDropInitialPackets(3);
  Gather(0);
  ASSERT_FALSE(StreamHasMatchingCandidate(0, "192.0.2.1"));
  WaitForGather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "192.0.2.1"));
}

// Test no host with our fake STUN server and apparently NATted.
TEST_F(WebRtcIceGatherTest, TestFakeStunServerNatedNoHost) {
  peer_ = MakeUnique<IceTestPeer>("P1", test_utils_, true, false, false, false, NrIceCtx::ICE_POLICY_NO_HOST);
  peer_->AddStream(1);
  UseFakeStunUdpServerWithResponse("192.0.2.1", 3333);
  Gather(0);
  WaitForGather();
  DumpCandidates(0);
  ASSERT_FALSE(StreamHasMatchingCandidate(0, "host"));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "srflx"));
  NrIceCandidate default_candidate;
  nsresult rv = peer_->GetDefaultCandidate(0, &default_candidate);
  if (NS_SUCCEEDED(rv)) {
    ASSERT_NE(NrIceCandidate::ICE_HOST, default_candidate.type);
  }
}

// Test no host with our fake STUN server and apparently non-NATted.
TEST_F(WebRtcIceGatherTest, TestFakeStunServerNoNatNoHost) {
  peer_ = MakeUnique<IceTestPeer>("P1", test_utils_, true, false, false, false, NrIceCtx::ICE_POLICY_NO_HOST);
  peer_->AddStream(1);
  UseTestStunServer();
  Gather(0);
  WaitForGather();
  DumpCandidates(0);
  ASSERT_FALSE(StreamHasMatchingCandidate(0, "host"));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "srflx"));
}

TEST_F(WebRtcIceGatherTest, TestStunTcpServerTrickle) {
  UseFakeStunTcpServerWithResponse("192.0.3.1", 3333);
  TestStunTcpServer::GetInstance(AF_INET)->SetDelay(500);
  Gather(0);
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " 192.0.3.1 ", " tcptype "));
  WaitForGather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " 192.0.3.1 ", " tcptype "));
}

TEST_F(WebRtcIceGatherTest, TestStunTcpAndUdpServerTrickle) {
  UseFakeStunUdpTcpServersWithResponse("192.0.2.1", 3333, "192.0.3.1", 3333);
  TestStunServer::GetInstance(AF_INET)->SetDropInitialPackets(3);
  TestStunTcpServer::GetInstance(AF_INET)->SetDelay(500);
  Gather(0);
  ASSERT_FALSE(StreamHasMatchingCandidate(0, "192.0.2.1", "UDP"));
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " 192.0.3.1 ", " tcptype "));
  WaitForGather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "192.0.2.1", "UDP"));
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " 192.0.3.1 ", " tcptype "));
}

TEST_F(WebRtcIceGatherTest, TestSetIceControlling) {
  EnsurePeer();
  peer_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  NrIceCtx::Controlling controlling = peer_->GetControlling();
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLING, controlling);
  // SetControlling should only allow setting this once
  peer_->SetControlling(NrIceCtx::ICE_CONTROLLED);
  controlling = peer_->GetControlling();
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLING, controlling);
}

TEST_F(WebRtcIceGatherTest, TestSetIceControlled) {
  EnsurePeer();
  peer_->SetControlling(NrIceCtx::ICE_CONTROLLED);
  NrIceCtx::Controlling controlling = peer_->GetControlling();
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLED, controlling);
  // SetControlling should only allow setting this once
  peer_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  controlling = peer_->GetControlling();
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLED, controlling);
}

TEST_F(WebRtcIceConnectTest, TestGather) {
  AddStream(1);
  ASSERT_TRUE(Gather());
}

TEST_F(WebRtcIceConnectTest, TestGatherTcp) {
  Init(false, true);
  AddStream(1);
  ASSERT_TRUE(Gather());
}

TEST_F(WebRtcIceConnectTest, TestGatherAutoPrioritize) {
  Init(false, false);
  AddStream(1);
  ASSERT_TRUE(Gather());
}


TEST_F(WebRtcIceConnectTest, TestConnect) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  Connect();
}


TEST_F(WebRtcIceConnectTest, TestConnectRestartIce) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  Connect();
  SendReceive(p1_.get(), p2_.get());

  p2_->RestartIce();
  ASSERT_FALSE(p2_->gathering_complete());

  // verify p1 and p2 streams are still connected after restarting ice on p2
  SendReceive(p1_.get(), p2_.get());

  mozilla::UniquePtr<IceTestPeer> p3_;
  p3_ = MakeUnique<IceTestPeer>("P3", test_utils_, true, false, false, false);
  InitPeer(p3_.get());
  p3_->AddStream(1);

  p2_->AddStream(1);
  ASSERT_TRUE(GatherCallerAndCallee(p2_.get(), p3_.get()));
  std::cout << "-------------------------------------------------" << std::endl;
  ConnectCallerAndCallee(p3_.get(), p2_.get());
  SendReceive(p1_.get(), p2_.get()); // p1 and p2 still connected
  SendReceive(p3_.get(), p2_.get()); // p3 and p2 are now connected

  p2_->FinalizeIceRestart();
  SendReceive(p3_.get(), p2_.get()); // p3 and p2 are still connected

  SendReceive(p1_.get(), p2_.get(), false, true); // p1 and p2 not connected

  p3_ = nullptr;
}

TEST_F(WebRtcIceConnectTest, TestConnectRestartIceThenAbort) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  Connect();
  SendReceive(p1_.get(), p2_.get());

  p2_->RestartIce();
  ASSERT_FALSE(p2_->gathering_complete());

  // verify p1 and p2 streams are still connected after restarting ice on p2
  SendReceive(p1_.get(), p2_.get());

  mozilla::UniquePtr<IceTestPeer> p3_;
  p3_ = MakeUnique<IceTestPeer>("P3", test_utils_, true, false, false, false);
  InitPeer(p3_.get());
  p3_->AddStream(1);

  p2_->AddStream(1);
  ASSERT_TRUE(GatherCallerAndCallee(p2_.get(), p3_.get()));
  std::cout << "-------------------------------------------------" << std::endl;
  ConnectCallerAndCallee(p3_.get(), p2_.get());
  SendReceive(p1_.get(), p2_.get()); // p1 and p2 still connected
  SendReceive(p3_.get(), p2_.get()); // p3 and p2 are now connected

  p2_->RollbackIceRestart();
  SendReceive(p1_.get(), p2_.get()); // p1 and p2 are still connected

  SendReceive(p3_.get(), p2_.get(), false, true); // p3 and p2 not connected

  p3_ = nullptr;
}

TEST_F(WebRtcIceConnectTest, TestConnectSetControllingAfterIceRestart) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  // Just for fun lets do this with switched rolls
  p1_->SetControlling(NrIceCtx::ICE_CONTROLLED);
  p2_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  Connect();
  SendReceive(p1_.get(), p2_.get());
  // Set rolls should not switch by connecting
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLED, p1_->GetControlling());
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLING, p2_->GetControlling());

  p2_->RestartIce();
  ASSERT_FALSE(p2_->gathering_complete());
  // ICE restart should allow us to set control role again
  p2_->SetControlling(NrIceCtx::ICE_CONTROLLED);
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLED, p2_->GetControlling());
  // But still only allowed to set control role once
  p2_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLED, p2_->GetControlling());

  mozilla::UniquePtr<IceTestPeer> p3_;
  p3_ = MakeUnique<IceTestPeer>("P3", test_utils_, true, false, false, false);
  InitPeer(p3_.get());
  p3_->AddStream(1);
  // Set control role for p3 accordingly (w/o role conflict)
  p3_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLING, p3_->GetControlling());

  p2_->AddStream(1);
  ASSERT_TRUE(GatherCallerAndCallee(p2_.get(), p3_.get()));
  std::cout << "-------------------------------------------------" << std::endl;
  ConnectCallerAndCallee(p3_.get(), p2_.get());
  // Again connecting should not result in role switch
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLED, p2_->GetControlling());
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLING, p3_->GetControlling());

  p2_->FinalizeIceRestart();
  // And again we are not allowed to switch roles at this point any more
  p2_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLED, p2_->GetControlling());
  p3_->SetControlling(NrIceCtx::ICE_CONTROLLED);
  ASSERT_EQ(NrIceCtx::ICE_CONTROLLING, p3_->GetControlling());

  p3_ = nullptr;
}

TEST_F(WebRtcIceConnectTest, TestConnectTcp) {
  Init(false, true);
  AddStream(1);
  ASSERT_TRUE(Gather());
  SetCandidateFilter(IsTcpCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_HOST,
    NrIceCandidate::Type::ICE_HOST, kNrIceTransportTcp);
  Connect();
}

//TCP SO tests works on localhost only with delay applied:
//  tc qdisc add dev lo root netem delay 10ms
TEST_F(WebRtcIceConnectTest, DISABLED_TestConnectTcpSo) {
  Init(false, true);
  AddStream(1);
  ASSERT_TRUE(Gather());
  SetCandidateFilter(IsTcpSoCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_HOST,
    NrIceCandidate::Type::ICE_HOST, kNrIceTransportTcp);
  Connect();
}

// Disabled because this breaks with hairpinning.
TEST_F(WebRtcIceConnectTest, DISABLED_TestConnectNoHost) {
  Init(false, false, false, NrIceCtx::ICE_POLICY_NO_HOST);
  AddStream(1);
  ASSERT_TRUE(Gather());
  SetExpectedTypes(NrIceCandidate::Type::ICE_SERVER_REFLEXIVE,
    NrIceCandidate::Type::ICE_SERVER_REFLEXIVE, kNrIceTransportTcp);
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestLoopbackOnlySortOf) {
  Init(true, false, false);
  AddStream(1);
  SetCandidateFilter(IsLoopbackCandidate);
  ASSERT_TRUE(Gather());
  SetExpectedRemoteCandidateAddr("127.0.0.1");
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectBothControllingP1Wins) {
  AddStream(1);
  p1_->SetTiebreaker(1);
  p2_->SetTiebreaker(0);
  ASSERT_TRUE(Gather());
  p1_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  p2_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectBothControllingP2Wins) {
  AddStream(1);
  p1_->SetTiebreaker(0);
  p2_->SetTiebreaker(1);
  ASSERT_TRUE(Gather());
  p1_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  p2_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectIceLiteOfferer) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  p1_->SimulateIceLite();
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestTrickleBothControllingP1Wins) {
  AddStream(1);
  p1_->SetTiebreaker(1);
  p2_->SetTiebreaker(0);
  ASSERT_TRUE(Gather());
  p1_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  p2_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  ConnectTrickle();
  SimulateTrickle(0);
  WaitForConnected(1000);
  AssertCheckingReached();
}

TEST_F(WebRtcIceConnectTest, TestTrickleBothControllingP2Wins) {
  AddStream(1);
  p1_->SetTiebreaker(0);
  p2_->SetTiebreaker(1);
  ASSERT_TRUE(Gather());
  p1_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  p2_->SetControlling(NrIceCtx::ICE_CONTROLLING);
  ConnectTrickle();
  SimulateTrickle(0);
  WaitForConnected(1000);
  AssertCheckingReached();
}

TEST_F(WebRtcIceConnectTest, TestTrickleIceLiteOfferer) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  p1_->SimulateIceLite();
  ConnectTrickle();
  SimulateTrickle(0);
  WaitForConnected(1000);
  AssertCheckingReached();
}

TEST_F(WebRtcIceConnectTest, TestGatherFullCone) {
  UseNat();
  AddStream(1);
  ASSERT_TRUE(Gather());
}

TEST_F(WebRtcIceConnectTest, TestGatherFullConeAutoPrioritize) {
  UseNat();
  Init(true, false);
  AddStream(1);
  ASSERT_TRUE(Gather());
}


TEST_F(WebRtcIceConnectTest, TestConnectFullCone) {
  UseNat();
  AddStream(1);
  SetExpectedTypes(NrIceCandidate::Type::ICE_SERVER_REFLEXIVE,
                   NrIceCandidate::Type::ICE_SERVER_REFLEXIVE);
  ASSERT_TRUE(Gather());
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectNoNatNoHost) {
  Init(false, false, false, NrIceCtx::ICE_POLICY_NO_HOST);
  AddStream(1);
  UseTestStunServer();
  // Because we are connecting from our host candidate to the
  // other side's apparent srflx (which is also their host)
  // we see a host/srflx pair.
  SetExpectedTypes(NrIceCandidate::Type::ICE_HOST,
                   NrIceCandidate::Type::ICE_SERVER_REFLEXIVE);
  ASSERT_TRUE(Gather());
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectFullConeNoHost) {
  UseNat();
  Init(false, false, false, NrIceCtx::ICE_POLICY_NO_HOST);
  AddStream(1);
  UseTestStunServer();
  SetExpectedTypes(NrIceCandidate::Type::ICE_SERVER_REFLEXIVE,
                   NrIceCandidate::Type::ICE_SERVER_REFLEXIVE);
  ASSERT_TRUE(Gather());
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestGatherAddressRestrictedCone) {
  UseNat();
  SetFilteringType(TestNat::ADDRESS_DEPENDENT);
  SetMappingType(TestNat::ENDPOINT_INDEPENDENT);
  AddStream(1);
  ASSERT_TRUE(Gather());
}

TEST_F(WebRtcIceConnectTest, TestConnectAddressRestrictedCone) {
  UseNat();
  SetFilteringType(TestNat::ADDRESS_DEPENDENT);
  SetMappingType(TestNat::ENDPOINT_INDEPENDENT);
  AddStream(1);
  SetExpectedTypes(NrIceCandidate::Type::ICE_SERVER_REFLEXIVE,
                   NrIceCandidate::Type::ICE_SERVER_REFLEXIVE);
  ASSERT_TRUE(Gather());
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestGatherPortRestrictedCone) {
  UseNat();
  SetFilteringType(TestNat::PORT_DEPENDENT);
  SetMappingType(TestNat::ENDPOINT_INDEPENDENT);
  AddStream(1);
  ASSERT_TRUE(Gather());
}

TEST_F(WebRtcIceConnectTest, TestConnectPortRestrictedCone) {
  UseNat();
  SetFilteringType(TestNat::PORT_DEPENDENT);
  SetMappingType(TestNat::ENDPOINT_INDEPENDENT);
  AddStream(1);
  SetExpectedTypes(NrIceCandidate::Type::ICE_SERVER_REFLEXIVE,
                   NrIceCandidate::Type::ICE_SERVER_REFLEXIVE);
  ASSERT_TRUE(Gather());
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestGatherSymmetricNat) {
  UseNat();
  SetFilteringType(TestNat::PORT_DEPENDENT);
  SetMappingType(TestNat::PORT_DEPENDENT);
  AddStream(1);
  ASSERT_TRUE(Gather());
}

TEST_F(WebRtcIceConnectTest, TestConnectSymmetricNat) {
  if (turn_server_.empty())
    return;

  UseNat();
  SetFilteringType(TestNat::PORT_DEPENDENT);
  SetMappingType(TestNat::PORT_DEPENDENT);
  AddStream(1);
  p1_->SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                        NrIceCandidate::Type::ICE_RELAYED);
  p2_->SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                        NrIceCandidate::Type::ICE_RELAYED);
  SetTurnServer(turn_server_, kDefaultStunServerPort,
                turn_user_, turn_password_);
  ASSERT_TRUE(Gather());
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectSymmetricNatAndNoNat) {
  p1_ = MakeUnique<IceTestPeer>("P1", test_utils_, true, false, false);
  p1_->UseNat();
  p1_->SetFilteringType(TestNat::PORT_DEPENDENT);
  p1_->SetMappingType(TestNat::PORT_DEPENDENT);

  p2_ = MakeUnique<IceTestPeer>("P2", test_utils_, false, false, false);
  initted_ = true;

  AddStream(1);
  p1_->SetExpectedTypes(NrIceCandidate::Type::ICE_PEER_REFLEXIVE,
                        NrIceCandidate::Type::ICE_HOST);
  p2_->SetExpectedTypes(NrIceCandidate::Type::ICE_HOST,
                        NrIceCandidate::Type::ICE_PEER_REFLEXIVE);
  ASSERT_TRUE(Gather());
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestGatherNatBlocksUDP) {
  if (turn_server_.empty())
    return;

  UseNat();
  BlockUdp();
  AddStream(1);
  std::vector<NrIceTurnServer> turn_servers;
  std::vector<unsigned char> password_vec(turn_password_.begin(),
                                          turn_password_.end());
  turn_servers.push_back(
      *NrIceTurnServer::Create(turn_server_, kDefaultStunServerPort,
                               turn_user_, password_vec, kNrIceTransportTcp));
  turn_servers.push_back(
      *NrIceTurnServer::Create(turn_server_, kDefaultStunServerPort,
                               turn_user_, password_vec, kNrIceTransportUdp));
  SetTurnServers(turn_servers);
  // We have to wait for the UDP-based stuff to time out.
  ASSERT_TRUE(Gather(kDefaultTimeout * 3));
}

TEST_F(WebRtcIceConnectTest, TestConnectNatBlocksUDP) {
  if (turn_server_.empty())
    return;

  UseNat();
  BlockUdp();
  AddStream(1);
  std::vector<NrIceTurnServer> turn_servers;
  std::vector<unsigned char> password_vec(turn_password_.begin(),
                                          turn_password_.end());
  turn_servers.push_back(
      *NrIceTurnServer::Create(turn_server_, kDefaultStunServerPort,
                               turn_user_, password_vec, kNrIceTransportTcp));
  turn_servers.push_back(
      *NrIceTurnServer::Create(turn_server_, kDefaultStunServerPort,
                               turn_user_, password_vec, kNrIceTransportUdp));
  SetTurnServers(turn_servers);
  p1_->SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                        NrIceCandidate::Type::ICE_RELAYED,
                        kNrIceTransportTcp);
  p2_->SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                        NrIceCandidate::Type::ICE_RELAYED,
                        kNrIceTransportTcp);
  ASSERT_TRUE(Gather(kDefaultTimeout * 3));
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectTwoComponents) {
  AddStream(2);
  ASSERT_TRUE(Gather());
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectTwoComponentsDisableSecond) {
  AddStream(2);
  ASSERT_TRUE(Gather());
  p1_->DisableComponent(0, 2);
  p2_->DisableComponent(0, 2);
  Connect();
}


TEST_F(WebRtcIceConnectTest, TestConnectP2ThenP1) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectP2();
  PR_Sleep(1000);
  ConnectP1();
  WaitForConnectedStreams();
}

TEST_F(WebRtcIceConnectTest, TestConnectP2ThenP1Trickle) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectP2();
  PR_Sleep(1000);
  ConnectP1(TRICKLE_SIMULATE);
  SimulateTrickleP1(0);
  WaitForConnectedStreams();
}

TEST_F(WebRtcIceConnectTest, TestConnectP2ThenP1TrickleTwoComponents) {
  AddStream(1);
  AddStream(2);
  ASSERT_TRUE(Gather());
  ConnectP2();
  PR_Sleep(1000);
  ConnectP1(TRICKLE_SIMULATE);
  SimulateTrickleP1(0);
  std::cerr << "Sleeping between trickle streams" << std::endl;
  PR_Sleep(1000);  // Give this some time to settle but not complete
                   // all of ICE.
  SimulateTrickleP1(1);
  WaitForConnectedStreams(2);
}

TEST_F(WebRtcIceConnectTest, TestConnectAutoPrioritize) {
  Init(false, false);
  AddStream(1);
  ASSERT_TRUE(Gather());
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectTrickleOneStreamOneComponent) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  SimulateTrickle(0);
  WaitForConnected(1000);
  AssertCheckingReached();
}

TEST_F(WebRtcIceConnectTest, TestConnectTrickleTwoStreamsOneComponent) {
  AddStream(1);
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  SimulateTrickle(0);
  SimulateTrickle(1);
  WaitForConnected(1000);
  AssertCheckingReached();
}

void RealisticTrickleDelay(
    std::vector<SchedulableTrickleCandidate*>& candidates) {
  for (size_t i = 0; i < candidates.size(); ++i) {
    SchedulableTrickleCandidate* cand = candidates[i];
    if (cand->IsHost()) {
      cand->Schedule(i*10);
    } else if (cand->IsReflexive()) {
      cand->Schedule(i*10 + 100);
    } else if (cand->IsRelay()) {
      cand->Schedule(i*10 + 200);
    }
  }
}

void DelayRelayCandidates(
    std::vector<SchedulableTrickleCandidate*>& candidates,
    unsigned int ms) {
  for (auto i = candidates.begin(); i != candidates.end(); ++i) {
    if ((*i)->IsRelay()) {
      (*i)->Schedule(ms);
    } else {
      (*i)->Schedule(0);
    }
  }
}

void AddNonPairableCandidates(
    std::vector<SchedulableTrickleCandidate*>& candidates,
    IceTestPeer *peer, size_t stream, int net_type,
    MtransportTestUtils* test_utils_) {
  for (int i=1; i<5; i++) {
    if (net_type == i)
      continue;
    switch (i) {
      case 1:
        candidates.push_back(new SchedulableTrickleCandidate(peer, stream,
                   "candidate:0 1 UDP 2113601790 10.0.0.1 12345 typ host",
                   test_utils_));
        break;
      case 2:
        candidates.push_back(new SchedulableTrickleCandidate(peer, stream,
                   "candidate:0 1 UDP 2113601791 172.16.1.1 12345 typ host",
                   test_utils_));
        break;
      case 3:
        candidates.push_back(new SchedulableTrickleCandidate(peer, stream,
                   "candidate:0 1 UDP 2113601792 192.168.0.1 12345 typ host",
                   test_utils_));
        break;
      case 4:
        candidates.push_back(new SchedulableTrickleCandidate(peer, stream,
                   "candidate:0 1 UDP 2113601793 100.64.1.1 12345 typ host",
                   test_utils_));
        break;
      default:
        UNIMPLEMENTED;
    }
  }

  for (auto i = candidates.rbegin(); i != candidates.rend(); ++i) {
    std::cerr << "Scheduling candidate: " << (*i)->Candidate().c_str() << std::endl;
    (*i)->Schedule(0);
  }
}

void DropTrickleCandidates(
    std::vector<SchedulableTrickleCandidate*>& candidates) {
}

TEST_F(WebRtcIceConnectTest, TestConnectTrickleAddStreamDuringICE) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(0));
  RealisticTrickleDelay(p2_->ControlTrickle(0));
  AddStream(1);
  RealisticTrickleDelay(p1_->ControlTrickle(1));
  RealisticTrickleDelay(p2_->ControlTrickle(1));
  WaitForConnected(1000);
  AssertCheckingReached();
}

TEST_F(WebRtcIceConnectTest, TestConnectTrickleAddStreamAfterICE) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(0));
  RealisticTrickleDelay(p2_->ControlTrickle(0));
  WaitForConnected(1000);
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(1));
  RealisticTrickleDelay(p2_->ControlTrickle(1));
  WaitForConnected(1000);
  AssertCheckingReached();
}

TEST_F(WebRtcIceConnectTest, RemoveStream) {
  AddStream(1);
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(0));
  RealisticTrickleDelay(p2_->ControlTrickle(0));
  RealisticTrickleDelay(p1_->ControlTrickle(1));
  RealisticTrickleDelay(p2_->ControlTrickle(1));
  WaitForConnected(1000);

  RemoveStream(0);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
}

TEST_F(WebRtcIceConnectTest, P1NoTrickle) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  DropTrickleCandidates(p1_->ControlTrickle(0));
  RealisticTrickleDelay(p2_->ControlTrickle(0));
  WaitForConnected(1000);
}

TEST_F(WebRtcIceConnectTest, P2NoTrickle) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(0));
  DropTrickleCandidates(p2_->ControlTrickle(0));
  WaitForConnected(1000);
}

TEST_F(WebRtcIceConnectTest, RemoveAndAddStream) {
  AddStream(1);
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(0));
  RealisticTrickleDelay(p2_->ControlTrickle(0));
  RealisticTrickleDelay(p1_->ControlTrickle(1));
  RealisticTrickleDelay(p2_->ControlTrickle(1));
  WaitForConnected(1000);

  RemoveStream(0);
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(2));
  RealisticTrickleDelay(p2_->ControlTrickle(2));
  WaitForConnected(1000);
}

TEST_F(WebRtcIceConnectTest, RemoveStreamBeforeGather) {
  AddStream(1);
  AddStream(1);
  ASSERT_TRUE(Gather(0));
  RemoveStream(0);
  WaitForGather();
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(1));
  RealisticTrickleDelay(p2_->ControlTrickle(1));
  WaitForConnected(1000);
}

TEST_F(WebRtcIceConnectTest, RemoveStreamDuringGather) {
  AddStream(1);
  AddStream(1);
  RemoveStream(0);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(1));
  RealisticTrickleDelay(p2_->ControlTrickle(1));
  WaitForConnected(1000);
}

TEST_F(WebRtcIceConnectTest, RemoveStreamDuringConnect) {
  AddStream(1);
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(0));
  RealisticTrickleDelay(p2_->ControlTrickle(0));
  RealisticTrickleDelay(p1_->ControlTrickle(1));
  RealisticTrickleDelay(p2_->ControlTrickle(1));
  RemoveStream(0);
  WaitForConnected(1000);
}

TEST_F(WebRtcIceConnectTest, TestConnectRealTrickleOneStreamOneComponent) {
  AddStream(1);
  AddStream(1);
  ASSERT_TRUE(Gather(0));
  ConnectTrickle(TRICKLE_REAL);
  WaitForConnected();
  WaitForGather();  // ICE can complete before we finish gathering.
  AssertCheckingReached();
}

TEST_F(WebRtcIceConnectTest, TestSendReceive) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  Connect();
  SendReceive();
}

TEST_F(WebRtcIceConnectTest, TestSendReceiveTcp) {
  Init(false, true);
  AddStream(1);
  ASSERT_TRUE(Gather());
  SetCandidateFilter(IsTcpCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_HOST,
    NrIceCandidate::Type::ICE_HOST, kNrIceTransportTcp);
  Connect();
  SendReceive();
}

//TCP SO tests works on localhost only with delay applied:
//  tc qdisc add dev lo root netem delay 10ms
TEST_F(WebRtcIceConnectTest, DISABLED_TestSendReceiveTcpSo) {
  Init(false, true);
  AddStream(1);
  ASSERT_TRUE(Gather());
  SetCandidateFilter(IsTcpSoCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_HOST,
    NrIceCandidate::Type::ICE_HOST, kNrIceTransportTcp);
  Connect();
  SendReceive();
}

TEST_F(WebRtcIceConnectTest, TestConsent) {
  AddStream(1);
  SetupAndCheckConsent();
  PR_Sleep(1500);
  AssertConsentRefresh();
  SendReceive();
}

TEST_F(WebRtcIceConnectTest, TestConsentTcp) {
  Init(false, true);
  AddStream(1);
  SetCandidateFilter(IsTcpCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_HOST,
    NrIceCandidate::Type::ICE_HOST, kNrIceTransportTcp);
  SetupAndCheckConsent();
  PR_Sleep(1500);
  AssertConsentRefresh();
  SendReceive();
}

TEST_F(WebRtcIceConnectTest, TestConsentIntermittent) {
  AddStream(1);
  SetupAndCheckConsent();
  p1_->SetBlockStun(true);
  p2_->SetBlockStun(true);
  WaitForDisconnected();
  AssertConsentRefresh(CONSENT_STALE);
  SendReceive();
  p1_->SetBlockStun(false);
  p2_->SetBlockStun(false);
  WaitForConnected();
  AssertConsentRefresh();
  SendReceive();
  p1_->SetBlockStun(true);
  p2_->SetBlockStun(true);
  WaitForDisconnected();
  AssertConsentRefresh(CONSENT_STALE);
  SendReceive();
  p1_->SetBlockStun(false);
  p2_->SetBlockStun(false);
  WaitForConnected();
  AssertConsentRefresh();
}

TEST_F(WebRtcIceConnectTest, TestConsentTimeout) {
  AddStream(1);
  SetupAndCheckConsent();
  p1_->SetBlockStun(true);
  p2_->SetBlockStun(true);
  WaitForDisconnected();
  AssertConsentRefresh(CONSENT_STALE);
  SendReceive();
  WaitForFailed();
  AssertConsentRefresh(CONSENT_EXPIRED);
  SendFailure();
}

TEST_F(WebRtcIceConnectTest, TestConsentDelayed) {
  AddStream(1);
  SetupAndCheckConsent();
  /* Note: We don't have a list of STUN transaction IDs of the previously timed
           out consent requests. Thus responses after sending the next consent
           request are ignored. */
  p1_->SetStunResponseDelay(300);
  p2_->SetStunResponseDelay(300);
  PR_Sleep(1000);
  AssertConsentRefresh();
  SendReceive();
}

TEST_F(WebRtcIceConnectTest, TestNetworkForcedOfflineAndRecovery) {
  AddStream(1);
  SetupAndCheckConsent();
  p1_->ChangeNetworkStateToOffline();
  ASSERT_TRUE_WAIT(p1_->ice_connected() == 0, kDefaultTimeout);
  // Next round of consent check should switch it back to online
  ASSERT_TRUE_WAIT(p1_->ice_connected(), kDefaultTimeout);
}

TEST_F(WebRtcIceConnectTest, TestNetworkForcedOfflineTwice) {
  AddStream(1);
  SetupAndCheckConsent();
  p2_->ChangeNetworkStateToOffline();
  ASSERT_TRUE_WAIT(p2_->ice_connected() == 0, kDefaultTimeout);
  p2_->ChangeNetworkStateToOffline();
  ASSERT_TRUE_WAIT(p2_->ice_connected() == 0, kDefaultTimeout);
}

TEST_F(WebRtcIceConnectTest, TestNetworkOnlineDoesntChangeState) {
  AddStream(1);
  SetupAndCheckConsent();
  p2_->ChangeNetworkStateToOnline();
  ASSERT_TRUE(p2_->ice_connected());
  PR_Sleep(1500);
  p2_->ChangeNetworkStateToOnline();
  ASSERT_TRUE(p2_->ice_connected());
}

TEST_F(WebRtcIceConnectTest, TestNetworkOnlineTriggersConsent) {
  // Let's emulate audio + video w/o rtcp-mux
  AddStream(2);
  AddStream(2);
  SetupAndCheckConsent();
  p1_->ChangeNetworkStateToOffline();
  p1_->SetBlockStun(true);
  ASSERT_TRUE_WAIT(p1_->ice_connected() == 0, kDefaultTimeout);
  PR_Sleep(1500);
  ASSERT_TRUE(p1_->ice_connected() == 0);
  p1_->SetBlockStun(false);
  p1_->ChangeNetworkStateToOnline();
  ASSERT_TRUE_WAIT(p1_->ice_connected(), 500);
}

TEST_F(WebRtcIceConnectTest, TestConnectTurn) {
  if (turn_server_.empty())
    return;

  AddStream(1);
  SetTurnServer(turn_server_, kDefaultStunServerPort,
                turn_user_, turn_password_);
  ASSERT_TRUE(Gather());
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectTurnWithDelay) {
  if (turn_server_.empty())
    return;

  AddStream(1);
  SetTurnServer(turn_server_, kDefaultStunServerPort,
                turn_user_, turn_password_);
  SetCandidateFilter(SabotageHostCandidateAndDropReflexive);
  p1_->Gather();
  PR_Sleep(500);
  p2_->Gather();
  ConnectTrickle(TRICKLE_REAL);
  WaitForGather();
  WaitForConnectedStreams();
}

TEST_F(WebRtcIceConnectTest, TestConnectTurnWithNormalTrickleDelay) {
  if (turn_server_.empty())
    return;

  AddStream(1);
  SetTurnServer(turn_server_, kDefaultStunServerPort,
                turn_user_, turn_password_);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(0));
  RealisticTrickleDelay(p2_->ControlTrickle(0));

  WaitForConnected();
  AssertCheckingReached();
}

TEST_F(WebRtcIceConnectTest, TestConnectTurnWithNormalTrickleDelayOneSided) {
  if (turn_server_.empty())
    return;

  AddStream(1);
  SetTurnServer(turn_server_, kDefaultStunServerPort,
                turn_user_, turn_password_);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  RealisticTrickleDelay(p1_->ControlTrickle(0));
  p2_->SimulateTrickle(0);

  WaitForConnected();
  AssertCheckingReached();
}

TEST_F(WebRtcIceConnectTest, TestConnectTurnWithLargeTrickleDelay) {
  if (turn_server_.empty())
    return;

  AddStream(1);
  SetTurnServer(turn_server_, kDefaultStunServerPort,
                turn_user_, turn_password_);
  SetCandidateFilter(SabotageHostCandidateAndDropReflexive);
  ASSERT_TRUE(Gather());
  ConnectTrickle();
  // Trickle host candidates immediately, but delay relay candidates
  DelayRelayCandidates(p1_->ControlTrickle(0), 3700);
  DelayRelayCandidates(p2_->ControlTrickle(0), 3700);

  WaitForConnected();
  AssertCheckingReached();
}

TEST_F(WebRtcIceConnectTest, TestConnectTurnTcp) {
  if (turn_server_.empty())
    return;

  AddStream(1);
  SetTurnServer(turn_server_, kDefaultStunServerPort,
                turn_user_, turn_password_, kNrIceTransportTcp);
  ASSERT_TRUE(Gather());
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectTurnOnly) {
  if (turn_server_.empty())
    return;

  AddStream(1);
  SetTurnServer(turn_server_, kDefaultStunServerPort,
                turn_user_, turn_password_);
  ASSERT_TRUE(Gather());
  SetCandidateFilter(IsRelayCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                   NrIceCandidate::Type::ICE_RELAYED);
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestConnectTurnTcpOnly) {
  if (turn_server_.empty())
    return;

  AddStream(1);
  SetTurnServer(turn_server_, kDefaultStunServerPort,
                turn_user_, turn_password_, kNrIceTransportTcp);
  ASSERT_TRUE(Gather());
  SetCandidateFilter(IsRelayCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                   NrIceCandidate::Type::ICE_RELAYED,
                   kNrIceTransportTcp);
  Connect();
}

TEST_F(WebRtcIceConnectTest, TestSendReceiveTurnOnly) {
  if (turn_server_.empty())
    return;

  AddStream(1);
  SetTurnServer(turn_server_, kDefaultStunServerPort,
                turn_user_, turn_password_);
  ASSERT_TRUE(Gather());
  SetCandidateFilter(IsRelayCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                   NrIceCandidate::Type::ICE_RELAYED);
  Connect();
  SendReceive();
}

TEST_F(WebRtcIceConnectTest, TestSendReceiveTurnTcpOnly) {
  if (turn_server_.empty())
    return;

  AddStream(1);
  SetTurnServer(turn_server_, kDefaultStunServerPort,
                turn_user_, turn_password_, kNrIceTransportTcp);
  ASSERT_TRUE(Gather());
  SetCandidateFilter(IsRelayCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                   NrIceCandidate::Type::ICE_RELAYED,
                   kNrIceTransportTcp);
  Connect();
  SendReceive();
}

TEST_F(WebRtcIceConnectTest, TestSendReceiveTurnBothOnly) {
  if (turn_server_.empty())
    return;

  AddStream(1);
  std::vector<NrIceTurnServer> turn_servers;
  std::vector<unsigned char> password_vec(turn_password_.begin(),
                                          turn_password_.end());
  turn_servers.push_back(*NrIceTurnServer::Create(
                           turn_server_, kDefaultStunServerPort,
                           turn_user_, password_vec, kNrIceTransportTcp));
  turn_servers.push_back(*NrIceTurnServer::Create(
                           turn_server_, kDefaultStunServerPort,
                           turn_user_, password_vec, kNrIceTransportUdp));
  SetTurnServers(turn_servers);
  ASSERT_TRUE(Gather());
  SetCandidateFilter(IsRelayCandidate);
  // UDP is preferred.
  SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                   NrIceCandidate::Type::ICE_RELAYED,
                   kNrIceTransportUdp);
  Connect();
  SendReceive();
}

TEST_F(WebRtcIceConnectTest, TestConnectShutdownOneSide) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  ConnectThenDelete();
}

TEST_F(WebRtcIceConnectTest, TestPollCandPairsBeforeConnect) {
  AddStream(1);
  ASSERT_TRUE(Gather());

  std::vector<NrIceCandidatePair> pairs;
  nsresult res = p1_->GetCandidatePairs(0, &pairs);
  // There should be no candidate pairs prior to calling Connect()
  ASSERT_EQ(NS_OK, res);
  ASSERT_EQ(0U, pairs.size());

  res = p2_->GetCandidatePairs(0, &pairs);
  ASSERT_EQ(NS_OK, res);
  ASSERT_EQ(0U, pairs.size());
}

TEST_F(WebRtcIceConnectTest, TestPollCandPairsAfterConnect) {
  AddStream(1);
  ASSERT_TRUE(Gather());
  Connect();

  std::vector<NrIceCandidatePair> pairs;
  nsresult r = p1_->GetCandidatePairs(0, &pairs);
  ASSERT_EQ(NS_OK, r);
  // How detailed of a check do we want to do here? If the turn server is
  // functioning, we'll get at least two pairs, but this is probably not
  // something we should assume.
  ASSERT_NE(0U, pairs.size());
  ASSERT_TRUE(p1_->CandidatePairsPriorityDescending(pairs));
  ASSERT_TRUE(ContainsSucceededPair(pairs));
  pairs.clear();

  r = p2_->GetCandidatePairs(0, &pairs);
  ASSERT_EQ(NS_OK, r);
  ASSERT_NE(0U, pairs.size());
  ASSERT_TRUE(p2_->CandidatePairsPriorityDescending(pairs));
  ASSERT_TRUE(ContainsSucceededPair(pairs));
}

// TODO Bug 1259842 - disabled until we find a better way to handle two
// candidates from different RFC1918 ranges
TEST_F(WebRtcIceConnectTest, DISABLED_TestHostCandPairingFilter) {
  Init(false, false, false);
  AddStream(1);
  ASSERT_TRUE(Gather());
  SetCandidateFilter(IsIpv4Candidate);

  int host_net = p1_->GetCandidatesPrivateIpv4Range(0);
  if (host_net <= 0) {
    // TODO bug 1226838: make this work with multiple private IPs
    FAIL() << "This test needs exactly one private IPv4 host candidate to work" << std::endl;
  }

  ConnectTrickle();
  AddNonPairableCandidates(p1_->ControlTrickle(0), p1_.get(), 0, host_net, test_utils_);
  AddNonPairableCandidates(p2_->ControlTrickle(0), p2_.get(), 0, host_net, test_utils_);

  std::vector<NrIceCandidatePair> pairs;
  p1_->GetCandidatePairs(0, &pairs);
  for (auto p : pairs) {
    std::cerr << "Verifying pair:" << std::endl;
    p1_->DumpCandidatePair(p);
    nr_transport_addr addr;
    nr_str_port_to_transport_addr(p.local.local_addr.host.c_str(), 0, IPPROTO_UDP, &addr);
    ASSERT_TRUE(nr_transport_addr_get_private_addr_range(&addr) == host_net);
    nr_str_port_to_transport_addr(p.remote.cand_addr.host.c_str(), 0, IPPROTO_UDP, &addr);
    ASSERT_TRUE(nr_transport_addr_get_private_addr_range(&addr) == host_net);
  }
}

// TODO Bug 1226838 - See Comment 2 - this test can't work as written
TEST_F(WebRtcIceConnectTest, DISABLED_TestSrflxCandPairingFilter) {
  if (stun_server_address_.empty()) {
    return;
  }

  Init(false, false, false);
  AddStream(1);
  ASSERT_TRUE(Gather());
  SetCandidateFilter(IsSrflxCandidate);

  if (p1_->GetCandidatesPrivateIpv4Range(0) <= 0) {
    // TODO bug 1226838: make this work with public IP addresses
    std::cerr << "Don't run this test at IETF meetings!" << std::endl;
    FAIL() << "This test needs one private IPv4 host candidate to work" << std::endl;
  }

  ConnectTrickle();
  SimulateTrickleP1(0);
  SimulateTrickleP2(0);

  std::vector<NrIceCandidatePair> pairs;
  p1_->GetCandidatePairs(0, &pairs);
  for (auto p : pairs) {
    std::cerr << "Verifying P1 pair:" << std::endl;
    p1_->DumpCandidatePair(p);
    nr_transport_addr addr;
    nr_str_port_to_transport_addr(p.local.local_addr.host.c_str(), 0, IPPROTO_UDP, &addr);
    ASSERT_TRUE(nr_transport_addr_get_private_addr_range(&addr) != 0);
    nr_str_port_to_transport_addr(p.remote.cand_addr.host.c_str(), 0, IPPROTO_UDP, &addr);
    ASSERT_TRUE(nr_transport_addr_get_private_addr_range(&addr) == 0);
  }
  p2_->GetCandidatePairs(0, &pairs);
  for (auto p : pairs) {
    std::cerr << "Verifying P2 pair:" << std::endl;
    p2_->DumpCandidatePair(p);
    nr_transport_addr addr;
    nr_str_port_to_transport_addr(p.local.local_addr.host.c_str(), 0, IPPROTO_UDP, &addr);
    ASSERT_TRUE(nr_transport_addr_get_private_addr_range(&addr) != 0);
    nr_str_port_to_transport_addr(p.remote.cand_addr.host.c_str(), 0, IPPROTO_UDP, &addr);
    ASSERT_TRUE(nr_transport_addr_get_private_addr_range(&addr) == 0);
  }
}

TEST_F(WebRtcIceConnectTest, TestPollCandPairsDuringConnect) {
  AddStream(1);
  ASSERT_TRUE(Gather());

  p2_->Connect(p1_.get(), TRICKLE_NONE, false);
  p1_->Connect(p2_.get(), TRICKLE_NONE, false);

  std::vector<NrIceCandidatePair> pairs1;
  std::vector<NrIceCandidatePair> pairs2;

  p1_->StartChecks();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);

  p2_->StartChecks();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);

  WaitForConnectedStreams();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);
  ASSERT_TRUE(ContainsSucceededPair(pairs1));
  ASSERT_TRUE(ContainsSucceededPair(pairs2));
}

TEST_F(WebRtcIceConnectTest, TestRLogConnector) {
  AddStream(1);
  ASSERT_TRUE(Gather());

  p2_->Connect(p1_.get(), TRICKLE_NONE, false);
  p1_->Connect(p2_.get(), TRICKLE_NONE, false);

  std::vector<NrIceCandidatePair> pairs1;
  std::vector<NrIceCandidatePair> pairs2;

  p1_->StartChecks();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);

  p2_->StartChecks();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);

  WaitForConnectedStreams();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);
  ASSERT_TRUE(ContainsSucceededPair(pairs1));
  ASSERT_TRUE(ContainsSucceededPair(pairs2));

  for (auto p = pairs1.begin(); p != pairs1.end(); ++p) {
    std::deque<std::string> logs;
    std::string substring("CAND-PAIR(");
    substring += p->codeword;
    RLogConnector::GetInstance()->Filter(substring, 0, &logs);
    ASSERT_NE(0U, logs.size());
  }

  for (auto p = pairs2.begin(); p != pairs2.end(); ++p) {
    std::deque<std::string> logs;
    std::string substring("CAND-PAIR(");
    substring += p->codeword;
    RLogConnector::GetInstance()->Filter(substring, 0, &logs);
    ASSERT_NE(0U, logs.size());
  }
}

TEST_F(WebRtcIcePrioritizerTest, TestPrioritizer) {
  SetPriorizer(::mozilla::CreateInterfacePrioritizer());

  AddInterface("0", NR_INTERFACE_TYPE_VPN, 100); // unknown vpn
  AddInterface("1", NR_INTERFACE_TYPE_VPN | NR_INTERFACE_TYPE_WIRED, 100); // wired vpn
  AddInterface("2", NR_INTERFACE_TYPE_VPN | NR_INTERFACE_TYPE_WIFI, 100); // wifi vpn
  AddInterface("3", NR_INTERFACE_TYPE_VPN | NR_INTERFACE_TYPE_MOBILE, 100); // wifi vpn
  AddInterface("4", NR_INTERFACE_TYPE_WIRED, 1000); // wired, high speed
  AddInterface("5", NR_INTERFACE_TYPE_WIRED, 10); // wired, low speed
  AddInterface("6", NR_INTERFACE_TYPE_WIFI, 10); // wifi, low speed
  AddInterface("7", NR_INTERFACE_TYPE_WIFI, 1000); // wifi, high speed
  AddInterface("8", NR_INTERFACE_TYPE_MOBILE, 10); // mobile, low speed
  AddInterface("9", NR_INTERFACE_TYPE_MOBILE, 1000); // mobile, high speed
  AddInterface("10", NR_INTERFACE_TYPE_UNKNOWN, 10); // unknown, low speed
  AddInterface("11", NR_INTERFACE_TYPE_UNKNOWN, 1000); // unknown, high speed

  // expected preference "4" > "5" > "1" > "7" > "6" > "2" > "9" > "8" > "3" > "11" > "10" > "0"

  HasLowerPreference("0", "10");
  HasLowerPreference("10", "11");
  HasLowerPreference("11", "3");
  HasLowerPreference("3", "8");
  HasLowerPreference("8", "9");
  HasLowerPreference("9", "2");
  HasLowerPreference("2", "6");
  HasLowerPreference("6", "7");
  HasLowerPreference("7", "1");
  HasLowerPreference("1", "5");
  HasLowerPreference("5", "4");
}

TEST_F(WebRtcIcePacketFilterTest, TestSendNonStunPacket) {
  const unsigned char data[] = "12345abcde";
  TestOutgoing(data, sizeof(data), 123, 45, false);
  TestOutgoingTcp(data, sizeof(data), false);
}

TEST_F(WebRtcIcePacketFilterTest, TestRecvNonStunPacket) {
  const unsigned char data[] = "12345abcde";
  TestIncoming(data, sizeof(data), 123, 45, false);
  TestIncomingTcp(data, sizeof(data), true);
}

TEST_F(WebRtcIcePacketFilterTest, TestSendStunPacket) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));
  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);
  TestOutgoingTcp(msg->buffer, msg->length, true);
  TestOutgoingTcpFramed(msg->buffer, msg->length, true);
  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST_F(WebRtcIcePacketFilterTest, TestRecvStunPacketWithoutAPendingId) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));

  msg->header.id.octet[0] = 1;
  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);
  TestOutgoingTcp(msg->buffer, msg->length, true);

  msg->header.id.octet[0] = 0;
  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestIncoming(msg->buffer, msg->length, 123, 45, true);
  TestIncomingTcp(msg->buffer, msg->length, true);

  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST_F(WebRtcIcePacketFilterTest, TestRecvStunPacketWithoutAPendingIdTcpFramed) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));

  msg->header.id.octet[0] = 1;
  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoingTcpFramed(msg->buffer, msg->length, true);

  msg->header.id.octet[0] = 0;
  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestIncomingTcpFramed(msg->buffer, msg->length, true);

  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST_F(WebRtcIcePacketFilterTest, TestRecvStunPacketWithoutAPendingAddress) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));

  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);
  // nothing to test here for the TCP filter

  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestIncoming(msg->buffer, msg->length, 123, 46, false);
  TestIncoming(msg->buffer, msg->length, 124, 45, false);

  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST_F(WebRtcIcePacketFilterTest, TestRecvStunPacketWithPendingIdAndAddress) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));

  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);
  TestOutgoingTcp(msg->buffer, msg->length, true);

  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestIncoming(msg->buffer, msg->length, 123, 45, true);
  TestIncomingTcp(msg->buffer, msg->length, true);

  // Test whitelist by filtering non-stun packets.
  const unsigned char data[] = "12345abcde";

  // 123:45 is white-listed.
  TestOutgoing(data, sizeof(data), 123, 45, true);
  TestOutgoingTcp(data, sizeof(data), true);
  TestIncoming(data, sizeof(data), 123, 45, true);
  TestIncomingTcp(data, sizeof(data), true);

  // Indications pass as well.
  msg->header.type = NR_STUN_MSG_BINDING_INDICATION;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);
  TestOutgoingTcp(msg->buffer, msg->length, true);
  TestIncoming(msg->buffer, msg->length, 123, 45, true);
  TestIncomingTcp(msg->buffer, msg->length, true);

  // Packets from and to other address are still disallowed.
  // Note: this doesn't apply for TCP connections
  TestOutgoing(data, sizeof(data), 123, 46, false);
  TestIncoming(data, sizeof(data), 123, 46, false);
  TestOutgoing(data, sizeof(data), 124, 45, false);
  TestIncoming(data, sizeof(data), 124, 45, false);

  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST_F(WebRtcIcePacketFilterTest, TestRecvStunPacketWithPendingIdTcpFramed) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));

  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoingTcpFramed(msg->buffer, msg->length, true);

  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestIncomingTcpFramed(msg->buffer, msg->length, true);

  // Test whitelist by filtering non-stun packets.
  const unsigned char data[] = "12345abcde";

  TestOutgoingTcpFramed(data, sizeof(data), true);
  TestIncomingTcpFramed(data, sizeof(data), true);

  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST_F(WebRtcIcePacketFilterTest, TestSendNonRequestStunPacket) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));

  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, false);
  TestOutgoingTcp(msg->buffer, msg->length, false);

  // Send a packet so we allow the incoming request.
  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);
  TestOutgoingTcp(msg->buffer, msg->length, true);

  // This packet makes us able to send a response.
  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestIncoming(msg->buffer, msg->length, 123, 45, true);
  TestIncomingTcp(msg->buffer, msg->length, true);

  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);
  TestOutgoingTcp(msg->buffer, msg->length, true);

  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST_F(WebRtcIcePacketFilterTest, TestRecvDataPacketWithAPendingAddress) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));

  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);
  TestOutgoingTcp(msg->buffer, msg->length, true);

  const unsigned char data[] = "12345abcde";
  TestIncoming(data, sizeof(data), 123, 45, true);
  TestIncomingTcp(data, sizeof(data), true);

  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST(WebRtcIceInternalsTest, TestAddBogusAttribute) {
  nr_stun_message *req;
  ASSERT_EQ(0, nr_stun_message_create(&req));
  Data *data;
  ASSERT_EQ(0, r_data_alloc(&data, 3000));
  memset(data->data, 'A', data->len);
  ASSERT_TRUE(nr_stun_message_add_message_integrity_attribute(req, data));
  ASSERT_EQ(0, r_data_destroy(&data));
  ASSERT_EQ(0, nr_stun_message_destroy(&req));
}
