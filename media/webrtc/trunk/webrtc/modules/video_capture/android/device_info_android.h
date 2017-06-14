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

class DeviceInfoAndroid : public DeviceInfoImpl {
 public:
  static void Initialize(JavaVM* javaVM);
  static void DeInitialize();

  DeviceInfoAndroid();
  virtual ~DeviceInfoAndroid();

  // Set |*index| to the index of the camera matching |deviceUniqueIdUTF8|, or
  // return false if no match.
  bool FindCameraIndex(const char* deviceUniqueIdUTF8, size_t* index);

  virtual int32_t Init();
  virtual uint32_t NumberOfDevices();
  virtual int32_t Refresh();
  virtual int32_t GetDeviceName(
      uint32_t deviceNumber,
      char* deviceNameUTF8,
      uint32_t deviceNameLength,
      char* deviceUniqueIdUTF8,
      uint32_t deviceUniqueIdUTF8Length,
      char* productUniqueIdUTF8 = 0,
      uint32_t productUniqueIdUTF8Length = 0,
      pid_t* pid = 0);
  virtual int32_t CreateCapabilityMap(const char* deviceUniqueIdUTF8);

  virtual int32_t DisplayCaptureSettingsDialogBox(
      const char* /*deviceUniqueIdUTF8*/,
      const char* /*dialogTitleUTF8*/,
      void* /*parentWindow*/,
      uint32_t /*positionX*/,
      uint32_t /*positionY*/) { return -1; }
  virtual int32_t GetOrientation(const char* deviceUniqueIdUTF8,
                                 VideoRotation& orientation);

  // Populate |min_mfps| and |max_mfps| with the closest supported range of the
  // device to |max_fps_to_match|.
  void GetMFpsRange(const char* deviceUniqueIdUTF8,
                    int max_fps_to_match,
                    int* min_mfps,
                    int* max_mfps);

 private:
  enum { kExpectedCaptureDelay = 190};
  static void BuildDeviceList();  
};

}  // namespace videocapturemodule
}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_ANDROID_DEVICE_INFO_ANDROID_H_
