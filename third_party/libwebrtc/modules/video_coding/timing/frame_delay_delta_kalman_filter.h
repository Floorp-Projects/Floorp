/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_FRAME_DELAY_DELTA_KALMAN_FILTER_H_
#define MODULES_VIDEO_CODING_TIMING_FRAME_DELAY_DELTA_KALMAN_FILTER_H_

#include "api/units/data_size.h"
#include "api/units/time_delta.h"

namespace webrtc {

class FrameDelayDeltaKalmanFilter {
 public:
  FrameDelayDeltaKalmanFilter();
  ~FrameDelayDeltaKalmanFilter() = default;

  // Resets the estimate to the initial state.
  void Reset();

  // Updates the Kalman filter for the line describing the frame size dependent
  // jitter.
  //
  // Input:
  //          - frame_delay
  //              Delay-delta calculated by UTILDelayEstimate.
  //          - delta_frame_size_bytes
  //              Frame size delta, i.e. frame size at time T minus frame size
  //              at time T-1. (May be negative!)
  //          - max_frame_size
  //              Filtered version of the largest frame size received.
  //          - var_noise
  //              Variance of the estimated random jitter.
  void KalmanEstimateChannel(TimeDelta frame_delay,
                             double delta_frame_size_bytes,
                             DataSize max_frame_size,
                             double var_noise);

  // Calculates the difference in delay between a sample and the expected delay
  // estimated by the Kalman filter.
  //
  // Input:
  //          - frame_delay       : Delay-delta calculated by UTILDelayEstimate.
  //          - delta_frame_size_bytes : Frame size delta, i.e. frame size at
  //                                     time T minus frame size at time T-1.
  //
  // Return value               : The delay difference in ms.
  double DeviationFromExpectedDelay(TimeDelta frame_delay,
                                    double delta_frame_size_bytes) const;

  // Returns the estimated slope.
  double GetSlope() const;

 private:
  double theta_[2];         // Estimated line parameters (slope, offset)
  double theta_cov_[2][2];  // Estimate covariance
  double q_cov_[2][2];      // Process noise covariance
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_TIMING_FRAME_DELAY_DELTA_KALMAN_FILTER_H_
