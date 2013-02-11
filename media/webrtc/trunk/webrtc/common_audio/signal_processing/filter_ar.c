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
 * This file contains the function WebRtcSpl_FilterAR().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

int WebRtcSpl_FilterAR(G_CONST WebRtc_Word16* a,
                       int a_length,
                       G_CONST WebRtc_Word16* x,
                       int x_length,
                       WebRtc_Word16* state,
                       int state_length,
                       WebRtc_Word16* state_low,
                       int state_low_length,
                       WebRtc_Word16* filtered,
                       WebRtc_Word16* filtered_low,
                       int filtered_low_length)
{
    WebRtc_Word32 o;
    WebRtc_Word32 oLOW;
    int i, j, stop;
    G_CONST WebRtc_Word16* x_ptr = &x[0];
    WebRtc_Word16* filteredFINAL_ptr = filtered;
    WebRtc_Word16* filteredFINAL_LOW_ptr = filtered_low;

    for (i = 0; i < x_length; i++)
    {
        // Calculate filtered[i] and filtered_low[i]
        G_CONST WebRtc_Word16* a_ptr = &a[1];
        WebRtc_Word16* filtered_ptr = &filtered[i - 1];
        WebRtc_Word16* filtered_low_ptr = &filtered_low[i - 1];
        WebRtc_Word16* state_ptr = &state[state_length - 1];
        WebRtc_Word16* state_low_ptr = &state_low[state_length - 1];

        o = (WebRtc_Word32)(*x_ptr++) << 12;
        oLOW = (WebRtc_Word32)0;

        stop = (i < a_length) ? i + 1 : a_length;
        for (j = 1; j < stop; j++)
        {
            o -= WEBRTC_SPL_MUL_16_16(*a_ptr, *filtered_ptr--);
            oLOW -= WEBRTC_SPL_MUL_16_16(*a_ptr++, *filtered_low_ptr--);
        }
        for (j = i + 1; j < a_length; j++)
        {
            o -= WEBRTC_SPL_MUL_16_16(*a_ptr, *state_ptr--);
            oLOW -= WEBRTC_SPL_MUL_16_16(*a_ptr++, *state_low_ptr--);
        }

        o += (oLOW >> 12);
        *filteredFINAL_ptr = (WebRtc_Word16)((o + (WebRtc_Word32)2048) >> 12);
        *filteredFINAL_LOW_ptr++ = (WebRtc_Word16)(o - ((WebRtc_Word32)(*filteredFINAL_ptr++)
                << 12));
    }

    // Save the filter state
    if (x_length >= state_length)
    {
        WebRtcSpl_CopyFromEndW16(filtered, x_length, a_length - 1, state);
        WebRtcSpl_CopyFromEndW16(filtered_low, x_length, a_length - 1, state_low);
    } else
    {
        for (i = 0; i < state_length - x_length; i++)
        {
            state[i] = state[i + x_length];
            state_low[i] = state_low[i + x_length];
        }
        for (i = 0; i < x_length; i++)
        {
            state[state_length - x_length + i] = filtered[i];
            state[state_length - x_length + i] = filtered_low[i];
        }
    }

    return x_length;
}
