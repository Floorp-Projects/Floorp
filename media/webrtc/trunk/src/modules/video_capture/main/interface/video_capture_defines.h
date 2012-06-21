/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_INTERFACE_VIDEO_CAPTURE_DEFINES_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_INTERFACE_VIDEO_CAPTURE_DEFINES_H_

// Includes
#include "typedefs.h"
#include "modules/interface/module_common_types.h"

namespace webrtc
{
// Defines
#ifndef NULL
    #define NULL    0
#endif

enum {kVideoCaptureUniqueNameLength =1024}; //Max unique capture device name lenght
enum {kVideoCaptureDeviceNameLength =256}; //Max capture device name lenght
enum {kVideoCaptureProductIdLength =128}; //Max product id length

// Enums
enum VideoCaptureRotation
{
    kCameraRotate0 = 0,
    kCameraRotate90 = 5,
    kCameraRotate180 = 10,
    kCameraRotate270 = 15
};

struct VideoCaptureCapability
{
    WebRtc_Word32 width;
    WebRtc_Word32 height;
    WebRtc_Word32 maxFPS;
    WebRtc_Word32 expectedCaptureDelay;
    RawVideoType rawType;
    VideoCodecType codecType;
    bool interlaced;

    VideoCaptureCapability()
    {
        width = 0;
        height = 0;
        maxFPS = 0;
        expectedCaptureDelay = 0;
        rawType = kVideoUnknown;
        codecType = kVideoCodecUnknown;
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
        if (rawType != other.rawType)
            return true;
        if (codecType != other.codecType)
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

enum VideoCaptureAlarm
{
    Raised = 0,
    Cleared = 1
};

// VideoFrameI420 doesn't take the ownership of the buffer.
// It's mostly used to group the parameters for external capture.
struct VideoFrameI420
{
  VideoFrameI420() {
    y_plane = NULL;
    u_plane = NULL;
    v_plane = NULL;
    y_pitch = 0;
    u_pitch = 0;
    v_pitch = 0;
    width = 0;
    height = 0;
  }

  unsigned char* y_plane;
  unsigned char* u_plane;
  unsigned char* v_plane;

  int y_pitch;
  int u_pitch;
  int v_pitch;

  unsigned short width;
  unsigned short height;
};

/* External Capture interface. Returned by Create
 and implemented by the capture module.
 */
class VideoCaptureExternal
{
public:
    virtual WebRtc_Word32 IncomingFrame(WebRtc_UWord8* videoFrame,
                                        WebRtc_Word32 videoFrameLength,
                                        const VideoCaptureCapability& frameInfo,
                                        WebRtc_Word64 captureTime = 0) = 0;
    virtual WebRtc_Word32 IncomingFrameI420(const VideoFrameI420& video_frame,
                                            WebRtc_Word64 captureTime = 0) = 0;
protected:
    ~VideoCaptureExternal() {}
};

// Callback class to be implemented by module user
class VideoCaptureDataCallback
{
public:
    virtual void OnIncomingCapturedFrame(const WebRtc_Word32 id,
                                         VideoFrame& videoFrame,
                                         VideoCodecType codecType) = 0;
    virtual void OnCaptureDelayChanged(const WebRtc_Word32 id,
                                       const WebRtc_Word32 delay) = 0;
protected:
    virtual ~VideoCaptureDataCallback(){}
};

class VideoCaptureFeedBack
{
public:
    virtual void OnCaptureFrameRate(const WebRtc_Word32 id,
                                    const WebRtc_UWord32 frameRate) = 0;
    virtual void OnNoPictureAlarm(const WebRtc_Word32 id,
                                  const VideoCaptureAlarm alarm) = 0;
protected:
    virtual ~VideoCaptureFeedBack(){}
};

} //namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_INTERFACE_VIDEO_CAPTURE_DEFINES_H_
