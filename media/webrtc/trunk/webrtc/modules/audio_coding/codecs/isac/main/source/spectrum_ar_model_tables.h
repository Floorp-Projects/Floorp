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

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_SPECTRUM_AR_MODEL_TABLES_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_SPECTRUM_AR_MODEL_TABLES_H_

#include "structs.h"

/********************* AR Coefficient Tables ************************/
/* cdf for quantized reflection coefficient 1 */
extern const WebRtc_UWord16 WebRtcIsac_kQArRc1Cdf[12];

/* cdf for quantized reflection coefficient 2 */
extern const WebRtc_UWord16 WebRtcIsac_kQArRc2Cdf[12];

/* cdf for quantized reflection coefficient 3 */
extern const WebRtc_UWord16 WebRtcIsac_kQArRc3Cdf[12];

/* cdf for quantized reflection coefficient 4 */
extern const WebRtc_UWord16 WebRtcIsac_kQArRc4Cdf[12];

/* cdf for quantized reflection coefficient 5 */
extern const WebRtc_UWord16 WebRtcIsac_kQArRc5Cdf[12];

/* cdf for quantized reflection coefficient 6 */
extern const WebRtc_UWord16 WebRtcIsac_kQArRc6Cdf[12];

/* quantization boundary levels for reflection coefficients */
extern const WebRtc_Word16 WebRtcIsac_kQArBoundaryLevels[12];

/* initial indices for AR reflection coefficient quantizer and cdf table search */
extern const WebRtc_UWord16 WebRtcIsac_kQArRcInitIndex[AR_ORDER];

/* pointers to AR cdf tables */
extern const WebRtc_UWord16 *WebRtcIsac_kQArRcCdfPtr[AR_ORDER];

/* pointers to AR representation levels tables */
extern const WebRtc_Word16 *WebRtcIsac_kQArRcLevelsPtr[AR_ORDER];


/******************** GAIN Coefficient Tables ***********************/
/* cdf for Gain coefficient */
extern const WebRtc_UWord16 WebRtcIsac_kQGainCdf[19];

/* representation levels for quantized Gain coefficient */
extern const WebRtc_Word32 WebRtcIsac_kQGain2Levels[18];

/* squared quantization boundary levels for Gain coefficient */
extern const WebRtc_Word32 WebRtcIsac_kQGain2BoundaryLevels[19];

/* pointer to Gain cdf table */
extern const WebRtc_UWord16 *WebRtcIsac_kQGainCdf_ptr[1];

/* Gain initial index for gain quantizer and cdf table search */
extern const WebRtc_UWord16 WebRtcIsac_kQGainInitIndex[1];

/************************* Cosine Tables ****************************/
/* Cosine table */
extern const WebRtc_Word16 WebRtcIsac_kCos[6][60];

#endif /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_SPECTRUM_AR_MODEL_TABLES_H_ */
