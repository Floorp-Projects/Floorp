/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "trace.h"
#include "internal_defines.h"
#include "jitter_buffer_common.h"
#include "timing.h"
#include "timestamp_extrapolator.h"

namespace webrtc {

VCMTiming::VCMTiming(TickTimeBase* clock,
                     WebRtc_Word32 vcmId,
                     WebRtc_Word32 timingId,
                     VCMTiming* masterTiming)
:
_critSect(CriticalSectionWrapper::CreateCriticalSection()),
_vcmId(vcmId),
_clock(clock),
_timingId(timingId),
_master(false),
_tsExtrapolator(),
_codecTimer(),
_renderDelayMs(kDefaultRenderDelayMs),
_minTotalDelayMs(0),
_requiredDelayMs(0),
_currentDelayMs(0),
_prevFrameTimestamp(0)
{
    if (masterTiming == NULL)
    {
        _master = true;
        _tsExtrapolator = new VCMTimestampExtrapolator(_clock, vcmId, timingId);
    }
    else
    {
        _tsExtrapolator = masterTiming->_tsExtrapolator;
    }
}

VCMTiming::~VCMTiming()
{
    if (_master)
    {
        delete _tsExtrapolator;
    }
    delete _critSect;
}

void
VCMTiming::Reset(WebRtc_Word64 nowMs /* = -1 */)
{
    CriticalSectionScoped cs(_critSect);
    if (nowMs > -1)
    {
        _tsExtrapolator->Reset(nowMs);
    }
    else
    {
        _tsExtrapolator->Reset();
    }
    _codecTimer.Reset();
    _renderDelayMs = kDefaultRenderDelayMs;
    _minTotalDelayMs = 0;
    _requiredDelayMs = 0;
    _currentDelayMs = 0;
    _prevFrameTimestamp = 0;
}

void VCMTiming::ResetDecodeTime()
{
    _codecTimer.Reset();
}

void
VCMTiming::SetRenderDelay(WebRtc_UWord32 renderDelayMs)
{
    CriticalSectionScoped cs(_critSect);
    _renderDelayMs = renderDelayMs;
}

void
VCMTiming::SetMinimumTotalDelay(WebRtc_UWord32 minTotalDelayMs)
{
    CriticalSectionScoped cs(_critSect);
    _minTotalDelayMs = minTotalDelayMs;
}

void
VCMTiming::SetRequiredDelay(WebRtc_UWord32 requiredDelayMs)
{
    CriticalSectionScoped cs(_critSect);
    if (requiredDelayMs != _requiredDelayMs)
    {
        if (_master)
        {
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId, _timingId),
                    "Desired jitter buffer level: %u ms", requiredDelayMs);
        }
        _requiredDelayMs = requiredDelayMs;
    }
}

void VCMTiming::UpdateCurrentDelay(WebRtc_UWord32 frameTimestamp)
{
    CriticalSectionScoped cs(_critSect);
    WebRtc_UWord32 targetDelayMs = TargetDelayInternal();

    // Make sure we try to sync with audio
    if (targetDelayMs < _minTotalDelayMs)
    {
        targetDelayMs = _minTotalDelayMs;
    }

    if (_currentDelayMs == 0)
    {
        // Not initialized, set current delay to target.
        _currentDelayMs = targetDelayMs;
    }
    else if (targetDelayMs != _currentDelayMs)
    {
        WebRtc_Word64 delayDiffMs = static_cast<WebRtc_Word64>(targetDelayMs) -
                                    _currentDelayMs;
        // Never change the delay with more than 100 ms every second. If we're changing the
        // delay in too large steps we will get noticable freezes. By limiting the change we
        // can increase the delay in smaller steps, which will be experienced as the video is
        // played in slow motion. When lowering the delay the video will be played at a faster
        // pace.
        WebRtc_Word64 maxChangeMs = 0;
        if (frameTimestamp < 0x0000ffff && _prevFrameTimestamp > 0xffff0000)
        {
            // wrap
            maxChangeMs = kDelayMaxChangeMsPerS * (frameTimestamp +
                         (static_cast<WebRtc_Word64>(1)<<32) - _prevFrameTimestamp) / 90000;
        }
        else
        {
            maxChangeMs = kDelayMaxChangeMsPerS *
                          (frameTimestamp - _prevFrameTimestamp) / 90000;
        }
        if (maxChangeMs <= 0)
        {
            // Any changes less than 1 ms are truncated and
            // will be postponed. Negative change will be due
            // to reordering and should be ignored.
            return;
        }
        else if (delayDiffMs < -maxChangeMs)
        {
            delayDiffMs = -maxChangeMs;
        }
        else if (delayDiffMs > maxChangeMs)
        {
            delayDiffMs = maxChangeMs;
        }
        _currentDelayMs = _currentDelayMs + static_cast<WebRtc_Word32>(delayDiffMs);
    }
    _prevFrameTimestamp = frameTimestamp;
}

void VCMTiming::UpdateCurrentDelay(WebRtc_Word64 renderTimeMs,
                                   WebRtc_Word64 actualDecodeTimeMs)
{
    CriticalSectionScoped cs(_critSect);
    WebRtc_UWord32 targetDelayMs = TargetDelayInternal();
    // Make sure we try to sync with audio
    if (targetDelayMs < _minTotalDelayMs)
    {
        targetDelayMs = _minTotalDelayMs;
    }
    WebRtc_Word64 delayedMs = actualDecodeTimeMs -
                              (renderTimeMs - MaxDecodeTimeMs() - _renderDelayMs);
    if (delayedMs < 0)
    {
        return;
    }
    else if (_currentDelayMs + delayedMs <= targetDelayMs)
    {
        _currentDelayMs += static_cast<WebRtc_UWord32>(delayedMs);
    }
    else
    {
        _currentDelayMs = targetDelayMs;
    }
}

