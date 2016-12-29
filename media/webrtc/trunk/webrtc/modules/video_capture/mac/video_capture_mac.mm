/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  video_capture_mac.cc
 *
 */

#include "webrtc/modules/video_capture/device_info_impl.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/modules/video_capture/video_capture_impl.h"
#include "webrtc/system_wrappers/include/ref_count.h"
#include "webrtc/system_wrappers/include/trace.h"

#include "webrtc/modules/video_capture/mac/avfoundation/video_capture_avfoundation.h"
#include "webrtc/modules/video_capture/mac/avfoundation/video_capture_avfoundation_info.h"

namespace webrtc
{
namespace videocapturemodule
{

// static
bool CheckOSVersion()
{
    // Check OSX version is at least 10.7 (min for AVFoundation)
    int major = 0;
    int minor = 0;

    NSString* versionString = [[NSDictionary dictionaryWithContentsOfFile:
                                @"/System/Library/CoreServices/SystemVersion.plist"] objectForKey:@"ProductVersion"];
    NSArray* versions = [versionString componentsSeparatedByString:@"."];
    NSUInteger count = [versions count];
    if (count > 0) {
        major = [(NSString *)[versions objectAtIndex:0] integerValue];
        if (count > 1) {
            minor = [(NSString *)[versions objectAtIndex:1] integerValue];
        }
    }

    if (major < 10)
    {
      return false;
    }
    if ((major == 10) && (minor < 7)) {
      return false;
    }

    return true;
}

/**************************************************************************
 *
 *    Create/Destroy a VideoCaptureModule
 *
 ***************************************************************************/

/*
 *   Returns version of the module and its components
 *
 *   version                 - buffer to which the version will be written
 *   remainingBufferInBytes  - remaining number of int8_t in the version
 *                             buffer
 *   position                - position of the next empty int8_t in the
 *                             version buffer
 */

VideoCaptureModule* VideoCaptureImpl::Create(
    const int32_t id, const char* deviceUniqueIdUTF8)
{

    if (webrtc::videocapturemodule::CheckOSVersion() == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "OS version is too old. Could not create video capture "
                     "module. Returning NULL");
        return NULL;
    }

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "Using AVFoundation framework to capture video", id);

    RefCountImpl<videocapturemodule::VideoCaptureMacAVFoundation>* newCaptureModule =
        new RefCountImpl<videocapturemodule::VideoCaptureMacAVFoundation>(id);

    if(!newCaptureModule)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, id,
                     "could not Create for unique device %s, !newCaptureModule",
                     deviceUniqueIdUTF8);
        return NULL;
    }
    if(newCaptureModule->Init(id, deviceUniqueIdUTF8) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, id,
                     "could not Create for unique device %s, "
                     "newCaptureModule->Init()!=0", deviceUniqueIdUTF8);
        delete newCaptureModule;
        return NULL;
    }

    // Successfully created VideoCaptureMacQuicktime. Return it
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "Module created for unique device %s, will use AVFoundation "
                 "framework",deviceUniqueIdUTF8);
    return newCaptureModule;
}

/**************************************************************************
 *
 *    Create/Destroy a DeviceInfo
 *
 ***************************************************************************/

VideoCaptureModule::DeviceInfo*
VideoCaptureImpl::CreateDeviceInfo(const int32_t id)
{


    if (webrtc::videocapturemodule::CheckOSVersion() == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "OS version is too old. Could not create video capture "
                     "module. Returning NULL");
        return NULL;
    }

    webrtc::videocapturemodule::VideoCaptureMacAVFoundationInfo* newCaptureInfoModule =
        new webrtc::videocapturemodule::VideoCaptureMacAVFoundationInfo(id);

    if(!newCaptureInfoModule || newCaptureInfoModule->Init() != 0)
    {
        //Destroy(newCaptureInfoModule);
        delete newCaptureInfoModule;
        newCaptureInfoModule = NULL;
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                     "Failed to Init newCaptureInfoModule created with id %d "
                     "and device \"\" ", id);
        return NULL;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "VideoCaptureModule created for id", id);
    return newCaptureInfoModule;

}

/**************************************************************************
 *
 *    End Create/Destroy VideoCaptureModule
 *
 ***************************************************************************/
}  // namespace videocapturemodule
}  // namespace webrtc
