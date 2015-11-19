/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_packet_history.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>   // memset
#include <limits>
#include <set>

#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

static const int kMinPacketRequestBytes = 50;

RTPPacketHistory::RTPPacketHistory(Clock* clock)
  : clock_(clock),
    critsect_(CriticalSectionWrapper::CreateCriticalSection()),
    store_(false),
    prev_index_(0),
    max_packet_length_(0) {
}

RTPPacketHistory::~RTPPacketHistory() {
}

void RTPPacketHistory::SetStorePacketsStatus(bool enable,
                                             uint16_t number_to_store) {
  CriticalSectionScoped cs(critsect_.get());
  if (enable) {
    if (store_) {
      LOG(LS_WARNING) << "Purging packet history in order to re-set status.";
      Free();
    }
    assert(!store_);
    Allocate(number_to_store);
  } else {
    Free();
  }
}

void RTPPacketHistory::Allocate(size_t number_to_store) {
  assert(number_to_store > 0);
  assert(number_to_store <= kMaxHistoryCapacity);
  store_ = true;
  stored_packets_.resize(number_to_store);
  stored_seq_nums_.resize(number_to_store);
  stored_lengths_.resize(number_to_store);
  stored_times_.resize(number_to_store);
  stored_send_times_.resize(number_to_store);
  stored_types_.resize(number_to_store);
}

void RTPPacketHistory::Free() {
  if (!store_) {
    return;
  }

  std::vector<std::vector<uint8_t> >::iterator it;
  for (it = stored_packets_.begin(); it != stored_packets_.end(); ++it) {
    it->clear();
  }

  stored_packets_.clear();
  stored_seq_nums_.clear();
  stored_lengths_.clear();
  stored_times_.clear();
  stored_send_times_.clear();
  stored_types_.clear();

  store_ = false;
  prev_index_ = 0;
  max_packet_length_ = 0;
}

bool RTPPacketHistory::StorePackets() const {
  CriticalSectionScoped cs(critsect_.get());
  return store_;
}

void RTPPacketHistory::VerifyAndAllocatePacketLength(size_t packet_length,
                                                     uint32_t start_index) {
  assert(packet_length > 0);
  if (!store_) {
    return;
  }

  // If start_index > 0 this is a resize and we must check any new (empty)
  // packets created during the resize.
  if (start_index == 0 && packet_length <= max_packet_length_) {
    return;
  }

  max_packet_length_ = std::max(packet_length, max_packet_length_);

  std::vector<std::vector<uint8_t> >::iterator it;
  for (it = stored_packets_.begin() + start_index; it != stored_packets_.end();
       ++it) {
    it->resize(max_packet_length_);
  }
}

int32_t RTPPacketHistory::PutRTPPacket(const uint8_t* packet,
                                       size_t packet_length,
                                       size_t max_packet_length,
                                       int64_t capture_time_ms,
                                       StorageType type) {
  if (type == kDontStore) {
    return 0;
  }

  CriticalSectionScoped cs(critsect_.get());
  if (!store_) {
    return 0;
  }

  assert(packet);
  assert(packet_length > 3);

  VerifyAndAllocatePacketLength(max_packet_length, 0);

  if (packet_length > max_packet_length_) {
    LOG(LS_WARNING) << "Failed to store RTP packet with length: "
                    << packet_length;
    return -1;
  }

  const uint16_t seq_num = (packet[2] << 8) + packet[3];

  // If index we're about to overwrite contains a packet that has not
  // yet been sent (probably pending in paced sender), we need to expand
  // the buffer.
  if (stored_lengths_[prev_index_] > 0 &&
      stored_send_times_[prev_index_] == 0) {
    size_t current_size = static_cast<uint16_t>(stored_packets_.size());
    if (current_size < kMaxHistoryCapacity) {
      size_t expanded_size = std::max(current_size * 3 / 2, current_size + 1);
      expanded_size = std::min(expanded_size, kMaxHistoryCapacity);
      Allocate(expanded_size);
      VerifyAndAllocatePacketLength(max_packet_length, current_size);
      // Causes discontinuity, but that's OK-ish. FindSeqNum() will still work,
      // but may be slower - at least until buffer has wrapped around once.
      prev_index_ = current_size;
    }
  }

  // Store packet
  std::vector<std::vector<uint8_t> >::iterator it =
      stored_packets_.begin() + prev_index_;
  // TODO(sprang): Overhaul this class and get rid of this copy step.
  //               (Finally introduce the RtpPacket class?)
  std::copy(packet, packet + packet_length, it->begin());

  stored_seq_nums_[prev_index_] = seq_num;
  stored_lengths_[prev_index_] = packet_length;
  stored_times_[prev_index_] = (capture_time_ms > 0) ? capture_time_ms :
      clock_->TimeInMilliseconds();
  stored_send_times_[prev_index_] = 0;  // Packet not sent.
  stored_types_[prev_index_] = type;

  ++prev_index_;
  if (prev_index_ >= stored_seq_nums_.size()) {
    prev_index_ = 0;
  }
  return 0;
}

