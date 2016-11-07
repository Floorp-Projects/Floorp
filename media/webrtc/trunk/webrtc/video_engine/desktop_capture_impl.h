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

#include <string>

#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/desktop_capture/screen_capturer.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/modules/desktop_capture/shared_memory.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/modules/desktop_capture/mouse_cursor_shape.h"
#include "webrtc/modules/desktop_capture/desktop_device_info.h"
#include "webrtc/modules/desktop_capture/desktop_and_cursor_composer.h"
#include "webrtc/video_engine/include/vie_capture.h"

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
  int32_t Refresh();

  virtual uint32_t NumberOfDevices();
  virtual int32_t GetDeviceName(uint32_t deviceNumber,
                                char* deviceNameUTF8,
                                uint32_t deviceNameLength,
                                char* deviceUniqueIdUTF8,
                                uint32_t deviceUniqueIdUTF8Length,
                                char* productUniqueIdUTF8,
                                uint32_t productUniqueIdUTF8Length,
                                pid_t* pid);

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
                                 VideoRotation& orientation);
protected:
  int32_t _id;
  rtc::scoped_ptr<DesktopDeviceInfo> desktop_device_info_;

};

class AppDeviceInfoImpl : public VideoCaptureModule::DeviceInfo {
public:
  AppDeviceInfoImpl(const int32_t id);
  virtual ~AppDeviceInfoImpl(void);

  int32_t Init();
  int32_t Refresh();

  virtual uint32_t NumberOfDevices();
  virtual int32_t GetDeviceName(uint32_t deviceNumber,
                                char* deviceNameUTF8,
                                uint32_t deviceNameLength,
                                char* deviceUniqueIdUTF8,
                                uint32_t deviceUniqueIdUTF8Length,
                                char* productUniqueIdUTF8,
                                uint32_t productUniqueIdUTF8Length,
                                pid_t* pid);

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
                                 VideoRotation& orientation);
protected:
  int32_t _id;
  rtc::scoped_ptr<DesktopDeviceInfo> desktop_device_info_;
};

class WindowDeviceInfoImpl : public VideoCaptureModule::DeviceInfo {
public:
  WindowDeviceInfoImpl(const int32_t id) : _id(id) {};
  virtual ~WindowDeviceInfoImpl(void) {};

  int32_t Init();
  int32_t Refresh();

  virtual uint32_t NumberOfDevices();
  virtual int32_t GetDeviceName(uint32_t deviceNumber,
                                char* deviceNameUTF8,
                                uint32_t deviceNameLength,
                                char* deviceUniqueIdUTF8,
                                uint32_t deviceUniqueIdUTF8Length,
                                char* productUniqueIdUTF8,
                                uint32_t productUniqueIdUTF8Length,
                                pid_t* pid);

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
                                 VideoRotation& orientation);
protected:
  int32_t _id;
  rtc::scoped_ptr<DesktopDeviceInfo> desktop_device_info_;

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
  static VideoCaptureModule* Create(const int32_t id, const char* uniqueId, const CaptureDeviceType type);
  static VideoCaptureModule::DeviceInfo* CreateDeviceInfo(const int32_t id, const CaptureDeviceType type);

  int32_t Init(const char* uniqueId, const CaptureDeviceType type);

  //Call backs
  virtual void RegisterCaptureDataCallback(VideoCaptureDataCallback& dataCallback) override;
  virtual void DeRegisterCaptureDataCallback() override;
  virtual void RegisterCaptureCallback(VideoCaptureFeedBack& callBack) override;
  virtual void DeRegisterCaptureCallback() override;

  virtual void SetCaptureDelay(int32_t delayMS) override;
  virtual int32_t CaptureDelay() override;
  virtual int32_t SetCaptureRotation(VideoRotation rotation) override;
  virtual bool SetApplyRotation(bool enable) override;
  virtual bool GetApplyRotation() { return true; }

  virtual void EnableFrameRateCallback(const bool enable) override;
  virtual void EnableNoPictureAlarm(const bool enable) override;

  virtual const char* CurrentDeviceName() const override;

  // Module handling
  virtual int64_t TimeUntilNextProcess() override;
  virtual int32_t Process() override;

  // Implement VideoCaptureExternal
  // |capture_time| must be specified in the NTP time format in milliseconds.
  virtual int32_t IncomingFrame(uint8_t* videoFrame,
                                size_t videoFrameLength,
                                const VideoCaptureCapability& frameInfo,
                                int64_t captureTime = 0) override;

  // Platform dependent
  virtual int32_t StartCapture(const VideoCaptureCapability& capability) override;
  virtual int32_t StopCapture() override;
  virtual bool CaptureStarted() override;
  virtual int32_t CaptureSettings(VideoCaptureCapability& settings) override;
  VideoCaptureEncodeInterface* GetEncodeInterface(const VideoCodec& codec) override { return NULL; }

  //ScreenCapturer::Callback
  virtual SharedMemory* CreateSharedMemory(size_t size) override;
  virtual void OnCaptureCompleted(DesktopFrame* frame) override;

protected:
  DesktopCaptureImpl(const int32_t id);
  virtual ~DesktopCaptureImpl();
  int32_t DeliverCapturedFrame(I420VideoFrame& captureFrame,
                               int64_t capture_time);

  static const uint32_t kMaxDesktopCaptureCpuUsage = 50; // maximum CPU usage in %

  int32_t _id; // Module ID
  std::string _deviceUniqueId; // current Device unique name;
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
  bool _noPictureAlarmCallBack; // true if EnableNoPictureAlarm
  VideoCaptureAlarm _captureAlarm; // current value of the noPictureAlarm

  int32_t _setCaptureDelay; // The currently used capture delay
  VideoCaptureDataCallback* _dataCallBack;
  VideoCaptureFeedBack* _captureCallBack;

  TickTime _lastProcessFrameCount;
  TickTime _incomingFrameTimes[kFrameRateCountHistorySize];// timestamp for local captured frames
  VideoRotation _rotateFrame; //Set if the frame should be rotated by the capture module.

  I420VideoFrame _captureFrame;

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
  rtc::scoped_ptr<DesktopAndCursorComposer> desktop_capturer_cursor_composer_;
  rtc::scoped_ptr<EventWrapper> time_event_;
  rtc::scoped_ptr<ThreadWrapper> capturer_thread_;
  bool started_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MAIN_SOURCE_DESKTOP_CAPTURE_IMPL_H_
