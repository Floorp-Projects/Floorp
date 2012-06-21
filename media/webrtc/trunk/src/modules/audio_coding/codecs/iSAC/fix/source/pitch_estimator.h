/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * pitch_estimator.h
 *
 * Pitch functions
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_PITCH_ESTIMATOR_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_PITCH_ESTIMATOR_H_

#include "structs.h"



void WebRtcIsacfix_PitchAnalysis(const WebRtc_Word16 *in,               /* PITCH_FRAME_LEN samples */
                                 WebRtc_Word16 *outQ0,                  /* PITCH_FRAME_LEN+QLOOKAHEAD samples */
                                 PitchAnalysisStruct *State,
                                 WebRtc_Word16 *lagsQ7,
                                 WebRtc_Word16 *PitchGains_Q12);


void WebRtcIsacfix_InitialPitch(const WebRtc_Word16 *in,
                                PitchAnalysisStruct *State,
                                WebRtc_Word16 *qlags);

void WebRtcIsacfix_PitchFilter(WebRtc_Word16 *indatFix,
                               WebRtc_Word16 *outdatQQ,
                               PitchFiltstr *pfp,
                               WebRtc_Word16 *lagsQ7,
                               WebRtc_Word16 *gainsQ12,
                               WebRtc_Word16 type);

void WebRtcIsacfix_PitchFilterGains(const WebRtc_Word16 *indatQ0,
                                    PitchFiltstr *pfp,
                                    WebRtc_Word16 *lagsQ7,
                                    WebRtc_Word16 *gainsQ12);



void WebRtcIsacfix_DecimateAllpass32(const WebRtc_Word16 *in,
                                     WebRtc_Word32 *state_in,        /* array of size: 2*ALLPASSSECTIONS+1 */
                                     WebRtc_Word16 N,                   /* number of input samples */
                                     WebRtc_Word16 *out);             /* array of size N/2 */

#endif /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_PITCH_ESTIMATOR_H_ */
