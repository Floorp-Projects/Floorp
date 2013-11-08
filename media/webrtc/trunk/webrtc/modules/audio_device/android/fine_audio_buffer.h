/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_FINE_AUDIO_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_FINE_AUDIO_BUFFER_H_

#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class AudioDeviceBuffer;

// FineAudioBuffer takes an AudioDeviceBuffer which delivers audio data
// corresponding to 10ms of data. It then allows for this data to be pulled in
// a finer or coarser granularity. I.e. interacting with this class instead of
// directly with the AudioDeviceBuffer one can ask for any number of audio data
// samples.
class FineAudioBuffer {
 public:
  // |device_buffer| is a buffer that provides 10ms of audio data.
  // |desired_frame_size_bytes| is the number of bytes of audio data
  // (not samples) |GetBufferData| should return on success.
  // |sample_rate| is the sample rate of the audio data. This is needed because
  // |device_buffer| delivers 10ms of data. Given the sample rate the number
  // of samples can be calculated.
  FineAudioBuffer(AudioDeviceBuffer* device_buffer,
                  int desired_frame_size_bytes,
                  int sample_rate);
  ~FineAudioBuffer();

  // Returns the required size of |buffer| when calling GetBufferData. If the
  // buffer is smaller memory trampling will happen.
  // |desired_frame_size_bytes| and |samples_rate| are as described in the
  // constructor.
  int RequiredBufferSizeBytes();

  // |buffer| must be of equal or greater size than what is returned by
  // RequiredBufferSize. This is to avoid unnecessary memcpy.
  void GetBufferData(int8_t* buffer);

 private:
  // Device buffer that provides 10ms chunks of data.
  AudioDeviceBuffer* device_buffer_;
  int desired_frame_size_bytes_;  // Number of bytes delivered per GetBufferData
  int sample_rate_;
  int samples_per_10_ms_;
  // Convenience parameter to avoid converting from samples
  int bytes_per_10_ms_;

  // Storage for samples that are not yet asked for.
  scoped_array<int8_t> cache_buffer_;
  int cached_buffer_start_;  // Location of first unread sample.
  int cached_bytes_;  // Number of bytes stored in cache.
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_FINE_AUDIO_BUFFER_H_
