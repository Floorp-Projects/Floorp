/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/nada.h"

#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"

namespace webrtc {
namespace testing {
namespace bwe {

NadaBweReceiver::NadaBweReceiver(int flow_id)
    : BweReceiver(flow_id),
      clock_(0),
      last_feedback_ms_(0),
      recv_stats_(ReceiveStatistics::Create(&clock_)),
      baseline_delay_ms_(0),
      delay_signal_ms_(0),
      last_congestion_signal_ms_(0) {
}

NadaBweReceiver::~NadaBweReceiver() {
}

void NadaBweReceiver::ReceivePacket(int64_t arrival_time_ms,
                                    const MediaPacket& media_packet) {
  clock_.AdvanceTimeMilliseconds(arrival_time_ms - clock_.TimeInMilliseconds());
  recv_stats_->IncomingPacket(media_packet.header(),
                              media_packet.payload_size(), false);
  int64_t delay_ms = arrival_time_ms - media_packet.creation_time_us() / 1000;
  // TODO(holmer): The min should time out after 10 minutes.
  if (delay_ms < baseline_delay_ms_) {
    baseline_delay_ms_ = delay_ms;
  }
  delay_signal_ms_ = delay_ms - baseline_delay_ms_;
}

FeedbackPacket* NadaBweReceiver::GetFeedback(int64_t now_ms) {
  if (now_ms - last_feedback_ms_ < 100)
    return NULL;

  StatisticianMap statisticians = recv_stats_->GetActiveStatisticians();
  int64_t loss_signal_ms = 0.0f;
  if (!statisticians.empty()) {
    RtcpStatistics stats;
    if (!statisticians.begin()->second->GetStatistics(&stats, true)) {
      const float kLossSignalWeight = 1000.0f;
      loss_signal_ms =
          (kLossSignalWeight * static_cast<float>(stats.fraction_lost) + 127) /
          255;
    }
  }

  int64_t congestion_signal_ms = delay_signal_ms_ + loss_signal_ms;

  float derivative = 0.0f;
  if (last_feedback_ms_ > 0) {
    derivative = (congestion_signal_ms - last_congestion_signal_ms_) /
                 static_cast<float>(now_ms - last_feedback_ms_);
  }
  last_feedback_ms_ = now_ms;
  last_congestion_signal_ms_ = congestion_signal_ms;
  return new NadaFeedback(flow_id_, now_ms, congestion_signal_ms, derivative);
}

NadaBweSender::NadaBweSender(int kbps, BitrateObserver* observer, Clock* clock)
    : clock_(clock),
      observer_(observer),
      bitrate_kbps_(kbps),
      last_feedback_ms_(0) {
}

NadaBweSender::~NadaBweSender() {
}

int NadaBweSender::GetFeedbackIntervalMs() const {
  return 100;
}

void NadaBweSender::GiveFeedback(const FeedbackPacket& feedback) {
  const NadaFeedback& fb = static_cast<const NadaFeedback&>(feedback);

  // TODO(holmer): Implement special start-up behavior.

  const float kEta = 2.0f;
  const float kTaoO = 500.0f;
  float x_hat = fb.congestion_signal() + kEta * kTaoO * fb.derivative();

  int64_t now_ms = clock_->TimeInMilliseconds();
  float delta_s = now_ms - last_feedback_ms_;
  last_feedback_ms_ = now_ms;

  const float kPriorityWeight = 1.0f;
  const float kReferenceDelayS = 10.0f;
  float kTheta =
      kPriorityWeight * (kMaxBitrateKbps - kMinBitrateKbps) * kReferenceDelayS;

  const float kKappa = 1.0f;
  bitrate_kbps_ = bitrate_kbps_ +
                  kKappa * delta_s / (kTaoO * kTaoO) *
                      (kTheta - (bitrate_kbps_ - kMinBitrateKbps) * x_hat) +
                  0.5f;
  bitrate_kbps_ = std::min(bitrate_kbps_, kMaxBitrateKbps);
  bitrate_kbps_ = std::max(bitrate_kbps_, kMinBitrateKbps);

  observer_->OnNetworkChanged(1000 * bitrate_kbps_, 0, 0);
}

int64_t NadaBweSender::TimeUntilNextProcess() {
  return 100;
}

int NadaBweSender::Process() {
  return 0;
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
