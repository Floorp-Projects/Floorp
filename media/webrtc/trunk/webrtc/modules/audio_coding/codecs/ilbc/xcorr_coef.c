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

 WebRtcIlbcfix_XcorrCoef.c

******************************************************************/

#include "defines.h"

/*----------------------------------------------------------------*
 * cross correlation which finds the optimal lag for the
 * crossCorr*crossCorr/(energy) criteria
 *---------------------------------------------------------------*/

int WebRtcIlbcfix_XcorrCoef(
    WebRtc_Word16 *target,  /* (i) first array */
    WebRtc_Word16 *regressor, /* (i) second array */
    WebRtc_Word16 subl,  /* (i) dimension arrays */
    WebRtc_Word16 searchLen, /* (i) the search lenght */
    WebRtc_Word16 offset,  /* (i) samples offset between arrays */
    WebRtc_Word16 step   /* (i) +1 or -1 */
                            ){
  int k;
  WebRtc_Word16 maxlag;
  WebRtc_Word16 pos;
  WebRtc_Word16 max;
  WebRtc_Word16 crossCorrScale, Energyscale;
  WebRtc_Word16 crossCorrSqMod, crossCorrSqMod_Max;
  WebRtc_Word32 crossCorr, Energy;
  WebRtc_Word16 crossCorrmod, EnergyMod, EnergyMod_Max;
  WebRtc_Word16 *tp, *rp;
  WebRtc_Word16 *rp_beg, *rp_end;
  WebRtc_Word16 totscale, totscale_max;
  WebRtc_Word16 scalediff;
  WebRtc_Word32 newCrit, maxCrit;
  int shifts;

  /* Initializations, to make sure that the first one is selected */
  crossCorrSqMod_Max=0;
  EnergyMod_Max=WEBRTC_SPL_WORD16_MAX;
  totscale_max=-500;
  maxlag=0;
  pos=0;

  /* Find scale value and start position */
  if (step==1) {
    max=WebRtcSpl_MaxAbsValueW16(regressor, (WebRtc_Word16)(subl+searchLen-1));
    rp_beg = regressor;
    rp_end = &regressor[subl];
  } else { /* step==-1 */
    max=WebRtcSpl_MaxAbsValueW16(&regressor[-searchLen], (WebRtc_Word16)(subl+searchLen-1));
    rp_beg = &regressor[-1];
    rp_end = &regressor[subl-1];
  }

  /* Introduce a scale factor on the Energy in WebRtc_Word32 in
     order to make sure that the calculation does not
     overflow */

  if (max>5000) {
    shifts=2;
  } else {
    shifts=0;
  }

  /* Calculate the first energy, then do a +/- to get the other energies */
  Energy=WebRtcSpl_DotProductWithScale(regressor, regressor, subl, shifts);

  for (k=0;k<searchLen;k++) {
    tp = target;
    rp = &regressor[pos];

    crossCorr=WebRtcSpl_DotProductWithScale(tp, rp, subl, shifts);

    if ((Energy>0)&&(crossCorr>0)) {

      /* Put cross correlation and energy on 16 bit word */
      crossCorrScale=(WebRtc_Word16)WebRtcSpl_NormW32(crossCorr)-16;
      crossCorrmod=(WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(crossCorr, crossCorrScale);
      Energyscale=(WebRtc_Word16)WebRtcSpl_NormW32(Energy)-16;
      EnergyMod=(WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(Energy, Energyscale);

      /* Square cross correlation and store upper WebRtc_Word16 */
      crossCorrSqMod=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16_RSFT(crossCorrmod, crossCorrmod, 16);

      /* Calculate the total number of (dynamic) right shifts that have
         been performed on (crossCorr*crossCorr)/energy
      */
      totscale=Energyscale-(crossCorrScale<<1);

      /* Calculate the shift difference in order to be able to compare the two
         (crossCorr*crossCorr)/energy in the same domain
      */
      scalediff=totscale-totscale_max;
      scalediff=WEBRTC_SPL_MIN(scalediff,31);
      scalediff=WEBRTC_SPL_MAX(scalediff,-31);

      /* Compute the cross multiplication between the old best criteria
         and the new one to be able to compare them without using a
         division */

      if (scalediff<0) {
        newCrit = ((WebRtc_Word32)crossCorrSqMod*EnergyMod_Max)>>(-scalediff);
        maxCrit = ((WebRtc_Word32)crossCorrSqMod_Max*EnergyMod);
      } else {
        newCrit = ((WebRtc_Word32)crossCorrSqMod*EnergyMod_Max);
        maxCrit = ((WebRtc_Word32)crossCorrSqMod_Max*EnergyMod)>>scalediff;
      }

      /* Store the new lag value if the new criteria is larger
         than previous largest criteria */

      if (newCrit > maxCrit) {
        crossCorrSqMod_Max = crossCorrSqMod;
        EnergyMod_Max = EnergyMod;
        totscale_max = totscale;
        maxlag = k;
      }
    }
    pos+=step;

    /* Do a +/- to get the next energy */
    Energy += step*(WEBRTC_SPL_RSHIFT_W32(
        ((WebRtc_Word32)(*rp_end)*(*rp_end)) - ((WebRtc_Word32)(*rp_beg)*(*rp_beg)),
        shifts));
    rp_beg+=step;
    rp_end+=step;
  }

  return(maxlag+offset);
}
