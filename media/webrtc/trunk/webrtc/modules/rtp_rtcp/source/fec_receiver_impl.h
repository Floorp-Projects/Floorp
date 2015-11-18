/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_FEC_RECEIVER_IMPL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_FEC_RECEIVER_IMPL_H_

// This header is included to get the nested declaration of Packet structure.

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/rtp_rtcp/interface/fec_receiver.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;

class FecReceiverImpl : public FecReceiver {
 public:
  FecReceiverImpl(RtpData* callback);
  virtual ~FecReceiverImpl();

  int32_t AddReceivedRedPacket(const RTPHeader& rtp_header,
                               const uint8_t* incoming_rtp_packet,
                               size_t packet_length,
                               uint8_t ulpfec_payload_type) override;

  int32_t ProcessReceivedFec() override;

  FecPacketCounter GetPacketCounter() const override;

 private:
  rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;
  RtpData* recovered_packet_callback_;
  ForwardErrorCorrection* fec_;
  // TODO(holmer): In the current version received_packet_list_ is never more
  // than one packet, since we process FEC every time a new packet
  // arrives. We should remove the list.
  ForwardErrorCorrection::ReceivedPacketList received_packet_list_;
  ForwardErrorCorrection::RecoveredPacketList recovered_packet_list_;
  FecPacketCounter packet_counter_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_FEC_RECEIVER_IMPL_H_
