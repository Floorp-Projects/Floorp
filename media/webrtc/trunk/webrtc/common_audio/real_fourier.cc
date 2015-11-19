/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/real_fourier.h"

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/real_fourier_ooura.h"
#include "webrtc/common_audio/real_fourier_openmax.h"
#include "webrtc/common_audio/signal_processing/include/spl_inl.h"

namespace webrtc {

using std::complex;

const int RealFourier::kFftBufferAlignment = 32;

rtc::scoped_ptr<RealFourier> RealFourier::Create(int fft_order) {
#if defined(RTC_USE_OPENMAX_DL)
  return rtc::scoped_ptr<RealFourier>(new RealFourierOpenmax(fft_order));
#else
  return rtc::scoped_ptr<RealFourier>(new RealFourierOoura(fft_order));
#endif
}

int RealFourier::FftOrder(int length) {
  CHECK_GT(length, 0);
  return WebRtcSpl_GetSizeInBits(length - 1);
}

int RealFourier::FftLength(int order) {
  CHECK_GE(order, 0);
  return 1 << order;
}

int RealFourier::ComplexLength(int order) {
  CHECK_GE(order, 0);
  return (1 << order) / 2 + 1;
}

RealFourier::fft_real_scoper RealFourier::AllocRealBuffer(int count) {
  return fft_real_scoper(static_cast<float*>(
      AlignedMalloc(sizeof(float) * count, kFftBufferAlignment)));
}

RealFourier::fft_cplx_scoper RealFourier::AllocCplxBuffer(int count) {
  return fft_cplx_scoper(static_cast<complex<float>*>(
      AlignedMalloc(sizeof(complex<float>) * count, kFftBufferAlignment)));
}

}  // namespace webrtc

