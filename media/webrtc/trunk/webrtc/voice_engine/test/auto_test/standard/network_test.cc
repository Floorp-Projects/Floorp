/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/include/mock/mock_voe_connection_observer.h"
#include "webrtc/voice_engine/include/mock/mock_voe_observer.h"
#include "webrtc/voice_engine/test/auto_test/fakes/fake_external_transport.h"
#include "webrtc/voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"
#include "webrtc/voice_engine/test/auto_test/voe_standard_test.h"
#include "webrtc/voice_engine/test/auto_test/voe_test_interface.h"

static const int kDefaultRtpPort = 8000;
static const int kDefaultRtcpPort = 8001;

class NetworkTest : public AfterStreamingFixture {
};

using ::testing::Between;

TEST_F(NetworkTest,
    CallsObserverOnTimeoutAndRestartWhenPacketTimeoutNotificationIsEnabled) {
  // First, get rid of the default, asserting observer and install our observer.
  EXPECT_EQ(0, voe_base_->DeRegisterVoiceEngineObserver());
  webrtc::MockVoEObserver mock_observer;
  EXPECT_EQ(0, voe_base_->RegisterVoiceEngineObserver(mock_observer));

  // Define expectations.
  int expected_error = VE_RECEIVE_PACKET_TIMEOUT;
  EXPECT_CALL(mock_observer, CallbackOnError(channel_, expected_error))
      .Times(1);
  expected_error = VE_PACKET_RECEIPT_RESTARTED;
    EXPECT_CALL(mock_observer, CallbackOnError(channel_, expected_error))
      .Times(1);

  // Get some speech going.
  Sleep(500);

  // Enable packet timeout.
  EXPECT_EQ(0, voe_network_->SetPacketTimeoutNotification(channel_, true, 1));

  // Trigger a timeout.
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  Sleep(1500);

  // Trigger a restart event.
  EXPECT_EQ(0, voe_base_->StartSend(channel_));
  Sleep(500);
}

TEST_F(NetworkTest, DoesNotCallDeRegisteredObserver) {
  // De-register the default observer. This test will fail if the observer gets
  // called for any reason, so if this de-register doesn't work the test will
  // fail.
  EXPECT_EQ(0, voe_base_->DeRegisterVoiceEngineObserver());

  // Get some speech going.
  Sleep(500);

  // Enable packet timeout.
  EXPECT_EQ(0, voe_network_->SetPacketTimeoutNotification(channel_, true, 1));

  // Trigger a timeout.
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  Sleep(1500);
}

// TODO(phoglund): flaky on Linux
TEST_F(NetworkTest,
       DISABLED_ON_LINUX(DeadOrAliveObserverSeesAliveMessagesIfEnabled)) {
  if (!FLAGS_include_timing_dependent_tests) {
    TEST_LOG("Skipping test - running in slow execution environment...\n");
    return;
  }

  webrtc::MockVoeConnectionObserver mock_observer;
  EXPECT_EQ(0, voe_network_->RegisterDeadOrAliveObserver(
      channel_, mock_observer));

  // We should be called about 4 times in four seconds, but 3 is OK too.
  EXPECT_CALL(mock_observer, OnPeriodicDeadOrAlive(channel_, true))
      .Times(Between(3, 4));

  EXPECT_EQ(0, voe_network_->SetPeriodicDeadOrAliveStatus(channel_, true, 1));
  Sleep(4000);

  EXPECT_EQ(0, voe_network_->DeRegisterDeadOrAliveObserver(channel_));
}

TEST_F(NetworkTest, DeadOrAliveObserverSeesDeadMessagesIfEnabled) {
  if (!FLAGS_include_timing_dependent_tests) {
    TEST_LOG("Skipping test - running in slow execution environment...\n");
    return;
  }

  // "When do you see them?" - "All the time!"
  webrtc::MockVoeConnectionObserver mock_observer;
  EXPECT_EQ(0, voe_network_->RegisterDeadOrAliveObserver(
      channel_, mock_observer));

  Sleep(500);

  // We should be called about 4 times in four seconds, but 3 is OK too.
  EXPECT_CALL(mock_observer, OnPeriodicDeadOrAlive(channel_, false))
      .Times(Between(3, 4));

  EXPECT_EQ(0, voe_network_->SetPeriodicDeadOrAliveStatus(channel_, true, 1));
  EXPECT_EQ(0, voe_rtp_rtcp_->SetRTCPStatus(channel_, false));
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  Sleep(4000);

  EXPECT_EQ(0, voe_network_->DeRegisterDeadOrAliveObserver(channel_));
}

TEST_F(NetworkTest, CanSwitchToExternalTransport) {
  EXPECT_EQ(0, voe_base_->StopReceive(channel_));
  EXPECT_EQ(0, voe_base_->DeleteChannel(channel_));
  channel_ = voe_base_->CreateChannel();

  FakeExternalTransport external_transport(voe_network_);
  EXPECT_EQ(0, voe_network_->RegisterExternalTransport(
      channel_, external_transport));

  EXPECT_EQ(0, voe_base_->StartReceive(channel_));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));
  EXPECT_EQ(0, voe_base_->StartPlayout(channel_));

  Sleep(1000);

  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
  EXPECT_EQ(0, voe_base_->StopReceive(channel_));

  EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));
}
