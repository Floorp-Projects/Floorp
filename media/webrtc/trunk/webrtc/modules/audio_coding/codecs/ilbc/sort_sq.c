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

 WebRtcIlbcfix_SortSq.c

******************************************************************/

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
                          ){
  int i;

  if (x <= cb[0]) {
    *index = 0;
    *xq = cb[0];
  } else {
    i = 0;
    while ((x > cb[i]) && (i < (cb_size-1))) {
      i++;
    }

    if (x > WEBRTC_SPL_RSHIFT_W32(( (WebRtc_Word32)cb[i] + cb[i - 1] + 1),1)) {
      *index = i;
      *xq = cb[i];
    } else {
      *index = i - 1;
      *xq = cb[i - 1];
    }
  }
}
