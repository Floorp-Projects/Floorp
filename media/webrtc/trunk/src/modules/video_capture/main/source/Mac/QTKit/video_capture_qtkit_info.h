/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_INFO_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_INFO_H_

#include "../../video_capture_impl.h"
#include "../../device_info_impl.h"
#include "video_capture_qtkit_utility.h"

#include "map_wrapper.h"


@class VideoCaptureMacQTKitInfoObjC;

namespace webrtc
{
namespace videocapturemodule
{

class VideoCaptureMacQTKitInfo: public DeviceInfoImpl
{
public:

   VideoCaptureMacQTKitInfo(const WebRtc_Word32 id);
    virtual ~VideoCaptureMacQTKitInfo();

    WebRtc_Word32 Init();

    virtual WebRtc_UWord32 NumberOfDevices();

    /*
     * Returns the available capture devices.
     * deviceNumber   -[in] index of capture device
     * deviceNameUTF8 - friendly name of the capture device
     * deviceUniqueIdUTF8 - unique name of the capture device if it exist.
     *      Otherwise same as deviceNameUTF8
     * productUniqueIdUTF8 - unique product id if it exist. Null terminated
     *      otherwise.
     */
    virtual WebRtc_Word32 GetDeviceName(
        WebRtc_UWord32 deviceNumber, char* deviceNameUTF8,
        WebRtc_UWord32 deviceNameLength, char* deviceUniqueIdUTF8,
        WebRtc_UWord32 deviceUniqueIdUTF8Length,
        char* productUniqueIdUTF8 = 0,
        WebRtc_UWord32 productUniqueIdUTF8Length = 0);

    /*
     *   Returns the number of capabilities for this device
     */
    virtual WebRtc_Word32 NumberOfCapabilities(
        const char* deviceUniqueIdUTF8);

    /*
     *   Gets the capabilities of the named device
     */
    virtual WebRtc_Word32 GetCapability(
        const char* deviceUniqueIdUTF8,
        const WebRtc_UWord32 deviceCapabilityNumber,
        VideoCaptureCapability& capability);

    /*
     *  Gets the capability that best matches the requested width, height and frame rate.
     *  Returns the deviceCapabilityNumber on success.
     */
    virtual WebRtc_Word32 GetBestMatchedCapability(
        const char* deviceUniqueIdUTF8,
        const VideoCaptureCapability& requested,
        VideoCaptureCapability& resulting);

    /*
     * Display OS /capture device specific settings dialog
     */
    virtual WebRtc_Word32 DisplayCaptureSettingsDialogBox(
        const char* deviceUniqueIdUTF8,
        const char* dialogTitleUTF8, void* parentWindow,
        WebRtc_UWord32 positionX, WebRtc_UWord32 positionY);

protected:
    virtual WebRtc_Word32 CreateCapabilityMap(
        const char* deviceUniqueIdUTF8);

    VideoCaptureMacQTKitInfoObjC*    _captureInfo;
};
}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_INFO_H_
