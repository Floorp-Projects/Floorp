/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_DEVICE_INFO_WINDOWS_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_DEVICE_INFO_WINDOWS_H_

#include "../video_capture_impl.h"
#include "../device_info_impl.h"

#include <Dshow.h>
#include "map_wrapper.h"

// forward declarations
namespace webrtc
{
namespace videocapturemodule
{
struct VideoCaptureCapabilityWindows: public VideoCaptureCapability
{
    WebRtc_UWord32 directShowCapabilityIndex;
    bool supportFrameRateControl;
    VideoCaptureCapabilityWindows()
    {
        directShowCapabilityIndex = 0;
        supportFrameRateControl = false;
    }

};
class DeviceInfoWindows: public DeviceInfoImpl
{
public:
    DeviceInfoWindows(const WebRtc_Word32 id);
    virtual ~DeviceInfoWindows();

    WebRtc_Word32 Init();
    virtual WebRtc_UWord32 NumberOfDevices();

    /*
     * Returns the available capture devices.
     */
    virtual WebRtc_Word32
        GetDeviceName(WebRtc_UWord32 deviceNumber,
                      char* deviceNameUTF8,
                      WebRtc_UWord32 deviceNameLength,
                      char* deviceUniqueIdUTF8,
                      WebRtc_UWord32 deviceUniqueIdUTF8Length,
                      char* productUniqueIdUTF8,
                      WebRtc_UWord32 productUniqueIdUTF8Length);

    /* 
     * Display OS /capture device specific settings dialog
     */
    virtual WebRtc_Word32
        DisplayCaptureSettingsDialogBox(
                                        const char* deviceUniqueIdUTF8,
                                        const char* dialogTitleUTF8,
                                        void* parentWindow,
                                        WebRtc_UWord32 positionX,
                                        WebRtc_UWord32 positionY);

    // Windows specific

    /* Gets a capture device filter
     The user of this API is responsible for releasing the filter when it not needed.
     */
    IBaseFilter * GetDeviceFilter(const char* deviceUniqueIdUTF8,
                                  char* productUniqueIdUTF8 = NULL,
                                  WebRtc_UWord32 productUniqueIdUTF8Length = 0);

    WebRtc_Word32
        GetWindowsCapability(const WebRtc_Word32 capabilityIndex,
                             VideoCaptureCapabilityWindows& windowsCapability);

    static void GetProductId(const char* devicePath,
                             char* productUniqueIdUTF8,
                             WebRtc_UWord32 productUniqueIdUTF8Length);
protected:

    WebRtc_Word32 GetDeviceInfo(WebRtc_UWord32 deviceNumber,
                                char* deviceNameUTF8,
                                WebRtc_UWord32 deviceNameLength,
                                char* deviceUniqueIdUTF8,
                                WebRtc_UWord32 deviceUniqueIdUTF8Length,
                                char* productUniqueIdUTF8,
                                WebRtc_UWord32 productUniqueIdUTF8Length);

    virtual WebRtc_Word32
        CreateCapabilityMap(const char* deviceUniqueIdUTF8);

private:
    ICreateDevEnum* _dsDevEnum;
    IEnumMoniker* _dsMonikerDevEnum;
    bool _CoUninitializeIsRequired;

};
} // namespace videocapturemodule
} // namespace webrtc
#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_WINDOWS_DEVICE_INFO_WINDOWS_H_
