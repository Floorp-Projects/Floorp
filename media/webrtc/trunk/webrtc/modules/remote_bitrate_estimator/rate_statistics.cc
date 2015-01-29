/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/rate_statistics.h"

#include <assert.h>

namespace webrtc {

RateStatistics::RateStatistics(uint32_t window_size_ms, float scale)
    : num_buckets_(window_size_ms + 1),  // N ms in (N+1) buckets.
      buckets_(new uint32_t[num_buckets_]()),
      accumulated_count_(0),
      oldest_time_(0),
      oldest_index_(0),
      scale_(scale / (num_buckets_ - 1)) {
}

RateStatistics::~RateStatistics() {
}

void RateStatistics::Reset() {
  accumulated_count_ = 0;
  oldest_time_ = 0;
  oldest_index_ = 0;
  for (int i = 0; i < num_buckets_; i++) {
    buckets_[i] = 0;
  }
}

void RateStatistics::Update(uint32_t count, int64_t now_ms) {
  if (now_ms < oldest_time_) {
    // Too old data is ignored.
    return;
  }

  EraseOld(now_ms);

  int now_offset = static_cast<int>(now_ms - oldest_time_);
  assert(now_offset < num_buckets_);
  int index = oldest_index_ + now_offset;
  if (index >= num_buckets_) {
    index -= num_buckets_;
  }
  buckets_[index] += count;
  accumulated_count_ += count;
}

uint32_t RateStatistics::Rate(int64_t now_ms) {
  EraseOld(now_ms);
  return static_cast<uint32_t>(accumulated_count_ * scale_ + 0.5f);
}

void RateStatistics::EraseOld(int64_t now_ms) {
  int64_t new_oldest_time = now_ms - num_buckets_ + 1;
  if (new_oldest_time <= oldest_time_) {
    return;
  }

  while (oldest_time_ < new_oldest_time) {
    uint32_t count_in_oldest_bucket = buckets_[oldest_index_];
    assert(accumulated_count_ >= count_in_oldest_bucket);
    accumulated_count_ -= count_in_oldest_bucket;
    buckets_[oldest_index_] = 0;
    if (++oldest_index_ >= num_buckets_) {
      oldest_index_ = 0;
    }
    ++oldest_time_;
    if (accumulated_count_ == 0) {
      // This guarantees we go through all the buckets at most once, even if
      // |new_oldest_time| is far greater than |oldest_time_|.
      break;
    }
  }
  oldest_time_ = new_oldest_time;
}

}  // namespace webrtc
