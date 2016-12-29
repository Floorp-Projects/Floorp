/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>

#include "webrtc/base/fakesslidentity.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/dtlstransportchannel.h"
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "webrtc/p2p/base/p2ptransportchannel.h"
#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/p2p/base/transportcontroller.h"
#include "webrtc/p2p/client/fakeportallocator.h"

static const int kTimeout = 100;
static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
static const char kIceUfrag2[] = "TESTICEUFRAG0002";
static const char kIcePwd2[] = "TESTICEPWD00000000000002";

using cricket::Candidate;
using cricket::Candidates;
using cricket::FakeTransportChannel;
using cricket::FakeTransportController;
using cricket::IceConnectionState;
using cricket::IceGatheringState;
using cricket::TransportChannel;
using cricket::TransportController;
using cricket::TransportDescription;
using cricket::TransportStats;

// Only subclassing from FakeTransportController because currently that's the
// only way to have a TransportController with fake TransportChannels.
//
// TODO(deadbeef): Change this once the Transport/TransportChannel class
// heirarchy is cleaned up, and we can pass a "TransportChannelFactory" or
// something similar into TransportController.
typedef FakeTransportController TransportControllerForTest;

class TransportControllerTest : public testing::Test,
                                public sigslot::has_slots<> {
 public:
  TransportControllerTest()
      : transport_controller_(new TransportControllerForTest()),
        signaling_thread_(rtc::Thread::Current()) {
    ConnectTransportControllerSignals();
  }

  void CreateTransportControllerWithWorkerThread() {
    if (!worker_thread_) {
      worker_thread_.reset(new rtc::Thread());
      worker_thread_->Start();
    }
    transport_controller_.reset(
        new TransportControllerForTest(worker_thread_.get()));
    ConnectTransportControllerSignals();
  }

  void ConnectTransportControllerSignals() {
    transport_controller_->SignalConnectionState.connect(
        this, &TransportControllerTest::OnConnectionState);
    transport_controller_->SignalReceiving.connect(
        this, &TransportControllerTest::OnReceiving);
    transport_controller_->SignalGatheringState.connect(
        this, &TransportControllerTest::OnGatheringState);
    transport_controller_->SignalCandidatesGathered.connect(
        this, &TransportControllerTest::OnCandidatesGathered);
  }

  FakeTransportChannel* CreateChannel(const std::string& content,
                                      int component) {
    TransportChannel* channel =
        transport_controller_->CreateTransportChannel_w(content, component);
    return static_cast<FakeTransportChannel*>(channel);
  }

  void DestroyChannel(const std::string& content, int component) {
    transport_controller_->DestroyTransportChannel_w(content, component);
  }

  Candidate CreateCandidate(int component) {
    Candidate c;
    c.set_address(rtc::SocketAddress("192.168.1.1", 8000));
    c.set_component(1);
    c.set_protocol(cricket::UDP_PROTOCOL_NAME);
    c.set_priority(1);
    return c;
  }

  // Used for thread hopping test.
  void CreateChannelsAndCompleteConnectionOnWorkerThread() {
    worker_thread_->Invoke<void>(rtc::Bind(
        &TransportControllerTest::CreateChannelsAndCompleteConnection_w, this));
  }

  void CreateChannelsAndCompleteConnection_w() {
    transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLING);
    FakeTransportChannel* channel1 = CreateChannel("audio", 1);
    ASSERT_NE(nullptr, channel1);
    FakeTransportChannel* channel2 = CreateChannel("video", 1);
    ASSERT_NE(nullptr, channel2);

    TransportDescription local_desc(
        std::vector<std::string>(), kIceUfrag1, kIcePwd1, cricket::ICEMODE_FULL,
        cricket::CONNECTIONROLE_ACTPASS, nullptr, Candidates());
    std::string err;
    transport_controller_->SetLocalTransportDescription(
        "audio", local_desc, cricket::CA_OFFER, &err);
    transport_controller_->SetLocalTransportDescription(
        "video", local_desc, cricket::CA_OFFER, &err);
    transport_controller_->MaybeStartGathering();
    channel1->SignalCandidateGathered(channel1, CreateCandidate(1));
    channel2->SignalCandidateGathered(channel2, CreateCandidate(1));
    channel1->SetCandidatesGatheringComplete();
    channel2->SetCandidatesGatheringComplete();
    channel1->SetConnectionCount(2);
    channel2->SetConnectionCount(2);
    channel1->SetReceiving(true);
    channel2->SetReceiving(true);
    channel1->SetWritable(true);
    channel2->SetWritable(true);
    channel1->SetConnectionCount(1);
    channel2->SetConnectionCount(1);
  }

  cricket::IceConfig CreateIceConfig(int receiving_timeout_ms,
                                     bool gather_continually) {
    cricket::IceConfig config;
    config.receiving_timeout_ms = receiving_timeout_ms;
    config.gather_continually = gather_continually;
    return config;
  }

 protected:
  void OnConnectionState(IceConnectionState state) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    connection_state_ = state;
    ++connection_state_signal_count_;
  }

  void OnReceiving(bool receiving) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    receiving_ = receiving;
    ++receiving_signal_count_;
  }

  void OnGatheringState(IceGatheringState state) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    gathering_state_ = state;
    ++gathering_state_signal_count_;
  }

  void OnCandidatesGathered(const std::string& transport_name,
                            const Candidates& candidates) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    candidates_[transport_name].insert(candidates_[transport_name].end(),
                                       candidates.begin(), candidates.end());
    ++candidates_signal_count_;
  }

  rtc::scoped_ptr<rtc::Thread> worker_thread_;  // Not used for most tests.
  rtc::scoped_ptr<TransportControllerForTest> transport_controller_;

  // Information received from signals from transport controller.
  IceConnectionState connection_state_ = cricket::kIceConnectionConnecting;
  bool receiving_ = false;
  IceGatheringState gathering_state_ = cricket::kIceGatheringNew;
  // transport_name => candidates
  std::map<std::string, Candidates> candidates_;
  // Counts of each signal emitted.
  int connection_state_signal_count_ = 0;
  int receiving_signal_count_ = 0;
  int gathering_state_signal_count_ = 0;
  int candidates_signal_count_ = 0;

  // Used to make sure signals only come on signaling thread.
  rtc::Thread* const signaling_thread_ = nullptr;
  bool signaled_on_non_signaling_thread_ = false;
};

