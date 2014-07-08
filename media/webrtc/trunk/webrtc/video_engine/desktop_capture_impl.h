/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_MAIN_SOURCE_DESKTOP_CAPTURE_IMPL_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_MAIN_SOURCE_DESKTOP_CAPTURE_IMPL_H_

/*
 * video_capture_impl.h
 */

#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/desktop_capture/screen_capturer.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/modules/desktop_capture/shared_memory.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/modules/desktop_capture/mouse_cursor_shape.h"
#include "webrtc/modules/desktop_capture/desktop_device_info.h"
#include "webrtc/modules/desktop_capture/desktop_and_cursor_composer.h"

using namespace webrtc::videocapturemodule;

namespace webrtc {

class CriticalSectionWrapper;
class VideoCaptureEncodeInterface;


//simulate deviceInfo interface for video engine, bridge screen/application and real screen/application device info

class ScreenDeviceInfoImpl : public VideoCaptureModule::DeviceInfo {
public:
  ScreenDeviceInfoImpl(const int32_t id);
  virtual ~ScreenDeviceInfoImpl(void);

  int32_t Init();

  virtual uint32_t NumberOfDevices();
  virtual int32_t GetDeviceName(uint32_t deviceNumber,
                                char* deviceNameUTF8,
                                uint32_t deviceNameLength,
                                char* deviceUniqueIdUTF8,
                                uint32_t deviceUniqueIdUTF8Length,
                                char* productUniqueIdUTF8,
                                uint32_t productUniqueIdUTF8Length);

  virtual int32_t DisplayCaptureSettingsDialogBox(const char* deviceUniqueIdUTF8,
                                                  const char* dialogTitleUTF8,
                                                  void* parentWindow,
                                                  uint32_t positionX,
                                                  uint32_t positionY);
  virtual int32_t NumberOfCapabilities(const char* deviceUniqueIdUTF8);
  virtual int32_t GetCapability(const char* deviceUniqueIdUTF8,
                                const uint32_t deviceCapabilityNumber,
                                VideoCaptureCapability& capability);

  virtual int32_t GetBestMatchedCapability(const char* deviceUniqueIdUTF8,
                                           const VideoCaptureCapability& requested,
                                           VideoCaptureCapability& resulting);
  virtual int32_t GetOrientation(const char* deviceUniqueIdUTF8,
                                 VideoCaptureRotation& orientation);
protected:
  int32_t _id;
  scoped_ptr<DesktopDeviceInfo> desktop_device_info_;

};

class AppDeviceInfoImpl : public VideoCaptureModule::DeviceInfo {
public:
  AppDeviceInfoImpl(const int32_t id);
  virtual ~AppDeviceInfoImpl(void);

  int32_t Init();

  virtual uint32_t NumberOfDevices();
  virtual int32_t GetDeviceName(uint32_t deviceNumber,
                                char* deviceNameUTF8,
                                uint32_t deviceNameLength,
                                char* deviceUniqueIdUTF8,
                                uint32_t deviceUniqueIdUTF8Length,
                                char* productUniqueIdUTF8,
                                uint32_t productUniqueIdUTF8Length);

  virtual int32_t DisplayCaptureSettingsDialogBox(const char* deviceUniqueIdUTF8,
                                                  const char* dialogTitleUTF8,
                                                  void* parentWindow,
                                                  uint32_t positionX,
                                                  uint32_t positionY);
  virtual int32_t NumberOfCapabilities(const char* deviceUniqueIdUTF8);
  virtual int32_t GetCapability(const char* deviceUniqueIdUTF8,
                                const uint32_t deviceCapabilityNumber,
                                VideoCaptureCapability& capability);

