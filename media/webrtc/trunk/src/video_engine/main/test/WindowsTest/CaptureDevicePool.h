/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

#include "common_types.h"

#include "vie_base.h"
#include "vie_capture.h"
#include "vie_file.h"
#include "map_wrapper.h"

namespace webrtc {
class CriticalSectionWrapper;
}
using namespace webrtc;
class CaptureDevicePool
{
public:
    CaptureDevicePool(VideoEngine* videoEngine);
    ~CaptureDevicePool(void);
    WebRtc_Word32 GetCaptureDevice(int& captureId, const char uniqeDeviceName[256]);
    WebRtc_Word32 ReturnCaptureDevice(int captureId);

    private: 
        struct DeviceItem
        {
            int captureId;
            WebRtc_Word32 refCount;
            char uniqeDeviceName[256];
            DeviceItem()
            {
                captureId=-1;
                refCount=0;
            }
        };
        CriticalSectionWrapper& _critSect;
        ViECapture* _vieCapture;
        ViEFile*    _vieFile;
        MapWrapper _deviceMap;

};
