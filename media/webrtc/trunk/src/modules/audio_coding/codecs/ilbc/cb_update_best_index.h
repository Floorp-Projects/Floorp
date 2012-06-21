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

 WebRtcIlbcfix_CbUpdateBestIndex.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_UPDATE_BEST_INDEX_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_UPDATE_BEST_INDEX_H_

#include "defines.h"

void WebRtcIlbcfix_CbUpdateBestIndex(
    WebRtc_Word32 CritNew,    /* (i) New Potentially best Criteria */
    WebRtc_Word16 CritNewSh,   /* (i) Shift value of above Criteria */
    WebRtc_Word16 IndexNew,   /* (i) Index of new Criteria */
    WebRtc_Word32 cDotNew,    /* (i) Cross dot of new index */
    WebRtc_Word16 invEnergyNew,  /* (i) Inversed energy new index */
    WebRtc_Word16 energyShiftNew,  /* (i) Energy shifts of new index */
    WebRtc_Word32 *CritMax,   /* (i/o) Maximum Criteria (so far) */
    WebRtc_Word16 *shTotMax,   /* (i/o) Shifts of maximum criteria */
    WebRtc_Word16 *bestIndex,   /* (i/o) Index that corresponds to
                                   maximum criteria */
    WebRtc_Word16 *bestGain);   /* (i/o) Gain in Q14 that corresponds
                                   to maximum criteria */

#endif
