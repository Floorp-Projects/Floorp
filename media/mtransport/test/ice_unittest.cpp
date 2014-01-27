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

#include "mozilla/Scoped.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"

#include "nricectx.h"
#include "nricemediastream.h"
#include "nriceresolverfake.h"
#include "nriceresolver.h"
#include "nrinterfaceprioritizer.h"
#include "mtransport_test_utils.h"
#include "gtest_ringbuffer_dumper.h"
#include "rlogringbuffer.h"
#include "runnable_utils.h"
#include "stunserver.h"
// TODO(bcampen@mozilla.com): Big fat hack since the build system doesn't give
// us a clean way to add object files to a single executable.
#include "stunserver.cpp"
#include "stun_udp_socket_filter.h"
#include "mozilla/net/DNS.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;
MtransportTestUtils *test_utils;

bool stream_added = false;

const std::string kDefaultStunServerAddress((char *)"23.21.150.121");
const std::string kDefaultStunServerHostname(
    (char *)"ec2-23-21-150-121.compute-1.amazonaws.com");
const std::string kBogusStunServerHostname(
    (char *)"stun-server-nonexistent.invalid");
const uint16_t kDefaultStunServerPort=3478;
const std::string kBogusIceCandidate(
    (char *)"candidate:0 2 UDP 2113601790 192.168.178.20 50769 typ");

std::string g_stun_server_address(kDefaultStunServerAddress);
std::string g_stun_server_hostname(kDefaultStunServerHostname);
std::string g_turn_server;
std::string g_turn_user;
std::string g_turn_password;

