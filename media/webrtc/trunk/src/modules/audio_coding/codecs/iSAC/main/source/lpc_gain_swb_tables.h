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
 * SWB_KLT_Tables_LPCGain.h
 *
 * This file declares tables used for entropy coding of LPC Gain
 * of upper-band.
 *
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_LPC_GAIN_SWB_TABLES_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_LPC_GAIN_SWB_TABLES_H_

#include "settings.h"
#include "typedefs.h"

extern const double WebRtcIsac_kQSizeLpcGain;

extern const double WebRtcIsac_kLeftRecPointLpcGain[SUBFRAMES];

extern const WebRtc_Word16 WebRtcIsac_kNumQCellLpcGain[SUBFRAMES];

extern const WebRtc_UWord16 WebRtcIsac_kLpcGainEntropySearch[SUBFRAMES];

extern const WebRtc_UWord16 WebRtcIsac_kLpcGainCdfVec0[18];

extern const WebRtc_UWord16 WebRtcIsac_kLpcGainCdfVec1[21];

extern const WebRtc_UWord16 WebRtcIsac_kLpcGainCdfVec2[26];

extern const WebRtc_UWord16 WebRtcIsac_kLpcGainCdfVec3[46];

extern const WebRtc_UWord16 WebRtcIsac_kLpcGainCdfVec4[78];

extern const WebRtc_UWord16 WebRtcIsac_kLpcGainCdfVec5[171];

extern const WebRtc_UWord16* WebRtcIsac_kLpcGainCdfMat[SUBFRAMES];

extern const double WebRtcIsac_kLpcGainDecorrMat[SUBFRAMES][SUBFRAMES];

#endif // WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_LPC_GAIN_SWB_TABLES_H_
