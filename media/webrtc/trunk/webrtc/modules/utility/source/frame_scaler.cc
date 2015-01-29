/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/utility/source/frame_scaler.h"

#ifdef WEBRTC_MODULE_UTILITY_VIDEO

#include "webrtc/common_video/libyuv/include/scaler.h"

namespace webrtc {

FrameScaler::FrameScaler()
    : scaler_(new Scaler()),
      scaled_frame_() {}

FrameScaler::~FrameScaler() {}

int FrameScaler::ResizeFrameIfNeeded(I420VideoFrame* video_frame,
                                     int out_width,
                                     int out_height) {
  if (video_frame->IsZeroSize()) {
    return -1;
  }

  if ((video_frame->width() != out_width) ||
      (video_frame->height() != out_height)) {
    // Set correct scale settings and scale |video_frame| into |scaled_frame_|.
    scaler_->Set(video_frame->width(), video_frame->height(), out_width,
                 out_height, kI420, kI420, kScaleBox);
    int ret = scaler_->Scale(*video_frame, &scaled_frame_);
    if (ret < 0) {
      return ret;
    }

    scaled_frame_.set_render_time_ms(video_frame->render_time_ms());
    scaled_frame_.set_timestamp(video_frame->timestamp());
    video_frame->SwapFrame(&scaled_frame_);
  }
  return 0;
}

}  // namespace webrtc

#endif  // WEBRTC_MODULE_UTILITY_VIDEO
