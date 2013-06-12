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
 * spectrum_ar_model_tables.h
 *
 * This file contains definitions of tables with AR coefficients,
 * Gain coefficients and cosine tables.
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_SPECTRUM_AR_MODEL_TABLES_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_SPECTRUM_AR_MODEL_TABLES_H_

#include "typedefs.h"
#include "settings.h"


/********************* AR Coefficient Tables ************************/
/* cdf for quantized reflection coefficient 1 */
extern const WebRtc_UWord16 WebRtcIsacfix_kRc1Cdf[12];

/* cdf for quantized reflection coefficient 2 */
extern const WebRtc_UWord16 WebRtcIsacfix_kRc2Cdf[12];

/* cdf for quantized reflection coefficient 3 */
extern const WebRtc_UWord16 WebRtcIsacfix_kRc3Cdf[12];

/* cdf for quantized reflection coefficient 4 */
extern const WebRtc_UWord16 WebRtcIsacfix_kRc4Cdf[12];

/* cdf for quantized reflection coefficient 5 */
extern const WebRtc_UWord16 WebRtcIsacfix_kRc5Cdf[12];

/* cdf for quantized reflection coefficient 6 */
extern const WebRtc_UWord16 WebRtcIsacfix_kRc6Cdf[12];

/* representation levels for quantized reflection coefficient 1 */
extern const WebRtc_Word16 WebRtcIsacfix_kRc1Levels[11];

/* representation levels for quantized reflection coefficient 2 */
extern const WebRtc_Word16 WebRtcIsacfix_kRc2Levels[11];

/* representation levels for quantized reflection coefficient 3 */
extern const WebRtc_Word16 WebRtcIsacfix_kRc3Levels[11];

/* representation levels for quantized reflection coefficient 4 */
extern const WebRtc_Word16 WebRtcIsacfix_kRc4Levels[11];

/* representation levels for quantized reflection coefficient 5 */
extern const WebRtc_Word16 WebRtcIsacfix_kRc5Levels[11];

/* representation levels for quantized reflection coefficient 6 */
extern const WebRtc_Word16 WebRtcIsacfix_kRc6Levels[11];

/* quantization boundary levels for reflection coefficients */
extern const WebRtc_Word16 WebRtcIsacfix_kRcBound[12];

/* initial indices for AR reflection coefficient quantizer and cdf table search */
extern const WebRtc_UWord16 WebRtcIsacfix_kRcInitInd[AR_ORDER];

/* pointers to AR cdf tables */
extern const WebRtc_UWord16 *WebRtcIsacfix_kRcCdfPtr[AR_ORDER];

/* pointers to AR representation levels tables */
extern const WebRtc_Word16 *WebRtcIsacfix_kRcLevPtr[AR_ORDER];


/******************** GAIN Coefficient Tables ***********************/
/* cdf for Gain coefficient */
extern const WebRtc_UWord16 WebRtcIsacfix_kGainCdf[19];

/* representation levels for quantized Gain coefficient */
extern const WebRtc_Word32 WebRtcIsacfix_kGain2Lev[18];

/* squared quantization boundary levels for Gain coefficient */
extern const WebRtc_Word32 WebRtcIsacfix_kGain2Bound[19];

/* pointer to Gain cdf table */
extern const WebRtc_UWord16 *WebRtcIsacfix_kGainPtr[1];

/* Gain initial index for gain quantizer and cdf table search */
extern const WebRtc_UWord16 WebRtcIsacfix_kGainInitInd[1];

/************************* Cosine Tables ****************************/
/* Cosine table */
extern const WebRtc_Word16 WebRtcIsacfix_kCos[6][60];

#endif /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_SPECTRUM_AR_MODEL_TABLES_H_ */
