/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/data_channel_interface.h"
#include "api/dtmf_sender_interface.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "pc/test/integration_test_helpers.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/virtual_socket_server.h"

namespace webrtc {

namespace {

class DataChannelIntegrationTest
    : public PeerConnectionIntegrationBaseTest,
      public ::testing::WithParamInterface<SdpSemantics> {
 protected:
  DataChannelIntegrationTest()
      : PeerConnectionIntegrationBaseTest(GetParam()) {}
};

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DataChannelIntegrationTest);

// Fake clock must be set before threads are started to prevent race on
// Set/GetClockForTesting().
// To achieve that, multiple inheritance is used as a mixin pattern
// where order of construction is finely controlled.
// This also ensures peerconnection is closed before switching back to non-fake
// clock, avoiding other races and DCHECK failures such as in rtp_sender.cc.
class FakeClockForTest : public rtc::ScopedFakeClock {
 protected:
  FakeClockForTest() {
    // Some things use a time of "0" as a special value, so we need to start out
    // the fake clock at a nonzero time.
    // TODO(deadbeef): Fix this.
    AdvanceTime(webrtc::TimeDelta::Seconds(1));
  }

  // Explicit handle.
  ScopedFakeClock& FakeClock() { return *this; }
};

// Ensure FakeClockForTest is constructed first (see class for rationale).
class DataChannelIntegrationTestWithFakeClock
    : public FakeClockForTest,
      public DataChannelIntegrationTest {};

class DataChannelIntegrationTestPlanB
    : public PeerConnectionIntegrationBaseTest {
 protected:
  DataChannelIntegrationTestPlanB()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kPlanB) {}
};

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    DataChannelIntegrationTestWithFakeClock);

class DataChannelIntegrationTestUnifiedPlan
    : public PeerConnectionIntegrationBaseTest {
 protected:
  DataChannelIntegrationTestUnifiedPlan()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan) {}
};

#ifdef WEBRTC_HAVE_SCTP

// This test causes a PeerConnection to enter Disconnected state, and
// sends data on a DataChannel while disconnected.
// The data should be surfaced when the connection reestablishes.
TEST_P(DataChannelIntegrationTest, DataChannelWhileDisconnected) {
  CreatePeerConnectionWrappers();
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer(), kDefaultTimeout);
  std::string data1 = "hello first";
  caller()->data_channel()->Send(DataBuffer(data1));
  EXPECT_EQ_WAIT(data1, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  // Cause a network outage
  virtual_socket_server()->set_drop_probability(1.0);
  EXPECT_EQ_WAIT(PeerConnectionInterface::kIceConnectionDisconnected,
                 caller()->standardized_ice_connection_state(),
                 kDefaultTimeout);
  std::string data2 = "hello second";
  caller()->data_channel()->Send(DataBuffer(data2));
  // Remove the network outage. The connection should reestablish.
  virtual_socket_server()->set_drop_probability(0.0);
  EXPECT_EQ_WAIT(data2, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
}

// This test causes a PeerConnection to enter Disconnected state,
// sends data on a DataChannel while disconnected, and then triggers
// an ICE restart.
// The data should be surfaced when the connection reestablishes.
TEST_P(DataChannelIntegrationTest, DataChannelWhileDisconnectedIceRestart) {
  CreatePeerConnectionWrappers();
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer(), kDefaultTimeout);
  std::string data1 = "hello first";
  caller()->data_channel()->Send(DataBuffer(data1));
  EXPECT_EQ_WAIT(data1, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  // Cause a network outage
  virtual_socket_server()->set_drop_probability(1.0);
  ASSERT_EQ_WAIT(PeerConnectionInterface::kIceConnectionDisconnected,
                 caller()->standardized_ice_connection_state(),
                 kDefaultTimeout);
  std::string data2 = "hello second";
  caller()->data_channel()->Send(DataBuffer(data2));

  // Trigger an ICE restart. The signaling channel is not affected by
  // the network outage.
  caller()->SetOfferAnswerOptions(IceRestartOfferAnswerOptions());
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Remove the network outage. The connection should reestablish.
  virtual_socket_server()->set_drop_probability(0.0);
  EXPECT_EQ_WAIT(data2, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
}

#endif  // WEBRTC_HAVE_SCTP

// This test sets up a call between two parties with audio, video and an RTP
// data channel.
TEST_P(DataChannelIntegrationTest, EndToEndCallWithRtpDataChannel) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.enable_rtp_data_channel = true;
  rtc_config.enable_dtls_srtp = false;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(rtc_config, rtc_config));
  ConnectFakeSignaling();
  // Expect that data channel created on caller side will show up for callee as
  // well.
  caller()->CreateDataChannel();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Ensure the existence of the RTP data channel didn't impede audio/video.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_NE(nullptr, callee()->data_channel());
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  SendRtpDataWithRetries(caller()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  SendRtpDataWithRetries(callee()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

TEST_P(DataChannelIntegrationTest, RtpDataChannelWorksAfterRollback) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.enable_rtp_data_channel = true;
  rtc_config.enable_dtls_srtp = false;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(rtc_config, rtc_config));
  ConnectFakeSignaling();
  auto data_channel = caller()->pc()->CreateDataChannel("label_1", nullptr);
  ASSERT_TRUE(data_channel.get() != nullptr);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);

  caller()->CreateDataChannel("label_2", nullptr);
  rtc::scoped_refptr<MockSetSessionDescriptionObserver> observer(
      new rtc::RefCountedObject<MockSetSessionDescriptionObserver>());
  caller()->pc()->SetLocalDescription(observer,
                                      caller()->CreateOfferAndWait().release());
  EXPECT_TRUE_WAIT(observer->called(), kDefaultTimeout);
  caller()->Rollback();

  std::string data = "hello world";
  SendRtpDataWithRetries(data_channel, data, 5);
  EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
}

