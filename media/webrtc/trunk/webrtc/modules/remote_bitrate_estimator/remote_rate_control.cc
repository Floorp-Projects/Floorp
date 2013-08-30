/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/remote_rate_control.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

const unsigned int kDefaultRttMs = 200;

RemoteRateControl::RemoteRateControl()
    : min_configured_bit_rate_(30000),
    max_configured_bit_rate_(30000000),
    current_bit_rate_(max_configured_bit_rate_),
    max_hold_rate_(0),
    avg_max_bit_rate_(-1.0f),
    var_max_bit_rate_(0.4f),
    rate_control_state_(kRcHold),
    came_from_state_(kRcDecrease),
    rate_control_region_(kRcMaxUnknown),
    last_bit_rate_change_(-1),
    current_input_(kBwNormal, 0, 1.0),
    updated_(false),
    time_first_incoming_estimate_(-1),
    initialized_bit_rate_(false),
    avg_change_period_(1000.0f),
    last_change_ms_(-1),
    beta_(0.9f),
    rtt_(kDefaultRttMs)
{
}

void RemoteRateControl::Reset() {
  *this = RemoteRateControl();
  came_from_state_ = kRcHold;
}

bool RemoteRateControl::ValidEstimate() const {
  return initialized_bit_rate_;
}

bool RemoteRateControl::TimeToReduceFurther(int64_t time_now,
    unsigned int incoming_bitrate) const {
  const int bitrate_reduction_interval = std::max(std::min(rtt_, 200u), 10u);
  if (time_now - last_bit_rate_change_ >= bitrate_reduction_interval) {
    return true;
  }
  if (ValidEstimate()) {
    const int threshold = static_cast<int>(1.05 * incoming_bitrate);
    const int bitrate_difference = LatestEstimate() - incoming_bitrate;
    return bitrate_difference > threshold;
  }
  return false;
}

int32_t RemoteRateControl::SetConfiguredBitRates(uint32_t min_bit_rate_bps,
                                                 uint32_t max_bit_rate_bps) {
  if (min_bit_rate_bps > max_bit_rate_bps) {
    return -1;
  }
  min_configured_bit_rate_ = min_bit_rate_bps;
  max_configured_bit_rate_ = max_bit_rate_bps;
  current_bit_rate_ = std::min(std::max(min_bit_rate_bps, current_bit_rate_),
                               max_bit_rate_bps);
  return 0;
}

uint32_t RemoteRateControl::LatestEstimate() const {
  return current_bit_rate_;
}

uint32_t RemoteRateControl::UpdateBandwidthEstimate(int64_t now_ms) {
  current_bit_rate_ = ChangeBitRate(current_bit_rate_,
                                    current_input_._incomingBitRate,
                                    current_input_._noiseVar,
                                    now_ms);
  return current_bit_rate_;
}

void RemoteRateControl::SetRtt(unsigned int rtt) {
  rtt_ = rtt;
}

RateControlRegion RemoteRateControl::Update(const RateControlInput* input,
                                            int64_t now_ms) {
  assert(input);

  // Set the initial bit rate value to what we're receiving the first half
  // second.
  if (!initialized_bit_rate_) {
    if (time_first_incoming_estimate_ < 0) {
      if (input->_incomingBitRate > 0) {
        time_first_incoming_estimate_ = now_ms;
      }
    } else if (now_ms - time_first_incoming_estimate_ > 500 &&
               input->_incomingBitRate > 0) {
      current_bit_rate_ = input->_incomingBitRate;
      initialized_bit_rate_ = true;
    }
  }

  if (updated_ && current_input_._bwState == kBwOverusing) {
    // Only update delay factor and incoming bit rate. We always want to react
    // on an over-use.
    current_input_._noiseVar = input->_noiseVar;
    current_input_._incomingBitRate = input->_incomingBitRate;
    return rate_control_region_;
  }
  updated_ = true;
  current_input_ = *input;
  WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1, "BWE: Incoming rate = %u kbps",
               input->_incomingBitRate/1000);
  return rate_control_region_;
}

