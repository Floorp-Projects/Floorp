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

 WebRtcIlbcfix_StateConstruct.c

******************************************************************/

#include "defines.h"
#include "constants.h"

/*----------------------------------------------------------------*
 *  decoding of the start state
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_StateConstruct(
    WebRtc_Word16 idxForMax,   /* (i) 6-bit index for the quantization of
                                           max amplitude */
    WebRtc_Word16 *idxVec,   /* (i) vector of quantization indexes */
    WebRtc_Word16 *syntDenum,  /* (i) synthesis filter denumerator */
    WebRtc_Word16 *Out_fix,  /* (o) the decoded state vector */
    WebRtc_Word16 len    /* (i) length of a state vector */
                                  ) {
  int k;
  WebRtc_Word16 maxVal;
  WebRtc_Word16 *tmp1, *tmp2, *tmp3;
  /* Stack based */
  WebRtc_Word16 numerator[1+LPC_FILTERORDER];
  WebRtc_Word16 sampleValVec[2*STATE_SHORT_LEN_30MS+LPC_FILTERORDER];
  WebRtc_Word16 sampleMaVec[2*STATE_SHORT_LEN_30MS+LPC_FILTERORDER];
  WebRtc_Word16 *sampleVal = &sampleValVec[LPC_FILTERORDER];
  WebRtc_Word16 *sampleMa = &sampleMaVec[LPC_FILTERORDER];
  WebRtc_Word16 *sampleAr = &sampleValVec[LPC_FILTERORDER];

  /* initialization of coefficients */

  for (k=0; k<LPC_FILTERORDER+1; k++){
    numerator[k] = syntDenum[LPC_FILTERORDER-k];
  }

  /* decoding of the maximum value */

  maxVal = WebRtcIlbcfix_kFrgQuantMod[idxForMax];

  /* decoding of the sample values */
  tmp1 = sampleVal;
  tmp2 = &idxVec[len-1];

  if (idxForMax<37) {
    for(k=0; k<len; k++){
      /*the shifting is due to the Q13 in sq4_fixQ13[i], also the adding of 2097152 (= 0.5 << 22)
        maxVal is in Q8 and result is in Q(-1) */
      (*tmp1) = (WebRtc_Word16) ((WEBRTC_SPL_MUL_16_16(maxVal,WebRtcIlbcfix_kStateSq3[(*tmp2)])+(WebRtc_Word32)2097152) >> 22);
      tmp1++;
      tmp2--;
    }
  } else if (idxForMax<59) {
    for(k=0; k<len; k++){
      /*the shifting is due to the Q13 in sq4_fixQ13[i], also the adding of 262144 (= 0.5 << 19)
        maxVal is in Q5 and result is in Q(-1) */
      (*tmp1) = (WebRtc_Word16) ((WEBRTC_SPL_MUL_16_16(maxVal,WebRtcIlbcfix_kStateSq3[(*tmp2)])+(WebRtc_Word32)262144) >> 19);
      tmp1++;
      tmp2--;
    }
  } else {
    for(k=0; k<len; k++){
      /*the shifting is due to the Q13 in sq4_fixQ13[i], also the adding of 65536 (= 0.5 << 17)
        maxVal is in Q3 and result is in Q(-1) */
      (*tmp1) = (WebRtc_Word16) ((WEBRTC_SPL_MUL_16_16(maxVal,WebRtcIlbcfix_kStateSq3[(*tmp2)])+(WebRtc_Word32)65536) >> 17);
      tmp1++;
      tmp2--;
    }
  }

  /* Set the rest of the data to zero */
  WebRtcSpl_MemSetW16(&sampleVal[len], 0, len);

  /* circular convolution with all-pass filter */

  /* Set the state to zero */
  WebRtcSpl_MemSetW16(sampleValVec, 0, (LPC_FILTERORDER));

  /* Run MA filter + AR filter */
  WebRtcSpl_FilterMAFastQ12(
      sampleVal, sampleMa,
      numerator, LPC_FILTERORDER+1, (WebRtc_Word16)(len + LPC_FILTERORDER));
  WebRtcSpl_MemSetW16(&sampleMa[len + LPC_FILTERORDER], 0, (len - LPC_FILTERORDER));
  WebRtcSpl_FilterARFastQ12(
      sampleMa, sampleAr,
      syntDenum, LPC_FILTERORDER+1, (WebRtc_Word16)(2*len));

  tmp1 = &sampleAr[len-1];
  tmp2 = &sampleAr[2*len-1];
  tmp3 = Out_fix;
  for(k=0;k<len;k++){
    (*tmp3) = (*tmp1) + (*tmp2);
    tmp1--;
    tmp2--;
    tmp3++;
  }
}
