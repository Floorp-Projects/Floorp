/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This function "unmutes" a vector on a sample by sample basis.
 */

#include "dsp_helpfunctions.h"

#include "signal_processing_library.h"


void WebRtcNetEQ_UnmuteSignal(int16_t *pw16_inVec, int16_t *startMuteFact,
                              int16_t *pw16_outVec, int16_t unmuteFact,
                              int16_t N)
{
    int i;
    uint16_t w16_tmp;
    int32_t w32_tmp;

    w16_tmp = (uint16_t) *startMuteFact;
    w32_tmp = WEBRTC_SPL_LSHIFT_W32((int32_t)w16_tmp,6) + 32;
    for (i = 0; i < N; i++)
    {
        pw16_outVec[i]
            = (int16_t) ((WEBRTC_SPL_MUL_16_16(w16_tmp, pw16_inVec[i]) + 8192) >> 14);
        w32_tmp += unmuteFact;
        w32_tmp = WEBRTC_SPL_MAX(0, w32_tmp);
        w16_tmp = (uint16_t) WEBRTC_SPL_RSHIFT_W32(w32_tmp, 6); /* 20 - 14 = 6 */
        w16_tmp = WEBRTC_SPL_MIN(16384, w16_tmp);
    }
    *startMuteFact = (int16_t) w16_tmp;
}

