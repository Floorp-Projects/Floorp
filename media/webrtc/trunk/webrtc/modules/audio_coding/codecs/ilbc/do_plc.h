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

 WebRtcIlbcfix_DoThePlc.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DO_PLC_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DO_PLC_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  Packet loss concealment routine. Conceals a residual signal
 *  and LP parameters. If no packet loss, update state.
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_DoThePlc(
    WebRtc_Word16 *PLCresidual,  /* (o) concealed residual */
    WebRtc_Word16 *PLClpc,    /* (o) concealed LP parameters */
    WebRtc_Word16 PLI,     /* (i) packet loss indicator
                                                           0 - no PL, 1 = PL */
    WebRtc_Word16 *decresidual,  /* (i) decoded residual */
    WebRtc_Word16 *lpc,    /* (i) decoded LPC (only used for no PL) */
    WebRtc_Word16 inlag,    /* (i) pitch lag */
    iLBC_Dec_Inst_t *iLBCdec_inst
    /* (i/o) decoder instance */
                            );

#endif