TEST_F(TransportControllerTest, TestSetIceConfig) {
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);

  transport_controller_->SetIceConfig(CreateIceConfig(1000, true));
  EXPECT_EQ(1000, channel1->receiving_timeout());
  EXPECT_TRUE(channel1->gather_continually());

  // Test that value stored in controller is applied to new channels.
  FakeTransportChannel* channel2 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel2);
  EXPECT_EQ(1000, channel2->receiving_timeout());
  EXPECT_TRUE(channel2->gather_continually());
}

TEST_F(TransportControllerTest, TestSetSslMaxProtocolVersion) {
  EXPECT_TRUE(transport_controller_->SetSslMaxProtocolVersion(
      rtc::SSL_PROTOCOL_DTLS_12));
  FakeTransportChannel* channel = CreateChannel("audio", 1);

  ASSERT_NE(nullptr, channel);
  EXPECT_EQ(rtc::SSL_PROTOCOL_DTLS_12, channel->ssl_max_protocol_version());

  // Setting max version after transport is created should fail.
  EXPECT_FALSE(transport_controller_->SetSslMaxProtocolVersion(
      rtc::SSL_PROTOCOL_DTLS_10));
}

TEST_F(TransportControllerTest, TestSetIceRole) {
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);

  transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel1->GetIceRole());
  transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLED);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel1->GetIceRole());

  // Test that value stored in controller is applied to new channels.
  FakeTransportChannel* channel2 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel2);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel2->GetIceRole());
}

// Test that when one channel encounters a role conflict, the ICE role is
// swapped on every channel.
TEST_F(TransportControllerTest, TestIceRoleConflict) {
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);
  FakeTransportChannel* channel2 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel2);

  transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel1->GetIceRole());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel2->GetIceRole());

  channel1->SignalRoleConflict(channel1);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel1->GetIceRole());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel2->GetIceRole());
}

