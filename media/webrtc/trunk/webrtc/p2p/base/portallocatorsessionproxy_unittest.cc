/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/portallocatorsessionproxy.h"
#include "webrtc/p2p/client/basicportallocator.h"
#include "webrtc/p2p/client/fakeportallocator.h"
#include "webrtc/base/fakenetwork.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/thread.h"

using cricket::Candidate;
using cricket::PortAllocatorSession;
using cricket::PortAllocatorSessionMuxer;
using cricket::PortAllocatorSessionProxy;

// Based on ICE_UFRAG_LENGTH
static const char kIceUfrag0[] = "TESTICEUFRAG0000";
// Based on ICE_PWD_LENGTH
static const char kIcePwd0[] = "TESTICEPWD00000000000000";

class TestSessionChannel : public sigslot::has_slots<> {
 public:
  explicit TestSessionChannel(PortAllocatorSessionProxy* proxy)
      : proxy_session_(proxy),
        candidates_count_(0),
        allocation_complete_(false),
        ports_count_(0) {
    proxy_session_->SignalCandidatesAllocationDone.connect(
        this, &TestSessionChannel::OnCandidatesAllocationDone);
    proxy_session_->SignalCandidatesReady.connect(
        this, &TestSessionChannel::OnCandidatesReady);
    proxy_session_->SignalPortReady.connect(
        this, &TestSessionChannel::OnPortReady);
  }
  virtual ~TestSessionChannel() {
    delete proxy_session_;
  }
  void OnCandidatesReady(PortAllocatorSession* session,
                         const std::vector<Candidate>& candidates) {
    EXPECT_EQ(proxy_session_, session);
    candidates_count_ += static_cast<int>(candidates.size());
  }
  void OnCandidatesAllocationDone(PortAllocatorSession* session) {
    EXPECT_EQ(proxy_session_, session);
    allocation_complete_ = true;
  }
  void OnPortReady(PortAllocatorSession* session,
                   cricket::PortInterface* port) {
    EXPECT_EQ(proxy_session_, session);
    ++ports_count_;
  }
  int candidates_count() { return candidates_count_; }
  bool allocation_complete() { return allocation_complete_; }
  int ports_count() { return ports_count_; }

  void StartGettingPorts() {
    proxy_session_->StartGettingPorts();
  }

  void StopGettingPorts() {
    proxy_session_->StopGettingPorts();
  }

  bool IsGettingPorts() {
    return proxy_session_->IsGettingPorts();
  }

 private:
  PortAllocatorSessionProxy* proxy_session_;
  int candidates_count_;
  bool allocation_complete_;
  int ports_count_;
};

class PortAllocatorSessionProxyTest : public testing::Test {
 public:
  PortAllocatorSessionProxyTest()
      : socket_factory_(rtc::Thread::Current()),
        allocator_(rtc::Thread::Current(), NULL),
        session_(new cricket::FakePortAllocatorSession(
                     rtc::Thread::Current(), &socket_factory_,
                     "test content", 1,
                     kIceUfrag0, kIcePwd0)),
        session_muxer_(new PortAllocatorSessionMuxer(session_)) {
  }
  virtual ~PortAllocatorSessionProxyTest() {}
  void RegisterSessionProxy(PortAllocatorSessionProxy* proxy) {
    session_muxer_->RegisterSessionProxy(proxy);
  }

  TestSessionChannel* CreateChannel() {
    PortAllocatorSessionProxy* proxy =
        new PortAllocatorSessionProxy("test content", 1, 0);
    TestSessionChannel* channel = new TestSessionChannel(proxy);
    session_muxer_->RegisterSessionProxy(proxy);
    channel->StartGettingPorts();
    return channel;
  }

 protected:
  rtc::BasicPacketSocketFactory socket_factory_;
  cricket::FakePortAllocator allocator_;
  cricket::FakePortAllocatorSession* session_;
  // Muxer object will be delete itself after all registered session proxies
  // are deleted.
  PortAllocatorSessionMuxer* session_muxer_;
};

TEST_F(PortAllocatorSessionProxyTest, TestBasic) {
  TestSessionChannel* channel = CreateChannel();
  EXPECT_EQ_WAIT(1, channel->candidates_count(), 1000);
  EXPECT_EQ(1, channel->ports_count());
  EXPECT_TRUE(channel->allocation_complete());
  delete channel;
}

TEST_F(PortAllocatorSessionProxyTest, TestLateBinding) {
  TestSessionChannel* channel1 = CreateChannel();
  EXPECT_EQ_WAIT(1, channel1->candidates_count(), 1000);
  EXPECT_EQ(1, channel1->ports_count());
  EXPECT_TRUE(channel1->allocation_complete());
  EXPECT_EQ(1, session_->port_config_count());
  // Creating another PortAllocatorSessionProxy and it also should receive
  // already happened events.
  PortAllocatorSessionProxy* proxy =
      new PortAllocatorSessionProxy("test content", 2, 0);
  TestSessionChannel* channel2 = new TestSessionChannel(proxy);
  session_muxer_->RegisterSessionProxy(proxy);
  EXPECT_TRUE(channel2->IsGettingPorts());
  EXPECT_EQ_WAIT(1, channel2->candidates_count(), 1000);
  EXPECT_EQ(1, channel2->ports_count());
  EXPECT_TRUE_WAIT(channel2->allocation_complete(), 1000);
  EXPECT_EQ(1, session_->port_config_count());
  delete channel1;
  delete channel2;
}