// Ensure that an RTP data channel is signaled as closed for the caller when
// the callee rejects it in a subsequent offer.
TEST_P(DataChannelIntegrationTest, RtpDataChannelSignaledClosedInCalleeOffer) {
  // Same procedure as above test.
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.enable_rtp_data_channel = true;
  rtc_config.enable_dtls_srtp = false;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(rtc_config, rtc_config));
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_NE(nullptr, callee()->data_channel());
  ASSERT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Close the data channel on the callee, and do an updated offer/answer.
  callee()->data_channel()->Close();
  callee()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  EXPECT_FALSE(caller()->data_observer()->IsOpen());
  EXPECT_FALSE(callee()->data_observer()->IsOpen());
}

#if !defined(THREAD_SANITIZER)
// This test provokes TSAN errors. See bugs.webrtc.org/11282

// Tests that data is buffered in an RTP data channel until an observer is
// registered for it.
//
// NOTE: RTP data channels can receive data before the underlying
// transport has detected that a channel is writable and thus data can be
// received before the data channel state changes to open. That is hard to test
// but the same buffering is expected to be used in that case.
//
// Use fake clock and simulated network delay so that we predictably can wait
// until an SCTP message has been delivered without "sleep()"ing.
TEST_P(DataChannelIntegrationTestWithFakeClock,
       DataBufferedUntilRtpDataChannelObserverRegistered) {
  virtual_socket_server()->set_delay_mean(5);  // 5 ms per hop.
  virtual_socket_server()->UpdateDelayDistribution();

  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.enable_rtp_data_channel = true;
  rtc_config.enable_dtls_srtp = false;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(rtc_config, rtc_config));
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE(caller()->data_channel() != nullptr);
  ASSERT_TRUE_SIMULATED_WAIT(callee()->data_channel() != nullptr,
                             kDefaultTimeout, FakeClock());
  ASSERT_TRUE_SIMULATED_WAIT(caller()->data_observer()->IsOpen(),
                             kDefaultTimeout, FakeClock());
  ASSERT_EQ_SIMULATED_WAIT(DataChannelInterface::kOpen,
                           callee()->data_channel()->state(), kDefaultTimeout,
                           FakeClock());

  // Unregister the observer which is normally automatically registered.
  callee()->data_channel()->UnregisterObserver();
  // Send data and advance fake clock until it should have been received.
  std::string data = "hello world";
  caller()->data_channel()->Send(DataBuffer(data));
  SIMULATED_WAIT(false, 50, FakeClock());

  // Attach data channel and expect data to be received immediately. Note that
  // EXPECT_EQ_WAIT is used, such that the simulated clock is not advanced any
  // further, but data can be received even if the callback is asynchronous.
  MockDataChannelObserver new_observer(callee()->data_channel());
  EXPECT_EQ_SIMULATED_WAIT(data, new_observer.last_message(), kDefaultTimeout,
                           FakeClock());
}

