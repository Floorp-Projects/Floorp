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

 WebRtcIlbcfix_Interpolate.c

******************************************************************/

#include "defines.h"
#include "constants.h"

/*----------------------------------------------------------------*
 *  interpolation between vectors
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_Interpolate(
    WebRtc_Word16 *out, /* (o) output vector */
    WebRtc_Word16 *in1, /* (i) first input vector */
    WebRtc_Word16 *in2, /* (i) second input vector */
    WebRtc_Word16 coef, /* (i) weight coefficient in Q14 */
    WebRtc_Word16 length)  /* (i) number of sample is vectors */
{
  int i;
  WebRtc_Word16 invcoef;

  /*
    Performs the operation out[i] = in[i]*coef + (1-coef)*in2[i] (with rounding)
  */

  invcoef = 16384 - coef; /* 16384 = 1.0 (Q14)*/
  for (i = 0; i < length; i++) {
    out[i] = (WebRtc_Word16) WEBRTC_SPL_RSHIFT_W32(
        (WEBRTC_SPL_MUL_16_16(coef, in1[i]) + WEBRTC_SPL_MUL_16_16(invcoef, in2[i]))+8192,
        14);
  }

  return;
}
