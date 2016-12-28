/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "webrtc/modules/video_capture/mac/qtkit/video_capture_qtkit_info_objc.h"
#include "webrtc/modules/video_capture/video_capture.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc
{
namespace videocapturemodule
{

VideoCaptureMacQTKitInfo::VideoCaptureMacQTKitInfo(const int32_t id) :
    DeviceInfoImpl(id)
{
    _captureInfo = [[VideoCaptureMacQTKitInfoObjC alloc] init];
}

VideoCaptureMacQTKitInfo::~VideoCaptureMacQTKitInfo()
{
    [_captureInfo release];

}

int32_t VideoCaptureMacQTKitInfo::Init()
{

    return 0;
}

uint32_t VideoCaptureMacQTKitInfo::NumberOfDevices()
{

    uint32_t captureDeviceCount =
        [[_captureInfo getCaptureDeviceCount]intValue];
    return captureDeviceCount;

}

int32_t VideoCaptureMacQTKitInfo::GetDeviceName(
    uint32_t deviceNumber, char* deviceNameUTF8,
    uint32_t deviceNameLength, char* deviceUniqueIdUTF8,
    uint32_t deviceUniqueIdUTF8Length, char* productUniqueIdUTF8,
    uint32_t productUniqueIdUTF8Length)
{
    int errNum = [[_captureInfo getDeviceNamesFromIndex:deviceNumber
                   DefaultName:deviceNameUTF8 WithLength:deviceNameLength
                   AndUniqueID:deviceUniqueIdUTF8
                   WithLength:deviceUniqueIdUTF8Length
                   AndProductID:productUniqueIdUTF8
                   WithLength:productUniqueIdUTF8Length]intValue];
    return errNum;
}

int32_t VideoCaptureMacQTKitInfo::NumberOfCapabilities(
    const char* deviceUniqueIdUTF8)
{
    // Not implemented. Mac doesn't use discrete steps in capabilities, rather
    // "analog". QTKit will do it's best to convert frames to what ever format
    // you ask for.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}


int32_t VideoCaptureMacQTKitInfo::GetCapability(
    const char* deviceUniqueIdUTF8,
    const uint32_t deviceCapabilityNumber,
    VideoCaptureCapability& capability)
{
    // Not implemented. Mac doesn't use discrete steps in capabilities, rather
    // "analog". QTKit will do it's best to convert frames to what ever format
    // you ask for.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}


int32_t VideoCaptureMacQTKitInfo::GetBestMatchedCapability(
    const char*deviceUniqueIdUTF8,
    const VideoCaptureCapability& requested, VideoCaptureCapability& resulting)
{
    // Not implemented. Mac doesn't use discrete steps in capabilities, rather
    // "analog". QTKit will do it's best to convert frames to what ever format
    // you ask for.
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}

int32_t VideoCaptureMacQTKitInfo::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8,
    const char* dialogTitleUTF8, void* parentWindow,
    uint32_t positionX, uint32_t positionY)
{

    return [[_captureInfo
             displayCaptureSettingsDialogBoxWithDevice:deviceUniqueIdUTF8
             AndTitle:dialogTitleUTF8
             AndParentWindow:parentWindow AtX:positionX AndY:positionY]
             intValue];
}

int32_t VideoCaptureMacQTKitInfo::CreateCapabilityMap(
    const char* deviceUniqueIdUTF8)
{
    // Not implemented. Mac doesn't use discrete steps in capabilities, rather
    // "analog". QTKit will do it's best to convert frames to what ever format
    // you ask for.
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
     return -1;
}
}  // namespace videocapturemodule
}  // namespace webrtc