#endif  // !defined(THREAD_SANITIZER)

// This test sets up a call between two parties with audio, video and but only
// the caller client supports RTP data channels.
TEST_P(DataChannelIntegrationTest, RtpDataChannelsRejectedByCallee) {
  PeerConnectionInterface::RTCConfiguration rtc_config_1;
  rtc_config_1.enable_rtp_data_channel = true;
  // Must disable DTLS to make negotiation succeed.
  rtc_config_1.enable_dtls_srtp = false;
  PeerConnectionInterface::RTCConfiguration rtc_config_2;
  rtc_config_2.enable_dtls_srtp = false;
  rtc_config_2.enable_dtls_srtp = false;
  ASSERT_TRUE(
      CreatePeerConnectionWrappersWithConfig(rtc_config_1, rtc_config_2));
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  ASSERT_TRUE(caller()->data_channel() != nullptr);
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // The caller should still have a data channel, but it should be closed, and
  // one should ever have been created for the callee.
  EXPECT_TRUE(caller()->data_channel() != nullptr);
  EXPECT_FALSE(caller()->data_observer()->IsOpen());
  EXPECT_EQ(nullptr, callee()->data_channel());
}

// This test sets up a call between two parties with audio, and video. When
// audio and video is setup and flowing, an RTP data channel is negotiated.
TEST_P(DataChannelIntegrationTest, AddRtpDataChannelInSubsequentOffer) {
  PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.enable_rtp_data_channel = true;
  rtc_config.enable_dtls_srtp = false;
  ASSERT_TRUE(CreatePeerConnectionWrappersWithConfig(rtc_config, rtc_config));
  ConnectFakeSignaling();
  // Do initial offer/answer with audio/video.
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Create data channel and do new offer and answer.
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_NE(nullptr, callee()->data_channel());
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);
  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  SendRtpDataWithRetries(caller()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  SendRtpDataWithRetries(callee()->data_channel(), data, 5);
  EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

#ifdef WEBRTC_HAVE_SCTP

// This test sets up a call between two parties with audio, video and an SCTP
// data channel.
TEST_P(DataChannelIntegrationTest, EndToEndCallWithSctpDataChannel) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Expect that data channel created on caller side will show up for callee as
  // well.
  caller()->CreateDataChannel();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Ensure the existence of the SCTP data channel didn't impede audio/video.
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  // Caller data channel should already exist (it created one). Callee data
  // channel may not exist yet, since negotiation happens in-band, not in SDP.
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  caller()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  callee()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

// This test sets up a call between two parties with an SCTP
// data channel only, and sends messages of various sizes.
TEST_P(DataChannelIntegrationTest,
       EndToEndCallWithSctpDataChannelVariousSizes) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Expect that data channel created on caller side will show up for callee as
  // well.
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Caller data channel should already exist (it created one). Callee data
  // channel may not exist yet, since negotiation happens in-band, not in SDP.
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  for (int message_size = 1; message_size < 100000; message_size *= 2) {
    std::string data(message_size, 'a');
    caller()->data_channel()->Send(DataBuffer(data));
    EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                   kDefaultTimeout);
    callee()->data_channel()->Send(DataBuffer(data));
    EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                   kDefaultTimeout);
  }
  // Specifically probe the area around the MTU size.
  for (int message_size = 1100; message_size < 1300; message_size += 1) {
    std::string data(message_size, 'a');
    caller()->data_channel()->Send(DataBuffer(data));
    EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                   kDefaultTimeout);
    callee()->data_channel()->Send(DataBuffer(data));
    EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                   kDefaultTimeout);
  }
}

