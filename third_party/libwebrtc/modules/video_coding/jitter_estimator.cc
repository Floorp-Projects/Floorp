/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/jitter_estimator.h"

#include <math.h>
#include <string.h>

#include <algorithm>
#include <cstdint>

#include "absl/types/optional.h"
#include "api/units/data_size.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/webrtc_key_value_config.h"
#include "modules/video_coding/rtt_filter.h"
#include "rtc_base/experiments/jitter_upper_bound_experiment.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {
static constexpr uint32_t kStartupDelaySamples = 30;
static constexpr int64_t kFsAccuStartupSamples = 5;
static constexpr Frequency kMaxFramerateEstimate = Frequency::Hertz(200);
static constexpr TimeDelta kNackCountTimeout = TimeDelta::Seconds(60);
static constexpr double kDefaultMaxTimestampDeviationInSigmas = 3.5;

constexpr double kPhi = 0.97;
constexpr double kPsi = 0.9999;
constexpr uint32_t kAlphaCountMax = 400;
constexpr double kThetaLow = 0.000001;
constexpr uint32_t kNackLimit = 3;
constexpr int32_t kNumStdDevDelayOutlier = 15;
constexpr int32_t kNumStdDevFrameSizeOutlier = 3;
// ~Less than 1% chance (look up in normal distribution table)...
constexpr double kNoiseStdDevs = 2.33;
// ...of getting 30 ms freezes
constexpr double kNoiseStdDevOffset = 30.0;

}  // namespace

VCMJitterEstimator::VCMJitterEstimator(Clock* clock,
                                       const WebRtcKeyValueConfig& field_trials)
    : fps_counter_(30),  // TODO(sprang): Use an estimator with limit based on
                         // time, rather than number of samples.
      time_deviation_upper_bound_(
          JitterUpperBoundExperiment::GetUpperBoundSigmas().value_or(
              kDefaultMaxTimestampDeviationInSigmas)),
      enable_reduced_delay_(
          !field_trials.IsEnabled("WebRTC-ReducedJitterDelayKillSwitch")),
      clock_(clock) {
  Reset();
}

VCMJitterEstimator::~VCMJitterEstimator() = default;

// Resets the JitterEstimate.
void VCMJitterEstimator::Reset() {
  theta_[0] = 1 / (512e3 / 8);
  theta_[1] = 0;
  var_noise_ = 4.0;

  theta_cov_[0][0] = 1e-4;
  theta_cov_[1][1] = 1e2;
  theta_cov_[0][1] = theta_cov_[1][0] = 0;
  q_cov_[0][0] = 2.5e-10;
  q_cov_[1][1] = 1e-10;
  q_cov_[0][1] = q_cov_[1][0] = 0;
  avg_frame_size_ = kDefaultAvgAndMaxFrameSize;
  max_frame_size_ = kDefaultAvgAndMaxFrameSize;
  var_frame_size_ = 100;
  last_update_time_ = absl::nullopt;
  prev_estimate_ = absl::nullopt;
  prev_frame_size_ = absl::nullopt;
  avg_noise_ = 0.0;
  alpha_count_ = 1;
  filter_jitter_estimate_ = TimeDelta::Zero();
  latest_nack_ = Timestamp::Zero();
  nack_count_ = 0;
  frame_size_sum_ = DataSize::Zero();
  frame_size_count_ = 0;
  startup_count_ = 0;
  rtt_filter_.Reset();
  fps_counter_.Reset();
}

