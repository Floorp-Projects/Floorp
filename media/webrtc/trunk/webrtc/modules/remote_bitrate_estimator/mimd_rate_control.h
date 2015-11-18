/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_MIMD_RATE_CONTROL_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_MIMD_RATE_CONTROL_H_

#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_rate_control.h"

namespace webrtc {

// A RemoteRateControl implementation based on multiplicative increases of
// bitrate when no over-use is detected and multiplicative decreases when
// over-uses are detected.
class MimdRateControl : public RemoteRateControl {
 public:
  explicit MimdRateControl(uint32_t min_bitrate_bps);
  virtual ~MimdRateControl() {}

  // Implements RemoteRateControl.
  RateControlType GetControlType() const override;
  uint32_t GetMinBitrate() const override;
  bool ValidEstimate() const override;
  int64_t GetFeedbackInterval() const override;
  bool TimeToReduceFurther(int64_t time_now,
                           uint32_t incoming_bitrate_bps) const override;
  uint32_t LatestEstimate() const override;
  uint32_t UpdateBandwidthEstimate(int64_t now_ms) override;
  void SetRtt(int64_t rtt) override;
  RateControlRegion Update(const RateControlInput* input,
                           int64_t now_ms) override;
  void SetEstimate(int bitrate_bps, int64_t now_ms) override;

 private:
  uint32_t ChangeBitRate(uint32_t current_bit_rate,
                         uint32_t incoming_bit_rate,
                         double delay_factor,
                         int64_t now_ms);
  double RateIncreaseFactor(int64_t now_ms,
                            int64_t last_ms,
                            int64_t reaction_time_ms,
                            double noise_var) const;
  void UpdateChangePeriod(int64_t now_ms);
  void UpdateMaxBitRateEstimate(float incoming_bit_rate_kbps);
  void ChangeState(const RateControlInput& input, int64_t now_ms);
  void ChangeState(RateControlState new_state);
  void ChangeRegion(RateControlRegion region);

  uint32_t min_configured_bit_rate_;
  uint32_t max_configured_bit_rate_;
  uint32_t current_bit_rate_;
  uint32_t max_hold_rate_;
  float avg_max_bit_rate_;
  float var_max_bit_rate_;
  RateControlState rate_control_state_;
  RateControlState came_from_state_;
  RateControlRegion rate_control_region_;
  int64_t last_bit_rate_change_;
  RateControlInput current_input_;
  bool updated_;
  int64_t time_first_incoming_estimate_;
  bool initialized_bit_rate_;
  float avg_change_period_;
  int64_t last_change_ms_;
  float beta_;
  int64_t rtt_;
  int64_t time_of_last_log_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MimdRateControl);
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_MIMD_RATE_CONTROL_H_
