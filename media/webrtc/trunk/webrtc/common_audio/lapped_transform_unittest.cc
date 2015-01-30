/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/lapped_transform.h"

#include <cmath>
#include <cstring>

#include "testing/gtest/include/gtest/gtest.h"

using std::complex;

namespace {

class NoopCallback : public webrtc::LappedTransform::Callback {
 public:
  NoopCallback() : block_num_(0) {}

  virtual void ProcessAudioBlock(const complex<float>* const* in_block,
                                 int in_channels, int frames, int out_channels,
                                 complex<float>* const* out_block) {
    CHECK_EQ(in_channels, out_channels);
    for (int i = 0; i < out_channels; ++i) {
      memcpy(out_block[i], in_block[i], sizeof(**in_block) * frames);
    }
    ++block_num_;
  }

  int block_num() {
    return block_num_;
  }

 private:
  int block_num_;
};

class FftCheckerCallback : public webrtc::LappedTransform::Callback {
 public:
  FftCheckerCallback() : block_num_(0) {}

  virtual void ProcessAudioBlock(const complex<float>* const* in_block,
                                 int in_channels, int frames, int out_channels,
                                 complex<float>* const* out_block) {
    CHECK_EQ(in_channels, out_channels);

    float full_length = (frames - 1) * 2;
    ++block_num_;

    if (block_num_ == 1) {
      for (int i = 0; i < frames; ++i) {
        ASSERT_NEAR(in_block[0][i].real(), 0.0f, 1e-5f);
        ASSERT_NEAR(in_block[0][i].imag(), 0.0f, 1e-5f);
      }
    } else {
      ASSERT_NEAR(in_block[0][0].real(), full_length, 1e-5f);
      ASSERT_NEAR(in_block[0][0].imag(), 0.0f, 1e-5f);
      for (int i = 1; i < frames; ++i) {
        ASSERT_NEAR(in_block[0][i].real(), 0.0f, 1e-5f);
        ASSERT_NEAR(in_block[0][i].imag(), 0.0f, 1e-5f);
      }
    }
  }

  int block_num() {
    return block_num_;
  }

 private:
  int block_num_;
};

void SetFloatArray(float value, int rows, int cols, float* const* array) {
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      array[i][j] = value;
    }
  }
}

}  // namespace

namespace webrtc {

TEST(LappedTransformTest, Windowless) {
  const int kChannels = 3;
  const int kChunkLength = 512;
  const int kBlockLength = 64;
  const int kShiftAmount = 32;
  NoopCallback noop;
  LappedTransform trans(kChannels, kChannels, kChunkLength, nullptr,
                        kBlockLength, kShiftAmount, &noop);
  float in_buffer[kChannels][kChunkLength];
  float* in_chunk[kChannels];
  float out_buffer[kChannels][kChunkLength];
  float* out_chunk[kChannels];

  in_chunk[0] = in_buffer[0];
  in_chunk[1] = in_buffer[1];
  in_chunk[2] = in_buffer[2];
  out_chunk[0] = out_buffer[0];
  out_chunk[1] = out_buffer[1];
  out_chunk[2] = out_buffer[2];
  SetFloatArray(2.0f, kChannels, kChunkLength, in_chunk);
  SetFloatArray(-1.0f, kChannels, kChunkLength, out_chunk);

  trans.ProcessChunk(in_chunk, out_chunk);

  for (int i = 0; i < kChannels; ++i) {
    for (int j = 0; j < kChunkLength; ++j) {
      ASSERT_NEAR(out_chunk[i][j], (j < kBlockLength) ? 0.0f : 2.0f, 1e-5f);
    }
  }

  ASSERT_EQ(kChunkLength / kBlockLength, noop.block_num());
}

TEST(LappedTransformTest, IdentityProcessor) {
  const int kChunkLength = 512;
  const int kBlockLength = 64;
  const int kShiftAmount = 32;
  NoopCallback noop;
  float window[kBlockLength];
  float* window_ptr = window;

  // Identity window for |overlap = block_size / 2|.
  SetFloatArray(sqrtf(0.5f), 1, kBlockLength, &window_ptr);

  LappedTransform trans(1, 1, kChunkLength, window, kBlockLength, kShiftAmount,
                        &noop);
  float in_buffer[kChunkLength];
  float* in_chunk = in_buffer;
  float out_buffer[kChunkLength];
  float* out_chunk = out_buffer;

  SetFloatArray(2.0f, 1, kChunkLength, &in_chunk);
  SetFloatArray(-1.0f, 1, kChunkLength, &out_chunk);

  trans.ProcessChunk(&in_chunk, &out_chunk);

  for (int i = 0; i < kChunkLength; ++i) {
    ASSERT_NEAR(out_chunk[i], (i < kBlockLength) ? 0.0f : 2.0f, 1e-5f);
  }

  ASSERT_EQ(kChunkLength / kShiftAmount, noop.block_num());
}

TEST(LappedTransformTest, Callbacks) {
  const int kChunkLength = 512;
  const int kBlockLength = 64;
  FftCheckerCallback call;
  LappedTransform trans(1, 1, kChunkLength, nullptr, kBlockLength,
                        kBlockLength, &call);
  float in_buffer[kChunkLength];
  float* in_chunk = in_buffer;
  float out_buffer[kChunkLength];
  float* out_chunk = out_buffer;

  SetFloatArray(1.0f, 1, kChunkLength, &in_chunk);
  SetFloatArray(-1.0f, 1, kChunkLength, &out_chunk);

  trans.ProcessChunk(&in_chunk, &out_chunk);

  ASSERT_EQ(kChunkLength / kBlockLength, call.block_num());
}

}  // namespace webrtc