// Updates the estimates with the new measurements.
void VCMJitterEstimator::UpdateEstimate(TimeDelta frame_delay,
                                        DataSize frame_size,
                                        bool incomplete_frame /* = false */) {
  if (frame_size.IsZero()) {
    return;
  }
  // Can't use DataSize since this can be negative.
  double delta_frame_bytes =
      frame_size.bytes() - prev_frame_size_.value_or(DataSize::Zero()).bytes();
  if (frame_size_count_ < kFsAccuStartupSamples) {
    frame_size_sum_ += frame_size;
    frame_size_count_++;
  } else if (frame_size_count_ == kFsAccuStartupSamples) {
    // Give the frame size filter.
    avg_frame_size_ = frame_size_sum_ / static_cast<double>(frame_size_count_);
    frame_size_count_++;
  }
  if (!incomplete_frame || frame_size > avg_frame_size_) {
    DataSize avg_frame_size = kPhi * avg_frame_size_ + (1 - kPhi) * frame_size;
    DataSize deviation_size = DataSize::Bytes(2 * sqrt(var_frame_size_));
    if (frame_size < avg_frame_size_ + deviation_size) {
      // Only update the average frame size if this sample wasn't a key frame.
      avg_frame_size_ = avg_frame_size;
    }
    // Update the variance anyway since we want to capture cases where we only
    // get key frames.
    double delta_bytes = frame_size.bytes() - avg_frame_size.bytes();
    var_frame_size_ = std::max(
        kPhi * var_frame_size_ + (1 - kPhi) * (delta_bytes * delta_bytes), 1.0);
  }

  // Update max_frame_size_ estimate.
  max_frame_size_ = std::max(kPsi * max_frame_size_, frame_size);

  if (!prev_frame_size_) {
    prev_frame_size_ = frame_size;
    return;
  }
  prev_frame_size_ = frame_size;

  // Cap frame_delay based on the current time deviation noise.
  TimeDelta max_time_deviation =
      TimeDelta::Millis(time_deviation_upper_bound_ * sqrt(var_noise_) + 0.5);
  frame_delay.Clamp(-max_time_deviation, max_time_deviation);

  // Only update the Kalman filter if the sample is not considered an extreme
  // outlier. Even if it is an extreme outlier from a delay point of view, if
  // the frame size also is large the deviation is probably due to an incorrect
  // line slope.
  double deviation = DeviationFromExpectedDelay(frame_delay, delta_frame_bytes);

  if (fabs(deviation) < kNumStdDevDelayOutlier * sqrt(var_noise_) ||
      frame_size.bytes() >
          avg_frame_size_.bytes() +
              kNumStdDevFrameSizeOutlier * sqrt(var_frame_size_)) {
    // Update the variance of the deviation from the line given by the Kalman
    // filter.
    EstimateRandomJitter(deviation, incomplete_frame);
    // Prevent updating with frames which have been congested by a large frame,
    // and therefore arrives almost at the same time as that frame.
    // This can occur when we receive a large frame (key frame) which has been
    // delayed. The next frame is of normal size (delta frame), and thus deltaFS
    // will be << 0. This removes all frame samples which arrives after a key
    // frame.
    if ((!incomplete_frame || deviation >= 0) &&
        delta_frame_bytes > -0.25 * max_frame_size_.bytes()) {
      // Update the Kalman filter with the new data
      KalmanEstimateChannel(frame_delay, delta_frame_bytes);
    }
  } else {
    int nStdDev =
        (deviation >= 0) ? kNumStdDevDelayOutlier : -kNumStdDevDelayOutlier;
    EstimateRandomJitter(nStdDev * sqrt(var_noise_), incomplete_frame);
  }
  // Post process the total estimated jitter
  if (startup_count_ >= kStartupDelaySamples) {
    PostProcessEstimate();
  } else {
    startup_count_++;
  }
}

// Updates the nack/packet ratio.
void VCMJitterEstimator::FrameNacked() {
  if (nack_count_ < kNackLimit) {
    nack_count_++;
  }
  latest_nack_ = clock_->CurrentTime();
}