WebRtc_Word32
VCMTiming::StopDecodeTimer(WebRtc_UWord32 timeStamp,
                           WebRtc_Word64 startTimeMs,
                           WebRtc_Word64 nowMs)
{
    CriticalSectionScoped cs(_critSect);
    const WebRtc_Word32 maxDecTime = MaxDecodeTimeMs();
    WebRtc_Word32 timeDiffMs = _codecTimer.StopTimer(startTimeMs, nowMs);
    if (timeDiffMs < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCoding, VCMId(_vcmId, _timingId),
            "Codec timer error: %d", timeDiffMs);
        assert(false);
    }

    if (_master)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId, _timingId),
                "Frame decoded: timeStamp=%u decTime=%d maxDecTime=%u, at %u",
                timeStamp, timeDiffMs, maxDecTime, MaskWord64ToUWord32(nowMs));
    }
    return 0;
}

void
VCMTiming::IncomingTimestamp(WebRtc_UWord32 timeStamp, WebRtc_Word64 nowMs)
{
    CriticalSectionScoped cs(_critSect);
    _tsExtrapolator->Update(nowMs, timeStamp, _master);
}

WebRtc_Word64
VCMTiming::RenderTimeMs(WebRtc_UWord32 frameTimestamp, WebRtc_Word64 nowMs) const
{
    CriticalSectionScoped cs(_critSect);
    const WebRtc_Word64 renderTimeMs = RenderTimeMsInternal(frameTimestamp, nowMs);
    if (renderTimeMs < 0)
    {
        return renderTimeMs;
    }
    if (_master)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId, _timingId),
            "Render frame %u at %u. Render delay %u, required delay %u,"
                " max decode time %u, min total delay %u",
            frameTimestamp, MaskWord64ToUWord32(renderTimeMs), _renderDelayMs,
            _requiredDelayMs, MaxDecodeTimeMs(),_minTotalDelayMs);
    }
    return renderTimeMs;
}

WebRtc_Word64
VCMTiming::RenderTimeMsInternal(WebRtc_UWord32 frameTimestamp, WebRtc_Word64 nowMs) const
{
    WebRtc_Word64 estimatedCompleteTimeMs =
            _tsExtrapolator->ExtrapolateLocalTime(frameTimestamp);
    if (estimatedCompleteTimeMs - nowMs > kMaxVideoDelayMs)
    {
        if (_master)
        {
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId, _timingId),
                    "Timestamp arrived 2 seconds early, reset statistics",
                    frameTimestamp, estimatedCompleteTimeMs);
        }
        return -1;
    }
    if (_master)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId, _timingId),
                "ExtrapolateLocalTime(%u)=%u ms",
                frameTimestamp, MaskWord64ToUWord32(estimatedCompleteTimeMs));
    }
    if (estimatedCompleteTimeMs == -1)
    {
        estimatedCompleteTimeMs = nowMs;
    }

    return estimatedCompleteTimeMs + _currentDelayMs;
}

// Must be called from inside a critical section
WebRtc_Word32
VCMTiming::MaxDecodeTimeMs(FrameType frameType /*= kVideoFrameDelta*/) const
{
    const WebRtc_Word32 decodeTimeMs = _codecTimer.RequiredDecodeTimeMs(frameType);

    if (decodeTimeMs < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCoding, VCMId(_vcmId, _timingId),
            "Negative maximum decode time: %d", decodeTimeMs);
        return -1;
    }
    return decodeTimeMs;
}

WebRtc_UWord32
VCMTiming::MaxWaitingTime(WebRtc_Word64 renderTimeMs, WebRtc_Word64 nowMs) const
{
    CriticalSectionScoped cs(_critSect);

    const WebRtc_Word64 maxWaitTimeMs = renderTimeMs - nowMs -
                                        MaxDecodeTimeMs() - _renderDelayMs;

    if (maxWaitTimeMs < 0)
    {
        return 0;
    }
    return static_cast<WebRtc_UWord32>(maxWaitTimeMs);
}

bool
VCMTiming::EnoughTimeToDecode(WebRtc_UWord32 availableProcessingTimeMs) const
{
    CriticalSectionScoped cs(_critSect);
    WebRtc_Word32 maxDecodeTimeMs = MaxDecodeTimeMs();
    if (maxDecodeTimeMs < 0)
    {
        // Haven't decoded any frames yet, try decoding one to get an estimate
        // of the decode time.
        return true;
    }
    else if (maxDecodeTimeMs == 0)
    {
        // Decode time is less than 1, set to 1 for now since
        // we don't have any better precision. Count ticks later?
        maxDecodeTimeMs = 1;
    }
    return static_cast<WebRtc_Word32>(availableProcessingTimeMs) - maxDecodeTimeMs > 0;
}

WebRtc_UWord32
VCMTiming::TargetVideoDelay() const
{
    CriticalSectionScoped cs(_critSect);
    return TargetDelayInternal();
}

WebRtc_UWord32
VCMTiming::TargetDelayInternal() const
{
    return _requiredDelayMs + MaxDecodeTimeMs() + _renderDelayMs;
}

}