uint32_t RemoteRateControl::ChangeBitRate(uint32_t current_bit_rate,
                                          uint32_t incoming_bit_rate,
                                          double noise_var,
                                          int64_t now_ms) {
  if (!updated_) {
    return current_bit_rate_;
  }
  updated_ = false;
  UpdateChangePeriod(now_ms);
  ChangeState(current_input_, now_ms);
  // calculated here because it's used in multiple places
  const float incoming_bit_rate_kbps = incoming_bit_rate / 1000.0f;
  // Calculate the max bit rate std dev given the normalized
  // variance and the current incoming bit rate.
  const float std_max_bit_rate = sqrt(var_max_bit_rate_ * avg_max_bit_rate_);
  bool recovery = false;
  switch (rate_control_state_) {
    case kRcHold: {
      max_hold_rate_ = std::max(max_hold_rate_, incoming_bit_rate);
      break;
    }
    case kRcIncrease: {
      if (avg_max_bit_rate_ >= 0) {
        if (incoming_bit_rate_kbps > avg_max_bit_rate_ + 3 * std_max_bit_rate) {
          ChangeRegion(kRcMaxUnknown);
          avg_max_bit_rate_ = -1.0;
        } else if (incoming_bit_rate_kbps > avg_max_bit_rate_ + 2.5 *
                   std_max_bit_rate) {
          ChangeRegion(kRcAboveMax);
        }
      }
      WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
                   "BWE: Response time: %f + %i + 10*33\n",
                   avg_change_period_, rtt_);
      const uint32_t response_time = static_cast<uint32_t>(avg_change_period_ +
          0.5f) + rtt_ + 300;
      double alpha = RateIncreaseFactor(now_ms, last_bit_rate_change_,
                                        response_time, noise_var);

      WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
          "BWE: avg_change_period_ = %f ms; RTT = %u ms", avg_change_period_,
          rtt_);

      current_bit_rate = static_cast<uint32_t>(current_bit_rate * alpha) + 1000;
      if (max_hold_rate_ > 0 && beta_ * max_hold_rate_ > current_bit_rate) {
        current_bit_rate = static_cast<uint32_t>(beta_ * max_hold_rate_);
        avg_max_bit_rate_ = beta_ * max_hold_rate_ / 1000.0f;
        ChangeRegion(kRcNearMax);
        recovery = true;
      }
      max_hold_rate_ = 0;
      WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
          "BWE: Increase rate to current_bit_rate = %u kbps",
          current_bit_rate / 1000);
      last_bit_rate_change_ = now_ms;
      break;
    }
    case kRcDecrease: {
      if (incoming_bit_rate < min_configured_bit_rate_) {
        current_bit_rate = min_configured_bit_rate_;
      } else {
        // Set bit rate to something slightly lower than max
        // to get rid of any self-induced delay.
        current_bit_rate = static_cast<uint32_t>(beta_ * incoming_bit_rate +
            0.5);
        if (current_bit_rate > current_bit_rate_) {
          // Avoid increasing the rate when over-using.
          if (rate_control_region_ != kRcMaxUnknown) {
            current_bit_rate = static_cast<uint32_t>(beta_ * avg_max_bit_rate_ *
                1000 + 0.5f);
          }
          current_bit_rate = std::min(current_bit_rate, current_bit_rate_);
        }
        ChangeRegion(kRcNearMax);

        if (incoming_bit_rate_kbps < avg_max_bit_rate_ - 3 * std_max_bit_rate) {
          avg_max_bit_rate_ = -1.0f;
        }

        UpdateMaxBitRateEstimate(incoming_bit_rate_kbps);

        WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
            "BWE: Decrease rate to current_bit_rate = %u kbps",
            current_bit_rate / 1000);
      }
      // Stay on hold until the pipes are cleared.
      ChangeState(kRcHold);
      last_bit_rate_change_ = now_ms;
      break;
    }
    default:
      assert(false);
  }
  if (!recovery && (incoming_bit_rate > 100000 || current_bit_rate > 150000) &&
      current_bit_rate > 1.5 * incoming_bit_rate) {
    // Allow changing the bit rate if we are operating at very low rates
    // Don't change the bit rate if the send side is too far off
    current_bit_rate = current_bit_rate_;
    last_bit_rate_change_ = now_ms;
  }
  return current_bit_rate;
}

