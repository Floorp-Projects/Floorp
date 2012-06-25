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

 WebRtcIlbcfix_BwExpand.c

******************************************************************/

#include "defines.h"

/*----------------------------------------------------------------*
 *  lpc bandwidth expansion
 *---------------------------------------------------------------*/

/* The output is in the same domain as the input */
void WebRtcIlbcfix_BwExpand(
    WebRtc_Word16 *out, /* (o) the bandwidth expanded lpc coefficients */
    WebRtc_Word16 *in,  /* (i) the lpc coefficients before bandwidth
                                   expansion */
    WebRtc_Word16 *coef, /* (i) the bandwidth expansion factor Q15 */
    WebRtc_Word16 length /* (i) the length of lpc coefficient vectors */
                            ) {
  int i;

  out[0] = in[0];
  for (i = 1; i < length; i++) {
    /* out[i] = coef[i] * in[i] with rounding.
       in[] and out[] are in Q12 and coef[] is in Q15
    */
    out[i] = (WebRtc_Word16)((WEBRTC_SPL_MUL_16_16(coef[i], in[i])+16384)>>15);
  }
}
