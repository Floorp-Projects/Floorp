/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/source/content_metrics_processing.h"

#include <math.h>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"

namespace webrtc {
//////////////////////////////////
/// VCMContentMetricsProcessing //
//////////////////////////////////

VCMContentMetricsProcessing::VCMContentMetricsProcessing()
    : recursive_avg_factor_(1 / 150.0f),  // matched to  30fps.
      frame_cnt_uniform_avg_(0),
      avg_motion_level_(0.0f),
      avg_spatial_level_(0.0f) {
  recursive_avg_ = new VideoContentMetrics();
  uniform_avg_ = new VideoContentMetrics();
}

VCMContentMetricsProcessing::~VCMContentMetricsProcessing() {
  delete recursive_avg_;
  delete uniform_avg_;
}

int VCMContentMetricsProcessing::Reset() {
  recursive_avg_->Reset();
  uniform_avg_->Reset();
  frame_cnt_uniform_avg_ = 0;
  avg_motion_level_  = 0.0f;
  avg_spatial_level_ = 0.0f;
  return VCM_OK;
}

void VCMContentMetricsProcessing::UpdateFrameRate(uint32_t frameRate) {
  // Update factor for recursive averaging.
  recursive_avg_factor_ = static_cast<float> (1000.0f) /
      static_cast<float>(frameRate *  kQmMinIntervalMs);
}

VideoContentMetrics* VCMContentMetricsProcessing::LongTermAvgData() {
  return recursive_avg_;
}

VideoContentMetrics* VCMContentMetricsProcessing::ShortTermAvgData() {
  if (frame_cnt_uniform_avg_ == 0) {
    return NULL;
  }
  // Two metrics are used: motion and spatial level.
  uniform_avg_->motion_magnitude = avg_motion_level_ /
      static_cast<float>(frame_cnt_uniform_avg_);
  uniform_avg_->spatial_pred_err = avg_spatial_level_ /
      static_cast<float>(frame_cnt_uniform_avg_);
  return uniform_avg_;
}

void VCMContentMetricsProcessing::ResetShortTermAvgData() {
  // Reset.
  avg_motion_level_ = 0.0f;
  avg_spatial_level_ = 0.0f;
  frame_cnt_uniform_avg_ = 0;
}

int VCMContentMetricsProcessing::UpdateContentData(
    const VideoContentMetrics *contentMetrics) {
  if (contentMetrics == NULL) {
    return VCM_OK;
  }
  return ProcessContent(contentMetrics);
}

int VCMContentMetricsProcessing::ProcessContent(
    const VideoContentMetrics *contentMetrics) {
  // Update the recursive averaged metrics: average is over longer window
  // of time: over QmMinIntervalMs ms.
  UpdateRecursiveAvg(contentMetrics);
  // Update the uniform averaged metrics: average is over shorter window
  // of time: based on ~RTCP reports.
  UpdateUniformAvg(contentMetrics);
  return VCM_OK;
}

void VCMContentMetricsProcessing::UpdateUniformAvg(
    const VideoContentMetrics *contentMetrics) {
  // Update frame counter.
  frame_cnt_uniform_avg_ += 1;
  // Update averaged metrics: motion and spatial level are used.
  avg_motion_level_ += contentMetrics->motion_magnitude;
  avg_spatial_level_ +=  contentMetrics->spatial_pred_err;
  return;
}

void VCMContentMetricsProcessing::UpdateRecursiveAvg(
    const VideoContentMetrics *contentMetrics) {

  // Spatial metrics: 2x2, 1x2(H), 2x1(V).
  recursive_avg_->spatial_pred_err = (1 - recursive_avg_factor_) *
      recursive_avg_->spatial_pred_err +
      recursive_avg_factor_ * contentMetrics->spatial_pred_err;

  recursive_avg_->spatial_pred_err_h = (1 - recursive_avg_factor_) *
      recursive_avg_->spatial_pred_err_h +
      recursive_avg_factor_ * contentMetrics->spatial_pred_err_h;

  recursive_avg_->spatial_pred_err_v = (1 - recursive_avg_factor_) *
      recursive_avg_->spatial_pred_err_v +
      recursive_avg_factor_ * contentMetrics->spatial_pred_err_v;

  // Motion metric: Derived from NFD (normalized frame difference).
  recursive_avg_->motion_magnitude = (1 - recursive_avg_factor_) *
      recursive_avg_->motion_magnitude +
      recursive_avg_factor_ * contentMetrics->motion_magnitude;
}
}  // namespace
