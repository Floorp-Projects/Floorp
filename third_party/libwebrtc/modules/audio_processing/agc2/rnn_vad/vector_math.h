/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_VECTOR_MATH_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_VECTOR_MATH_H_

#include <numeric>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/arch.h"

namespace webrtc {
namespace rnn_vad {

// Provides optimizations for mathematical operations having vectors as
// operand(s).
class VectorMath {
 public:
  explicit VectorMath(AvailableCpuFeatures cpu_features)
      : cpu_features_(cpu_features) {}

  // Computes the dot product between two equally sized vectors.
  float DotProduct(rtc::ArrayView<const float> x,
                   rtc::ArrayView<const float> y) const {
#if defined(WEBRTC_ARCH_X86_FAMILY)
    if (cpu_features_.avx2) {
      return DotProductAvx2(x, y);
    }
    // TODO(bugs.webrtc.org/10480): Add SSE2 alternative implementation.
#endif
    // TODO(bugs.webrtc.org/10480): Add NEON alternative implementation.
    RTC_DCHECK_EQ(x.size(), y.size());
    return std::inner_product(x.begin(), x.end(), y.begin(), 0.f);
  }

 private:
  float DotProductAvx2(rtc::ArrayView<const float> x,
                       rtc::ArrayView<const float> y) const;

  const AvailableCpuFeatures cpu_features_;
};

}  // namespace rnn_vad
}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_VECTOR_MATH_H_
