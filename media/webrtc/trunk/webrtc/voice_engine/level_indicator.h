/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_LEVEL_INDICATOR_H
#define WEBRTC_VOICE_ENGINE_LEVEL_INDICATOR_H

#include "typedefs.h"
#include "voice_engine_defines.h"

namespace webrtc {

class AudioFrame;
namespace voe {

class AudioLevel
{
public:
    AudioLevel();
    virtual ~AudioLevel();

    void ComputeLevel(const AudioFrame& audioFrame);

    WebRtc_Word8 Level() const;

    WebRtc_Word16 LevelFullRange() const;

    void Clear();

private:
    enum { kUpdateFrequency = 10};

    WebRtc_Word16 _absMax;
    WebRtc_Word16 _count;
    WebRtc_Word8 _currentLevel;
    WebRtc_Word16 _currentLevelFullRange;
};

}  // namespace voe

}  // namespace webrtc

#endif // WEBRTC_VOICE_ENGINE_LEVEL_INDICATOR_H
