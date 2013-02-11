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

 WebRtcIlbcfix_MyCorr.c

******************************************************************/

#include "defines.h"

/*----------------------------------------------------------------*
 * compute cross correlation between sequences
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_MyCorr(
    WebRtc_Word32 *corr,  /* (o) correlation of seq1 and seq2 */
    WebRtc_Word16 *seq1,  /* (i) first sequence */
    WebRtc_Word16 dim1,  /* (i) dimension first seq1 */
    const WebRtc_Word16 *seq2, /* (i) second sequence */
    WebRtc_Word16 dim2   /* (i) dimension seq2 */
                          ){
  WebRtc_Word16 max, scale, loops;

  /* Calculate correlation between the two sequences. Scale the
     result of the multiplcication to maximum 26 bits in order
     to avoid overflow */
  max=WebRtcSpl_MaxAbsValueW16(seq1, dim1);
  scale=WebRtcSpl_GetSizeInBits(max);

  scale = (WebRtc_Word16)(WEBRTC_SPL_MUL_16_16(2,scale)-26);
  if (scale<0) {
    scale=0;
  }

  loops=dim1-dim2+1;

  /* Calculate the cross correlations */
  WebRtcSpl_CrossCorrelation(corr, (WebRtc_Word16*)seq2, seq1, dim2, loops, scale, 1);

  return;
}
