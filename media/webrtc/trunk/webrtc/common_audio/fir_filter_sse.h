/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_FIR_FILTER_SSE_H_
#define WEBRTC_COMMON_AUDIO_FIR_FILTER_SSE_H_

#include "webrtc/common_audio/fir_filter.h"
#include "webrtc/system_wrappers/interface/aligned_malloc.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class FIRFilterSSE2 : public FIRFilter {
 public:
  FIRFilterSSE2(const float* coefficients,
                size_t coefficients_length,
                size_t max_input_length);

  virtual void Filter(const float* in, size_t length, float* out) OVERRIDE;

 private:
  size_t coefficients_length_;
  size_t state_length_;
  scoped_ptr<float[], AlignedFreeDeleter> coefficients_;
  scoped_ptr<float[], AlignedFreeDeleter> state_;
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_FIR_FILTER_SSE_H_
