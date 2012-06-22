/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_CAPTURE_DEVICE_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_CAPTURE_DEVICE_H_

#include <string>

#include "modules/video_capture/main/interface/video_capture_factory.h"

class TbInterfaces;

class TbCaptureDevice
{
public:
    TbCaptureDevice(TbInterfaces& Engine);
    ~TbCaptureDevice(void);

    int captureId;
    void ConnectTo(int videoChannel);
    void Disconnect(int videoChannel);
    std::string device_name() const;

private:
    TbInterfaces& ViE;
    webrtc::VideoCaptureModule* vcpm_;
    std::string device_name_;
};

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_CAPTURE_DEVICE_H_