namespace {

enum TrickleMode { TRICKLE_NONE, TRICKLE_SIMULATE, TRICKLE_REAL };

typedef bool (*CandidateFilter)(const std::string& candidate);

static bool IsRelayCandidate(const std::string& candidate) {
  return candidate.find("typ relay") != std::string::npos;
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

class IceTestPeer : public sigslot::has_slots<> {
 public:

  IceTestPeer(const std::string& name, bool offerer, bool set_priorities) :
      name_(name),
      ice_ctx_(NrIceCtx::Create(name, offerer, set_priorities)),
      streams_(),
      candidates_(),
      gathering_complete_(false),
      ready_ct_(0),
      ice_complete_(false),
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
      trickled_(0) {
    ice_ctx_->SignalGatheringStateChange.connect(
        this,
        &IceTestPeer::GatheringStateChange);
    ice_ctx_->SignalConnectionStateChange.connect(
        this,
        &IceTestPeer::ConnectionStateChange);
  }

  ~IceTestPeer() {
    test_utils->sts_target()->Dispatch(WrapRunnable(this,
                                                    &IceTestPeer::Shutdown),
        NS_DISPATCH_SYNC);

    // Give the ICE destruction callback time to fire before
    // we destroy the resolver.
    PR_Sleep(1000);
  }

  void AddStream(int components) {
    char name[100];
    snprintf(name, sizeof(name), "%s:stream%d", name_.c_str(),
             (int)streams_.size());

    mozilla::RefPtr<NrIceMediaStream> stream =
        ice_ctx_->CreateStream(static_cast<char *>(name), components);

    ASSERT_TRUE(stream);
    streams_.push_back(stream);
    stream->SignalCandidate.connect(this, &IceTestPeer::CandidateInitialized);
    stream->SignalReady.connect(this, &IceTestPeer::StreamReady);
    stream->SignalFailed.connect(this, &IceTestPeer::StreamFailed);
    stream->SignalPacketReceived.connect(this, &IceTestPeer::PacketReceived);
  }

  void SetStunServer(const std::string addr, uint16_t port) {
    std::vector<NrIceStunServer> stun_servers;
    ScopedDeletePtr<NrIceStunServer> server(NrIceStunServer::Create(addr,
                                                                    port));
    stun_servers.push_back(*server);
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->SetStunServers(stun_servers)));
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
    ScopedDeletePtr<NrIceTurnServer> server(NrIceTurnServer::Create(
        addr, port, username, password, transport));
    turn_servers.push_back(*server);
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->SetTurnServers(turn_servers)));
  }

  void SetTurnServers(const std::vector<NrIceTurnServer> servers) {
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->SetTurnServers(servers)));
  }

  void SetFakeResolver() {
    ASSERT_TRUE(NS_SUCCEEDED(dns_resolver_->Init()));
    PRNetAddr addr;
    PRStatus status = PR_StringToNetAddr(g_stun_server_address.c_str(),
                                          &addr);
    addr.inet.port = kDefaultStunServerPort;
    ASSERT_EQ(PR_SUCCESS, status);
    fake_resolver_.SetAddr(g_stun_server_hostname, addr);
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->SetResolver(
        fake_resolver_.AllocateResolver())));
  }

  void SetDNSResolver() {
    ASSERT_TRUE(NS_SUCCEEDED(dns_resolver_->Init()));
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->SetResolver(
        dns_resolver_->AllocateResolver())));
  }

  void Gather() {
    nsresult res;

    test_utils->sts_target()->Dispatch(
        WrapRunnableRet(ice_ctx_, &NrIceCtx::StartGathering, &res),
        NS_DISPATCH_SYNC);

    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  // Get various pieces of state
  std::vector<std::string> GetGlobalAttributes() {
    return ice_ctx_->GetGlobalAttributes();
  }

   std::vector<std::string> GetCandidates(size_t stream) {
    std::vector<std::string> v;

    RUN_ON_THREAD(
        test_utils->sts_target(),
        WrapRunnableRet(this, &IceTestPeer::GetCandidates_s, stream, &v),
        NS_DISPATCH_SYNC);

    return v;
  }

  std::vector<std::string> GetCandidates_s(size_t stream) {
    std::vector<std::string> candidates;

    if (stream >= streams_.size())
      return candidates;

    std::vector<std::string> candidates_in =
      streams_[stream]->GetCandidates();


    for (size_t i=0; i < candidates_in.size(); i++) {
      if ((!candidate_filter_) || candidate_filter_(candidates_in[i])) {
        std::cerr << "Returning candidate: " << candidates_in[i] << std::endl;
        candidates.push_back(candidates_in[i]);
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

  bool gathering_complete() { return gathering_complete_; }
  int ready_ct() { return ready_ct_; }
  bool is_ready(size_t stream) {
    return streams_[stream]->state() == NrIceMediaStream::ICE_OPEN;
  }
  bool ice_complete() { return ice_complete_; }
  size_t received() { return received_; }
  size_t sent() { return sent_; }

  // Start connecting to another peer
  void Connect_s(IceTestPeer *remote, TrickleMode trickle_mode,
                 bool start = true) {
    nsresult res;

    remote_ = remote;

    trickle_mode_ = trickle_mode;
    res = ice_ctx_->ParseGlobalAttributes(remote->GetGlobalAttributes());
    ASSERT_TRUE(NS_SUCCEEDED(res));

    if (trickle_mode == TRICKLE_NONE ||
        trickle_mode == TRICKLE_REAL) {
      for (size_t i=0; i<streams_.size(); ++i) {
        std::vector<std::string> candidates =
            remote->GetCandidates(i);

        for (size_t j=0; j<candidates.size(); ++j) {
          std::cerr << "Candidate: " + candidates[j] << std::endl;
        }
        res = streams_[i]->ParseAttributes(candidates);
        ASSERT_TRUE(NS_SUCCEEDED(res));
      }
    } else {
      // Parse empty attributes and then trickle them out later
      for (size_t i=0; i<streams_.size(); ++i) {
        std::vector<std::string> empty_attrs;
        res = streams_[i]->ParseAttributes(empty_attrs);
        ASSERT_TRUE(NS_SUCCEEDED(res));
      }
    }

    if (start) {
      // Now start checks
      res = ice_ctx_->StartChecks();
      ASSERT_TRUE(NS_SUCCEEDED(res));
    }
  }

  void Connect(IceTestPeer *remote, TrickleMode trickle_mode,
               bool start = true) {
    test_utils->sts_target()->Dispatch(
        WrapRunnable(
            this, &IceTestPeer::Connect_s, remote, trickle_mode, start),
        NS_DISPATCH_SYNC);
  }

  void SimulateTrickle(size_t stream) {
    std::cerr << "Doing trickle for stream " << stream << std::endl;
    // If we are in trickle deferred mode, now trickle in the candidates
    // for |stream}
    nsresult res;

    ASSERT_GT(remote_->streams_.size(), stream);

    std::vector<std::string> candidates =
      remote_->GetCandidates(stream);

    for (size_t j=0; j<candidates.size(); j++) {
      test_utils->sts_target()->Dispatch(
        WrapRunnableRet(streams_[stream],
                        &NrIceMediaStream::ParseTrickleCandidate,
                        candidates[j],
                        &res), NS_DISPATCH_SYNC);

      ASSERT_TRUE(NS_SUCCEEDED(res));
    }
  }

  void DumpCandidate(std::string which, const NrIceCandidate& cand) {
    std::string type;

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

    std::cerr << which
              << " --> "
              << type
              << " "
              << cand.local_addr.host
              << ":"
              << cand.local_addr.port
              << " codeword="
              << cand.codeword
              << std::endl;
  }

  void DumpAndCheckActiveCandidates_s() {
    std::cerr << "Active candidates:" << std::endl;
    for (size_t i=0; i < streams_.size(); ++i) {
      for (int j=0; j < streams_[i]->components(); ++j) {
        std::cerr << "Stream " << i << " component " << j+1 << std::endl;

        NrIceCandidate *local;
        NrIceCandidate *remote;

        nsresult res = streams_[i]->GetActivePair(j+1, &local, &remote);
        if (res == NS_ERROR_NOT_AVAILABLE) {
          std::cerr << "Component unpaired or disabled." << std::endl;
        } else {
          ASSERT_TRUE(NS_SUCCEEDED(res));
          DumpCandidate("Local  ", *local);
          ASSERT_EQ(expected_local_type_, local->type);
          ASSERT_EQ(expected_local_transport_, local->local_addr.transport);
          DumpCandidate("Remote ", *remote);
          ASSERT_EQ(expected_remote_type_, remote->type);
          delete local;
          delete remote;
        }
      }
    }
  }

  void DumpAndCheckActiveCandidates() {
    test_utils->sts_target()->Dispatch(
      WrapRunnable(this, &IceTestPeer::DumpAndCheckActiveCandidates_s),
      NS_DISPATCH_SYNC);
  }

  void Close() {
    test_utils->sts_target()->Dispatch(
      WrapRunnable(ice_ctx_, &NrIceCtx::destroy_peer_ctx),
      NS_DISPATCH_SYNC);
  }

  void Shutdown() {
    ice_ctx_ = nullptr;
  }

  void StartChecks() {
    nsresult res;

    // Now start checks
    test_utils->sts_target()->Dispatch(
        WrapRunnableRet(ice_ctx_, &NrIceCtx::StartChecks, &res),
        NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  // Handle events
  void GatheringStateChange(NrIceCtx* ctx,
                            NrIceCtx::GatheringState state) {
    (void)ctx;
    if (state != NrIceCtx::ICE_CTX_GATHER_COMPLETE) {
      return;
    }

    std::cerr << "Gathering complete for " <<  name_ << std::endl;
    gathering_complete_ = true;

    std::cerr << "CANDIDATES:" << std::endl;
    for (size_t i=0; i<streams_.size(); ++i) {
      std::cerr << "Stream " << name_ << std::endl;
      std::vector<std::string> candidates =
          streams_[i]->GetCandidates();

      for(size_t j=0; j<candidates.size(); ++j) {
        std::cerr << candidates[j] << std::endl;
      }
    }
    std::cerr << std::endl;

  }

  void CandidateInitialized(NrIceMediaStream *stream, const std::string &candidate) {
    std::cerr << "Candidate initialized: " << candidate << std::endl;
    candidates_[stream->name()].push_back(candidate);

    // If we are connected, then try to trickle to the
    // other side.
    if (remote_ && remote_->remote_) {
      std::vector<mozilla::RefPtr<NrIceMediaStream> >::iterator it =
          std::find(streams_.begin(), streams_.end(), stream);
      ASSERT_NE(streams_.end(), it);
      size_t index = it - streams_.begin();

      ASSERT_GT(remote_->streams_.size(), index);
      nsresult res = remote_->streams_[index]->ParseTrickleCandidate(
          candidate);
      ASSERT_TRUE(NS_SUCCEEDED(res));
      ++trickled_;
    }
  }

  nsresult GetCandidatePairs(size_t stream_index,
                             std::vector<NrIceCandidatePair>* pairs) {
    MOZ_ASSERT(pairs);
    if (stream_index >= streams_.size()) {
      // Is there a better error for "no such index"?
      ADD_FAILURE() << "No such media stream index: " << stream_index;
      return NS_ERROR_INVALID_ARG;
    }

    nsresult res;
    test_utils->sts_target()->Dispatch(
        WrapRunnableRet(streams_[stream_index],
                        &NrIceMediaStream::GetCandidatePairs,
                        pairs,
                        &res),
        NS_DISPATCH_SYNC);
    return res;
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

  void DumpCandidatePairs(NrIceMediaStream *stream) {
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

  void DumpCandidatePairs() {
    std::cerr << "Dumping candidate pairs for all streams [" << std::endl;
    for (size_t s = 0; s < streams_.size(); ++s) {
      DumpCandidatePairs(streams_[s]);
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
        std::cerr << "Duplicate priority in subseqent pairs:" << std::endl;
        DumpCandidatePair(pairs[p-1]);
        DumpCandidatePair(pairs[p]);
        return false;
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
    std::cerr << "Stream ready " << stream->name() << " ct=" << ready_ct_ << std::endl;
    DumpCandidatePairs(stream);
  }
  void StreamFailed(NrIceMediaStream *stream) {
    std::cerr << "Stream failed " << stream->name() << " ct=" << ready_ct_ << std::endl;
    DumpCandidatePairs(stream);
  }

  void ConnectionStateChange(NrIceCtx* ctx,
                             NrIceCtx::ConnectionState state) {
    (void)ctx;
    if (state != NrIceCtx::ICE_CTX_OPEN) {
      return;
    }
    std::cerr << "ICE completed " << name_ << std::endl;
    ice_complete_ = true;
  }

  void PacketReceived(NrIceMediaStream *stream, int component, const unsigned char *data,
                      int len) {
    std::cerr << "Received " << len << " bytes" << std::endl;
    ++received_;
  }

  void SendPacket(int stream, int component, const unsigned char *data,
                  int len) {
    ASSERT_TRUE(NS_SUCCEEDED(streams_[stream]->SendPacket(component, data, len)));

    ++sent_;
    std::cerr << "Sent " << len << " bytes" << std::endl;
  }

  void SetCandidateFilter(CandidateFilter filter) {
    candidate_filter_ = filter;
  }

  // Allow us to parse candidates directly on the current thread.
  void ParseCandidate(size_t i, const std::string& candidate) {
    std::vector<std::string> attributes;

    attributes.push_back(candidate);
    streams_[i]->ParseAttributes(attributes);
  }

  void DisableComponent(size_t stream, int component_id) {
    ASSERT_LT(stream, streams_.size());
    nsresult res = streams_[stream]->DisableComponent(component_id);
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  int trickled() { return trickled_; }

 private:
  std::string name_;
  nsRefPtr<NrIceCtx> ice_ctx_;
  std::vector<mozilla::RefPtr<NrIceMediaStream> > streams_;
  std::map<std::string, std::vector<std::string> > candidates_;
  bool gathering_complete_;
  int ready_ct_;
  bool ice_complete_;
  size_t received_;
  size_t sent_;
  NrIceResolverFake fake_resolver_;
  nsRefPtr<NrIceResolver> dns_resolver_;
  IceTestPeer *remote_;
  CandidateFilter candidate_filter_;
  NrIceCandidate::Type expected_local_type_;
  std::string expected_local_transport_;
  NrIceCandidate::Type expected_remote_type_;
  TrickleMode trickle_mode_;
  int trickled_;
};

class IceGatherTest : public ::testing::Test {
 public:
  void SetUp() {
    test_utils->sts_target()->Dispatch(WrapRunnable(TestStunServer::GetInstance(),
                                                    &TestStunServer::Reset),
                                       NS_DISPATCH_SYNC);
    peer_ = new IceTestPeer("P1", true, false);
    peer_->AddStream(1);
  }

  void Gather(bool wait = true) {
     peer_->Gather();

    if (wait) {
      WaitForGather();
    }
  }

  void WaitForGather() {
    ASSERT_TRUE_WAIT(peer_->gathering_complete(), 10000);
  }

  void UseFakeStunServerWithResponse(const std::string& fake_addr,
                                     uint16_t fake_port) {
    TestStunServer::GetInstance()->SetResponseAddr(fake_addr, fake_port);
    // Sets an additional stun server
    peer_->SetStunServer(TestStunServer::GetInstance()->addr(),
                         TestStunServer::GetInstance()->port());
  }

  // NB: Only does substring matching, watch out for stuff like "1.2.3.4"
  // matching "21.2.3.47". " 1.2.3.4 " should not have false positives.
  bool StreamHasMatchingCandidate(unsigned int stream,
                                  const std::string& match) {
    std::vector<std::string> candidates = peer_->GetCandidates(stream);
    for (size_t c = 0; c < candidates.size(); ++c) {
      if (std::string::npos != candidates[c].find(match)) {
        return true;
      }
    }
    return false;
  }

 protected:
  mozilla::ScopedDeletePtr<IceTestPeer> peer_;
};

class IceConnectTest : public ::testing::Test {
 public:
  IceConnectTest() : initted_(false) {}

  void SetUp() {
    nsresult rv;
    target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    ASSERT_TRUE(NS_SUCCEEDED(rv));
  }

  void AddStream(const std::string& name, int components) {
    Init(false);
    p1_->AddStream(components);
    p2_->AddStream(components);
  }

  void Init(bool set_priorities) {
    if (!initted_) {
      p1_ = new IceTestPeer("P1", true, set_priorities);
      p2_ = new IceTestPeer("P2", false, set_priorities);
    }
    initted_ = true;
  }

  bool Gather(bool wait) {
    Init(false);
    p1_->SetStunServer(g_stun_server_address, kDefaultStunServerPort);
    p2_->SetStunServer(g_stun_server_address, kDefaultStunServerPort);
    p1_->Gather();
    p2_->Gather();

    if (wait) {
      EXPECT_TRUE_WAIT(p1_->gathering_complete(), 10000);
      if (!p1_->gathering_complete())
        return false;
      EXPECT_TRUE_WAIT(p2_->gathering_complete(), 10000);
      if (!p2_->gathering_complete())
        return false;
    }
    return true;
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
    p1_->Connect(p2_, TRICKLE_NONE);
    p2_->Connect(p1_, TRICKLE_NONE);

    ASSERT_TRUE_WAIT(p1_->ready_ct() == 1 && p2_->ready_ct() == 1, 5000);
    ASSERT_TRUE_WAIT(p1_->ice_complete() && p2_->ice_complete(), 5000);

    p1_->DumpAndCheckActiveCandidates();
    p2_->DumpAndCheckActiveCandidates();
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

  void ConnectP1(TrickleMode mode = TRICKLE_NONE) {
    p1_->Connect(p2_, mode);
  }

  void ConnectP2(TrickleMode mode = TRICKLE_NONE) {
    p2_->Connect(p1_, mode);
  }

  void WaitForComplete(int expected_streams = 1) {
    ASSERT_TRUE_WAIT(p1_->ready_ct() == expected_streams &&
                     p2_->ready_ct() == expected_streams, 5000);
    ASSERT_TRUE_WAIT(p1_->ice_complete() && p2_->ice_complete(), 5000);
  }

  void WaitForGather() {
    ASSERT_TRUE_WAIT(p1_->gathering_complete(), 10000);
    ASSERT_TRUE_WAIT(p2_->gathering_complete(), 10000);
  }

  void ConnectTrickle(TrickleMode trickle = TRICKLE_SIMULATE) {
    p1_->Connect(p2_, trickle);
    p2_->Connect(p1_, trickle);
  }

  void SimulateTrickle(size_t stream) {
    p1_->SimulateTrickle(stream);
    p2_->SimulateTrickle(stream);
    ASSERT_TRUE_WAIT(p1_->is_ready(stream), 5000);
    ASSERT_TRUE_WAIT(p2_->is_ready(stream), 5000);
  }

  void SimulateTrickleP1(size_t stream) {
    p1_->SimulateTrickle(stream);
  }

  void SimulateTrickleP2(size_t stream) {
    p2_->SimulateTrickle(stream);
  }

  void VerifyConnected() {
  }

  void CloseP1() {
    p1_->Close();
  }

  void ConnectThenDelete() {
    p1_->Connect(p2_, TRICKLE_NONE, true);
    p2_->Connect(p1_, TRICKLE_NONE, false);
    test_utils->sts_target()->Dispatch(WrapRunnable(this,
                                                    &IceConnectTest::CloseP1),
                                       NS_DISPATCH_SYNC);
    p2_->StartChecks();

    // Wait to see if we crash
    PR_Sleep(PR_MillisecondsToInterval(5000));
  }

  void SendReceive() {
    //    p1_->Send(2);
    test_utils->sts_target()->Dispatch(
        WrapRunnable(p1_.get(),
                     &IceTestPeer::SendPacket, 0, 1,
                     reinterpret_cast<const unsigned char *>("TEST"), 4),
        NS_DISPATCH_SYNC);
    ASSERT_EQ(1u, p1_->sent());
    ASSERT_TRUE_WAIT(p2_->received() == 1, 1000);
  }

 protected:
  bool initted_;
  nsCOMPtr<nsIEventTarget> target_;
  mozilla::ScopedDeletePtr<IceTestPeer> p1_;
  mozilla::ScopedDeletePtr<IceTestPeer> p2_;
};

class PrioritizerTest : public ::testing::Test {
 public:
  PrioritizerTest():
    prioritizer_(nullptr) {}

  ~PrioritizerTest() {
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

    int r = nr_ip4_str_port_to_transport_addr(str_addr.c_str(), 0,
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

class PacketFilterTest : public ::testing::Test {
 public:
  PacketFilterTest(): filter_(nullptr) {}

  void SetUp() {
    nsCOMPtr<nsIUDPSocketFilterHandler> handler =
      do_GetService(NS_STUN_UDP_SOCKET_FILTER_HANDLER_CONTRACTID);
    handler->NewFilter(getter_AddRefs(filter_));
  }

  void TestIncoming(const uint8_t* data, uint32_t len,
                    uint8_t from_addr, int from_port,
                    bool expected_result) {
    mozilla::net::NetAddr addr;
    MakeNetAddr(&addr, from_addr, from_port);
    bool result;
    nsresult rv = filter_->FilterPacket(&addr, data, len,
                                        nsIUDPSocketFilter::SF_INCOMING,
                                        &result);
    ASSERT_EQ(NS_OK, rv);
    ASSERT_EQ(expected_result, result);
  }

  void TestOutgoing(const uint8_t* data, uint32_t len,
                    uint8_t to_addr, int to_port,
                    bool expected_result) {
    mozilla::net::NetAddr addr;
    MakeNetAddr(&addr, to_addr, to_port);
    bool result;
    nsresult rv = filter_->FilterPacket(&addr, data, len,
                                        nsIUDPSocketFilter::SF_OUTGOING,
                                        &result);
    ASSERT_EQ(NS_OK, rv);
    ASSERT_EQ(expected_result, result);
  }

 private:
  void MakeNetAddr(mozilla::net::NetAddr* net_addr,
                   uint8_t last_digit, uint16_t port) {
    net_addr->inet.family = AF_INET;
    net_addr->inet.ip = 192 << 24 | 168 << 16 | 1 << 8 | last_digit;
    net_addr->inet.port = port;
  }

  nsCOMPtr<nsIUDPSocketFilter> filter_;
};
}  // end namespace

TEST_F(IceGatherTest, TestGatherFakeStunServerHostnameNoResolver) {
  peer_->SetStunServer(g_stun_server_hostname, kDefaultStunServerPort);
  Gather();
}

TEST_F(IceGatherTest, TestGatherFakeStunServerIpAddress) {
  peer_->SetStunServer(g_stun_server_address, kDefaultStunServerPort);
  peer_->SetFakeResolver();
  Gather();
}

TEST_F(IceGatherTest, TestGatherFakeStunServerHostname) {
  peer_->SetStunServer(g_stun_server_hostname, kDefaultStunServerPort);
  peer_->SetFakeResolver();
  Gather();
}

TEST_F(IceGatherTest, TestGatherFakeStunBogusHostname) {
  peer_->SetStunServer(kBogusStunServerHostname, kDefaultStunServerPort);
  peer_->SetFakeResolver();
  Gather();
}

TEST_F(IceGatherTest, TestGatherDNSStunServerIpAddress) {
  peer_->SetStunServer(g_stun_server_address, kDefaultStunServerPort);
  peer_->SetDNSResolver();
  Gather();
  // TODO(jib@mozilla.com): ensure we get server reflexive candidates Bug 848094
}

TEST_F(IceGatherTest, TestGatherDNSStunServerHostname) {
  peer_->SetStunServer(g_stun_server_hostname, kDefaultStunServerPort);
  peer_->SetDNSResolver();
  Gather();
}

TEST_F(IceGatherTest, TestGatherDNSStunBogusHostname) {
  peer_->SetStunServer(kBogusStunServerHostname, kDefaultStunServerPort);
  peer_->SetDNSResolver();
  Gather();
}

TEST_F(IceGatherTest, TestGatherTurn) {
  if (g_turn_server.empty())
    return;
  peer_->SetTurnServer(g_turn_server, kDefaultStunServerPort,
                       g_turn_user, g_turn_password, kNrIceTransportUdp);
  Gather();
}

TEST_F(IceGatherTest, TestGatherTurnTcp) {
  if (g_turn_server.empty())
    return;
  peer_->SetTurnServer(g_turn_server, kDefaultStunServerPort,
                       g_turn_user, g_turn_password, kNrIceTransportTcp);
  Gather();
}

TEST_F(IceGatherTest, TestGatherDisableComponent) {
  peer_->SetStunServer(kDefaultStunServerHostname, kDefaultStunServerPort);
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


// Verify that a bogus candidate doesn't cause crashes on the
// main thread. See bug 856433.
TEST_F(IceGatherTest, TestBogusCandidate) {
  Gather();
  peer_->ParseCandidate(0, kBogusIceCandidate);
}

TEST_F(IceGatherTest, VerifyTestStunServer) {
  UseFakeStunServerWithResponse("192.0.2.133", 3333);
  Gather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, " 192.0.2.133 3333 "));
}

TEST_F(IceGatherTest, TestStunServerReturnsWildcardAddr) {
  UseFakeStunServerWithResponse("0.0.0.0", 3333);
  Gather();
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " 0.0.0.0 "));
}

TEST_F(IceGatherTest, TestStunServerReturnsPort0) {
  UseFakeStunServerWithResponse("192.0.2.133", 0);
  Gather();
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " 192.0.2.133 0 "));
}

TEST_F(IceGatherTest, TestStunServerReturnsLoopbackAddr) {
  UseFakeStunServerWithResponse("127.0.0.133", 3333);
  Gather();
  ASSERT_FALSE(StreamHasMatchingCandidate(0, " 127.0.0.133 "));
}

TEST_F(IceGatherTest, TestStunServerTrickle) {
  UseFakeStunServerWithResponse("192.0.2.1", 3333);
  TestStunServer::GetInstance()->SetActive(false);
  Gather(false);
  ASSERT_FALSE(StreamHasMatchingCandidate(0, "192.0.2.1"));
  TestStunServer::GetInstance()->SetActive(true);
  WaitForGather();
  ASSERT_TRUE(StreamHasMatchingCandidate(0, "192.0.2.1"));
}

TEST_F(IceConnectTest, TestGather) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
}

TEST_F(IceConnectTest, TestGatherAutoPrioritize) {
  Init(false);
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
}


TEST_F(IceConnectTest, TestConnect) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  Connect();
}

TEST_F(IceConnectTest, TestConnectTwoComponents) {
  AddStream("first", 2);
  ASSERT_TRUE(Gather(true));
  Connect();
}

TEST_F(IceConnectTest, TestConnectTwoComponentsDisableSecond) {
  AddStream("first", 2);
  ASSERT_TRUE(Gather(true));
  p1_->DisableComponent(0, 2);
  p2_->DisableComponent(0, 2);
  Connect();
}


TEST_F(IceConnectTest, TestConnectP2ThenP1) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  ConnectP2();
  PR_Sleep(1000);
  ConnectP1();
  WaitForComplete();
}

TEST_F(IceConnectTest, TestConnectP2ThenP1Trickle) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  ConnectP2();
  PR_Sleep(1000);
  ConnectP1(TRICKLE_SIMULATE);
  SimulateTrickleP1(0);
  WaitForComplete();
}

TEST_F(IceConnectTest, TestConnectP2ThenP1TrickleTwoComponents) {
  AddStream("first", 1);
  AddStream("second", 2);
  ASSERT_TRUE(Gather(true));
  ConnectP2();
  PR_Sleep(1000);
  ConnectP1(TRICKLE_SIMULATE);
  SimulateTrickleP1(0);
  std::cerr << "Sleeping between trickle streams" << std::endl;
  PR_Sleep(1000);  // Give this some time to settle but not complete
                   // all of ICE.
  SimulateTrickleP1(1);
  WaitForComplete(2);
}

TEST_F(IceConnectTest, TestConnectAutoPrioritize) {
  Init(false);
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  Connect();
}

TEST_F(IceConnectTest, TestConnectTrickleOneStreamOneComponent) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  ConnectTrickle();
  SimulateTrickle(0);
  ASSERT_TRUE_WAIT(p1_->ice_complete(), 1000);
  ASSERT_TRUE_WAIT(p2_->ice_complete(), 1000);
}

TEST_F(IceConnectTest, TestConnectTrickleTwoStreamsOneComponent) {
  AddStream("first", 1);
  AddStream("second", 1);
  ASSERT_TRUE(Gather(true));
  ConnectTrickle();
  SimulateTrickle(0);
  SimulateTrickle(1);
  ASSERT_TRUE_WAIT(p1_->ice_complete(), 1000);
  ASSERT_TRUE_WAIT(p2_->ice_complete(), 1000);
}

TEST_F(IceConnectTest, TestConnectRealTrickleOneStreamOneComponent) {
  AddStream("first", 1);
  AddStream("second", 1);
  ASSERT_TRUE(Gather(false));
  ConnectTrickle(TRICKLE_REAL);
  ASSERT_TRUE_WAIT(p1_->ice_complete(), 5000);
  ASSERT_TRUE_WAIT(p2_->ice_complete(), 5000);
  WaitForGather();  // ICE can complete before we finish gathering.
}

TEST_F(IceConnectTest, TestSendReceive) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  Connect();
  SendReceive();
}

TEST_F(IceConnectTest, TestConnectTurn) {
  if (g_turn_server.empty())
    return;

  AddStream("first", 1);
  SetTurnServer(g_turn_server, kDefaultStunServerPort,
                g_turn_user, g_turn_password);
  ASSERT_TRUE(Gather(true));
  Connect();
}

TEST_F(IceConnectTest, TestConnectTurnTcp) {
  if (g_turn_server.empty())
    return;

  AddStream("first", 1);
  SetTurnServer(g_turn_server, kDefaultStunServerPort,
                g_turn_user, g_turn_password, kNrIceTransportTcp);
  ASSERT_TRUE(Gather(true));
  Connect();
}

TEST_F(IceConnectTest, TestConnectTurnOnly) {
  if (g_turn_server.empty())
    return;

  AddStream("first", 1);
  SetTurnServer(g_turn_server, kDefaultStunServerPort,
                g_turn_user, g_turn_password);
  ASSERT_TRUE(Gather(true));
  SetCandidateFilter(IsRelayCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                   NrIceCandidate::Type::ICE_RELAYED);
  Connect();
}

TEST_F(IceConnectTest, TestConnectTurnTcpOnly) {
  if (g_turn_server.empty())
    return;

  AddStream("first", 1);
  SetTurnServer(g_turn_server, kDefaultStunServerPort,
                g_turn_user, g_turn_password, kNrIceTransportTcp);
  ASSERT_TRUE(Gather(true));
  SetCandidateFilter(IsRelayCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                   NrIceCandidate::Type::ICE_RELAYED,
                   kNrIceTransportTcp);
  Connect();
}

TEST_F(IceConnectTest, TestSendReceiveTurnOnly) {
  if (g_turn_server.empty())
    return;

  AddStream("first", 1);
  SetTurnServer(g_turn_server, kDefaultStunServerPort,
                g_turn_user, g_turn_password);
  ASSERT_TRUE(Gather(true));
  SetCandidateFilter(IsRelayCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                   NrIceCandidate::Type::ICE_RELAYED);
  Connect();
  SendReceive();
}

TEST_F(IceConnectTest, TestSendReceiveTurnTcpOnly) {
  if (g_turn_server.empty())
    return;

  AddStream("first", 1);
  SetTurnServer(g_turn_server, kDefaultStunServerPort,
                g_turn_user, g_turn_password, kNrIceTransportTcp);
  ASSERT_TRUE(Gather(true));
  SetCandidateFilter(IsRelayCandidate);
  SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                   NrIceCandidate::Type::ICE_RELAYED,
                   kNrIceTransportTcp);
  Connect();
  SendReceive();
}