TEST_F(TransportControllerTest, TestGetSslRole) {
  FakeTransportChannel* channel = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel);
  ASSERT_TRUE(channel->SetSslRole(rtc::SSL_CLIENT));
  rtc::SSLRole role;
  EXPECT_FALSE(transport_controller_->GetSslRole("video", &role));
  EXPECT_TRUE(transport_controller_->GetSslRole("audio", &role));
  EXPECT_EQ(rtc::SSL_CLIENT, role);
}

TEST_F(TransportControllerTest, TestSetAndGetLocalCertificate) {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate1 =
      rtc::RTCCertificate::Create(rtc::scoped_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("session1", rtc::KT_DEFAULT)));
  rtc::scoped_refptr<rtc::RTCCertificate> certificate2 =
      rtc::RTCCertificate::Create(rtc::scoped_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("session2", rtc::KT_DEFAULT)));
  rtc::scoped_refptr<rtc::RTCCertificate> returned_certificate;

  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);

  EXPECT_TRUE(transport_controller_->SetLocalCertificate(certificate1));
  EXPECT_TRUE(transport_controller_->GetLocalCertificate(
      "audio", &returned_certificate));
  EXPECT_EQ(certificate1->identity()->certificate().ToPEMString(),
            returned_certificate->identity()->certificate().ToPEMString());

  // Should fail if called for a nonexistant transport.
  EXPECT_FALSE(transport_controller_->GetLocalCertificate(
      "video", &returned_certificate));

  // Test that identity stored in controller is applied to new channels.
  FakeTransportChannel* channel2 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel2);
  EXPECT_TRUE(transport_controller_->GetLocalCertificate(
      "video", &returned_certificate));
  EXPECT_EQ(certificate1->identity()->certificate().ToPEMString(),
            returned_certificate->identity()->certificate().ToPEMString());

  // Shouldn't be able to change the identity once set.
  EXPECT_FALSE(transport_controller_->SetLocalCertificate(certificate2));
}

TEST_F(TransportControllerTest, TestGetRemoteSSLCertificate) {
  rtc::FakeSSLCertificate fake_certificate("fake_data");
  rtc::scoped_ptr<rtc::SSLCertificate> returned_certificate;

  FakeTransportChannel* channel = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel);

  channel->SetRemoteSSLCertificate(&fake_certificate);
  EXPECT_TRUE(transport_controller_->GetRemoteSSLCertificate(
      "audio", returned_certificate.accept()));
  EXPECT_EQ(fake_certificate.ToPEMString(),
            returned_certificate->ToPEMString());

  // Should fail if called for a nonexistant transport.
  EXPECT_FALSE(transport_controller_->GetRemoteSSLCertificate(
      "video", returned_certificate.accept()));
}

TEST_F(TransportControllerTest, TestSetLocalTransportDescription) {
  FakeTransportChannel* channel = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel);
  TransportDescription local_desc(
      std::vector<std::string>(), kIceUfrag1, kIcePwd1, cricket::ICEMODE_FULL,
      cricket::CONNECTIONROLE_ACTPASS, nullptr, Candidates());
  std::string err;
  EXPECT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, cricket::CA_OFFER, &err));
  // Check that ICE ufrag and pwd were propagated to channel.
  EXPECT_EQ(kIceUfrag1, channel->ice_ufrag());
  EXPECT_EQ(kIcePwd1, channel->ice_pwd());
  // After setting local description, we should be able to start gathering
  // candidates.
  transport_controller_->MaybeStartGathering();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSetRemoteTransportDescription) {
  FakeTransportChannel* channel = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel);
  TransportDescription remote_desc(
      std::vector<std::string>(), kIceUfrag1, kIcePwd1, cricket::ICEMODE_FULL,
      cricket::CONNECTIONROLE_ACTPASS, nullptr, Candidates());
  std::string err;
  EXPECT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, cricket::CA_OFFER, &err));
  // Check that ICE ufrag and pwd were propagated to channel.
  EXPECT_EQ(kIceUfrag1, channel->remote_ice_ufrag());
  EXPECT_EQ(kIcePwd1, channel->remote_ice_pwd());
}

