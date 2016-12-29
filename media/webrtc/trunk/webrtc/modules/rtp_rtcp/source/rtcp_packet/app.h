/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_APP_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_APP_H_

#include "webrtc/base/buffer.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

namespace webrtc {
namespace rtcp {

class App : public RtcpPacket {
 public:
  static const uint8_t kPacketType = 204;
  // 28 bytes for UDP header
  // 12 bytes for RTCP app header
  static const size_t kMaxDataSize = IP_PACKET_SIZE - 12 - 28;
  App() : sub_type_(0), ssrc_(0), name_(0) {}

  virtual ~App() {}

  // Parse assumes header is already parsed and validated.
  bool Parse(const RTCPUtility::RtcpCommonHeader& header,
             const uint8_t* payload);  // Size of the payload is in the header.

  void From(uint32_t ssrc) { ssrc_ = ssrc; }
  void WithSubType(uint8_t subtype);
  void WithName(uint32_t name) { name_ = name; }
  void WithData(const uint8_t* data, size_t data_length);

  uint8_t sub_type() const { return sub_type_; }
  uint32_t ssrc() const { return ssrc_; }
  uint32_t name() const { return name_; }
  size_t data_size() const { return data_.size(); }
  const uint8_t* data() const { return data_.data(); }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  size_t BlockLength() const override { return 12 + data_.size(); }

  uint8_t sub_type_;
  uint32_t ssrc_;
  uint32_t name_;
  rtc::Buffer data_;

  RTC_DISALLOW_COPY_AND_ASSIGN(App);
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_APP_H_
