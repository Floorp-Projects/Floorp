/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TMMBR_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TMMBR_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

namespace webrtc {
namespace rtcp {
// Temporary Maximum Media Stream Bit Rate Request (TMMBR) (RFC 5104).
class Tmmbr : public RtcpPacket {
 public:
  Tmmbr() : RtcpPacket() {
    memset(&tmmbr_, 0, sizeof(tmmbr_));
    memset(&tmmbr_item_, 0, sizeof(tmmbr_item_));
  }

  virtual ~Tmmbr() {}

  void From(uint32_t ssrc) {
    tmmbr_.SenderSSRC = ssrc;
  }
  void To(uint32_t ssrc) {
    tmmbr_item_.SSRC = ssrc;
  }
  void WithBitrateKbps(uint32_t bitrate_kbps) {
    tmmbr_item_.MaxTotalMediaBitRate = bitrate_kbps;
  }
  void WithOverhead(uint16_t overhead) {
    assert(overhead <= 0x1ff);
    tmmbr_item_.MeasuredOverhead = overhead;
  }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  size_t BlockLength() const {
    const size_t kFciLen = 8;
    return kCommonFbFmtLength + kFciLen;
  }

  RTCPUtility::RTCPPacketRTPFBTMMBR tmmbr_;
  RTCPUtility::RTCPPacketRTPFBTMMBRItem tmmbr_item_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Tmmbr);
};
}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TMMBR_H_
