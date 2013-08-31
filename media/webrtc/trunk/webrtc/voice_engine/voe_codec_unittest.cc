/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/include/voe_codec.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_device/include/fake_audio_device.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

namespace webrtc {
namespace voe {
namespace {

class VoECodecTest : public ::testing::Test {
 protected:
  VoECodecTest()
      : voe_(VoiceEngine::Create()),
        base_(VoEBase::GetInterface(voe_)),
        voe_codec_(VoECodec::GetInterface(voe_)),
        channel_(-1),
        adm_(new FakeAudioDeviceModule),
        red_payload_type_(-1) {
  }

  ~VoECodecTest() {}

  void TearDown() {
    base_->DeleteChannel(channel_);
    base_->Terminate();
    base_->Release();
    voe_codec_->Release();
    VoiceEngine::Delete(voe_);
  }

  void SetUp() {
    // Check if all components are valid.
    ASSERT_TRUE(voe_ != NULL);
    ASSERT_TRUE(base_ != NULL);
    ASSERT_TRUE(voe_codec_ != NULL);
    ASSERT_TRUE(adm_.get() != NULL);
    ASSERT_EQ(0, base_->Init(adm_.get()));
    channel_ = base_->CreateChannel();
    ASSERT_NE(-1, channel_);

    CodecInst my_codec;

    bool primary_found = false;
    bool valid_secondary_found = false;
    bool invalid_secondary_found = false;

    // Find primary and secondary codecs.
    int num_codecs = voe_codec_->NumOfCodecs();
    int n = 0;
    while (n < num_codecs && (!primary_found || !valid_secondary_found ||
        !invalid_secondary_found || red_payload_type_ < 0)) {
      EXPECT_EQ(0, voe_codec_->GetCodec(n, my_codec));
      if (!STR_CASE_CMP(my_codec.plname, "isac") && my_codec.plfreq == 16000) {
        memcpy(&valid_secondary_, &my_codec, sizeof(my_codec));
        valid_secondary_found = true;
      } else if (!STR_CASE_CMP(my_codec.plname, "isac") &&
          my_codec.plfreq == 32000) {
        memcpy(&invalid_secondary_, &my_codec, sizeof(my_codec));
        invalid_secondary_found = true;
      } else if (!STR_CASE_CMP(my_codec.plname, "L16") &&
          my_codec.plfreq == 16000) {
        memcpy(&primary_, &my_codec, sizeof(my_codec));
        primary_found = true;
      } else if (!STR_CASE_CMP(my_codec.plname, "RED")) {
        red_payload_type_ = my_codec.pltype;
      }
      n++;
    }

    EXPECT_TRUE(primary_found);
    EXPECT_TRUE(valid_secondary_found);
    EXPECT_TRUE(invalid_secondary_found);
    EXPECT_NE(-1, red_payload_type_);
  }

  VoiceEngine* voe_;
  VoEBase* base_;
  VoECodec* voe_codec_;
  int channel_;
  CodecInst primary_;
  CodecInst valid_secondary_;
  scoped_ptr<FakeAudioDeviceModule> adm_;

  // A codec which is not valid to be registered as secondary codec.
  CodecInst invalid_secondary_;
  int red_payload_type_;
};


TEST_F(VoECodecTest, DualStreamSetSecondaryBeforePrimaryFails) {
  // Setting secondary before a primary is registered should fail.
  EXPECT_EQ(-1, voe_codec_->SetSecondarySendCodec(channel_, valid_secondary_,
                                                  red_payload_type_));
  red_payload_type_ = 1;
}

TEST_F(VoECodecTest, DualStreamRegisterWithWrongInputsFails) {
  // Register primary codec.
  EXPECT_EQ(0, voe_codec_->SetSendCodec(channel_, primary_));

  // Wrong secondary.
  EXPECT_EQ(-1, voe_codec_->SetSecondarySendCodec(channel_, invalid_secondary_,
                                                  red_payload_type_));

  // Wrong payload.
  EXPECT_EQ(-1, voe_codec_->SetSecondarySendCodec(channel_, valid_secondary_,
                                                  -1));
  // Wrong channel.
  EXPECT_EQ(-1, voe_codec_->SetSecondarySendCodec(channel_ + 1,
                                                  valid_secondary_,
                                                  red_payload_type_));
}

TEST_F(VoECodecTest, DualStreamGetSecodaryEncoder) {
  // Register primary codec.
  EXPECT_EQ(0, voe_codec_->SetSendCodec(channel_, primary_));

  // Register a valid codec.
  EXPECT_EQ(0, voe_codec_->SetSecondarySendCodec(channel_, valid_secondary_,
                                                 red_payload_type_));
  CodecInst my_codec;

  // Get secondary codec from wrong channel.
  EXPECT_EQ(-1, voe_codec_->GetSecondarySendCodec(channel_ + 1, my_codec));

  // Get secondary and compare.
  memset(&my_codec, 0, sizeof(my_codec));
  EXPECT_EQ(0, voe_codec_->GetSecondarySendCodec(channel_, my_codec));

  EXPECT_EQ(valid_secondary_.plfreq, my_codec.plfreq);
  EXPECT_EQ(valid_secondary_.channels, my_codec.channels);
  EXPECT_EQ(valid_secondary_.pacsize, my_codec.pacsize);
  EXPECT_EQ(valid_secondary_.rate, my_codec.rate);
  EXPECT_EQ(valid_secondary_.pltype, my_codec.pltype);
  EXPECT_EQ(0, STR_CASE_CMP(valid_secondary_.plname, my_codec.plname));
}

TEST_F(VoECodecTest, DualStreamRemoveSecondaryCodec) {
  // Register primary codec.
  EXPECT_EQ(0, voe_codec_->SetSendCodec(channel_, primary_));

  // Register a valid codec.
  EXPECT_EQ(0, voe_codec_->SetSecondarySendCodec(channel_, valid_secondary_,
                                                 red_payload_type_));
  // Remove from wrong channel.
  EXPECT_EQ(-1, voe_codec_->RemoveSecondarySendCodec(channel_ + 1));
  EXPECT_EQ(0, voe_codec_->RemoveSecondarySendCodec(channel_));

  CodecInst my_codec;

  // Get should fail, if secondary is removed.
  EXPECT_EQ(-1, voe_codec_->GetSecondarySendCodec(channel_, my_codec));
}

}  // namespace
}  // namespace voe
}  // namespace webrtc
