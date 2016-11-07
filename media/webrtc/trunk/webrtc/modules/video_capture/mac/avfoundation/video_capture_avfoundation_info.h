/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_AVFOUNDATION_VIDEO_CAPTURE_AVFOUNDATION_INFO_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_AVFOUNDATION_VIDEO_CAPTURE_AVFOUNDATION_INFO_H_

#include "webrtc/modules/video_capture/device_info_impl.h"
#include "webrtc/modules/video_capture/mac/avfoundation/video_capture_avfoundation_utility.h"
#include "webrtc/modules/video_capture/video_capture_impl.h"


@class VideoCaptureMacAVFoundationInfoObjC;

namespace webrtc
{
namespace videocapturemodule
{

class VideoCaptureMacAVFoundationInfo: public DeviceInfoImpl
{
public:

   VideoCaptureMacAVFoundationInfo(const int32_t id);
    virtual ~VideoCaptureMacAVFoundationInfo();

    int32_t Init();

    virtual uint32_t NumberOfDevices();

    /*
     * Returns the available capture devices.
     * deviceNumber   -[in] index of capture device
     * deviceNameUTF8 - friendly name of the capture device
     * deviceUniqueIdUTF8 - unique name of the capture device if it exist.
     *      Otherwise same as deviceNameUTF8
     * productUniqueIdUTF8 - unique product id if it exist. Null terminated
     *      otherwise.
     */
    virtual int32_t GetDeviceName(
        uint32_t deviceNumber, char* deviceNameUTF8,
        uint32_t deviceNameLength, char* deviceUniqueIdUTF8,
        uint32_t deviceUniqueIdUTF8Length,
        char* productUniqueIdUTF8 = 0,
        uint32_t productUniqueIdUTF8Length = 0,
        pid_t* pid = 0);

    /*
     *   Returns the number of capabilities for this device
     */
    virtual int32_t NumberOfCapabilities(
        const char* deviceUniqueIdUTF8);

    /*
     *   Gets the capabilities of the named device
     */
    virtual int32_t GetCapability(
        const char* deviceUniqueIdUTF8,
        const uint32_t deviceCapabilityNumber,
        VideoCaptureCapability& capability);

    /*
     * Display OS /capture device specific settings dialog
     */
    virtual int32_t DisplayCaptureSettingsDialogBox(
        const char* deviceUniqueIdUTF8,
        const char* dialogTitleUTF8, void* parentWindow,
        uint32_t positionX, uint32_t positionY);

protected:
    virtual int32_t CreateCapabilityMap(
        const char* deviceUniqueIdUTF8);

    VideoCaptureMacAVFoundationInfoObjC*    _captureInfo;
};
}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_AVFOUNDATION_VIDEO_CAPTURE_AVFOUNDATION_INFO_H_
