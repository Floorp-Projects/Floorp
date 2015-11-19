/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/include/voe_base.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_device/include/fake_audio_device.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class VoEBaseTest : public ::testing::Test {
 protected:
  VoEBaseTest() :
      voe_(VoiceEngine::Create()),
      base_(VoEBase::GetInterface(voe_)),
      adm_(new FakeAudioDeviceModule) {
  }

  ~VoEBaseTest() {
    base_->Release();
    VoiceEngine::Delete(voe_);
  }

  VoiceEngine* voe_;
  VoEBase* base_;
  rtc::scoped_ptr<FakeAudioDeviceModule> adm_;
};

TEST_F(VoEBaseTest, AcceptsAudioProcessingPtr) {
  AudioProcessing* audioproc = AudioProcessing::Create();
  EXPECT_EQ(0, base_->Init(adm_.get(), audioproc));
  EXPECT_EQ(audioproc, base_->audio_processing());
}

TEST_F(VoEBaseTest, AudioProcessingCreatedAfterInit) {
  EXPECT_TRUE(base_->audio_processing() == NULL);
  EXPECT_EQ(0, base_->Init(adm_.get(), NULL));
  EXPECT_TRUE(base_->audio_processing() != NULL);
}

}  // namespace webrtc
