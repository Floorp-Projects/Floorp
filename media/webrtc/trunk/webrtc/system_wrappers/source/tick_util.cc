/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/include/tick_util.h"

#include "webrtc/base/timeutils.h"

namespace webrtc {

int64_t TickTime::MillisecondTimestamp() {
  return TicksToMilliseconds(TickTime::Now().Ticks());
}

int64_t TickTime::MicrosecondTimestamp() {
  return TicksToMicroseconds(TickTime::Now().Ticks());
}

int64_t TickTime::MillisecondsToTicks(const int64_t ms) {
  return ms * rtc::kNumNanosecsPerMillisec;
}

int64_t TickTime::TicksToMilliseconds(const int64_t ticks) {
  return ticks / rtc::kNumNanosecsPerMillisec;
}

int64_t TickTime::TicksToMicroseconds(const int64_t ticks) {
  return ticks / rtc::kNumNanosecsPerMicrosec;
}

// Gets the native system tick count, converted to nanoseconds.
int64_t TickTime::QueryOsForTicks() {
  return rtc::TimeNanos();
}

}  // namespace webrtc
