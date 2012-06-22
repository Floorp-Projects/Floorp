/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "utility.h"

#include "module.h"
#include "trace.h"
#include "signal_processing_library.h"

namespace webrtc
{

namespace voe
{
enum{kMaxTargetLen = 2*32*10}; // stereo 32KHz 10ms

void Utility::MixWithSat(WebRtc_Word16 target[],
                         int target_channel,
                         const WebRtc_Word16 source[],
                         int source_channel,
                         int source_len)
{
    assert((target_channel == 1) || (target_channel == 2));
    assert((source_channel == 1) || (source_channel == 2));
    assert(source_len <= kMaxTargetLen);

    if ((target_channel == 2) && (source_channel == 1))
    {
        // Convert source from mono to stereo.
        WebRtc_Word32 left = 0;
        WebRtc_Word32 right = 0;
        for (int i = 0; i < source_len; ++i) {
            left  = source[i] + target[i*2];
            right = source[i] + target[i*2 + 1];
            target[i*2]     = WebRtcSpl_SatW32ToW16(left);
            target[i*2 + 1] = WebRtcSpl_SatW32ToW16(right);
        }
    }
    else if ((target_channel == 1) && (source_channel == 2))
    {
        // Convert source from stereo to mono.
        WebRtc_Word32 temp = 0;
        for (int i = 0; i < source_len/2; ++i) {
          temp = ((source[i*2] + source[i*2 + 1])>>1) + target[i];
          target[i] = WebRtcSpl_SatW32ToW16(temp);
        }
    }
    else
    {
        WebRtc_Word32 temp = 0;
        for (int i = 0; i < source_len; ++i) {
          temp = source[i] + target[i];
          target[i] = WebRtcSpl_SatW32ToW16(temp);
        }
    }
}

void Utility::MixSubtractWithSat(WebRtc_Word16 target[],
                                 const WebRtc_Word16 source[],
                                 WebRtc_UWord16 len)
{
    WebRtc_Word32 temp(0);
    for (int i = 0; i < len; i++)
    {
        temp = target[i] - source[i];
        if (temp > 32767)
            target[i] = 32767;
        else if (temp < -32768)
            target[i] = -32768;
        else
            target[i] = (WebRtc_Word16) temp;
    }
}

void Utility::MixAndScaleWithSat(WebRtc_Word16 target[],
                                 const WebRtc_Word16 source[], float scale,
                                 WebRtc_UWord16 len)
{
    WebRtc_Word32 temp(0);
    for (int i = 0; i < len; i++)
    {
        temp = (WebRtc_Word32) (target[i] + scale * source[i]);
        if (temp > 32767)
            target[i] = 32767;
        else if (temp < -32768)
            target[i] = -32768;
        else
            target[i] = (WebRtc_Word16) temp;
    }
}

void Utility::Scale(WebRtc_Word16 vector[], float scale, WebRtc_UWord16 len)
{
    for (int i = 0; i < len; i++)
    {
        vector[i] = (WebRtc_Word16) (scale * vector[i]);
    }
}

void Utility::ScaleWithSat(WebRtc_Word16 vector[], float scale,
                           WebRtc_UWord16 len)
{
    WebRtc_Word32 temp(0);
    for (int i = 0; i < len; i++)
    {
        temp = (WebRtc_Word32) (scale * vector[i]);
        if (temp > 32767)
            vector[i] = 32767;
        else if (temp < -32768)
            vector[i] = -32768;
        else
            vector[i] = (WebRtc_Word16) temp;
    }
}

} // namespace voe

} // namespace webrtc
