/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/remote_estimator_proxy.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"

namespace webrtc {

// TODO(sprang): Tune these!
const int RemoteEstimatorProxy::kDefaultProcessIntervalMs = 50;
const int RemoteEstimatorProxy::kBackWindowMs = 500;

RemoteEstimatorProxy::RemoteEstimatorProxy(Clock* clock,
                                           PacketRouter* packet_router)
    : clock_(clock),
      packet_router_(packet_router),
      last_process_time_ms_(-1),
      media_ssrc_(0),
      feedback_sequence_(0),
      window_start_seq_(-1) {}

RemoteEstimatorProxy::~RemoteEstimatorProxy() {}

void RemoteEstimatorProxy::IncomingPacketFeedbackVector(
    const std::vector<PacketInfo>& packet_feedback_vector) {
  rtc::CritScope cs(&lock_);
  for (PacketInfo info : packet_feedback_vector)
    OnPacketArrival(info.sequence_number, info.arrival_time_ms);
}

void RemoteEstimatorProxy::IncomingPacket(int64_t arrival_time_ms,
                                          size_t payload_size,
                                          const RTPHeader& header,
                                          bool was_paced) {
  if (!header.extension.hasTransportSequenceNumber) {
    LOG(LS_WARNING) << "RemoteEstimatorProxy: Incoming packet "
                       "is missing the transport sequence number extension!";
    return;
  }
  rtc::CritScope cs(&lock_);
  media_ssrc_ = header.ssrc;
  OnPacketArrival(header.extension.transportSequenceNumber, arrival_time_ms);
}

void RemoteEstimatorProxy::RemoveStream(unsigned int ssrc) {}

bool RemoteEstimatorProxy::LatestEstimate(std::vector<unsigned int>* ssrcs,
                                          unsigned int* bitrate_bps) const {
  return false;
}

bool RemoteEstimatorProxy::GetStats(
    ReceiveBandwidthEstimatorStats* output) const {
  return false;
}


int64_t RemoteEstimatorProxy::TimeUntilNextProcess() {
  int64_t now = clock_->TimeInMilliseconds();
  int64_t time_until_next = 0;
  if (last_process_time_ms_ != -1 &&
      now - last_process_time_ms_ < kDefaultProcessIntervalMs) {
    time_until_next = (last_process_time_ms_ + kDefaultProcessIntervalMs - now);
  }
  return time_until_next;
}

int32_t RemoteEstimatorProxy::Process() {
  // TODO(sprang): Perhaps we need a dedicated thread here instead?

  if (TimeUntilNextProcess() > 0)
    return 0;
  last_process_time_ms_ = clock_->TimeInMilliseconds();

  bool more_to_build = true;
  while (more_to_build) {
    rtcp::TransportFeedback feedback_packet;
    if (BuildFeedbackPacket(&feedback_packet)) {
      RTC_DCHECK(packet_router_ != nullptr);
      packet_router_->SendFeedback(&feedback_packet);
    } else {
      more_to_build = false;
    }
  }

  return 0;
}

void RemoteEstimatorProxy::OnPacketArrival(uint16_t sequence_number,
                                           int64_t arrival_time) {
  int64_t seq = unwrapper_.Unwrap(sequence_number);

  if (window_start_seq_ == -1) {
    window_start_seq_ = seq;
    // Start new feedback packet, cull old packets.
    for (auto it = packet_arrival_times_.begin();
         it != packet_arrival_times_.end() && it->first < seq &&
         arrival_time - it->second >= kBackWindowMs;) {
      auto delete_it = it;
      ++it;
      packet_arrival_times_.erase(delete_it);
    }
  } else if (seq < window_start_seq_) {
    window_start_seq_ = seq;
  }

  RTC_DCHECK(packet_arrival_times_.end() == packet_arrival_times_.find(seq));
  packet_arrival_times_[seq] = arrival_time;
}

bool RemoteEstimatorProxy::BuildFeedbackPacket(
    rtcp::TransportFeedback* feedback_packet) {
  rtc::CritScope cs(&lock_);
  if (window_start_seq_ == -1)
    return false;

  // window_start_seq_ is the first sequence number to include in the current
  // feedback packet. Some older may still be in the map, in case a reordering
  // happens and we need to retransmit them.
  auto it = packet_arrival_times_.find(window_start_seq_);
  RTC_DCHECK(it != packet_arrival_times_.end());

  // TODO(sprang): Measure receive times in microseconds and remove the
  // conversions below.
  feedback_packet->WithMediaSourceSsrc(media_ssrc_);
  feedback_packet->WithBase(static_cast<uint16_t>(it->first & 0xFFFF),
                            it->second * 1000);
  feedback_packet->WithFeedbackSequenceNumber(feedback_sequence_++);
  for (; it != packet_arrival_times_.end(); ++it) {
    if (!feedback_packet->WithReceivedPacket(
            static_cast<uint16_t>(it->first & 0xFFFF), it->second * 1000)) {
      // If we can't even add the first seq to the feedback packet, we won't be
      // able to build it at all.
      RTC_CHECK_NE(window_start_seq_, it->first);

      // Could not add timestamp, feedback packet might be full. Return and
      // try again with a fresh packet.
      window_start_seq_ = it->first;
      break;
    }
    // Note: Don't erase items from packet_arrival_times_ after sending, in case
    // they need to be re-sent after a reordering. Removal will be handled
    // by OnPacketArrival once packets are too old.
  }
  if (it == packet_arrival_times_.end())
    window_start_seq_ = -1;

  return true;
}

}  // namespace webrtc
