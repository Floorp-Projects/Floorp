/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/signal_processing/include/real_fft.h"

#include <stdlib.h>

#include "common_audio/signal_processing/include/signal_processing_library.h"

struct RealFFT {
  int order;
};

struct RealFFT* WebRtcSpl_CreateRealFFT(int order) {
  struct RealFFT* self = NULL;

  // This constraint comes from ComplexFFT().
  if (order > 10 || order < 0) {
    return NULL;
  }

  self = malloc(sizeof(struct RealFFT));
  self->order = order;

  return self;
}

void WebRtcSpl_FreeRealFFT(struct RealFFT* self) {
  free(self);
}

// WebRtcSpl_ComplexFFT and WebRtcSpl_ComplexIFFT use in-place algorithm,
// so copy data from data_in to data_out in the next two functions.

int WebRtcSpl_RealForwardFFTC(struct RealFFT* self,
                              const int16_t* data_in,
                              int16_t* data_out) {
  memcpy(data_out, data_in, sizeof(int16_t) * (1 << (self->order + 1)));
  WebRtcSpl_ComplexBitReverse(data_out, self->order);
  return WebRtcSpl_ComplexFFT(data_out, self->order, 1);
}

int WebRtcSpl_RealInverseFFTC(struct RealFFT* self,
                              const int16_t* data_in,
                              int16_t* data_out) {
  memcpy(data_out, data_in, sizeof(int16_t) * (1 << (self->order + 1)));
  WebRtcSpl_ComplexBitReverse(data_out, self->order);
  return WebRtcSpl_ComplexIFFT(data_out, self->order, 1);
}

#if defined(WEBRTC_DETECT_ARM_NEON) || defined(WEBRTC_ARCH_ARM_NEON)
// TODO(kma): Replace the following function bodies into optimized functions
// for ARM Neon.
int WebRtcSpl_RealForwardFFTNeon(struct RealFFT* self,
                                 const int16_t* data_in,
                                 int16_t* data_out) {
  return WebRtcSpl_RealForwardFFTC(self, data_in, data_out);
}

int WebRtcSpl_RealInverseFFTNeon(struct RealFFT* self,
                                 const int16_t* data_in,
                                 int16_t* data_out) {
  return WebRtcSpl_RealInverseFFTC(self, data_in, data_out);
}
#endif
