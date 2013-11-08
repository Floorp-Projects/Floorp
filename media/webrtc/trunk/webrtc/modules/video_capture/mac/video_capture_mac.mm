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

#include <QuickTime/QuickTime.h>

#include "webrtc/modules/video_capture/device_info_impl.h"
#include "webrtc/modules/video_capture/video_capture_config.h"
#include "webrtc/modules/video_capture/video_capture_impl.h"
#include "webrtc/system_wrappers/interface/ref_count.h"
#include "webrtc/system_wrappers/interface/trace.h"

// 10.4 support must be decided runtime. We will just decide which framework to
// use at compile time "work" classes. One for QTKit, one for QuickTime
#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
#include <QuickTime/video_capture_quick_time.h>
#include <QuickTime/video_capture_quick_time_info.h>
#else
#include <QTKit/video_capture_qtkit.h>
#include <QTKit/video_capture_qtkit_info.h>
#endif

namespace webrtc
{
namespace videocapturemodule
{

// static
bool CheckOSVersion()
{
    // Check OSX version
    OSErr err = noErr;

    SInt32 version;

    err = Gestalt(gestaltSystemVersion, &version);
    if (err != noErr)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Could not get OS version");
        return false;
    }

    if (version < 0x00001040) // Older version than Mac OSX 10.4
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "OS version too old: 0x%x", version);
        return false;
    }

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, 0,
                 "OS version compatible: 0x%x", version);

    return true;
}

// static
bool CheckQTVersion()
{
    // Check OSX version
    OSErr err = noErr;

    SInt32 version;

    err = Gestalt(gestaltQuickTime, &version);
    if (err != noErr)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "Could not get QuickTime version");
        return false;
    }

    if (version < 0x07000000) // QT v. 7.x or newer (QT 5.0.2 0x05020000)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, 0,
                     "QuickTime version too old: 0x%x", version);
        return false;
    }

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, 0,
                 "QuickTime version compatible: 0x%x", version);
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

#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    if (webrtc::videocapturemodule::CheckQTVersion() == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "QuickTime version is too old. Could not create video "
                     "capture module. Returning NULL");
        return NULL;
    }

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "%s line %d. QTKit is not supported on this machine. Using "
                 "QuickTime framework to capture video",
                 __FILE__, __LINE__);

    RefCountImpl<videocapturemodule::VideoCaptureMacQuickTime>*
        newCaptureModule =
            new RefCountImpl<videocapturemodule::VideoCaptureMacQuickTime>(id);

    if (!newCaptureModule)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, id,
                     "could not Create for unique device %s, !newCaptureModule",
                     deviceUniqueIdUTF8);
        return NULL;
    }

    if (newCaptureModule->Init(id, deviceUniqueIdUTF8) != 0)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, id,
                     "could not Create for unique device %s, "
                     "newCaptureModule->Init()!=0",
                     deviceUniqueIdUTF8);
        delete newCaptureModule;
        return NULL;
    }

    // Successfully created VideoCaptureMacQuicktime. Return it
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "Module created for unique device %s. Will use QuickTime "
                 "framework to capture",
                 deviceUniqueIdUTF8);
    return newCaptureModule;

#else // QTKit version

    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "Using QTKit framework to capture video", id);

    RefCountImpl<videocapturemodule::VideoCaptureMacQTKit>* newCaptureModule =
        new RefCountImpl<videocapturemodule::VideoCaptureMacQTKit>(id);

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
                 "Module created for unique device %s, will use QTKit "
                 "framework",deviceUniqueIdUTF8);
    return newCaptureModule;
#endif
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

#if __MAC_OS_X_VERSION_MIN_REQUIRED == __MAC_10_4 // QuickTime version
    if (webrtc::videocapturemodule::CheckQTVersion() == false)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, id,
                     "QuickTime version is too old. Could not create video "
                     "capture module. Returning NULL");
        return NULL;
    }

    webrtc::videocapturemodule::VideoCaptureMacQuickTimeInfo* newCaptureInfoModule =
        new webrtc::videocapturemodule::VideoCaptureMacQuickTimeInfo(id);

    if (!newCaptureInfoModule || newCaptureInfoModule->Init() != 0)
    {
        Destroy(newCaptureInfoModule);
        newCaptureInfoModule = NULL;
        WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                     "Failed to Init newCaptureInfoModule created with id %d "
                     "and device \"\" ", id);
        return NULL;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, id,
                 "VideoCaptureModule created for id", id);
    return newCaptureInfoModule;

#else // QTKit version
    webrtc::videocapturemodule::VideoCaptureMacQTKitInfo* newCaptureInfoModule =
        new webrtc::videocapturemodule::VideoCaptureMacQTKitInfo(id);

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

#endif

}

/**************************************************************************
 *
 *    End Create/Destroy VideoCaptureModule
 *
 ***************************************************************************/
}  // namespace videocapturemodule
}  // namespace webrtc
