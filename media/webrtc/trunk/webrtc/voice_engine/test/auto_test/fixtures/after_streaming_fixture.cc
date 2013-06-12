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

AfterStreamingFixture::AfterStreamingFixture()
    : channel_(voe_base_->CreateChannel()) {
  EXPECT_GE(channel_, 0);

  fake_microphone_input_file_ = resource_manager_.long_audio_file_path();
  EXPECT_FALSE(fake_microphone_input_file_.empty());

  SetUpLocalPlayback();
  ResumePlaying();
  RestartFakeMicrophone();
}

AfterStreamingFixture::~AfterStreamingFixture() {
  voe_file_->StopPlayingFileAsMicrophone(channel_);
  PausePlaying();

  EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));
  voe_base_->DeleteChannel(channel_);
  delete transport_;
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

void AfterStreamingFixture::PausePlaying() {
  EXPECT_EQ(0, voe_base_->StopSend(channel_));
  EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
  EXPECT_EQ(0, voe_base_->StopReceive(channel_));
}

void AfterStreamingFixture::ResumePlaying() {
  EXPECT_EQ(0, voe_base_->StartReceive(channel_));
  EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
  EXPECT_EQ(0, voe_base_->StartSend(channel_));
}

void AfterStreamingFixture::SetUpLocalPlayback() {
  transport_ = new LoopBackTransport(voe_network_);
  EXPECT_EQ(0, voe_network_->RegisterExternalTransport(channel_, *transport_));

  webrtc::CodecInst codec;
  codec.channels = 1;
  codec.pacsize = 160;
  codec.plfreq = 8000;
  codec.pltype = 0;
  codec.rate = 64000;
  strcpy(codec.plname, "PCMU");

  voe_codec_->SetSendCodec(channel_, codec);
}
