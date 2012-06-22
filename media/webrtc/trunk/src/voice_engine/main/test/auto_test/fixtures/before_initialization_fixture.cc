/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "before_initialization_fixture.h"

#include "voice_engine_defines.h"

BeforeInitializationFixture::BeforeInitializationFixture()
    : voice_engine_(webrtc::VoiceEngine::Create()) {
  EXPECT_TRUE(voice_engine_ != NULL);

  voe_base_ = webrtc::VoEBase::GetInterface(voice_engine_);
  voe_codec_ = webrtc::VoECodec::GetInterface(voice_engine_);
  voe_volume_control_ = webrtc::VoEVolumeControl::GetInterface(voice_engine_);
  voe_dtmf_ = webrtc::VoEDtmf::GetInterface(voice_engine_);
  voe_rtp_rtcp_ = webrtc::VoERTP_RTCP::GetInterface(voice_engine_);
  voe_apm_ = webrtc::VoEAudioProcessing::GetInterface(voice_engine_);
  voe_network_ = webrtc::VoENetwork::GetInterface(voice_engine_);
  voe_file_ = webrtc::VoEFile::GetInterface(voice_engine_);
  voe_vsync_ = webrtc::VoEVideoSync::GetInterface(voice_engine_);
  voe_encrypt_ = webrtc::VoEEncryption::GetInterface(voice_engine_);
  voe_hardware_ = webrtc::VoEHardware::GetInterface(voice_engine_);
  voe_xmedia_ = webrtc::VoEExternalMedia::GetInterface(voice_engine_);
  voe_call_report_ = webrtc::VoECallReport::GetInterface(voice_engine_);
  voe_neteq_stats_ = webrtc::VoENetEqStats::GetInterface(voice_engine_);
}

BeforeInitializationFixture::~BeforeInitializationFixture() {
  EXPECT_EQ(0, voe_base_->Release());
  EXPECT_EQ(0, voe_codec_->Release());
  EXPECT_EQ(0, voe_volume_control_->Release());
  EXPECT_EQ(0, voe_dtmf_->Release());
  EXPECT_EQ(0, voe_rtp_rtcp_->Release());
  EXPECT_EQ(0, voe_apm_->Release());
  EXPECT_EQ(0, voe_network_->Release());
  EXPECT_EQ(0, voe_file_->Release());
  EXPECT_EQ(0, voe_vsync_->Release());
  EXPECT_EQ(0, voe_encrypt_->Release());
  EXPECT_EQ(0, voe_hardware_->Release());
  EXPECT_EQ(0, voe_xmedia_->Release());
  EXPECT_EQ(0, voe_call_report_->Release());
  EXPECT_EQ(0, voe_neteq_stats_->Release());

  EXPECT_TRUE(webrtc::VoiceEngine::Delete(voice_engine_));
}

void BeforeInitializationFixture::Sleep(long milliseconds) {
  // Implementation note: This method is used to reduce usage of the macro and
  // avoid ugly errors in Eclipse (its parser can't deal with the sleep macro).
  SLEEP(milliseconds);
}
