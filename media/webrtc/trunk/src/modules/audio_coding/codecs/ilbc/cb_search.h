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

 WebRtcIlbcfix_CbSearch.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_SEARCH_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_SEARCH_H_

void WebRtcIlbcfix_CbSearch(
    iLBC_Enc_Inst_t *iLBCenc_inst,
    /* (i) the encoder state structure */
    WebRtc_Word16 *index,  /* (o) Codebook indices */
    WebRtc_Word16 *gain_index, /* (o) Gain quantization indices */
    WebRtc_Word16 *intarget, /* (i) Target vector for encoding */
    WebRtc_Word16 *decResidual,/* (i) Decoded residual for codebook construction */
    WebRtc_Word16 lMem,  /* (i) Length of buffer */
    WebRtc_Word16 lTarget,  /* (i) Length of vector */
    WebRtc_Word16 *weightDenum,/* (i) weighting filter coefficients in Q12 */
    WebRtc_Word16 block  /* (i) the subblock number */
                            );

#endif
