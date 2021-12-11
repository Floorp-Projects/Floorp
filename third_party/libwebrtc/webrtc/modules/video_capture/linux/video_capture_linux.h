/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_VIDEO_CAPTURE_LINUX_H_
#define MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_VIDEO_CAPTURE_LINUX_H_

#include <memory>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_capture/video_capture_impl.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/platform_thread.h"

namespace webrtc
{
namespace videocapturemodule
{
class VideoCaptureModuleV4L2: public VideoCaptureImpl
{
public:
    VideoCaptureModuleV4L2();
    virtual ~VideoCaptureModuleV4L2();
    virtual int32_t Init(const char* deviceUniqueId);
    virtual int32_t StartCapture(const VideoCaptureCapability& capability);
    virtual int32_t StopCapture();
    virtual bool CaptureStarted();
    virtual int32_t CaptureSettings(VideoCaptureCapability& settings);

private:
    enum {kNoOfV4L2Bufffers=4};

    static bool CaptureThread(void*);
    bool CaptureProcess();
    bool AllocateVideoBuffers();
    bool DeAllocateVideoBuffers();

    // TODO(pbos): Stop using unique_ptr and resetting the thread.
    std::unique_ptr<rtc::PlatformThread> _captureThread;
    rtc::CriticalSection _captureCritSect;

    int32_t _deviceId;
    int32_t _deviceFd;

    int32_t _buffersAllocatedByDevice;
    int32_t _currentWidth;
    int32_t _currentHeight;
    int32_t _currentFrameRate;
    bool _captureStarted;
    VideoType _captureVideoType;
    struct Buffer
    {
        void *start;
        size_t length;
    };
    Buffer *_pool;
};
}  // namespace videocapturemodule
}  // namespace webrtc

#endif // MODULES_VIDEO_CAPTURE_MAIN_SOURCE_LINUX_VIDEO_CAPTURE_LINUX_H_
