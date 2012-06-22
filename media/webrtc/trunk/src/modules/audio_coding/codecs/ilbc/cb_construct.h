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

 WebRtcIlbcfix_CbConstruct.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_CONSTRUCT_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_CONSTRUCT_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  Construct decoded vector from codebook and gains.
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_CbConstruct(
    WebRtc_Word16 *decvector,  /* (o) Decoded vector */
    WebRtc_Word16 *index,   /* (i) Codebook indices */
    WebRtc_Word16 *gain_index,  /* (i) Gain quantization indices */
    WebRtc_Word16 *mem,   /* (i) Buffer for codevector construction */
    WebRtc_Word16 lMem,   /* (i) Length of buffer */
    WebRtc_Word16 veclen   /* (i) Length of vector */
                               );


#endif
