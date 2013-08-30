/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_REAL_FFT_H_
#define WEBRTC_COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_REAL_FFT_H_

#include "webrtc/typedefs.h"

struct RealFFT;

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*RealForwardFFT)(struct RealFFT* self,
                              const int16_t* data_in,
                              int16_t* data_out);
typedef int (*RealInverseFFT)(struct RealFFT* self,
                              const int16_t* data_in,
                              int16_t* data_out);

extern RealForwardFFT WebRtcSpl_RealForwardFFT;
extern RealInverseFFT WebRtcSpl_RealInverseFFT;

struct RealFFT* WebRtcSpl_CreateRealFFT(int order);
void WebRtcSpl_FreeRealFFT(struct RealFFT* self);

// TODO(kma): Implement FFT functions for real signals.

// Compute the forward FFT for a complex signal of length 2^order.
// Input Arguments:
//   self - pointer to preallocated and initialized FFT specification structure.
//   data_in - the input signal.
//
// Output Arguments:
//   data_out - the output signal; must be different to data_in.
//
// Return Value:
//   0  - FFT calculation is successful.
//   -1 - Error
//
int WebRtcSpl_RealForwardFFTC(struct RealFFT* self,
                              const int16_t* data_in,
                              int16_t* data_out);

#if (defined WEBRTC_DETECT_ARM_NEON) || (defined WEBRTC_ARCH_ARM_NEON)
int WebRtcSpl_RealForwardFFTNeon(struct RealFFT* self,
                                 const int16_t* data_in,
                                 int16_t* data_out);
#endif

// Compute the inverse FFT for a complex signal of length 2^order.
// Input Arguments:
//   self - pointer to preallocated and initialized FFT specification structure.
//   data_in - the input signal.
//
// Output Arguments:
//   data_out - the output signal; must be different to data_in.
//
// Return Value:
//   0 or a positive number - a value that the elements in the |data_out| should
//                            be shifted left with in order to get correct
//                            physical values.
//   -1                     - Error
int WebRtcSpl_RealInverseFFTC(struct RealFFT* self,
                              const int16_t* data_in,
                              int16_t* data_out);

#if (defined WEBRTC_DETECT_ARM_NEON) || (defined WEBRTC_ARCH_ARM_NEON)
int WebRtcSpl_RealInverseFFTNeon(struct RealFFT* self,
                                 const int16_t* data_in,
                                 int16_t* data_out);
#endif

#ifdef __cplusplus
}
#endif

#endif  // WEBRTC_COMMON_AUDIO_SIGNAL_PROCESSING_INCLUDE_REAL_FFT_H_
