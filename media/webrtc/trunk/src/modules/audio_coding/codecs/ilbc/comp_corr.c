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

 WebRtcIlbcfix_CompCorr.c

******************************************************************/

#include "defines.h"

/*----------------------------------------------------------------*
 *  Compute cross correlation and pitch gain for pitch prediction
 *  of last subframe at given lag.
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_CompCorr(
    WebRtc_Word32 *corr, /* (o) cross correlation */
    WebRtc_Word32 *ener, /* (o) energy */
    WebRtc_Word16 *buffer, /* (i) signal buffer */
    WebRtc_Word16 lag,  /* (i) pitch lag */
    WebRtc_Word16 bLen, /* (i) length of buffer */
    WebRtc_Word16 sRange, /* (i) correlation search length */
    WebRtc_Word16 scale /* (i) number of rightshifts to use */
                            ){
  WebRtc_Word16 *w16ptr;

  w16ptr=&buffer[bLen-sRange-lag];

  /* Calculate correlation and energy */
  (*corr)=WebRtcSpl_DotProductWithScale(&buffer[bLen-sRange], w16ptr, sRange, scale);
  (*ener)=WebRtcSpl_DotProductWithScale(w16ptr, w16ptr, sRange, scale);

  /* For zero energy set the energy to 0 in order to avoid potential
     problems for coming divisions */
  if (*ener == 0) {
    *corr = 0;
    *ener = 1;
  }
}
