/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_COMMON_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_COMMON_H_

#include <assert.h>
#include <string.h>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

static inline int ChannelsFromLayout(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
    case AudioProcessing::kMonoAndKeyboard:
      return 1;
    case AudioProcessing::kStereo:
    case AudioProcessing::kStereoAndKeyboard:
      return 2;
  }
  assert(false);
  return -1;
}

// Helper to encapsulate a contiguous data buffer with access to a pointer
// array of the deinterleaved channels.
template <typename T>
class ChannelBuffer {
 public:
  ChannelBuffer(int samples_per_channel, int num_channels)
      : data_(new T[samples_per_channel * num_channels]),
        channels_(new T*[num_channels]),
        samples_per_channel_(samples_per_channel),
        num_channels_(num_channels) {
    Initialize();
  }

  ChannelBuffer(const T* data, int samples_per_channel, int num_channels)
      : data_(new T[samples_per_channel * num_channels]),
        channels_(new T*[num_channels]),
        samples_per_channel_(samples_per_channel),
        num_channels_(num_channels) {
    Initialize();
    memcpy(data_.get(), data, length() * sizeof(T));
  }

  ChannelBuffer(const T* const* channels, int samples_per_channel,
                int num_channels)
      : data_(new T[samples_per_channel * num_channels]),
        channels_(new T*[num_channels]),
        samples_per_channel_(samples_per_channel),
        num_channels_(num_channels) {
    Initialize();
    for (int i = 0; i < num_channels_; ++i)
      CopyFrom(channels[i], i);
  }

  ~ChannelBuffer() {}

  void CopyFrom(const void* channel_ptr, int i) {
    DCHECK_LT(i, num_channels_);
    memcpy(channels_[i], channel_ptr, samples_per_channel_ * sizeof(T));
  }

  T* data() { return data_.get(); }
  const T* data() const { return data_.get(); }

  const T* channel(int i) const {
    DCHECK_GE(i, 0);
    DCHECK_LT(i, num_channels_);
    return channels_[i];
  }
  T* channel(int i) {
    const ChannelBuffer<T>* t = this;
    return const_cast<T*>(t->channel(i));
  }

  T* const* channels() { return channels_.get(); }
  const T* const* channels() const { return channels_.get(); }

  int samples_per_channel() const { return samples_per_channel_; }
  int num_channels() const { return num_channels_; }
  int length() const { return samples_per_channel_ * num_channels_; }

 private:
  void Initialize() {
    memset(data_.get(), 0, sizeof(T) * length());
    for (int i = 0; i < num_channels_; ++i)
      channels_[i] = &data_[i * samples_per_channel_];
  }

  scoped_ptr<T[]> data_;
  scoped_ptr<T*[]> channels_;
  const int samples_per_channel_;
  const int num_channels_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_COMMON_H_