TEST_F(TransportControllerTest, TestAddRemoteCandidates) {
  FakeTransportChannel* channel = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel);
  Candidates candidates;
  candidates.push_back(CreateCandidate(1));
  std::string err;
  EXPECT_TRUE(
      transport_controller_->AddRemoteCandidates("audio", candidates, &err));
  EXPECT_EQ(1U, channel->remote_candidates().size());
}

TEST_F(TransportControllerTest, TestReadyForRemoteCandidates) {
  FakeTransportChannel* channel = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel);
  // We expect to be ready for remote candidates only after local and remote
  // descriptions are set.
  EXPECT_FALSE(transport_controller_->ReadyForRemoteCandidates("audio"));

  std::string err;
  TransportDescription remote_desc(
      std::vector<std::string>(), kIceUfrag1, kIcePwd1, cricket::ICEMODE_FULL,
      cricket::CONNECTIONROLE_ACTPASS, nullptr, Candidates());
  EXPECT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, cricket::CA_OFFER, &err));
  EXPECT_FALSE(transport_controller_->ReadyForRemoteCandidates("audio"));

  TransportDescription local_desc(
      std::vector<std::string>(), kIceUfrag2, kIcePwd2, cricket::ICEMODE_FULL,
      cricket::CONNECTIONROLE_ACTPASS, nullptr, Candidates());
  EXPECT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, cricket::CA_ANSWER, &err));
  EXPECT_TRUE(transport_controller_->ReadyForRemoteCandidates("audio"));
}

TEST_F(TransportControllerTest, TestGetStats) {
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);
  FakeTransportChannel* channel2 = CreateChannel("audio", 2);
  ASSERT_NE(nullptr, channel2);
  FakeTransportChannel* channel3 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel3);

  TransportStats stats;
  EXPECT_TRUE(transport_controller_->GetStats("audio", &stats));
  EXPECT_EQ("audio", stats.transport_name);
  EXPECT_EQ(2U, stats.channel_stats.size());
}

// Test that transport gets destroyed when it has no more channels.
TEST_F(TransportControllerTest, TestCreateAndDestroyChannel) {
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);
  FakeTransportChannel* channel2 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel2);
  ASSERT_EQ(channel1, channel2);
  FakeTransportChannel* channel3 = CreateChannel("audio", 2);
  ASSERT_NE(nullptr, channel3);

  // Using GetStats to check if transport is destroyed from an outside class's
  // perspective.
  TransportStats stats;
  EXPECT_TRUE(transport_controller_->GetStats("audio", &stats));
  DestroyChannel("audio", 2);
  DestroyChannel("audio", 1);
  EXPECT_TRUE(transport_controller_->GetStats("audio", &stats));
  DestroyChannel("audio", 1);
  EXPECT_FALSE(transport_controller_->GetStats("audio", &stats));
}

