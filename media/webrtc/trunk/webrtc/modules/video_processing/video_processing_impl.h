/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_PROCESSING_IMPL_H_
#define WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_PROCESSING_IMPL_H_

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/modules/video_processing/brightness_detection.h"
#include "webrtc/modules/video_processing/deflickering.h"
#include "webrtc/modules/video_processing/frame_preprocessor.h"

namespace webrtc {
class CriticalSectionWrapper;

class VideoProcessingImpl : public VideoProcessing {
 public:
  VideoProcessingImpl();
  ~VideoProcessingImpl() override;

  // Implements VideoProcessing.
  int32_t Deflickering(VideoFrame* frame, FrameStats* stats) override;
  int32_t BrightnessDetection(const VideoFrame& frame,
                              const FrameStats& stats) override;
  void EnableTemporalDecimation(bool enable) override;
  void SetInputFrameResampleMode(VideoFrameResampling resampling_mode) override;
  void EnableContentAnalysis(bool enable) override;
  int32_t SetTargetResolution(uint32_t width,
                              uint32_t height,
                              uint32_t frame_rate) override;
  void SetTargetFramerate(int frame_rate) override;
  uint32_t GetDecimatedFrameRate() override;
  uint32_t GetDecimatedWidth() const override;
  uint32_t GetDecimatedHeight() const override;
  void EnableDenosing(bool enable) override;
  const VideoFrame* PreprocessFrame(const VideoFrame& frame) override;
  VideoContentMetrics* GetContentMetrics() const override;

 private:
  mutable rtc::CriticalSection mutex_;
  VPMDeflickering deflickering_ GUARDED_BY(mutex_);
  VPMBrightnessDetection brightness_detection_;
  VPMFramePreprocessor frame_pre_processor_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_PROCESSING_VIDEO_PROCESSING_IMPL_H_