TEST_P(DataChannelIntegrationTest,
       EndToEndCallWithSctpDataChannelLowestSafeMtu) {
  // The lowest payload size limit that's tested and found safe for this
  // application. Note that this is not the safe limit under all conditions;
  // in particular, the default is not the largest DTLS signature, and
  // this test does not use TURN.
  const size_t kLowestSafePayloadSizeLimit = 1225;

  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Expect that data channel created on caller side will show up for callee as
  // well.
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Caller data channel should already exist (it created one). Callee data
  // channel may not exist yet, since negotiation happens in-band, not in SDP.
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  virtual_socket_server()->set_max_udp_payload(kLowestSafePayloadSizeLimit);
  for (int message_size = 1140; message_size < 1240; message_size += 1) {
    std::string data(message_size, 'a');
    caller()->data_channel()->Send(DataBuffer(data));
    ASSERT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                   kDefaultTimeout);
    callee()->data_channel()->Send(DataBuffer(data));
    ASSERT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                   kDefaultTimeout);
  }
}

// This test verifies that lowering the MTU of the connection will cause
// the datachannel to not transmit reliably.
// The purpose of this test is to ensure that we know how a too-small MTU
// error manifests itself.
TEST_P(DataChannelIntegrationTest, EndToEndCallWithSctpDataChannelHarmfulMtu) {
  // The lowest payload size limit that's tested and found safe for this
  // application in this configuration (see test above).
  const size_t kLowestSafePayloadSizeLimit = 1225;
  // The size of the smallest message that fails to be delivered.
  const size_t kMessageSizeThatIsNotDelivered = 1157;

  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  virtual_socket_server()->set_max_udp_payload(kLowestSafePayloadSizeLimit - 1);
  // Probe for an undelivered or slowly delivered message. The exact
  // size limit seems to be dependent on the message history, so make the
  // code easily able to find the current value.
  bool failure_seen = false;
  for (size_t message_size = 1110; message_size < 1400; message_size++) {
    const size_t message_count =
        callee()->data_observer()->received_message_count();
    const std::string data(message_size, 'a');
    caller()->data_channel()->Send(DataBuffer(data));
    // Wait a very short time for the message to be delivered.
    // Note: Waiting only 10 ms is too short for Windows bots; they will
    // flakily fail at a random frame.
    WAIT(callee()->data_observer()->received_message_count() > message_count,
         100);
    if (callee()->data_observer()->received_message_count() == message_count) {
      ASSERT_EQ(kMessageSizeThatIsNotDelivered, message_size);
      failure_seen = true;
      break;
    }
  }
  ASSERT_TRUE(failure_seen);
}

// Ensure that when the callee closes an SCTP data channel, the closing
// procedure results in the data channel being closed for the caller as well.
TEST_P(DataChannelIntegrationTest, CalleeClosesSctpDataChannel) {
  // Same procedure as above test.
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  ASSERT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Close the data channel on the callee side, and wait for it to reach the
  // "closed" state on both sides.
  callee()->data_channel()->Close();
  EXPECT_TRUE_WAIT(!caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(!callee()->data_observer()->IsOpen(), kDefaultTimeout);
}

TEST_P(DataChannelIntegrationTest, SctpDataChannelConfigSentToOtherSide) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  webrtc::DataChannelInit init;
  init.id = 53;
  init.maxRetransmits = 52;
  caller()->CreateDataChannel("data-channel", &init);
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);
  // Since "negotiated" is false, the "id" parameter should be ignored.
  EXPECT_NE(init.id, callee()->data_channel()->id());
  EXPECT_EQ("data-channel", callee()->data_channel()->label());
  EXPECT_EQ(init.maxRetransmits, callee()->data_channel()->maxRetransmits());
  EXPECT_FALSE(callee()->data_channel()->negotiated());
}

