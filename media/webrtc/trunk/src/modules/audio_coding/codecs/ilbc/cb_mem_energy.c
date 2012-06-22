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

 WebRtcIlbcfix_CbMemEnergy.c

******************************************************************/

#include "defines.h"
#include "constants.h"
#include "cb_mem_energy_calc.h"

/*----------------------------------------------------------------*
 *  Function WebRtcIlbcfix_CbMemEnergy computes the energy of all
 * the vectors in the codebook memory that will be used in the
 * following search for the best match.
 *----------------------------------------------------------------*/

void WebRtcIlbcfix_CbMemEnergy(
    WebRtc_Word16 range,
    WebRtc_Word16 *CB,   /* (i) The CB memory (1:st section) */
    WebRtc_Word16 *filteredCB,  /* (i) The filtered CB memory (2:nd section) */
    WebRtc_Word16 lMem,   /* (i) Length of the CB memory */
    WebRtc_Word16 lTarget,   /* (i) Length of the target vector */
    WebRtc_Word16 *energyW16,  /* (o) Energy in the CB vectors */
    WebRtc_Word16 *energyShifts, /* (o) Shift value of the energy */
    WebRtc_Word16 scale,   /* (i) The scaling of all energy values */
    WebRtc_Word16 base_size  /* (i) Index to where the energy values should be stored */
                               ) {
  WebRtc_Word16 *ppi, *ppo, *pp;
  WebRtc_Word32 energy, tmp32;

  /* Compute the energy and store it in a vector. Also the
   * corresponding shift values are stored. The energy values
   * are reused in all three stages. */

  /* Calculate the energy in the first block of 'lTarget' sampels. */
  ppi = CB+lMem-lTarget-1;
  ppo = CB+lMem-1;

  pp=CB+lMem-lTarget;
  energy = WebRtcSpl_DotProductWithScale( pp, pp, lTarget, scale);

  /* Normalize the energy and store the number of shifts */
  energyShifts[0] = (WebRtc_Word16)WebRtcSpl_NormW32(energy);
  tmp32 = WEBRTC_SPL_LSHIFT_W32(energy, energyShifts[0]);
  energyW16[0] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp32, 16);

  /* Compute the energy of the rest of the cb memory
   * by step wise adding and subtracting the next
   * sample and the last sample respectively. */
  WebRtcIlbcfix_CbMemEnergyCalc(energy, range, ppi, ppo, energyW16, energyShifts, scale, 0);

  /* Next, precompute the energy values for the filtered cb section */
  energy=0;
  pp=filteredCB+lMem-lTarget;

  energy = WebRtcSpl_DotProductWithScale( pp, pp, lTarget, scale);

  /* Normalize the energy and store the number of shifts */
  energyShifts[base_size] = (WebRtc_Word16)WebRtcSpl_NormW32(energy);
  tmp32 = WEBRTC_SPL_LSHIFT_W32(energy, energyShifts[base_size]);
  energyW16[base_size] = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp32, 16);

  ppi = filteredCB + lMem - 1 - lTarget;
  ppo = filteredCB + lMem - 1;

  WebRtcIlbcfix_CbMemEnergyCalc(energy, range, ppi, ppo, energyW16, energyShifts, scale, base_size);
}
