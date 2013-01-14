/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file implements a class that can be used for scaling frames.

#ifndef WEBRTC_MODULES_UTILITY_SOURCE_FRAME_SCALER_H_
#define WEBRTC_MODULES_UTILITY_SOURCE_FRAME_SCALER_H_

#ifdef WEBRTC_MODULE_UTILITY_VIDEO

#include "common_video/interface/i420_video_frame.h"
#include "engine_configurations.h"
#include "modules/interface/module_common_types.h"
#include "system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class Scaler;
class VideoFrame;

class FrameScaler {
 public:
    FrameScaler();
    ~FrameScaler();

    // Re-sizes |video_frame| so that it has the width |out_width| and height
    // |out_height|.
    int ResizeFrameIfNeeded(I420VideoFrame* video_frame,
                            int out_width,
                            int out_height);

 private:
    scoped_ptr<Scaler> scaler_;
    I420VideoFrame scaled_frame_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULE_UTILITY_VIDEO

#endif  // WEBRTC_MODULES_UTILITY_SOURCE_FRAME_SCALER_H_
