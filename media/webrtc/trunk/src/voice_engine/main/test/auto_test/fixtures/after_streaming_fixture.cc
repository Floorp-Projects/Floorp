/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "after_streaming_fixture.h"

#include <cstring>

static const char* kLoopbackIp = "127.0.0.1";

AfterStreamingFixture::AfterStreamingFixture()
    : channel_(voe_base_->CreateChannel()) {
  EXPECT_GE(channel_, 0);

  fake_microphone_input_file_ = resource_manager_.long_audio_file_path();
  EXPECT_FALSE(fake_microphone_input_file_.empty());

  SetUpLocalPlayback();
  StartPlaying();
}

AfterStreamingFixture::~AfterStreamingFixture() {
  voe_file_->StopPlayingFileAsMicrophone(channel_);
  voe_base_->StopSend(channel_);
  voe_base_->StopPlayout(channel_);
  voe_base_->StopReceive(channel_);

  voe_base_->DeleteChannel(channel_);
}

void AfterStreamingFixture::SwitchToManualMicrophone() {
  EXPECT_EQ(0, voe_file_->StopPlayingFileAsMicrophone(channel_));

  TEST_LOG("You need to speak manually into the microphone for this test.\n");
  TEST_LOG("Please start speaking now.\n");
  Sleep(1000);
}

void AfterStreamingFixture::RestartFakeMicrophone() {
  EXPECT_EQ(0, voe_file_->StartPlayingFileAsMicrophone(
        channel_, fake_microphone_input_file_.c_str(), true, true));
}

void AfterStreamingFixture::SetUpLocalPlayback() {
  EXPECT_EQ(0, voe_base_->SetSendDestination(channel_, 8000, kLoopbackIp));
  EXPECT_EQ(0, voe_base_->SetLocalReceiver(0, 8000));

  webrtc::CodecInst codec;
  codec.channels = 1;
  codec.pacsize = 160;
  codec.plfreq = 8000;
  codec.pltype = 0;
  codec.rate = 64000;
  strcpy(codec.plname, "PCMU");

  voe_codec_->SetSendCodec(channel_, codec);
}

void AfterStreamingFixture::StartPlaying() {
  EXPECT_EQ(0, voe_base_->StartReceive(channel_));
  EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));
  RestartFakeMicrophone();
}
