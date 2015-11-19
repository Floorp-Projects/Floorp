/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULE_VIDEO_PROCESSING_IMPL_H
#define WEBRTC_MODULE_VIDEO_PROCESSING_IMPL_H

#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_processing/main/source/brighten.h"
#include "webrtc/modules/video_processing/main/source/brightness_detection.h"
#include "webrtc/modules/video_processing/main/source/color_enhancement.h"
#include "webrtc/modules/video_processing/main/source/deflickering.h"
#include "webrtc/modules/video_processing/main/source/frame_preprocessor.h"

namespace webrtc {
class CriticalSectionWrapper;

class VideoProcessingModuleImpl : public VideoProcessingModule {
 public:
  VideoProcessingModuleImpl(int32_t id);

  virtual ~VideoProcessingModuleImpl();

  void Reset() override;

  int32_t Deflickering(I420VideoFrame* frame, FrameStats* stats) override;

  int32_t BrightnessDetection(const I420VideoFrame& frame,
                              const FrameStats& stats) override;

  // Frame pre-processor functions

  // Enable temporal decimation
  void EnableTemporalDecimation(bool enable) override;

  void SetInputFrameResampleMode(VideoFrameResampling resampling_mode) override;

  // Enable content analysis
  void EnableContentAnalysis(bool enable) override;

  // Set Target Resolution: frame rate and dimension
  int32_t SetTargetResolution(uint32_t width,
                              uint32_t height,
                              uint32_t frame_rate) override;

  // Get decimated values: frame rate/dimension
  uint32_t Decimatedframe_rate() override;
  uint32_t DecimatedWidth() const override;
  uint32_t DecimatedHeight() const override;

  // Preprocess:
  // Pre-process incoming frame: Sample when needed and compute content
  // metrics when enabled.
  // If no resampling takes place - processed_frame is set to NULL.
  int32_t PreprocessFrame(const I420VideoFrame& frame,
                          I420VideoFrame** processed_frame) override;
  VideoContentMetrics* ContentMetrics() const override;

 private:
  CriticalSectionWrapper& mutex_;
  VPMDeflickering deflickering_;
  VPMBrightnessDetection brightness_detection_;
  VPMFramePreprocessor frame_pre_processor_;
};

}  // namespace

#endif
