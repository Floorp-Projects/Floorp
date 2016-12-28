/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/packet_router.h"

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"

namespace webrtc {

PacketRouter::PacketRouter() : transport_seq_(0) {
}

PacketRouter::~PacketRouter() {
  RTC_DCHECK(rtp_modules_.empty());
}

void PacketRouter::AddRtpModule(RtpRtcp* rtp_module) {
  rtc::CritScope cs(&modules_lock_);
  RTC_DCHECK(std::find(rtp_modules_.begin(), rtp_modules_.end(), rtp_module) ==
             rtp_modules_.end());
  rtp_modules_.push_back(rtp_module);
}

void PacketRouter::RemoveRtpModule(RtpRtcp* rtp_module) {
  rtc::CritScope cs(&modules_lock_);
  auto it = std::find(rtp_modules_.begin(), rtp_modules_.end(), rtp_module);
  RTC_DCHECK(it != rtp_modules_.end());
  rtp_modules_.erase(it);
}

bool PacketRouter::TimeToSendPacket(uint32_t ssrc,
                                    uint16_t sequence_number,
                                    int64_t capture_timestamp,
                                    bool retransmission) {
  rtc::CritScope cs(&modules_lock_);
  for (auto* rtp_module : rtp_modules_) {
    if (rtp_module->SendingMedia() && ssrc == rtp_module->SSRC()) {
      return rtp_module->TimeToSendPacket(ssrc, sequence_number,
                                          capture_timestamp, retransmission);
    }
  }
  return true;
}

size_t PacketRouter::TimeToSendPadding(size_t bytes_to_send) {
  size_t total_bytes_sent = 0;
  rtc::CritScope cs(&modules_lock_);
  for (RtpRtcp* module : rtp_modules_) {
    if (module->SendingMedia()) {
      size_t bytes_sent =
          module->TimeToSendPadding(bytes_to_send - total_bytes_sent);
      total_bytes_sent += bytes_sent;
      if (total_bytes_sent >= bytes_to_send)
        break;
    }
  }
  return total_bytes_sent;
}

void PacketRouter::SetTransportWideSequenceNumber(uint16_t sequence_number) {
  rtc::AtomicOps::ReleaseStore(&transport_seq_, sequence_number);
}

uint16_t PacketRouter::AllocateSequenceNumber() {
  int prev_seq = rtc::AtomicOps::AcquireLoad(&transport_seq_);
  int desired_prev_seq;
  int new_seq;
  do {
    desired_prev_seq = prev_seq;
    new_seq = (desired_prev_seq + 1) & 0xFFFF;
    // Note: CompareAndSwap returns the actual value of transport_seq at the
    // time the CAS operation was executed. Thus, if prev_seq is returned, the
    // operation was successful - otherwise we need to retry. Saving the
    // return value saves us a load on retry.
    prev_seq = rtc::AtomicOps::CompareAndSwap(&transport_seq_, desired_prev_seq,
                                              new_seq);
  } while (prev_seq != desired_prev_seq);

  return new_seq;
}

bool PacketRouter::SendFeedback(rtcp::TransportFeedback* packet) {
  rtc::CritScope cs(&modules_lock_);
  for (auto* rtp_module : rtp_modules_) {
    packet->WithPacketSenderSsrc(rtp_module->SSRC());
    if (rtp_module->SendFeedbackPacket(*packet))
      return true;
  }
  return false;
}

}  // namespace webrtc