// Updates Kalman estimate of the channel.
// The caller is expected to sanity check the inputs.
void VCMJitterEstimator::KalmanEstimateChannel(TimeDelta frame_delay,
                                               double delta_frame_size_bytes) {
  double Mh[2];
  double hMh_sigma;
  double kalmanGain[2];
  double measureRes;
  double t00, t01;

  // Kalman filtering

  // Prediction
  // M = M + Q
  theta_cov_[0][0] += q_cov_[0][0];
  theta_cov_[0][1] += q_cov_[0][1];
  theta_cov_[1][0] += q_cov_[1][0];
  theta_cov_[1][1] += q_cov_[1][1];

  // Kalman gain
  // K = M*h'/(sigma2n + h*M*h') = M*h'/(1 + h*M*h')
  // h = [dFS 1]
  // Mh = M*h'
  // hMh_sigma = h*M*h' + R
  Mh[0] = theta_cov_[0][0] * delta_frame_size_bytes + theta_cov_[0][1];
  Mh[1] = theta_cov_[1][0] * delta_frame_size_bytes + theta_cov_[1][1];
  // sigma weights measurements with a small deltaFS as noisy and
  // measurements with large deltaFS as good
  if (max_frame_size_ < DataSize::Bytes(1)) {
    return;
  }
  double sigma = (300.0 * exp(-fabs(delta_frame_size_bytes) /
                              (1e0 * max_frame_size_.bytes())) +
                  1) *
                 sqrt(var_noise_);
  if (sigma < 1.0) {
    sigma = 1.0;
  }
  hMh_sigma = delta_frame_size_bytes * Mh[0] + Mh[1] + sigma;
  if ((hMh_sigma < 1e-9 && hMh_sigma >= 0) ||
      (hMh_sigma > -1e-9 && hMh_sigma <= 0)) {
    RTC_DCHECK_NOTREACHED();
    return;
  }
  kalmanGain[0] = Mh[0] / hMh_sigma;
  kalmanGain[1] = Mh[1] / hMh_sigma;

  // Correction
  // theta = theta + K*(dT - h*theta)
  measureRes =
      frame_delay.ms() - (delta_frame_size_bytes * theta_[0] + theta_[1]);
  theta_[0] += kalmanGain[0] * measureRes;
  theta_[1] += kalmanGain[1] * measureRes;

  if (theta_[0] < kThetaLow) {
    theta_[0] = kThetaLow;
  }

  // M = (I - K*h)*M
  t00 = theta_cov_[0][0];
  t01 = theta_cov_[0][1];
  theta_cov_[0][0] = (1 - kalmanGain[0] * delta_frame_size_bytes) * t00 -
                     kalmanGain[0] * theta_cov_[1][0];
  theta_cov_[0][1] = (1 - kalmanGain[0] * delta_frame_size_bytes) * t01 -
                     kalmanGain[0] * theta_cov_[1][1];
  theta_cov_[1][0] = theta_cov_[1][0] * (1 - kalmanGain[1]) -
                     kalmanGain[1] * delta_frame_size_bytes * t00;
  theta_cov_[1][1] = theta_cov_[1][1] * (1 - kalmanGain[1]) -
                     kalmanGain[1] * delta_frame_size_bytes * t01;

  // Covariance matrix, must be positive semi-definite.
  RTC_DCHECK(theta_cov_[0][0] + theta_cov_[1][1] >= 0 &&
             theta_cov_[0][0] * theta_cov_[1][1] -
                     theta_cov_[0][1] * theta_cov_[1][0] >=
                 0 &&
             theta_cov_[0][0] >= 0);
}

// Calculate difference in delay between a sample and the expected delay
// estimated by the Kalman filter
double VCMJitterEstimator::DeviationFromExpectedDelay(
    TimeDelta frame_delay,
    double delta_frame_size_bytes) const {
  return frame_delay.ms() - (theta_[0] * delta_frame_size_bytes + theta_[1]);
}

// Estimates the random jitter by calculating the variance of the sample
// distance from the line given by theta.
void VCMJitterEstimator::EstimateRandomJitter(double d_dT,
                                              bool incomplete_frame) {
  Timestamp now = clock_->CurrentTime();
  if (last_update_time_.has_value()) {
    fps_counter_.AddSample((now - *last_update_time_).us());
  }
  last_update_time_ = now;

  if (alpha_count_ == 0) {
    RTC_DCHECK_NOTREACHED();
    return;
  }
  double alpha =
      static_cast<double>(alpha_count_ - 1) / static_cast<double>(alpha_count_);
  alpha_count_++;
  if (alpha_count_ > kAlphaCountMax)
    alpha_count_ = kAlphaCountMax;

  // In order to avoid a low frame rate stream to react slower to changes,
  // scale the alpha weight relative a 30 fps stream.
  Frequency fps = GetFrameRate();
  if (fps > Frequency::Zero()) {
    constexpr Frequency k30Fps = Frequency::Hertz(30);
    double rate_scale = k30Fps / fps;
    // At startup, there can be a lot of noise in the fps estimate.
    // Interpolate rate_scale linearly, from 1.0 at sample #1, to 30.0 / fps
    // at sample #kStartupDelaySamples.
    if (alpha_count_ < kStartupDelaySamples) {
      rate_scale =
          (alpha_count_ * rate_scale + (kStartupDelaySamples - alpha_count_)) /
          kStartupDelaySamples;
    }
    alpha = pow(alpha, rate_scale);
  }

  double avgNoise = alpha * avg_noise_ + (1 - alpha) * d_dT;
  double varNoise = alpha * var_noise_ +
                    (1 - alpha) * (d_dT - avg_noise_) * (d_dT - avg_noise_);
  if (!incomplete_frame || varNoise > var_noise_) {
    avg_noise_ = avgNoise;
    var_noise_ = varNoise;
  }
  if (var_noise_ < 1.0) {
    // The variance should never be zero, since we might get stuck and consider
    // all samples as outliers.
    var_noise_ = 1.0;
  }
}

