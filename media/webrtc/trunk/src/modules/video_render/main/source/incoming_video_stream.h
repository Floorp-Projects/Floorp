/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_INCOMING_VIDEO_STREAM_H_
#define WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_INCOMING_VIDEO_STREAM_H_

#include "video_render.h"
#include "map_wrapper.h"

namespace webrtc {
class CriticalSectionWrapper;
class EventWrapper;
class ThreadWrapper;
class VideoRenderCallback;
class VideoRenderFrames;

struct VideoMirroring
{
    bool mirrorXAxis;
    bool mirrorYAxis;
    VideoMirroring() :
        mirrorXAxis(false), mirrorYAxis(false)
    {
    }
};

// Class definitions
class IncomingVideoStream: public VideoRenderCallback
{
public:
    /*
     *   VideoRenderer constructor/destructor
     */
    IncomingVideoStream(const WebRtc_Word32 moduleId,
                        const WebRtc_UWord32 streamId);
    ~IncomingVideoStream();

    WebRtc_Word32 ChangeModuleId(const WebRtc_Word32 id);

    // Get callbck to deliver frames to the module
    VideoRenderCallback* ModuleCallback();
    virtual WebRtc_Word32 RenderFrame(const WebRtc_UWord32 streamId,
                                      VideoFrame& videoFrame);

    // Set callback to the platform dependant code
    WebRtc_Word32 SetRenderCallback(VideoRenderCallback* renderCallback);

    // Callback for file recording, snapshot, ...
    WebRtc_Word32 SetExternalCallback(VideoRenderCallback* renderObject);

    /*
     *   Start/Stop
     */
    WebRtc_Word32 Start();
    WebRtc_Word32 Stop();

    // Clear all buffers
    WebRtc_Word32 Reset();

    /*
     *   Properties
     */
    WebRtc_UWord32 StreamId() const;
    WebRtc_UWord32 IncomingRate() const;

    /*
     *
     */
    WebRtc_Word32 GetLastRenderedFrame(VideoFrame& videoFrame) const;

    WebRtc_Word32 SetStartImage(const VideoFrame& videoFrame);

    WebRtc_Word32 SetTimeoutImage(const VideoFrame& videoFrame,
                                  const WebRtc_UWord32 timeout);

    WebRtc_Word32 EnableMirroring(const bool enable,
                                  const bool mirrorXAxis,
                                  const bool mirrorYAxis);

protected:
    static bool IncomingVideoStreamThreadFun(void* obj);
    bool IncomingVideoStreamProcess();

private:

    // Enums
    enum
    {
        KEventStartupTimeMS = 10
    };
    enum
    {
        KEventMaxWaitTimeMs = 100
    };
    enum
    {
        KFrameRatePeriodMs = 1000
    };

    WebRtc_Word32 _moduleId;
    WebRtc_UWord32 _streamId;
    CriticalSectionWrapper& _streamCritsect; // Critsects in allowed to enter order
    CriticalSectionWrapper& _threadCritsect;
    CriticalSectionWrapper& _bufferCritsect;
    ThreadWrapper* _ptrIncomingRenderThread;
    EventWrapper& _deliverBufferEvent;
    bool _running;

    VideoRenderCallback* _ptrExternalCallback;
    VideoRenderCallback* _ptrRenderCallback;
    VideoRenderFrames& _renderBuffers;

    RawVideoType _callbackVideoType;
    WebRtc_UWord32 _callbackWidth;
    WebRtc_UWord32 _callbackHeight;

    WebRtc_UWord32 _incomingRate;
    WebRtc_Word64 _lastRateCalculationTimeMs;
    WebRtc_UWord16 _numFramesSinceLastCalculation;
    VideoFrame _lastRenderedFrame;
    VideoFrame _tempFrame;
    VideoFrame _startImage;
    VideoFrame _timeoutImage;
    WebRtc_UWord32 _timeoutTime;

    bool _mirrorFramesEnabled;
    VideoMirroring _mirroring;
    VideoFrame _transformedVideoFrame;
};

} //namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_RENDER_MAIN_SOURCE_INCOMING_VIDEO_STREAM_H_
