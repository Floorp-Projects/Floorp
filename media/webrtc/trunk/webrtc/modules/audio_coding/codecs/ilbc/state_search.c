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

 WebRtcIlbcfix_StateSearch.c

******************************************************************/

#include "defines.h"
#include "constants.h"
#include "abs_quant.h"

/*----------------------------------------------------------------*
 *  encoding of start state
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_StateSearch(
    iLBC_Enc_Inst_t *iLBCenc_inst,
    /* (i) Encoder instance */
    iLBC_bits *iLBC_encbits,/* (i/o) Encoded bits (output idxForMax
                               and idxVec, input state_first) */
    WebRtc_Word16 *residual,   /* (i) target residual vector */
    WebRtc_Word16 *syntDenum,  /* (i) lpc synthesis filter */
    WebRtc_Word16 *weightDenum  /* (i) weighting filter denuminator */
                               ) {
  WebRtc_Word16 k, index;
  WebRtc_Word16 maxVal;
  WebRtc_Word16 scale, shift;
  WebRtc_Word32 maxValsq;
  WebRtc_Word16 scaleRes;
  WebRtc_Word16 max;
  int i;
  /* Stack based */
  WebRtc_Word16 numerator[1+LPC_FILTERORDER];
  WebRtc_Word16 residualLongVec[2*STATE_SHORT_LEN_30MS+LPC_FILTERORDER];
  WebRtc_Word16 sampleMa[2*STATE_SHORT_LEN_30MS];
  WebRtc_Word16 *residualLong = &residualLongVec[LPC_FILTERORDER];
  WebRtc_Word16 *sampleAr = residualLong;

  /* Scale to maximum 12 bits to avoid saturation in circular convolution filter */
  max = WebRtcSpl_MaxAbsValueW16(residual, iLBCenc_inst->state_short_len);
  scaleRes = WebRtcSpl_GetSizeInBits(max)-12;
  scaleRes = WEBRTC_SPL_MAX(0, scaleRes);
  /* Set up the filter coefficients for the circular convolution */
  for (i=0; i<LPC_FILTERORDER+1; i++) {
    numerator[i] = (syntDenum[LPC_FILTERORDER-i]>>scaleRes);
  }

  /* Copy the residual to a temporary buffer that we can filter
   * and set the remaining samples to zero.
   */
  WEBRTC_SPL_MEMCPY_W16(residualLong, residual, iLBCenc_inst->state_short_len);
  WebRtcSpl_MemSetW16(residualLong + iLBCenc_inst->state_short_len, 0, iLBCenc_inst->state_short_len);

  /* Run the Zero-Pole filter (Ciurcular convolution) */
  WebRtcSpl_MemSetW16(residualLongVec, 0, LPC_FILTERORDER);
  WebRtcSpl_FilterMAFastQ12(
      residualLong, sampleMa,
      numerator, LPC_FILTERORDER+1, (WebRtc_Word16)(iLBCenc_inst->state_short_len + LPC_FILTERORDER));
  WebRtcSpl_MemSetW16(&sampleMa[iLBCenc_inst->state_short_len + LPC_FILTERORDER], 0, iLBCenc_inst->state_short_len - LPC_FILTERORDER);

  WebRtcSpl_FilterARFastQ12(
      sampleMa, sampleAr,
      syntDenum, LPC_FILTERORDER+1, (WebRtc_Word16)(2*iLBCenc_inst->state_short_len));

  for(k=0;k<iLBCenc_inst->state_short_len;k++){
    sampleAr[k] += sampleAr[k+iLBCenc_inst->state_short_len];
  }

  /* Find maximum absolute value in the vector */
  maxVal=WebRtcSpl_MaxAbsValueW16(sampleAr, iLBCenc_inst->state_short_len);

  /* Find the best index */

  if ((((WebRtc_Word32)maxVal)<<scaleRes)<23170) {
    maxValsq=((WebRtc_Word32)maxVal*maxVal)<<(2+2*scaleRes);
  } else {
    maxValsq=(WebRtc_Word32)WEBRTC_SPL_WORD32_MAX;
  }

  index=0;
  for (i=0;i<63;i++) {

    if (maxValsq>=WebRtcIlbcfix_kChooseFrgQuant[i]) {
      index=i+1;
    } else {
      i=63;
    }
  }
  iLBC_encbits->idxForMax=index;

  /* Rescale the vector before quantization */
  scale=WebRtcIlbcfix_kScale[index];

  if (index<27) { /* scale table is in Q16, fout[] is in Q(-1) and we want the result to be in Q11 */
    shift=4;
  } else { /* scale table is in Q21, fout[] is in Q(-1) and we want the result to be in Q11 */
    shift=9;
  }

  /* Set up vectors for AbsQuant and rescale it with the scale factor */
  WebRtcSpl_ScaleVectorWithSat(sampleAr, sampleAr, scale,
                              iLBCenc_inst->state_short_len, (WebRtc_Word16)(shift-scaleRes));

  /* Quantize the values in fout[] */
  WebRtcIlbcfix_AbsQuant(iLBCenc_inst, iLBC_encbits, sampleAr, weightDenum);

  return;
}