TEST_F(TransportControllerTest, TestSignalConnectionStateFailed) {
  // Need controlling ICE role to get in failed state.
  transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);
  FakeTransportChannel* channel2 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel2);

  // Should signal "failed" if any channel failed; channel is considered failed
  // if it previously had a connection but now has none, and gathering is
  // complete.
  channel1->SetCandidatesGatheringComplete();
  channel1->SetConnectionCount(1);
  channel1->SetConnectionCount(0);
  EXPECT_EQ_WAIT(cricket::kIceConnectionFailed, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalConnectionStateConnected) {
  transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);
  FakeTransportChannel* channel2 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel2);
  FakeTransportChannel* channel3 = CreateChannel("video", 2);
  ASSERT_NE(nullptr, channel3);

  // First, have one channel connect, and another fail, to ensure that
  // the first channel connecting didn't trigger a "connected" state signal.
  // We should only get a signal when all are connected.
  channel1->SetConnectionCount(2);
  channel1->SetWritable(true);
  channel3->SetCandidatesGatheringComplete();
  channel3->SetConnectionCount(1);
  channel3->SetConnectionCount(0);
  EXPECT_EQ_WAIT(cricket::kIceConnectionFailed, connection_state_, kTimeout);
  // Signal count of 1 means that the only signal emitted was "failed".
  EXPECT_EQ(1, connection_state_signal_count_);

  // Destroy the failed channel to return to "connecting" state.
  DestroyChannel("video", 2);
  EXPECT_EQ_WAIT(cricket::kIceConnectionConnecting, connection_state_,
                 kTimeout);
  EXPECT_EQ(2, connection_state_signal_count_);

  // Make the remaining channel reach a connected state.
  channel2->SetConnectionCount(2);
  channel2->SetWritable(true);
  EXPECT_EQ_WAIT(cricket::kIceConnectionConnected, connection_state_, kTimeout);
  EXPECT_EQ(3, connection_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalConnectionStateComplete) {
  transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);
  FakeTransportChannel* channel2 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel2);
  FakeTransportChannel* channel3 = CreateChannel("video", 2);
  ASSERT_NE(nullptr, channel3);

  // Similar to above test, but we're now reaching the completed state, which
  // means only one connection per FakeTransportChannel.
  channel1->SetCandidatesGatheringComplete();
  channel1->SetConnectionCount(1);
  channel1->SetWritable(true);
  channel3->SetCandidatesGatheringComplete();
  channel3->SetConnectionCount(1);
  channel3->SetConnectionCount(0);
  EXPECT_EQ_WAIT(cricket::kIceConnectionFailed, connection_state_, kTimeout);
  // Signal count of 1 means that the only signal emitted was "failed".
  EXPECT_EQ(1, connection_state_signal_count_);

  // Destroy the failed channel to return to "connecting" state.
  DestroyChannel("video", 2);
  EXPECT_EQ_WAIT(cricket::kIceConnectionConnecting, connection_state_,
                 kTimeout);
  EXPECT_EQ(2, connection_state_signal_count_);

  // Make the remaining channel reach a connected state.
  channel2->SetCandidatesGatheringComplete();
  channel2->SetConnectionCount(2);
  channel2->SetWritable(true);
  EXPECT_EQ_WAIT(cricket::kIceConnectionConnected, connection_state_, kTimeout);
  EXPECT_EQ(3, connection_state_signal_count_);

  // Finally, transition to completed state.
  channel2->SetConnectionCount(1);
  EXPECT_EQ_WAIT(cricket::kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(4, connection_state_signal_count_);
}

// Make sure that if we're "connected" and remove a transport, we stay in the
// "connected" state.
TEST_F(TransportControllerTest, TestDestroyTransportAndStayConnected) {
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);
  FakeTransportChannel* channel2 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel2);

  channel1->SetCandidatesGatheringComplete();
  channel1->SetConnectionCount(2);
  channel1->SetWritable(true);
  channel2->SetCandidatesGatheringComplete();
  channel2->SetConnectionCount(2);
  channel2->SetWritable(true);
  EXPECT_EQ_WAIT(cricket::kIceConnectionConnected, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);

  // Destroy one channel, then "complete" the other one, so we reach
  // a known state.
  DestroyChannel("video", 1);
  channel1->SetConnectionCount(1);
  EXPECT_EQ_WAIT(cricket::kIceConnectionCompleted, connection_state_, kTimeout);
  // Signal count of 2 means the deletion didn't cause any unexpected signals
  EXPECT_EQ(2, connection_state_signal_count_);
}

