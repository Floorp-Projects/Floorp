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

#include <algorithm>
#include <limits>
#include <set>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

static const int kMinPacketRequestBytes = 50;

RTPPacketHistory::RTPPacketHistory(Clock* clock)
    : clock_(clock),
      critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      store_(false),
      prev_index_(0) {}

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
}

void RTPPacketHistory::Free() {
  if (!store_) {
    return;
  }

  stored_packets_.clear();

  store_ = false;
  prev_index_ = 0;
}

bool RTPPacketHistory::StorePackets() const {
  CriticalSectionScoped cs(critsect_.get());
  return store_;
}

int32_t RTPPacketHistory::PutRTPPacket(const uint8_t* packet,
                                       size_t packet_length,
                                       int64_t capture_time_ms,
                                       StorageType type) {
  CriticalSectionScoped cs(critsect_.get());
  if (!store_) {
    return 0;
  }

  assert(packet);
  assert(packet_length > 3);

  if (packet_length > IP_PACKET_SIZE) {
    LOG(LS_WARNING) << "Failed to store RTP packet with length: "
                    << packet_length;
    return -1;
  }

  const uint16_t seq_num = (packet[2] << 8) + packet[3];

  // If index we're about to overwrite contains a packet that has not
  // yet been sent (probably pending in paced sender), we need to expand
  // the buffer.
  if (stored_packets_[prev_index_].length > 0 &&
      stored_packets_[prev_index_].send_time == 0) {
    size_t current_size = static_cast<uint16_t>(stored_packets_.size());
    if (current_size < kMaxHistoryCapacity) {
      size_t expanded_size = std::max(current_size * 3 / 2, current_size + 1);
      expanded_size = std::min(expanded_size, kMaxHistoryCapacity);
      Allocate(expanded_size);
      // Causes discontinuity, but that's OK-ish. FindSeqNum() will still work,
      // but may be slower - at least until buffer has wrapped around once.
      prev_index_ = current_size;
    }
  }

  // Store packet
  // TODO(sprang): Overhaul this class and get rid of this copy step.
  //               (Finally introduce the RtpPacket class?)
  memcpy(stored_packets_[prev_index_].data, packet, packet_length);
  stored_packets_[prev_index_].length = packet_length;

  stored_packets_[prev_index_].sequence_number = seq_num;
  stored_packets_[prev_index_].time_ms =
      (capture_time_ms > 0) ? capture_time_ms : clock_->TimeInMilliseconds();
  stored_packets_[prev_index_].send_time = 0;  // Packet not sent.
  stored_packets_[prev_index_].storage_type = type;
  stored_packets_[prev_index_].has_been_retransmitted = false;

  ++prev_index_;
  if (prev_index_ >= stored_packets_.size()) {
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

  if (stored_packets_[index].length == 0) {
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
  if (stored_packets_[index].send_time != 0) {
    return false;
  }

  stored_packets_[index].send_time = clock_->TimeInMilliseconds();
  return true;
}

bool RTPPacketHistory::GetPacketAndSetSendTime(uint16_t sequence_number,
                                               int64_t min_elapsed_time_ms,
                                               bool retransmit,
                                               uint8_t* packet,
                                               size_t* packet_length,
                                               int64_t* stored_time_ms) {
  CriticalSectionScoped cs(critsect_.get());
  RTC_CHECK_GE(*packet_length, static_cast<size_t>(IP_PACKET_SIZE));
  if (!store_)
    return false;

  int32_t index = 0;
  bool found = FindSeqNum(sequence_number, &index);
  if (!found) {
    LOG(LS_WARNING) << "No match for getting seqNum " << sequence_number;
    return false;
  }

  size_t length = stored_packets_[index].length;
  assert(length <= IP_PACKET_SIZE);
  if (length == 0) {
    LOG(LS_WARNING) << "No match for getting seqNum " << sequence_number
                    << ", len " << length;
    return false;
  }

  // Verify elapsed time since last retrieve, but only for retransmissions and
  // always send packet upon first retransmission request.
  int64_t now = clock_->TimeInMilliseconds();
  if (min_elapsed_time_ms > 0 && retransmit &&
      stored_packets_[index].has_been_retransmitted &&
      ((now - stored_packets_[index].send_time) < min_elapsed_time_ms)) {
    return false;
  }

  if (retransmit) {
    if (stored_packets_[index].storage_type == kDontRetransmit) {
      // No bytes copied since this packet shouldn't be retransmitted or is
      // of zero size.
      return false;
    }
    stored_packets_[index].has_been_retransmitted = true;
  }
  stored_packets_[index].send_time = clock_->TimeInMilliseconds();
  GetPacket(index, packet, packet_length, stored_time_ms);
  return true;
}

void RTPPacketHistory::GetPacket(int index,
                                 uint8_t* packet,
                                 size_t* packet_length,
                                 int64_t* stored_time_ms) const {
  // Get packet.
  size_t length = stored_packets_[index].length;
  memcpy(packet, stored_packets_[index].data, length);
  *packet_length = length;
  *stored_time_ms = stored_packets_[index].time_ms;
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
    temp_sequence_number = stored_packets_[*index].sequence_number;
  } else {
    *index = stored_packets_.size() - 1;
    temp_sequence_number = stored_packets_[*index].sequence_number;  // wrap
  }

  int32_t idx = (prev_index_ - 1) - (temp_sequence_number - sequence_number);
  if (idx >= 0 && idx < static_cast<int>(stored_packets_.size())) {
    *index = idx;
    temp_sequence_number = stored_packets_[*index].sequence_number;
  }

  if (temp_sequence_number != sequence_number) {
    // We did not found a match, search all.
    for (uint16_t m = 0; m < stored_packets_.size(); m++) {
      if (stored_packets_[m].sequence_number == sequence_number) {
        *index = m;
        temp_sequence_number = stored_packets_[*index].sequence_number;
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
  if (size < kMinPacketRequestBytes || stored_packets_.empty())
    return -1;
  size_t min_diff = std::numeric_limits<size_t>::max();
  int best_index = -1;  // Returned unchanged if we don't find anything.
  for (size_t i = 0; i < stored_packets_.size(); ++i) {
    if (stored_packets_[i].length == 0)
      continue;
    size_t diff = (stored_packets_[i].length > size)
                      ? (stored_packets_[i].length - size)
                      : (size - stored_packets_[i].length);
    if (diff < min_diff) {
      min_diff = diff;
      best_index = static_cast<int>(i);
    }
  }
  return best_index;
}

RTPPacketHistory::StoredPacket::StoredPacket() {}

}  // namespace webrtc
