/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_MAIN_SOURCE_CONTENT_ANALYSIS_H
#define WEBRTC_MODULES_VIDEO_PROCESSING_MAIN_SOURCE_CONTENT_ANALYSIS_H

#include "webrtc/common_video/interface/i420_video_frame.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_processing/main/interface/video_processing_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class VPMContentAnalysis {
 public:
  // When |runtime_cpu_detection| is true, runtime selection of an optimized
  // code path is allowed.
  explicit VPMContentAnalysis(bool runtime_cpu_detection);
  ~VPMContentAnalysis();

  // Initialize ContentAnalysis - should be called prior to
  //  extractContentFeature
  // Inputs:         width, height
  // Return value:   0 if OK, negative value upon error
  int32_t Initialize(int width, int height);

  // Extract content Feature - main function of ContentAnalysis
  // Input:           new frame
  // Return value:    pointer to structure containing content Analysis
  //                  metrics or NULL value upon error
  VideoContentMetrics* ComputeContentMetrics(const I420VideoFrame&
                                             inputFrame);

  // Release all allocated memory
  // Output: 0 if OK, negative value upon error
  int32_t Release();

 private:
  // return motion metrics
  VideoContentMetrics* ContentMetrics();

  // Normalized temporal difference metric: for motion magnitude
  typedef int32_t (VPMContentAnalysis::*TemporalDiffMetricFunc)();
  TemporalDiffMetricFunc TemporalDiffMetric;
  int32_t TemporalDiffMetric_C();

  // Motion metric method: call 2 metrics (magnitude and size)
  int32_t ComputeMotionMetrics();

  // Spatial metric method: computes the 3 frame-average spatial
  //  prediction errors (1x2,2x1,2x2)
  typedef int32_t (VPMContentAnalysis::*ComputeSpatialMetricsFunc)();
  ComputeSpatialMetricsFunc ComputeSpatialMetrics;
  int32_t ComputeSpatialMetrics_C();

#if defined(WEBRTC_ARCH_X86_FAMILY)
  int32_t ComputeSpatialMetrics_SSE2();
  int32_t TemporalDiffMetric_SSE2();
#endif

  const uint8_t* orig_frame_;
  uint8_t* prev_frame_;
  int width_;
  int height_;
  int skip_num_;
  int border_;

  // Content Metrics: Stores the local average of the metrics.
  float motion_magnitude_;   // motion class
  float spatial_pred_err_;   // spatial class
  float spatial_pred_err_h_;  // spatial class
  float spatial_pred_err_v_;  // spatial class
  bool first_frame_;
  bool ca_Init_;

  VideoContentMetrics*   content_metrics_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_MAIN_SOURCE_CONTENT_ANALYSIS_H
