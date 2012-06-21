/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/utility/source/frame_scaler.h"

#ifdef WEBRTC_MODULE_UTILITY_VIDEO

#include "common_video/libyuv/include/scaler.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

FrameScaler::FrameScaler()
    : scaler_(new Scaler()),
      scaled_frame_() {}

FrameScaler::~FrameScaler() {}

int FrameScaler::ResizeFrameIfNeeded(VideoFrame* video_frame,
                                     WebRtc_UWord32 out_width,
                                     WebRtc_UWord32 out_height) {
  if (video_frame->Length() == 0) {
    return -1;
  }

  if ((video_frame->Width() != out_width) ||
      (video_frame->Height() != out_height)) {
    // Set correct scale settings and scale |video_frame| into |scaled_frame_|.
    scaler_->Set(video_frame->Width(), video_frame->Height(), out_width,
                 out_height, kI420, kI420, kScaleBox);
    int out_length = CalcBufferSize(kI420, out_width, out_height);
    scaled_frame_.VerifyAndAllocate(out_length);
    int ret = scaler_->Scale(video_frame->Buffer(), scaled_frame_.Buffer(),
                             out_length);
    if (ret < 0) {
      return ret;
    }

    scaled_frame_.SetWidth(out_width);
    scaled_frame_.SetHeight(out_height);
    scaled_frame_.SetLength(out_length);
    scaled_frame_.SetRenderTime(video_frame->RenderTimeMs());
    scaled_frame_.SetTimeStamp(video_frame->TimeStamp());
    video_frame->SwapFrame(scaled_frame_);
  }
  return 0;
}

}  // namespace webrtc

#endif  // WEBRTC_MODULE_UTILITY_VIDEO
