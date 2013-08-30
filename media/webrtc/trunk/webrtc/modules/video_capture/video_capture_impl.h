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
    static VideoCaptureModule* Create(const int32_t id,
                                      const char* deviceUniqueIdUTF8);

    /*
     *   Create a video capture module object used for external capture.
     *
     *   id              - unique identifier of this video capture module object
     *   externalCapture - [out] interface to call when a new frame is captured.
     */
    static VideoCaptureModule* Create(const int32_t id,
                                      VideoCaptureExternal*& externalCapture);

    static DeviceInfo* CreateDeviceInfo(const int32_t id);

    // Implements Module declared functions.
    virtual int32_t ChangeUniqueId(const int32_t id);

    //Call backs
    virtual int32_t RegisterCaptureDataCallback(VideoCaptureDataCallback& dataCallback);
    virtual int32_t DeRegisterCaptureDataCallback();
    virtual int32_t RegisterCaptureCallback(VideoCaptureFeedBack& callBack);
    virtual int32_t DeRegisterCaptureCallback();

    virtual int32_t SetCaptureDelay(int32_t delayMS);
    virtual int32_t CaptureDelay();
    virtual int32_t SetCaptureRotation(VideoCaptureRotation rotation);

    virtual int32_t EnableFrameRateCallback(const bool enable);
    virtual int32_t EnableNoPictureAlarm(const bool enable);

    virtual const char* CurrentDeviceName() const;

    // Module handling
    virtual int32_t TimeUntilNextProcess();
    virtual int32_t Process();

    // Implement VideoCaptureExternal
    // |capture_time| must be specified in the NTP time format in milliseconds.
    virtual int32_t IncomingFrame(uint8_t* videoFrame,
                                  int32_t videoFrameLength,
                                  const VideoCaptureCapability& frameInfo,
                                  int64_t captureTime = 0);
    virtual int32_t IncomingFrameI420(
        const VideoFrameI420& video_frame,
        int64_t captureTime = 0);

    // Platform dependent
    virtual int32_t StartCapture(const VideoCaptureCapability& capability)
    {
        _requestedCapability = capability;
        return -1;
    }
    virtual int32_t StopCapture()   { return -1; }
    virtual bool CaptureStarted() {return false; }
    virtual int32_t CaptureSettings(VideoCaptureCapability& /*settings*/)
    { return -1; }
    VideoCaptureEncodeInterface* GetEncodeInterface(const VideoCodec& /*codec*/)
    { return NULL; }

protected:
    VideoCaptureImpl(const int32_t id);
    virtual ~VideoCaptureImpl();
    int32_t DeliverCapturedFrame(I420VideoFrame& captureFrame,
                                 int64_t capture_time);

    int32_t _id; // Module ID
    char* _deviceUniqueId; // current Device unique name;
    CriticalSectionWrapper& _apiCs;
    int32_t _captureDelay; // Current capture delay. May be changed of platform dependent parts.
    VideoCaptureCapability _requestedCapability; // Should be set by platform dependent code in StartCapture.
private:
    void UpdateFrameCount();
    uint32_t CalculateFrameRate(const TickTime& now);

    CriticalSectionWrapper& _callBackCs;

    TickTime _lastProcessTime; // last time the module process function was called.
    TickTime _lastFrameRateCallbackTime; // last time the frame rate callback function was called.
    bool _frameRateCallBack; // true if EnableFrameRateCallback
    bool _noPictureAlarmCallBack; //true if EnableNoPictureAlarm
    VideoCaptureAlarm _captureAlarm; // current value of the noPictureAlarm

    int32_t _setCaptureDelay; // The currently used capture delay
    VideoCaptureDataCallback* _dataCallBack;
    VideoCaptureFeedBack* _captureCallBack;

    TickTime _lastProcessFrameCount;
    TickTime _incomingFrameTimes[kFrameRateCountHistorySize];// timestamp for local captured frames
    VideoRotationMode _rotateFrame; //Set if the frame should be rotated by the capture module.

    I420VideoFrame _captureFrame;
    VideoFrame _capture_encoded_frame;

    // Used to make sure incoming timestamp is increasing for every frame.
    int64_t last_capture_time_;
};
} // namespace videocapturemodule
} //namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_VIDEO_CAPTURE_IMPL_H_
