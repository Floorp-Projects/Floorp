/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/tick_util.h"

#include <cassert>

namespace webrtc {

bool TickTime::use_fake_clock_ = false;
WebRtc_Word64 TickTime::fake_ticks_ = 0;

void TickTime::UseFakeClock(WebRtc_Word64 start_millisecond) {
  use_fake_clock_ = true;
  fake_ticks_ = MillisecondsToTicks(start_millisecond);
}

void TickTime::AdvanceFakeClock(WebRtc_Word64 milliseconds) {
  assert(use_fake_clock_);
  fake_ticks_ += MillisecondsToTicks(milliseconds);
}

}  // namespace webrtc
