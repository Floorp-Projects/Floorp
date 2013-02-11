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

 WebRtcIlbcfix_InterpolateSamples.c

******************************************************************/

#include "defines.h"
#include "constants.h"

void WebRtcIlbcfix_InterpolateSamples(
    WebRtc_Word16 *interpSamples, /* (o) The interpolated samples */
    WebRtc_Word16 *CBmem,   /* (i) The CB memory */
    WebRtc_Word16 lMem    /* (i) Length of the CB memory */
                                      ) {
  WebRtc_Word16 *ppi, *ppo, i, j, temp1, temp2;
  WebRtc_Word16 *tmpPtr;

  /* Calculate the 20 vectors of interpolated samples (4 samples each)
     that are used in the codebooks for lag 20 to 39 */
  tmpPtr = interpSamples;
  for (j=0; j<20; j++) {
    temp1 = 0;
    temp2 = 3;
    ppo = CBmem+lMem-4;
    ppi = CBmem+lMem-j-24;
    for (i=0; i<4; i++) {

      *tmpPtr++ = (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(WebRtcIlbcfix_kAlpha[temp2],*ppo, 15) +
          (WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(WebRtcIlbcfix_kAlpha[temp1], *ppi, 15);

      ppo++;
      ppi++;
      temp1++;
      temp2--;
    }
  }

  return;
}
