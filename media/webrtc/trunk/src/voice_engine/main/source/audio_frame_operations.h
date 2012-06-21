/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_AUDIO_FRAME_OPERATIONS_H
#define WEBRTC_VOICE_ENGINE_AUDIO_FRAME_OPERATIONS_H

#include "typedefs.h"

namespace webrtc {

class AudioFrame;

namespace voe {

class AudioFrameOperations
{
public:
    static WebRtc_Word32 MonoToStereo(AudioFrame& audioFrame);

    static WebRtc_Word32 StereoToMono(AudioFrame& audioFrame);

    static WebRtc_Word32 Mute(AudioFrame& audioFrame);

    static WebRtc_Word32 Scale(const float left,
                               const float right,
                               AudioFrame& audioFrame);

    static WebRtc_Word32 ScaleWithSat(const float scale,
                                      AudioFrame& audioFrame);
};

}  //  namespace voe

}  //  namespace webrtc

#endif  // #ifndef WEBRTC_VOICE_ENGINE_AUDIO_FRAME_OPERATIONS_H
