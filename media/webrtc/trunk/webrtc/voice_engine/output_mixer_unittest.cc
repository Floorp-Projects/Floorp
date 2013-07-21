/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "gtest/gtest.h"

#include "output_mixer.h"
#include "output_mixer_internal.h"

namespace webrtc {
namespace voe {
namespace {

class OutputMixerTest : public ::testing::Test {
 protected:
  OutputMixerTest() {
    src_frame_.sample_rate_hz_ = 16000;
    src_frame_.samples_per_channel_ = src_frame_.sample_rate_hz_ / 100;
    src_frame_.num_channels_ = 1;
    dst_frame_ = src_frame_;
    golden_frame_ = src_frame_;
  }

  void RunResampleTest(int src_channels, int src_sample_rate_hz,
                       int dst_channels, int dst_sample_rate_hz);

  Resampler resampler_;
  AudioFrame src_frame_;
  AudioFrame dst_frame_;
  AudioFrame golden_frame_;
};

// Sets the signal value to increase by |data| with every sample. Floats are
// used so non-integer values result in rounding error, but not an accumulating
// error.
void SetMonoFrame(AudioFrame* frame, float data, int sample_rate_hz) {
  frame->num_channels_ = 1;
  frame->sample_rate_hz_ = sample_rate_hz;
  frame->samples_per_channel_ = sample_rate_hz / 100;
  for (int i = 0; i < frame->samples_per_channel_; i++) {
    frame->data_[i] = data * i;
  }
}

// Keep the existing sample rate.
void SetMonoFrame(AudioFrame* frame, float data) {
  SetMonoFrame(frame, data, frame->sample_rate_hz_);
}

// Sets the signal value to increase by |left| and |right| with every sample in
// each channel respectively.
void SetStereoFrame(AudioFrame* frame, float left, float right,
                    int sample_rate_hz) {
  frame->num_channels_ = 2;
  frame->sample_rate_hz_ = sample_rate_hz;
  frame->samples_per_channel_ = sample_rate_hz / 100;
  for (int i = 0; i < frame->samples_per_channel_; i++) {
    frame->data_[i * 2] = left * i;
    frame->data_[i * 2 + 1] = right * i;
  }
}

// Keep the existing sample rate.
void SetStereoFrame(AudioFrame* frame, float left, float right) {
  SetStereoFrame(frame, left, right, frame->sample_rate_hz_);
}

void VerifyParams(const AudioFrame& ref_frame, const AudioFrame& test_frame) {
  EXPECT_EQ(ref_frame.num_channels_, test_frame.num_channels_);
  EXPECT_EQ(ref_frame.samples_per_channel_, test_frame.samples_per_channel_);
  EXPECT_EQ(ref_frame.sample_rate_hz_, test_frame.sample_rate_hz_);
}

// Computes the best SNR based on the error between |ref_frame| and
// |test_frame|. It allows for a sample delay between the signals to
// compensate for the resampling delay.
float ComputeSNR(const AudioFrame& ref_frame, const AudioFrame& test_frame,
                 int max_delay) {
  VerifyParams(ref_frame, test_frame);
  float best_snr = 0;
  int best_delay = 0;
  for (int delay = 0; delay < max_delay; delay++) {
    float mse = 0;
    float variance = 0;
    for (int i = 0; i < ref_frame.samples_per_channel_ *
        ref_frame.num_channels_ - delay; i++) {
      int error = ref_frame.data_[i] - test_frame.data_[i + delay];
      mse += error * error;
      variance += ref_frame.data_[i] * ref_frame.data_[i];
    }
    float snr = 100;  // We assign 100 dB to the zero-error case.
    if (mse > 0)
      snr = 10 * log10(variance / mse);
    if (snr > best_snr) {
      best_snr = snr;
      best_delay = delay;
    }
  }
  printf("SNR=%.1f dB at delay=%d\n", best_snr, best_delay);
  return best_snr;
}

void VerifyFramesAreEqual(const AudioFrame& ref_frame,
                          const AudioFrame& test_frame) {
  VerifyParams(ref_frame, test_frame);
  for (int i = 0; i < ref_frame.samples_per_channel_ * ref_frame.num_channels_;
      i++) {
    EXPECT_EQ(ref_frame.data_[i], test_frame.data_[i]);
  }
}

void OutputMixerTest::RunResampleTest(int src_channels,
                                      int src_sample_rate_hz,
                                      int dst_channels,
                                      int dst_sample_rate_hz) {
  Resampler resampler;  // Create a new one with every test.
  const int16_t kSrcLeft = 60;  // Shouldn't overflow for any used sample rate.
  const int16_t kSrcRight = 30;
  const float kResamplingFactor = (1.0 * src_sample_rate_hz) /
      dst_sample_rate_hz;
  const float kDstLeft = kResamplingFactor * kSrcLeft;
  const float kDstRight = kResamplingFactor * kSrcRight;
  const float kDstMono = (kDstLeft + kDstRight) / 2;
  if (src_channels == 1)
    SetMonoFrame(&src_frame_, kSrcLeft, src_sample_rate_hz);
  else
    SetStereoFrame(&src_frame_, kSrcLeft, kSrcRight, src_sample_rate_hz);

  if (dst_channels == 1) {
    SetMonoFrame(&dst_frame_, 0, dst_sample_rate_hz);
    if (src_channels == 1)
      SetMonoFrame(&golden_frame_, kDstLeft, dst_sample_rate_hz);
    else
      SetMonoFrame(&golden_frame_, kDstMono, dst_sample_rate_hz);
  } else {
    SetStereoFrame(&dst_frame_, 0, 0, dst_sample_rate_hz);
    if (src_channels == 1)
      SetStereoFrame(&golden_frame_, kDstLeft, kDstLeft, dst_sample_rate_hz);
    else
      SetStereoFrame(&golden_frame_, kDstLeft, kDstRight, dst_sample_rate_hz);
  }

  // The speex resampler has a known delay dependent on quality and rates,
  // which we approximate here. Multiplying by two gives us a crude maximum
  // for any resampling, as the old resampler typically (but not always)
  // has lower delay.  The actual delay is calculated internally based on the
  // filter length in the QualityMap.
  static const int kInputKernelDelaySamples = 16*3;
  const int max_delay = std::min(1.0f, 1/kResamplingFactor) *
                        kInputKernelDelaySamples * dst_channels * 2;
  printf("(%d, %d Hz) -> (%d, %d Hz) ",  // SNR reported on the same line later.
      src_channels, src_sample_rate_hz, dst_channels, dst_sample_rate_hz);
  EXPECT_EQ(0, RemixAndResample(src_frame_, &resampler, &dst_frame_));
  EXPECT_GT(ComputeSNR(golden_frame_, dst_frame_, max_delay), 40.0f);
}

// These two tests assume memcpy() (no delay and no filtering) for input
// freq == output freq && same channels.  RemixAndResample uses 'Fixed'
// resamplers to enable this behavior
TEST_F(OutputMixerTest, RemixAndResampleCopyFrameSucceeds) {
  // Stereo -> stereo.
  SetStereoFrame(&src_frame_, 10, 10);
  SetStereoFrame(&dst_frame_, 0, 0);
  EXPECT_EQ(0, RemixAndResample(src_frame_, &resampler_, &dst_frame_));
  VerifyFramesAreEqual(src_frame_, dst_frame_);

  // Mono -> mono.
  SetMonoFrame(&src_frame_, 20);
  SetMonoFrame(&dst_frame_, 0);
  EXPECT_EQ(0, RemixAndResample(src_frame_, &resampler_, &dst_frame_));
  VerifyFramesAreEqual(src_frame_, dst_frame_);
}

TEST_F(OutputMixerTest, RemixAndResampleMixingOnlySucceeds) {
  // Stereo -> mono.
  SetStereoFrame(&dst_frame_, 0, 0);
  SetMonoFrame(&src_frame_, 10);
  SetStereoFrame(&golden_frame_, 10, 10);
  EXPECT_EQ(0, RemixAndResample(src_frame_, &resampler_, &dst_frame_));
  VerifyFramesAreEqual(dst_frame_, golden_frame_);

  // Mono -> stereo.
  SetMonoFrame(&dst_frame_, 0);
  SetStereoFrame(&src_frame_, 10, 20);
  SetMonoFrame(&golden_frame_, 15);
  EXPECT_EQ(0, RemixAndResample(src_frame_, &resampler_, &dst_frame_));
  VerifyFramesAreEqual(golden_frame_, dst_frame_);
}

TEST_F(OutputMixerTest, RemixAndResampleSucceeds) {
  // We don't attempt to be exhaustive here, but just get good coverage. Some
  // combinations of rates will not be resampled, and some give an odd
  // resampling factor which makes it more difficult to evaluate.
  const int kSampleRates[] = {16000, 32000, 44100, 48000};
  const int kSampleRatesSize = sizeof(kSampleRates) / sizeof(*kSampleRates);
  const int kChannels[] = {1, 2};
  const int kChannelsSize = sizeof(kChannels) / sizeof(*kChannels);
  for (int src_rate = 0; src_rate < kSampleRatesSize; src_rate++) {
    for (int dst_rate = 0; dst_rate < kSampleRatesSize; dst_rate++) {
      for (int src_channel = 0; src_channel < kChannelsSize; src_channel++) {
        for (int dst_channel = 0; dst_channel < kChannelsSize; dst_channel++) {
          RunResampleTest(kChannels[src_channel], kSampleRates[src_rate],
                          kChannels[dst_channel], kSampleRates[dst_rate]);
        }
      }
    }
  }
}

}  // namespace
}  // namespace voe
}  // namespace webrtc
