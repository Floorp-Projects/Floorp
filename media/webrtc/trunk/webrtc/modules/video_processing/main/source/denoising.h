/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_MAIN_SOURCE_DENOISING_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_MAIN_SOURCE_DENOISING_H_

#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class VPMDenoising {
 public:
  VPMDenoising();
  ~VPMDenoising();

  int32_t ChangeUniqueId(int32_t id);

  void Reset();

  int32_t ProcessFrame(I420VideoFrame* frame);

 private:
  int32_t id_;

  uint32_t* moment1_;  // (Q8) First order moment (mean).
  uint32_t* moment2_;  // (Q8) Second order moment.
  uint32_t  frame_size_;  // Size (# of pixels) of frame.
  int denoise_frame_cnt_;  // Counter for subsampling in time.
};

}  // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_PROCESSING_MAIN_SOURCE_DENOISING_H_

