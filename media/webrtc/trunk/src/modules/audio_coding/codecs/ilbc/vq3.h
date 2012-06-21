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

 WebRtcIlbcfix_Vq3.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_VQ3_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_VQ3_H_

#include "typedefs.h"

/*----------------------------------------------------------------*
 *  Vector quantization of order 3 (based on MSE)
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_Vq3(
    WebRtc_Word16 *Xq,  /* (o) the quantized vector (Q13) */
    WebRtc_Word16 *index, /* (o) the quantization index */
    WebRtc_Word16 *CB,  /* (i) the vector quantization codebook (Q13) */
    WebRtc_Word16 *X,  /* (i) the vector to quantize (Q13) */
    WebRtc_Word16 n_cb  /* (i) the number of vectors in the codebook */
                       );

#endif
