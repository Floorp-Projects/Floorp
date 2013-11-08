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
