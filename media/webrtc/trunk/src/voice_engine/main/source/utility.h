/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Contains functions often used by different parts of VoiceEngine.
 */

#ifndef WEBRTC_VOICE_ENGINE_UTILITY_H
#define WEBRTC_VOICE_ENGINE_UTILITY_H

#include "typedefs.h"
#include "voice_engine_defines.h"

namespace webrtc
{

class Module;

namespace voe
{

class Utility
{
public:
    static void MixWithSat(WebRtc_Word16 target[],
                           int target_channel,
                           const WebRtc_Word16 source[],
                           int source_channel,
                           int source_len);

    static void MixSubtractWithSat(WebRtc_Word16 target[],
                                   const WebRtc_Word16 source[],
                                   WebRtc_UWord16 len);

    static void MixAndScaleWithSat(WebRtc_Word16 target[],
                                   const WebRtc_Word16 source[],
                                   float scale,
                                   WebRtc_UWord16 len);

    static void Scale(WebRtc_Word16 vector[], float scale, WebRtc_UWord16 len);

    static void ScaleWithSat(WebRtc_Word16 vector[],
                             float scale,
                             WebRtc_UWord16 len);
};

} // namespace voe

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_UTILITY_H
