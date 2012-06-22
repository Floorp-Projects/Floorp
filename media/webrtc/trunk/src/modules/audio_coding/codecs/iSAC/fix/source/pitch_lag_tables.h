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
 * pitch_lag_tables.h
 *
 * This file contains tables for the pitch filter side-info in the entropy coder.
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_PITCH_LAG_TABLES_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_PITCH_LAG_TABLES_H_


#include "typedefs.h"


/********************* Pitch Filter Lag Coefficient Tables ************************/

/* tables for use with small pitch gain */

/* cdfs for quantized pitch lags */
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf1Lo[127];
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf2Lo[20];
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf3Lo[2];
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf4Lo[10];

extern const WebRtc_UWord16 *WebRtcIsacfix_kPitchLagPtrLo[4];

/* size of first cdf table */
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagSizeLo[1];

/* index limits and ranges */
extern const WebRtc_Word16 WebRtcIsacfix_kLowerLimitLo[4];
extern const WebRtc_Word16 WebRtcIsacfix_kUpperLimitLo[4];

/* initial index for arithmetic decoder */
extern const WebRtc_UWord16 WebRtcIsacfix_kInitIndLo[3];

/* mean values of pitch filter lags */
extern const WebRtc_Word16 WebRtcIsacfix_kMeanLag2Lo[19];
extern const WebRtc_Word16 WebRtcIsacfix_kMeanLag4Lo[9];



/* tables for use with medium pitch gain */

/* cdfs for quantized pitch lags */
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf1Mid[255];
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf2Mid[36];
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf3Mid[2];
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf4Mid[20];

extern const WebRtc_UWord16 *WebRtcIsacfix_kPitchLagPtrMid[4];

/* size of first cdf table */
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagSizeMid[1];

/* index limits and ranges */
extern const WebRtc_Word16 WebRtcIsacfix_kLowerLimitMid[4];
extern const WebRtc_Word16 WebRtcIsacfix_kUpperLimitMid[4];

/* initial index for arithmetic decoder */
extern const WebRtc_UWord16 WebRtcIsacfix_kInitIndMid[3];

/* mean values of pitch filter lags */
extern const WebRtc_Word16 WebRtcIsacfix_kMeanLag2Mid[35];
extern const WebRtc_Word16 WebRtcIsacfix_kMeanLag4Mid[19];


/* tables for use with large pitch gain */

/* cdfs for quantized pitch lags */
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf1Hi[511];
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf2Hi[68];
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf3Hi[2];
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagCdf4Hi[35];

extern const WebRtc_UWord16 *WebRtcIsacfix_kPitchLagPtrHi[4];

/* size of first cdf table */
extern const WebRtc_UWord16 WebRtcIsacfix_kPitchLagSizeHi[1];

/* index limits and ranges */
extern const WebRtc_Word16 WebRtcIsacfix_kLowerLimitHi[4];
extern const WebRtc_Word16 WebRtcIsacfix_kUpperLimitHi[4];

/* initial index for arithmetic decoder */
extern const WebRtc_UWord16 WebRtcIsacfix_kInitIndHi[3];

/* mean values of pitch filter lags */
extern const WebRtc_Word16 WebRtcIsacfix_kMeanLag2Hi[67];
extern const WebRtc_Word16 WebRtcIsacfix_kMeanLag4Hi[34];


#endif /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_PITCH_LAG_TABLES_H_ */
