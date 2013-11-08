/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_ANDROID_DEVICE_INFO_ANDROID_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_ANDROID_DEVICE_INFO_ANDROID_H_

#include <jni.h>

#include "webrtc/modules/video_capture/device_info_impl.h"
#include "webrtc/modules/video_capture/video_capture_impl.h"

#define AndroidJavaCaptureDeviceInfoClass "org/webrtc/videoengine/VideoCaptureDeviceInfoAndroid"
#define AndroidJavaCaptureCapabilityClass "org/webrtc/videoengine/CaptureCapabilityAndroid"

namespace webrtc
{
namespace videocapturemodule
{

// Android logging, uncomment to print trace to
// logcat instead of trace file/callback
// #include <android/log.h>
// #define WEBRTC_TRACE(a,b,c,...)
// __android_log_print(ANDROID_LOG_DEBUG, "*WEBRTCN*", __VA_ARGS__)

class DeviceInfoAndroid : public DeviceInfoImpl {

 public:
  static void SetAndroidCaptureClasses(jclass capabilityClass);
  DeviceInfoAndroid(const int32_t id);
  int32_t Init();
  virtual ~DeviceInfoAndroid();
  virtual uint32_t NumberOfDevices();
  virtual int32_t GetDeviceName(
      uint32_t deviceNumber,
      char* deviceNameUTF8,
      uint32_t deviceNameLength,
      char* deviceUniqueIdUTF8,
      uint32_t deviceUniqueIdUTF8Length,
      char* productUniqueIdUTF8 = 0,
      uint32_t productUniqueIdUTF8Length = 0);
  virtual int32_t CreateCapabilityMap(const char* deviceUniqueIdUTF8);

  virtual int32_t DisplayCaptureSettingsDialogBox(
      const char* /*deviceUniqueIdUTF8*/,
      const char* /*dialogTitleUTF8*/,
      void* /*parentWindow*/,
      uint32_t /*positionX*/,
      uint32_t /*positionY*/) { return -1; }
  virtual int32_t GetOrientation(const char* deviceUniqueIdUTF8,
                                 VideoCaptureRotation& orientation);
 private:
  bool IsDeviceNameMatches(const char* name, const char* deviceUniqueIdUTF8);
  enum {_expectedCaptureDelay = 190};
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_ANDROID_DEVICE_INFO_ANDROID_H_
