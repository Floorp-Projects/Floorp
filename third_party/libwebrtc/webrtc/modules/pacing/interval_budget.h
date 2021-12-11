/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_PACING_INTERVAL_BUDGET_H_
#define MODULES_PACING_INTERVAL_BUDGET_H_

#include "common_types.h"  // NOLINT(build/include)

namespace webrtc {

// TODO(tschumim): Reflector IntervalBudget so that we can set a under- and
// over-use budget in ms.
class IntervalBudget {
 public:
  explicit IntervalBudget(int initial_target_rate_kbps);
  IntervalBudget(int initial_target_rate_kbps, bool can_build_up_underuse);
  void set_target_rate_kbps(int target_rate_kbps);

  // TODO(tschumim): Unify IncreaseBudget and UseBudget to one function.
  void IncreaseBudget(int64_t delta_time_ms);
  void UseBudget(size_t bytes);

  size_t bytes_remaining() const;
  int budget_level_percent() const;
  int target_rate_kbps() const;

 private:
  int target_rate_kbps_;
  int max_bytes_in_budget_;
  int bytes_remaining_;
  bool can_build_up_underuse_;
};

}  // namespace webrtc

#endif  // MODULES_PACING_INTERVAL_BUDGET_H_
