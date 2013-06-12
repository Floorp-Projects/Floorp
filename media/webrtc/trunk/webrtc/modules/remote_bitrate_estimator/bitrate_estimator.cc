/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/bitrate_estimator.h"

namespace webrtc {

const float kBitrateAverageWindowMs = 500.0f;

BitRateStats::BitRateStats()
    : data_samples_(),
      accumulated_bytes_(0) {
}

BitRateStats::~BitRateStats() {
  Init();
}

void BitRateStats::Init() {
  accumulated_bytes_ = 0;
  while (data_samples_.size() > 0) {
    delete data_samples_.front();
    data_samples_.pop_front();
  }
}

void BitRateStats::Update(uint32_t packet_size_bytes, int64_t now_ms) {
  // Find an empty slot for storing the new sample and at the same time
  // accumulate the history.
  data_samples_.push_back(new DataTimeSizeTuple(packet_size_bytes, now_ms));
  accumulated_bytes_ += packet_size_bytes;
  EraseOld(now_ms);
}

void BitRateStats::EraseOld(int64_t now_ms) {
  while (data_samples_.size() > 0) {
    if (now_ms - data_samples_.front()->time_complete_ms >
        kBitrateAverageWindowMs) {
      // Delete old sample
      accumulated_bytes_ -= data_samples_.front()->size_bytes;
      delete data_samples_.front();
      data_samples_.pop_front();
    } else {
      break;
    }
  }
}

uint32_t BitRateStats::BitRate(int64_t now_ms) {
  // Calculate the average bit rate the past BITRATE_AVERAGE_WINDOW ms.
  // Removes any old samples from the list.
  EraseOld(now_ms);
  return static_cast<uint32_t>(accumulated_bytes_ * 8.0f * 1000.0f /
                     kBitrateAverageWindowMs + 0.5f);
}
}  // namespace webrtc
