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
 * filterbank_tables.h
 *
 * Header file for variables that are defined in
 * filterbank_tables.c.
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_FILTERBANK_TABLES_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_FILTERBANK_TABLES_H_

#include "typedefs.h"

/********************* Coefficient Tables ************************/

/* HPstcoeff_in_Q14 = {a1, a2, b1 - b0 * a1, b2 - b0 * a2}; */
extern const WebRtc_Word16 WebRtcIsacfix_kHpStCoeffInQ30[8]; /* [Q30hi Q30lo Q30hi Q30lo Q35hi Q35lo Q35hi Q35lo] */

/* HPstcoeff_out_1_Q14 = {a1, a2, b1 - b0 * a1, b2 - b0 * a2}; */
extern const WebRtc_Word16 WebRtcIsacfix_kHPStCoeffOut1Q30[8]; /* [Q30hi Q30lo Q30hi Q30lo Q35hi Q35lo Q35hi Q35lo] */

/* HPstcoeff_out_2_Q14 = {a1, a2, b1 - b0 * a1, b2 - b0 * a2}; */
extern const WebRtc_Word16 WebRtcIsacfix_kHPStCoeffOut2Q30[8]; /* [Q30hi Q30lo Q30hi Q30lo Q35hi Q35lo Q35hi Q35lo] */

/* The upper channel all-pass filter factors */
extern const WebRtc_Word16 WebRtcIsacfix_kUpperApFactorsQ15[2];

/* The lower channel all-pass filter factors */
extern const WebRtc_Word16 WebRtcIsacfix_kLowerApFactorsQ15[2];

#endif /* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_FILTERBANK_TABLES_H_ */
