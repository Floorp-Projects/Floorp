/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "webrtc/modules/video_capture/mac/avfoundation/video_capture_avfoundation_info_objc.h"
#include "webrtc/modules/video_capture/video_capture.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "nsDebug.h"

namespace webrtc
{
namespace videocapturemodule
{

VideoCaptureMacAVFoundationInfo::VideoCaptureMacAVFoundationInfo(const int32_t id)
{
    nsAutoreleasePool localPool;
    _captureInfo = [[VideoCaptureMacAVFoundationInfoObjC alloc] init];
    [_captureInfo registerOwner:this];
}

VideoCaptureMacAVFoundationInfo::~VideoCaptureMacAVFoundationInfo()
{
    nsAutoreleasePool localPool;
    [_captureInfo registerOwner:nil];
    [_captureInfo release];
}

int32_t VideoCaptureMacAVFoundationInfo::Init()
{

    return 0;
}

uint32_t VideoCaptureMacAVFoundationInfo::NumberOfDevices()
{

    nsAutoreleasePool localPool;
    uint32_t captureDeviceCount =
        [[_captureInfo getCaptureDeviceCount]intValue];
    return captureDeviceCount;

}

int32_t VideoCaptureMacAVFoundationInfo::GetDeviceName(
    uint32_t deviceNumber, char* deviceNameUTF8,
    uint32_t deviceNameLength, char* deviceUniqueIdUTF8,
    uint32_t deviceUniqueIdUTF8Length, char* productUniqueIdUTF8,
    uint32_t productUniqueIdUTF8Length,
    pid_t* pid)
{
    nsAutoreleasePool localPool;
    int errNum = [[_captureInfo getDeviceNamesFromIndex:deviceNumber
                   DefaultName:deviceNameUTF8 WithLength:deviceNameLength
                   AndUniqueID:deviceUniqueIdUTF8
                   WithLength:deviceUniqueIdUTF8Length
                   AndProductID:productUniqueIdUTF8
                   WithLength:productUniqueIdUTF8Length]intValue];
    return errNum;
}

int32_t VideoCaptureMacAVFoundationInfo::NumberOfCapabilities(
    const char* deviceUniqueIdUTF8)
{
    nsAutoreleasePool localPool;
    uint32_t captureCapabilityCount =
        [[_captureInfo getCaptureCapabilityCount:deviceUniqueIdUTF8]intValue];
    return captureCapabilityCount;
}


int32_t VideoCaptureMacAVFoundationInfo::GetCapability(
    const char* deviceUniqueIdUTF8,
    const uint32_t deviceCapabilityNumber,
    VideoCaptureCapability& capability)
{
    nsAutoreleasePool localPool;
    uint32_t result =
        [[_captureInfo getCaptureCapability:deviceUniqueIdUTF8
                               CapabilityId:deviceCapabilityNumber
                           Capability_width:&capability.width
                          Capability_height:&capability.height
                          Capability_maxFPS:&capability.maxFPS
                          Capability_format:&capability.rawType]intValue];

    return result;
}

int32_t VideoCaptureMacAVFoundationInfo::DisplayCaptureSettingsDialogBox(
    const char* deviceUniqueIdUTF8,
    const char* dialogTitleUTF8, void* parentWindow,
    uint32_t positionX, uint32_t positionY)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                 "API not supported on Mac OS X.");
    return -1;
}

int32_t VideoCaptureMacAVFoundationInfo::CreateCapabilityMap(
    const char* deviceUniqueIdUTF8)
{
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, 0,
                 "API not supported on Mac OS X.");
    return -1;
}
}  // namespace videocapturemodule
}  // namespace webrtc
