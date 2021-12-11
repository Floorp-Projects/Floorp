/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_RATE_LIMITER_H_
#define RTC_BASE_RATE_LIMITER_H_

#include <limits>

#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/rate_statistics.h"

namespace webrtc {

class Clock;

// Class used to limit a bitrate, making sure the average does not exceed a
// maximum as measured over a sliding window. This class is thread safe; all
// methods will acquire (the same) lock befeore executing.
class RateLimiter {
 public:
  RateLimiter(const Clock* clock, int64_t max_window_ms);
  ~RateLimiter();

  // Try to use rate to send bytes. Returns true on success and if so updates
  // current rate.
  bool TryUseRate(size_t packet_size_bytes);

  // Set the maximum bitrate, in bps, that this limiter allows to send.
  void SetMaxRate(uint32_t max_rate_bps);

  // Set the window size over which to measure the current bitrate.
  // For example, irt retransmissions, this is typically the RTT.
  // Returns true on success and false if window_size_ms is out of range.
  bool SetWindowSize(int64_t window_size_ms);

 private:
  const Clock* const clock_;
  rtc::CriticalSection lock_;
  RateStatistics current_rate_ RTC_GUARDED_BY(lock_);
  int64_t window_size_ms_ RTC_GUARDED_BY(lock_);
  uint32_t max_rate_bps_ RTC_GUARDED_BY(lock_);

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RateLimiter);
};

}  // namespace webrtc

#endif  // RTC_BASE_RATE_LIMITER_H_
