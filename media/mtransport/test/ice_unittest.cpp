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

#include "nsThreadUtils.h"
#include "nsXPCOM.h"

#include "logging.h"
#include "nricectx.h"
#include "nricemediastream.h"
#include "mtransport_test_utils.h"
#include "runnable_utils.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;
MtransportTestUtils *test_utils;

bool stream_added = false;

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
      remote_(nullptr) {
    ice_ctx_->SignalGatheringCompleted.connect(this,
                                              &IceTestPeer::GatheringComplete);
    ice_ctx_->SignalCompleted.connect(this, &IceTestPeer::IceCompleted);
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
    ice_ctx_->destroy_peer_ctx();
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
  IceTestPeer *remote_;
};

class IceTest : public ::testing::Test {
 public:
  IceTest() : initted_(false) {}

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
                                                    &IceTest::CloseP1),
                                       NS_DISPATCH_SYNC);
    p2_->StartChecks();

    // Wait to see if we crash
    PR_Sleep(PR_MillisecondsToInterval(5000));
  }

  void SendReceive() {
    //    p1_->Send(2);
    p1_->SendPacket(0, 1, reinterpret_cast<const unsigned char *>("TEST"), 4);
    ASSERT_EQ(1, p1_->sent());
    ASSERT_TRUE_WAIT(p2_->received() == 1, 1000);
  }

 protected:
  bool initted_;
  nsCOMPtr<nsIEventTarget> target_;
  mozilla::ScopedDeletePtr<IceTestPeer> p1_;
  mozilla::ScopedDeletePtr<IceTestPeer> p2_;
};

}  // end namespace


TEST_F(IceTest, TestGather) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
}

TEST_F(IceTest, TestGatherAutoPrioritize) {
  Init(false);
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
}


TEST_F(IceTest, TestConnect) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  Connect();
}

TEST_F(IceTest, TestConnectAutoPrioritize) {
  Init(false);
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  Connect();
}

TEST_F(IceTest, TestConnectTrickleOneStreamOneComponent) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  ConnectTrickle();
  DoTrickle(0);
  ASSERT_TRUE_WAIT(p1_->ice_complete(), 1000);
  ASSERT_TRUE_WAIT(p2_->ice_complete(), 1000);
}

TEST_F(IceTest, TestConnectTrickleTwoStreamsOneComponent) {
  AddStream("first", 1);
  AddStream("second", 1);
  ASSERT_TRUE(Gather(true));
  ConnectTrickle();
  DoTrickle(0);
  DoTrickle(1);
  ASSERT_TRUE_WAIT(p1_->ice_complete(), 1000);
  ASSERT_TRUE_WAIT(p2_->ice_complete(), 1000);
}


TEST_F(IceTest, TestSendReceive) {
  AddStream("first", 1);
  ASSERT_TRUE(Gather(true));
  Connect();
  SendReceive();
}

TEST_F(IceTest, TestConnectShutdownOneSide) {
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
