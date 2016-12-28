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

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_DLRR_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_DLRR_H_

#include <vector>

#include "webrtc/base/basictypes.h"

namespace webrtc {
namespace rtcp {

// DLRR Report Block: Delay since the Last Receiver Report (RFC 3611).
class Dlrr {
 public:
  struct SubBlock {
    // RFC 3611 4.5
    uint32_t ssrc;
    uint32_t last_rr;
    uint32_t delay_since_last_rr;
  };

  static const uint8_t kBlockType = 5;
  static const size_t kMaxNumberOfDlrrItems = 100;

  Dlrr() {}
  Dlrr(const Dlrr& other) = default;
  ~Dlrr() {}

  Dlrr& operator=(const Dlrr& other) = default;

  // Second parameter is value read from block header,
  // i.e. size of block in 32bits excluding block header itself.
  bool Parse(const uint8_t* buffer, uint16_t block_length_32bits);

  size_t BlockLength() const;
  // Fills buffer with the Dlrr.
  // Consumes BlockLength() bytes.
  void Create(uint8_t* buffer) const;

  // Max 100 DLRR Items can be added per DLRR report block.
  bool WithDlrrItem(uint32_t ssrc, uint32_t last_rr, uint32_t delay_last_rr);

  const std::vector<SubBlock>& sub_blocks() const { return sub_blocks_; }

 private:
  static const size_t kBlockHeaderLength = 4;
  static const size_t kSubBlockLength = 12;

  std::vector<SubBlock> sub_blocks_;
};
}  // namespace rtcp
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_PACKET_DLRR_H_
