/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AECM_INCLUDE_ECHO_CONTROL_MOBILE_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AECM_INCLUDE_ECHO_CONTROL_MOBILE_H_

#include "typedefs.h"

enum {
    AecmFalse = 0,
    AecmTrue
};

// Errors
#define AECM_UNSPECIFIED_ERROR           12000
#define AECM_UNSUPPORTED_FUNCTION_ERROR  12001
#define AECM_UNINITIALIZED_ERROR         12002
#define AECM_NULL_POINTER_ERROR          12003
#define AECM_BAD_PARAMETER_ERROR         12004

// Warnings
#define AECM_BAD_PARAMETER_WARNING       12100

typedef struct {
    WebRtc_Word16 cngMode;            // AECM_FALSE, AECM_TRUE (default)
    WebRtc_Word16 echoMode;           // 0, 1, 2, 3 (default), 4
} AecmConfig;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Allocates the memory needed by the AECM. The memory needs to be
 * initialized separately using the WebRtcAecm_Init() function.
 *
 * Inputs                           Description
 * -------------------------------------------------------------------
 * void **aecmInst                  Pointer to the AECM instance to be
 *                                  created and initialized
 *
 * Outputs                          Description
 * -------------------------------------------------------------------
 * WebRtc_Word32 return             0: OK
 *                                 -1: error
 */
WebRtc_Word32 WebRtcAecm_Create(void **aecmInst);

/*
 * This function releases the memory allocated by WebRtcAecm_Create()
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void *aecmInst               Pointer to the AECM instance
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * WebRtc_Word32  return        0: OK
 *                             -1: error
 */
WebRtc_Word32 WebRtcAecm_Free(void *aecmInst);

/*
 * Initializes an AECM instance.
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void           *aecmInst     Pointer to the AECM instance
 * WebRtc_Word32  sampFreq      Sampling frequency of data
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * WebRtc_Word32  return        0: OK
 *                             -1: error
 */
WebRtc_Word32 WebRtcAecm_Init(void* aecmInst,
                              WebRtc_Word32 sampFreq);

/*
 * Inserts an 80 or 160 sample block of data into the farend buffer.
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void           *aecmInst     Pointer to the AECM instance
 * WebRtc_Word16  *farend       In buffer containing one frame of
 *                              farend signal
 * WebRtc_Word16  nrOfSamples   Number of samples in farend buffer
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * WebRtc_Word32  return        0: OK
 *                             -1: error
 */
WebRtc_Word32 WebRtcAecm_BufferFarend(void* aecmInst,
                                      const WebRtc_Word16* farend,
                                      WebRtc_Word16 nrOfSamples);

/*
 * Runs the AECM on an 80 or 160 sample blocks of data.
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void           *aecmInst      Pointer to the AECM instance
 * WebRtc_Word16  *nearendNoisy  In buffer containing one frame of
 *                               reference nearend+echo signal. If
 *                               noise reduction is active, provide
 *                               the noisy signal here.
 * WebRtc_Word16  *nearendClean  In buffer containing one frame of
 *                               nearend+echo signal. If noise
 *                               reduction is active, provide the
 *                               clean signal here. Otherwise pass a
 *                               NULL pointer.
 * WebRtc_Word16  nrOfSamples    Number of samples in nearend buffer
 * WebRtc_Word16  msInSndCardBuf Delay estimate for sound card and
 *                               system buffers
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * WebRtc_Word16  *out          Out buffer, one frame of processed nearend
 * WebRtc_Word32  return        0: OK
 *                             -1: error
 */
WebRtc_Word32 WebRtcAecm_Process(void* aecmInst,
                                 const WebRtc_Word16* nearendNoisy,
                                 const WebRtc_Word16* nearendClean,
                                 WebRtc_Word16* out,
                                 WebRtc_Word16 nrOfSamples,
                                 WebRtc_Word16 msInSndCardBuf);

/*
 * This function enables the user to set certain parameters on-the-fly
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void     *aecmInst           Pointer to the AECM instance
 * AecmConfig config            Config instance that contains all
 *                              properties to be set
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * WebRtc_Word32  return        0: OK
 *                             -1: error
 */
WebRtc_Word32 WebRtcAecm_set_config(void* aecmInst,
                                    AecmConfig config);

/*
 * This function enables the user to set certain parameters on-the-fly
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void *aecmInst               Pointer to the AECM instance
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * AecmConfig  *config          Pointer to the config instance that
 *                              all properties will be written to
 * WebRtc_Word32  return        0: OK
 *                             -1: error
 */
WebRtc_Word32 WebRtcAecm_get_config(void *aecmInst,
                                    AecmConfig *config);

/*
 * This function enables the user to set the echo path on-the-fly.
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void*        aecmInst        Pointer to the AECM instance
 * void*        echo_path       Pointer to the echo path to be set
 * size_t       size_bytes      Size in bytes of the echo path
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * WebRtc_Word32  return        0: OK
 *                             -1: error
 */
WebRtc_Word32 WebRtcAecm_InitEchoPath(void* aecmInst,
                                      const void* echo_path,
                                      size_t size_bytes);

/*
 * This function enables the user to get the currently used echo path
 * on-the-fly
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void*        aecmInst        Pointer to the AECM instance
 * void*        echo_path       Pointer to echo path
 * size_t       size_bytes      Size in bytes of the echo path
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * WebRtc_Word32  return        0: OK
 *                             -1: error
 */
WebRtc_Word32 WebRtcAecm_GetEchoPath(void* aecmInst,
                                     void* echo_path,
                                     size_t size_bytes);

/*
 * This function enables the user to get the echo path size in bytes
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * size_t       return           : size in bytes
 */
size_t WebRtcAecm_echo_path_size_bytes();

/*
 * Gets the last error code.
 *
 * Inputs                       Description
 * -------------------------------------------------------------------
 * void         *aecmInst       Pointer to the AECM instance
 *
 * Outputs                      Description
 * -------------------------------------------------------------------
 * WebRtc_Word32  return        11000-11100: error code
 */
WebRtc_Word32 WebRtcAecm_get_error_code(void *aecmInst);

#ifdef __cplusplus
}
#endif
#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AECM_INCLUDE_ECHO_CONTROL_MOBILE_H_
