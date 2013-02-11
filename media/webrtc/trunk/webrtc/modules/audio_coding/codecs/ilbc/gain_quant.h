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

 WebRtcIlbcfix_GainQuant.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_GAIN_QUANT_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_GAIN_QUANT_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  quantizer for the gain in the gain-shape coding of residual
 *---------------------------------------------------------------*/

WebRtc_Word16 WebRtcIlbcfix_GainQuant( /* (o) quantized gain value */
    WebRtc_Word16 gain, /* (i) gain value Q14 */
    WebRtc_Word16 maxIn, /* (i) maximum of gain value Q14 */
    WebRtc_Word16 stage, /* (i) The stage of the search */
    WebRtc_Word16 *index /* (o) quantization index */
                                       );

#endif