  virtual int32_t GetBestMatchedCapability(const char* deviceUniqueIdUTF8,
                                           const VideoCaptureCapability& requested,
                                           VideoCaptureCapability& resulting);
  virtual int32_t GetOrientation(const char* deviceUniqueIdUTF8,
                                 VideoCaptureRotation& orientation);
protected:
  int32_t _id;
  scoped_ptr<DesktopDeviceInfo> desktop_device_info_;
};

// Reuses the video engine pipeline for screen sharing.
// As with video, DesktopCaptureImpl is a proxy for screen sharing
// and follows the video pipeline design
class DesktopCaptureImpl: public VideoCaptureModule,
                          public VideoCaptureExternal,
                          public ScreenCapturer::Callback,
                          public ScreenCapturer::MouseShapeObserver
{
public:
  /* Create a screen capture modules object
   */
  static VideoCaptureModule* Create(const int32_t id, const char* uniqueId, const bool bIsApp);
  static VideoCaptureModule::DeviceInfo* CreateDeviceInfo(const int32_t id, const bool bIsApp);

  int32_t Init(const char* uniqueId,const bool bIsApp);
  // Implements Module declared functions.
  virtual int32_t ChangeUniqueId(const int32_t id) OVERRIDE;

  //Call backs
  virtual void RegisterCaptureDataCallback(VideoCaptureDataCallback& dataCallback) OVERRIDE;
  virtual void DeRegisterCaptureDataCallback() OVERRIDE;
  virtual void RegisterCaptureCallback(VideoCaptureFeedBack& callBack) OVERRIDE;
  virtual void DeRegisterCaptureCallback() OVERRIDE;

  virtual void SetCaptureDelay(int32_t delayMS) OVERRIDE;
  virtual int32_t CaptureDelay() OVERRIDE;
  virtual int32_t SetCaptureRotation(VideoCaptureRotation rotation) OVERRIDE;

  virtual void EnableFrameRateCallback(const bool enable) OVERRIDE;
  virtual void EnableNoPictureAlarm(const bool enable) OVERRIDE;

  virtual const char* CurrentDeviceName() const OVERRIDE;

  // Module handling
  virtual int32_t TimeUntilNextProcess() OVERRIDE;
  virtual int32_t Process() OVERRIDE;

  // Implement VideoCaptureExternal
  // |capture_time| must be specified in the NTP time format in milliseconds.
  virtual int32_t IncomingFrame(uint8_t* videoFrame,
                                int32_t videoFrameLength,
                                const VideoCaptureCapability& frameInfo,
                                int64_t captureTime = 0) OVERRIDE;
  virtual int32_t IncomingI420VideoFrame(
                                         I420VideoFrame* video_frame,
                                         int64_t captureTime = 0) OVERRIDE;

  // Platform dependent
  virtual int32_t StartCapture(const VideoCaptureCapability& capability) OVERRIDE;
  virtual int32_t StopCapture() OVERRIDE;
  virtual bool CaptureStarted() OVERRIDE;
  virtual int32_t CaptureSettings(VideoCaptureCapability& settings) OVERRIDE;
  VideoCaptureEncodeInterface* GetEncodeInterface(const VideoCodec& codec) OVERRIDE { return NULL; }

  //ScreenCapturer::Callback
  virtual SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  virtual void OnCaptureCompleted(DesktopFrame* frame) OVERRIDE;

  //ScreenCapturer::MouseShapeObserver
  virtual void OnCursorShapeChanged(MouseCursorShape* cursor_shape) OVERRIDE;

protected:
  DesktopCaptureImpl(const int32_t id);
  virtual ~DesktopCaptureImpl();
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

  // Delta used for translating between NTP and internal timestamps.
  const int64_t delta_ntp_internal_ms_;

public:
  static bool Run(void*obj) {
    static_cast<DesktopCaptureImpl*>(obj)->process();
    return true;
  };
  void process();

private:
  scoped_ptr<DesktopAndCursorComposer> desktop_capturer_cursor_composer_;
  ThreadWrapper&  capturer_thread_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MAIN_SOURCE_DESKTOP_CAPTURE_IMPL_H_
