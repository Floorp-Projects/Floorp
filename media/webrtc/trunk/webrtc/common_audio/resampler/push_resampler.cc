/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/resampler/include/push_resampler.h"

#include <string.h>

#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/resampler/push_sinc_resampler.h"

namespace webrtc {

PushResampler::PushResampler()
    : src_sample_rate_hz_(0),
      dst_sample_rate_hz_(0),
      num_channels_(0),
      src_left_(NULL),
      src_right_(NULL),
      dst_left_(NULL),
      dst_right_(NULL) {
}

PushResampler::~PushResampler() {
}

int PushResampler::InitializeIfNeeded(int src_sample_rate_hz,
                                      int dst_sample_rate_hz,
                                      int num_channels) {
  if (src_sample_rate_hz == src_sample_rate_hz_ &&
      dst_sample_rate_hz == dst_sample_rate_hz_ &&
      num_channels == num_channels_)
    // No-op if settings haven't changed.
    return 0;

  if (src_sample_rate_hz <= 0 || dst_sample_rate_hz <= 0 ||
      num_channels <= 0 || num_channels > 2)
    return -1;

  src_sample_rate_hz_ = src_sample_rate_hz;
  dst_sample_rate_hz_ = dst_sample_rate_hz;
  num_channels_ = num_channels;

  const int src_size_10ms_mono = src_sample_rate_hz / 100;
  const int dst_size_10ms_mono = dst_sample_rate_hz / 100;
  sinc_resampler_.reset(new PushSincResampler(src_size_10ms_mono,
                                              dst_size_10ms_mono));
  if (num_channels_ == 2) {
    src_left_.reset(new int16_t[src_size_10ms_mono]);
    src_right_.reset(new int16_t[src_size_10ms_mono]);
    dst_left_.reset(new int16_t[dst_size_10ms_mono]);
    dst_right_.reset(new int16_t[dst_size_10ms_mono]);
    sinc_resampler_right_.reset(new PushSincResampler(src_size_10ms_mono,
                                                      dst_size_10ms_mono));
  }

  return 0;
}

int PushResampler::Resample(const int16_t* src, int src_length,
                            int16_t* dst, int dst_capacity) {
  const int src_size_10ms = src_sample_rate_hz_ * num_channels_ / 100;
  const int dst_size_10ms = dst_sample_rate_hz_ * num_channels_ / 100;
  if (src_length != src_size_10ms || dst_capacity < dst_size_10ms)
    return -1;

  if (src_sample_rate_hz_ == dst_sample_rate_hz_) {
    // The old resampler provides this memcpy facility in the case of matching
    // sample rates, so reproduce it here for the sinc resampler.
    memcpy(dst, src, src_length * sizeof(int16_t));
    return src_length;
  }
  if (num_channels_ == 2) {
    const int src_length_mono = src_length / num_channels_;
    const int dst_capacity_mono = dst_capacity / num_channels_;
    int16_t* deinterleaved[] = {src_left_.get(), src_right_.get()};
    Deinterleave(src, src_length_mono, num_channels_, deinterleaved);

    int dst_length_mono =
        sinc_resampler_->Resample(src_left_.get(), src_length_mono,
                                  dst_left_.get(), dst_capacity_mono);
    sinc_resampler_right_->Resample(src_right_.get(), src_length_mono,
                                    dst_right_.get(), dst_capacity_mono);

    deinterleaved[0] = dst_left_.get();
    deinterleaved[1] = dst_right_.get();
    Interleave(deinterleaved, dst_length_mono, num_channels_, dst);
    return dst_length_mono * num_channels_;
  } else {
    return sinc_resampler_->Resample(src, src_length, dst, dst_capacity);
  }
}

}  // namespace webrtc
