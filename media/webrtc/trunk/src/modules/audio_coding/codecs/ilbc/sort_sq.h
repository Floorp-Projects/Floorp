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

 WebRtcIlbcfix_SortSq.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SORT_SQ_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SORT_SQ_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  scalar quantization
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_SortSq(
    WebRtc_Word16 *xq,   /* (o) the quantized value */
    WebRtc_Word16 *index,  /* (o) the quantization index */
    WebRtc_Word16 x,   /* (i) the value to quantize */
    const WebRtc_Word16 *cb, /* (i) the quantization codebook */
    WebRtc_Word16 cb_size  /* (i) the size of the quantization codebook */
                           );

#endif
