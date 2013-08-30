/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"

#include <math.h>

#include <cassert>
#include <iostream>

#include "gtest/gtest.h"
#include "testsupport/fileutils.h"
#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/main/test/Channel.h"
#include "webrtc/modules/audio_coding/main/test/PCMFile.h"
#include "webrtc/modules/audio_coding/main/test/utility.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

namespace {
double FrameRms(AudioFrame& frame) {
  int samples = frame.num_channels_ * frame.samples_per_channel_;
  double rms = 0;
  for (int n = 0; n < samples; ++n)
    rms += frame.data_[n] * frame.data_[n];
  rms /= samples;
  rms = sqrt(rms);
  return rms;
}

}

class InitialPlayoutDelayTest : public ::testing::Test {
 protected:

  InitialPlayoutDelayTest()
      : acm_a_(NULL),
        acm_b_(NULL),
        channel_a2b_(NULL) {
  }

  ~InitialPlayoutDelayTest() {
  }

  void TearDown() {
    if (acm_a_ != NULL) {
      AudioCodingModule::Destroy(acm_a_);
      acm_a_ = NULL;
    }
    if (acm_b_ != NULL) {
      AudioCodingModule::Destroy(acm_b_);
      acm_b_ = NULL;
    }
    if (channel_a2b_ != NULL) {
      delete channel_a2b_;
      channel_a2b_ = NULL;
    }
  }

  void SetUp() {
    acm_a_ = AudioCodingModule::Create(0);
    acm_b_ = AudioCodingModule::Create(1);

    acm_b_->InitializeReceiver();
    acm_a_->InitializeReceiver();

    // Register all L16 codecs in receiver.
    CodecInst codec;
    const int kFsHz[3] = { 8000, 16000, 32000 };
    const int kChannels[2] = { 1, 2 };
    for (int n = 0; n < 3; ++n) {
      for (int k = 0; k < 2; ++k) {
        AudioCodingModule::Codec("L16", &codec, kFsHz[n], kChannels[k]);
        acm_b_->RegisterReceiveCodec(codec);
      }
    }

    // Create and connect the channel
    channel_a2b_ = new Channel;
    acm_a_->RegisterTransportCallback(channel_a2b_);
    channel_a2b_->RegisterReceiverACM(acm_b_);
  }

  void Run(CodecInst codec, int initial_delay_ms) {
    AudioFrame in_audio_frame;
    AudioFrame out_audio_frame;
    int num_frames = 0;
    const int kAmp = 10000;
    in_audio_frame.sample_rate_hz_ = codec.plfreq;
    in_audio_frame.num_channels_ = codec.channels;
    in_audio_frame.samples_per_channel_ = codec.plfreq / 100;  // 10 ms.
    int samples = in_audio_frame.num_channels_ *
        in_audio_frame.samples_per_channel_;
    for (int n = 0; n < samples; ++n) {
      in_audio_frame.data_[n] = kAmp;
    }

    uint32_t timestamp = 0;
    double rms = 0;
    acm_a_->RegisterSendCodec(codec);
    acm_b_->SetInitialPlayoutDelay(initial_delay_ms);
    while (rms < kAmp / 2) {
      in_audio_frame.timestamp_ = timestamp;
      timestamp += in_audio_frame.samples_per_channel_;
      ASSERT_EQ(0, acm_a_->Add10MsData(in_audio_frame));
      ASSERT_LE(0, acm_a_->Process());
      ASSERT_EQ(0, acm_b_->PlayoutData10Ms(codec.plfreq, &out_audio_frame));
      rms = FrameRms(out_audio_frame);
      ++num_frames;
    }

    ASSERT_GE(num_frames * 10, initial_delay_ms);
    ASSERT_LE(num_frames * 10, initial_delay_ms + 100);
  }

  AudioCodingModule* acm_a_;
  AudioCodingModule* acm_b_;
  Channel* channel_a2b_;
};

TEST_F( InitialPlayoutDelayTest, NbMono) {
  CodecInst codec;
  AudioCodingModule::Codec("L16", &codec, 8000, 1);
  Run(codec, 3000);
}

TEST_F( InitialPlayoutDelayTest, WbMono) {
  CodecInst codec;
  AudioCodingModule::Codec("L16", &codec, 16000, 1);
  Run(codec, 3000);
}

TEST_F( InitialPlayoutDelayTest, SwbMono) {
  CodecInst codec;
  AudioCodingModule::Codec("L16", &codec, 32000, 1);
  Run(codec, 2000);  // NetEq buffer is not sufficiently large for 3 sec of
                     // PCM16 super-wideband.
}

TEST_F( InitialPlayoutDelayTest, NbStereo) {
  CodecInst codec;
  AudioCodingModule::Codec("L16", &codec, 8000, 2);
  Run(codec, 3000);
}

TEST_F( InitialPlayoutDelayTest, WbStereo) {
  CodecInst codec;
  AudioCodingModule::Codec("L16", &codec, 16000, 2);
  Run(codec, 3000);
}

TEST_F( InitialPlayoutDelayTest, SwbStereo) {
  CodecInst codec;
  AudioCodingModule::Codec("L16", &codec, 32000, 2);
  Run(codec, 2000);  // NetEq buffer is not sufficiently large for 3 sec of
                     // PCM16 super-wideband.
}

}
  // namespace webrtc
