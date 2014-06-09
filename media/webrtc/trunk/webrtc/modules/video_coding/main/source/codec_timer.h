/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODEC_TIMER_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODEC_TIMER_H_

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc
{

// MAX_HISTORY_SIZE * SHORT_FILTER_MS defines the window size in milliseconds
#define MAX_HISTORY_SIZE 10
#define SHORT_FILTER_MS 1000

class VCMShortMaxSample
{
public:
    VCMShortMaxSample() : shortMax(0), timeMs(-1) {};

    int32_t     shortMax;
    int64_t     timeMs;
};

class VCMCodecTimer
{
public:
    VCMCodecTimer();

    // Updates and returns the max filtered decode time.
    int32_t StopTimer(int64_t startTimeMs, int64_t nowMs);

    // Empty the list of timers.
    void Reset();

    // Get the required decode time in ms.
    int32_t RequiredDecodeTimeMs(FrameType frameType) const;

private:
    void UpdateMaxHistory(int32_t decodeTime, int64_t now);
    void MaxFilter(int32_t newTime, int64_t nowMs);
    void ProcessHistory(int64_t nowMs);

    int32_t                     _filteredMax;
    // The number of samples ignored so far.
    int32_t                     _ignoredSampleCount;
    int32_t                     _shortMax;
    VCMShortMaxSample           _history[MAX_HISTORY_SIZE];

};

}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_CODEC_TIMER_H_
