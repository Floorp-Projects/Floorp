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
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

enum { kMinPacketRequestBytes = 50 };

RTPPacketHistory::RTPPacketHistory(Clock* clock)
  : clock_(clock),
    critsect_(CriticalSectionWrapper::CreateCriticalSection()),
    store_(false),
    prev_index_(0),
    max_packet_length_(0) {
}

RTPPacketHistory::~RTPPacketHistory() {
  Free();
  delete critsect_;
}

void RTPPacketHistory::SetStorePacketsStatus(bool enable, 
                                             uint16_t number_to_store) {
  if (enable) {
    Allocate(number_to_store);
  } else {
    Free();
  }
}

void RTPPacketHistory::Allocate(uint16_t number_to_store) {
  assert(number_to_store > 0);
  CriticalSectionScoped cs(critsect_);
  if (store_) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1,
        "SetStorePacketsStatus already set, number: %d", number_to_store);
    return;
  }

  store_ = true;
  stored_packets_.resize(number_to_store);
  stored_seq_nums_.resize(number_to_store);
  stored_lengths_.resize(number_to_store);
  stored_times_.resize(number_to_store);
  stored_send_times_.resize(number_to_store);
  stored_types_.resize(number_to_store);
}

void RTPPacketHistory::Free() {
  CriticalSectionScoped cs(critsect_);
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
  CriticalSectionScoped cs(critsect_);
  return store_;
}

// private, lock should already be taken
void RTPPacketHistory::VerifyAndAllocatePacketLength(uint16_t packet_length) {
  assert(packet_length > 0);
  if (!store_) {
    return;
  }

  if (packet_length <= max_packet_length_) {
    return;
  }

  std::vector<std::vector<uint8_t> >::iterator it;
  for (it = stored_packets_.begin(); it != stored_packets_.end(); ++it) {
    it->resize(packet_length);
  }
  max_packet_length_ = packet_length;
}

int32_t RTPPacketHistory::PutRTPPacket(const uint8_t* packet,
                                       uint16_t packet_length,
                                       uint16_t max_packet_length,
                                       int64_t capture_time_ms,
                                       StorageType type) {
  if (type == kDontStore) {
    return 0;
  }

  CriticalSectionScoped cs(critsect_);
  if (!store_) {
    return 0;
  }

  assert(packet);
  assert(packet_length > 3);

  VerifyAndAllocatePacketLength(max_packet_length);

  if (packet_length > max_packet_length_) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, -1,
        "Failed to store RTP packet, length: %d", packet_length);
    return -1;
  }

  const uint16_t seq_num = (packet[2] << 8) + packet[3];

  // Store packet
  std::vector<std::vector<uint8_t> >::iterator it =
      stored_packets_.begin() + prev_index_;
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

int32_t RTPPacketHistory::ReplaceRTPHeader(const uint8_t* packet,
                                           uint16_t sequence_number,
                                           uint16_t rtp_header_length) {
  CriticalSectionScoped cs(critsect_);
  if (!store_) {
    return 0;
  }

  assert(packet);
  assert(rtp_header_length > 3);

  if (rtp_header_length > max_packet_length_) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
        "Failed to replace RTP packet, length: %d", rtp_header_length);
    return -1;
  }

  int32_t index = 0;
  bool found = FindSeqNum(sequence_number, &index);
  if (!found) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
        "No match for getting seqNum %u", sequence_number);
    return -1;
  }

  uint16_t length = stored_lengths_.at(index);
  if (length == 0 || length > max_packet_length_) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
        "No match for getting seqNum %u, len %d", sequence_number, length);
    return -1;
  }
  assert(stored_seq_nums_[index] == sequence_number);

  // Update RTP header.
  std::vector<std::vector<uint8_t> >::iterator it =
      stored_packets_.begin() + index;
  std::copy(packet, packet + rtp_header_length, it->begin());
  return 0;
}

bool RTPPacketHistory::HasRTPPacket(uint16_t sequence_number) const {
  CriticalSectionScoped cs(critsect_);
  if (!store_) {
    return false;
  }

  int32_t index = 0;
  bool found = FindSeqNum(sequence_number, &index);
  if (!found) {
    return false;
  }
 
  uint16_t length = stored_lengths_.at(index);
  if (length == 0 || length > max_packet_length_) {
    // Invalid length.
    return false;
  }
  return true;
}

bool RTPPacketHistory::GetPacketAndSetSendTime(uint16_t sequence_number,
                                               uint32_t min_elapsed_time_ms,
                                               bool retransmit,
                                               uint8_t* packet,
                                               uint16_t* packet_length,
                                               int64_t* stored_time_ms) {
  CriticalSectionScoped cs(critsect_);
  if (!store_) {
    return false;
  }

  int32_t index = 0;
  bool found = FindSeqNum(sequence_number, &index);
  if (!found) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
        "No match for getting seqNum %u", sequence_number);
    return false;
  }

  uint16_t length = stored_lengths_.at(index);
  if (length == 0 || length > max_packet_length_) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1,
        "No match for getting seqNum %u, len %d", sequence_number, length);
    return false;
  }

  if (length > *packet_length) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1, 
                 "Input buffer too short for packet %u", sequence_number);
    return false;
  }

  // Verify elapsed time since last retrieve. 
  int64_t now = clock_->TimeInMilliseconds();
  if (min_elapsed_time_ms > 0 &&
      ((now - stored_send_times_.at(index)) < min_elapsed_time_ms)) {
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, -1, 
        "Skip getting packet %u, packet recently resent.", sequence_number);
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
                                 uint16_t* packet_length,
                                 int64_t* stored_time_ms) const {
  // Get packet.
  uint16_t length = stored_lengths_.at(index);
  std::vector<std::vector<uint8_t> >::const_iterator it_found_packet =
      stored_packets_.begin() + index;
  std::copy(it_found_packet->begin(), it_found_packet->begin() + length,
            packet);
  *packet_length = length;
  *stored_time_ms = stored_times_.at(index);
}

bool RTPPacketHistory::GetBestFittingPacket(uint8_t* packet,
                                            uint16_t* packet_length,
                                            int64_t* stored_time_ms) {
  CriticalSectionScoped cs(critsect_);
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

int RTPPacketHistory::FindBestFittingPacket(uint16_t size) const {
  if (size < kMinPacketRequestBytes || stored_lengths_.empty())
    return -1;
  int min_diff = -1;
  size_t best_index = 0;
  for (size_t i = 0; i < stored_lengths_.size(); ++i) {
    if (stored_lengths_[i] == 0)
      continue;
    int diff = abs(stored_lengths_[i] - size);
    if (min_diff < 0 || diff < min_diff) {
      min_diff = diff;
      best_index = i;
    }
  }
  if (min_diff < 0)
    return -1;
  return best_index;
}
}  // namespace webrtc
