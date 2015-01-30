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

#include <cstdlib>
#include <cstring>

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/real_fourier.h"

namespace webrtc {

void LappedTransform::BlockThunk::ProcessBlock(const float* const* input,
                                               int num_frames,
                                               int num_input_channels,
                                               int num_output_channels,
                                               float* const* output) {
  CHECK_EQ(num_input_channels, parent_->in_channels_);
  CHECK_EQ(num_output_channels, parent_->out_channels_);
  CHECK_EQ(parent_->block_length_, num_frames);

  for (int i = 0; i < num_input_channels; ++i) {
    memcpy(parent_->real_buf_.Row(i), input[i],
           num_frames * sizeof(*input[0]));
    parent_->fft_.Forward(parent_->real_buf_.Row(i), parent_->cplx_pre_.Row(i));
  }

  int block_length = RealFourier::ComplexLength(
      RealFourier::FftOrder(num_frames));
  CHECK_EQ(parent_->cplx_length_, block_length);
  parent_->block_processor_->ProcessAudioBlock(parent_->cplx_pre_.Array(),
                                               num_input_channels,
                                               parent_->cplx_length_,
                                               num_output_channels,
                                               parent_->cplx_post_.Array());

  for (int i = 0; i < num_output_channels; ++i) {
    parent_->fft_.Inverse(parent_->cplx_post_.Row(i),
                          parent_->real_buf_.Row(i));
    memcpy(output[i], parent_->real_buf_.Row(i),
           num_frames * sizeof(*input[0]));
  }
}

LappedTransform::LappedTransform(int in_channels, int out_channels,
                                 int chunk_length, const float* window,
                                 int block_length, int shift_amount,
                                 Callback* callback)
    : blocker_callback_(this),
      in_channels_(in_channels),
      out_channels_(out_channels),
      window_(window),
      own_window_(false),
      window_shift_amount_(shift_amount),
      block_length_(block_length),
      chunk_length_(chunk_length),
      block_processor_(callback),
      blocker_(nullptr),
      fft_(RealFourier::FftOrder(block_length_)),
      cplx_length_(RealFourier::ComplexLength(fft_.order())),
      real_buf_(in_channels, block_length, RealFourier::kFftBufferAlignment),
      cplx_pre_(in_channels, cplx_length_, RealFourier::kFftBufferAlignment),
      cplx_post_(out_channels, cplx_length_, RealFourier::kFftBufferAlignment) {
  CHECK(in_channels_ > 0 && out_channels_ > 0);
  CHECK_GT(block_length_, 0);
  CHECK_GT(chunk_length_, 0);
  CHECK(block_processor_);
  CHECK_EQ(0, block_length & (block_length - 1));  // block_length_ power of 2?

  if (!window_) {
    own_window_ = true;
    window_ = new float[block_length_];
    CHECK(window_ != nullptr);
    window_shift_amount_ = block_length_;
    float* temp = const_cast<float*>(window_);
    for (int i = 0; i < block_length_; ++i) {
      temp[i] = 1.0f;
    }
  }

  blocker_.reset(new Blocker(chunk_length_, block_length_, in_channels_,
                             out_channels_, window_, window_shift_amount_,
                             &blocker_callback_));
}

LappedTransform::~LappedTransform() {
  if (own_window_) {
    delete [] window_;
  }
}

void LappedTransform::ProcessChunk(const float* const* in_chunk,
                                   float* const* out_chunk) {
  blocker_->ProcessChunk(in_chunk, chunk_length_, in_channels_, out_channels_,
                         out_chunk);
}

}  // namespace webrtc

