/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/video_processing_impl.h"

#include <assert.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

namespace {

int GetSubSamplingFactor(int width, int height) {
  if (width * height >= 640 * 480) {
    return 3;
  } else if (width * height >= 352 * 288) {
    return 2;
  } else if (width * height >= 176 * 144) {
    return 1;
  } else {
    return 0;
  }
}
}  // namespace

VideoProcessing* VideoProcessing::Create() {
  return new VideoProcessingImpl();
}

VideoProcessingImpl::VideoProcessingImpl() {}
VideoProcessingImpl::~VideoProcessingImpl() {}

void VideoProcessing::GetFrameStats(const VideoFrame& frame,
                                    FrameStats* stats) {
  ClearFrameStats(stats);  // The histogram needs to be zeroed out.
  if (frame.IsZeroSize()) {
    return;
  }

  int width = frame.width();
  int height = frame.height();
  stats->sub_sampling_factor = GetSubSamplingFactor(width, height);

  const uint8_t* buffer = frame.buffer(kYPlane);
  // Compute histogram and sum of frame
  for (int i = 0; i < height; i += (1 << stats->sub_sampling_factor)) {
    int k = i * width;
    for (int j = 0; j < width; j += (1 << stats->sub_sampling_factor)) {
      stats->hist[buffer[k + j]]++;
      stats->sum += buffer[k + j];
    }
  }

  stats->num_pixels = (width * height) / ((1 << stats->sub_sampling_factor) *
                                          (1 << stats->sub_sampling_factor));
  assert(stats->num_pixels > 0);

  // Compute mean value of frame
  stats->mean = stats->sum / stats->num_pixels;
}

bool VideoProcessing::ValidFrameStats(const FrameStats& stats) {
  if (stats.num_pixels == 0) {
    LOG(LS_WARNING) << "Invalid frame stats.";
    return false;
  }
  return true;
}

void VideoProcessing::ClearFrameStats(FrameStats* stats) {
  stats->mean = 0;
  stats->sum = 0;
  stats->num_pixels = 0;
  stats->sub_sampling_factor = 0;
  memset(stats->hist, 0, sizeof(stats->hist));
}

void VideoProcessing::Brighten(int delta, VideoFrame* frame) {
  RTC_DCHECK(!frame->IsZeroSize());
  RTC_DCHECK(frame->width() > 0);
  RTC_DCHECK(frame->height() > 0);

  int num_pixels = frame->width() * frame->height();

  int look_up[256];
  for (int i = 0; i < 256; i++) {
    int val = i + delta;
    look_up[i] = ((((val < 0) ? 0 : val) > 255) ? 255 : val);
  }

  uint8_t* temp_ptr = frame->buffer(kYPlane);
  for (int i = 0; i < num_pixels; i++) {
    *temp_ptr = static_cast<uint8_t>(look_up[*temp_ptr]);
    temp_ptr++;
  }
}

int32_t VideoProcessingImpl::Deflickering(VideoFrame* frame,
                                          FrameStats* stats) {
  rtc::CritScope mutex(&mutex_);
  return deflickering_.ProcessFrame(frame, stats);
}

int32_t VideoProcessingImpl::BrightnessDetection(const VideoFrame& frame,
                                                 const FrameStats& stats) {
  rtc::CritScope mutex(&mutex_);
  return brightness_detection_.ProcessFrame(frame, stats);
}

void VideoProcessingImpl::EnableTemporalDecimation(bool enable) {
  rtc::CritScope mutex(&mutex_);
  frame_pre_processor_.EnableTemporalDecimation(enable);
}

void VideoProcessingImpl::SetInputFrameResampleMode(
    VideoFrameResampling resampling_mode) {
  rtc::CritScope cs(&mutex_);
  frame_pre_processor_.SetInputFrameResampleMode(resampling_mode);
}

int32_t VideoProcessingImpl::SetTargetResolution(uint32_t width,
                                                 uint32_t height,
                                                 uint32_t frame_rate) {
  rtc::CritScope cs(&mutex_);
  return frame_pre_processor_.SetTargetResolution(width, height, frame_rate);
}

void VideoProcessingImpl::SetTargetFramerate(int frame_rate) {
  rtc::CritScope cs(&mutex_);
  frame_pre_processor_.SetTargetFramerate(frame_rate);
}

uint32_t VideoProcessingImpl::GetDecimatedFrameRate() {
  rtc::CritScope cs(&mutex_);
  return frame_pre_processor_.GetDecimatedFrameRate();
}

uint32_t VideoProcessingImpl::GetDecimatedWidth() const {
  rtc::CritScope cs(&mutex_);
  return frame_pre_processor_.GetDecimatedWidth();
}

uint32_t VideoProcessingImpl::GetDecimatedHeight() const {
  rtc::CritScope cs(&mutex_);
  return frame_pre_processor_.GetDecimatedHeight();
}

void VideoProcessingImpl::EnableDenosing(bool enable) {
  rtc::CritScope cs(&mutex_);
  frame_pre_processor_.EnableDenosing(enable);
}

const VideoFrame* VideoProcessingImpl::PreprocessFrame(
    const VideoFrame& frame) {
  rtc::CritScope mutex(&mutex_);
  return frame_pre_processor_.PreprocessFrame(frame);
}

VideoContentMetrics* VideoProcessingImpl::GetContentMetrics() const {
  rtc::CritScope mutex(&mutex_);
  return frame_pre_processor_.GetContentMetrics();
}

void VideoProcessingImpl::EnableContentAnalysis(bool enable) {
  rtc::CritScope mutex(&mutex_);
  frame_pre_processor_.EnableContentAnalysis(enable);
}

}  // namespace webrtc
