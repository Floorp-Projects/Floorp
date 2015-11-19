/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_INCLUDE_AUDIO_UTIL_H_
#define WEBRTC_COMMON_AUDIO_INCLUDE_AUDIO_UTIL_H_

#include <limits>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

typedef std::numeric_limits<int16_t> limits_int16;

// The conversion functions use the following naming convention:
// S16:      int16_t [-32768, 32767]
// Float:    float   [-1.0, 1.0]
// FloatS16: float   [-32768.0, 32767.0]
static inline int16_t FloatToS16(float v) {
  if (v > 0)
    return v >= 1 ? limits_int16::max() :
                    static_cast<int16_t>(v * limits_int16::max() + 0.5f);
  return v <= -1 ? limits_int16::min() :
                   static_cast<int16_t>(-v * limits_int16::min() - 0.5f);
}

static inline float S16ToFloat(int16_t v) {
  static const float kMaxInt16Inverse = 1.f / limits_int16::max();
  static const float kMinInt16Inverse = 1.f / limits_int16::min();
  return v * (v > 0 ? kMaxInt16Inverse : -kMinInt16Inverse);
}

static inline int16_t FloatS16ToS16(float v) {
  static const float kMaxRound = limits_int16::max() - 0.5f;
  static const float kMinRound = limits_int16::min() + 0.5f;
  if (v > 0)
    return v >= kMaxRound ? limits_int16::max() :
                            static_cast<int16_t>(v + 0.5f);
  return v <= kMinRound ? limits_int16::min() :
                          static_cast<int16_t>(v - 0.5f);
}

static inline float FloatToFloatS16(float v) {
  return v * (v > 0 ? limits_int16::max() : -limits_int16::min());
}

static inline float FloatS16ToFloat(float v) {
  static const float kMaxInt16Inverse = 1.f / limits_int16::max();
  static const float kMinInt16Inverse = 1.f / limits_int16::min();
  return v * (v > 0 ? kMaxInt16Inverse : -kMinInt16Inverse);
}

void FloatToS16(const float* src, size_t size, int16_t* dest);
void S16ToFloat(const int16_t* src, size_t size, float* dest);
void FloatS16ToS16(const float* src, size_t size, int16_t* dest);
void FloatToFloatS16(const float* src, size_t size, float* dest);
void FloatS16ToFloat(const float* src, size_t size, float* dest);

// Deinterleave audio from |interleaved| to the channel buffers pointed to
// by |deinterleaved|. There must be sufficient space allocated in the
// |deinterleaved| buffers (|num_channel| buffers with |samples_per_channel|
// per buffer).
template <typename T>
void Deinterleave(const T* interleaved, int samples_per_channel,
                  int num_channels, T* const* deinterleaved) {
  for (int i = 0; i < num_channels; ++i) {
    T* channel = deinterleaved[i];
    int interleaved_idx = i;
    for (int j = 0; j < samples_per_channel; ++j) {
      channel[j] = interleaved[interleaved_idx];
      interleaved_idx += num_channels;
    }
  }
}

// Interleave audio from the channel buffers pointed to by |deinterleaved| to
// |interleaved|. There must be sufficient space allocated in |interleaved|
// (|samples_per_channel| * |num_channels|).
template <typename T>
void Interleave(const T* const* deinterleaved, int samples_per_channel,
                int num_channels, T* interleaved) {
  for (int i = 0; i < num_channels; ++i) {
    const T* channel = deinterleaved[i];
    int interleaved_idx = i;
    for (int j = 0; j < samples_per_channel; ++j) {
      interleaved[interleaved_idx] = channel[j];
      interleaved_idx += num_channels;
    }
  }
}

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_INCLUDE_AUDIO_UTIL_H_
