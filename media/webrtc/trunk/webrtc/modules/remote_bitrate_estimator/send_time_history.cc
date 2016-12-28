/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>

#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"

namespace webrtc {

SendTimeHistory::SendTimeHistory(Clock* clock, int64_t packet_age_limit)
    : clock_(clock),
      packet_age_limit_(packet_age_limit),
      oldest_sequence_number_(0) {}

SendTimeHistory::~SendTimeHistory() {
}

void SendTimeHistory::Clear() {
  history_.clear();
}

void SendTimeHistory::AddAndRemoveOld(uint16_t sequence_number,
                                      size_t length,
                                      bool was_paced) {
  EraseOld();

  if (history_.empty())
    oldest_sequence_number_ = sequence_number;

  history_.insert(std::pair<uint16_t, PacketInfo>(
      sequence_number, PacketInfo(clock_->TimeInMilliseconds(), 0, -1,
                                  sequence_number, length, was_paced)));
}

bool SendTimeHistory::OnSentPacket(uint16_t sequence_number,
                                   int64_t send_time_ms) {
  auto it = history_.find(sequence_number);
  if (it == history_.end())
    return false;
  it->second.send_time_ms = send_time_ms;
  return true;
}

void SendTimeHistory::EraseOld() {
  while (!history_.empty()) {
    auto it = history_.find(oldest_sequence_number_);
    assert(it != history_.end());

    if (clock_->TimeInMilliseconds() - it->second.creation_time_ms <=
        packet_age_limit_) {
      return;  // Oldest packet within age limit, return.
    }

    // TODO(sprang): Warn if erasing (too many) old items?
    history_.erase(it);
    UpdateOldestSequenceNumber();
  }
}

void SendTimeHistory::UpdateOldestSequenceNumber() {
  // After removing an element from the map, update oldest_sequence_number_ to
  // the element with the lowest sequence number higher than the previous
  // value (there might be gaps).
  if (history_.empty())
    return;
  auto it = history_.upper_bound(oldest_sequence_number_);
  if (it == history_.end()) {
    // No element with higher sequence number than oldest_sequence_number_
    // found, check wrap around. Note that history_.upper_bound(0) will not
    // find 0 even if it is there, need to explicitly check for 0.
    it = history_.find(0);
    if (it == history_.end())
      it = history_.upper_bound(0);
  }
  assert(it != history_.end());
  oldest_sequence_number_ = it->first;
}

bool SendTimeHistory::GetInfo(PacketInfo* packet, bool remove) {
  auto it = history_.find(packet->sequence_number);
  if (it == history_.end())
    return false;
  int64_t receive_time = packet->arrival_time_ms;
  *packet = it->second;
  packet->arrival_time_ms = receive_time;
  if (remove) {
    history_.erase(it);
    if (packet->sequence_number == oldest_sequence_number_)
      UpdateOldestSequenceNumber();
  }
  return true;
}

}  // namespace webrtc
