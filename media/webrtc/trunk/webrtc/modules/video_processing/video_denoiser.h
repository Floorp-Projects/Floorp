/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DENOISER_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DENOISER_H_

#include "webrtc/modules/video_processing/util/denoiser_filter.h"
#include "webrtc/modules/video_processing/util/skin_detection.h"

namespace webrtc {

class VideoDenoiser {
 public:
  explicit VideoDenoiser(bool runtime_cpu_detection);
  void DenoiseFrame(const VideoFrame& frame, VideoFrame* denoised_frame);

 private:
  void TrailingReduction(int mb_rows,
                         int mb_cols,
                         const uint8_t* y_src,
                         int stride_y,
                         uint8_t* y_dst);
  int width_;
  int height_;
  rtc::scoped_ptr<DenoiseMetrics[]> metrics_;
  rtc::scoped_ptr<DenoiserFilter> filter_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_DENOISER_H_