TEST_F(IceConnectTest, TestSendReceiveTurnBothOnly) {
  if (g_turn_server.empty())
    return;

  AddStream("first", 1);
  std::vector<NrIceTurnServer> turn_servers;
  std::vector<unsigned char> password_vec(g_turn_password.begin(),
                                          g_turn_password.end());
  turn_servers.push_back(*NrIceTurnServer::Create(
                           g_turn_server, kDefaultStunServerPort,
                           g_turn_user, password_vec, kNrIceTransportTcp));
  turn_servers.push_back(*NrIceTurnServer::Create(
                           g_turn_server, kDefaultStunServerPort,
                           g_turn_user, password_vec, kNrIceTransportUdp));
  SetTurnServers(turn_servers);
  ASSERT_TRUE(Gather(true));
  SetCandidateFilter(IsRelayCandidate);
  // UDP is preferred.
  SetExpectedTypes(NrIceCandidate::Type::ICE_RELAYED,
                   NrIceCandidate::Type::ICE_RELAYED,
                   kNrIceTransportUdp);
  Connect();
  SendReceive();
}

TEST_F(IceConnectTest, TestConnectShutdownOneSide) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  ConnectThenDelete();
}

TEST_F(IceConnectTest, TestPollCandPairsBeforeConnect) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));

  std::vector<NrIceCandidatePair> pairs;
  nsresult res = p1_->GetCandidatePairs(0, &pairs);
  // There should be no candidate pairs prior to calling Connect()
  ASSERT_TRUE(NS_FAILED(res));
  ASSERT_EQ(0U, pairs.size());

  res = p2_->GetCandidatePairs(0, &pairs);
  ASSERT_TRUE(NS_FAILED(res));
  ASSERT_EQ(0U, pairs.size());
}

