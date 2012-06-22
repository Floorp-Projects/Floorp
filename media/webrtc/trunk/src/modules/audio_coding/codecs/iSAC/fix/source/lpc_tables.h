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
 * lpc_tables.h
 *
 * header file for coding tables for the LPC coefficients
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_LPC_TABLES_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_LPC_TABLES_H_

#include "typedefs.h"


/* indices of KLT coefficients used */
extern const WebRtc_UWord16 WebRtcIsacfix_kSelIndGain[12];

extern const WebRtc_UWord16 WebRtcIsacfix_kSelIndShape[108];

/* cdf array for model indicator */
extern const WebRtc_UWord16 WebRtcIsacfix_kModelCdf[KLT_NUM_MODELS+1];

/* pointer to cdf array for model indicator */
extern const WebRtc_UWord16 *WebRtcIsacfix_kModelCdfPtr[1];

/* initial cdf index for decoder of model indicator */
extern const WebRtc_UWord16 WebRtcIsacfix_kModelInitIndex[1];

/* offset to go from rounded value to quantization index */
extern const WebRtc_Word16 WebRtcIsacfix_kQuantMinGain[12];

extern const WebRtc_Word16 WebRtcIsacfix_kQuantMinShape[108];

/* maximum quantization index */
extern const WebRtc_UWord16 WebRtcIsacfix_kMaxIndGain[12];

extern const WebRtc_UWord16 WebRtcIsacfix_kMaxIndShape[108];

/* index offset */
extern const WebRtc_UWord16 WebRtcIsacfix_kOffsetGain[KLT_NUM_MODELS][12];

extern const WebRtc_UWord16 WebRtcIsacfix_kOffsetShape[KLT_NUM_MODELS][108];

/* initial cdf index for KLT coefficients */
extern const WebRtc_UWord16 WebRtcIsacfix_kInitIndexGain[KLT_NUM_MODELS][12];

extern const WebRtc_UWord16 WebRtcIsacfix_kInitIndexShape[KLT_NUM_MODELS][108];

/* offsets for quantizer representation levels */
extern const WebRtc_UWord16 WebRtcIsacfix_kOfLevelsGain[3];

extern const WebRtc_UWord16 WebRtcIsacfix_kOfLevelsShape[3];

/* quantizer representation levels */
extern const WebRtc_Word32 WebRtcIsacfix_kLevelsGainQ17[1176];

extern const WebRtc_Word16 WebRtcIsacfix_kLevelsShapeQ10[1735];

/* cdf tables for quantizer indices */
extern const WebRtc_UWord16 WebRtcIsacfix_kCdfGain[1212];

extern const WebRtc_UWord16 WebRtcIsacfix_kCdfShape[2059];

/* pointers to cdf tables for quantizer indices */
extern const WebRtc_UWord16 *WebRtcIsacfix_kCdfGainPtr[KLT_NUM_MODELS][12];

extern const WebRtc_UWord16 *WebRtcIsacfix_kCdfShapePtr[KLT_NUM_MODELS][108];

/* code length for all coefficients using different models */
extern const WebRtc_Word16 WebRtcIsacfix_kCodeLenGainQ11[392];

extern const WebRtc_Word16 WebRtcIsacfix_kCodeLenShapeQ11[577];

/* left KLT transforms */
extern const WebRtc_Word16 WebRtcIsacfix_kT1GainQ15[KLT_NUM_MODELS][4];

extern const WebRtc_Word16 WebRtcIsacfix_kT1ShapeQ15[KLT_NUM_MODELS][324];

/* right KLT transforms */
extern const WebRtc_Word16 WebRtcIsacfix_kT2GainQ15[KLT_NUM_MODELS][36];

extern const WebRtc_Word16 WebRtcIsacfix_kT2ShapeQ15[KLT_NUM_MODELS][36];

/* means of log gains and LAR coefficients */
extern const WebRtc_Word16 WebRtcIsacfix_kMeansGainQ8[KLT_NUM_MODELS][12];

extern const WebRtc_Word32 WebRtcIsacfix_kMeansShapeQ17[3][108];

#endif /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_LPC_TABLES_H_ */
