/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_INTERFACE_FEC_RECEIVER_H_
#define WEBRTC_MODULES_RTP_RTCP_INTERFACE_FEC_RECEIVER_H_

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class FecReceiver {
 public:
  static FecReceiver* Create(int32_t id, RtpData* callback);

  virtual ~FecReceiver() {}

  virtual int32_t AddReceivedRedPacket(const RTPHeader& rtp_header,
                                       const uint8_t* incoming_rtp_packet,
                                       int packet_length,
                                       uint8_t ulpfec_payload_type) = 0;

  virtual int32_t ProcessReceivedFec() = 0;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_INTERFACE_FEC_RECEIVER_H_
