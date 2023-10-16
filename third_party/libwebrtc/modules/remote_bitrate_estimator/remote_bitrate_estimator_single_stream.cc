/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"

#include <cstdint>
#include <utility>

#include "absl/types/optional.h"
#include "modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/remote_bitrate_estimator/inter_arrival.h"
#include "modules/remote_bitrate_estimator/overuse_detector.h"
#include "modules/remote_bitrate_estimator/overuse_estimator.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {
namespace {

constexpr int kTimestampGroupLengthMs = 5;
constexpr double kTimestampToMs = 1.0 / 90.0;

absl::optional<DataRate> OptionalRateFromOptionalBps(
    absl::optional<int> bitrate_bps) {
  if (bitrate_bps) {
    return DataRate::BitsPerSec(*bitrate_bps);
  } else {
    return absl::nullopt;
  }
}
}  // namespace

RemoteBitrateEstimatorSingleStream::Detector::Detector()
    : inter_arrival(90 * kTimestampGroupLengthMs, kTimestampToMs) {}

RemoteBitrateEstimatorSingleStream::RemoteBitrateEstimatorSingleStream(
    RemoteBitrateObserver* observer,
    Clock* clock)
    : clock_(clock),
      incoming_bitrate_(kBitrateWindowMs, 8000),
      last_valid_incoming_bitrate_(0),
      remote_rate_(field_trials_),
      observer_(observer),
      last_process_time_(-1),
      process_interval_ms_(kProcessIntervalMs),
      uma_recorded_(false) {
  RTC_LOG(LS_INFO) << "RemoteBitrateEstimatorSingleStream: Instantiating.";
}

RemoteBitrateEstimatorSingleStream::~RemoteBitrateEstimatorSingleStream() =
    default;

void RemoteBitrateEstimatorSingleStream::IncomingPacket(
    const RtpPacketReceived& rtp_packet) {
  absl::optional<int32_t> transmission_time_offset =
      rtp_packet.GetExtension<TransmissionOffset>();
  if (!uma_recorded_) {
    BweNames type = transmission_time_offset.has_value()
                        ? BweNames::kReceiverTOffset
                        : BweNames::kReceiverNoExtension;
    RTC_HISTOGRAM_ENUMERATION(kBweTypeHistogram, type, BweNames::kBweNamesMax);
    uma_recorded_ = true;
  }
  uint32_t ssrc = rtp_packet.Ssrc();
  uint32_t rtp_timestamp =
      rtp_packet.Timestamp() + transmission_time_offset.value_or(0);
  int64_t now_ms = clock_->TimeInMilliseconds();
  Detector& estimator = overuse_detectors_[ssrc];
  estimator.last_packet_time_ms = now_ms;

  // Check if incoming bitrate estimate is valid, and if it needs to be reset.
  absl::optional<uint32_t> incoming_bitrate = incoming_bitrate_.Rate(now_ms);
  if (incoming_bitrate) {
    last_valid_incoming_bitrate_ = *incoming_bitrate;
  } else if (last_valid_incoming_bitrate_ > 0) {
    // Incoming bitrate had a previous valid value, but now not enough data
    // point are left within the current window. Reset incoming bitrate
    // estimator so that the window size will only contain new data points.
    incoming_bitrate_.Reset();
    last_valid_incoming_bitrate_ = 0;
  }
  size_t payload_size = rtp_packet.payload_size() + rtp_packet.padding_size();
  incoming_bitrate_.Update(payload_size, now_ms);

  const BandwidthUsage prior_state = estimator.detector.State();
  uint32_t timestamp_delta = 0;
  int64_t time_delta = 0;
  int size_delta = 0;
  if (estimator.inter_arrival.ComputeDeltas(
          rtp_timestamp, rtp_packet.arrival_time().ms(), now_ms, payload_size,
          &timestamp_delta, &time_delta, &size_delta)) {
    double timestamp_delta_ms = timestamp_delta * kTimestampToMs;
    estimator.estimator.Update(time_delta, timestamp_delta_ms, size_delta,
                               estimator.detector.State(), now_ms);
    estimator.detector.Detect(estimator.estimator.offset(), timestamp_delta_ms,
                              estimator.estimator.num_of_deltas(), now_ms);
  }
  if (estimator.detector.State() == BandwidthUsage::kBwOverusing) {
    absl::optional<uint32_t> incoming_bitrate_bps =
        incoming_bitrate_.Rate(now_ms);
    if (incoming_bitrate_bps &&
        (prior_state != BandwidthUsage::kBwOverusing ||
         remote_rate_.TimeToReduceFurther(
             Timestamp::Millis(now_ms),
             DataRate::BitsPerSec(*incoming_bitrate_bps)))) {
      // The first overuse should immediately trigger a new estimate.
      // We also have to update the estimate immediately if we are overusing
      // and the target bitrate is too high compared to what we are receiving.
      UpdateEstimate(now_ms);
    }
  }
}

