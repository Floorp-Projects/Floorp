/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Class for storing RTP packets.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_RTP_PACKET_HISTORY_H_
#define WEBRTC_MODULES_RTP_RTCP_RTP_PACKET_HISTORY_H_

#include <vector>

#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Clock;
class CriticalSectionWrapper;

static const size_t kMaxHistoryCapacity = 9600;

class RTPPacketHistory {
 public:
  RTPPacketHistory(Clock* clock);
  ~RTPPacketHistory();

  void SetStorePacketsStatus(bool enable, uint16_t number_to_store);

  bool StorePackets() const;

  // Stores RTP packet.
  int32_t PutRTPPacket(const uint8_t* packet,
                       size_t packet_length,
                       size_t max_packet_length,
                       int64_t capture_time_ms,
                       StorageType type);

  // Gets stored RTP packet corresponding to the input sequence number.
  // The packet is copied to the buffer pointed to by ptr_rtp_packet.
  // The rtp_packet_length should show the available buffer size.
  // Returns true if packet is found.
  // rtp_packet_length: returns the copied packet length on success.
  // min_elapsed_time_ms: the minimum time that must have elapsed since the last
  // time the packet was resent (parameter is ignored if set to zero).
  // If the packet is found but the minimum time has not elaped, no bytes are
  // copied.
  // stored_time_ms: returns the time when the packet was stored.
  // type: returns the storage type set in PutRTPPacket.
  bool GetPacketAndSetSendTime(uint16_t sequence_number,
                               int64_t min_elapsed_time_ms,
                               bool retransmit,
                               uint8_t* packet,
                               size_t* packet_length,
                               int64_t* stored_time_ms);

  bool GetBestFittingPacket(uint8_t* packet, size_t* packet_length,
                            int64_t* stored_time_ms);

  bool HasRTPPacket(uint16_t sequence_number) const;

  bool SetSent(uint16_t sequence_number);

 private:
  void GetPacket(int index,
                 uint8_t* packet,
                 size_t* packet_length,
                 int64_t* stored_time_ms) const
      EXCLUSIVE_LOCKS_REQUIRED(*critsect_);
  void Allocate(size_t number_to_store) EXCLUSIVE_LOCKS_REQUIRED(*critsect_);
  void Free() EXCLUSIVE_LOCKS_REQUIRED(*critsect_);
  void VerifyAndAllocatePacketLength(size_t packet_length, uint32_t start_index)
      EXCLUSIVE_LOCKS_REQUIRED(*critsect_);
  bool FindSeqNum(uint16_t sequence_number, int32_t* index) const
      EXCLUSIVE_LOCKS_REQUIRED(*critsect_);
  int FindBestFittingPacket(size_t size) const
      EXCLUSIVE_LOCKS_REQUIRED(*critsect_);

 private:
  Clock* clock_;
  rtc::scoped_ptr<CriticalSectionWrapper> critsect_;
  bool store_ GUARDED_BY(critsect_);
  uint32_t prev_index_ GUARDED_BY(critsect_);
  size_t max_packet_length_ GUARDED_BY(critsect_);

  std::vector<std::vector<uint8_t> > stored_packets_ GUARDED_BY(critsect_);
  std::vector<uint16_t> stored_seq_nums_ GUARDED_BY(critsect_);
  std::vector<size_t> stored_lengths_ GUARDED_BY(critsect_);
  std::vector<int64_t> stored_times_ GUARDED_BY(critsect_);
  std::vector<int64_t> stored_send_times_ GUARDED_BY(critsect_);
  std::vector<StorageType> stored_types_ GUARDED_BY(critsect_);
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_RTP_PACKET_HISTORY_H_
