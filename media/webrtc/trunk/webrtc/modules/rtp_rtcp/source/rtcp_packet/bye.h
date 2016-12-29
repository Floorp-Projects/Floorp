/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_BYE_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_BYE_H_

#include <string>
#include <vector>

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

namespace webrtc {
namespace rtcp {

class Bye : public RtcpPacket {
 public:
  static const uint8_t kPacketType = 203;

  Bye();
  virtual ~Bye() {}

  // Parse assumes header is already parsed and validated.
  bool Parse(const RTCPUtility::RtcpCommonHeader& header,
             const uint8_t* payload);  // Size of the payload is in the header.

  void From(uint32_t ssrc) { sender_ssrc_ = ssrc; }
  bool WithCsrc(uint32_t csrc);
  void WithReason(const std::string& reason);

  uint32_t sender_ssrc() const { return sender_ssrc_; }
  const std::vector<uint32_t>& csrcs() const { return csrcs_; }
  const std::string& reason() const { return reason_; }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const int kMaxNumberOfCsrcs = 0x1f - 1;  // First item is sender SSRC.

  size_t BlockLength() const override;

  uint32_t sender_ssrc_;
  std::vector<uint32_t> csrcs_;
  std::string reason_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Bye);
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_BYE_H_
