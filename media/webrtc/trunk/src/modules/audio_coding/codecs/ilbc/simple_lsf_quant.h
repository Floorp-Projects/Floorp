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

 WebRtcIlbcfix_SimpleLsfQ.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SIMPLE_LSF_QUANT_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SIMPLE_LSF_QUANT_H_

#include "defines.h"

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
                              );

#endif
