/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/remote_bitrate_estimator/packet_arrival_map.h"

#include <algorithm>

#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

constexpr size_t PacketArrivalTimeMap::kMaxNumberOfPackets;

void PacketArrivalTimeMap::AddPacket(int64_t sequence_number,
                                     Timestamp arrival_time) {
  RTC_DCHECK_GE(arrival_time, Timestamp::Zero());
  if (!has_seen_packet_) {
    // First packet.
    has_seen_packet_ = true;
    begin_sequence_number_ = sequence_number;
    arrival_times_.push_back(arrival_time);
    return;
  }

  int64_t pos = sequence_number - begin_sequence_number_;
  if (pos >= 0 && pos < static_cast<int64_t>(arrival_times_.size())) {
    // The packet is within the buffer - no need to expand it.
    arrival_times_[pos] = arrival_time;
    return;
  }

  if (pos < 0) {
    // The packet goes before the current buffer. Expand to add packet, but only
    // if it fits within kMaxNumberOfPackets.
    size_t missing_packets = -pos;
    if (missing_packets + arrival_times_.size() > kMaxNumberOfPackets) {
      // Don't expand the buffer further, as that would remove newly received
      // packets.
      return;
    }

    arrival_times_.insert(arrival_times_.begin(), missing_packets,
                          Timestamp::MinusInfinity());
    arrival_times_[0] = arrival_time;
    begin_sequence_number_ = sequence_number;
    return;
  }

  // The packet goes after the buffer.

  if (static_cast<size_t>(pos) >= kMaxNumberOfPackets) {
    // The buffer grows too large - old packets have to be removed.
    size_t packets_to_remove = pos - kMaxNumberOfPackets + 1;
    if (packets_to_remove >= arrival_times_.size()) {
      arrival_times_.clear();
      begin_sequence_number_ = sequence_number;
      pos = 0;
    } else {
      // Also trim the buffer to remove leading non-received packets, to
      // ensure that the buffer only spans received packets.
      while (packets_to_remove < arrival_times_.size() &&
             arrival_times_[packets_to_remove].IsInfinite()) {
        ++packets_to_remove;
      }

      arrival_times_.erase(arrival_times_.begin(),
                           arrival_times_.begin() + packets_to_remove);
      begin_sequence_number_ += packets_to_remove;
      pos -= packets_to_remove;
      RTC_DCHECK_GE(pos, 0);
    }
  }

  // Packets can be received out-of-order. If this isn't the next expected
  // packet, add enough placeholders to fill the gap.
  size_t missing_gap_packets = pos - arrival_times_.size();
  if (missing_gap_packets > 0) {
    arrival_times_.insert(arrival_times_.end(), missing_gap_packets,
                          Timestamp::MinusInfinity());
  }
  RTC_DCHECK_EQ(arrival_times_.size(), pos);
  arrival_times_.push_back(arrival_time);
  RTC_DCHECK_LE(arrival_times_.size(), kMaxNumberOfPackets);
}

void PacketArrivalTimeMap::RemoveOldPackets(int64_t sequence_number,
                                            Timestamp arrival_time_limit) {
  while (!arrival_times_.empty() && begin_sequence_number_ < sequence_number &&
         arrival_times_.front() <= arrival_time_limit) {
    arrival_times_.pop_front();
    ++begin_sequence_number_;
  }
}

bool PacketArrivalTimeMap::has_received(int64_t sequence_number) const {
  int64_t pos = sequence_number - begin_sequence_number_;
  if (pos >= 0 && pos < static_cast<int64_t>(arrival_times_.size()) &&
      arrival_times_[pos].IsFinite()) {
    return true;
  }
  return false;
}

void PacketArrivalTimeMap::EraseTo(int64_t sequence_number) {
  if (sequence_number > begin_sequence_number_) {
    size_t count =
        std::min(static_cast<size_t>(sequence_number - begin_sequence_number_),
                 arrival_times_.size());

    arrival_times_.erase(arrival_times_.begin(),
                         arrival_times_.begin() + count);
    begin_sequence_number_ += count;
  }
}

int64_t PacketArrivalTimeMap::clamp(int64_t sequence_number) const {
  return rtc::SafeClamp(sequence_number, begin_sequence_number(),
                        end_sequence_number());
}

}  // namespace webrtc
