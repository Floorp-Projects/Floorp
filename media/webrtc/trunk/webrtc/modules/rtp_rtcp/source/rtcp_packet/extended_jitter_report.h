/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_JITTER_REPORT_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_JITTER_REPORT_H_

#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

namespace webrtc {
namespace rtcp {

class ExtendedJitterReport : public RtcpPacket {
 public:
  static const uint8_t kPacketType = 195;

  ExtendedJitterReport() : RtcpPacket() {}

  virtual ~ExtendedJitterReport() {}

  // Parse assumes header is already parsed and validated.
  bool Parse(const RTCPUtility::RtcpCommonHeader& header,
             const uint8_t* payload);  // Size of the payload is in the header.

  bool WithJitter(uint32_t jitter);

  size_t jitters_count() const { return inter_arrival_jitters_.size(); }
  uint32_t jitter(size_t index) const {
    RTC_DCHECK_LT(index, jitters_count());
    return inter_arrival_jitters_[index];
  }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  static const int kMaxNumberOfJitters = 0x1f;

  size_t BlockLength() const override {
    return kHeaderLength + 4 * inter_arrival_jitters_.size();
  }

  std::vector<uint32_t> inter_arrival_jitters_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ExtendedJitterReport);
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_EXTENDED_JITTER_REPORT_H_
