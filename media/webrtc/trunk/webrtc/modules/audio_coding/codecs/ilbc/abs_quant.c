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

 WebRtcIlbcfix_AbsQuant.c

******************************************************************/

#include "defines.h"
#include "constants.h"
#include "abs_quant_loop.h"


/*----------------------------------------------------------------*
 *  predictive noise shaping encoding of scaled start state
 *  (subrutine for WebRtcIlbcfix_StateSearch)
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_AbsQuant(
    iLBC_Enc_Inst_t *iLBCenc_inst,
    /* (i) Encoder instance */
    iLBC_bits *iLBC_encbits, /* (i/o) Encoded bits (outputs idxForMax
                                   and idxVec, uses state_first as
                                   input) */
    WebRtc_Word16 *in,     /* (i) vector to encode */
    WebRtc_Word16 *weightDenum   /* (i) denominator of synthesis filter */
                            ) {
  WebRtc_Word16 *syntOut;
  WebRtc_Word16 quantLen[2];

  /* Stack based */
  WebRtc_Word16 syntOutBuf[LPC_FILTERORDER+STATE_SHORT_LEN_30MS];
  WebRtc_Word16 in_weightedVec[STATE_SHORT_LEN_30MS+LPC_FILTERORDER];
  WebRtc_Word16 *in_weighted = &in_weightedVec[LPC_FILTERORDER];

  /* Initialize the buffers */
  WebRtcSpl_MemSetW16(syntOutBuf, 0, LPC_FILTERORDER+STATE_SHORT_LEN_30MS);
  syntOut = &syntOutBuf[LPC_FILTERORDER];
  /* Start with zero state */
  WebRtcSpl_MemSetW16(in_weightedVec, 0, LPC_FILTERORDER);

  /* Perform the quantization loop in two sections of length quantLen[i],
     where the perceptual weighting filter is updated at the subframe
     border */

  if (iLBC_encbits->state_first) {
    quantLen[0]=SUBL;
    quantLen[1]=iLBCenc_inst->state_short_len-SUBL;
  } else {
    quantLen[0]=iLBCenc_inst->state_short_len-SUBL;
    quantLen[1]=SUBL;
  }

  /* Calculate the weighted residual, switch perceptual weighting
     filter at the subframe border */
  WebRtcSpl_FilterARFastQ12(
      in, in_weighted,
      weightDenum, LPC_FILTERORDER+1, quantLen[0]);
  WebRtcSpl_FilterARFastQ12(
      &in[quantLen[0]], &in_weighted[quantLen[0]],
      &weightDenum[LPC_FILTERORDER+1], LPC_FILTERORDER+1, quantLen[1]);

  WebRtcIlbcfix_AbsQuantLoop(
      syntOut,
      in_weighted,
      weightDenum,
      quantLen,
      iLBC_encbits->idxVec);

}
