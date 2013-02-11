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

 WebRtcIlbcfix_GetCbVec.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_GET_CD_VEC_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_GET_CD_VEC_H_

void WebRtcIlbcfix_GetCbVec(
    WebRtc_Word16 *cbvec,   /* (o) Constructed codebook vector */
    WebRtc_Word16 *mem,   /* (i) Codebook buffer */
    WebRtc_Word16 index,   /* (i) Codebook index */
    WebRtc_Word16 lMem,   /* (i) Length of codebook buffer */
    WebRtc_Word16 cbveclen   /* (i) Codebook vector length */
                            );

#endif
