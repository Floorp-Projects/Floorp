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

 WebRtcIlbcfix_CbMemEnergyAugmentation.c

******************************************************************/

#include "defines.h"
#include "constants.h"

void WebRtcIlbcfix_CbMemEnergyAugmentation(
    WebRtc_Word16 *interpSamples, /* (i) The interpolated samples */
    WebRtc_Word16 *CBmem,   /* (i) The CB memory */
    WebRtc_Word16 scale,   /* (i) The scaling of all energy values */
    WebRtc_Word16 base_size,  /* (i) Index to where the energy values should be stored */
    WebRtc_Word16 *energyW16,  /* (o) Energy in the CB vectors */
    WebRtc_Word16 *energyShifts /* (o) Shift value of the energy */
                                           ){
  WebRtc_Word32 energy, tmp32;
  WebRtc_Word16 *ppe, *pp, *interpSamplesPtr;
  WebRtc_Word16 *CBmemPtr, lagcount;
  WebRtc_Word16 *enPtr=&energyW16[base_size-20];
  WebRtc_Word16 *enShPtr=&energyShifts[base_size-20];
  WebRtc_Word32 nrjRecursive;

  CBmemPtr = CBmem+147;
  interpSamplesPtr = interpSamples;

  /* Compute the energy for the first (low-5) noninterpolated samples */
  nrjRecursive = WebRtcSpl_DotProductWithScale( CBmemPtr-19, CBmemPtr-19, 15, scale);
  ppe = CBmemPtr - 20;

  for (lagcount=20; lagcount<=39; lagcount++) {

    /* Update the energy recursively to save complexity */
    nrjRecursive = nrjRecursive +
        WEBRTC_SPL_MUL_16_16_RSFT(*ppe, *ppe, scale);
    ppe--;
    energy = nrjRecursive;

    /* interpolation */
    energy += WebRtcSpl_DotProductWithScale(interpSamplesPtr, interpSamplesPtr, 4, scale);
    interpSamplesPtr += 4;

    /* Compute energy for the remaining samples */
    pp = CBmemPtr - lagcount;
    energy += WebRtcSpl_DotProductWithScale(pp, pp, SUBL-lagcount, scale);

    /* Normalize the energy and store the number of shifts */
    (*enShPtr) = (WebRtc_Word16)WebRtcSpl_NormW32(energy);
    tmp32 = WEBRTC_SPL_LSHIFT_W32(energy, (*enShPtr));
    (*enPtr) = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(tmp32, 16);
    enShPtr++;
    enPtr++;
  }
}
