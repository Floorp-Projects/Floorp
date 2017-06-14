/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "webrtc/common_audio/resampler/include/resampler.h"
#include "webrtc/test/gtest.h"

// TODO(andrew): this is a work-in-progress. Many more tests are needed.

namespace webrtc {
namespace {

const int kNumChannels[] = {1, 2};
const size_t kNumChannelsSize = sizeof(kNumChannels) / sizeof(*kNumChannels);

// Rates we must support.
const int kMaxRate = 96000;
const int kRates[] = {
  8000,
  16000,
  32000,
  44100,
  48000,
  kMaxRate
};
const size_t kRatesSize = sizeof(kRates) / sizeof(*kRates);
const int kMaxChannels = 2;
const size_t kDataSize = static_cast<size_t> (kMaxChannels * kMaxRate / 100);

class ResamplerTest : public testing::Test {
 protected:
  ResamplerTest();
  virtual void SetUp();
  virtual void TearDown();
  void RunResampleTest(int channels,
                       int src_sample_rate_hz,
                       int dst_sample_rate_hz);

  Resampler rs_;
  int16_t data_in_[kDataSize];
  int16_t data_out_[kDataSize];
  int16_t data_reference_[kDataSize];
};

ResamplerTest::ResamplerTest() {}

void ResamplerTest::SetUp() {
  // Initialize input data with anything. The tests are content independent.
  memset(data_in_, 1, sizeof(data_in_));
}

void ResamplerTest::TearDown() {}

TEST_F(ResamplerTest, Reset) {
  // The only failure mode for the constructor is if Reset() fails. For the
  // time being then (until an Init function is added), we rely on Reset()
  // to test the constructor.

  // Check that all required combinations are supported.
  for (size_t i = 0; i < kRatesSize; ++i) {
    for (size_t j = 0; j < kRatesSize; ++j) {
      for (size_t k = 0; k < kNumChannelsSize; ++k) {
        std::ostringstream ss;
        ss << "Input rate: " << kRates[i] << ", output rate: " << kRates[j]
            << ", channels: " << kNumChannels[k];
        SCOPED_TRACE(ss.str());
        EXPECT_EQ(0, rs_.Reset(kRates[i], kRates[j], kTypes[k]));
      }
    }
  }
}

// Sets the signal value to increase by |data| with every sample. Floats are
// used so non-integer values result in rounding error, but not an accumulating
// error.
void SetMonoFrame(int16_t* buffer, float data, int sample_rate_hz) {
  for (int i = 0; i < sample_rate_hz / 100; i++) {
    buffer[i] = data * i;
  }
}

// Sets the signal value to increase by |left| and |right| with every sample in
// each channel respectively.
void SetStereoFrame(int16_t* buffer, float left, float right,
                    int sample_rate_hz) {
  for (int i = 0; i < sample_rate_hz / 100; i++) {
    buffer[i * 2] = left * i;
    buffer[i * 2 + 1] = right * i;
  }
}

// Computes the best SNR based on the error between |ref_frame| and
// |test_frame|. It allows for a sample delay between the signals to
// compensate for the resampling delay.
float ComputeSNR(const int16_t* reference, const int16_t* test,
                 int sample_rate_hz, int channels, int max_delay) {
  float best_snr = 0;
  int best_delay = 0;
  int samples_per_channel = sample_rate_hz/100;
  for (int delay = 0; delay < max_delay; delay++) {
    float mse = 0;
    float variance = 0;
    for (int i = 0; i < samples_per_channel * channels - delay; i++) {
      int error = reference[i] - test[i + delay];
      mse += error * error;
      variance += reference[i] * reference[i];
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

void ResamplerTest::RunResampleTest(int channels,
                                    int src_sample_rate_hz,
                                    int dst_sample_rate_hz) {
  Resampler resampler;  // Create a new one with every test.
  const int16_t kSrcLeft = 60;  // Shouldn't overflow for any used sample rate.
  const int16_t kSrcRight = 30;
  const float kResamplingFactor = (1.0 * src_sample_rate_hz) /
      dst_sample_rate_hz;
  const float kDstLeft = kResamplingFactor * kSrcLeft;
  const float kDstRight = kResamplingFactor * kSrcRight;
  if (channels == 1)
    SetMonoFrame(data_in_, kSrcLeft, src_sample_rate_hz);
  else
    SetStereoFrame(data_in_, kSrcLeft, kSrcRight, src_sample_rate_hz);

  if (channels == 1) {
    SetMonoFrame(data_out_, 0, dst_sample_rate_hz);
    SetMonoFrame(data_reference_, kDstLeft, dst_sample_rate_hz);
  } else {
    SetStereoFrame(data_out_, 0, 0, dst_sample_rate_hz);
    SetStereoFrame(data_reference_, kDstLeft, kDstRight, dst_sample_rate_hz);
  }

  // The speex resampler has a known delay dependent on quality and rates,
  // which we approximate here. Multiplying by two gives us a crude maximum
  // for any resampling, as the old resampler typically (but not always)
  // has lower delay.  The actual delay is calculated internally based on the
  // filter length in the QualityMap.
  static const int kInputKernelDelaySamples = 16*3;
  const int max_delay = std::min(1.0f, 1/kResamplingFactor) *
                        kInputKernelDelaySamples * channels * 2;
  printf("(%d, %d Hz) -> (%d, %d Hz) ",  // SNR reported on the same line later.
      channels, src_sample_rate_hz, channels, dst_sample_rate_hz);

  int in_length = channels * src_sample_rate_hz / 100;
  int out_length = 0;
  EXPECT_EQ(0, rs_.Reset(src_sample_rate_hz, dst_sample_rate_hz,
                         (channels == 1 ?
                          kResamplerSynchronous :
                          kResamplerSynchronousStereo)));
  EXPECT_EQ(0, rs_.Push(data_in_, in_length, data_out_, kDataSize,
                        out_length));
  EXPECT_EQ(channels * dst_sample_rate_hz / 100, out_length);

  //  EXPECT_EQ(0, Resample(src_frame_, &resampler, &dst_frame_));
  EXPECT_GT(ComputeSNR(data_reference_, data_out_, dst_sample_rate_hz,
                       channels, max_delay), 40.0f);
}

TEST_F(ResamplerTest, Mono) {
  // We don't attempt to be exhaustive here, but just get good coverage. Some
  // combinations of rates will not be resampled, and some give an odd
  // resampling factor which makes it more difficult to evaluate.
  const int kSampleRates[] = {16000, 32000, 44100, 48000};
  const int kSampleRatesSize = sizeof(kSampleRates) / sizeof(*kSampleRates);
  for (int src_rate = 0; src_rate < kSampleRatesSize; src_rate++) {
    for (int dst_rate = 0; dst_rate < kSampleRatesSize; dst_rate++) {
      RunResampleTest(kChannels, kSampleRates[src_rate], kSampleRates[dst_rate]);
    }
  }
}

TEST_F(ResamplerTest, Stereo) {
  const int kChannels = 2;
  // We don't attempt to be exhaustive here, but just get good coverage. Some
  // combinations of rates will not be resampled, and some give an odd
  // resampling factor which makes it more difficult to evaluate.
  const int kSampleRates[] = {16000, 32000, 44100, 48000};
  const int kSampleRatesSize = sizeof(kSampleRates) / sizeof(*kSampleRates);
  for (int src_rate = 0; src_rate < kSampleRatesSize; src_rate++) {
    for (int dst_rate = 0; dst_rate < kSampleRatesSize; dst_rate++) {
      RunResampleTest(kChannels, kSampleRates[src_rate], kSampleRates[dst_rate]);
    }
  }
}

}  // namespace
}  // namespace webrtc
