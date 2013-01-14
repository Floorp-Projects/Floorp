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

 WebRtcIlbcfix_Enhancer.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_ENHANCER_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_ENHANCER_H_

#include "defines.h"

/*----------------------------------------------------------------*
 * perform enhancement on idata+centerStartPos through
 * idata+centerStartPos+ENH_BLOCKL-1
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_Enhancer(
    WebRtc_Word16 *odata,   /* (o) smoothed block, dimension blockl */
    WebRtc_Word16 *idata,   /* (i) data buffer used for enhancing */
    WebRtc_Word16 idatal,   /* (i) dimension idata */
    WebRtc_Word16 centerStartPos, /* (i) first sample current block within idata */
    WebRtc_Word16 *period,   /* (i) pitch period array (pitch bward-in time) */
    WebRtc_Word16 *plocs,   /* (i) locations where period array values valid */
    WebRtc_Word16 periodl   /* (i) dimension of period and plocs */
                            );

#endif
