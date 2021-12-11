/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_INCLUDE_BITRATE_ADJUSTER_H_
#define COMMON_VIDEO_INCLUDE_BITRATE_ADJUSTER_H_

#include <functional>

#include "rtc_base/criticalsection.h"
#include "rtc_base/rate_statistics.h"

namespace webrtc {

class Clock;

// Certain hardware encoders tend to consistently overshoot the bitrate that
// they are configured to encode at. This class estimates an adjusted bitrate
// that when set on the encoder will produce the desired bitrate.
class BitrateAdjuster {
 public:
  // min_adjusted_bitrate_pct and max_adjusted_bitrate_pct are the lower and
  // upper bound outputted adjusted bitrates as a percentage of the target
  // bitrate.
  BitrateAdjuster(Clock* clock,
                  float min_adjusted_bitrate_pct,
                  float max_adjusted_bitrate_pct);
  virtual ~BitrateAdjuster() {}

  static const uint32_t kBitrateUpdateIntervalMs;
  static const uint32_t kBitrateUpdateFrameInterval;
  static const float kBitrateTolerancePct;
  static const float kBytesPerMsToBitsPerSecond;

  // Sets the desired bitrate in bps (bits per second).
  // Should be called at least once before Update.
  void SetTargetBitrateBps(uint32_t bitrate_bps);
  uint32_t GetTargetBitrateBps() const;

  // Returns the adjusted bitrate in bps.
  uint32_t GetAdjustedBitrateBps() const;

  // Returns what we think the current bitrate is.
  rtc::Optional<uint32_t> GetEstimatedBitrateBps();

  // This should be called after each frame is encoded. The timestamp at which
  // it is called is used to estimate the output bitrate of the encoder.
  // Should be called from only one thread.
  void Update(size_t frame_size);

 private:
  // Returns true if the bitrate is within kBitrateTolerancePct of bitrate_bps.
  bool IsWithinTolerance(uint32_t bitrate_bps, uint32_t target_bitrate_bps);

  // Returns smallest possible adjusted value.
  uint32_t GetMinAdjustedBitrateBps() const RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);
  // Returns largest possible adjusted value.
  uint32_t GetMaxAdjustedBitrateBps() const RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  void Reset();
  void UpdateBitrate(uint32_t current_time_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_);

  rtc::CriticalSection crit_;
  Clock* const clock_;
  const float min_adjusted_bitrate_pct_;
  const float max_adjusted_bitrate_pct_;
  // The bitrate we want.
  volatile uint32_t target_bitrate_bps_ RTC_GUARDED_BY(crit_);
  // The bitrate we use to get what we want.
  volatile uint32_t adjusted_bitrate_bps_ RTC_GUARDED_BY(crit_);
  // The target bitrate that the adjusted bitrate was computed from.
  volatile uint32_t last_adjusted_target_bitrate_bps_ RTC_GUARDED_BY(crit_);
  // Used to estimate bitrate.
  RateStatistics bitrate_tracker_ RTC_GUARDED_BY(crit_);
  // The last time we tried to adjust the bitrate.
  uint32_t last_bitrate_update_time_ms_ RTC_GUARDED_BY(crit_);
  // The number of frames since the last time we tried to adjust the bitrate.
  uint32_t frames_since_last_update_ RTC_GUARDED_BY(crit_);
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_INCLUDE_BITRATE_ADJUSTER_H_
