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

#include "typedefs.h"
#include "video_processing.h"

namespace webrtc {

class VPMBrightnessDetection
{
public:
    VPMBrightnessDetection();
    ~VPMBrightnessDetection();

    WebRtc_Word32 ChangeUniqueId(WebRtc_Word32 id);

    void Reset();

    WebRtc_Word32 ProcessFrame(const I420VideoFrame& frame,
                               const VideoProcessingModule::FrameStats& stats);

private:
    WebRtc_Word32 _id;

    WebRtc_UWord32 _frameCntBright;
    WebRtc_UWord32 _frameCntDark;
};

} //namespace

#endif // VPM_BRIGHTNESS_DETECTION_H
