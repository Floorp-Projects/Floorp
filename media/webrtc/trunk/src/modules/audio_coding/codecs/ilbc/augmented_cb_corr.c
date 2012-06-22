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

 WebRtcIlbcfix_AugmentedCbCorr.c

******************************************************************/

#include "defines.h"
#include "constants.h"
#include "augmented_cb_corr.h"

void WebRtcIlbcfix_AugmentedCbCorr(
    WebRtc_Word16 *target,   /* (i) Target vector */
    WebRtc_Word16 *buffer,   /* (i) Memory buffer */
    WebRtc_Word16 *interpSamples, /* (i) buffer with
                                     interpolated samples */
    WebRtc_Word32 *crossDot,  /* (o) The cross correlation between
                                 the target and the Augmented
                                 vector */
    WebRtc_Word16 low,    /* (i) Lag to start from (typically
                             20) */
    WebRtc_Word16 high,   /* (i) Lag to end at (typically 39) */
    WebRtc_Word16 scale)   /* (i) Scale factor to use for
                              the crossDot */
{
  int lagcount;
  WebRtc_Word16 ilow;
  WebRtc_Word16 *targetPtr;
  WebRtc_Word32 *crossDotPtr;
  WebRtc_Word16 *iSPtr=interpSamples;

  /* Calculate the correlation between the target and the
     interpolated codebook. The correlation is calculated in
     3 sections with the interpolated part in the middle */
  crossDotPtr=crossDot;
  for (lagcount=low; lagcount<=high; lagcount++) {

    ilow = (WebRtc_Word16) (lagcount-4);

    /* Compute dot product for the first (lagcount-4) samples */
    (*crossDotPtr) = WebRtcSpl_DotProductWithScale(target, buffer-lagcount, ilow, scale);

    /* Compute dot product on the interpolated samples */
    (*crossDotPtr) += WebRtcSpl_DotProductWithScale(target+ilow, iSPtr, 4, scale);
    targetPtr = target + lagcount;
    iSPtr += lagcount-ilow;

    /* Compute dot product for the remaining samples */
    (*crossDotPtr) += WebRtcSpl_DotProductWithScale(targetPtr, buffer-lagcount, SUBL-lagcount, scale);
    crossDotPtr++;
  }
}
