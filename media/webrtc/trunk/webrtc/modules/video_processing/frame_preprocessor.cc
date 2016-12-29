/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/frame_preprocessor.h"

#include "webrtc/modules/video_processing/video_denoiser.h"

namespace webrtc {

VPMFramePreprocessor::VPMFramePreprocessor()
    : content_metrics_(nullptr),
      resampled_frame_(),
      enable_ca_(false),
      frame_cnt_(0) {
  spatial_resampler_ = new VPMSimpleSpatialResampler();
  ca_ = new VPMContentAnalysis(true);
  vd_ = new VPMVideoDecimator();
}

VPMFramePreprocessor::~VPMFramePreprocessor() {
  Reset();
  delete ca_;
  delete vd_;
  delete spatial_resampler_;
}

void VPMFramePreprocessor::Reset() {
  ca_->Release();
  vd_->Reset();
  content_metrics_ = nullptr;
  spatial_resampler_->Reset();
  enable_ca_ = false;
  frame_cnt_ = 0;
}

void VPMFramePreprocessor::EnableTemporalDecimation(bool enable) {
  vd_->EnableTemporalDecimation(enable);
}

void VPMFramePreprocessor::EnableContentAnalysis(bool enable) {
  enable_ca_ = enable;
}

void VPMFramePreprocessor::SetInputFrameResampleMode(
    VideoFrameResampling resampling_mode) {
  spatial_resampler_->SetInputFrameResampleMode(resampling_mode);
}

int32_t VPMFramePreprocessor::SetTargetResolution(uint32_t width,
                                                  uint32_t height,
                                                  uint32_t frame_rate) {
  if ((width == 0) || (height == 0) || (frame_rate == 0)) {
    return VPM_PARAMETER_ERROR;
  }
  int32_t ret_val = 0;
  ret_val = spatial_resampler_->SetTargetFrameSize(width, height);

  if (ret_val < 0)
    return ret_val;

  vd_->SetTargetFramerate(frame_rate);
  return VPM_OK;
}

void VPMFramePreprocessor::SetTargetFramerate(int frame_rate) {
  if (frame_rate == -1) {
    vd_->EnableTemporalDecimation(false);
  } else {
    vd_->EnableTemporalDecimation(true);
    vd_->SetTargetFramerate(frame_rate);
  }
}

void VPMFramePreprocessor::UpdateIncomingframe_rate() {
  vd_->UpdateIncomingframe_rate();
}

uint32_t VPMFramePreprocessor::GetDecimatedFrameRate() {
  return vd_->GetDecimatedFrameRate();
}

uint32_t VPMFramePreprocessor::GetDecimatedWidth() const {
  return spatial_resampler_->TargetWidth();
}

uint32_t VPMFramePreprocessor::GetDecimatedHeight() const {
  return spatial_resampler_->TargetHeight();
}

void VPMFramePreprocessor::EnableDenosing(bool enable) {
  denoiser_.reset(new VideoDenoiser(true));
}

const VideoFrame* VPMFramePreprocessor::PreprocessFrame(
    const VideoFrame& frame) {
  if (frame.IsZeroSize()) {
    return nullptr;
  }

  vd_->UpdateIncomingframe_rate();
  if (vd_->DropFrame()) {
    return nullptr;
  }

  const VideoFrame* current_frame = &frame;
  if (denoiser_) {
    denoiser_->DenoiseFrame(*current_frame, &denoised_frame_);
    current_frame = &denoised_frame_;
  }

  if (spatial_resampler_->ApplyResample(current_frame->width(),
                                        current_frame->height())) {
    if (spatial_resampler_->ResampleFrame(*current_frame, &resampled_frame_) !=
        VPM_OK) {
      return nullptr;
    }
    current_frame = &resampled_frame_;
  }

  // Perform content analysis on the frame to be encoded.
  if (enable_ca_ && frame_cnt_ % kSkipFrameCA == 0) {
    // Compute new metrics every |kSkipFramesCA| frames, starting with
    // the first frame.
    content_metrics_ = ca_->ComputeContentMetrics(*current_frame);
  }
  ++frame_cnt_;
  return current_frame;
}

VideoContentMetrics* VPMFramePreprocessor::GetContentMetrics() const {
  return content_metrics_;
}

}  // namespace webrtc
