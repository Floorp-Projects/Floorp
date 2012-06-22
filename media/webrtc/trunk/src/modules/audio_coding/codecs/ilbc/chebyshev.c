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

 WebRtcIlbcfix_Chebyshev.c

******************************************************************/

#include "defines.h"
#include "constants.h"

/*------------------------------------------------------------------*
 *  Calculate the Chevyshev polynomial series
 *  F(w) = 2*exp(-j5w)*C(x)
 *   C(x) = (T_0(x) + f(1)T_1(x) + ... + f(4)T_1(x) + f(5)/2)
 *   T_i(x) is the i:th order Chebyshev polynomial
 *------------------------------------------------------------------*/

WebRtc_Word16 WebRtcIlbcfix_Chebyshev(
    /* (o) Result of C(x) */
    WebRtc_Word16 x,  /* (i) Value to the Chevyshev polynomial */
    WebRtc_Word16 *f  /* (i) The coefficients in the polynomial */
                                      ) {
  WebRtc_Word16 b1_high, b1_low; /* Use the high, low format to increase the accuracy */
  WebRtc_Word32 b2;
  WebRtc_Word32 tmp1W32;
  WebRtc_Word32 tmp2W32;
  int i;

  b2 = (WebRtc_Word32)0x1000000; /* b2 = 1.0 (Q23) */
  /* Calculate b1 = 2*x + f[1] */
  tmp1W32 = WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)x, 10);
  tmp1W32 += WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)f[1], 14);

  for (i = 2; i < 5; i++) {
    tmp2W32 = tmp1W32;

    /* Split b1 (in tmp1W32) into a high and low part */
    b1_high = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp1W32, 16);
    b1_low = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp1W32-WEBRTC_SPL_LSHIFT_W32(((WebRtc_Word32)b1_high),16), 1);

    /* Calculate 2*x*b1-b2+f[i] */
    tmp1W32 = WEBRTC_SPL_LSHIFT_W32( (WEBRTC_SPL_MUL_16_16(b1_high, x) +
                                      WEBRTC_SPL_MUL_16_16_RSFT(b1_low, x, 15)), 2);

    tmp1W32 -= b2;
    tmp1W32 += WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)f[i], 14);

    /* Update b2 for next round */
    b2 = tmp2W32;
  }

  /* Split b1 (in tmp1W32) into a high and low part */
  b1_high = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp1W32, 16);
  b1_low = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp1W32-WEBRTC_SPL_LSHIFT_W32(((WebRtc_Word32)b1_high),16), 1);

  /* tmp1W32 = x*b1 - b2 + f[i]/2 */
  tmp1W32 = WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_16(b1_high, x), 1) +
      WEBRTC_SPL_LSHIFT_W32(WEBRTC_SPL_MUL_16_16_RSFT(b1_low, x, 15), 1);

  tmp1W32 -= b2;
  tmp1W32 += WEBRTC_SPL_LSHIFT_W32((WebRtc_Word32)f[i], 13);

  /* Handle overflows and set to maximum or minimum WebRtc_Word16 instead */
  if (tmp1W32>((WebRtc_Word32)33553408)) {
    return(WEBRTC_SPL_WORD16_MAX);
  } else if (tmp1W32<((WebRtc_Word32)-33554432)) {
    return(WEBRTC_SPL_WORD16_MIN);
  } else {
    return((WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp1W32, 10));
  }
}
