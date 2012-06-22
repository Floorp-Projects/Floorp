/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This header file includes the VAD API calls. Specific function calls are given below.
 */

#ifndef WEBRTC_COMMON_AUDIO_VAD_INCLUDE_WEBRTC_VAD_H_
#define WEBRTC_COMMON_AUDIO_VAD_INCLUDE_WEBRTC_VAD_H_

#include <stdlib.h>

#include "typedefs.h"

typedef struct WebRtcVadInst VadInst;

#ifdef __cplusplus
extern "C" {
#endif

// TODO(bjornv): Investigate if we need the Assign calls below at all.

// Gets the size needed for storing the instance for the VAD.
//
// returns  : The size in bytes needed to allocate memory for the VAD instance.
size_t WebRtcVad_AssignSize();

// Assigns memory for the instances at a given address. It is assumed that the
// memory for the VAD instance is allocated at |memory| in accordance with
// WebRtcVad_AssignSize().
//
// - memory [i] : Address to where the memory is assigned.
// - handle [o] : Pointer to the instance that should be created.
//
// returns      : 0 - (OK), -1 (NULL pointer in)
int WebRtcVad_Assign(void* memory, VadInst** handle);

// Creates an instance to the VAD structure.
//
// - handle [o] : Pointer to the VAD instance that should be created.
//
// returns      : 0 - (OK), -1 - (Error)
int WebRtcVad_Create(VadInst** handle);

// Frees the dynamic memory of a specified VAD instance.
//
// - handle [i] : Pointer to VAD instance that should be freed.
//
// returns      : 0 - (OK), -1 - (NULL pointer in)
int WebRtcVad_Free(VadInst* handle);

// Initializes a VAD instance.
//
// - handle [i/o] : Instance that should be initialized.
//
// returns        : 0 - (OK),
//                 -1 - (NULL pointer or Default mode could not be set).
int WebRtcVad_Init(VadInst* handle);

// Sets the VAD operating mode. A more aggressive (higher mode) VAD is more
// restrictive in reporting speech. Put in other words the probability of being
// speech when the VAD returns 1 is increased with increasing mode. As a
// consequence also the missed detection rate goes up.
//
// - handle [i/o] : VAD instance.
// - mode   [i]   : Aggressiveness mode (0, 1, 2, or 3).
//
// returns        : 0 - (OK),
//                 -1 - (NULL pointer, mode could not be set or the VAD instance
//                       has not been initialized).
int WebRtcVad_set_mode(VadInst* handle, int mode);

/****************************************************************************
 * WebRtcVad_Process(...)
 * 
 * This functions does a VAD for the inserted speech frame
 *
 * Input
 *        - vad_inst     : VAD Instance. Needs to be initiated before call.
 *        - fs           : sampling frequency (Hz): 8000, 16000, or 32000
 *        - speech_frame : Pointer to speech frame buffer
 *        - frame_length : Length of speech frame buffer in number of samples
 *
 * Output:
 *        - vad_inst     : Updated VAD instance
 *
 * Return value          :  1 - Active Voice
 *                          0 - Non-active Voice
 *                         -1 - Error
 */
int16_t WebRtcVad_Process(VadInst* vad_inst, int16_t fs, int16_t* speech_frame,
                          int16_t frame_length);

#ifdef __cplusplus
}
#endif

#endif  // WEBRTC_COMMON_AUDIO_VAD_INCLUDE_WEBRTC_VAD_H_
