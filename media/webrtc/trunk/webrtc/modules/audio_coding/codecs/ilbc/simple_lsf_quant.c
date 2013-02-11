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

 WebRtcIlbcfix_SimpleLsfQ.c

******************************************************************/

#include "defines.h"
#include "split_vq.h"
#include "constants.h"

/*----------------------------------------------------------------*
 *  lsf quantizer (subrutine to LPCencode)
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_SimpleLsfQ(
    WebRtc_Word16 *lsfdeq, /* (o) dequantized lsf coefficients
                                   (dimension FILTERORDER) Q13 */
    WebRtc_Word16 *index, /* (o) quantization index */
    WebRtc_Word16 *lsf, /* (i) the lsf coefficient vector to be
                           quantized (dimension FILTERORDER) Q13 */
    WebRtc_Word16 lpc_n /* (i) number of lsf sets to quantize */
                              ){

  /* Quantize first LSF with memoryless split VQ */
  WebRtcIlbcfix_SplitVq( lsfdeq, index, lsf,
                         (WebRtc_Word16*)WebRtcIlbcfix_kLsfCb, (WebRtc_Word16*)WebRtcIlbcfix_kLsfDimCb, (WebRtc_Word16*)WebRtcIlbcfix_kLsfSizeCb);

  if (lpc_n==2) {
    /* Quantize second LSF with memoryless split VQ */
    WebRtcIlbcfix_SplitVq( lsfdeq + LPC_FILTERORDER, index + LSF_NSPLIT,
                           lsf + LPC_FILTERORDER, (WebRtc_Word16*)WebRtcIlbcfix_kLsfCb,
                           (WebRtc_Word16*)WebRtcIlbcfix_kLsfDimCb, (WebRtc_Word16*)WebRtcIlbcfix_kLsfSizeCb);
  }
  return;
}
