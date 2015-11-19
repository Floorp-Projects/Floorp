/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/fine_audio_buffer.h"

#include <limits.h>
#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_device/mock_audio_device_buffer.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

namespace webrtc {

// The fake audio data is 0,1,..SCHAR_MAX-1,0,1,... This is to make it easy
// to detect errors. This function verifies that the buffers contain such data.
// E.g. if there are two buffers of size 3, buffer 1 would contain 0,1,2 and
// buffer 2 would contain 3,4,5. Note that SCHAR_MAX is 127 so wrap-around
// will happen.
// |buffer| is the audio buffer to verify.
bool VerifyBuffer(const int8_t* buffer, int buffer_number, int size) {
  int start_value = (buffer_number * size) % SCHAR_MAX;
  for (int i = 0; i < size; ++i) {
    if (buffer[i] != (i + start_value) % SCHAR_MAX) {
      return false;
    }
  }
  return true;
}

// This function replaces GetPlayoutData when it's called (which is done
// implicitly when calling GetBufferData). It writes the sequence
// 0,1,..SCHAR_MAX-1,0,1,... to the buffer. Note that this is likely a buffer of
// different size than the one VerifyBuffer verifies.
// |iteration| is the number of calls made to UpdateBuffer prior to this call.
// |samples_per_10_ms| is the number of samples that should be written to the
// buffer (|arg0|).
ACTION_P2(UpdateBuffer, iteration, samples_per_10_ms) {
  int8_t* buffer = static_cast<int8_t*>(arg0);
  int bytes_per_10_ms = samples_per_10_ms * static_cast<int>(sizeof(int16_t));
  int start_value = (iteration * bytes_per_10_ms) % SCHAR_MAX;
  for (int i = 0; i < bytes_per_10_ms; ++i) {
    buffer[i] = (i + start_value) % SCHAR_MAX;
  }
  return samples_per_10_ms;
}

void RunFineBufferTest(int sample_rate, int frame_size_in_samples) {
  const int kSamplesPer10Ms = sample_rate * 10 / 1000;
  const int kFrameSizeBytes = frame_size_in_samples *
      static_cast<int>(sizeof(int16_t));
  const int kNumberOfFrames = 5;
  // Ceiling of integer division: 1 + ((x - 1) / y)
  const int kNumberOfUpdateBufferCalls =
      1 + ((kNumberOfFrames * frame_size_in_samples - 1) / kSamplesPer10Ms);

  MockAudioDeviceBuffer audio_device_buffer;
  EXPECT_CALL(audio_device_buffer, RequestPlayoutData(_))
      .WillRepeatedly(Return(kSamplesPer10Ms));
  {
    InSequence s;
    for (int i = 0; i < kNumberOfUpdateBufferCalls; ++i) {
      EXPECT_CALL(audio_device_buffer, GetPlayoutData(_))
          .WillOnce(UpdateBuffer(i, kSamplesPer10Ms))
          .RetiresOnSaturation();
    }
  }
  FineAudioBuffer fine_buffer(&audio_device_buffer, kFrameSizeBytes,
                              sample_rate);

  rtc::scoped_ptr<int8_t[]> out_buffer;
  out_buffer.reset(
      new int8_t[fine_buffer.RequiredBufferSizeBytes()]);
  for (int i = 0; i < kNumberOfFrames; ++i) {
    fine_buffer.GetBufferData(out_buffer.get());
    EXPECT_TRUE(VerifyBuffer(out_buffer.get(), i, kFrameSizeBytes));
  }
}

TEST(FineBufferTest, BufferLessThan10ms) {
  const int kSampleRate = 44100;
  const int kSamplesPer10Ms = kSampleRate * 10 / 1000;
  const int kFrameSizeSamples = kSamplesPer10Ms - 50;
  RunFineBufferTest(kSampleRate, kFrameSizeSamples);
}

TEST(FineBufferTest, GreaterThan10ms) {
  const int kSampleRate = 44100;
  const int kSamplesPer10Ms = kSampleRate * 10 / 1000;
  const int kFrameSizeSamples = kSamplesPer10Ms + 50;
  RunFineBufferTest(kSampleRate, kFrameSizeSamples);
}

}  // namespace webrtc
