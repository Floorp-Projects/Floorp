/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_RATE_STATISTICS_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_RATE_STATISTICS_H_

#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class RateStatistics {
 public:
  // window_size = window size in ms for the rate estimation
  // scale = coefficient to convert counts/ms to desired units,
  //         ex: if counts represents bytes, use 8*1000 to go to bits/s
  RateStatistics(uint32_t window_size_ms, float scale);
  ~RateStatistics();

  void Reset();
  void Update(uint32_t count, int64_t now_ms);
  uint32_t Rate(int64_t now_ms);

 private:
  void EraseOld(int64_t now_ms);

  // Counters are kept in buckets (circular buffer), with one bucket
  // per millisecond.
  const int num_buckets_;
  scoped_ptr<uint32_t[]> buckets_;

  // Total count recorded in buckets.
  uint32_t accumulated_count_;

  // Oldest time recorded in buckets.
  int64_t oldest_time_;

  // Bucket index of oldest counter recorded in buckets.
  int oldest_index_;

  // To convert counts/ms to desired units
  const float scale_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_RATE_STATISTICS_H_