// If we destroy the last/only transport, we should simply transition to
// "connecting".
TEST_F(TransportControllerTest, TestDestroyLastTransportWhileConnected) {
  FakeTransportChannel* channel = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel);

  channel->SetCandidatesGatheringComplete();
  channel->SetConnectionCount(2);
  channel->SetWritable(true);
  EXPECT_EQ_WAIT(cricket::kIceConnectionConnected, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);

  DestroyChannel("audio", 1);
  EXPECT_EQ_WAIT(cricket::kIceConnectionConnecting, connection_state_,
                 kTimeout);
  // Signal count of 2 means the deletion didn't cause any unexpected signals
  EXPECT_EQ(2, connection_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalReceiving) {
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);
  FakeTransportChannel* channel2 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel2);

  // Should signal receiving as soon as any channel is receiving.
  channel1->SetReceiving(true);
  EXPECT_TRUE_WAIT(receiving_, kTimeout);
  EXPECT_EQ(1, receiving_signal_count_);

  channel2->SetReceiving(true);
  channel1->SetReceiving(false);
  channel2->SetReceiving(false);
  EXPECT_TRUE_WAIT(!receiving_, kTimeout);
  EXPECT_EQ(2, receiving_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalGatheringStateGathering) {
  FakeTransportChannel* channel = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel);
  channel->Connect();
  channel->MaybeStartGathering();
  // Should be in the gathering state as soon as any transport starts gathering.
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalGatheringStateComplete) {
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);
  FakeTransportChannel* channel2 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel2);
  FakeTransportChannel* channel3 = CreateChannel("data", 1);
  ASSERT_NE(nullptr, channel3);

  channel3->Connect();
  channel3->MaybeStartGathering();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);

  // Have one channel finish gathering, then destroy it, to make sure gathering
  // completion wasn't signalled if only one transport finished gathering.
  channel3->SetCandidatesGatheringComplete();
  DestroyChannel("data", 1);
  EXPECT_EQ_WAIT(cricket::kIceGatheringNew, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);

  // Make remaining channels start and then finish gathering.
  channel1->Connect();
  channel1->MaybeStartGathering();
  channel2->Connect();
  channel2->MaybeStartGathering();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(3, gathering_state_signal_count_);

  channel1->SetCandidatesGatheringComplete();
  channel2->SetCandidatesGatheringComplete();
  EXPECT_EQ_WAIT(cricket::kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(4, gathering_state_signal_count_);
}

// Test that when the last transport that hasn't finished connecting and/or
// gathering is destroyed, the aggregate state jumps to "completed". This can
// happen if, for example, we have an audio and video transport, the audio
// transport completes, then we start bundling video on the audio transport.
TEST_F(TransportControllerTest,
       TestSignalingWhenLastIncompleteTransportDestroyed) {
  transport_controller_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  FakeTransportChannel* channel1 = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel1);
  FakeTransportChannel* channel2 = CreateChannel("video", 1);
  ASSERT_NE(nullptr, channel2);

  channel1->SetCandidatesGatheringComplete();
  EXPECT_EQ_WAIT(cricket::kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);

  channel1->SetConnectionCount(1);
  channel1->SetWritable(true);
  DestroyChannel("video", 1);
  EXPECT_EQ_WAIT(cricket::kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);
  EXPECT_EQ_WAIT(cricket::kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalCandidatesGathered) {
  FakeTransportChannel* channel = CreateChannel("audio", 1);
  ASSERT_NE(nullptr, channel);

  // Transport won't signal candidates until it has a local description.
  TransportDescription local_desc(
      std::vector<std::string>(), kIceUfrag1, kIcePwd1, cricket::ICEMODE_FULL,
      cricket::CONNECTIONROLE_ACTPASS, nullptr, Candidates());
  std::string err;
  EXPECT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, cricket::CA_OFFER, &err));
  transport_controller_->MaybeStartGathering();

  channel->SignalCandidateGathered(channel, CreateCandidate(1));
  EXPECT_EQ_WAIT(1, candidates_signal_count_, kTimeout);
  EXPECT_EQ(1U, candidates_["audio"].size());
}

TEST_F(TransportControllerTest, TestSignalingOccursOnSignalingThread) {
  CreateTransportControllerWithWorkerThread();
  CreateChannelsAndCompleteConnectionOnWorkerThread();

  // connecting --> connected --> completed
  EXPECT_EQ_WAIT(cricket::kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(2, connection_state_signal_count_);

  EXPECT_TRUE_WAIT(receiving_, kTimeout);
  EXPECT_EQ(1, receiving_signal_count_);

  // new --> gathering --> complete
  EXPECT_EQ_WAIT(cricket::kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);

  EXPECT_EQ_WAIT(1U, candidates_["audio"].size(), kTimeout);
  EXPECT_EQ_WAIT(1U, candidates_["video"].size(), kTimeout);
  EXPECT_EQ(2, candidates_signal_count_);

  EXPECT_TRUE(!signaled_on_non_signaling_thread_);
}
