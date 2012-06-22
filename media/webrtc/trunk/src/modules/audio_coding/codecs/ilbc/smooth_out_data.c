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

 WebRtcIlbcfix_Smooth_odata.c

******************************************************************/

#include "defines.h"
#include "constants.h"

WebRtc_Word32 WebRtcIlbcfix_Smooth_odata(
    WebRtc_Word16 *odata,
    WebRtc_Word16 *psseq,
    WebRtc_Word16 *surround,
    WebRtc_Word16 C)
{
  int i;

  WebRtc_Word16 err;
  WebRtc_Word32 errs;

  for(i=0;i<80;i++) {
    odata[i]= (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32(
        (WEBRTC_SPL_MUL_16_16(C, surround[i])+1024), 11);
  }

  errs=0;
  for(i=0;i<80;i++) {
    err=(WebRtc_Word16)WEBRTC_SPL_RSHIFT_W16((psseq[i]-odata[i]), 3);
    errs+=WEBRTC_SPL_MUL_16_16(err, err); /* errs in Q-6 */
  }

  return errs;
}
