/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/video_processing/content_analysis.h"

#include <math.h>
#include <stdlib.h>

#include "webrtc/system_wrappers/include/cpu_features_wrapper.h"
#include "webrtc/system_wrappers/include/tick_util.h"

namespace webrtc {

VPMContentAnalysis::VPMContentAnalysis(bool runtime_cpu_detection)
    : orig_frame_(NULL),
      prev_frame_(NULL),
      width_(0),
      height_(0),
      skip_num_(1),
      border_(8),
      motion_magnitude_(0.0f),
      spatial_pred_err_(0.0f),
      spatial_pred_err_h_(0.0f),
      spatial_pred_err_v_(0.0f),
      first_frame_(true),
      ca_Init_(false),
      content_metrics_(NULL) {
  ComputeSpatialMetrics = &VPMContentAnalysis::ComputeSpatialMetrics_C;
  TemporalDiffMetric = &VPMContentAnalysis::TemporalDiffMetric_C;

  if (runtime_cpu_detection) {
#if defined(WEBRTC_ARCH_X86_FAMILY)
    if (WebRtc_GetCPUInfo(kSSE2)) {
      ComputeSpatialMetrics = &VPMContentAnalysis::ComputeSpatialMetrics_SSE2;
      TemporalDiffMetric = &VPMContentAnalysis::TemporalDiffMetric_SSE2;
    }
#endif
  }
  Release();
}

VPMContentAnalysis::~VPMContentAnalysis() {
  Release();
}

VideoContentMetrics* VPMContentAnalysis::ComputeContentMetrics(
    const VideoFrame& inputFrame) {
  if (inputFrame.IsZeroSize())
    return NULL;

  // Init if needed (native dimension change).
  if (width_ != inputFrame.width() || height_ != inputFrame.height()) {
    if (VPM_OK != Initialize(inputFrame.width(), inputFrame.height()))
      return NULL;
  }
  // Only interested in the Y plane.
  orig_frame_ = inputFrame.buffer(kYPlane);

  // Compute spatial metrics: 3 spatial prediction errors.
  (this->*ComputeSpatialMetrics)();

  // Compute motion metrics
  if (first_frame_ == false)
    ComputeMotionMetrics();

  // Saving current frame as previous one: Y only.
  memcpy(prev_frame_, orig_frame_, width_ * height_);

  first_frame_ = false;
  ca_Init_ = true;

  return ContentMetrics();
}

int32_t VPMContentAnalysis::Release() {
  if (content_metrics_ != NULL) {
    delete content_metrics_;
    content_metrics_ = NULL;
  }

  if (prev_frame_ != NULL) {
    delete[] prev_frame_;
    prev_frame_ = NULL;
  }

  width_ = 0;
  height_ = 0;
  first_frame_ = true;

  return VPM_OK;
}

int32_t VPMContentAnalysis::Initialize(int width, int height) {
  width_ = width;
  height_ = height;
  first_frame_ = true;

  // skip parameter: # of skipped rows: for complexity reduction
  //  temporal also currently uses it for column reduction.
  skip_num_ = 1;

  // use skipNum = 2 for 4CIF, WHD
  if ((height_ >= 576) && (width_ >= 704)) {
    skip_num_ = 2;
  }
  // use skipNum = 4 for FULLL_HD images
  if ((height_ >= 1080) && (width_ >= 1920)) {
    skip_num_ = 4;
  }

  if (content_metrics_ != NULL) {
    delete content_metrics_;
  }

  if (prev_frame_ != NULL) {
    delete[] prev_frame_;
  }

  // Spatial Metrics don't work on a border of 8. Minimum processing
  // block size is 16 pixels.  So make sure the width and height support this.
  if (width_ <= 32 || height_ <= 32) {
    ca_Init_ = false;
    return VPM_PARAMETER_ERROR;
  }

  content_metrics_ = new VideoContentMetrics();
  if (content_metrics_ == NULL) {
    return VPM_MEMORY;
  }

  prev_frame_ = new uint8_t[width_ * height_];  // Y only.
  if (prev_frame_ == NULL)
    return VPM_MEMORY;

  return VPM_OK;
}

// Compute motion metrics: magnitude over non-zero motion vectors,
//  and size of zero cluster
int32_t VPMContentAnalysis::ComputeMotionMetrics() {
  // Motion metrics: only one is derived from normalized
  //  (MAD) temporal difference
  (this->*TemporalDiffMetric)();
  return VPM_OK;
}

// Normalized temporal difference (MAD): used as a motion level metric
// Normalize MAD by spatial contrast: images with more contrast
//  (pixel variance) likely have larger temporal difference
// To reduce complexity, we compute the metric for a reduced set of points.
int32_t VPMContentAnalysis::TemporalDiffMetric_C() {
  // size of original frame
  int sizei = height_;
  int sizej = width_;
  uint32_t tempDiffSum = 0;
  uint32_t pixelSum = 0;
  uint64_t pixelSqSum = 0;

  uint32_t num_pixels = 0;  // Counter for # of pixels.
  const int width_end = ((width_ - 2 * border_) & -16) + border_;

  for (int i = border_; i < sizei - border_; i += skip_num_) {
    for (int j = border_; j < width_end; j++) {
      num_pixels += 1;
      int ssn = i * sizej + j;

      uint8_t currPixel = orig_frame_[ssn];
      uint8_t prevPixel = prev_frame_[ssn];

      tempDiffSum +=
          static_cast<uint32_t>(abs((int16_t)(currPixel - prevPixel)));
      pixelSum += static_cast<uint32_t>(currPixel);
      pixelSqSum += static_cast<uint64_t>(currPixel * currPixel);
    }
  }

  // Default.
  motion_magnitude_ = 0.0f;

  if (tempDiffSum == 0)
    return VPM_OK;

  // Normalize over all pixels.
  float const tempDiffAvg =
      static_cast<float>(tempDiffSum) / static_cast<float>(num_pixels);
  float const pixelSumAvg =
      static_cast<float>(pixelSum) / static_cast<float>(num_pixels);
  float const pixelSqSumAvg =
      static_cast<float>(pixelSqSum) / static_cast<float>(num_pixels);
  float contrast = pixelSqSumAvg - (pixelSumAvg * pixelSumAvg);

  if (contrast > 0.0) {
    contrast = sqrt(contrast);
    motion_magnitude_ = tempDiffAvg / contrast;
  }
  return VPM_OK;
}

// Compute spatial metrics:
// To reduce complexity, we compute the metric for a reduced set of points.
// The spatial metrics are rough estimates of the prediction error cost for
//  each QM spatial mode: 2x2,1x2,2x1
// The metrics are a simple estimate of the up-sampling prediction error,
// estimated assuming sub-sampling for decimation (no filtering),
// and up-sampling back up with simple bilinear interpolation.
int32_t VPMContentAnalysis::ComputeSpatialMetrics_C() {
  const int sizei = height_;
  const int sizej = width_;

  // Pixel mean square average: used to normalize the spatial metrics.
  uint32_t pixelMSA = 0;

  uint32_t spatialErrSum = 0;
  uint32_t spatialErrVSum = 0;
  uint32_t spatialErrHSum = 0;

  // make sure work section is a multiple of 16
  const int width_end = ((sizej - 2 * border_) & -16) + border_;

  for (int i = border_; i < sizei - border_; i += skip_num_) {
    for (int j = border_; j < width_end; j++) {
      int ssn1 = i * sizej + j;
      int ssn2 = (i + 1) * sizej + j;  // bottom
      int ssn3 = (i - 1) * sizej + j;  // top
      int ssn4 = i * sizej + j + 1;    // right
      int ssn5 = i * sizej + j - 1;    // left

      uint16_t refPixel1 = orig_frame_[ssn1] << 1;
      uint16_t refPixel2 = orig_frame_[ssn1] << 2;

      uint8_t bottPixel = orig_frame_[ssn2];
      uint8_t topPixel = orig_frame_[ssn3];
      uint8_t rightPixel = orig_frame_[ssn4];
      uint8_t leftPixel = orig_frame_[ssn5];

      spatialErrSum += static_cast<uint32_t>(abs(static_cast<int16_t>(
          refPixel2 - static_cast<uint16_t>(bottPixel + topPixel + leftPixel +
                                            rightPixel))));
      spatialErrVSum += static_cast<uint32_t>(abs(static_cast<int16_t>(
          refPixel1 - static_cast<uint16_t>(bottPixel + topPixel))));
      spatialErrHSum += static_cast<uint32_t>(abs(static_cast<int16_t>(
          refPixel1 - static_cast<uint16_t>(leftPixel + rightPixel))));
      pixelMSA += orig_frame_[ssn1];
    }
  }

  // Normalize over all pixels.
  const float spatialErr = static_cast<float>(spatialErrSum >> 2);
  const float spatialErrH = static_cast<float>(spatialErrHSum >> 1);
  const float spatialErrV = static_cast<float>(spatialErrVSum >> 1);
  const float norm = static_cast<float>(pixelMSA);

  // 2X2:
  spatial_pred_err_ = spatialErr / norm;
  // 1X2:
  spatial_pred_err_h_ = spatialErrH / norm;
  // 2X1:
  spatial_pred_err_v_ = spatialErrV / norm;
  return VPM_OK;
}

VideoContentMetrics* VPMContentAnalysis::ContentMetrics() {
  if (ca_Init_ == false)
    return NULL;

  content_metrics_->spatial_pred_err = spatial_pred_err_;
  content_metrics_->spatial_pred_err_h = spatial_pred_err_h_;
  content_metrics_->spatial_pred_err_v = spatial_pred_err_v_;
  // Motion metric: normalized temporal difference (MAD).
  content_metrics_->motion_magnitude = motion_magnitude_;

  return content_metrics_;
}

}  // namespace webrtc
