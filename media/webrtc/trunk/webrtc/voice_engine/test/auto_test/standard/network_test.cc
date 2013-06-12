/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/test/auto_test/fakes/fake_external_transport.h"
#include "voice_engine/test/auto_test/fixtures/after_streaming_fixture.h"
#include "voice_engine/test/auto_test/voe_test_interface.h"
#include "voice_engine/test/auto_test/voe_standard_test.h"
#include "voice_engine/include/mock/mock_voe_connection_observer.h"
#include "voice_engine/include/mock/mock_voe_observer.h"

static const int kDefaultRtpPort = 8000;
static const int kDefaultRtcpPort = 8001;

class NetworkTest : public AfterStreamingFixture {
};

using ::testing::Between;

TEST_F(NetworkTest, GetSourceInfoReturnsPortsAndIpAfterReceivingPackets) {
  // Give some time to send speech packets.
  Sleep(200);

  int rtp_port = 0;
  int rtcp_port = 0;
  char source_ip[32] = "127.0.0.1";

  EXPECT_EQ(0, voe_network_->GetSourceInfo(channel_, rtp_port, rtcp_port,
      source_ip));

  EXPECT_EQ(kDefaultRtpPort, rtp_port);
  EXPECT_EQ(kDefaultRtcpPort, rtcp_port);
}

TEST_F(NetworkTest, NoFilterIsEnabledByDefault) {
  int filter_rtp_port = -1;
  int filter_rtcp_port = -1;
  char filter_ip[64] = { 0 };

  EXPECT_EQ(0, voe_network_->GetSourceFilter(
      channel_, filter_rtp_port, filter_rtcp_port, filter_ip));

  EXPECT_EQ(0, filter_rtp_port);
  EXPECT_EQ(0, filter_rtcp_port);
  EXPECT_STREQ("", filter_ip);
}

TEST_F(NetworkTest, ManualCanFilterRtpPort) {
  TEST_LOG("No filter, should hear audio.\n");
  Sleep(1000);

  int port_to_block = kDefaultRtpPort + 10;
  EXPECT_EQ(0, voe_network_->SetSourceFilter(channel_, port_to_block));

  // Changes should take effect immediately.
  int filter_rtp_port = -1;
  int filter_rtcp_port = -1;
  char filter_ip[64] = { 0 };

  EXPECT_EQ(0, voe_network_->GetSourceFilter(
      channel_, filter_rtp_port, filter_rtcp_port, filter_ip));

  EXPECT_EQ(port_to_block, filter_rtp_port);

  TEST_LOG("Now filtering port %d, should not hear audio.\n", port_to_block);
  Sleep(1000);

  TEST_LOG("Removing filter, should hear audio.\n");
  EXPECT_EQ(0, voe_network_->SetSourceFilter(channel_, 0));
  Sleep(1000);
}

TEST_F(NetworkTest, ManualCanFilterIp) {
  TEST_LOG("You should hear audio.\n");
  Sleep(1000);

  int rtcp_port_to_block = kDefaultRtcpPort + 10;
  TEST_LOG("Filtering IP 10.10.10.10, should not hear audio.\n");
  EXPECT_EQ(0, voe_network_->SetSourceFilter(
      channel_, 0, rtcp_port_to_block, "10.10.10.10"));

  int filter_rtp_port = -1;
  int filter_rtcp_port = -1;
  char filter_ip[64] = { 0 };
  EXPECT_EQ(0, voe_network_->GetSourceFilter(
      channel_, filter_rtp_port, filter_rtcp_port, filter_ip));

  EXPECT_EQ(0, filter_rtp_port);
  EXPECT_EQ(rtcp_port_to_block, filter_rtcp_port);
  EXPECT_STREQ("10.10.10.10", filter_ip);
}

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
