/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/frame_delay_delta_kalman_filter.h"

#include "api/units/data_size.h"
#include "api/units/time_delta.h"

namespace webrtc {

namespace {
constexpr double kThetaLow = 0.000001;
}

FrameDelayDeltaKalmanFilter::FrameDelayDeltaKalmanFilter() {
  // TODO(brandtr): Is there a factor 1000 missing here?
  estimate_[0] = 1 / (512e3 / 8);  // Unit: [1 / bytes per ms]
  estimate_[1] = 0;                // Unit: [ms]

  // Initial estimate covariance.
  estimate_cov_[0][0] = 1e-4;  // Unit: [(1 / bytes per ms)^2]
  estimate_cov_[1][1] = 1e2;   // Unit: [ms^2]
  estimate_cov_[0][1] = estimate_cov_[1][0] = 0;

  // Process noise covariance.
  process_noise_cov_[0][0] = 2.5e-10;  // Unit: [(1 / bytes per ms)^2]
  process_noise_cov_[1][1] = 1e-10;    // Unit: [ms^2]
  // TODO(brandtr): Remove, since matrix is always diagonal. No need to multiply
  // by zero, below.
  process_noise_cov_[0][1] = process_noise_cov_[1][0] = 0;
}

void FrameDelayDeltaKalmanFilter::PredictAndUpdate(
    TimeDelta frame_delay_variation,
    double frame_size_variation_bytes,
    DataSize max_frame_size,
    double var_noise) {
  // 1) Estimate prediction: There is no need to explicitly predict the
  // estimate, since the state transition matrix is the identity.

  // 2) Estimate covariance prediction: This is done by simply adding the
  // process noise covariance, again since the state transition matrix is the
  // identity.
  estimate_cov_[0][0] += process_noise_cov_[0][0];
  estimate_cov_[0][1] += process_noise_cov_[0][1];  // TODO(brandtr): Remove.
  estimate_cov_[1][0] += process_noise_cov_[1][0];
  estimate_cov_[1][1] += process_noise_cov_[1][1];  // TODO(brandtr): Remove.

  // Measurement update.
  // TODO(brandtr): Reorganize the code below to follow the standard update
  // order as given by, e.g., Wikipedia.
  double Mh[2];
  double hMh_sigma;
  double kalmanGain[2];
  double measureRes;
  double t00, t01;

  // Kalman gain
  // K = M*h'/(sigma2n + h*M*h') = M*h'/(1 + h*M*h')
  // h = [dFS 1]
  // Mh = M*h'
  // hMh_sigma = h*M*h' + R
  Mh[0] =
      estimate_cov_[0][0] * frame_size_variation_bytes + estimate_cov_[0][1];
  Mh[1] =
      estimate_cov_[1][0] * frame_size_variation_bytes + estimate_cov_[1][1];
  // sigma weights measurements with a small deltaFS as noisy and
  // measurements with large deltaFS as good
  if (max_frame_size < DataSize::Bytes(1)) {
    return;
  }
  double sigma = (300.0 * exp(-fabs(frame_size_variation_bytes) /
                              (1e0 * max_frame_size.bytes())) +
                  1) *
                 sqrt(var_noise);
  if (sigma < 1.0) {
    sigma = 1.0;
  }
  // TODO(brandtr): Shouldn't we add sigma^2 here? Otherwise, the dimensional
  // analysis fails.
  hMh_sigma = frame_size_variation_bytes * Mh[0] + Mh[1] + sigma;
  if ((hMh_sigma < 1e-9 && hMh_sigma >= 0) ||
      (hMh_sigma > -1e-9 && hMh_sigma <= 0)) {
    RTC_DCHECK_NOTREACHED();
    return;
  }
  kalmanGain[0] = Mh[0] / hMh_sigma;
  kalmanGain[1] = Mh[1] / hMh_sigma;

  // Correction
  // theta = theta + K*(dT - h*theta)
  measureRes = frame_delay_variation.ms() -
               (frame_size_variation_bytes * estimate_[0] + estimate_[1]);
  estimate_[0] += kalmanGain[0] * measureRes;
  estimate_[1] += kalmanGain[1] * measureRes;

  if (estimate_[0] < kThetaLow) {
    estimate_[0] = kThetaLow;
  }

  // M = (I - K*h)*M
  t00 = estimate_cov_[0][0];
  t01 = estimate_cov_[0][1];
  estimate_cov_[0][0] = (1 - kalmanGain[0] * frame_size_variation_bytes) * t00 -
                        kalmanGain[0] * estimate_cov_[1][0];
  estimate_cov_[0][1] = (1 - kalmanGain[0] * frame_size_variation_bytes) * t01 -
                        kalmanGain[0] * estimate_cov_[1][1];
  estimate_cov_[1][0] = estimate_cov_[1][0] * (1 - kalmanGain[1]) -
                        kalmanGain[1] * frame_size_variation_bytes * t00;
  estimate_cov_[1][1] = estimate_cov_[1][1] * (1 - kalmanGain[1]) -
                        kalmanGain[1] * frame_size_variation_bytes * t01;

  // Covariance matrix, must be positive semi-definite.
  RTC_DCHECK(estimate_cov_[0][0] + estimate_cov_[1][1] >= 0 &&
             estimate_cov_[0][0] * estimate_cov_[1][1] -
                     estimate_cov_[0][1] * estimate_cov_[1][0] >=
                 0 &&
             estimate_cov_[0][0] >= 0);
}

double FrameDelayDeltaKalmanFilter::GetFrameDelayVariationEstimateSizeBased(
    double frame_size_variation_bytes) const {
  // Unit: [1 / bytes per millisecond] * [bytes] = [milliseconds].
  return estimate_[0] * frame_size_variation_bytes;
}

double FrameDelayDeltaKalmanFilter::GetFrameDelayVariationEstimateTotal(
    double frame_size_variation_bytes) const {
  double frame_transmission_delay_ms =
      GetFrameDelayVariationEstimateSizeBased(frame_size_variation_bytes);
  double link_queuing_delay_ms = estimate_[1];
  return frame_transmission_delay_ms + link_queuing_delay_ms;
}

}  // namespace webrtc
