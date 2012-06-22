/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "transmission_bucket.h"

#include <assert.h>
#include "critical_section_wrapper.h"

namespace webrtc {

TransmissionBucket::TransmissionBucket()
  : critsect_(CriticalSectionWrapper::CreateCriticalSection()),
    accumulator_(0),
    bytes_rem_total_(0),
    bytes_rem_interval_(0),
    packets_(),
    first_(true) {
}

TransmissionBucket::~TransmissionBucket() {
  packets_.clear();
  delete critsect_;
}

void TransmissionBucket::Reset() {
  webrtc::CriticalSectionScoped cs(*critsect_);
  accumulator_ = 0;
  bytes_rem_total_ = 0;
  bytes_rem_interval_ = 0;
  packets_.clear();
  first_ = true;
}

void TransmissionBucket::Fill(const uint16_t seq_num,
                              const uint32_t num_bytes) {
  webrtc::CriticalSectionScoped cs(*critsect_);
  accumulator_ += num_bytes;

  Packet p(seq_num, num_bytes);
  packets_.push_back(p);
}

bool TransmissionBucket::Empty() {
  webrtc::CriticalSectionScoped cs(*critsect_);
  return packets_.empty();
}

void TransmissionBucket::UpdateBytesPerInterval(
    const uint32_t delta_time_ms,
    const uint16_t target_bitrate_kbps) {
  webrtc::CriticalSectionScoped cs(*critsect_);

  const float kMargin = 1.05f;
  uint32_t bytes_per_interval = 
      kMargin * (target_bitrate_kbps * delta_time_ms / 8);

  if (bytes_rem_interval_ < 0) {
    bytes_rem_interval_ += bytes_per_interval;
  } else {
    bytes_rem_interval_ = bytes_per_interval;
  }

  if (accumulator_) {
    bytes_rem_total_ += bytes_per_interval;
    return;
  }
  bytes_rem_total_ = bytes_per_interval;
}

int32_t TransmissionBucket::GetNextPacket() {
  webrtc::CriticalSectionScoped cs(*critsect_);

  if (accumulator_ == 0) {
    // Empty.
    return -1;
  }

  std::vector<Packet>::const_iterator it_begin = packets_.begin();
  const uint16_t num_bytes = (*it_begin).length_;
  const uint16_t seq_num = (*it_begin).sequence_number_;

  if (first_) {
    // Ok to transmit first packet.
    first_ = false;
    packets_.erase(packets_.begin());
    return seq_num;
  }

  const float kFrameComplete = 0.80f;
  if (num_bytes * kFrameComplete > bytes_rem_total_) {
    // Packet does not fit.
    return -1;
  }

  if (bytes_rem_interval_ <= 0) {
    // All bytes consumed for this interval.
    return -1;
  }

  // Ok to transmit packet.
  bytes_rem_total_ -= num_bytes;
  bytes_rem_interval_ -= num_bytes;

  assert(accumulator_ >= num_bytes);
  accumulator_ -= num_bytes;

  packets_.erase(packets_.begin());
  return seq_num;
}
} // namespace webrtc
