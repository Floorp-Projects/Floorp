/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/fakesslidentity.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/network.h"
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "webrtc/p2p/base/p2ptransport.h"

using cricket::Candidate;
using cricket::Candidates;
using cricket::Transport;
using cricket::FakeTransport;
using cricket::TransportChannel;
using cricket::FakeTransportChannel;
using cricket::IceRole;
using cricket::TransportDescription;
using rtc::SocketAddress;

static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";

static const char kIceUfrag2[] = "TESTICEUFRAG0002";
static const char kIcePwd2[] = "TESTICEPWD00000000000002";

class TransportTest : public testing::Test,
                      public sigslot::has_slots<> {
 public:
  TransportTest()
      : transport_(new FakeTransport("test content name")), channel_(NULL) {}
  ~TransportTest() {
    transport_->DestroyAllChannels();
  }
  bool SetupChannel() {
    channel_ = CreateChannel(1);
    return (channel_ != NULL);
  }
  FakeTransportChannel* CreateChannel(int component) {
    return static_cast<FakeTransportChannel*>(
        transport_->CreateChannel(component));
  }
  void DestroyChannel() {
    transport_->DestroyChannel(1);
    channel_ = NULL;
  }

 protected:
  rtc::scoped_ptr<FakeTransport> transport_;
  FakeTransportChannel* channel_;
};

// This test verifies channels are created with proper ICE
// role, tiebreaker and remote ice mode and credentials after offer and
// answer negotiations.
TEST_F(TransportTest, TestChannelIceParameters) {
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  transport_->SetIceTiebreaker(99U);
  cricket::TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(local_desc,
                                                       cricket::CA_OFFER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());
  EXPECT_TRUE(SetupChannel());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
  EXPECT_EQ(cricket::ICEMODE_FULL, channel_->remote_ice_mode());
  EXPECT_EQ(kIceUfrag1, channel_->ice_ufrag());
  EXPECT_EQ(kIcePwd1, channel_->ice_pwd());

  cricket::TransportDescription remote_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(remote_desc,
                                                        cricket::CA_ANSWER,
                                                        NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
  EXPECT_EQ(99U, channel_->IceTiebreaker());
  EXPECT_EQ(cricket::ICEMODE_FULL, channel_->remote_ice_mode());
  // Changing the transport role from CONTROLLING to CONTROLLED.
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLED);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel_->GetIceRole());
  EXPECT_EQ(cricket::ICEMODE_FULL, channel_->remote_ice_mode());
  EXPECT_EQ(kIceUfrag1, channel_->remote_ice_ufrag());
  EXPECT_EQ(kIcePwd1, channel_->remote_ice_pwd());
}

// Verifies that IceCredentialsChanged returns true when either ufrag or pwd
// changed, and false in other cases.
TEST_F(TransportTest, TestIceCredentialsChanged) {
  EXPECT_TRUE(cricket::IceCredentialsChanged("u1", "p1", "u2", "p2"));
  EXPECT_TRUE(cricket::IceCredentialsChanged("u1", "p1", "u2", "p1"));
  EXPECT_TRUE(cricket::IceCredentialsChanged("u1", "p1", "u1", "p2"));
  EXPECT_FALSE(cricket::IceCredentialsChanged("u1", "p1", "u1", "p1"));
}

