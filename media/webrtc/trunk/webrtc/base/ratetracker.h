/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_RATETRACKER_H_
#define WEBRTC_BASE_RATETRACKER_H_

#include <stdlib.h>
#include "webrtc/base/basictypes.h"

namespace rtc {

// Computes instantaneous units per second.
class RateTracker {
 public:
  RateTracker();
  virtual ~RateTracker() {}

  size_t total_units() const;
  size_t units_second();
  void Update(size_t units);

 protected:
  // overrideable for tests
  virtual uint32 Time() const;

 private:
  size_t total_units_;
  size_t units_second_;
  uint32 last_units_second_time_;
  size_t last_units_second_calc_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_RATETRACKER_H_
