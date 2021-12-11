/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CAPTURE_VIDEO_CAPTURE_H_
#define MODULES_VIDEO_CAPTURE_VIDEO_CAPTURE_H_

#include "modules/audio_processing/include/config.h"
#include "api/video/video_rotation.h"
#include "media/base/videosinkinterface.h"
#include "modules/include/module.h"
#include "modules/video_capture/video_capture_defines.h"
#include <set>

#if defined(ANDROID)
#include <jni.h>
#endif

namespace webrtc {

// Mozilla addition
enum class CaptureDeviceType {
  Camera,
  Screen,
  Window,
  Browser
};
// Mozilla addition

struct CaptureDeviceInfo {
  CaptureDeviceType type;

  CaptureDeviceInfo() : type(CaptureDeviceType::Camera) {}
  CaptureDeviceInfo(CaptureDeviceType t) : type(t) {}

  static const ConfigOptionID identifier = ConfigOptionID::kCaptureDeviceInfo;
  const char * TypeName() const
  {
    switch(type) {
    case CaptureDeviceType::Camera: {
      return "Camera";
    }
    case CaptureDeviceType::Screen: {
      return "Screen";
    }
    case CaptureDeviceType::Window: {
      return "Window";
    }
    case CaptureDeviceType::Browser: {
      return "Browser";
    }
    }
    assert(false);
    return "UNKOWN-CaptureDeviceType!";
  }
};

class VideoInputFeedBack
{
public:
    virtual void OnDeviceChange() = 0;
protected:
    virtual ~VideoInputFeedBack(){}
};

#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
  int32_t SetCaptureAndroidVM(JavaVM* javaVM);
#endif

class VideoCaptureModule: public rtc::RefCountInterface {
 public:
  // Interface for receiving information about available camera devices.
  class DeviceInfo {
   public:
    virtual uint32_t NumberOfDevices() = 0;
    virtual int32_t Refresh() = 0;
    virtual void DeviceChange() {
      for (auto inputCallBack : _inputCallBacks) {
        inputCallBack->OnDeviceChange();
      }
    }
    virtual void RegisterVideoInputFeedBack(VideoInputFeedBack* callBack) {
      _inputCallBacks.insert(callBack);
    }

    virtual void DeRegisterVideoInputFeedBack(VideoInputFeedBack* callBack) {
      auto it = _inputCallBacks.find(callBack);
      if (it != _inputCallBacks.end()) {
        _inputCallBacks.erase(it);
      }
    }

    // Returns the available capture devices.
    // deviceNumber   - Index of capture device.
    // deviceNameUTF8 - Friendly name of the capture device.
    // deviceUniqueIdUTF8 - Unique name of the capture device if it exist.
    //                      Otherwise same as deviceNameUTF8.
    // productUniqueIdUTF8 - Unique product id if it exist.
    //                       Null terminated otherwise.
    virtual int32_t GetDeviceName(
        uint32_t deviceNumber,
        char* deviceNameUTF8,
        uint32_t deviceNameSize,
        char* deviceUniqueIdUTF8,
        uint32_t deviceUniqueIdUTF8Size,
        char* productUniqueIdUTF8 = 0,
        uint32_t productUniqueIdUTF8Size = 0,
        pid_t* pid = 0) = 0;


    // Returns the number of capabilities this device.
    virtual int32_t NumberOfCapabilities(
        const char* deviceUniqueIdUTF8) = 0;

    // Gets the capabilities of the named device.
    virtual int32_t GetCapability(
        const char* deviceUniqueIdUTF8,
        const uint32_t deviceCapabilityNumber,
        VideoCaptureCapability& capability) = 0;

    // Gets clockwise angle the captured frames should be rotated in order
    // to be displayed correctly on a normally rotated display.
    virtual int32_t GetOrientation(const char* deviceUniqueIdUTF8,
                                   VideoRotation& orientation) = 0;

    // Gets the capability that best matches the requested width, height and
    // frame rate.
    // Returns the deviceCapabilityNumber on success.
    virtual int32_t GetBestMatchedCapability(
        const char* deviceUniqueIdUTF8,
        const VideoCaptureCapability& requested,
        VideoCaptureCapability& resulting) = 0;

     // Display OS /capture device specific settings dialog
    virtual int32_t DisplayCaptureSettingsDialogBox(
        const char* deviceUniqueIdUTF8,
        const char* dialogTitleUTF8,
        void* parentWindow,
        uint32_t positionX,
        uint32_t positionY) = 0;

    virtual ~DeviceInfo() {}
   private:
    std::set<VideoInputFeedBack*> _inputCallBacks;
  };

  //   Register capture data callback
  virtual void RegisterCaptureDataCallback(
      rtc::VideoSinkInterface<VideoFrame> *dataCallback) = 0;

  //  Remove capture data callback
  virtual void DeRegisterCaptureDataCallback(
      rtc::VideoSinkInterface<VideoFrame> *dataCallback) = 0;

  // Start capture device
  virtual int32_t StartCapture(
      const VideoCaptureCapability& capability) = 0;

  virtual int32_t StopCaptureIfAllClientsClose() = 0;

  virtual bool FocusOnSelectedSource() { return false; };

  virtual int32_t StopCapture() = 0;

  // Returns the name of the device used by this module.
  virtual const char* CurrentDeviceName() const = 0;

  // Returns true if the capture device is running
  virtual bool CaptureStarted() = 0;

  // Gets the current configuration.
  virtual int32_t CaptureSettings(VideoCaptureCapability& settings) = 0;

  // Set the rotation of the captured frames.
  // If the rotation is set to the same as returned by
  // DeviceInfo::GetOrientation the captured frames are
  // displayed correctly if rendered.
  virtual int32_t SetCaptureRotation(VideoRotation rotation) = 0;

  // Tells the capture module whether to apply the pending rotation. By default,
  // the rotation is applied and the generated frame is up right. When set to
  // false, generated frames will carry the rotation information from
  // SetCaptureRotation. Return value indicates whether this operation succeeds.
  virtual bool SetApplyRotation(bool enable) = 0;

  // Return whether the rotation is applied or left pending.
  virtual bool GetApplyRotation() = 0;

protected:
  virtual ~VideoCaptureModule() {};
};

}  // namespace webrtc
#endif  // MODULES_VIDEO_CAPTURE_VIDEO_CAPTURE_H_
