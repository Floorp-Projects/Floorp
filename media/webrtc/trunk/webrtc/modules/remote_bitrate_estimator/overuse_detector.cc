/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdlib.h>  // fabsf

#include "webrtc/modules/remote_bitrate_estimator/overuse_detector.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_rate_control.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/trace.h"

enum { kOverUsingTimeThreshold = 100 };
enum { kMinFramePeriodHistoryLength = 60 };

namespace webrtc {
OveruseDetector::OveruseDetector(const OverUseDetectorOptions& options)
    : options_(options),
      current_frame_(),
      prev_frame_(),
      num_of_deltas_(0),
      slope_(options_.initial_slope),
      offset_(options_.initial_offset),
      E_(),
      process_noise_(),
      avg_noise_(options_.initial_avg_noise),
      var_noise_(options_.initial_var_noise),
      threshold_(options_.initial_threshold),
      ts_delta_hist_(),
      prev_offset_(0.0),
      time_over_using_(-1),
      over_use_counter_(0),
      hypothesis_(kBwNormal) {
  memcpy(E_, options_.initial_e, sizeof(E_));
  memcpy(process_noise_, options_.initial_process_noise,
         sizeof(process_noise_));
}

OveruseDetector::~OveruseDetector() {
  ts_delta_hist_.clear();
}

void OveruseDetector::Update(uint16_t packet_size,
                             int64_t timestamp_ms,
                             uint32_t timestamp,
                             const int64_t arrival_time_ms) {
  bool new_timestamp = (timestamp != current_frame_.timestamp);
  if (timestamp_ms >= 0) {
    if (prev_frame_.timestamp_ms == -1 && current_frame_.timestamp_ms == -1) {
      SwitchTimeBase();
    }
    new_timestamp = (timestamp_ms != current_frame_.timestamp_ms);
  }
  if (current_frame_.timestamp == -1) {
    // This is the first incoming packet. We don't have enough data to update
    // the filter, so we store it until we have two frames of data to process.
    current_frame_.timestamp = timestamp;
    current_frame_.timestamp_ms = timestamp_ms;
  } else if (!PacketInOrder(timestamp, timestamp_ms)) {
    return;
  } else if (new_timestamp) {
    // First packet of a later frame, the previous frame sample is ready.
    if (prev_frame_.complete_time_ms >= 0) {  // This is our second frame.
      int64_t t_delta = 0;
      double ts_delta = 0;
      TimeDeltas(current_frame_, prev_frame_, &t_delta, &ts_delta);
      UpdateKalman(t_delta, ts_delta, current_frame_.size, prev_frame_.size);
    }
    prev_frame_ = current_frame_;
    // The new timestamp is now the current frame.
    current_frame_.timestamp = timestamp;
    current_frame_.timestamp_ms = timestamp_ms;
    current_frame_.size = 0;
  }
  // Accumulate the frame size
  current_frame_.size += packet_size;
  current_frame_.complete_time_ms = arrival_time_ms;
}

BandwidthUsage OveruseDetector::State() const {
  return hypothesis_;
}

double OveruseDetector::NoiseVar() const {
  return var_noise_;
}

void OveruseDetector::SetRateControlRegion(RateControlRegion region) {
  switch (region) {
    case kRcMaxUnknown: {
      threshold_ = options_.initial_threshold;
      break;
    }
    case kRcAboveMax:
    case kRcNearMax: {
      threshold_ = options_.initial_threshold / 2;
      break;
    }
  }
}

void OveruseDetector::SwitchTimeBase() {
  current_frame_.size = 0;
  current_frame_.complete_time_ms = -1;
  current_frame_.timestamp = -1;
  prev_frame_ = current_frame_;
}

void OveruseDetector::TimeDeltas(const FrameSample& current_frame,
                                 const FrameSample& prev_frame,
                                 int64_t* t_delta,
                                 double* ts_delta) {
  assert(t_delta);
  assert(ts_delta);
  num_of_deltas_++;
  if (num_of_deltas_ > 1000) {
    num_of_deltas_ = 1000;
  }
  if (current_frame.timestamp_ms == -1) {
    uint32_t timestamp_diff = current_frame.timestamp - prev_frame.timestamp;
    *ts_delta = timestamp_diff / 90.0;
  } else {
    *ts_delta = current_frame.timestamp_ms - prev_frame.timestamp_ms;
  }
  *t_delta = current_frame.complete_time_ms - prev_frame.complete_time_ms;
  assert(*ts_delta > 0);
}

bool OveruseDetector::PacketInOrder(uint32_t timestamp, int64_t timestamp_ms) {
  if (current_frame_.timestamp_ms == -1 && current_frame_.timestamp > -1) {
    return InOrderTimestamp(timestamp, current_frame_.timestamp);
  } else if (current_frame_.timestamp_ms > 0) {
    // Using timestamps converted to NTP time.
    return timestamp_ms > current_frame_.timestamp_ms;
  }
  // This is the first packet.
  return true;
}

bool OveruseDetector::InOrderTimestamp(uint32_t timestamp,
                                       uint32_t prev_timestamp) {
  uint32_t timestamp_diff = timestamp - prev_timestamp;
  // Assume that a diff this big must be due to reordering. Don't update
  // with reordered samples.
  return (timestamp_diff < 0x80000000);
}

double OveruseDetector::CurrentDrift() {
  return 1.0;
}

void OveruseDetector::UpdateKalman(int64_t t_delta,
                                   double ts_delta,
                                   uint32_t frame_size,
                                   uint32_t prev_frame_size) {
  const double min_frame_period = UpdateMinFramePeriod(ts_delta);
  const double drift = CurrentDrift();
  // Compensate for drift
  const double t_ts_delta = t_delta - ts_delta / drift;
  double fs_delta = static_cast<double>(frame_size) - prev_frame_size;

  // Update the Kalman filter
  const double scale_factor =  min_frame_period / (1000.0 / 30.0);
  E_[0][0] += process_noise_[0] * scale_factor;
  E_[1][1] += process_noise_[1] * scale_factor;

  if ((hypothesis_ == kBwOverusing && offset_ < prev_offset_) ||
      (hypothesis_ == kBwUnderusing && offset_ > prev_offset_)) {
    E_[1][1] += 10 * process_noise_[1] * scale_factor;
  }

  const double h[2] = {fs_delta, 1.0};
  const double Eh[2] = {E_[0][0]*h[0] + E_[0][1]*h[1],
                        E_[1][0]*h[0] + E_[1][1]*h[1]};

  const double residual = t_ts_delta - slope_*h[0] - offset_;

  const bool stable_state =
      (BWE_MIN(num_of_deltas_, 60) * fabs(offset_) < threshold_);
  // We try to filter out very late frames. For instance periodic key
  // frames doesn't fit the Gaussian model well.
  if (fabs(residual) < 3 * sqrt(var_noise_)) {
    UpdateNoiseEstimate(residual, min_frame_period, stable_state);
  } else {
    UpdateNoiseEstimate(3 * sqrt(var_noise_), min_frame_period, stable_state);
  }

  const double denom = var_noise_ + h[0]*Eh[0] + h[1]*Eh[1];

  const double K[2] = {Eh[0] / denom,
                       Eh[1] / denom};

  const double IKh[2][2] = {{1.0 - K[0]*h[0], -K[0]*h[1]},
                            {-K[1]*h[0], 1.0 - K[1]*h[1]}};
  const double e00 = E_[0][0];
  const double e01 = E_[0][1];

  // Update state
  E_[0][0] = e00 * IKh[0][0] + E_[1][0] * IKh[0][1];
  E_[0][1] = e01 * IKh[0][0] + E_[1][1] * IKh[0][1];
  E_[1][0] = e00 * IKh[1][0] + E_[1][0] * IKh[1][1];
  E_[1][1] = e01 * IKh[1][0] + E_[1][1] * IKh[1][1];

  // Covariance matrix, must be positive semi-definite
  assert(E_[0][0] + E_[1][1] >= 0 &&
         E_[0][0] * E_[1][1] - E_[0][1] * E_[1][0] >= 0 &&
         E_[0][0] >= 0);

  slope_ = slope_ + K[0] * residual;
  prev_offset_ = offset_;
  offset_ = offset_ + K[1] * residual;

  Detect(ts_delta);
}

double OveruseDetector::UpdateMinFramePeriod(double ts_delta) {
  double min_frame_period = ts_delta;
  if (ts_delta_hist_.size() >= kMinFramePeriodHistoryLength) {
    std::list<double>::iterator first_item = ts_delta_hist_.begin();
    ts_delta_hist_.erase(first_item);
  }
  std::list<double>::iterator it = ts_delta_hist_.begin();
  for (; it != ts_delta_hist_.end(); it++) {
    min_frame_period = BWE_MIN(*it, min_frame_period);
  }
  ts_delta_hist_.push_back(ts_delta);
  return min_frame_period;
}

void OveruseDetector::UpdateNoiseEstimate(double residual,
                                          double ts_delta,
                                          bool stable_state) {
  if (!stable_state) {
    return;
  }
  // Faster filter during startup to faster adapt to the jitter level
  // of the network alpha is tuned for 30 frames per second, but
  double alpha = 0.01;
  if (num_of_deltas_ > 10*30) {
    alpha = 0.002;
  }
  // Only update the noise estimate if we're not over-using
  // beta is a function of alpha and the time delta since
  // the previous update.
  const double beta = pow(1 - alpha, ts_delta * 30.0 / 1000.0);
  avg_noise_ = beta * avg_noise_
              + (1 - beta) * residual;
  var_noise_ = beta * var_noise_
              + (1 - beta) * (avg_noise_ - residual) * (avg_noise_ - residual);
  if (var_noise_ < 1e-7) {
    var_noise_ = 1e-7;
  }
}

BandwidthUsage OveruseDetector::Detect(double ts_delta) {
  if (num_of_deltas_ < 2) {
    return kBwNormal;
  }
  const double T = BWE_MIN(num_of_deltas_, 60) * offset_;
  if (fabs(T) > threshold_) {
    if (offset_ > 0) {
      if (time_over_using_ == -1) {
        // Initialize the timer. Assume that we've been
        // over-using half of the time since the previous
        // sample.
        time_over_using_ = ts_delta / 2;
      } else {
        // Increment timer
        time_over_using_ += ts_delta;
      }
      over_use_counter_++;
      if (time_over_using_ > kOverUsingTimeThreshold
          && over_use_counter_ > 1) {
        if (offset_ >= prev_offset_) {
          time_over_using_ = 0;
          over_use_counter_ = 0;
          hypothesis_ = kBwOverusing;
        }
      }
    } else {
      time_over_using_ = -1;
      over_use_counter_ = 0;
      hypothesis_ = kBwUnderusing;
    }
  } else {
    time_over_using_ = -1;
    over_use_counter_ = 0;
    hypothesis_ = kBwNormal;
  }
  return hypothesis_;
}
}  // namespace webrtc
