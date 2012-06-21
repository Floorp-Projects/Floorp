/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "incoming_video_stream.h"

#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "trace.h"
#include "thread_wrapper.h"
#include "video_render_frames.h"
#include "tick_util.h"
#include "map_wrapper.h"
#include "common_video/libyuv/include/libyuv.h"

#include <cassert>

// Platform specifics
#if defined(_WIN32)
#include <windows.h>
#elif defined(WEBRTC_LINUX)
#include <ctime>
#include <sys/time.h>
#else
#include <sys/time.h>
#endif

namespace webrtc {
IncomingVideoStream::IncomingVideoStream(const WebRtc_Word32 moduleId,
                                         const WebRtc_UWord32 streamId) :
    _moduleId(moduleId),
    _streamId(streamId),
    _streamCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
    _threadCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
    _bufferCritsect(*CriticalSectionWrapper::CreateCriticalSection()),
    _ptrIncomingRenderThread(),
    _deliverBufferEvent(*EventWrapper::Create()),
    _running(false),
    _ptrExternalCallback(NULL),
    _ptrRenderCallback(NULL),
    _renderBuffers(*(new VideoRenderFrames)),
    _callbackVideoType(kVideoI420),
    _callbackWidth(0),
    _callbackHeight(0),
    _incomingRate(0),
    _lastRateCalculationTimeMs(0),
    _numFramesSinceLastCalculation(0),
    _lastRenderedFrame(),
    _tempFrame(),
    _startImage(),
    _timeoutImage(),
    _timeoutTime(),
    _mirrorFramesEnabled(false),
    _mirroring(),
    _transformedVideoFrame()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVideoRenderer, _moduleId,
                 "%s created for stream %d", __FUNCTION__, streamId);
}

IncomingVideoStream::~IncomingVideoStream()
{
    WEBRTC_TRACE(kTraceMemory, kTraceVideoRenderer, _moduleId,
                 "%s deleted for stream %d", __FUNCTION__, _streamId);

    Stop();

    // _ptrIncomingRenderThread - Delete in stop
    delete &_renderBuffers;
    delete &_streamCritsect;
    delete &_bufferCritsect;
    delete &_threadCritsect;
    delete &_deliverBufferEvent;

}

WebRtc_Word32 IncomingVideoStream::ChangeModuleId(const WebRtc_Word32 id)
{
    CriticalSectionScoped cs(&_streamCritsect);

    _moduleId = id;
    return 0;
}

VideoRenderCallback*
IncomingVideoStream::ModuleCallback()
{
    CriticalSectionScoped cs(&_streamCritsect);
    return this;
}

WebRtc_Word32 IncomingVideoStream::RenderFrame(const WebRtc_UWord32 streamId,
                                               VideoFrame& videoFrame)
{

    CriticalSectionScoped csS(&_streamCritsect);
    WEBRTC_TRACE(kTraceStream, kTraceVideoRenderer, _moduleId,
                 "%s for stream %d, render time: %u", __FUNCTION__, _streamId,
                 videoFrame.RenderTimeMs());

    if (!_running)
    {
        WEBRTC_TRACE(kTraceStream, kTraceVideoRenderer, _moduleId,
                     "%s: Not running", __FUNCTION__);
        return -1;
    }

    if (true == _mirrorFramesEnabled)
    {
        _transformedVideoFrame.VerifyAndAllocate(videoFrame.Length());
        if (_mirroring.mirrorXAxis)
        {
            MirrorI420UpDown(videoFrame.Buffer(),
                                     _transformedVideoFrame.Buffer(),
                                     videoFrame.Width(), videoFrame.Height());
            _transformedVideoFrame.SetLength(videoFrame.Length());
            _transformedVideoFrame.SetWidth(videoFrame.Width());
            _transformedVideoFrame.SetHeight(videoFrame.Height());
            videoFrame.SwapFrame(_transformedVideoFrame);
        }
        if (_mirroring.mirrorYAxis)
        {
            MirrorI420LeftRight(videoFrame.Buffer(),
                                        _transformedVideoFrame.Buffer(),
                                        videoFrame.Width(), videoFrame.Height());
            _transformedVideoFrame.SetLength(videoFrame.Length());
            _transformedVideoFrame.SetWidth(videoFrame.Width());
            _transformedVideoFrame.SetHeight(videoFrame.Height());
            videoFrame.SwapFrame(_transformedVideoFrame);
        }
    }

    // Rate statistics
    _numFramesSinceLastCalculation++;
    WebRtc_Word64 nowMs = TickTime::MillisecondTimestamp();
    if (nowMs >= _lastRateCalculationTimeMs + KFrameRatePeriodMs)
    {
        _incomingRate = (WebRtc_UWord32) (1000 * _numFramesSinceLastCalculation
                / (nowMs - _lastRateCalculationTimeMs));
        _numFramesSinceLastCalculation = 0;
        _lastRateCalculationTimeMs = nowMs;
    }

    // Insert frame
    CriticalSectionScoped csB(&_bufferCritsect);
    if (_renderBuffers.AddFrame(&videoFrame) == 1)
        _deliverBufferEvent.Set();

    return 0;
}

