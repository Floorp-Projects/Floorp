/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * brightness_detection.h
 */
#ifndef VPM_BRIGHTNESS_DETECTION_H
#define VPM_BRIGHTNESS_DETECTION_H

#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class VPMBrightnessDetection
{
public:
    VPMBrightnessDetection();
    ~VPMBrightnessDetection();

    int32_t ChangeUniqueId(int32_t id);

    void Reset();

    int32_t ProcessFrame(const I420VideoFrame& frame,
                         const VideoProcessingModule::FrameStats& stats);

private:
    int32_t _id;

    uint32_t _frameCntBright;
    uint32_t _frameCntDark;
};

} //namespace

#endif // VPM_BRIGHTNESS_DETECTION_H
