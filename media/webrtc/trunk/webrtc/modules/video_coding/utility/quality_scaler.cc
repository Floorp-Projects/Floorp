/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/quality_scaler.h"

namespace webrtc {

static const int kMinFps = 10;
static const int kMeasureSeconds = 5;
static const int kFramedropPercentThreshold = 60;
static const int kLowQpThresholdDenominator = 3;

QualityScaler::QualityScaler()
    : num_samples_(0), low_qp_threshold_(-1), downscale_shift_(0) {
}

void QualityScaler::Init(int max_qp) {
  ClearSamples();
  downscale_shift_ = 0;
  low_qp_threshold_ = max_qp / kLowQpThresholdDenominator ;
}

void QualityScaler::ReportFramerate(int framerate) {
  num_samples_ = static_cast<size_t>(
      kMeasureSeconds * (framerate < kMinFps ? kMinFps : framerate));
}

void QualityScaler::ReportEncodedFrame(int qp) {
  average_qp_.AddSample(qp);
  framedrop_percent_.AddSample(0);
}

void QualityScaler::ReportDroppedFrame() {
  framedrop_percent_.AddSample(100);
}

QualityScaler::Resolution QualityScaler::GetScaledResolution(
    const I420VideoFrame& frame) {
  // Both of these should be set through InitEncode -> Should be set by now.
  assert(low_qp_threshold_ >= 0);
  assert(num_samples_ > 0);
  // Update scale factor.
  int avg;
  if (framedrop_percent_.GetAverage(num_samples_, &avg) &&
      avg >= kFramedropPercentThreshold) {
    AdjustScale(false);
  } else if (average_qp_.GetAverage(num_samples_, &avg) &&
             avg <= low_qp_threshold_) {
    AdjustScale(true);
  }

  Resolution res;
  res.width = frame.width();
  res.height = frame.height();

  assert(downscale_shift_ >= 0);
  for (int shift = downscale_shift_;
       shift > 0 && res.width > 1 && res.height > 1;
       --shift) {
    res.width >>= 1;
    res.height >>= 1;
  }

  return res;
}

const I420VideoFrame& QualityScaler::GetScaledFrame(
    const I420VideoFrame& frame) {
  Resolution res = GetScaledResolution(frame);
  if (res.width == frame.width())
    return frame;

  scaler_.Set(frame.width(),
              frame.height(),
              res.width,
              res.height,
              kI420,
              kI420,
              kScaleBox);
  if (scaler_.Scale(frame, &scaled_frame_) != 0)
    return frame;

  scaled_frame_.set_ntp_time_ms(frame.ntp_time_ms());
  scaled_frame_.set_timestamp(frame.timestamp());
  scaled_frame_.set_render_time_ms(frame.render_time_ms());

  return scaled_frame_;
}

QualityScaler::MovingAverage::MovingAverage() : sum_(0) {
}

void QualityScaler::MovingAverage::AddSample(int sample) {
  samples_.push_back(sample);
  sum_ += sample;
}

bool QualityScaler::MovingAverage::GetAverage(size_t num_samples, int* avg) {
  assert(num_samples > 0);
  if (num_samples > samples_.size())
    return false;

  // Remove old samples.
  while (num_samples < samples_.size()) {
    sum_ -= samples_.front();
    samples_.pop_front();
  }

  *avg = sum_ / static_cast<int>(num_samples);
  return true;
}

void QualityScaler::MovingAverage::Reset() {
  sum_ = 0;
  samples_.clear();
}

void QualityScaler::ClearSamples() {
  average_qp_.Reset();
  framedrop_percent_.Reset();
}

void QualityScaler::AdjustScale(bool up) {
  downscale_shift_ += up ? -1 : 1;
  if (downscale_shift_ < 0)
    downscale_shift_ = 0;
  ClearSamples();
}

}  // namespace webrtc
