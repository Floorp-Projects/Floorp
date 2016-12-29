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

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_SLI_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_SLI_H_

#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/psfb.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"

namespace webrtc {
namespace rtcp {

// Slice loss indication (SLI) (RFC 4585).
class Sli : public Psfb {
 public:
  static const uint8_t kFeedbackMessageType = 2;
  class Macroblocks {
   public:
    static const size_t kLength = 4;
    Macroblocks() : item_(0) {}
    Macroblocks(uint8_t picture_id, uint16_t first, uint16_t number);
    ~Macroblocks() {}

    void Parse(const uint8_t* buffer);
    void Create(uint8_t* buffer) const;

    uint16_t first() const { return item_ >> 19; }
    uint16_t number() const { return (item_ >> 6) & 0x1fff; }
    uint8_t picture_id() const { return (item_ & 0x3f); }

   private:
    uint32_t item_;
  };

  Sli() {}
  virtual ~Sli() {}

  // Parse assumes header is already parsed and validated.
  bool Parse(const RTCPUtility::RtcpCommonHeader& header,
             const uint8_t* payload);  // Size of the payload is in the header.

  void WithPictureId(uint8_t picture_id,
                     uint16_t first_macroblock = 0,
                     uint16_t number_macroblocks = 0x1fff) {
    items_.push_back(
        Macroblocks(picture_id, first_macroblock, number_macroblocks));
  }

  const std::vector<Macroblocks>& macroblocks() const { return items_; }

 protected:
  bool Create(uint8_t* packet,
              size_t* index,
              size_t max_length,
              RtcpPacket::PacketReadyCallback* callback) const override;

 private:
  size_t BlockLength() const override {
    return RtcpPacket::kHeaderLength + Psfb::kCommonFeedbackLength +
           items_.size() * Macroblocks::kLength;
  }

  std::vector<Macroblocks> items_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Sli);
};

}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_SLI_H_
