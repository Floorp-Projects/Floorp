/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_BRIGHTNESS_DETECTION_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_BRIGHTNESS_DETECTION_H_

#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class VPMBrightnessDetection {
 public:
  VPMBrightnessDetection();
  ~VPMBrightnessDetection();

  void Reset();
  int32_t ProcessFrame(const VideoFrame& frame,
                       const VideoProcessing::FrameStats& stats);

 private:
  uint32_t frame_cnt_bright_;
  uint32_t frame_cnt_dark_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_BRIGHTNESS_DETECTION_H_
