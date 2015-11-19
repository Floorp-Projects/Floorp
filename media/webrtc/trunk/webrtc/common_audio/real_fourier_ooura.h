/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_REAL_FOURIER_OOURA_H_
#define WEBRTC_COMMON_AUDIO_REAL_FOURIER_OOURA_H_

#include <complex>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/real_fourier.h"

namespace webrtc {

class RealFourierOoura : public RealFourier {
 public:
  explicit RealFourierOoura(int fft_order);

  void Forward(const float* src, std::complex<float>* dest) const override;
  void Inverse(const std::complex<float>* src, float* dest) const override;

  int order() const override {
    return order_;
  }

 private:
  const int order_;
  const int length_;
  const int complex_length_;
  // These are work arrays for Ooura. The names are based on the comments in
  // fft4g.c.
  const rtc::scoped_ptr<int[]> work_ip_;
  const rtc::scoped_ptr<float[]> work_w_;
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_REAL_FOURIER_OOURA_H_

