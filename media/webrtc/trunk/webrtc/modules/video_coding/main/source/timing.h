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

class TickTimeBase;
class VCMTimestampExtrapolator;

class VCMTiming
{
public:
    // The primary timing component should be passed
    // if this is the dual timing component.
    VCMTiming(TickTimeBase* clock,
              WebRtc_Word32 vcmId = 0,
              WebRtc_Word32 timingId = 0,
              VCMTiming* masterTiming = NULL);
    ~VCMTiming();

    // Resets the timing to the initial state.
    void Reset(WebRtc_Word64 nowMs = -1);
    void ResetDecodeTime();

    // The amount of time needed to render an image. Defaults to 10 ms.
    void SetRenderDelay(WebRtc_UWord32 renderDelayMs);

    // The minimum time the video must be delayed on the receiver to
    // get the desired jitter buffer level.
    void SetRequiredDelay(WebRtc_UWord32 requiredDelayMs);

    // Minimum total delay required to sync video with audio.
    void SetMinimumTotalDelay(WebRtc_UWord32 minTotalDelayMs);

    // Increases or decreases the current delay to get closer to the target delay.
    // Calculates how long it has been since the previous call to this function,
    // and increases/decreases the delay in proportion to the time difference.
    void UpdateCurrentDelay(WebRtc_UWord32 frameTimestamp);

    // Increases or decreases the current delay to get closer to the target delay.
    // Given the actual decode time in ms and the render time in ms for a frame, this
    // function calculates how late the frame is and increases the delay accordingly.
    void UpdateCurrentDelay(WebRtc_Word64 renderTimeMs, WebRtc_Word64 actualDecodeTimeMs);

    // Stops the decoder timer, should be called when the decoder returns a frame
    // or when the decoded frame callback is called.
    WebRtc_Word32 StopDecodeTimer(WebRtc_UWord32 timeStamp,
                                  WebRtc_Word64 startTimeMs,
                                  WebRtc_Word64 nowMs);

    // Used to report that a frame is passed to decoding. Updates the timestamp filter
    // which is used to map between timestamps and receiver system time.
    void IncomingTimestamp(WebRtc_UWord32 timeStamp, WebRtc_Word64 lastPacketTimeMs);

    // Returns the receiver system time when the frame with timestamp frameTimestamp
    // should be rendered, assuming that the system time currently is nowMs.
    WebRtc_Word64 RenderTimeMs(WebRtc_UWord32 frameTimestamp, WebRtc_Word64 nowMs) const;

    // Returns the maximum time in ms that we can wait for a frame to become complete
    // before we must pass it to the decoder.
    WebRtc_UWord32 MaxWaitingTime(WebRtc_Word64 renderTimeMs, WebRtc_Word64 nowMs) const;

    // Returns the current target delay which is required delay + decode time + render
    // delay.
    WebRtc_UWord32 TargetVideoDelay() const;

    // Calculates whether or not there is enough time to decode a frame given a
    // certain amount of processing time.
    bool EnoughTimeToDecode(WebRtc_UWord32 availableProcessingTimeMs) const;

    enum { kDefaultRenderDelayMs = 10 };
    enum { kDelayMaxChangeMsPerS = 100 };

protected:
    WebRtc_Word32 MaxDecodeTimeMs(FrameType frameType = kVideoFrameDelta) const;
    WebRtc_Word64 RenderTimeMsInternal(WebRtc_UWord32 frameTimestamp,
                                       WebRtc_Word64 nowMs) const;
    WebRtc_UWord32 TargetDelayInternal() const;

private:
    CriticalSectionWrapper*       _critSect;
    WebRtc_Word32                 _vcmId;
    TickTimeBase*                 _clock;
    WebRtc_Word32                 _timingId;
    bool                          _master;
    VCMTimestampExtrapolator*     _tsExtrapolator;
    VCMCodecTimer                 _codecTimer;
    WebRtc_UWord32                _renderDelayMs;
    WebRtc_UWord32                _minTotalDelayMs;
    WebRtc_UWord32                _requiredDelayMs;
    WebRtc_UWord32                _currentDelayMs;
    WebRtc_UWord32                _prevFrameTimestamp;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_TIMING_H_
