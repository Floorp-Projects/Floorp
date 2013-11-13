/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// The TimeScheduler class keeps track of periodic events. It is non-drifting
// and keeps track of any missed periods so that it is possible to catch up.
// (compare to a metronome)

#ifndef WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_TIME_SCHEDULER_H_
#define WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_TIME_SCHEDULER_H_

#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {
class CriticalSectionWrapper;
class TimeScheduler
{
public:
    TimeScheduler(const uint32_t periodicityInMs);
    ~TimeScheduler();

    // Signal that a periodic event has been triggered.
    int32_t UpdateScheduler();

    // Set updateTimeInMs to the amount of time until UpdateScheduler() should
    // be called. This time will never be negative.
    int32_t TimeToNextUpdate(int32_t& updateTimeInMS) const;

private:
    CriticalSectionWrapper* _crit;

    bool _isStarted;
    TickTime _lastPeriodMark;

    uint32_t _periodicityInMs;
    int64_t  _periodicityInTicks;
    uint32_t _missedPeriods;
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_TIME_SCHEDULER_H_
