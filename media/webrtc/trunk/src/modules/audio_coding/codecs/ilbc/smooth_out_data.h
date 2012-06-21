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

 WebRtcIlbcfix_Smooth_odata.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SMOOTH_OUT_DATA_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SMOOTH_OUT_DATA_H_

#include "defines.h"

/*----------------------------------------------------------------*
 * help function to WebRtcIlbcfix_Smooth()
 *---------------------------------------------------------------*/

WebRtc_Word32 WebRtcIlbcfix_Smooth_odata(
    WebRtc_Word16 *odata,
    WebRtc_Word16 *psseq,
    WebRtc_Word16 *surround,
    WebRtc_Word16 C);


#endif