WebRtc_Word32 IncomingVideoStream::SetStartImage(const VideoFrame& videoFrame)
{
    CriticalSectionScoped csS(&_threadCritsect);
    return _startImage.CopyFrame(videoFrame);
}

WebRtc_Word32 IncomingVideoStream::SetTimeoutImage(const VideoFrame& videoFrame,
                                                   const WebRtc_UWord32 timeout)
{
    CriticalSectionScoped csS(&_threadCritsect);
    _timeoutTime = timeout;
    return _timeoutImage.CopyFrame(videoFrame);
}

WebRtc_Word32 IncomingVideoStream::SetRenderCallback(VideoRenderCallback* renderCallback)
{
    CriticalSectionScoped cs(&_streamCritsect);

    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _moduleId,
                 "%s(%x) for stream %d", __FUNCTION__, renderCallback,
                 _streamId);
    _ptrRenderCallback = renderCallback;
    return 0;
}

WebRtc_Word32 IncomingVideoStream::EnableMirroring(const bool enable,
                                                   const bool mirrorXAxis,
                                                   const bool mirrorYAxis)
{
    CriticalSectionScoped cs(&_streamCritsect);
    _mirrorFramesEnabled = enable;
    _mirroring.mirrorXAxis = mirrorXAxis;
    _mirroring.mirrorYAxis = mirrorYAxis;

    return 0;
}

WebRtc_Word32 IncomingVideoStream::SetExternalCallback(VideoRenderCallback* externalCallback)
{
    CriticalSectionScoped cs(&_streamCritsect);

    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _moduleId,
                 "%s(%x) for stream %d", __FUNCTION__, externalCallback,
                 _streamId);
    _ptrExternalCallback = externalCallback;
    _callbackVideoType = kVideoI420;
    _callbackWidth = 0;
    _callbackHeight = 0;
    return 0;
}

WebRtc_Word32 IncomingVideoStream::Start()
{
    CriticalSectionScoped csS(&_streamCritsect);
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _moduleId,
                 "%s for stream %d", __FUNCTION__, _streamId);
    if (_running)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _moduleId,
                     "%s: Already running", __FUNCTION__);
        return 0;
    }

    CriticalSectionScoped csT(&_threadCritsect);
    assert(_ptrIncomingRenderThread == NULL);

    _ptrIncomingRenderThread
            = ThreadWrapper::CreateThread(IncomingVideoStreamThreadFun, this,
                                          kRealtimePriority,
                                          "IncomingVideoStreamThread");
    if (!_ptrIncomingRenderThread)
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _moduleId,
                     "%s: No thread", __FUNCTION__);
        return -1;
    }

    unsigned int tId = 0;
    if (_ptrIncomingRenderThread->Start(tId))
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _moduleId,
                     "%s: thread started: %u", __FUNCTION__, tId);
    }
    else
    {
        WEBRTC_TRACE(kTraceError, kTraceVideoRenderer, _moduleId,
                     "%s: Could not start send thread", __FUNCTION__);
        return -1;
    }
    _deliverBufferEvent.StartTimer(false, KEventStartupTimeMS);

    _running = true;
    return 0;
}

WebRtc_Word32 IncomingVideoStream::Stop()
{
    CriticalSectionScoped csStream(&_streamCritsect);
    WEBRTC_TRACE(kTraceInfo, kTraceVideoRenderer, _moduleId,
                 "%s for stream %d", __FUNCTION__, _streamId);

    if (!_running)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _moduleId,
                     "%s: Not running", __FUNCTION__);
        return 0;
    }

    _threadCritsect.Enter();
    if (_ptrIncomingRenderThread)
    {
        ThreadWrapper* ptrThread = _ptrIncomingRenderThread;
        _ptrIncomingRenderThread = NULL;
        ptrThread->SetNotAlive();
#ifndef _WIN32
        _deliverBufferEvent.StopTimer();
#endif
        _threadCritsect.Leave();
        if (ptrThread->Stop())
        {
            delete ptrThread;
        }
        else
        {
            assert(false);
            WEBRTC_TRACE(kTraceWarning, kTraceVideoRenderer, _moduleId,
                         "%s: Not able to stop thread, leaking", __FUNCTION__);
        }
    }
    else
    {
        _threadCritsect.Leave();
    }
    _running = false;
    return 0;
}

