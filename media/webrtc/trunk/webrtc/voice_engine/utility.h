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

#include "webrtc/typedefs.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

namespace webrtc
{

class Module;

namespace voe
{

class Utility
{
public:
    static void MixWithSat(int16_t target[],
                           int target_channel,
                           const int16_t source[],
                           int source_channel,
                           int source_len);

    static void MixSubtractWithSat(int16_t target[],
                                   const int16_t source[],
                                   uint16_t len);

    static void MixAndScaleWithSat(int16_t target[],
                                   const int16_t source[],
                                   float scale,
                                   uint16_t len);

    static void Scale(int16_t vector[], float scale, uint16_t len);

    static void ScaleWithSat(int16_t vector[],
                             float scale,
                             uint16_t len);
};

} // namespace voe

} // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_UTILITY_H
