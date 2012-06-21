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

 WebRtcIlbcfix_MyCorr.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_MY_CORR_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_MY_CORR_H_

#include "defines.h"

/*----------------------------------------------------------------*
 * compute cross correlation between sequences
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_MyCorr(
    WebRtc_Word32 *corr,  /* (o) correlation of seq1 and seq2 */
    WebRtc_Word16 *seq1,  /* (i) first sequence */
    WebRtc_Word16 dim1,  /* (i) dimension first seq1 */
    const WebRtc_Word16 *seq2, /* (i) second sequence */
    WebRtc_Word16 dim2   /* (i) dimension seq2 */
                          );

#endif
