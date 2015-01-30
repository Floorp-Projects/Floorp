/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/audio_converter.h"
#include "webrtc/common_audio/resampler/push_sinc_resampler.h"

namespace webrtc {
namespace {

void DownmixToMono(const float* const* src,
                   int src_channels,
                   int frames,
                   float* dst) {
  DCHECK_GT(src_channels, 0);
  for (int i = 0; i < frames; ++i) {
    float sum = 0;
    for (int j = 0; j < src_channels; ++j)
      sum += src[j][i];
    dst[i] = sum / src_channels;
  }
}

void UpmixFromMono(const float* src,
                   int dst_channels,
                   int frames,
                   float* const* dst) {
  DCHECK_GT(dst_channels, 0);
  for (int i = 0; i < frames; ++i) {
    float value = src[i];
    for (int j = 0; j < dst_channels; ++j)
      dst[j][i] = value;
  }
}

}  // namespace

AudioConverter::AudioConverter(int src_channels, int src_frames,
                               int dst_channels, int dst_frames)
    : src_channels_(src_channels),
      src_frames_(src_frames),
      dst_channels_(dst_channels),
      dst_frames_(dst_frames) {
  CHECK(dst_channels == src_channels || dst_channels == 1 || src_channels == 1);
  const int resample_channels = std::min(src_channels, dst_channels);

  // Prepare buffers as needed for intermediate stages.
  if (dst_channels < src_channels)
    downmix_buffer_.reset(new ChannelBuffer<float>(src_frames,
                                                   resample_channels));

  if (src_frames != dst_frames) {
    resamplers_.reserve(resample_channels);
    for (int i = 0; i < resample_channels; ++i)
      resamplers_.push_back(new PushSincResampler(src_frames, dst_frames));
  }
}

void AudioConverter::Convert(const float* const* src,
                             int src_channels,
                             int src_frames,
                             int dst_channels,
                             int dst_frames,
                             float* const* dst) {
  DCHECK_EQ(src_channels_, src_channels);
  DCHECK_EQ(src_frames_, src_frames);
  DCHECK_EQ(dst_channels_, dst_channels);
  DCHECK_EQ(dst_frames_, dst_frames);;

  if (src_channels == dst_channels && src_frames == dst_frames) {
    // Shortcut copy.
    if (src != dst) {
      for (int i = 0; i < src_channels; ++i)
        memcpy(dst[i], src[i], dst_frames * sizeof(*dst[i]));
    }
    return;
  }

  const float* const* src_ptr = src;
  if (dst_channels < src_channels) {
    float* const* dst_ptr = dst;
    if (src_frames != dst_frames) {
      // Downmix to a buffer for subsequent resampling.
      DCHECK_EQ(downmix_buffer_->num_channels(), dst_channels);
      DCHECK_EQ(downmix_buffer_->samples_per_channel(), src_frames);
      dst_ptr = downmix_buffer_->channels();
    }

    DownmixToMono(src, src_channels, src_frames, dst_ptr[0]);
    src_ptr = dst_ptr;
  }

  if (src_frames != dst_frames) {
    for (size_t i = 0; i < resamplers_.size(); ++i)
      resamplers_[i]->Resample(src_ptr[i], src_frames, dst[i], dst_frames);
    src_ptr = dst;
  }

  if (dst_channels > src_channels)
    UpmixFromMono(src_ptr[0], dst_channels, dst_frames, dst);
}

}  // namespace webrtc
