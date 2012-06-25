/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_NS_INCLUDE_NOISE_SUPPRESSION_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_NS_INCLUDE_NOISE_SUPPRESSION_H_

#include "typedefs.h"

typedef struct NsHandleT NsHandle;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This function creates an instance to the noise reduction structure
 *
 * Input:
 *      - NS_inst       : Pointer to noise reduction instance that should be
 *                        created
 *
 * Output:
 *      - NS_inst       : Pointer to created noise reduction instance
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
int WebRtcNs_Create(NsHandle** NS_inst);


/*
 * This function frees the dynamic memory of a specified Noise Reduction
 * instance.
 *
 * Input:
 *      - NS_inst       : Pointer to NS instance that should be freed
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
int WebRtcNs_Free(NsHandle* NS_inst);


/*
 * This function initializes a NS instance
 *
 * Input:
 *      - NS_inst       : Instance that should be initialized
 *      - fs            : sampling frequency
 *
 * Output:
 *      - NS_inst       : Initialized instance
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
int WebRtcNs_Init(NsHandle* NS_inst, WebRtc_UWord32 fs);

/*
 * This changes the aggressiveness of the noise suppression method.
 *
 * Input:
 *      - NS_inst       : Instance that should be initialized
 *      - mode          : 0: Mild, 1: Medium , 2: Aggressive
 *
 * Output:
 *      - NS_inst       : Initialized instance
 *
 * Return value         :  0 - Ok
 *                        -1 - Error
 */
int WebRtcNs_set_policy(NsHandle* NS_inst, int mode);


/*
 * This functions does Noise Suppression for the inserted speech frame. The
 * input and output signals should always be 10ms (80 or 160 samples).
 *
 * Input
 *      - NS_inst       : NS Instance. Needs to be initiated before call.
 *      - spframe       : Pointer to speech frame buffer for L band
 *      - spframe_H     : Pointer to speech frame buffer for H band
 *      - fs            : sampling frequency
 *
 * Output:
 *      - NS_inst       : Updated NS instance
 *      - outframe      : Pointer to output frame for L band
 *      - outframe_H    : Pointer to output frame for H band
 *
 * Return value         :  0 - OK
 *                        -1 - Error
 */
int WebRtcNs_Process(NsHandle* NS_inst,
                     short* spframe,
                     short* spframe_H,
                     short* outframe,
                     short* outframe_H);

#ifdef __cplusplus
}
#endif

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_NS_INCLUDE_NOISE_SUPPRESSION_H_
