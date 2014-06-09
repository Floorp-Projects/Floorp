/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/main/source/spatial_resampler.h"


namespace webrtc {

VPMSimpleSpatialResampler::VPMSimpleSpatialResampler()
    : resampling_mode_(kFastRescaling),
      target_width_(0),
      target_height_(0),
      scaler_() {}

VPMSimpleSpatialResampler::~VPMSimpleSpatialResampler() {}


int32_t VPMSimpleSpatialResampler::SetTargetFrameSize(int32_t width,
                                                      int32_t height) {
  if (resampling_mode_ == kNoRescaling) return VPM_OK;

  if (width < 1 || height < 1) return VPM_PARAMETER_ERROR;

  target_width_ = width;
  target_height_ = height;

  return VPM_OK;
}

void VPMSimpleSpatialResampler::SetInputFrameResampleMode(
    VideoFrameResampling resampling_mode) {
  resampling_mode_ = resampling_mode;
}

void VPMSimpleSpatialResampler::Reset() {
  resampling_mode_ = kFastRescaling;
  target_width_ = 0;
  target_height_ = 0;
}

int32_t VPMSimpleSpatialResampler::ResampleFrame(const I420VideoFrame& inFrame,
                                                 I420VideoFrame* outFrame) {
  // Don't copy if frame remains as is.
  if (resampling_mode_ == kNoRescaling)
     return VPM_OK;
  // Check if re-sampling is needed
  else if ((inFrame.width() == target_width_) &&
    (inFrame.height() == target_height_))  {
    return VPM_OK;
  }

  // Setting scaler
  // TODO(mikhal/marpan): Should we allow for setting the filter mode in
  // _scale.Set() with |resampling_mode_|?
  int ret_val = 0;
  ret_val = scaler_.Set(inFrame.width(), inFrame.height(),
                       target_width_, target_height_, kI420, kI420, kScaleBox);
  if (ret_val < 0)
    return ret_val;

  ret_val = scaler_.Scale(inFrame, outFrame);

  // Setting time parameters to the output frame.
  // Timestamp will be reset in Scale call above, so we should set it after.
  outFrame->set_timestamp(inFrame.timestamp());
  outFrame->set_render_time_ms(inFrame.render_time_ms());

  if (ret_val == 0)
    return VPM_OK;
  else
    return VPM_SCALE_ERROR;
}

int32_t VPMSimpleSpatialResampler::TargetHeight() {
  return target_height_;
}

int32_t VPMSimpleSpatialResampler::TargetWidth() {
  return target_width_;
}

bool VPMSimpleSpatialResampler::ApplyResample(int32_t width,
                                              int32_t height) {
  if ((width == target_width_ && height == target_height_) ||
       resampling_mode_ == kNoRescaling)
    return false;
  else
    return true;
}

}  // namespace webrtc
