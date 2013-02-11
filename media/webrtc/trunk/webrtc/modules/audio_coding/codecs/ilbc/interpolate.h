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

 WebRtcIlbcfix_Interpolate.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_INTERPOLATE_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_INTERPOLATE_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  interpolation between vectors
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_Interpolate(
    WebRtc_Word16 *out, /* (o) output vector */
    WebRtc_Word16 *in1, /* (i) first input vector */
    WebRtc_Word16 *in2, /* (i) second input vector */
    WebRtc_Word16 coef, /* (i) weight coefficient in Q14 */
    WebRtc_Word16 length); /* (i) number of sample is vectors */

#endif
