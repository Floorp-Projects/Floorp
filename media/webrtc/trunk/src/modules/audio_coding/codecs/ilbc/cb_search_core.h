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

 WebRtcIlbcfix_CbSearchCore.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_SEARCH_CORE_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_SEARCH_CORE_H_

#include "defines.h"

void WebRtcIlbcfix_CbSearchCore(
    WebRtc_Word32 *cDot,    /* (i) Cross Correlation */
    WebRtc_Word16 range,    /* (i) Search range */
    WebRtc_Word16 stage,    /* (i) Stage of this search */
    WebRtc_Word16 *inverseEnergy,  /* (i) Inversed energy */
    WebRtc_Word16 *inverseEnergyShift, /* (i) Shifts of inversed energy
                                          with the offset 2*16-29 */
    WebRtc_Word32 *Crit,    /* (o) The criteria */
    WebRtc_Word16 *bestIndex,   /* (o) Index that corresponds to
                                   maximum criteria (in this
                                   vector) */
    WebRtc_Word32 *bestCrit,   /* (o) Value of critera for the
                                  chosen index */
    WebRtc_Word16 *bestCritSh);  /* (o) The domain of the chosen
                                    criteria */

#endif
