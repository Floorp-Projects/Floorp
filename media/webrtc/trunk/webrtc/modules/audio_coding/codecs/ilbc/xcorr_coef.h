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

 WebRtcIlbcfix_XcorrCoef.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_XCORR_COEF_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_XCORR_COEF_H_

#include "defines.h"

/*----------------------------------------------------------------*
 * cross correlation which finds the optimal lag for the
 * crossCorr*crossCorr/(energy) criteria
 *---------------------------------------------------------------*/

int WebRtcIlbcfix_XcorrCoef(
    WebRtc_Word16 *target,  /* (i) first array */
    WebRtc_Word16 *regressor, /* (i) second array */
    WebRtc_Word16 subl,  /* (i) dimension arrays */
    WebRtc_Word16 searchLen, /* (i) the search lenght */
    WebRtc_Word16 offset,  /* (i) samples offset between arrays */
    WebRtc_Word16 step   /* (i) +1 or -1 */
                            );

#endif
