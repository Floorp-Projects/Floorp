/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/send_side.h"

#include "webrtc/base/logging.h"

namespace webrtc {
namespace testing {
namespace bwe {

FullBweSender::FullBweSender(int kbps, BitrateObserver* observer, Clock* clock)
    : bitrate_controller_(
          BitrateController::CreateBitrateController(clock, observer)),
      rbe_(AbsoluteSendTimeRemoteBitrateEstimatorFactory()
               .Create(this, clock, kAimdControl, 1000 * kMinBitrateKbps)),
      feedback_observer_(bitrate_controller_->CreateRtcpBandwidthObserver()),
      clock_(clock),
      send_time_history_(10000) {
  assert(kbps >= kMinBitrateKbps);
  assert(kbps <= kMaxBitrateKbps);
  bitrate_controller_->SetStartBitrate(1000 * kbps);
  bitrate_controller_->SetMinMaxBitrate(1000 * kMinBitrateKbps,
                                        1000 * kMaxBitrateKbps);
}

FullBweSender::~FullBweSender() {
}

int FullBweSender::GetFeedbackIntervalMs() const {
  return 100;
}

void FullBweSender::GiveFeedback(const FeedbackPacket& feedback) {
  const SendSideBweFeedback& fb =
      static_cast<const SendSideBweFeedback&>(feedback);
  if (fb.packet_feedback_vector().empty())
    return;
  // TODO(sprang): Unconstify PacketInfo so we don't need temp copy?
  std::vector<PacketInfo> packet_feedback_vector(fb.packet_feedback_vector());
  for (PacketInfo& packet : packet_feedback_vector) {
    if (!send_time_history_.GetSendTime(packet.sequence_number,
                                        &packet.send_time_ms, true)) {
      LOG(LS_WARNING) << "Ack arrived too late.";
    }
  }
  rbe_->IncomingPacketFeedbackVector(packet_feedback_vector);
  // TODO(holmer): Handle losses in between feedback packets.
  int expected_packets = fb.packet_feedback_vector().back().sequence_number -
                         fb.packet_feedback_vector().front().sequence_number +
                         1;
  // Assuming no reordering for now.
  if (expected_packets <= 0)
    return;
  int lost_packets = expected_packets - fb.packet_feedback_vector().size();
  report_block_.fractionLost = (lost_packets << 8) / expected_packets;
  report_block_.cumulativeLost += lost_packets;
  ReportBlockList report_blocks;
  report_blocks.push_back(report_block_);
  feedback_observer_->OnReceivedRtcpReceiverReport(
      report_blocks, 0, clock_->TimeInMilliseconds());
  bitrate_controller_->Process();
}

void FullBweSender::OnPacketsSent(const Packets& packets) {
  for (Packet* packet : packets) {
    if (packet->GetPacketType() == Packet::kMedia) {
      MediaPacket* media_packet = static_cast<MediaPacket*>(packet);
      send_time_history_.AddAndRemoveOldSendTimes(
          media_packet->header().sequenceNumber,
          media_packet->GetAbsSendTimeInMs());
    }
  }
}

void FullBweSender::OnReceiveBitrateChanged(
    const std::vector<unsigned int>& ssrcs,
    unsigned int bitrate) {
  feedback_observer_->OnReceivedEstimatedBitrate(bitrate);
}

int64_t FullBweSender::TimeUntilNextProcess() {
  return bitrate_controller_->TimeUntilNextProcess();
}

int FullBweSender::Process() {
  rbe_->Process();
  return bitrate_controller_->Process();
}

SendSideBweReceiver::SendSideBweReceiver(int flow_id)
    : BweReceiver(flow_id), last_feedback_ms_(0) {
}

SendSideBweReceiver::~SendSideBweReceiver() {
}

void SendSideBweReceiver::ReceivePacket(int64_t arrival_time_ms,
                                        const MediaPacket& media_packet) {
  packet_feedback_vector_.push_back(PacketInfo(
      arrival_time_ms,
      GetAbsSendTimeInMs(media_packet.header().extension.absoluteSendTime),
      media_packet.header().sequenceNumber, media_packet.payload_size()));
}

FeedbackPacket* SendSideBweReceiver::GetFeedback(int64_t now_ms) {
  if (now_ms - last_feedback_ms_ < 100)
    return NULL;
  last_feedback_ms_ = now_ms;
  FeedbackPacket* fb =
      new SendSideBweFeedback(flow_id_, now_ms * 1000, packet_feedback_vector_);
  packet_feedback_vector_.clear();
  return fb;
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
