/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "after_streaming_fixture.h"

class NetEQTest : public AfterStreamingFixture {
 protected:
  void SetUp() {
    additional_channel_[0] = voe_base_->CreateChannel();
    additional_channel_[1] = voe_base_->CreateChannel();
  }

  void TearDown() {
    voe_base_->DeleteChannel(additional_channel_[0]);
    voe_base_->DeleteChannel(additional_channel_[1]);
  }

  int additional_channel_[2];
};

TEST_F(NetEQTest, GetNetEQPlayoutModeReturnsDefaultModeByDefault) {
  webrtc::NetEqModes mode;
  EXPECT_EQ(0, voe_base_->GetNetEQPlayoutMode(channel_, mode));
  EXPECT_EQ(webrtc::kNetEqDefault, mode);
}

TEST_F(NetEQTest, SetNetEQPlayoutModeActuallySetsTheModeForTheChannel) {
  webrtc::NetEqModes mode;
  // Set for the first channel but leave the others.
  EXPECT_EQ(0, voe_base_->SetNetEQPlayoutMode(channel_, webrtc::kNetEqFax));
  EXPECT_EQ(0, voe_base_->GetNetEQPlayoutMode(channel_, mode));
  EXPECT_EQ(webrtc::kNetEqFax, mode);

  EXPECT_EQ(0, voe_base_->GetNetEQPlayoutMode(additional_channel_[0], mode));
  EXPECT_EQ(webrtc::kNetEqDefault, mode);
  EXPECT_EQ(0, voe_base_->GetNetEQPlayoutMode(additional_channel_[1], mode));
  EXPECT_EQ(webrtc::kNetEqDefault, mode);

  // Set the second channel, leave the others.
  EXPECT_EQ(0, voe_base_->SetNetEQPlayoutMode(
      additional_channel_[0], webrtc::kNetEqStreaming));
  EXPECT_EQ(0, voe_base_->GetNetEQPlayoutMode(additional_channel_[0], mode));
  EXPECT_EQ(webrtc::kNetEqStreaming, mode);

  EXPECT_EQ(0, voe_base_->GetNetEQPlayoutMode(channel_, mode));
  EXPECT_EQ(webrtc::kNetEqFax, mode);
  EXPECT_EQ(0, voe_base_->GetNetEQPlayoutMode(additional_channel_[1], mode));
  EXPECT_EQ(webrtc::kNetEqDefault, mode);

  // Set the third channel, leave the others.
  EXPECT_EQ(0, voe_base_->SetNetEQPlayoutMode(
      additional_channel_[1], webrtc::kNetEqOff));
  EXPECT_EQ(0, voe_base_->GetNetEQPlayoutMode(additional_channel_[1], mode));
  EXPECT_EQ(webrtc::kNetEqOff, mode);

  EXPECT_EQ(0, voe_base_->GetNetEQPlayoutMode(channel_, mode));
  EXPECT_EQ(webrtc::kNetEqFax, mode);
  EXPECT_EQ(0, voe_base_->GetNetEQPlayoutMode(additional_channel_[0], mode));
  EXPECT_EQ(webrtc::kNetEqStreaming, mode);
}

TEST_F(NetEQTest, GetNetEQBgnModeReturnsBgnOnByDefault) {
  webrtc::NetEqBgnModes bgn_mode;
  EXPECT_EQ(0, voe_base_->GetNetEQBGNMode(channel_, bgn_mode));
  EXPECT_EQ(webrtc::kBgnOn, bgn_mode);
}

TEST_F(NetEQTest, SetNetEQBgnModeActuallySetsTheBgnMode) {
  webrtc::NetEqBgnModes bgn_mode;
  EXPECT_EQ(0, voe_base_->SetNetEQBGNMode(channel_, webrtc::kBgnOff));
  EXPECT_EQ(0, voe_base_->GetNetEQBGNMode(channel_, bgn_mode));
  EXPECT_EQ(webrtc::kBgnOff, bgn_mode);

  EXPECT_EQ(0, voe_base_->SetNetEQBGNMode(channel_, webrtc::kBgnFade));
  EXPECT_EQ(0, voe_base_->GetNetEQBGNMode(channel_, bgn_mode));
  EXPECT_EQ(webrtc::kBgnFade, bgn_mode);
}

TEST_F(NetEQTest, ManualSetEQPlayoutModeStillProducesOkAudio) {
  EXPECT_EQ(0, voe_base_->SetNetEQPlayoutMode(channel_, webrtc::kNetEqDefault));
  TEST_LOG("NetEQ default playout mode enabled => should hear OK audio.\n");
  Sleep(2000);

  EXPECT_EQ(0, voe_base_->SetNetEQPlayoutMode(
      channel_, webrtc::kNetEqStreaming));
  TEST_LOG("NetEQ streaming playout mode enabled => should hear OK audio.\n");
  Sleep(2000);

  EXPECT_EQ(0, voe_base_->SetNetEQPlayoutMode(channel_, webrtc::kNetEqFax));
  TEST_LOG("NetEQ fax playout mode enabled => should hear OK audio.\n");
  Sleep(2000);

  EXPECT_EQ(0, voe_base_->SetNetEQPlayoutMode(channel_, webrtc::kNetEqOff));
  TEST_LOG("NetEQ off playout mode enabled => should hear OK audio.\n");
  Sleep(2000);
}
