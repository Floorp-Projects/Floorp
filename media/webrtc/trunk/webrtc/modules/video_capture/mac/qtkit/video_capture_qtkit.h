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

#include "webrtc/modules/video_capture/device_info_impl.h"
#include "webrtc/modules/video_capture/mac/qtkit/video_capture_qtkit_utility.h"
#include "webrtc/modules/video_capture/video_capture_impl.h"

@class VideoCaptureMacQTKitObjC;
@class VideoCaptureMacQTKitInfoObjC;

namespace webrtc
{
namespace videocapturemodule
{

class VideoCaptureMacQTKit : public VideoCaptureImpl
{
public:
    VideoCaptureMacQTKit(const int32_t id);
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

    int32_t Init(const int32_t id, const char* deviceUniqueIdUTF8);


    // Start/Stop
    virtual int32_t StartCapture(
        const VideoCaptureCapability& capability);
    virtual int32_t StopCapture();

    // Properties of the set device

    virtual bool CaptureStarted();

    int32_t CaptureSettings(VideoCaptureCapability& settings);

protected:
    // Help functions
    int32_t SetCameraOutput();

private:
    VideoCaptureMacQTKitObjC*        _captureDevice;
    VideoCaptureMacQTKitInfoObjC*    _captureInfo;
    bool                    _isCapturing;
    int32_t            _id;
    int32_t            _captureWidth;
    int32_t            _captureHeight;
    int32_t            _captureFrameRate;
    char                     _currentDeviceNameUTF8[MAX_NAME_LENGTH];
    char                     _currentDeviceUniqueIdUTF8[MAX_NAME_LENGTH];
    char                     _currentDeviceProductUniqueIDUTF8[MAX_NAME_LENGTH];
    int32_t            _frameCount;
};
}  // namespace videocapturemodule
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_MAC_QTKIT_VIDEO_CAPTURE_QTKIT_H_