// Test usrsctp's ability to process unordered data stream, where data actually
// arrives out of order using simulated delays. Previously there have been some
// bugs in this area.
TEST_P(DataChannelIntegrationTest, StressTestUnorderedSctpDataChannel) {
  // Introduce random network delays.
  // Otherwise it's not a true "unordered" test.
  virtual_socket_server()->set_delay_mean(20);
  virtual_socket_server()->set_delay_stddev(5);
  virtual_socket_server()->UpdateDelayDistribution();
  // Normal procedure, but with unordered data channel config.
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  webrtc::DataChannelInit init;
  init.ordered = false;
  caller()->CreateDataChannel(&init);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  ASSERT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  static constexpr int kNumMessages = 100;
  // Deliberately chosen to be larger than the MTU so messages get fragmented.
  static constexpr size_t kMaxMessageSize = 4096;
  // Create and send random messages.
  std::vector<std::string> sent_messages;
  for (int i = 0; i < kNumMessages; ++i) {
    size_t length =
        (rand() % kMaxMessageSize) + 1;  // NOLINT (rand_r instead of rand)
    std::string message;
    ASSERT_TRUE(rtc::CreateRandomString(length, &message));
    caller()->data_channel()->Send(DataBuffer(message));
    callee()->data_channel()->Send(DataBuffer(message));
    sent_messages.push_back(message);
  }

  // Wait for all messages to be received.
  EXPECT_EQ_WAIT(rtc::checked_cast<size_t>(kNumMessages),
                 caller()->data_observer()->received_message_count(),
                 kDefaultTimeout);
  EXPECT_EQ_WAIT(rtc::checked_cast<size_t>(kNumMessages),
                 callee()->data_observer()->received_message_count(),
                 kDefaultTimeout);

  // Sort and compare to make sure none of the messages were corrupted.
  std::vector<std::string> caller_received_messages =
      caller()->data_observer()->messages();
  std::vector<std::string> callee_received_messages =
      callee()->data_observer()->messages();
  absl::c_sort(sent_messages);
  absl::c_sort(caller_received_messages);
  absl::c_sort(callee_received_messages);
  EXPECT_EQ(sent_messages, caller_received_messages);
  EXPECT_EQ(sent_messages, callee_received_messages);
}

// This test sets up a call between two parties with audio, and video. When
// audio and video are setup and flowing, an SCTP data channel is negotiated.
TEST_P(DataChannelIntegrationTest, AddSctpDataChannelInSubsequentOffer) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Do initial offer/answer with audio/video.
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Create data channel and do new offer and answer.
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Caller data channel should already exist (it created one). Callee data
  // channel may not exist yet, since negotiation happens in-band, not in SDP.
  ASSERT_NE(nullptr, caller()->data_channel());
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);
  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  caller()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  callee()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

// Set up a connection initially just using SCTP data channels, later upgrading
// to audio/video, ensuring frames are received end-to-end. Effectively the
// inverse of the test above.
// This was broken in M57; see https://crbug.com/711243
TEST_P(DataChannelIntegrationTest, SctpDataChannelToAudioVideoUpgrade) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  // Do initial offer/answer with just data channel.
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  // Wait until data can be sent over the data channel.
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  ASSERT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Do subsequent offer/answer with two-way audio and video. Audio and video
  // should end up bundled on the DTLS/ICE transport already used for data.
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.ExpectBidirectionalAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
}

static void MakeSpecCompliantSctpOffer(cricket::SessionDescription* desc) {
  cricket::SctpDataContentDescription* dcd_offer =
      GetFirstSctpDataContentDescription(desc);
  // See https://crbug.com/webrtc/11211 - this function is a no-op
  ASSERT_TRUE(dcd_offer);
  dcd_offer->set_use_sctpmap(false);
  dcd_offer->set_protocol("UDP/DTLS/SCTP");
}

// Test that the data channel works when a spec-compliant SCTP m= section is
// offered (using "a=sctp-port" instead of "a=sctpmap", and using
// "UDP/DTLS/SCTP" as the protocol).
TEST_P(DataChannelIntegrationTest,
       DataChannelWorksWhenSpecCompliantSctpOfferReceived) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->SetGeneratedSdpMunger(MakeSpecCompliantSctpOffer);
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_channel() != nullptr, kDefaultTimeout);
  EXPECT_TRUE_WAIT(caller()->data_observer()->IsOpen(), kDefaultTimeout);
  EXPECT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);

  // Ensure data can be sent in both directions.
  std::string data = "hello world";
  caller()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, callee()->data_observer()->last_message(),
                 kDefaultTimeout);
  callee()->data_channel()->Send(DataBuffer(data));
  EXPECT_EQ_WAIT(data, caller()->data_observer()->last_message(),
                 kDefaultTimeout);
}

