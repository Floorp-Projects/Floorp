/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_REAL_FOURIER_H_
#define WEBRTC_COMMON_AUDIO_REAL_FOURIER_H_

#include <complex>

#include "webrtc/system_wrappers/interface/aligned_malloc.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

// Uniform interface class for the real DFT and its inverse, for power-of-2
// input lengths. Also contains helper functions for buffer allocation, taking
// care of any memory alignment requirements the underlying library might have.

namespace webrtc {

class RealFourier {
 public:
  // Shorthand typenames for the scopers used by the buffer allocation helpers.
  typedef scoped_ptr<float[], AlignedFreeDeleter> fft_real_scoper;
  typedef scoped_ptr<std::complex<float>[], AlignedFreeDeleter> fft_cplx_scoper;

  // The maximum input order supported by this implementation.
  static const int kMaxFftOrder;

  // The alignment required for all input and output buffers, in bytes.
  static const int kFftBufferAlignment;

  // Construct a wrapper instance for the given input order, which must be
  // between 1 and kMaxFftOrder, inclusively.
  explicit RealFourier(int fft_order);
  ~RealFourier();

  // Short helper to compute the smallest FFT order (a power of 2) which will
  // contain the given input length. Returns -1 if the order would have been
  // too big for the implementation.
  static int FftOrder(int length);

  // Short helper to compute the exact length, in complex floats, of the
  // transform output (i.e. |2^order / 2 + 1|).
  static int ComplexLength(int order);

  // Buffer allocation helpers. The buffers are large enough to hold |count|
  // floats/complexes and suitably aligned for use by the implementation.
  // The returned scopers are set up with proper deleters; the caller owns
  // the allocated memory.
  static fft_real_scoper AllocRealBuffer(int count);
  static fft_cplx_scoper AllocCplxBuffer(int count);

  // Main forward transform interface. The output array need only be big
  // enough for |2^order / 2 + 1| elements - the conjugate pairs are not
  // returned. Input and output must be properly aligned (e.g. through
  // AllocRealBuffer and AllocCplxBuffer) and input length must be
  // |2^order| (same as given at construction time).
  void Forward(const float* src, std::complex<float>* dest) const;

  // Inverse transform. Same input format as output above, conjugate pairs
  // not needed.
  void Inverse(const std::complex<float>* src, float* dest) const;

  int order() const {
    return order_;
  }

 private:
  // Basically a forward declare of OMXFFTSpec_R_F32. To get rid of the
  // dependency on openmax.
  typedef void OMXFFTSpec_R_F32_;
  const int order_;

  OMXFFTSpec_R_F32_* omx_spec_;
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_REAL_FOURIER_H_

