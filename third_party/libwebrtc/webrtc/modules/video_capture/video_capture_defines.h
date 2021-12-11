/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEFINES_H_
#define MODULES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEFINES_H_

#include "api/video/video_frame.h"
#include "modules/include/module_common_types.h"
#include "typedefs.h"  // NOLINT(build/include)

#ifdef XP_WIN
typedef int pid_t;
#endif

namespace webrtc
{
// Defines
#ifndef NULL
    #define NULL    0
#endif

enum {kVideoCaptureUniqueNameSize =1024}; //Max unique capture device name length
enum {kVideoCaptureProductIdSize =128}; //Max product id length

struct VideoCaptureCapability
{
    int32_t width;
    int32_t height;
    int32_t maxFPS;
    VideoType videoType;
    bool interlaced;

    VideoCaptureCapability()
    {
        width = 0;
        height = 0;
        maxFPS = 0;
        videoType = VideoType::kUnknown;
        interlaced = false;
    }
    ;
    bool operator!=(const VideoCaptureCapability &other) const
    {
        if (width != other.width)
            return true;
        if (height != other.height)
            return true;
        if (maxFPS != other.maxFPS)
            return true;
        if (videoType != other.videoType)
          return true;
        if (interlaced != other.interlaced)
            return true;
        return false;
    }
    bool operator==(const VideoCaptureCapability &other) const
    {
        return !operator!=(other);
    }
};

/* External Capture interface. Returned by Create
 and implemented by the capture module.
 */
class VideoCaptureExternal
{
public:
    // |capture_time| must be specified in the NTP time format in milliseconds.
    virtual int32_t IncomingFrame(uint8_t* videoFrame,
                                  size_t videoFrameLength,
                                  const VideoCaptureCapability& frameInfo,
                                  int64_t captureTime = 0) = 0;
protected:
    ~VideoCaptureExternal() {}
};


// Callback class to be implemented by module user
class VideoCaptureDataCallback
{
public:
    virtual void OnIncomingCapturedFrame(const int32_t id,
                                         const VideoFrame& videoFrame) = 0;
    virtual void OnCaptureDelayChanged(const int32_t id,
                                       const int32_t delay) = 0;
protected:
    virtual ~VideoCaptureDataCallback(){}
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEFINES_H_
