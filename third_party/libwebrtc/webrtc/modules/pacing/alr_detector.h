/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_ALR_DETECTOR_H_
#define MODULES_PACING_ALR_DETECTOR_H_

#include "api/optional.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/pacing/interval_budget.h"
#include "modules/pacing/paced_sender.h"
#include "rtc_base/rate_statistics.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

// Application limited region detector is a class that utilizes signals of
// elapsed time and bytes sent to estimate whether network traffic is
// currently limited by the application's ability to generate traffic.
//
// AlrDetector provides a signal that can be utilized to adjust
// estimate bandwidth.
// Note: This class is not thread-safe.
class AlrDetector {
 public:
  AlrDetector();
  ~AlrDetector();

  void OnBytesSent(size_t bytes_sent, int64_t delta_time_ms);

  // Set current estimated bandwidth.
  void SetEstimatedBitrate(int bitrate_bps);

  // Returns time in milliseconds when the current application-limited region
  // started or empty result if the sender is currently not application-limited.
  rtc::Optional<int64_t> GetApplicationLimitedRegionStartTime() const;

  struct AlrExperimentSettings {
    float pacing_factor = PacedSender::kDefaultPaceMultiplier;
    int64_t max_paced_queue_time = PacedSender::kMaxQueueLengthMs;
    int alr_bandwidth_usage_percent = kDefaultAlrBandwidthUsagePercent;
    int alr_start_budget_level_percent = kDefaultAlrStartBudgetLevelPercent;
    int alr_stop_budget_level_percent = kDefaultAlrStopBudgetLevelPercent;
    // Will be sent to the receive side for stats slicing.
    // Can be 0..6, because it's sent as a 3 bits value and there's also
    // reserved value to indicate absence of experiment.
    int group_id = 0;
  };
  static rtc::Optional<AlrExperimentSettings> ParseAlrSettingsFromFieldTrial(
      const char* experiment_name);

  // Sent traffic percentage as a function of network capacity used to determine
  // application-limited region. ALR region start when bandwidth usage drops
  // below kAlrStartUsagePercent and ends when it raises above
  // kAlrEndUsagePercent. NOTE: This is intentionally conservative at the moment
  // until BW adjustments of application limited region is fine tuned.
  static constexpr int kDefaultAlrBandwidthUsagePercent = 65;
  static constexpr int kDefaultAlrStartBudgetLevelPercent = 80;
  static constexpr int kDefaultAlrStopBudgetLevelPercent = 50;
  static const char kScreenshareProbingBweExperimentName[];
  static const char kStrictPacingAndProbingExperimentName[];

  void UpdateBudgetWithElapsedTime(int64_t delta_time_ms);
  void UpdateBudgetWithBytesSent(size_t bytes_sent);

 private:
  int bandwidth_usage_percent_;
  int alr_start_budget_level_percent_;
  int alr_stop_budget_level_percent_;

  IntervalBudget alr_budget_;
  rtc::Optional<int64_t> alr_started_time_ms_;
};

}  // namespace webrtc

#endif  // MODULES_PACING_ALR_DETECTOR_H_
