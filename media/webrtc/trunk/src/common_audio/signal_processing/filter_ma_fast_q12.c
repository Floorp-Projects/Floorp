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
 * This file contains the function WebRtcSpl_FilterMAFastQ12().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

void WebRtcSpl_FilterMAFastQ12(WebRtc_Word16* in_ptr,
                               WebRtc_Word16* out_ptr,
                               WebRtc_Word16* B,
                               WebRtc_Word16 B_length,
                               WebRtc_Word16 length)
{
    WebRtc_Word32 o;
    int i, j;
    for (i = 0; i < length; i++)
    {
        G_CONST WebRtc_Word16* b_ptr = &B[0];
        G_CONST WebRtc_Word16* x_ptr = &in_ptr[i];

        o = (WebRtc_Word32)0;

        for (j = 0; j < B_length; j++)
        {
            o += WEBRTC_SPL_MUL_16_16(*b_ptr++, *x_ptr--);
        }

        // If output is higher than 32768, saturate it. Same with negative side
        // 2^27 = 134217728, which corresponds to 32768 in Q12

        // Saturate the output
        o = WEBRTC_SPL_SAT((WebRtc_Word32)134215679, o, (WebRtc_Word32)-134217728);

        *out_ptr++ = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12);
    }
    return;
}
