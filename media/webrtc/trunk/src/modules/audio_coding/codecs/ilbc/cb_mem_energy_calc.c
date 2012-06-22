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

 WebRtcIlbcfix_CbMemEnergyCalc.c

******************************************************************/

#include "defines.h"

/* Compute the energy of the rest of the cb memory
 * by step wise adding and subtracting the next
 * sample and the last sample respectively */
void WebRtcIlbcfix_CbMemEnergyCalc(
    WebRtc_Word32 energy,   /* (i) input start energy */
    WebRtc_Word16 range,   /* (i) number of iterations */
    WebRtc_Word16 *ppi,   /* (i) input pointer 1 */
    WebRtc_Word16 *ppo,   /* (i) input pointer 2 */
    WebRtc_Word16 *energyW16,  /* (o) Energy in the CB vectors */
    WebRtc_Word16 *energyShifts, /* (o) Shift value of the energy */
    WebRtc_Word16 scale,   /* (i) The scaling of all energy values */
    WebRtc_Word16 base_size  /* (i) Index to where the energy values should be stored */
                                   )
{
  WebRtc_Word16 j,shft;
  WebRtc_Word32 tmp;
  WebRtc_Word16 *eSh_ptr;
  WebRtc_Word16 *eW16_ptr;


  eSh_ptr  = &energyShifts[1+base_size];
  eW16_ptr = &energyW16[1+base_size];

  for(j=0;j<range-1;j++) {

    /* Calculate next energy by a +/-
       operation on the edge samples */
    tmp  = WEBRTC_SPL_MUL_16_16(*ppi, *ppi);
    tmp -= WEBRTC_SPL_MUL_16_16(*ppo, *ppo);
    energy += WEBRTC_SPL_RSHIFT_W32(tmp, scale);
    energy = WEBRTC_SPL_MAX(energy, 0);

    ppi--;
    ppo--;

    /* Normalize the energy into a WebRtc_Word16 and store
       the number of shifts */

    shft = (WebRtc_Word16)WebRtcSpl_NormW32(energy);
    *eSh_ptr++ = shft;

    tmp = WEBRTC_SPL_LSHIFT_W32(energy, shft);
    *eW16_ptr++ = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp, 16);
  }
}
