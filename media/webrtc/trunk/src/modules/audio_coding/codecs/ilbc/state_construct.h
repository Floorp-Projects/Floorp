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

 WebRtcIlbcfix_StateConstruct.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_STATE_CONSTRUCT_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_STATE_CONSTRUCT_H_

/*----------------------------------------------------------------*
 *  Generate the start state from the quantized indexes
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_StateConstruct(
    WebRtc_Word16 idxForMax,   /* (i) 6-bit index for the quantization of
                                           max amplitude */
    WebRtc_Word16 *idxVec,   /* (i) vector of quantization indexes */
    WebRtc_Word16 *syntDenum,  /* (i) synthesis filter denumerator */
    WebRtc_Word16 *Out_fix,  /* (o) the decoded state vector */
    WebRtc_Word16 len    /* (i) length of a state vector */
                                  );

#endif