double VCMJitterEstimator::NoiseThreshold() const {
  double noiseThreshold = kNoiseStdDevs * sqrt(var_noise_) - kNoiseStdDevOffset;
  if (noiseThreshold < 1.0) {
    noiseThreshold = 1.0;
  }
  return noiseThreshold;
}

// Calculates the current jitter estimate from the filtered estimates.
TimeDelta VCMJitterEstimator::CalculateEstimate() {
  double retMs =
      theta_[0] * (max_frame_size_.bytes() - avg_frame_size_.bytes()) +
      NoiseThreshold();

  TimeDelta ret = TimeDelta::Millis(retMs);

  constexpr TimeDelta kMinPrevEstimate = TimeDelta::Micros(10);
  constexpr TimeDelta kMaxEstimate = TimeDelta::Seconds(10);
  // A very low estimate (or negative) is neglected.
  if (ret < TimeDelta::Millis(1)) {
    if (!prev_estimate_ || prev_estimate_ <= kMinPrevEstimate) {
      ret = TimeDelta::Millis(1);
    } else {
      ret = *prev_estimate_;
    }
  }
  if (ret > kMaxEstimate) {  // Sanity
    ret = kMaxEstimate;
  }
  prev_estimate_ = ret;
  return ret;
}

void VCMJitterEstimator::PostProcessEstimate() {
  filter_jitter_estimate_ = CalculateEstimate();
}

void VCMJitterEstimator::UpdateRtt(TimeDelta rtt) {
  rtt_filter_.Update(rtt);
}

// Returns the current filtered estimate if available,
// otherwise tries to calculate an estimate.
TimeDelta VCMJitterEstimator::GetJitterEstimate(
    double rtt_multiplier,
    absl::optional<TimeDelta> rtt_mult_add_cap) {
  TimeDelta jitter = CalculateEstimate() + OPERATING_SYSTEM_JITTER;
  Timestamp now = clock_->CurrentTime();

  if (now - latest_nack_ > kNackCountTimeout)
    nack_count_ = 0;

  if (filter_jitter_estimate_ > jitter)
    jitter = filter_jitter_estimate_;
  if (nack_count_ >= kNackLimit) {
    if (rtt_mult_add_cap.has_value()) {
      jitter += std::min(rtt_filter_.Rtt() * rtt_multiplier,
                         rtt_mult_add_cap.value());
    } else {
      jitter += rtt_filter_.Rtt() * rtt_multiplier;
    }
  }

  if (enable_reduced_delay_) {
    static const Frequency kJitterScaleLowThreshold = Frequency::Hertz(5);
    static const Frequency kJitterScaleHighThreshold = Frequency::Hertz(10);
    Frequency fps = GetFrameRate();
    // Ignore jitter for very low fps streams.
    if (fps < kJitterScaleLowThreshold) {
      if (fps.IsZero()) {
        return std::max(TimeDelta::Zero(), jitter);
      }
      return TimeDelta::Zero();
    }

    // Semi-low frame rate; scale by factor linearly interpolated from 0.0 at
    // kJitterScaleLowThreshold to 1.0 at kJitterScaleHighThreshold.
    if (fps < kJitterScaleHighThreshold) {
      jitter = (1.0 / (kJitterScaleHighThreshold - kJitterScaleLowThreshold)) *
               (fps - kJitterScaleLowThreshold) * jitter;
    }
  }

  return std::max(TimeDelta::Zero(), jitter);
}

Frequency VCMJitterEstimator::GetFrameRate() const {
  TimeDelta mean_frame_period = TimeDelta::Micros(fps_counter_.ComputeMean());
  if (mean_frame_period <= TimeDelta::Zero())
    return Frequency::Zero();

  Frequency fps = 1 / mean_frame_period;
  // Sanity check.
  RTC_DCHECK_GE(fps, Frequency::Zero());
  return std::min(fps, kMaxFramerateEstimate);
}
}  // namespace webrtc
