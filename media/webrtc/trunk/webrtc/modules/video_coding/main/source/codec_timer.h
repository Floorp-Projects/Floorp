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

#include "typedefs.h"
#include "module_common_types.h"

namespace webrtc
{

// MAX_HISTORY_SIZE * SHORT_FILTER_MS defines the window size in milliseconds
#define MAX_HISTORY_SIZE 20
#define SHORT_FILTER_MS 1000

class VCMShortMaxSample
{
public:
    VCMShortMaxSample() : shortMax(0), timeMs(-1) {};

    WebRtc_Word32     shortMax;
    WebRtc_Word64     timeMs;
};

class VCMCodecTimer
{
public:
    VCMCodecTimer();

    // Updates and returns the max filtered decode time.
    WebRtc_Word32 StopTimer(WebRtc_Word64 startTimeMs, WebRtc_Word64 nowMs);

    // Empty the list of timers.
    void Reset();

    // Get the required decode time in ms.
    WebRtc_Word32 RequiredDecodeTimeMs(FrameType frameType) const;

private:
    void UpdateMaxHistory(WebRtc_Word32 decodeTime, WebRtc_Word64 now);
    void MaxFilter(WebRtc_Word32 newTime, WebRtc_Word64 nowMs);
    void ProcessHistory(WebRtc_Word64 nowMs);

    WebRtc_Word32                     _filteredMax;
    bool                              _firstDecodeTime;
    WebRtc_Word32                     _shortMax;
    VCMShortMaxSample                 _history[MAX_HISTORY_SIZE];

};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_CODEC_TIMER_H_
