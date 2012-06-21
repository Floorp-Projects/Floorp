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

 WebRtcIlbcfix_AbsQuantLoop.c

******************************************************************/

#include "defines.h"
#include "constants.h"
#include "sort_sq.h"

void WebRtcIlbcfix_AbsQuantLoop(
    WebRtc_Word16 *syntOutIN,
    WebRtc_Word16 *in_weightedIN,
    WebRtc_Word16 *weightDenumIN,
    WebRtc_Word16 *quantLenIN,
    WebRtc_Word16 *idxVecIN
                                )
{
  int n, k1, k2;
  WebRtc_Word16 index;
  WebRtc_Word32 toQW32;
  WebRtc_Word32 toQ32;
  WebRtc_Word16 tmp16a;
  WebRtc_Word16 xq;

  WebRtc_Word16 *syntOut   = syntOutIN;
  WebRtc_Word16 *in_weighted  = in_weightedIN;
  WebRtc_Word16 *weightDenum  = weightDenumIN;
  WebRtc_Word16 *quantLen  = quantLenIN;
  WebRtc_Word16 *idxVec   = idxVecIN;

  n=0;

  for(k1=0;k1<2;k1++) {
    for(k2=0;k2<quantLen[k1];k2++){

      /* Filter to get the predicted value */
      WebRtcSpl_FilterARFastQ12(
          syntOut, syntOut,
          weightDenum, LPC_FILTERORDER+1, 1);

      /* the quantizer */
      toQW32 = (WebRtc_Word32)(*in_weighted) - (WebRtc_Word32)(*syntOut);

      toQ32 = (((WebRtc_Word32)toQW32)<<2);

      if (toQ32 > 32767) {
        toQ32 = (WebRtc_Word32) 32767;
      } else if (toQ32 < -32768) {
        toQ32 = (WebRtc_Word32) -32768;
      }

      /* Quantize the state */
      if (toQW32<(-7577)) {
        /* To prevent negative overflow */
        index=0;
      } else if (toQW32>8151) {
        /* To prevent positive overflow */
        index=7;
      } else {
        /* Find the best quantization index
           (state_sq3Tbl is in Q13 and toQ is in Q11)
        */
        WebRtcIlbcfix_SortSq(&xq, &index,
                             (WebRtc_Word16)toQ32,
                             WebRtcIlbcfix_kStateSq3, 8);
      }

      /* Store selected index */
      (*idxVec++) = index;

      /* Compute decoded sample and update of the prediction filter */
      tmp16a = ((WebRtcIlbcfix_kStateSq3[index] + 2 ) >> 2);

      *syntOut     = (WebRtc_Word16) (tmp16a + (WebRtc_Word32)(*in_weighted) - toQW32);

      n++;
      syntOut++; in_weighted++;
    }
    /* Update perceptual weighting filter at subframe border */
    weightDenum += 11;
  }
}
