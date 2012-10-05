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

#include "module_common_types.h"
#include "rtp_rtcp_defines.h"
#include "typedefs.h"

namespace webrtc {

class RtpRtcpClock;
class CriticalSectionWrapper;

class RTPPacketHistory {
 public:
  RTPPacketHistory(RtpRtcpClock* clock);
  ~RTPPacketHistory();

  void SetStorePacketsStatus(bool enable, uint16_t number_to_store);

  bool StorePackets() const;

  // Stores RTP packet.
  int32_t PutRTPPacket(const uint8_t* packet,
                       uint16_t packet_length,
                       uint16_t max_packet_length,
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
  bool GetRTPPacket(uint16_t sequence_number,
                    uint32_t min_elapsed_time_ms,
                    uint8_t* packet,
                    uint16_t* packet_length,
                    int64_t* stored_time_ms,
                    StorageType* type) const;

  bool HasRTPPacket(uint16_t sequence_number) const;

  void UpdateResendTime(uint16_t sequence_number);

 private:
  void Allocate(uint16_t number_to_store);
  void Free();
  void VerifyAndAllocatePacketLength(uint16_t packet_length);
  bool FindSeqNum(uint16_t sequence_number, int32_t* index) const;

 private:
  RtpRtcpClock& clock_;
  CriticalSectionWrapper* critsect_;
  bool store_;
  uint32_t prev_index_;
  uint16_t max_packet_length_;

  std::vector<std::vector<uint8_t> > stored_packets_;
  std::vector<uint16_t> stored_seq_nums_;
  std::vector<uint16_t> stored_lengths_;
  std::vector<int64_t> stored_times_;
  std::vector<int64_t> stored_resend_times_;
  std::vector<StorageType> stored_types_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_RTP_PACKET_HISTORY_H_
