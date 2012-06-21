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

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_LPC_TABLES_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_LPC_TABLES_H_

#include "structs.h"

#include "settings.h"

#define KLT_STEPSIZE         1.00000000
#define KLT_NUM_AVG_GAIN     0
#define KLT_NUM_AVG_SHAPE    0
#define KLT_NUM_MODELS  3
#define LPC_GAIN_SCALE     4.000f
#define LPC_LOBAND_SCALE   2.100f
#define LPC_LOBAND_ORDER   ORDERLO
#define LPC_HIBAND_SCALE   0.450f
#define LPC_HIBAND_ORDER   ORDERHI
#define LPC_GAIN_ORDER     2

#define LPC_SHAPE_ORDER    (LPC_LOBAND_ORDER + LPC_HIBAND_ORDER)

#define KLT_ORDER_GAIN     (LPC_GAIN_ORDER * SUBFRAMES)
#define KLT_ORDER_SHAPE    (LPC_SHAPE_ORDER * SUBFRAMES)
/* indices of KLT coefficients used */
extern const WebRtc_UWord16 WebRtcIsac_kQKltSelIndGain[12];

extern const WebRtc_UWord16 WebRtcIsac_kQKltSelIndShape[108];

/* cdf array for model indicator */
extern const WebRtc_UWord16 WebRtcIsac_kQKltModelCdf[KLT_NUM_MODELS+1];

/* pointer to cdf array for model indicator */
extern const WebRtc_UWord16 *WebRtcIsac_kQKltModelCdfPtr[1];

/* initial cdf index for decoder of model indicator */
extern const WebRtc_UWord16 WebRtcIsac_kQKltModelInitIndex[1];

/* offset to go from rounded value to quantization index */
extern const short WebRtcIsac_kQKltQuantMinGain[12];

extern const short WebRtcIsac_kQKltQuantMinShape[108];

/* maximum quantization index */
extern const WebRtc_UWord16 WebRtcIsac_kQKltMaxIndGain[12];

extern const WebRtc_UWord16 WebRtcIsac_kQKltMaxIndShape[108];

/* index offset */
extern const WebRtc_UWord16 WebRtcIsac_kQKltOffsetGain[KLT_NUM_MODELS][12];

extern const WebRtc_UWord16 WebRtcIsac_kQKltOffsetShape[KLT_NUM_MODELS][108];

/* initial cdf index for KLT coefficients */
extern const WebRtc_UWord16 WebRtcIsac_kQKltInitIndexGain[KLT_NUM_MODELS][12];

extern const WebRtc_UWord16 WebRtcIsac_kQKltInitIndexShape[KLT_NUM_MODELS][108];

/* offsets for quantizer representation levels */
extern const WebRtc_UWord16 WebRtcIsac_kQKltOfLevelsGain[3];

extern const WebRtc_UWord16 WebRtcIsac_kQKltOfLevelsShape[3];

/* quantizer representation levels */
extern const double WebRtcIsac_kQKltLevelsGain[1176];

extern const double WebRtcIsac_kQKltLevelsShape[1735];

/* cdf tables for quantizer indices */
extern const WebRtc_UWord16 WebRtcIsac_kQKltCdfGain[1212];

extern const WebRtc_UWord16 WebRtcIsac_kQKltCdfShape[2059];

/* pointers to cdf tables for quantizer indices */
extern const WebRtc_UWord16 *WebRtcIsac_kQKltCdfPtrGain[KLT_NUM_MODELS][12];

extern const WebRtc_UWord16 *WebRtcIsac_kQKltCdfPtrShape[KLT_NUM_MODELS][108];

/* code length for all coefficients using different models */
extern const double WebRtcIsac_kQKltCodeLenGain[392];

extern const double WebRtcIsac_kQKltCodeLenShape[578];

/* left KLT transforms */
extern const double WebRtcIsac_kKltT1Gain[KLT_NUM_MODELS][4];

extern const double WebRtcIsac_kKltT1Shape[KLT_NUM_MODELS][324];

/* right KLT transforms */
extern const double WebRtcIsac_kKltT2Gain[KLT_NUM_MODELS][36];

extern const double WebRtcIsac_kKltT2Shape[KLT_NUM_MODELS][36];

/* means of log gains and LAR coefficients */
extern const double WebRtcIsac_kLpcMeansGain[KLT_NUM_MODELS][12];

extern const double WebRtcIsac_kLpcMeansShape[KLT_NUM_MODELS][108];

#endif /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_LPC_TABLES_H_ */
