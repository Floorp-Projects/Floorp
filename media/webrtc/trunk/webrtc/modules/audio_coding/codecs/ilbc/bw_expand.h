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

 WebRtcIlbcfix_BwExpand.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_BW_EXPAND_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_BW_EXPAND_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  lpc bandwidth expansion
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_BwExpand(
    WebRtc_Word16 *out, /* (o) the bandwidth expanded lpc coefficients */
    WebRtc_Word16 *in,  /* (i) the lpc coefficients before bandwidth
                                   expansion */
    WebRtc_Word16 *coef, /* (i) the bandwidth expansion factor Q15 */
    WebRtc_Word16 length /* (i) the length of lpc coefficient vectors */
                            );

#endif
