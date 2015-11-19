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

#include "webrtc/modules/bitrate_controller/send_time_history.h"

namespace webrtc {

SendTimeHistory::SendTimeHistory(int64_t packet_age_limit)
    : packet_age_limit_(packet_age_limit), oldest_sequence_number_(0) {
}

SendTimeHistory::~SendTimeHistory() {
}

void SendTimeHistory::Clear() {
  history_.clear();
}

void SendTimeHistory::AddAndRemoveOldSendTimes(uint16_t sequence_number,
                                               int64_t timestamp) {
  EraseOld(timestamp - packet_age_limit_);

  if (history_.empty())
    oldest_sequence_number_ = sequence_number;

  history_[sequence_number] = timestamp;
}

void SendTimeHistory::EraseOld(int64_t limit) {
  while (!history_.empty()) {
    auto it = history_.find(oldest_sequence_number_);
    assert(it != history_.end());

    if (it->second > limit)
      return;  // Oldest packet within age limit, return.

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

bool SendTimeHistory::GetSendTime(uint16_t sequence_number,
                                  int64_t* timestamp,
                                  bool remove) {
  auto it = history_.find(sequence_number);
  if (it == history_.end())
    return false;
  *timestamp = it->second;
  if (remove) {
    history_.erase(it);
    if (sequence_number == oldest_sequence_number_)
      UpdateOldestSequenceNumber();
  }
  return true;
}

}  // namespace webrtc
