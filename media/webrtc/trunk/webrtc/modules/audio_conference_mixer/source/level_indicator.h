/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_LEVEL_INDICATOR_H_
#define WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_LEVEL_INDICATOR_H_

#include "typedefs.h"

namespace webrtc {
class LevelIndicator
{
public:
    enum{TICKS_BEFORE_CALCULATION = 10};

    LevelIndicator();
    ~LevelIndicator();

    // Updates the level.
    void ComputeLevel(const int16_t* speech,
                      const uint16_t nrOfSamples);

    int32_t GetLevel();
private:
    int32_t  _max;
    uint32_t _count;
    uint32_t _currentLevel;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_LEVEL_INDICATOR_H_
