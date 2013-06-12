/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AGC_MAIN_SOURCE_DIGITAL_AGC_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AGC_MAIN_SOURCE_DIGITAL_AGC_H_

#ifdef AGC_DEBUG
#include <stdio.h>
#endif
#include "typedefs.h"
#include "signal_processing_library.h"

// the 32 most significant bits of A(19) * B(26) >> 13
#define AGC_MUL32(A, B)             (((B)>>13)*(A) + ( ((0x00001FFF & (B))*(A)) >> 13 ))
// C + the 32 most significant bits of A * B
#define AGC_SCALEDIFF32(A, B, C)    ((C) + ((B)>>16)*(A) + ( ((0x0000FFFF & (B))*(A)) >> 16 ))

typedef struct
{
    WebRtc_Word32 downState[8];
    WebRtc_Word16 HPstate;
    WebRtc_Word16 counter;
    WebRtc_Word16 logRatio; // log( P(active) / P(inactive) ) (Q10)
    WebRtc_Word16 meanLongTerm; // Q10
    WebRtc_Word32 varianceLongTerm; // Q8
    WebRtc_Word16 stdLongTerm; // Q10
    WebRtc_Word16 meanShortTerm; // Q10
    WebRtc_Word32 varianceShortTerm; // Q8
    WebRtc_Word16 stdShortTerm; // Q10
} AgcVad_t; // total = 54 bytes

typedef struct
{
    WebRtc_Word32 capacitorSlow;
    WebRtc_Word32 capacitorFast;
    WebRtc_Word32 gain;
    WebRtc_Word32 gainTable[32];
    WebRtc_Word16 gatePrevious;
    WebRtc_Word16 agcMode;
    AgcVad_t      vadNearend;
    AgcVad_t      vadFarend;
#ifdef AGC_DEBUG
    FILE*         logFile;
    int           frameCounter;
#endif
} DigitalAgc_t;

WebRtc_Word32 WebRtcAgc_InitDigital(DigitalAgc_t *digitalAgcInst, WebRtc_Word16 agcMode);

WebRtc_Word32 WebRtcAgc_ProcessDigital(DigitalAgc_t *digitalAgcInst, const WebRtc_Word16 *inNear,
                             const WebRtc_Word16 *inNear_H, WebRtc_Word16 *out,
                             WebRtc_Word16 *out_H, WebRtc_UWord32 FS,
                             WebRtc_Word16 lowLevelSignal);

WebRtc_Word32 WebRtcAgc_AddFarendToDigital(DigitalAgc_t *digitalAgcInst, const WebRtc_Word16 *inFar,
                                 WebRtc_Word16 nrSamples);

void WebRtcAgc_InitVad(AgcVad_t *vadInst);

WebRtc_Word16 WebRtcAgc_ProcessVad(AgcVad_t *vadInst, // (i) VAD state
                            const WebRtc_Word16 *in, // (i) Speech signal
                            WebRtc_Word16 nrSamples); // (i) number of samples

WebRtc_Word32 WebRtcAgc_CalculateGainTable(WebRtc_Word32 *gainTable, // Q16
                                 WebRtc_Word16 compressionGaindB, // Q0 (in dB)
                                 WebRtc_Word16 targetLevelDbfs,// Q0 (in dB)
                                 WebRtc_UWord8 limiterEnable, WebRtc_Word16 analogTarget);

#endif // WEBRTC_MODULES_AUDIO_PROCESSING_AGC_MAIN_SOURCE_ANALOG_AGC_H_
