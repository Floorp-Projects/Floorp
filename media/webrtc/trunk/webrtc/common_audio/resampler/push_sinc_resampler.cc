/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/common_audio/resampler/push_sinc_resampler.h"

#include <string.h>

namespace webrtc {

PushSincResampler::PushSincResampler(int source_frames,
                                     int destination_frames)
    : resampler_(new SincResampler(source_frames * 1.0 / destination_frames,
                                   source_frames, this)),
      float_buffer_(new float[destination_frames]),
      source_ptr_(NULL),
      destination_frames_(destination_frames),
      first_pass_(true),
      source_available_(0) {
}

PushSincResampler::~PushSincResampler() {
}

int PushSincResampler::Resample(const int16_t* source,
                                int source_length,
                                int16_t* destination,
                                int destination_capacity) {
  assert(source_length == resampler_->request_frames());
  assert(destination_capacity >= destination_frames_);
  // Cache the source pointer. Calling Resample() will immediately trigger
  // the Run() callback whereupon we provide the cached value.
  source_ptr_ = source;
  source_available_ = source_length;

  // On the first pass, we call Resample() twice. During the first call, we
  // provide dummy input and discard the output. This is done to prime the
  // SincResampler buffer with the correct delay (half the kernel size), thereby
  // ensuring that all later Resample() calls will only result in one input
  // request through Run().
  //
  // If this wasn't done, SincResampler would call Run() twice on the first
  // pass, and we'd have to introduce an entire |source_frames| of delay, rather
  // than the minimum half kernel.
  //
  // It works out that ChunkSize() is exactly the amount of output we need to
  // request in order to prime the buffer with a single Run() request for
  // |source_frames|.
  if (first_pass_)
    resampler_->Resample(resampler_->ChunkSize(), float_buffer_.get());

  resampler_->Resample(destination_frames_, float_buffer_.get());
  for (int i = 0; i < destination_frames_; ++i)
    destination[i] = RoundToInt16(ClampInt16(float_buffer_[i]));
  source_ptr_ = NULL;
  return destination_frames_;
}

void PushSincResampler::Run(int frames, float* destination) {
  assert(source_ptr_ != NULL);
  // Ensure we are only asked for the available samples. This would fail if
  // Run() was triggered more than once per Resample() call.
  assert(source_available_ == frames);

  if (first_pass_) {
    // Provide dummy input on the first pass, the output of which will be
    // discarded, as described in Resample().
    memset(destination, 0, frames * sizeof(float));
    first_pass_ = false;
  } else {
    for (int i = 0; i < frames; ++i)
      destination[i] = static_cast<float>(source_ptr_[i]);
    source_available_ -= frames;
  }
}

}  // namespace webrtc
