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

#include <memory.h>
#include <stdio.h>
#include <algorithm>

#include "webrtc/modules/audio_device/audio_device_buffer.h"

namespace webrtc {

FineAudioBuffer::FineAudioBuffer(AudioDeviceBuffer* device_buffer,
                                 int desired_frame_size_bytes,
                                 int sample_rate)
    : device_buffer_(device_buffer),
      desired_frame_size_bytes_(desired_frame_size_bytes),
      sample_rate_(sample_rate),
      samples_per_10_ms_(sample_rate_ * 10 / 1000),
      bytes_per_10_ms_(samples_per_10_ms_ * sizeof(int16_t)),
      cached_buffer_start_(0),
      cached_bytes_(0) {
  cache_buffer_.reset(new int8_t[bytes_per_10_ms_]);
}

FineAudioBuffer::~FineAudioBuffer() {
}

int FineAudioBuffer::RequiredBufferSizeBytes() {
  // It is possible that we store the desired frame size - 1 samples. Since new
  // audio frames are pulled in chunks of 10ms we will need a buffer that can
  // hold desired_frame_size - 1 + 10ms of data. We omit the - 1.
  return desired_frame_size_bytes_ + bytes_per_10_ms_;
}

void FineAudioBuffer::GetBufferData(int8_t* buffer) {
  if (desired_frame_size_bytes_ <= cached_bytes_) {
    memcpy(buffer, &cache_buffer_.get()[cached_buffer_start_],
           desired_frame_size_bytes_);
    cached_buffer_start_ += desired_frame_size_bytes_;
    cached_bytes_ -= desired_frame_size_bytes_;
    assert(cached_buffer_start_ + cached_bytes_ < bytes_per_10_ms_);
    return;
  }
  memcpy(buffer, &cache_buffer_.get()[cached_buffer_start_], cached_bytes_);
  // Push another n*10ms of audio to |buffer|. n > 1 if
  // |desired_frame_size_bytes_| is greater than 10ms of audio. Note that we
  // write the audio after the cached bytes copied earlier.
  int8_t* unwritten_buffer = &buffer[cached_bytes_];
  int bytes_left = desired_frame_size_bytes_ - cached_bytes_;
  // Ceiling of integer division: 1 + ((x - 1) / y)
  int number_of_requests = 1 + (bytes_left - 1) / (bytes_per_10_ms_);
  for (int i = 0; i < number_of_requests; ++i) {
    device_buffer_->RequestPlayoutData(samples_per_10_ms_);
    int num_out = device_buffer_->GetPlayoutData(unwritten_buffer);
    if (num_out != samples_per_10_ms_) {
      assert(num_out == 0);
      cached_bytes_ = 0;
      return;
    }
    unwritten_buffer += bytes_per_10_ms_;
    assert(bytes_left >= 0);
    bytes_left -= bytes_per_10_ms_;
  }
  assert(bytes_left <= 0);
  // Put the samples that were written to |buffer| but are not used in the
  // cache.
  int cache_location = desired_frame_size_bytes_;
  int8_t* cache_ptr = &buffer[cache_location];
  cached_bytes_ = number_of_requests * bytes_per_10_ms_ -
      (desired_frame_size_bytes_ - cached_bytes_);
  // If cached_bytes_ is larger than the cache buffer, uninitialized memory
  // will be read.
  assert(cached_bytes_ <= bytes_per_10_ms_);
  assert(-bytes_left == cached_bytes_);
  cached_buffer_start_ = 0;
  memcpy(cache_buffer_.get(), cache_ptr, cached_bytes_);
}

}  // namespace webrtc
