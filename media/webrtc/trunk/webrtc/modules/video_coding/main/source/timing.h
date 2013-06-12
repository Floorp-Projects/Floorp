/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TIMING_H_
#define WEBRTC_MODULES_VIDEO_CODING_TIMING_H_

#include "typedefs.h"
#include "critical_section_wrapper.h"
#include "codec_timer.h"

namespace webrtc
{

class Clock;
class VCMTimestampExtrapolator;

class VCMTiming
{
public:
    // The primary timing component should be passed
    // if this is the dual timing component.
    VCMTiming(Clock* clock,
              int32_t vcmId = 0,
              int32_t timingId = 0,
              VCMTiming* masterTiming = NULL);
    ~VCMTiming();

    // Resets the timing to the initial state.
    void Reset(int64_t nowMs = -1);
    void ResetDecodeTime();

    // The amount of time needed to render an image. Defaults to 10 ms.
    void SetRenderDelay(uint32_t renderDelayMs);

    // The minimum time the video must be delayed on the receiver to
    // get the desired jitter buffer level.
    void SetRequiredDelay(uint32_t requiredDelayMs);

    // Minimum total delay required to sync video with audio.
    void SetMinimumTotalDelay(uint32_t minTotalDelayMs);

    // Increases or decreases the current delay to get closer to the target delay.
    // Calculates how long it has been since the previous call to this function,
    // and increases/decreases the delay in proportion to the time difference.
    void UpdateCurrentDelay(uint32_t frameTimestamp);

    // Increases or decreases the current delay to get closer to the target delay.
    // Given the actual decode time in ms and the render time in ms for a frame, this
    // function calculates how late the frame is and increases the delay accordingly.
    void UpdateCurrentDelay(int64_t renderTimeMs, int64_t actualDecodeTimeMs);

    // Stops the decoder timer, should be called when the decoder returns a frame
    // or when the decoded frame callback is called.
    int32_t StopDecodeTimer(uint32_t timeStamp,
                                  int64_t startTimeMs,
                                  int64_t nowMs);

    // Used to report that a frame is passed to decoding. Updates the timestamp filter
    // which is used to map between timestamps and receiver system time.
    void IncomingTimestamp(uint32_t timeStamp, int64_t lastPacketTimeMs);

    // Returns the receiver system time when the frame with timestamp frameTimestamp
    // should be rendered, assuming that the system time currently is nowMs.
    int64_t RenderTimeMs(uint32_t frameTimestamp, int64_t nowMs) const;

    // Returns the maximum time in ms that we can wait for a frame to become complete
    // before we must pass it to the decoder.
    uint32_t MaxWaitingTime(int64_t renderTimeMs, int64_t nowMs) const;

    // Returns the current target delay which is required delay + decode time + render
    // delay.
    uint32_t TargetVideoDelay() const;

    // Calculates whether or not there is enough time to decode a frame given a
    // certain amount of processing time.
    bool EnoughTimeToDecode(uint32_t availableProcessingTimeMs) const;

    // Set the max allowed video delay.
    void SetMaxVideoDelay(int maxVideoDelayMs);

    enum { kDefaultRenderDelayMs = 10 };
    enum { kDelayMaxChangeMsPerS = 100 };

protected:
    int32_t MaxDecodeTimeMs(FrameType frameType = kVideoFrameDelta) const;
    int64_t RenderTimeMsInternal(uint32_t frameTimestamp,
                                       int64_t nowMs) const;
    uint32_t TargetDelayInternal() const;

private:
    CriticalSectionWrapper* _critSect;
    int32_t _vcmId;
    Clock* _clock;
    int32_t _timingId;
    bool _master;
    VCMTimestampExtrapolator* _tsExtrapolator;
    VCMCodecTimer _codecTimer;
    uint32_t _renderDelayMs;
    uint32_t _minTotalDelayMs;
    uint32_t _requiredDelayMs;
    uint32_t _currentDelayMs;
    uint32_t _prevFrameTimestamp;
    int _maxVideoDelayMs;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_TIMING_H_
