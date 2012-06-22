/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/main/interface/voe_external_media.h"
#include "voice_engine/main/test/auto_test/fakes/fake_media_process.h"
#include "voice_engine/main/test/auto_test/fixtures/after_streaming_fixture.h"

class ExternalMediaTest : public AfterStreamingFixture {
 protected:
  void TestRegisterExternalMedia(int channel, webrtc::ProcessingTypes type) {
    FakeMediaProcess fake_media_process;
    EXPECT_EQ(0, voe_xmedia_->RegisterExternalMediaProcessing(
        channel, type, fake_media_process));
    Sleep(2000);

    TEST_LOG("Back to normal.\n");
    EXPECT_EQ(0, voe_xmedia_->DeRegisterExternalMediaProcessing(
        channel, type));
    Sleep(2000);
  }
};

TEST_F(ExternalMediaTest, ManualCanRecordAndPlaybackUsingExternalPlayout) {
  SwitchToManualMicrophone();

  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
  EXPECT_EQ(0, voe_xmedia_->SetExternalPlayoutStatus(true));
  EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));

  TEST_LOG("Recording data for 2 seconds starting now: please speak.\n");
  int16_t recording[32000];
  for (int i = 0; i < 200; i++) {
    int sample_length = 0;
    EXPECT_EQ(0, voe_xmedia_->ExternalPlayoutGetData(
        &(recording[i * 160]), 16000, 100, sample_length));
    EXPECT_EQ(160, sample_length);
    Sleep(10);
  }

  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
  EXPECT_EQ(0, voe_xmedia_->SetExternalPlayoutStatus(false));
  EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
  EXPECT_EQ(0, voe_xmedia_->SetExternalRecordingStatus(true));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));

  TEST_LOG("Playing back recording, you should hear what you said earlier.\n");
  for (int i = 0; i < 200; i++) {
    EXPECT_EQ(0, voe_xmedia_->ExternalRecordingInsertData(
        &(recording[i * 160]), 160, 16000, 20));
    Sleep(10);
  }

  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_xmedia_->SetExternalRecordingStatus(false));
}

TEST_F(ExternalMediaTest,
    ManualRegisterExternalMediaProcessingOnAllChannelsAffectsPlayout) {
  TEST_LOG("Enabling external media processing: audio should be affected.\n");
  TestRegisterExternalMedia(-1, webrtc::kPlaybackAllChannelsMixed);
}

TEST_F(ExternalMediaTest,
    ManualRegisterExternalMediaOnSingleChannelAffectsPlayout) {
  TEST_LOG("Enabling external media processing: audio should be affected.\n");
  TestRegisterExternalMedia(channel_, webrtc::kRecordingPerChannel);
}

TEST_F(ExternalMediaTest,
    ManualRegisterExternalMediaOnAllChannelsMixedAffectsRecording) {
  SwitchToManualMicrophone();
  TEST_LOG("Speak and verify your voice is distorted.\n");
  TestRegisterExternalMedia(-1, webrtc::kRecordingAllChannelsMixed);
}
