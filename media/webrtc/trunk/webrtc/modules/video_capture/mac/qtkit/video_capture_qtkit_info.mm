/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "trace.h"
#include "../../video_capture_config.h"
#import "video_capture_qtkit_info_objc.h"

#include "video_capture.h"

namespace webrtc
{
namespace videocapturemodule
{

VideoCaptureMacQTKitInfo::VideoCaptureMacQTKitInfo(const WebRtc_Word32 id) :
    DeviceInfoImpl(id)
{
    _captureInfo = [[VideoCaptureMacQTKitInfoObjC alloc] init];
}

VideoCaptureMacQTKitInfo::~VideoCaptureMacQTKitInfo()
{
    [_captureInfo release];

}

WebRtc_Word32 VideoCaptureMacQTKitInfo::Init()
{

    return 0;
}

WebRtc_UWord32 VideoCaptureMacQTKitInfo::NumberOfDevices()
{

    WebRtc_UWord32 captureDeviceCount =
        [[_captureInfo getCaptureDeviceCount]intValue];
    return captureDeviceCount;

}

WebRtc_Word32 VideoCaptureMacQTKitInfo::GetDeviceName(
    WebRtc_UWord32 deviceNumber, char* deviceNameUTF8,
    WebRtc_UWord32 deviceNameLength, char* deviceUniqueIdUTF8,
    WebRtc_UWord32 deviceUniqueIdUTF8Length, char* productUniqueIdUTF8,
    WebRtc_UWord32 productUniqueIdUTF8Length)
{
    int errNum = [[_captureInfo getDeviceNamesFromIndex:deviceNumber
                   DefaultName:deviceNameUTF8 WithLength:deviceNameLength
                   AndUniqueID:deviceUniqueIdUTF8
                   WithLength:deviceUniqueIdUTF8Length
                   AndProductID:productUniqueIdUTF8
                   WithLength:productUniqueIdUTF8Length]intValue];
    return errNum;
}

WebRtc_Word32 VideoCaptureMacQTKitInfo::NumberOfCapabilities(
    const char* deviceUniqueIdUTF8)
{
    // Not implemented. Mac doesn't use discrete steps in capabilities, rather
    // "analog". QTKit will do it's best to convert frames to what ever format
    // you ask for.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}


WebRtc_Word32 VideoCaptureMacQTKitInfo::GetCapability(
    const char* deviceUniqueIdUTF8,
    const WebRtc_UWord32 deviceCapabilityNumber,
    VideoCaptureCapability& capability)
{
    // Not implemented. Mac doesn't use discrete steps in capabilities, rather
    // "analog". QTKit will do it's best to convert frames to what ever format
    // you ask for.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "NumberOfCapabilities is not supported on the Mac platform.");
    return -1;
}


WebRtc_Word32 VideoCaptureMacQTKitInfo::GetBestMatchedCapability(
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

WebRtc_Word32 VideoCaptureMacQTKitInfo::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8,
    const char* dialogTitleUTF8, void* parentWindow,
    WebRtc_UWord32 positionX, WebRtc_UWord32 positionY)
{

    return [[_captureInfo
             displayCaptureSettingsDialogBoxWithDevice:deviceUniqueIdUTF8
             AndTitle:dialogTitleUTF8
             AndParentWindow:parentWindow AtX:positionX AndY:positionY]
             intValue];
}

WebRtc_Word32 VideoCaptureMacQTKitInfo::CreateCapabilityMap(
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
