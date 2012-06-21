/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_VIDEO_CAPTURE_LINUX_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_VIDEO_CAPTURE_LINUX_H_

#include "common_types.h"
#include "../video_capture_impl.h"

namespace webrtc
{
class CriticalSectionWrapper;
class ThreadWrapper;
namespace videocapturemodule
{
class VideoCaptureModuleV4L2: public VideoCaptureImpl
{
public:
    VideoCaptureModuleV4L2(WebRtc_Word32 id);
    virtual ~VideoCaptureModuleV4L2();
    virtual WebRtc_Word32 Init(const char* deviceUniqueId);
    virtual WebRtc_Word32 StartCapture(const VideoCaptureCapability& capability);
    virtual WebRtc_Word32 StopCapture();
    virtual bool CaptureStarted();
    virtual WebRtc_Word32 CaptureSettings(VideoCaptureCapability& settings);

private:
    enum {kNoOfV4L2Bufffers=4};

    static bool CaptureThread(void*);
    bool CaptureProcess();
    bool AllocateVideoBuffers();
    bool DeAllocateVideoBuffers();

    ThreadWrapper* _captureThread;
    CriticalSectionWrapper* _captureCritSect;

    WebRtc_Word32 _deviceId;
    WebRtc_Word32 _deviceFd;

    WebRtc_Word32 _buffersAllocatedByDevice;
    WebRtc_Word32 _currentWidth;
    WebRtc_Word32 _currentHeight;
    WebRtc_Word32 _currentFrameRate;
    bool _captureStarted;
    RawVideoType _captureVideoType;
    struct Buffer
    {
        void *start;
        size_t length;
    };
    Buffer *_pool;
};
} // namespace videocapturemodule
} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_VIDEO_CAPTURE_LINUX_H_