#endif  // WEBRTC_HAVE_SCTP

// Test that after closing PeerConnections, they stop sending any packets (ICE,
// DTLS, RTP...).
TEST_P(DataChannelIntegrationTest, ClosingConnectionStopsPacketFlow) {
  // Set up audio/video/data, wait for some frames to be received.
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->AddAudioVideoTracks();
#ifdef WEBRTC_HAVE_SCTP
  caller()->CreateDataChannel();
#endif
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  MediaExpectations media_expectations;
  media_expectations.CalleeExpectsSomeAudioAndVideo();
  ASSERT_TRUE(ExpectNewFrames(media_expectations));
  // Close PeerConnections.
  ClosePeerConnections();
  // Pump messages for a second, and ensure no new packets end up sent.
  uint32_t sent_packets_a = virtual_socket_server()->sent_packets();
  WAIT(false, 1000);
  uint32_t sent_packets_b = virtual_socket_server()->sent_packets();
  EXPECT_EQ(sent_packets_a, sent_packets_b);
}

// Test that transport stats are generated by the RTCStatsCollector for a
// connection that only involves data channels. This is a regression test for
// crbug.com/826972.
#ifdef WEBRTC_HAVE_SCTP
TEST_P(DataChannelIntegrationTest,
       TransportStatsReportedForDataChannelOnlyConnection) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->CreateDataChannel();

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_channel(), kDefaultTimeout);

  auto caller_report = caller()->NewGetStats();
  EXPECT_EQ(1u, caller_report->GetStatsOfType<RTCTransportStats>().size());
  auto callee_report = callee()->NewGetStats();
  EXPECT_EQ(1u, callee_report->GetStatsOfType<RTCTransportStats>().size());
}

INSTANTIATE_TEST_SUITE_P(DataChannelIntegrationTest,
                         DataChannelIntegrationTest,
                         Values(SdpSemantics::kPlanB,
                                SdpSemantics::kUnifiedPlan));

INSTANTIATE_TEST_SUITE_P(DataChannelIntegrationTest,
                         DataChannelIntegrationTestWithFakeClock,
                         Values(SdpSemantics::kPlanB,
                                SdpSemantics::kUnifiedPlan));

TEST_F(DataChannelIntegrationTestUnifiedPlan,
       EndToEndCallWithBundledSctpDataChannel) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->AddAudioVideoTracks();
  callee()->AddAudioVideoTracks();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  network_thread()->Invoke<void>(RTC_FROM_HERE, [this] {
    ASSERT_EQ_WAIT(SctpTransportState::kConnected,
                   caller()->pc()->GetSctpTransport()->Information().state(),
                   kDefaultTimeout);
  });
  ASSERT_TRUE_WAIT(callee()->data_channel(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);
}

TEST_F(DataChannelIntegrationTestUnifiedPlan,
       EndToEndCallWithDataChannelOnlyConnects) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_channel(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);
  ASSERT_TRUE(caller()->data_observer()->IsOpen());
}

TEST_F(DataChannelIntegrationTestUnifiedPlan, DataChannelClosesWhenClosed) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);
  caller()->data_channel()->Close();
  ASSERT_TRUE_WAIT(!callee()->data_observer()->IsOpen(), kDefaultTimeout);
}

TEST_F(DataChannelIntegrationTestUnifiedPlan,
       DataChannelClosesWhenClosedReverse) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);
  callee()->data_channel()->Close();
  ASSERT_TRUE_WAIT(!caller()->data_observer()->IsOpen(), kDefaultTimeout);
}

TEST_F(DataChannelIntegrationTestUnifiedPlan,
       DataChannelClosesWhenPeerConnectionClosed) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  caller()->CreateDataChannel();
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_TRUE_WAIT(SignalingStateStable(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer(), kDefaultTimeout);
  ASSERT_TRUE_WAIT(callee()->data_observer()->IsOpen(), kDefaultTimeout);
  caller()->pc()->Close();
  ASSERT_TRUE_WAIT(!callee()->data_observer()->IsOpen(), kDefaultTimeout);
}

#endif  // WEBRTC_HAVE_SCTP

}  // namespace

}  // namespace webrtc
