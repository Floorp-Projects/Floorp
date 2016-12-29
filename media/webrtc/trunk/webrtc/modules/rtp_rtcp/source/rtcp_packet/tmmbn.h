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

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TMMBN_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TMMBN_H_

#include <vector>
#include "webrtc/base/basictypes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

namespace webrtc {
namespace rtcp {

// Temporary Maximum Media Stream Bit Rate Notification (TMMBN) (RFC 5104).
class Tmmbn : public RtcpPacket {
 public:
  Tmmbn() : RtcpPacket() {
    memset(&tmmbn_, 0, sizeof(tmmbn_));
  }

  virtual ~Tmmbn() {}

  void From(uint32_t ssrc) {
    tmmbn_.SenderSSRC = ssrc;
  }
  // Max 50 TMMBR can be added per TMMBN.
  bool WithTmmbr(uint32_t ssrc, uint32_t bitrate_kbps, uint16_t overhead);

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const int kMaxNumberOfTmmbrs = 50;

  size_t BlockLength() const {
    const size_t kFciLen = 8;
    return kCommonFbFmtLength + kFciLen * tmmbn_items_.size();
  }

  RTCPUtility::RTCPPacketRTPFBTMMBN tmmbn_;
  std::vector<RTCPUtility::RTCPPacketRTPFBTMMBRItem> tmmbn_items_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Tmmbn);
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_TMMBN_H_