TEST_F(IceConnectTest, TestPollCandPairsAfterConnect) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
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

TEST_F(IceConnectTest, TestPollCandPairsDuringConnect) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));

  p1_->Connect(p2_, TRICKLE_NONE, false);
  p2_->Connect(p1_, TRICKLE_NONE, false);

  std::vector<NrIceCandidatePair> pairs1;
  std::vector<NrIceCandidatePair> pairs2;

  p1_->StartChecks();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);

  p2_->StartChecks();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);

  WaitForComplete();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);
  ASSERT_TRUE(ContainsSucceededPair(pairs1));
  ASSERT_TRUE(ContainsSucceededPair(pairs2));
}

TEST_F(IceConnectTest, TestRLogRingBuffer) {
  RLogRingBuffer::CreateInstance();
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));

  p1_->Connect(p2_, TRICKLE_NONE, false);
  p2_->Connect(p1_, TRICKLE_NONE, false);

  std::vector<NrIceCandidatePair> pairs1;
  std::vector<NrIceCandidatePair> pairs2;

  p1_->StartChecks();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);

  p2_->StartChecks();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);

  WaitForComplete();
  p1_->UpdateAndValidateCandidatePairs(0, &pairs1);
  p2_->UpdateAndValidateCandidatePairs(0, &pairs2);
  ASSERT_TRUE(ContainsSucceededPair(pairs1));
  ASSERT_TRUE(ContainsSucceededPair(pairs2));

  for (auto p = pairs1.begin(); p != pairs1.end(); ++p) {
    std::deque<std::string> logs;
    std::string substring("CAND-PAIR(");
    substring += p->codeword;
    RLogRingBuffer::GetInstance()->Filter(substring, 0, &logs);
    ASSERT_NE(0U, logs.size());
  }

  for (auto p = pairs2.begin(); p != pairs2.end(); ++p) {
    std::deque<std::string> logs;
    std::string substring("CAND-PAIR(");
    substring += p->codeword;
    RLogRingBuffer::GetInstance()->Filter(substring, 0, &logs);
    ASSERT_NE(0U, logs.size());
  }

  RLogRingBuffer::DestroyInstance();
}

