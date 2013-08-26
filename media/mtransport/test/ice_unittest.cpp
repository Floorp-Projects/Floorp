/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <iostream>
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
#include "runnable_utils.h"

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

std::string g_turn_server;
std::string g_turn_user;
std::string g_turn_password;

namespace {

enum TrickleMode { TRICKLE_NONE, TRICKLE_DEFERRED };
typedef bool (*CandidateFilter)(const std::string& candidate);

static bool IsRelayCandidate(const std::string& candidate) {
  return candidate.find("typ relay") != std::string::npos;
}

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
      expected_remote_type_(NrIceCandidate::ICE_HOST) {
    ice_ctx_->SignalGatheringCompleted.connect(this,
                                               &IceTestPeer::GatheringComplete);
    ice_ctx_->SignalCompleted.connect(this, &IceTestPeer::IceCompleted);
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
    stream->SignalCandidate.connect(this, &IceTestPeer::GotCandidate);
    stream->SignalReady.connect(this, &IceTestPeer::StreamReady);
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
                     const std::string password) {
    std::vector<unsigned char> password_vec(password.begin(), password.end());
    SetTurnServer(addr, port, username, password_vec);
  }


  void SetTurnServer(const std::string addr, uint16_t port,
                     const std::string username,
                     const std::vector<unsigned char> password) {
    std::vector<NrIceTurnServer> turn_servers;
    ScopedDeletePtr<NrIceTurnServer> server(NrIceTurnServer::Create(
        addr, port, username, password));
    turn_servers.push_back(*server);
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->SetTurnServers(turn_servers)));
  }

  void SetFakeResolver() {
    ASSERT_TRUE(NS_SUCCEEDED(dns_resolver_->Init()));
    PRNetAddr addr;
    PRStatus status = PR_StringToNetAddr(kDefaultStunServerAddress.c_str(),
                                         &addr);
    ASSERT_EQ(PR_SUCCESS, status);
    fake_resolver_.SetAddr(kDefaultStunServerHostname, addr);
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

  void SetExpectedTypes(NrIceCandidate::Type local, NrIceCandidate::Type remote) {
    expected_local_type_ = local;
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
  void Connect(IceTestPeer *remote, TrickleMode trickle_mode,
               bool start = true) {
    nsresult res;

    remote_ = remote;

    test_utils->sts_target()->Dispatch(
      WrapRunnableRet(ice_ctx_,
        &NrIceCtx::ParseGlobalAttributes, remote->GetGlobalAttributes(), &res),
      NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(res));

    if (trickle_mode == TRICKLE_NONE) {
      for (size_t i=0; i<streams_.size(); ++i) {
        test_utils->sts_target()->Dispatch(
            WrapRunnableRet(streams_[i], &NrIceMediaStream::ParseAttributes,
                            remote->GetCandidates(i),
                            &res), NS_DISPATCH_SYNC);

        ASSERT_TRUE(NS_SUCCEEDED(res));
      }
    } else {
      // Parse empty attributes and then trickle them out later
      for (size_t i=0; i<streams_.size(); ++i) {
        std::vector<std::string> empty_attrs;
        test_utils->sts_target()->Dispatch(
            WrapRunnableRet(streams_[i], &NrIceMediaStream::ParseAttributes,
                            empty_attrs,
                            &res), NS_DISPATCH_SYNC);

        ASSERT_TRUE(NS_SUCCEEDED(res));
      }
    }

    if (start) {
      // Now start checks
      test_utils->sts_target()->Dispatch(
        WrapRunnableRet(ice_ctx_, &NrIceCtx::StartChecks, &res),
        NS_DISPATCH_SYNC);
      ASSERT_TRUE(NS_SUCCEEDED(res));
    }
  }

  void DoTrickle(size_t stream) {
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
        break;
      default:
        FAIL();
    };

    std::cerr << which
              << " --> "
              << type
              << " "
              << cand.host
              << ":"
              << cand.port
              << std::endl;
  }

  void DumpAndCheckActiveCandidates() {
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
          DumpCandidate("Remote ", *remote);
          ASSERT_EQ(expected_remote_type_, remote->type);
          delete local;
          delete remote;
        }
      }
    }
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
  void GatheringComplete(NrIceCtx *ctx) {
    gathering_complete_ = true;
  }

  void GotCandidate(NrIceMediaStream *stream, const std::string &candidate) {
    std::cerr << "Got candidate " << candidate << std::endl;
    candidates_[stream->name()].push_back(candidate);
  }

  void StreamReady(NrIceMediaStream *stream) {
    ++ready_ct_;
    std::cerr << "Stream ready " << stream->name() << " ct=" << ready_ct_ << std::endl;
  }

  void IceCompleted(NrIceCtx *ctx) {
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

  void DisableComponent(int stream, int component_id) {
    ASSERT_LT(stream, streams_.size());
    nsresult res = streams_[stream]->DisableComponent(component_id);
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

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
  NrIceCandidate::Type expected_remote_type_;
};

class IceGatherTest : public ::testing::Test {
 public:
  void SetUp() {
    peer_ = new IceTestPeer("P1", true, false);
    peer_->AddStream(1);
  }

  void Gather() {
    peer_->Gather();

    ASSERT_TRUE_WAIT(peer_->gathering_complete(), 10000);
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
    p1_->SetStunServer(kDefaultStunServerAddress, kDefaultStunServerPort);
    p2_->SetStunServer(kDefaultStunServerAddress, kDefaultStunServerPort);
    p1_->Gather();
    p2_->Gather();

    EXPECT_TRUE_WAIT(p1_->gathering_complete(), 10000);
    if (!p1_->gathering_complete())
      return false;
    EXPECT_TRUE_WAIT(p2_->gathering_complete(), 10000);
    if (!p2_->gathering_complete())
      return false;

    return true;
  }

  void SetTurnServer(const std::string addr, uint16_t port,
                     const std::string username,
                     const std::string password) {
    p1_->SetTurnServer(addr, port, username, password);
    p2_->SetTurnServer(addr, port, username, password);
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

  void SetExpectedTypes(NrIceCandidate::Type local, NrIceCandidate::Type remote) {
    p1_->SetExpectedTypes(local, remote);
    p2_->SetExpectedTypes(local, remote);
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

  void ConnectTrickle() {
    p1_->Connect(p2_, TRICKLE_DEFERRED);
    p2_->Connect(p1_, TRICKLE_DEFERRED);
  }

  void DoTrickle(size_t stream) {
    p1_->DoTrickle(stream);
    p2_->DoTrickle(stream);
    ASSERT_TRUE_WAIT(p1_->is_ready(stream), 5000);
    ASSERT_TRUE_WAIT(p2_->is_ready(stream), 5000);
  }

  void DoTrickleP1(size_t stream) {
    p1_->DoTrickle(stream);
  }

  void DoTrickleP2(size_t stream) {
    p2_->DoTrickle(stream);
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

}  // end namespace

TEST_F(IceGatherTest, TestGatherFakeStunServerHostnameNoResolver) {
  peer_->SetStunServer(kDefaultStunServerHostname, kDefaultStunServerPort);
  Gather();
}

TEST_F(IceGatherTest, TestGatherFakeStunServerIpAddress) {
  peer_->SetStunServer(kDefaultStunServerAddress, kDefaultStunServerPort);
  peer_->SetFakeResolver();
  Gather();
}

TEST_F(IceGatherTest, TestGatherFakeStunServerHostname) {
  peer_->SetStunServer(kDefaultStunServerHostname, kDefaultStunServerPort);
  peer_->SetFakeResolver();
  Gather();
}

TEST_F(IceGatherTest, TestGatherFakeStunBogusHostname) {
  peer_->SetStunServer(kBogusStunServerHostname, kDefaultStunServerPort);
  peer_->SetFakeResolver();
  Gather();
}

TEST_F(IceGatherTest, TestGatherDNSStunServerIpAddress) {
  peer_->SetStunServer(kDefaultStunServerAddress, kDefaultStunServerPort);
  peer_->SetDNSResolver();
  Gather();
  // TODO(jib@mozilla.com): ensure we get server reflexive candidates Bug 848094
}

TEST_F(IceGatherTest, TestGatherDNSStunServerHostname) {
  peer_->SetStunServer(kDefaultStunServerHostname, kDefaultStunServerPort);
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
                       g_turn_user, g_turn_password);
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
  ConnectP1(TRICKLE_DEFERRED);
  DoTrickleP1(0);
  WaitForComplete();
}

TEST_F(IceConnectTest, TestConnectP2ThenP1TrickleTwoComponents) {
  AddStream("first", 1);
  AddStream("second", 2);
  ASSERT_TRUE(Gather(true));
  ConnectP2();
  PR_Sleep(1000);
  ConnectP1(TRICKLE_DEFERRED);
  DoTrickleP1(0);
  std::cerr << "Sleeping between trickle streams" << std::endl;
  PR_Sleep(1000);  // Give this some time to settle but not complete
                   // all of ICE.
  DoTrickleP1(1);
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
  DoTrickle(0);
  ASSERT_TRUE_WAIT(p1_->ice_complete(), 1000);
  ASSERT_TRUE_WAIT(p2_->ice_complete(), 1000);
}

TEST_F(IceConnectTest, TestConnectTrickleTwoStreamsOneComponent) {
  AddStream("first", 1);
  AddStream("second", 1);
  ASSERT_TRUE(Gather(true));
  ConnectTrickle();
  DoTrickle(0);
  DoTrickle(1);
  ASSERT_TRUE_WAIT(p1_->ice_complete(), 1000);
  ASSERT_TRUE_WAIT(p2_->ice_complete(), 1000);
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

TEST_F(IceConnectTest, TestConnectShutdownOneSide) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  ConnectThenDelete();
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

  test_utils = new MtransportTestUtils();
  NSS_NoDB_Init(nullptr);
  NSS_SetDomesticPolicy();

  // Start the tests
  ::testing::InitGoogleTest(&argc, argv);

  int rv = RUN_ALL_TESTS();
  delete test_utils;
  return rv;
}
