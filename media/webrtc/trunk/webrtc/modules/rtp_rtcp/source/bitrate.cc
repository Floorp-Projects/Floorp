/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/bitrate.h"

#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

Bitrate::Bitrate(Clock* clock)
    : clock_(clock),
      crit_(CriticalSectionWrapper::CreateCriticalSection()),
      packet_rate_(0),
      bitrate_(0),
      bitrate_next_idx_(0),
      time_last_rate_update_(0),
      bytes_count_(0),
      packet_count_(0) {
  memset(packet_rate_array_, 0, sizeof(packet_rate_array_));
  memset(bitrate_diff_ms_, 0, sizeof(bitrate_diff_ms_));
  memset(bitrate_array_, 0, sizeof(bitrate_array_));
}

void Bitrate::Update(const int32_t bytes) {
  CriticalSectionScoped cs(crit_.get());
  bytes_count_ += bytes;
  packet_count_++;
}

uint32_t Bitrate::PacketRate() const {
  CriticalSectionScoped cs(crit_.get());
  return packet_rate_;
}

uint32_t Bitrate::BitrateLast() const {
  CriticalSectionScoped cs(crit_.get());
  return bitrate_;
}

uint32_t Bitrate::BitrateNow() const {
  CriticalSectionScoped cs(crit_.get());
  int64_t now = clock_->TimeInMilliseconds();
  int64_t diff_ms = now - time_last_rate_update_;

  if (diff_ms > 10000) {  // 10 seconds.
    // Too high difference, ignore.
    return bitrate_;
  }
  int64_t bits_since_last_rate_update = 8 * bytes_count_ * 1000;

  // We have to consider the time when the measurement was done:
  // ((bits/sec * sec) + (bits)) / sec.
  int64_t bitrate = (static_cast<uint64_t>(bitrate_) * 1000 +
                           bits_since_last_rate_update) / (1000 + diff_ms);
  return static_cast<uint32_t>(bitrate);
}

int64_t Bitrate::time_last_rate_update() const {
  CriticalSectionScoped cs(crit_.get());
  return time_last_rate_update_;
}

void Bitrate::Process() {
  // Triggered by timer.
  CriticalSectionScoped cs(crit_.get());
  int64_t now = clock_->TimeInMilliseconds();
  int64_t diff_ms = now - time_last_rate_update_;

  if (diff_ms < 100) {
    // Not enough data, wait...
    return;
  }
  if (diff_ms > 10000) {  // 10 seconds.
    // Too high difference, ignore.
    time_last_rate_update_ = now;
    bytes_count_ = 0;
    packet_count_ = 0;
    return;
  }
  packet_rate_array_[bitrate_next_idx_] = (packet_count_ * 1000) / diff_ms;
  bitrate_array_[bitrate_next_idx_] = 8 * ((bytes_count_ * 1000) / diff_ms);
  bitrate_diff_ms_[bitrate_next_idx_] = diff_ms;
  bitrate_next_idx_++;
  if (bitrate_next_idx_ >= 10) {
    bitrate_next_idx_ = 0;
  }
  int64_t sum_diffMS = 0;
  int64_t sum_bitrateMS = 0;
  int64_t sum_packetrateMS = 0;
  for (int i = 0; i < 10; i++) {
    sum_diffMS += bitrate_diff_ms_[i];
    sum_bitrateMS += bitrate_array_[i] * bitrate_diff_ms_[i];
    sum_packetrateMS += packet_rate_array_[i] * bitrate_diff_ms_[i];
  }
  time_last_rate_update_ = now;
  bytes_count_ = 0;
  packet_count_ = 0;
  packet_rate_ = static_cast<uint32_t>(sum_packetrateMS / sum_diffMS);
  bitrate_ = static_cast<uint32_t>(sum_bitrateMS / sum_diffMS);
}

}  // namespace webrtc