bool RTPPacketHistory::HasRTPPacket(uint16_t sequence_number) const {
  CriticalSectionScoped cs(critsect_.get());
  if (!store_) {
    return false;
  }

  int32_t index = 0;
  bool found = FindSeqNum(sequence_number, &index);
  if (!found) {
    return false;
  }

  size_t length = stored_lengths_.at(index);
  if (length == 0 || length > max_packet_length_) {
    // Invalid length.
    return false;
  }
  return true;
}

bool RTPPacketHistory::SetSent(uint16_t sequence_number) {
  CriticalSectionScoped cs(critsect_.get());
  if (!store_) {
    return false;
  }

  int32_t index = 0;
  bool found = FindSeqNum(sequence_number, &index);
  if (!found) {
    return false;
  }

  // Send time already set.
  if (stored_send_times_[index] != 0) {
    return false;
  }

  stored_send_times_[index] = clock_->TimeInMilliseconds();
  return true;
}

bool RTPPacketHistory::GetPacketAndSetSendTime(uint16_t sequence_number,
                                               int64_t min_elapsed_time_ms,
                                               bool retransmit,
                                               uint8_t* packet,
                                               size_t* packet_length,
                                               int64_t* stored_time_ms) {
  CriticalSectionScoped cs(critsect_.get());
  assert(*packet_length >= max_packet_length_);
  if (!store_) {
    return false;
  }

  int32_t index = 0;
  bool found = FindSeqNum(sequence_number, &index);
  if (!found) {
    LOG(LS_WARNING) << "No match for getting seqNum " << sequence_number;
    return false;
  }

  size_t length = stored_lengths_.at(index);
  assert(length <= max_packet_length_);
  if (length == 0) {
    LOG(LS_WARNING) << "No match for getting seqNum " << sequence_number
                    << ", len " << length;
    return false;
  }

  // Verify elapsed time since last retrieve.
  int64_t now = clock_->TimeInMilliseconds();
  if (min_elapsed_time_ms > 0 &&
      ((now - stored_send_times_.at(index)) < min_elapsed_time_ms)) {
    return false;
  }

  if (retransmit && stored_types_.at(index) == kDontRetransmit) {
    // No bytes copied since this packet shouldn't be retransmitted or is
    // of zero size.
    return false;
  }
  stored_send_times_[index] = clock_->TimeInMilliseconds();
  GetPacket(index, packet, packet_length, stored_time_ms);
  return true;
}

void RTPPacketHistory::GetPacket(int index,
                                 uint8_t* packet,
                                 size_t* packet_length,
                                 int64_t* stored_time_ms) const {
  // Get packet.
  size_t length = stored_lengths_.at(index);
  std::vector<std::vector<uint8_t> >::const_iterator it_found_packet =
      stored_packets_.begin() + index;
  std::copy(it_found_packet->begin(), it_found_packet->begin() + length,
            packet);
  *packet_length = length;
  *stored_time_ms = stored_times_.at(index);
}

bool RTPPacketHistory::GetBestFittingPacket(uint8_t* packet,
                                            size_t* packet_length,
                                            int64_t* stored_time_ms) {
  CriticalSectionScoped cs(critsect_.get());
  if (!store_)
    return false;
  int index = FindBestFittingPacket(*packet_length);
  if (index < 0)
    return false;
  GetPacket(index, packet, packet_length, stored_time_ms);
  return true;
}

// private, lock should already be taken
bool RTPPacketHistory::FindSeqNum(uint16_t sequence_number,
                                  int32_t* index) const {
  uint16_t temp_sequence_number = 0;
  if (prev_index_ > 0) {
    *index = prev_index_ - 1;
    temp_sequence_number = stored_seq_nums_[*index];
  } else {
    *index = stored_seq_nums_.size() - 1;
    temp_sequence_number = stored_seq_nums_[*index];  // wrap
  }

  int32_t idx = (prev_index_ - 1) - (temp_sequence_number - sequence_number);
  if (idx >= 0 && idx < static_cast<int>(stored_seq_nums_.size())) {
    *index = idx;
    temp_sequence_number = stored_seq_nums_[*index];
  }

  if (temp_sequence_number != sequence_number) {
    // We did not found a match, search all.
    for (uint16_t m = 0; m < stored_seq_nums_.size(); m++) {
      if (stored_seq_nums_[m] == sequence_number) {
        *index = m;
        temp_sequence_number = stored_seq_nums_[*index];
        break;
      }
    }
  }
  if (temp_sequence_number == sequence_number) {
    // We found a match.
    return true;
  }
  return false;
}

int RTPPacketHistory::FindBestFittingPacket(size_t size) const {
  if (size < kMinPacketRequestBytes || stored_lengths_.empty())
    return -1;
  size_t min_diff = std::numeric_limits<size_t>::max();
  int best_index = -1;  // Returned unchanged if we don't find anything.
  for (size_t i = 0; i < stored_lengths_.size(); ++i) {
    if (stored_lengths_[i] == 0)
      continue;
    size_t diff = (stored_lengths_[i] > size) ?
        (stored_lengths_[i] - size) : (size - stored_lengths_[i]);
    if (diff < min_diff) {
      min_diff = diff;
      best_index = static_cast<int>(i);
    }
  }
  return best_index;
}
}  // namespace webrtc