double RemoteRateControl::RateIncreaseFactor(int64_t now_ms,
                                             int64_t last_ms,
                                             uint32_t reaction_time_ms,
                                             double noise_var) const {
  // alpha = 1.02 + B ./ (1 + exp(b*(tr - (c1*s2 + c2))))
  // Parameters
  const double B = 0.0407;
  const double b = 0.0025;
  const double c1 = -6700.0 / (33 * 33);
  const double c2 = 800.0;
  const double d = 0.85;

  double alpha = 1.005 + B / (1 + exp( b * (d * reaction_time_ms -
      (c1 * noise_var + c2))));

  if (alpha < 1.005) {
    alpha = 1.005;
  } else if (alpha > 1.3) {
    alpha = 1.3;
  }

  WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1, "BWE: alpha = %f", alpha);

  if (last_ms > -1) {
    alpha = pow(alpha, (now_ms - last_ms) / 1000.0);
  }

  if (rate_control_region_ == kRcNearMax) {
    // We're close to our previous maximum. Try to stabilize the
    // bit rate in this region, by increasing in smaller steps.
    alpha = alpha - (alpha - 1.0) / 2.0;
  } else if (rate_control_region_ == kRcMaxUnknown) {
    alpha = alpha + (alpha - 1.0) * 2.0;
  }

  return alpha;
}

void RemoteRateControl::UpdateChangePeriod(int64_t now_ms) {
  int64_t change_period = 0;
  if (last_change_ms_ > -1) {
    change_period = now_ms - last_change_ms_;
  }
  last_change_ms_ = now_ms;
  avg_change_period_ = 0.9f * avg_change_period_ + 0.1f * change_period;
}

void RemoteRateControl::UpdateMaxBitRateEstimate(float incoming_bit_rate_kbps) {
  const float alpha = 0.05f;
  if (avg_max_bit_rate_ == -1.0f) {
    avg_max_bit_rate_ = incoming_bit_rate_kbps;
  } else {
    avg_max_bit_rate_ = (1 - alpha) * avg_max_bit_rate_ +
        alpha * incoming_bit_rate_kbps;
  }
  // Estimate the max bit rate variance and normalize the variance
  // with the average max bit rate.
  const float norm = std::max(avg_max_bit_rate_, 1.0f);
  var_max_bit_rate_ = (1 - alpha) * var_max_bit_rate_ +
      alpha * (avg_max_bit_rate_ - incoming_bit_rate_kbps) *
          (avg_max_bit_rate_ - incoming_bit_rate_kbps) / norm;
  // 0.4 ~= 14 kbit/s at 500 kbit/s
  if (var_max_bit_rate_ < 0.4f) {
    var_max_bit_rate_ = 0.4f;
  }
  // 2.5f ~= 35 kbit/s at 500 kbit/s
  if (var_max_bit_rate_ > 2.5f) {
    var_max_bit_rate_ = 2.5f;
  }
}

void RemoteRateControl::ChangeState(const RateControlInput& input,
                                    int64_t now_ms) {
  switch (current_input_._bwState) {
    case kBwNormal:
      if (rate_control_state_ == kRcHold) {
        last_bit_rate_change_ = now_ms;
        ChangeState(kRcIncrease);
      }
      break;
    case kBwOverusing:
      if (rate_control_state_ != kRcDecrease) {
        ChangeState(kRcDecrease);
      }
      break;
    case kBwUnderusing:
      ChangeState(kRcHold);
      break;
    default:
      assert(false);
  }
}

void RemoteRateControl::ChangeRegion(RateControlRegion region) {
  rate_control_region_ = region;
  switch (rate_control_region_) {
    case kRcAboveMax:
    case kRcMaxUnknown:
      beta_ = 0.9f;
      break;
    case kRcNearMax:
      beta_ = 0.95f;
      break;
    default:
      assert(false);
  }
}

void RemoteRateControl::ChangeState(RateControlState new_state) {
  came_from_state_ = rate_control_state_;
  rate_control_state_ = new_state;
  char state1[15];
  char state2[15];
  char state3[15];
  StateStr(came_from_state_, state1);
  StateStr(rate_control_state_, state2);
  StateStr(current_input_._bwState, state3);
  WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
               "\t%s => %s due to %s\n", state1, state2, state3);
}

void RemoteRateControl::StateStr(RateControlState state, char* str) {
  switch (state) {
    case kRcDecrease:
      strncpy(str, "DECREASE", 9);
      break;
    case kRcHold:
      strncpy(str, "HOLD", 5);
      break;
    case kRcIncrease:
      strncpy(str, "INCREASE", 9);
      break;
    default:
      assert(false);
  }
}

void RemoteRateControl::StateStr(BandwidthUsage state, char* str) {
  switch (state) {
    case kBwNormal:
      strncpy(str, "NORMAL", 7);
      break;
    case kBwOverusing:
      strncpy(str, "OVER USING", 11);
      break;
    case kBwUnderusing:
      strncpy(str, "UNDER USING", 12);
      break;
    default:
      assert(false);
  }
}
} // namespace webrtc