TimeDelta RemoteBitrateEstimatorSingleStream::Process() {
  int64_t now_ms = clock_->TimeInMilliseconds();
  int64_t next_process_time_ms = last_process_time_ + process_interval_ms_;
  if (last_process_time_ == -1 || now_ms >= next_process_time_ms) {
    UpdateEstimate(now_ms);
    last_process_time_ = now_ms;
    return TimeDelta::Millis(process_interval_ms_);
  }

  return TimeDelta::Millis(next_process_time_ms - now_ms);
}

void RemoteBitrateEstimatorSingleStream::UpdateEstimate(int64_t now_ms) {
  BandwidthUsage bw_state = BandwidthUsage::kBwNormal;
  auto it = overuse_detectors_.begin();
  while (it != overuse_detectors_.end()) {
    const int64_t time_of_last_received_packet = it->second.last_packet_time_ms;
    if (time_of_last_received_packet >= 0 &&
        now_ms - time_of_last_received_packet > kStreamTimeOutMs) {
      // This over-use detector hasn't received packets for `kStreamTimeOutMs`
      // milliseconds and is considered stale.
      overuse_detectors_.erase(it++);
    } else {
      // Make sure that we trigger an over-use if any of the over-use detectors
      // is detecting over-use.
      if (it->second.detector.State() > bw_state) {
        bw_state = it->second.detector.State();
      }
      ++it;
    }
  }
  // We can't update the estimate if we don't have any active streams.
  if (overuse_detectors_.empty()) {
    return;
  }

  const RateControlInput input(
      bw_state, OptionalRateFromOptionalBps(incoming_bitrate_.Rate(now_ms)));
  uint32_t target_bitrate =
      remote_rate_.Update(input, Timestamp::Millis(now_ms)).bps<uint32_t>();
  if (remote_rate_.ValidEstimate()) {
    process_interval_ms_ = remote_rate_.GetFeedbackInterval().ms();
    RTC_DCHECK_GT(process_interval_ms_, 0);
    if (observer_)
      observer_->OnReceiveBitrateChanged(GetSsrcs(), target_bitrate);
  }
}

void RemoteBitrateEstimatorSingleStream::OnRttUpdate(int64_t avg_rtt_ms,
                                                     int64_t max_rtt_ms) {
  remote_rate_.SetRtt(TimeDelta::Millis(avg_rtt_ms));
}

void RemoteBitrateEstimatorSingleStream::RemoveStream(uint32_t ssrc) {
  overuse_detectors_.erase(ssrc);
}

DataRate RemoteBitrateEstimatorSingleStream::LatestEstimate() const {
  if (!remote_rate_.ValidEstimate() || overuse_detectors_.empty()) {
    return DataRate::Zero();
  }
  return remote_rate_.LatestEstimate();
}

std::vector<uint32_t> RemoteBitrateEstimatorSingleStream::GetSsrcs() const {
  std::vector<uint32_t> ssrcs;
  ssrcs.reserve(overuse_detectors_.size());
  for (const auto& [ssrc, unused] : overuse_detectors_) {
    ssrcs.push_back(ssrc);
  }
  return ssrcs;
}

}  // namespace webrtc
