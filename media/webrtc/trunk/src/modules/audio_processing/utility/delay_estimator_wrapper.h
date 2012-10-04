/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Performs delay estimation on block by block basis.
// The return value is  0 - OK and -1 - Error, unless otherwise stated.

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_DELAY_ESTIMATOR_WRAPPER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_DELAY_ESTIMATOR_WRAPPER_H_

#include "typedefs.h"

// Releases the memory allocated by WebRtc_CreateDelayEstimator(...)
// Input:
//      - handle        : Pointer to the delay estimation instance.
//
void WebRtc_FreeDelayEstimator(void* handle);

// Allocates the memory needed by the delay estimation. The memory needs to be
// initialized separately through WebRtc_InitDelayEstimator(...).
//
// Inputs:
//      - spectrum_size : Size of the spectrum used both in far-end and
//                        near-end. Used to allocate memory for spectrum
//                        specific buffers.
//      - max_delay     : The maximum delay which can be estimated. Needed to
//                        allocate memory for history buffers.
//      - lookahead     : Amount of non-causal lookahead to use. This can
//                        detect cases in which a near-end signal occurs before
//                        the corresponding far-end signal. It will delay the
//                        estimate for the current block by an equal amount,
//                        and the returned values will be offset by it.
//
//                        A value of zero is the typical no-lookahead case.
//                        This also represents the minimum delay which can be
//                        estimated.
//
// Return value:
//      - void*         : Created |handle|. If the memory can't be allocated or
//                        if any of the input parameters are invalid NULL is
//                        returned.
//
void* WebRtc_CreateDelayEstimator(int spectrum_size, int max_delay,
                                  int lookahead);

// Initializes the delay estimation instance returned by
// WebRtc_CreateDelayEstimator(...)
// Input:
//      - handle        : Pointer to the delay estimation instance.
//
// Output:
//      - handle        : Initialized instance.
//
int WebRtc_InitDelayEstimator(void* handle);

// Estimates and returns the delay between the far-end and near-end blocks. The
// value will be offset by the lookahead (i.e. the lookahead should be
// subtracted from the returned value).
// Inputs:
//      - handle        : Pointer to the delay estimation instance.
//      - far_spectrum  : Pointer to the far-end spectrum data.
//      - near_spectrum : Pointer to the near-end spectrum data of the current
//                        block.
//      - spectrum_size : The size of the data arrays (same for both far- and
//                        near-end).
//      - far_q         : The Q-domain of the far-end data.
//      - near_q        : The Q-domain of the near-end data.
//
// Output:
//      - handle        : Updated instance.
//
// Return value:
//      - delay         :  >= 0 - Calculated delay value.
//                        -1    - Error.
//                        -2    - Insufficient data for estimation.
//
int WebRtc_DelayEstimatorProcessFix(void* handle,
                                    uint16_t* far_spectrum,
                                    uint16_t* near_spectrum,
                                    int spectrum_size,
                                    int far_q,
                                    int near_q);

// See WebRtc_DelayEstimatorProcessFix() for description.
int WebRtc_DelayEstimatorProcessFloat(void* handle,
                                      float* far_spectrum,
                                      float* near_spectrum,
                                      int spectrum_size);

// Returns the last calculated delay updated by the function
// WebRtc_DelayEstimatorProcess(...).
//
// Input:
//      - handle        : Pointer to the delay estimation instance.
//
// Return value:
//      - delay         :  >= 0 - Last calculated delay value.
//                        -1    - Error.
//                        -2    - Insufficient data for estimation.
//
int WebRtc_last_delay(void* handle);

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_UTILITY_DELAY_ESTIMATOR_WRAPPER_H_
