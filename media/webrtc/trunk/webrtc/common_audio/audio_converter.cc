/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/audio_converter.h"

#include <cstring>

#include "webrtc/base/checks.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/common_audio/resampler/push_sinc_resampler.h"
#include "webrtc/system_wrappers/interface/scoped_vector.h"

using rtc::checked_cast;

namespace webrtc {

class CopyConverter : public AudioConverter {
 public:
  CopyConverter(int src_channels, int src_frames, int dst_channels,
                int dst_frames)
      : AudioConverter(src_channels, src_frames, dst_channels, dst_frames) {}
  ~CopyConverter() override {};

  void Convert(const float* const* src, size_t src_size, float* const* dst,
               size_t dst_capacity) override {
    CheckSizes(src_size, dst_capacity);
    if (src != dst) {
      for (int i = 0; i < src_channels(); ++i)
        std::memcpy(dst[i], src[i], dst_frames() * sizeof(*dst[i]));
    }
  }
};

class UpmixConverter : public AudioConverter {
 public:
  UpmixConverter(int src_channels, int src_frames, int dst_channels,
                 int dst_frames)
      : AudioConverter(src_channels, src_frames, dst_channels, dst_frames) {}
  ~UpmixConverter() override {};

  void Convert(const float* const* src, size_t src_size, float* const* dst,
               size_t dst_capacity) override {
    CheckSizes(src_size, dst_capacity);
    for (int i = 0; i < dst_frames(); ++i) {
      const float value = src[0][i];
      for (int j = 0; j < dst_channels(); ++j)
        dst[j][i] = value;
    }
  }
};

class DownmixConverter : public AudioConverter {
 public:
  DownmixConverter(int src_channels, int src_frames, int dst_channels,
                   int dst_frames)
      : AudioConverter(src_channels, src_frames, dst_channels, dst_frames) {
  }
  ~DownmixConverter() override {};

  void Convert(const float* const* src, size_t src_size, float* const* dst,
               size_t dst_capacity) override {
    CheckSizes(src_size, dst_capacity);
    float* dst_mono = dst[0];
    for (int i = 0; i < src_frames(); ++i) {
      float sum = 0;
      for (int j = 0; j < src_channels(); ++j)
        sum += src[j][i];
      dst_mono[i] = sum / src_channels();
    }
  }
};

class ResampleConverter : public AudioConverter {
 public:
  ResampleConverter(int src_channels, int src_frames, int dst_channels,
                    int dst_frames)
      : AudioConverter(src_channels, src_frames, dst_channels, dst_frames) {
    resamplers_.reserve(src_channels);
    for (int i = 0; i < src_channels; ++i)
      resamplers_.push_back(new PushSincResampler(src_frames, dst_frames));
  }
  ~ResampleConverter() override {};

  void Convert(const float* const* src, size_t src_size, float* const* dst,
               size_t dst_capacity) override {
    CheckSizes(src_size, dst_capacity);
    for (size_t i = 0; i < resamplers_.size(); ++i)
      resamplers_[i]->Resample(src[i], src_frames(), dst[i], dst_frames());
  }

 private:
  ScopedVector<PushSincResampler> resamplers_;
};

// Apply a vector of converters in serial, in the order given. At least two
// converters must be provided.
class CompositionConverter : public AudioConverter {
 public:
  CompositionConverter(ScopedVector<AudioConverter> converters)
      : converters_(converters.Pass()) {
    CHECK_GE(converters_.size(), 2u);
    // We need an intermediate buffer after every converter.
    for (auto it = converters_.begin(); it != converters_.end() - 1; ++it)
      buffers_.push_back(new ChannelBuffer<float>((*it)->dst_frames(),
                                                  (*it)->dst_channels()));
  }
  ~CompositionConverter() override {};

  void Convert(const float* const* src, size_t src_size, float* const* dst,
               size_t dst_capacity) override {
    converters_.front()->Convert(src, src_size, buffers_.front()->channels(),
                                 buffers_.front()->size());
    for (size_t i = 2; i < converters_.size(); ++i) {
      auto src_buffer = buffers_[i - 2];
      auto dst_buffer = buffers_[i - 1];
      converters_[i]->Convert(src_buffer->channels(),
                              src_buffer->size(),
                              dst_buffer->channels(),
                              dst_buffer->size());
    }
    converters_.back()->Convert(buffers_.back()->channels(),
                                buffers_.back()->size(), dst, dst_capacity);
  }

 private:
  ScopedVector<AudioConverter> converters_;
  ScopedVector<ChannelBuffer<float>> buffers_;
};

rtc::scoped_ptr<AudioConverter> AudioConverter::Create(int src_channels,
                                                       int src_frames,
                                                       int dst_channels,
                                                       int dst_frames) {
  rtc::scoped_ptr<AudioConverter> sp;
  if (src_channels > dst_channels) {
    if (src_frames != dst_frames) {
      ScopedVector<AudioConverter> converters;
      converters.push_back(new DownmixConverter(src_channels, src_frames,
                                                dst_channels, src_frames));
      converters.push_back(new ResampleConverter(dst_channels, src_frames,
                                                 dst_channels, dst_frames));
      sp.reset(new CompositionConverter(converters.Pass()));
    } else {
      sp.reset(new DownmixConverter(src_channels, src_frames, dst_channels,
                                    dst_frames));
    }
  } else if (src_channels < dst_channels) {
    if (src_frames != dst_frames) {
      ScopedVector<AudioConverter> converters;
      converters.push_back(new ResampleConverter(src_channels, src_frames,
                                                 src_channels, dst_frames));
      converters.push_back(new UpmixConverter(src_channels, dst_frames,
                                              dst_channels, dst_frames));
      sp.reset(new CompositionConverter(converters.Pass()));
    } else {
      sp.reset(new UpmixConverter(src_channels, src_frames, dst_channels,
                                  dst_frames));
    }
  } else if (src_frames != dst_frames) {
    sp.reset(new ResampleConverter(src_channels, src_frames, dst_channels,
                                   dst_frames));
  } else {
    sp.reset(new CopyConverter(src_channels, src_frames, dst_channels,
                               dst_frames));
  }

  return sp.Pass();
}

// For CompositionConverter.
AudioConverter::AudioConverter()
    : src_channels_(0),
      src_frames_(0),
      dst_channels_(0),
      dst_frames_(0) {}

AudioConverter::AudioConverter(int src_channels, int src_frames,
                               int dst_channels, int dst_frames)
    : src_channels_(src_channels),
      src_frames_(src_frames),
      dst_channels_(dst_channels),
      dst_frames_(dst_frames) {
  CHECK(dst_channels == src_channels || dst_channels == 1 || src_channels == 1);
}

void AudioConverter::CheckSizes(size_t src_size, size_t dst_capacity) const {
  CHECK_EQ(src_size, checked_cast<size_t>(src_channels() * src_frames()));
  CHECK_GE(dst_capacity, checked_cast<size_t>(dst_channels() * dst_frames()));
}

}  // namespace webrtc
