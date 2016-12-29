/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TRANSPORT_FEEDBACK_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TRANSPORT_FEEDBACK_H_

#include <deque>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"

namespace webrtc {
namespace rtcp {

class PacketStatusChunk;

class TransportFeedback : public RtcpPacket {
 public:
  TransportFeedback();
  virtual ~TransportFeedback();

  void WithPacketSenderSsrc(uint32_t ssrc);
  void WithMediaSourceSsrc(uint32_t ssrc);
  void WithBase(uint16_t base_sequence,     // Seq# of first packet in this msg.
                int64_t ref_timestamp_us);  // Reference timestamp for this msg.
  void WithFeedbackSequenceNumber(uint8_t feedback_sequence);
  // NOTE: This method requires increasing sequence numbers (excepting wraps).
  bool WithReceivedPacket(uint16_t sequence_number, int64_t timestamp_us);

  enum class StatusSymbol {
    kNotReceived,
    kReceivedSmallDelta,
    kReceivedLargeDelta,
  };

  uint16_t GetBaseSequence() const;
  std::vector<TransportFeedback::StatusSymbol> GetStatusVector() const;
  std::vector<int16_t> GetReceiveDeltas() const;

  // Get the reference time in microseconds, including any precision loss.
  int64_t GetBaseTimeUs() const;
  // Convenience method for getting all deltas as microseconds. The first delta
  // is relative the base time.
  std::vector<int64_t> GetReceiveDeltasUs() const;

  uint32_t GetPacketSenderSsrc() const;
  uint32_t GetMediaSourceSsrc() const;
  static const int kDeltaScaleFactor = 250;  // Convert to multiples of 0.25ms.
  static const uint8_t kFeedbackMessageType = 15;  // TODO(sprang): IANA reg?
  static const uint8_t kPayloadType = 205;         // RTPFB, see RFC4585.

  static rtc::scoped_ptr<TransportFeedback> ParseFrom(const uint8_t* buffer,
                                                      size_t length);

 protected:
  bool Create(uint8_t* packet,
              size_t* position,
              size_t max_length,
              PacketReadyCallback* callback) const override;

  size_t BlockLength() const override;

 private:
  static PacketStatusChunk* ParseChunk(const uint8_t* buffer, size_t max_size);

  int64_t Unwrap(uint16_t sequence_number);
  bool AddSymbol(StatusSymbol symbol, int64_t seq);
  bool Encode(StatusSymbol symbol);
  bool HandleRleCandidate(StatusSymbol symbol,
                          int current_capacity,
                          int delta_size);
  void EmitRemaining();
  void EmitVectorChunk();
  void EmitRunLengthChunk();

  uint32_t packet_sender_ssrc_;
  uint32_t media_source_ssrc_;
  int32_t base_seq_;
  int64_t base_time_;
  uint8_t feedback_seq_;
  std::vector<PacketStatusChunk*> status_chunks_;
  std::vector<int16_t> receive_deltas_;

  int64_t last_seq_;
  int64_t last_timestamp_;
  std::deque<StatusSymbol> symbol_vec_;
  uint16_t first_symbol_cardinality_;
  bool vec_needs_two_bit_symbols_;
  uint32_t size_bytes_;

  RTC_DISALLOW_COPY_AND_ASSIGN(TransportFeedback);
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TRANSPORT_FEEDBACK_H_
