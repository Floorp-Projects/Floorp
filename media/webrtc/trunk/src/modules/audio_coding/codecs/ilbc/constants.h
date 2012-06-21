/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************

 iLBC Speech Coder ANSI-C Source Code

 constants.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CONSTANTS_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CONSTANTS_H_

#include "defines.h"
#include "typedefs.h"

/* high pass filters */

extern const WebRtc_Word16 WebRtcIlbcfix_kHpInCoefs[];
extern const WebRtc_Word16 WebRtcIlbcfix_kHpOutCoefs[];

/* Window for start state decision */
extern const WebRtc_Word16 WebRtcIlbcfix_kStartSequenceEnrgWin[];

/* low pass filter used for downsampling */
extern const WebRtc_Word16 WebRtcIlbcfix_kLpFiltCoefs[];

/* LPC analysis and quantization */

extern const WebRtc_Word16 WebRtcIlbcfix_kLpcWin[];
extern const WebRtc_Word16 WebRtcIlbcfix_kLpcAsymWin[];
extern const WebRtc_Word32 WebRtcIlbcfix_kLpcLagWin[];
extern const WebRtc_Word16 WebRtcIlbcfix_kLpcChirpSyntDenum[];
extern const WebRtc_Word16 WebRtcIlbcfix_kLpcChirpWeightDenum[];
extern const WebRtc_Word16 WebRtcIlbcfix_kLsfDimCb[];
extern const WebRtc_Word16 WebRtcIlbcfix_kLsfSizeCb[];
extern const WebRtc_Word16 WebRtcIlbcfix_kLsfCb[];
extern const WebRtc_Word16 WebRtcIlbcfix_kLsfWeight20ms[];
extern const WebRtc_Word16 WebRtcIlbcfix_kLsfWeight30ms[];
extern const WebRtc_Word16 WebRtcIlbcfix_kLsfMean[];
extern const WebRtc_Word16 WebRtcIlbcfix_kLspMean[];
extern const WebRtc_Word16 WebRtcIlbcfix_kCos[];
extern const WebRtc_Word16 WebRtcIlbcfix_kCosDerivative[];
extern const WebRtc_Word16 WebRtcIlbcfix_kCosGrid[];
extern const WebRtc_Word16 WebRtcIlbcfix_kAcosDerivative[];

/* state quantization tables */

extern const WebRtc_Word16 WebRtcIlbcfix_kStateSq3[];
extern const WebRtc_Word32 WebRtcIlbcfix_kChooseFrgQuant[];
extern const WebRtc_Word16 WebRtcIlbcfix_kScale[];
extern const WebRtc_Word16 WebRtcIlbcfix_kFrgQuantMod[];

/* Ranges for search and filters at different subframes */

extern const WebRtc_Word16 WebRtcIlbcfix_kSearchRange[5][CB_NSTAGES];
extern const WebRtc_Word16 WebRtcIlbcfix_kFilterRange[];

/* gain quantization tables */

extern const WebRtc_Word16 WebRtcIlbcfix_kGainSq3[];
extern const WebRtc_Word16 WebRtcIlbcfix_kGainSq4[];
extern const WebRtc_Word16 WebRtcIlbcfix_kGainSq5[];
extern const WebRtc_Word16 WebRtcIlbcfix_kGainSq5Sq[];
extern const WebRtc_Word16* const WebRtcIlbcfix_kGain[];

/* adaptive codebook definitions */

extern const WebRtc_Word16 WebRtcIlbcfix_kCbFiltersRev[];
extern const WebRtc_Word16 WebRtcIlbcfix_kAlpha[];

/* enhancer definitions */

extern const WebRtc_Word16 WebRtcIlbcfix_kEnhPolyPhaser[ENH_UPS0][ENH_FLO_MULT2_PLUS1];
extern const WebRtc_Word16 WebRtcIlbcfix_kEnhWt[];
extern const WebRtc_Word16 WebRtcIlbcfix_kEnhPlocs[];

/* PLC tables */

extern const WebRtc_Word16 WebRtcIlbcfix_kPlcPerSqr[];
extern const WebRtc_Word16 WebRtcIlbcfix_kPlcPitchFact[];
extern const WebRtc_Word16 WebRtcIlbcfix_kPlcPfSlope[];

#endif
