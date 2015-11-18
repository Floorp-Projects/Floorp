/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/ratetracker.h"
#include "webrtc/base/timeutils.h"

namespace rtc {

RateTracker::RateTracker()
    : total_units_(0), units_second_(0),
      last_units_second_time_(~0u),
      last_units_second_calc_(0) {
}

size_t RateTracker::total_units() const {
  return total_units_;
}

size_t RateTracker::units_second() {
  // Snapshot units / second calculator. Determine how many seconds have
  // elapsed since our last reference point. If over 1 second, establish
  // a new reference point that is an integer number of seconds since the
  // last one, and compute the units over that interval.
  uint32 current_time = Time();
  if (last_units_second_time_ == ~0u) {
      last_units_second_time_ = current_time;
      last_units_second_calc_ = total_units_;
  } else {
    int delta = rtc::TimeDiff(current_time, last_units_second_time_);
    if (delta >= 1000) {
      int fraction_time = delta % 1000;
      int seconds = delta / 1000;
      int fraction_units =
          static_cast<int>(total_units_ - last_units_second_calc_) *
              fraction_time / delta;
      // Compute "units received during the interval" / "seconds in interval"
      units_second_ =
          (total_units_ - last_units_second_calc_ - fraction_units) / seconds;
      last_units_second_time_ = current_time - fraction_time;
      last_units_second_calc_ = total_units_ - fraction_units;
    }
  }

  return units_second_;
}

void RateTracker::Update(size_t units) {
  if (last_units_second_time_ == ~0u)
    last_units_second_time_ = Time();
  total_units_ += units;
}

uint32 RateTracker::Time() const {
  return rtc::Time();
}

}  // namespace rtc
