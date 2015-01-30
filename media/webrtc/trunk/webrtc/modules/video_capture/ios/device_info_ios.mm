/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "webrtc/modules/video_capture/ios/device_info_ios.h"
#include "webrtc/modules/video_capture/ios/device_info_ios_objc.h"
#include "webrtc/modules/video_capture/video_capture_impl.h"
#include "webrtc/system_wrappers/interface/trace.h"

using namespace webrtc;
using namespace videocapturemodule;

#define IOS_UNSUPPORTED()                                  \
  WEBRTC_TRACE(kTraceError,                                \
               kTraceVideoCapture,                         \
               _id,                                        \
               "%s is not supported on the iOS platform.", \
               __FUNCTION__);                              \
  return -1;

VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo(
    const int32_t device_id) {
  return new DeviceInfoIos(device_id);
}

DeviceInfoIos::DeviceInfoIos(const int32_t device_id)
    : DeviceInfoImpl(device_id) {}

DeviceInfoIos::~DeviceInfoIos() {}

int32_t DeviceInfoIos::Init() { return 0; }

uint32_t DeviceInfoIos::NumberOfDevices() {
  return [DeviceInfoIosObjC captureDeviceCount];
}

int32_t DeviceInfoIos::GetDeviceName(uint32_t deviceNumber,
                                     char* deviceNameUTF8,
                                     uint32_t deviceNameUTF8Length,
                                     char* deviceUniqueIdUTF8,
                                     uint32_t deviceUniqueIdUTF8Length,
                                     char* productUniqueIdUTF8,
                                     uint32_t productUniqueIdUTF8Length) {
  NSString* deviceName = [DeviceInfoIosObjC deviceNameForIndex:deviceNumber];

  NSString* deviceUniqueId =
      [DeviceInfoIosObjC deviceUniqueIdForIndex:deviceNumber];

  strncpy(deviceNameUTF8, [deviceName UTF8String], deviceNameUTF8Length);
  deviceNameUTF8[deviceNameUTF8Length - 1] = '\0';

  strncpy(deviceUniqueIdUTF8,
          [deviceUniqueId UTF8String],
          deviceUniqueIdUTF8Length);
  deviceUniqueIdUTF8[deviceUniqueIdUTF8Length - 1] = '\0';

  if (productUniqueIdUTF8) {
    productUniqueIdUTF8[0] = '\0';
  }

  return 0;
}

int32_t DeviceInfoIos::NumberOfCapabilities(const char* deviceUniqueIdUTF8) {
  IOS_UNSUPPORTED();
}

int32_t DeviceInfoIos::GetCapability(const char* deviceUniqueIdUTF8,
                                     const uint32_t deviceCapabilityNumber,
                                     VideoCaptureCapability& capability) {
  IOS_UNSUPPORTED();
}

int32_t DeviceInfoIos::GetBestMatchedCapability(
    const char* deviceUniqueIdUTF8,
    const VideoCaptureCapability& requested,
    VideoCaptureCapability& resulting) {
  IOS_UNSUPPORTED();
}

int32_t DeviceInfoIos::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8,
    const char* dialogTitleUTF8,
    void* parentWindow,
    uint32_t positionX,
    uint32_t positionY) {
  IOS_UNSUPPORTED();
}

int32_t DeviceInfoIos::GetOrientation(const char* deviceUniqueIdUTF8,
                                      VideoCaptureRotation& orientation) {
  if (strcmp(deviceUniqueIdUTF8, "Front Camera") == 0) {
    orientation = kCameraRotate0;
  } else {
    orientation = kCameraRotate90;
  }
  return orientation;
}

int32_t DeviceInfoIos::CreateCapabilityMap(const char* deviceUniqueIdUTF8) {
  IOS_UNSUPPORTED();
}
