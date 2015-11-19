/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "webrtc/modules/video_processing/main/source/video_processing_impl.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"

#include <assert.h>

namespace webrtc {

namespace {
void  SetSubSampling(VideoProcessingModule::FrameStats* stats,
                     const int32_t width,
                     const int32_t height) {
  if (width * height >= 640 * 480) {
    stats->subSamplWidth = 3;
    stats->subSamplHeight = 3;
  } else if (width * height >= 352 * 288) {
    stats->subSamplWidth = 2;
    stats->subSamplHeight = 2;
  } else if (width * height >= 176 * 144) {
    stats->subSamplWidth = 1;
    stats->subSamplHeight = 1;
  } else {
    stats->subSamplWidth = 0;
    stats->subSamplHeight = 0;
  }
}
}  // namespace

VideoProcessingModule* VideoProcessingModule::Create(const int32_t id) {
  return new VideoProcessingModuleImpl(id);
}

void VideoProcessingModule::Destroy(VideoProcessingModule* module) {
  if (module)
    delete static_cast<VideoProcessingModuleImpl*>(module);
}

VideoProcessingModuleImpl::VideoProcessingModuleImpl(const int32_t id)
    : mutex_(*CriticalSectionWrapper::CreateCriticalSection()) {
}

VideoProcessingModuleImpl::~VideoProcessingModuleImpl() {
  delete &mutex_;
}

void VideoProcessingModuleImpl::Reset() {
  CriticalSectionScoped mutex(&mutex_);
  deflickering_.Reset();
  brightness_detection_.Reset();
  frame_pre_processor_.Reset();
}

int32_t VideoProcessingModule::GetFrameStats(FrameStats* stats,
                                             const I420VideoFrame& frame) {
  if (frame.IsZeroSize()) {
    LOG(LS_ERROR) << "Zero size frame.";
    return VPM_PARAMETER_ERROR;
  }

  int width = frame.width();
  int height = frame.height();

  ClearFrameStats(stats);  // The histogram needs to be zeroed out.
  SetSubSampling(stats, width, height);

  const uint8_t* buffer = frame.buffer(kYPlane);
  // Compute histogram and sum of frame
  for (int i = 0; i < height; i += (1 << stats->subSamplHeight)) {
    int k = i * width;
    for (int j = 0; j < width; j += (1 << stats->subSamplWidth)) {
      stats->hist[buffer[k + j]]++;
      stats->sum += buffer[k + j];
    }
  }

  stats->num_pixels = (width * height) / ((1 << stats->subSamplWidth) *
                     (1 << stats->subSamplHeight));
  assert(stats->num_pixels > 0);

  // Compute mean value of frame
  stats->mean = stats->sum / stats->num_pixels;

  return VPM_OK;
}

bool VideoProcessingModule::ValidFrameStats(const FrameStats& stats) {
  if (stats.num_pixels == 0) {
    LOG(LS_WARNING) << "Invalid frame stats.";
    return false;
  }
  return true;
}

void VideoProcessingModule::ClearFrameStats(FrameStats* stats) {
  stats->mean = 0;
  stats->sum = 0;
  stats->num_pixels = 0;
  stats->subSamplWidth = 0;
  stats->subSamplHeight = 0;
  memset(stats->hist, 0, sizeof(stats->hist));
}

int32_t VideoProcessingModule::ColorEnhancement(I420VideoFrame* frame) {
  return VideoProcessing::ColorEnhancement(frame);
}

int32_t VideoProcessingModule::Brighten(I420VideoFrame* frame, int delta) {
  return VideoProcessing::Brighten(frame, delta);
}

int32_t VideoProcessingModuleImpl::Deflickering(I420VideoFrame* frame,
                                                FrameStats* stats) {
  CriticalSectionScoped mutex(&mutex_);
  return deflickering_.ProcessFrame(frame, stats);
}

int32_t VideoProcessingModuleImpl::BrightnessDetection(
  const I420VideoFrame& frame,
  const FrameStats& stats) {
  CriticalSectionScoped mutex(&mutex_);
  return brightness_detection_.ProcessFrame(frame, stats);
}


void VideoProcessingModuleImpl::EnableTemporalDecimation(bool enable) {
  CriticalSectionScoped mutex(&mutex_);
  frame_pre_processor_.EnableTemporalDecimation(enable);
}


void VideoProcessingModuleImpl::SetInputFrameResampleMode(VideoFrameResampling
                                                          resampling_mode) {
  CriticalSectionScoped cs(&mutex_);
  frame_pre_processor_.SetInputFrameResampleMode(resampling_mode);
}

int32_t VideoProcessingModuleImpl::SetTargetResolution(uint32_t width,
                                                       uint32_t height,
                                                       uint32_t frame_rate) {
  CriticalSectionScoped cs(&mutex_);
  return frame_pre_processor_.SetTargetResolution(width, height, frame_rate);
}

uint32_t VideoProcessingModuleImpl::Decimatedframe_rate() {
  CriticalSectionScoped cs(&mutex_);
  return  frame_pre_processor_.Decimatedframe_rate();
}

uint32_t VideoProcessingModuleImpl::DecimatedWidth() const {
  CriticalSectionScoped cs(&mutex_);
  return frame_pre_processor_.DecimatedWidth();
}

uint32_t VideoProcessingModuleImpl::DecimatedHeight() const {
  CriticalSectionScoped cs(&mutex_);
  return frame_pre_processor_.DecimatedHeight();
}

int32_t VideoProcessingModuleImpl::PreprocessFrame(
    const I420VideoFrame& frame,
    I420VideoFrame **processed_frame) {
  CriticalSectionScoped mutex(&mutex_);
  return frame_pre_processor_.PreprocessFrame(frame, processed_frame);
}

VideoContentMetrics* VideoProcessingModuleImpl::ContentMetrics() const {
  CriticalSectionScoped mutex(&mutex_);
  return frame_pre_processor_.ContentMetrics();
}

void VideoProcessingModuleImpl::EnableContentAnalysis(bool enable) {
  CriticalSectionScoped mutex(&mutex_);
  frame_pre_processor_.EnableContentAnalysis(enable);
}

}  // namespace webrtc
