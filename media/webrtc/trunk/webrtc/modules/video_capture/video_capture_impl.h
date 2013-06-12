/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_IMPL_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_IMPL_H_

/*
 * video_capture_impl.h
 */

#include "video_capture.h"
#include "video_capture_config.h"
#include "tick_util.h"
#include "common_video/interface/i420_video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc
{
class CriticalSectionWrapper;

namespace videocapturemodule {
// Class definitions
class VideoCaptureImpl: public VideoCaptureModule, public VideoCaptureExternal
{
public:

    /*
     *   Create a video capture module object
     *
     *   id              - unique identifier of this video capture module object
     *   deviceUniqueIdUTF8 -  name of the device. Available names can be found by using GetDeviceName
     */
    static VideoCaptureModule* Create(const WebRtc_Word32 id,
                                      const char* deviceUniqueIdUTF8);

    /*
     *   Create a video capture module object used for external capture.
     *
     *   id              - unique identifier of this video capture module object
     *   externalCapture - [out] interface to call when a new frame is captured.
     */
    static VideoCaptureModule* Create(const WebRtc_Word32 id,
                                      VideoCaptureExternal*& externalCapture);

    static DeviceInfo* CreateDeviceInfo(const WebRtc_Word32 id);

    // Implements Module declared functions.
    virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

    //Call backs
    virtual WebRtc_Word32 RegisterCaptureDataCallback(VideoCaptureDataCallback& dataCallback);
    virtual WebRtc_Word32 DeRegisterCaptureDataCallback();
    virtual WebRtc_Word32 RegisterCaptureCallback(VideoCaptureFeedBack& callBack);
    virtual WebRtc_Word32 DeRegisterCaptureCallback();

    virtual WebRtc_Word32 SetCaptureDelay(WebRtc_Word32 delayMS);
    virtual WebRtc_Word32 CaptureDelay();
    virtual WebRtc_Word32 SetCaptureRotation(VideoCaptureRotation rotation);

    virtual WebRtc_Word32 EnableFrameRateCallback(const bool enable);
    virtual WebRtc_Word32 EnableNoPictureAlarm(const bool enable);

    virtual const char* CurrentDeviceName() const;

    // Module handling
    virtual WebRtc_Word32 TimeUntilNextProcess();
    virtual WebRtc_Word32 Process();

    // Implement VideoCaptureExternal
    virtual WebRtc_Word32 IncomingFrame(WebRtc_UWord8* videoFrame,
                                        WebRtc_Word32 videoFrameLength,
                                        const VideoCaptureCapability& frameInfo,
                                        WebRtc_Word64 captureTime = 0);
    virtual WebRtc_Word32 IncomingFrameI420(
        const VideoFrameI420& video_frame,
        WebRtc_Word64 captureTime = 0);

    // Platform dependent
    virtual WebRtc_Word32 StartCapture(const VideoCaptureCapability& capability)
    {
        _requestedCapability = capability;
        return -1;
    }
    virtual WebRtc_Word32 StopCapture()   { return -1; }
    virtual bool CaptureStarted() {return false; }
    virtual WebRtc_Word32 CaptureSettings(VideoCaptureCapability& /*settings*/)
    { return -1; }
    VideoCaptureEncodeInterface* GetEncodeInterface(const VideoCodec& /*codec*/)
    { return NULL; }

protected:
    VideoCaptureImpl(const WebRtc_Word32 id);
    virtual ~VideoCaptureImpl();
    WebRtc_Word32 DeliverCapturedFrame(I420VideoFrame& captureFrame,
                                       WebRtc_Word64 capture_time);
    WebRtc_Word32 DeliverEncodedCapturedFrame(VideoFrame& captureFrame,
                                              WebRtc_Word64 capture_time,
                                              VideoCodecType codec_type);

    WebRtc_Word32 _id; // Module ID
    char* _deviceUniqueId; // current Device unique name;
    CriticalSectionWrapper& _apiCs;
    WebRtc_Word32 _captureDelay; // Current capture delay. May be changed of platform dependent parts.
    VideoCaptureCapability _requestedCapability; // Should be set by platform dependent code in StartCapture.
private:
    void UpdateFrameCount();
    WebRtc_UWord32 CalculateFrameRate(const TickTime& now);

    CriticalSectionWrapper& _callBackCs;

    TickTime _lastProcessTime; // last time the module process function was called.
    TickTime _lastFrameRateCallbackTime; // last time the frame rate callback function was called.
    bool _frameRateCallBack; // true if EnableFrameRateCallback
    bool _noPictureAlarmCallBack; //true if EnableNoPictureAlarm
    VideoCaptureAlarm _captureAlarm; // current value of the noPictureAlarm

    WebRtc_Word32 _setCaptureDelay; // The currently used capture delay
    VideoCaptureDataCallback* _dataCallBack;
    VideoCaptureFeedBack* _captureCallBack;

    TickTime _lastProcessFrameCount;
    TickTime _incomingFrameTimes[kFrameRateCountHistorySize];// timestamp for local captured frames
    VideoRotationMode _rotateFrame; //Set if the frame should be rotated by the capture module.

    I420VideoFrame _captureFrame;
    VideoFrame _capture_encoded_frame;

    // Used to make sure incoming timestamp is increasing for every frame.
    WebRtc_Word64 last_capture_time_;
};
} // namespace videocapturemodule
} //namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_IMPL_H_
