/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_MOCK_FAKE_TICK_TIME_H_
#define WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_MOCK_FAKE_TICK_TIME_H_

#include <assert.h>

#include <limits>

#include "modules/video_coding/main/source/tick_time_base.h"

namespace webrtc {

// Provides a fake implementation of TickTimeBase, intended for offline
// testing. This implementation does not query the system clock, but returns a
// time value set by the user when creating the object, and incremented with
// the method IncrementDebugClock.
class FakeTickTime : public TickTimeBase {
 public:
  explicit FakeTickTime(int64_t start_time_ms) : fake_now_ms_(start_time_ms) {}
  virtual ~FakeTickTime() {}
  virtual int64_t MillisecondTimestamp() const {
    return fake_now_ms_;
  }
  virtual int64_t MicrosecondTimestamp() const {
    return 1000 * fake_now_ms_;
  }
  virtual void IncrementDebugClock(int64_t increase_ms) {
    assert(increase_ms <= std::numeric_limits<int64_t>::max() - fake_now_ms_);
    fake_now_ms_ += increase_ms;
  }

 private:
  int64_t fake_now_ms_;
};

}  // namespace

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_MOCK_FAKE_TICK_TIME_H_
