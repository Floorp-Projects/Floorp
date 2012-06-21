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

 WebRtcIlbcfix_AugmentedCbCorr.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_AUGMENTED_CB_CORR_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_AUGMENTED_CB_CORR_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  Calculate correlation between target and Augmented codebooks
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_AugmentedCbCorr(
    WebRtc_Word16 *target,   /* (i) Target vector */
    WebRtc_Word16 *buffer,   /* (i) Memory buffer */
    WebRtc_Word16 *interpSamples, /* (i) buffer with
                                           interpolated samples */
    WebRtc_Word32 *crossDot,  /* (o) The cross correlation between
                                           the target and the Augmented
                                           vector */
    WebRtc_Word16 low,    /* (i) Lag to start from (typically
                                                   20) */
    WebRtc_Word16 high,   /* (i) Lag to end at (typically 39 */
    WebRtc_Word16 scale);   /* (i) Scale factor to use for
                                                   the crossDot */

#endif
