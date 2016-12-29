/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/compound_packet.h"

namespace webrtc {
namespace rtcp {

bool CompoundPacket::Create(uint8_t* packet,
                            size_t* index,
                            size_t max_length,
                            RtcpPacket::PacketReadyCallback* callback) const {
  return true;
}

size_t CompoundPacket::BlockLength() const {
  return 0;
}

}  // namespace rtcp
}  // namespace webrtc