TEST_F(PrioritizerTest, TestPrioritizer) {
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

TEST_F(PacketFilterTest, TestSendNonStunPacket) {
  const unsigned char data[] = "12345abcde";
  TestOutgoing(data, sizeof(data), 123, 45, false);
}

TEST_F(PacketFilterTest, TestRecvNonStunPacket) {
  const unsigned char data[] = "12345abcde";
  TestIncoming(data, sizeof(data), 123, 45, false);
}

TEST_F(PacketFilterTest, TestSendStunPacket) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));
  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);
  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST_F(PacketFilterTest, TestRecvStunPacketWithoutAPendingId) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));

  msg->header.id.octet[0] = 1;
  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);

  msg->header.id.octet[0] = 0;
  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestIncoming(msg->buffer, msg->length, 123, 45, true);

  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST_F(PacketFilterTest, TestRecvStunPacketWithoutAPendingAddress) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));

  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);

  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestIncoming(msg->buffer, msg->length, 123, 46, false);
  TestIncoming(msg->buffer, msg->length, 124, 45, false);

  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST_F(PacketFilterTest, TestRecvStunPacketWithPendingIdAndAddress) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));

  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);

  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestIncoming(msg->buffer, msg->length, 123, 45, true);

  // Test whitelist by filtering non-stun packets.
  const unsigned char data[] = "12345abcde";

  // 123:45 is white-listed.
  TestOutgoing(data, sizeof(data), 123, 45, true);
  TestIncoming(data, sizeof(data), 123, 45, true);

  // Indications pass as well.
  msg->header.type = NR_STUN_MSG_BINDING_INDICATION;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);
  TestIncoming(msg->buffer, msg->length, 123, 45, true);

  // Packets from and to other address are still disallowed.
  TestOutgoing(data, sizeof(data), 123, 46, false);
  TestIncoming(data, sizeof(data), 123, 46, false);
  TestOutgoing(data, sizeof(data), 124, 45, false);
  TestIncoming(data, sizeof(data), 124, 45, false);

  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

