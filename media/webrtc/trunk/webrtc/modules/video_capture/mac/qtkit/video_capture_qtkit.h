/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_H_

#import <QTKit/QTKit.h>

#include <stdio.h>

#include "../../video_capture_impl.h"
#include "video_capture_qtkit_utility.h"
#include "../../device_info_impl.h"


// Forward declaraion
@class VideoCaptureMacQTKitObjC;
@class VideoCaptureMacQTKitInfoObjC;

namespace webrtc
{
namespace videocapturemodule
{

class VideoCaptureMacQTKit : public VideoCaptureImpl
{
public:
    VideoCaptureMacQTKit(const WebRtc_Word32 id);
    virtual ~VideoCaptureMacQTKit();

    /*
    *   Create a video capture module object
    *
    *   id - unique identifier of this video capture module object
    *   deviceUniqueIdUTF8 -  name of the device. Available names can be found
    *       by using GetDeviceName
    *   deviceUniqueIdUTF8Length - length of deviceUniqueIdUTF8
    */
    static void Destroy(VideoCaptureModule* module);

    WebRtc_Word32 Init(const WebRtc_Word32 id,
                       const char* deviceUniqueIdUTF8);


    // Start/Stop
    virtual WebRtc_Word32 StartCapture(
        const VideoCaptureCapability& capability);
    virtual WebRtc_Word32 StopCapture();

    // Properties of the set device

    virtual bool CaptureStarted();

    WebRtc_Word32 CaptureSettings(VideoCaptureCapability& settings);

protected:
    // Help functions
    WebRtc_Word32 SetCameraOutput();

private:
    VideoCaptureMacQTKitObjC*        _captureDevice;
    VideoCaptureMacQTKitInfoObjC*    _captureInfo;
    bool                    _isCapturing;
    WebRtc_Word32            _id;
    WebRtc_Word32            _captureWidth;
    WebRtc_Word32            _captureHeight;
    WebRtc_Word32            _captureFrameRate;
    char                     _currentDeviceNameUTF8[MAX_NAME_LENGTH];
    char                     _currentDeviceUniqueIdUTF8[MAX_NAME_LENGTH];
    char                     _currentDeviceProductUniqueIDUTF8[MAX_NAME_LENGTH];
    WebRtc_Word32            _frameCount;
};
}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_H_
