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

 WebRtcIlbcfix_SplitVq.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SPLIT_VQ_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SPLIT_VQ_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  split vector quantization
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_SplitVq(
    WebRtc_Word16 *qX,  /* (o) the quantized vector in Q13 */
    WebRtc_Word16 *index, /* (o) a vector of indexes for all vector
                                   codebooks in the split */
    WebRtc_Word16 *X,  /* (i) the vector to quantize */
    WebRtc_Word16 *CB,  /* (i) the quantizer codebook in Q13 */
    WebRtc_Word16 *dim, /* (i) the dimension of X and qX */
    WebRtc_Word16 *cbsize /* (i) the number of vectors in the codebook */
                           );

#endif