TEST_F(PacketFilterTest, TestSendNonRequestStunPacket) {
  nr_stun_message *msg;
  ASSERT_EQ(0, nr_stun_build_req_no_auth(NULL, &msg));

  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, false);

  // Send a packet so we allow the incoming request.
  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);

  // This packet makes us able to send a response.
  msg->header.type = NR_STUN_MSG_BINDING_REQUEST;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestIncoming(msg->buffer, msg->length, 123, 45, true);

  msg->header.type = NR_STUN_MSG_BINDING_RESPONSE;
  ASSERT_EQ(0, nr_stun_encode_message(msg));
  TestOutgoing(msg->buffer, msg->length, 123, 45, true);

  ASSERT_EQ(0, nr_stun_message_destroy(&msg));
}

static std::string get_environment(const char *name) {
  char *value = getenv(name);

  if (!value)
    return "";

  return value;
}

int main(int argc, char **argv)
{
#ifdef LINUX
  // This test can cause intermittent oranges on the builders on Linux
  CHECK_ENVIRONMENT_FLAG("MOZ_WEBRTC_TESTS")
#endif

  g_turn_server = get_environment("TURN_SERVER_ADDRESS");
  g_turn_user = get_environment("TURN_SERVER_USER");
  g_turn_password = get_environment("TURN_SERVER_PASSWORD");

  if (g_turn_server.empty() ||
      g_turn_user.empty(),
      g_turn_password.empty()) {
    printf(
        "Set TURN_SERVER_ADDRESS, TURN_SERVER_USER, and TURN_SERVER_PASSWORD\n"
        "environment variables to run this test\n");
    g_turn_server="";
  }

  std::string tmp = get_environment("STUN_SERVER_ADDRESS");
  if (tmp != "")
    g_stun_server_address = tmp;


  tmp = get_environment("STUN_SERVER_HOSTNAME");
  if (tmp != "")
    g_stun_server_hostname = tmp;

  test_utils = new MtransportTestUtils();
  NSS_NoDB_Init(nullptr);
  NSS_SetDomesticPolicy();

  // Start the tests
  ::testing::InitGoogleTest(&argc, argv);

  ::testing::TestEventListeners& listeners =
        ::testing::UnitTest::GetInstance()->listeners();
  // Adds a listener to the end.  Google Test takes the ownership.

  listeners.Append(new test::RingbufferDumper(test_utils));
  test_utils->sts_target()->Dispatch(
    WrapRunnableNM(&TestStunServer::GetInstance), NS_DISPATCH_SYNC);

  int rv = RUN_ALL_TESTS();

  test_utils->sts_target()->Dispatch(
    WrapRunnableNM(&TestStunServer::ShutdownInstance), NS_DISPATCH_SYNC);

  delete test_utils;
  return rv;
}
