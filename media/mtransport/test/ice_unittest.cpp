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

#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include "mozilla/Scoped.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"

#include "logging.h"
#include "nricectx.h"
#include "nricemediastream.h"
#include "nriceresolverfake.h"
#include "nriceresolver.h"
#include "mtransport_test_utils.h"
#include "runnable_utils.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;
MtransportTestUtils *test_utils;

bool stream_added = false;

const std::string kDefaultStunServerAddress((char *)"23.21.150.121");
const std::string kDefaultStunServerHostname((char *)"ec2-23-21-150-121.compute-1.amazonaws.com");
const std::string kBogusStunServerHostname((char *)"stun-server-nonexistent.invalid");
const uint16_t kDefaultStunServerPort=3478;

namespace {

enum TrickleMode { TRICKLE_NONE, TRICKLE_DEFERRED };

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
      remote_(nullptr) {
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
    snprintf(name, sizeof(name), "%s:stream%d", name_.c_str(), (int)streams_.size());

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

  void SetFakeResolver() {
    ASSERT_TRUE(NS_SUCCEEDED(dns_resolver_->Init()));
    PRNetAddr addr;
    PRStatus status = PR_StringToNetAddr(kDefaultStunServerAddress.c_str(), &addr);
    ASSERT_EQ(PR_SUCCESS, status);
    fake_resolver_.SetAddr(kDefaultStunServerHostname, addr);
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->SetResolver(fake_resolver_.AllocateResolver())));
  }

  void SetDNSResolver() {
    ASSERT_TRUE(NS_SUCCEEDED(dns_resolver_->Init()));
    ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->SetResolver(dns_resolver_->AllocateResolver())));
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

  std::vector<std::string>& GetCandidates(const std::string &name) {
    return candidates_[name];
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
  void Connect(IceTestPeer *remote, TrickleMode trickle_mode, bool start = true) {
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
                            remote->GetCandidates(remote->streams_[i]->name()),
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
      remote_->GetCandidates(remote_->streams_[stream]->name());

    for (size_t j=0; j<candidates.size(); j++) {
      test_utils->sts_target()->Dispatch(
        WrapRunnableRet(streams_[stream],
                        &NrIceMediaStream::ParseTrickleCandidate,
                        candidates[j],
                        &res), NS_DISPATCH_SYNC);

      ASSERT_TRUE(NS_SUCCEEDED(res));
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
    std::cout << "Got candidate " << candidate << std::endl;
    candidates_[stream->name()].push_back(candidate);
  }

  void StreamReady(NrIceMediaStream *stream) {
    std::cout << "Stream ready " << stream->name() << std::endl;
    ++ready_ct_;
  }

  void IceCompleted(NrIceCtx *ctx) {
    std::cout << "ICE completed " << name_ << std::endl;
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

  void Connect() {
    p1_->Connect(p2_, TRICKLE_NONE);
    p2_->Connect(p1_, TRICKLE_NONE);

    ASSERT_TRUE_WAIT(p1_->ready_ct() == 1 && p2_->ready_ct() == 1, 5000);
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

TEST_F(IceConnectTest, TestConnectShutdownOneSide) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  ConnectThenDelete();
}


int main(int argc, char **argv)
{
#ifdef LINUX
  // This test can cause intermittent oranges on the builders on Linux
  CHECK_ENVIRONMENT_FLAG("MOZ_WEBRTC_TESTS")
#endif

  test_utils = new MtransportTestUtils();
  NSS_NoDB_Init(nullptr);
  NSS_SetDomesticPolicy();

  // Start the tests
  ::testing::InitGoogleTest(&argc, argv);

  int rv = RUN_ALL_TESTS();
  delete test_utils;
  return rv;
}
