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

const int QualityScaler::kDefaultLowQpDenominator = 3;
// Note that this is the same for width and height to permit 120x90 in both
// portrait and landscape mode.
const int QualityScaler::kDefaultMinDownscaleDimension = 90;

QualityScaler::QualityScaler()
    : num_samples_(0),
      low_qp_threshold_(-1),
      downscale_shift_(0),
      framerate_down_(false),
      min_width_(kDefaultMinDownscaleDimension),
      min_height_(kDefaultMinDownscaleDimension) {}

void QualityScaler::Init(int low_qp_threshold,
                         int high_qp_threshold,
                         bool use_framerate_reduction) {
  ClearSamples();
  low_qp_threshold_ = low_qp_threshold;
  high_qp_threshold_ = high_qp_threshold;
  use_framerate_reduction_ = use_framerate_reduction;
  target_framerate_ = -1;
}

void QualityScaler::SetMinResolution(int min_width, int min_height) {
  min_width_ = min_width;
  min_height_ = min_height;
}

// Report framerate(fps) to estimate # of samples.
void QualityScaler::ReportFramerate(int framerate) {
  num_samples_ = static_cast<size_t>(
      kMeasureSeconds * (framerate < kMinFps ? kMinFps : framerate));
  framerate_ = framerate;
}

void QualityScaler::ReportQP(int qp) {
  framedrop_percent_.AddSample(0);
  average_qp_.AddSample(qp);
}

void QualityScaler::ReportDroppedFrame() {
  framedrop_percent_.AddSample(100);
}

void QualityScaler::OnEncodeFrame(const VideoFrame& frame) {
  // Should be set through InitEncode -> Should be set by now.
  assert(low_qp_threshold_ >= 0);
  assert(num_samples_ > 0);
  res_.width = frame.width();
  res_.height = frame.height();

  // Update scale factor.
  int avg_drop = 0;
  int avg_qp = 0;

  // When encoder consistently overshoots, framerate reduction and spatial
  // resizing will be triggered to get a smoother video.
  if ((framedrop_percent_.GetAverage(num_samples_, &avg_drop) &&
       avg_drop >= kFramedropPercentThreshold) ||
      (average_qp_.GetAverage(num_samples_, &avg_qp) &&
       avg_qp > high_qp_threshold_)) {
    // Reducing frame rate before spatial resolution change.
    // Reduce frame rate only when it is above a certain number.
    // Only one reduction is allowed for now.
    // TODO(jackychen): Allow more than one framerate reduction.
    if (use_framerate_reduction_ && !framerate_down_ && framerate_ >= 20) {
      target_framerate_ = framerate_ / 2;
      framerate_down_ = true;
      // If frame rate has been updated, clear the buffer. We don't want
      // spatial resolution to change right after frame rate change.
      ClearSamples();
    } else {
      AdjustScale(false);
    }
  } else if (average_qp_.GetAverage(num_samples_, &avg_qp) &&
             avg_qp <= low_qp_threshold_) {
    if (use_framerate_reduction_ && framerate_down_) {
      target_framerate_ = -1;
      framerate_down_ = false;
      ClearSamples();
    } else {
      AdjustScale(true);
    }
  }

  assert(downscale_shift_ >= 0);
  for (int shift = downscale_shift_;
       shift > 0 && (res_.width / 2 >= min_width_) &&
       (res_.height / 2 >= min_height_);
       --shift) {
    res_.width /= 2;
    res_.height /= 2;
  }
}

QualityScaler::Resolution QualityScaler::GetScaledResolution() const {
  return res_;
}

int QualityScaler::GetTargetFramerate() const {
  return target_framerate_;
}

const VideoFrame& QualityScaler::GetScaledFrame(const VideoFrame& frame) {
  Resolution res = GetScaledResolution();
  if (res.width == frame.width())
    return frame;

  scaler_.Set(frame.width(), frame.height(), res.width, res.height, kI420,
              kI420, kScaleBox);
  if (scaler_.Scale(frame, &scaled_frame_) != 0)
    return frame;

  scaled_frame_.set_ntp_time_ms(frame.ntp_time_ms());
  scaled_frame_.set_timestamp(frame.timestamp());
  scaled_frame_.set_render_time_ms(frame.render_time_ms());

  return scaled_frame_;
}

void QualityScaler::ClearSamples() {
  framedrop_percent_.Reset();
  average_qp_.Reset();
}

void QualityScaler::AdjustScale(bool up) {
  downscale_shift_ += up ? -1 : 1;
  if (downscale_shift_ < 0)
    downscale_shift_ = 0;
  ClearSamples();
}

}  // namespace webrtc