// This test verifies that the callee's ICE role changes from controlled to
// controlling when the callee triggers an ICE restart.
TEST_F(TransportTest, TestIceControlledToControllingOnIceRestart) {
  EXPECT_TRUE(SetupChannel());
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLED);

  cricket::TransportDescription desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(desc,
                                                        cricket::CA_OFFER,
                                                        NULL));
  ASSERT_TRUE(transport_->SetLocalTransportDescription(desc,
                                                       cricket::CA_ANSWER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, transport_->ice_role());

  cricket::TransportDescription new_local_desc(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(new_local_desc,
                                                       cricket::CA_OFFER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
}

// This test verifies that the caller's ICE role changes from controlling to
// controlled when the callee triggers an ICE restart.
TEST_F(TransportTest, TestIceControllingToControlledOnIceRestart) {
  EXPECT_TRUE(SetupChannel());
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLING);

  cricket::TransportDescription desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(desc,
                                                       cricket::CA_OFFER,
                                                       NULL));
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(desc,
                                                        cricket::CA_ANSWER,
                                                        NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());

  cricket::TransportDescription new_local_desc(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(new_local_desc,
                                                       cricket::CA_ANSWER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, transport_->ice_role());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel_->GetIceRole());
}

// This test verifies that the caller's ICE role is still controlling after the
// callee triggers ICE restart if the callee's ICE mode is LITE.
TEST_F(TransportTest, TestIceControllingOnIceRestartIfRemoteIsIceLite) {
  EXPECT_TRUE(SetupChannel());
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLING);

  cricket::TransportDescription desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(desc,
                                                       cricket::CA_OFFER,
                                                       NULL));

  cricket::TransportDescription remote_desc(
      std::vector<std::string>(),
      kIceUfrag1, kIcePwd1, cricket::ICEMODE_LITE,
      cricket::CONNECTIONROLE_NONE, NULL, cricket::Candidates());
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(remote_desc,
                                                        cricket::CA_ANSWER,
                                                        NULL));

  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());

  cricket::TransportDescription new_local_desc(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(new_local_desc,
                                                       cricket::CA_ANSWER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
}

// Tests channel role is reversed after receiving ice-lite from remote.
TEST_F(TransportTest, TestSetRemoteIceLiteInOffer) {
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLED);
  cricket::TransportDescription remote_desc(
      std::vector<std::string>(),
      kIceUfrag1, kIcePwd1, cricket::ICEMODE_LITE,
      cricket::CONNECTIONROLE_ACTPASS, NULL, cricket::Candidates());
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(remote_desc,
                                                        cricket::CA_OFFER,
                                                        NULL));
  cricket::TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(local_desc,
                                                       cricket::CA_ANSWER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());
  EXPECT_TRUE(SetupChannel());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
  EXPECT_EQ(cricket::ICEMODE_LITE, channel_->remote_ice_mode());
}

// Tests ice-lite in remote answer.
TEST_F(TransportTest, TestSetRemoteIceLiteInAnswer) {
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  cricket::TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(local_desc,
                                                       cricket::CA_OFFER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());
  EXPECT_TRUE(SetupChannel());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
  // Channels will be created in ICEFULL_MODE.
  EXPECT_EQ(cricket::ICEMODE_FULL, channel_->remote_ice_mode());
  cricket::TransportDescription remote_desc(
      std::vector<std::string>(),
      kIceUfrag1, kIcePwd1, cricket::ICEMODE_LITE,
      cricket::CONNECTIONROLE_NONE, NULL, cricket::Candidates());
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(remote_desc,
                                                        cricket::CA_ANSWER,
                                                        NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
  // After receiving remote description with ICEMODE_LITE, channel should
  // have mode set to ICEMODE_LITE.
  EXPECT_EQ(cricket::ICEMODE_LITE, channel_->remote_ice_mode());
}

TEST_F(TransportTest, TestGetStats) {
  EXPECT_TRUE(SetupChannel());
  cricket::TransportStats stats;
  EXPECT_TRUE(transport_->GetStats(&stats));
  // Note that this tests the behavior of a FakeTransportChannel.
  ASSERT_EQ(1U, stats.channel_stats.size());
  EXPECT_EQ(1, stats.channel_stats[0].component);
  transport_->ConnectChannels();
  EXPECT_TRUE(transport_->GetStats(&stats));
  ASSERT_EQ(1U, stats.channel_stats.size());
  EXPECT_EQ(1, stats.channel_stats[0].component);
}

