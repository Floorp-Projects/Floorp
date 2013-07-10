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

void Utility::MixWithSat(int16_t target[],
                         int target_channel,
                         const int16_t source[],
                         int source_channel,
                         int source_len)
{
    assert((target_channel == 1) || (target_channel == 2));
    assert((source_channel == 1) || (source_channel == 2));
    assert(source_len <= kMaxTargetLen);

    if ((target_channel == 2) && (source_channel == 1))
    {
        // Convert source from mono to stereo.
        int32_t left = 0;
        int32_t right = 0;
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
        int32_t temp = 0;
        for (int i = 0; i < source_len/2; ++i) {
          temp = ((source[i*2] + source[i*2 + 1])>>1) + target[i];
          target[i] = WebRtcSpl_SatW32ToW16(temp);
        }
    }
    else
    {
        int32_t temp = 0;
        for (int i = 0; i < source_len; ++i) {
          temp = source[i] + target[i];
          target[i] = WebRtcSpl_SatW32ToW16(temp);
        }
    }
}

void Utility::MixSubtractWithSat(int16_t target[],
                                 const int16_t source[],
                                 uint16_t len)
{
    int32_t temp(0);
    for (int i = 0; i < len; i++)
    {
        temp = target[i] - source[i];
        if (temp > 32767)
            target[i] = 32767;
        else if (temp < -32768)
            target[i] = -32768;
        else
            target[i] = (int16_t) temp;
    }
}

void Utility::MixAndScaleWithSat(int16_t target[],
                                 const int16_t source[], float scale,
                                 uint16_t len)
{
    int32_t temp(0);
    for (int i = 0; i < len; i++)
    {
        temp = (int32_t) (target[i] + scale * source[i]);
        if (temp > 32767)
            target[i] = 32767;
        else if (temp < -32768)
            target[i] = -32768;
        else
            target[i] = (int16_t) temp;
    }
}

void Utility::Scale(int16_t vector[], float scale, uint16_t len)
{
    for (int i = 0; i < len; i++)
    {
        vector[i] = (int16_t) (scale * vector[i]);
    }
}

void Utility::ScaleWithSat(int16_t vector[], float scale,
                           uint16_t len)
{
    int32_t temp(0);
    for (int i = 0; i < len; i++)
    {
        temp = (int32_t) (scale * vector[i]);
        if (temp > 32767)
            vector[i] = 32767;
        else if (temp < -32768)
            vector[i] = -32768;
        else
            vector[i] = (int16_t) temp;
    }
}

} // namespace voe

} // namespace webrtc
