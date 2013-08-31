/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVER_FEC_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVER_FEC_H_

// This header is included to get the nested declaration of Packet structure.

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class RTPReceiverVideo;

class ReceiverFEC {
 public:
  ReceiverFEC(const int32_t id, RTPReceiverVideo* owner);
  virtual ~ReceiverFEC();

  int32_t AddReceivedFECPacket(const WebRtcRTPHeader* rtp_header,
                               const uint8_t* incoming_rtp_packet,
                               const uint16_t payload_data_length,
                               bool& FECpacket);

  int32_t ProcessReceivedFEC();

  void SetPayloadTypeFEC(const int8_t payload_type);

 private:
  int ParseAndReceivePacket(const ForwardErrorCorrection::Packet* packet);

  int id_;
  RTPReceiverVideo* owner_;
  ForwardErrorCorrection* fec_;
  // TODO(holmer): In the current version received_packet_list_ is never more
  // than one packet, since we process FEC every time a new packet
  // arrives. We should remove the list.
  ForwardErrorCorrection::ReceivedPacketList received_packet_list_;
  ForwardErrorCorrection::RecoveredPacketList recovered_packet_list_;
  int8_t payload_type_fec_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RECEIVER_FEC_H_