WebRtc_Word32 IncomingVideoStream::Reset()
{
    CriticalSectionScoped csStream(&_streamCritsect);
    CriticalSectionScoped csBuffer(&_bufferCritsect);

    _renderBuffers.ReleaseAllFrames();
    return 0;
}

WebRtc_UWord32 IncomingVideoStream::StreamId() const
{
    CriticalSectionScoped csStream(&_streamCritsect);
    return _streamId;
}

WebRtc_UWord32 IncomingVideoStream::IncomingRate() const
{
    CriticalSectionScoped cs(&_streamCritsect);
    return _incomingRate;
}

bool IncomingVideoStream::IncomingVideoStreamThreadFun(void* obj)
{
    return static_cast<IncomingVideoStream*> (obj)->IncomingVideoStreamProcess();
}

bool IncomingVideoStream::IncomingVideoStreamProcess()
{
    if (kEventError != _deliverBufferEvent.Wait(KEventMaxWaitTimeMs))
    {
        if (_ptrIncomingRenderThread == NULL)
        {
            // Terminating
            return false;
        }

        _threadCritsect.Enter();

        VideoFrame* ptrFrameToRender = NULL;

        // Get a new frame to render and the time for the frame after this one.
        _bufferCritsect.Enter();
        ptrFrameToRender = _renderBuffers.FrameToRender();
        WebRtc_UWord32 waitTime = _renderBuffers.TimeToNextFrameRelease();
        _bufferCritsect.Leave();

        // Set timer for next frame to render
        if (waitTime > KEventMaxWaitTimeMs)
        {
            waitTime = KEventMaxWaitTimeMs;
        }
        _deliverBufferEvent.StartTimer(false, waitTime);

        if (!ptrFrameToRender)
        {
            if (_ptrRenderCallback)
            {
                if (_lastRenderedFrame.RenderTimeMs() == 0
                        && _startImage.Size()) // And we have not rendered anything and have a start image
                {
                    _tempFrame.CopyFrame(_startImage);// Copy the startimage if the renderer modifies the render buffer.
                    _ptrRenderCallback->RenderFrame(_streamId, _tempFrame);
                }
                else if (_timeoutImage.Size()
                        && _lastRenderedFrame.RenderTimeMs() + _timeoutTime
                                < TickTime::MillisecondTimestamp()) // We have rendered something a long time ago and have a timeout image
                {
                    _tempFrame.CopyFrame(_timeoutImage); // Copy the timeoutImage if the renderer modifies the render buffer.
                    _ptrRenderCallback->RenderFrame(_streamId, _tempFrame);
                }
            }

            // No frame
            _threadCritsect.Leave();
            return true;
        }

        // Send frame for rendering
        if (_ptrExternalCallback)
        {
            WEBRTC_TRACE(kTraceStream,
                         kTraceVideoRenderer,
                         _moduleId,
                         "%s: executing external renderer callback to deliver frame",
                         __FUNCTION__, ptrFrameToRender->RenderTimeMs());
            _ptrExternalCallback->RenderFrame(_streamId, *ptrFrameToRender);
        }
        else
        {
            if (_ptrRenderCallback)
            {
                WEBRTC_TRACE(kTraceStream, kTraceVideoRenderer, _moduleId,
                             "%s: Render frame, time: ", __FUNCTION__,
                             ptrFrameToRender->RenderTimeMs());
                _ptrRenderCallback->RenderFrame(_streamId, *ptrFrameToRender);
            }
        }

        // Release critsect before calling the module user
        _threadCritsect.Leave();

        // We're done with this frame, delete it.
        if (ptrFrameToRender)
        {
            CriticalSectionScoped cs(&_bufferCritsect);
            _lastRenderedFrame.SwapFrame(*ptrFrameToRender);
            _renderBuffers.ReturnFrame(ptrFrameToRender);
        }
    }
    return true;
}
WebRtc_Word32 IncomingVideoStream::GetLastRenderedFrame(VideoFrame& videoFrame) const
{
    CriticalSectionScoped cs(&_bufferCritsect);
    return videoFrame.CopyFrame(_lastRenderedFrame);
}

} //namespace webrtc

