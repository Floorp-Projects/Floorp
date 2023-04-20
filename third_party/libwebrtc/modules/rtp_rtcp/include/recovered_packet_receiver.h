/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_INCLUDE_RECOVERED_PACKET_RECEIVER_H_
#define MODULES_RTP_RTCP_INCLUDE_RECOVERED_PACKET_RECEIVER_H_

#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"

namespace webrtc {

// Callback interface for packets recovered by FlexFEC or ULPFEC. In
// the FlexFEC case, the implementation should be able to demultiplex
// the recovered RTP packets based on SSRC.
class RecoveredPacketReceiver {
 public:
  // TODO(bugs.webrtc.org/7135,perkj): Remove when all
  // implementations implement OnRecoveredPacket(const RtpPacketReceived&)
  virtual void OnRecoveredPacket(const uint8_t* packet, size_t length) {
    RTC_DCHECK_NOTREACHED();
  }
  // TODO(bugs.webrtc.org/7135,perkj): Make pure virtual when all
  // implementations are updated.
  virtual void OnRecoveredPacket(const RtpPacketReceived& packet) {
    OnRecoveredPacket(packet.Buffer().data(), packet.Buffer().size());
  }

 protected:
  virtual ~RecoveredPacketReceiver() = default;
};

}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_INCLUDE_RECOVERED_PACKET_RECEIVER_H_
