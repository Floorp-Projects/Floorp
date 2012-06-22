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

 WebRtcIlbcfix_CbUpdateBestIndex.c

******************************************************************/

#include "defines.h"
#include "cb_update_best_index.h"
#include "constants.h"

void WebRtcIlbcfix_CbUpdateBestIndex(
    WebRtc_Word32 CritNew,    /* (i) New Potentially best Criteria */
    WebRtc_Word16 CritNewSh,   /* (i) Shift value of above Criteria */
    WebRtc_Word16 IndexNew,   /* (i) Index of new Criteria */
    WebRtc_Word32 cDotNew,    /* (i) Cross dot of new index */
    WebRtc_Word16 invEnergyNew,  /* (i) Inversed energy new index */
    WebRtc_Word16 energyShiftNew,  /* (i) Energy shifts of new index */
    WebRtc_Word32 *CritMax,   /* (i/o) Maximum Criteria (so far) */
    WebRtc_Word16 *shTotMax,   /* (i/o) Shifts of maximum criteria */
    WebRtc_Word16 *bestIndex,   /* (i/o) Index that corresponds to
                                                   maximum criteria */
    WebRtc_Word16 *bestGain)   /* (i/o) Gain in Q14 that corresponds
                                                   to maximum criteria */
{
  WebRtc_Word16 shOld, shNew, tmp16;
  WebRtc_Word16 scaleTmp;
  WebRtc_Word32 gainW32;

  /* Normalize the new and old Criteria to the same domain */
  if (CritNewSh>(*shTotMax)) {
    shOld=WEBRTC_SPL_MIN(31,CritNewSh-(*shTotMax));
    shNew=0;
  } else {
    shOld=0;
    shNew=WEBRTC_SPL_MIN(31,(*shTotMax)-CritNewSh);
  }

  /* Compare the two criterias. If the new one is better,
     calculate the gain and store this index as the new best one
  */

  if (WEBRTC_SPL_RSHIFT_W32(CritNew, shNew)>
      WEBRTC_SPL_RSHIFT_W32((*CritMax),shOld)) {

    tmp16 = (WebRtc_Word16)WebRtcSpl_NormW32(cDotNew);
    tmp16 = 16 - tmp16;

    /* Calculate the gain in Q14
       Compensate for inverseEnergyshift in Q29 and that the energy
       value was stored in a WebRtc_Word16 (shifted down 16 steps)
       => 29-14+16 = 31 */

    scaleTmp = -energyShiftNew-tmp16+31;
    scaleTmp = WEBRTC_SPL_MIN(31, scaleTmp);

    gainW32 = WEBRTC_SPL_MUL_16_16_RSFT(
        ((WebRtc_Word16)WEBRTC_SPL_SHIFT_W32(cDotNew, -tmp16)), invEnergyNew, scaleTmp);

    /* Check if criteria satisfies Gain criteria (max 1.3)
       if it is larger set the gain to 1.3
       (slightly different from FLP version)
    */
    if (gainW32>21299) {
      *bestGain=21299;
    } else if (gainW32<-21299) {
      *bestGain=-21299;
    } else {
      *bestGain=(WebRtc_Word16)gainW32;
    }

    *CritMax=CritNew;
    *shTotMax=CritNewSh;
    *bestIndex = IndexNew;
  }

  return;
}
