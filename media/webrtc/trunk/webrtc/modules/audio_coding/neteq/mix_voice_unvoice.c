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
 * This function mixes a voiced signal with an unvoiced signal and
 * updates the weight on a sample by sample basis.
 */

#include "dsp_helpfunctions.h"

#include "signal_processing_library.h"

void WebRtcNetEQ_MixVoiceUnvoice(int16_t *pw16_outData, int16_t *pw16_voicedVec,
                                 int16_t *pw16_unvoicedVec,
                                 int16_t *w16_current_vfraction,
                                 int16_t w16_vfraction_change, int16_t N)
{
    int i;
    int16_t w16_tmp2;
    int16_t vfraction = *w16_current_vfraction;

    w16_tmp2 = 16384 - vfraction;
    for (i = 0; i < N; i++)
    {
        pw16_outData[i] = (int16_t) WEBRTC_SPL_RSHIFT_W32(
            WEBRTC_SPL_MUL_16_16(vfraction, pw16_voicedVec[i]) +
            WEBRTC_SPL_MUL_16_16(w16_tmp2, pw16_unvoicedVec[i]) + 8192,
            14);
        vfraction -= w16_vfraction_change;
        w16_tmp2 += w16_vfraction_change;
    }
    *w16_current_vfraction = vfraction;
}

