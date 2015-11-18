/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/bitrate_controller/send_side_bandwidth_estimation.h"

#include <cmath>

#include "webrtc/base/checks.h"
#include "webrtc/system_wrappers/interface/field_trial.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/metrics.h"

namespace webrtc {
namespace {
const int64_t kBweIncreaseIntervalMs = 1000;
const int64_t kBweDecreaseIntervalMs = 300;
const int64_t kStartPhaseMs = 2000;
const int64_t kBweConverganceTimeMs = 20000;
const int kLimitNumPackets = 20;
const int kAvgPacketSizeBytes = 1000;
const int kDefaultMinBitrateBps = 10000;
const int kDefaultMaxBitrateBps = 1000000000;

struct UmaRampUpMetric {
  const char* metric_name;
  int bitrate_kbps;
};

const UmaRampUpMetric kUmaRampupMetrics[] = {
    {"WebRTC.BWE.RampUpTimeTo500kbpsInMs", 500},
    {"WebRTC.BWE.RampUpTimeTo1000kbpsInMs", 1000},
    {"WebRTC.BWE.RampUpTimeTo2000kbpsInMs", 2000}};
const size_t kNumUmaRampupMetrics =
    sizeof(kUmaRampupMetrics) / sizeof(kUmaRampupMetrics[0]);

// Calculate the rate that TCP-Friendly Rate Control (TFRC) would apply.
// The formula in RFC 3448, Section 3.1, is used.
uint32_t CalcTfrcBps(int64_t rtt, uint8_t loss) {
  if (rtt == 0 || loss == 0) {
    // Input variables out of range.
    return 0;
  }
  double R = static_cast<double>(rtt) / 1000;  // RTT in seconds.
  int b = 1;  // Number of packets acknowledged by a single TCP acknowledgement:
              // recommended = 1.
  double t_RTO = 4.0 * R;  // TCP retransmission timeout value in seconds
                           // recommended = 4*R.
  double p = static_cast<double>(loss) / 255;  // Packet loss rate in [0, 1).
  double s = static_cast<double>(kAvgPacketSizeBytes);

  // Calculate send rate in bytes/second.
  double X =
      s / (R * std::sqrt(2 * b * p / 3) +
           (t_RTO * (3 * std::sqrt(3 * b * p / 8) * p * (1 + 32 * p * p))));

  // Convert to bits/second.
  return (static_cast<uint32_t>(X * 8));
}
}

SendSideBandwidthEstimation::SendSideBandwidthEstimation()
    : accumulate_lost_packets_Q8_(0),
      accumulate_expected_packets_(0),
      bitrate_(0),
      min_bitrate_configured_(kDefaultMinBitrateBps),
      max_bitrate_configured_(kDefaultMaxBitrateBps),
      time_last_receiver_block_ms_(0),
      last_fraction_loss_(0),
      last_round_trip_time_ms_(0),
      bwe_incoming_(0),
      time_last_decrease_ms_(0),
      first_report_time_ms_(-1),
      initially_lost_packets_(0),
      bitrate_at_2_seconds_kbps_(0),
      uma_update_state_(kNoUpdate),
      rampup_uma_stats_updated_(kNumUmaRampupMetrics, false) {
}

SendSideBandwidthEstimation::~SendSideBandwidthEstimation() {}

void SendSideBandwidthEstimation::SetSendBitrate(int bitrate) {
  DCHECK_GT(bitrate, 0);
  bitrate_ = bitrate;

  // Clear last sent bitrate history so the new value can be used directly
  // and not capped.
  min_bitrate_history_.clear();
}

void SendSideBandwidthEstimation::SetMinMaxBitrate(int min_bitrate,
                                                   int max_bitrate) {
  DCHECK_GE(min_bitrate, 0);
  min_bitrate_configured_ = std::max(min_bitrate, kDefaultMinBitrateBps);
  if (max_bitrate > 0) {
    max_bitrate_configured_ =
        std::max<uint32_t>(min_bitrate_configured_, max_bitrate);
  } else {
    max_bitrate_configured_ = kDefaultMaxBitrateBps;
  }
}

int SendSideBandwidthEstimation::GetMinBitrate() const {
  return min_bitrate_configured_;
}

void SendSideBandwidthEstimation::CurrentEstimate(int* bitrate,
                                                  uint8_t* loss,
                                                  int64_t* rtt) const {
  *bitrate = bitrate_;
  *loss = last_fraction_loss_;
  *rtt = last_round_trip_time_ms_;
}

void SendSideBandwidthEstimation::UpdateReceiverEstimate(uint32_t bandwidth) {
  bwe_incoming_ = bandwidth;
  bitrate_ = CapBitrateToThresholds(bitrate_);
}

void SendSideBandwidthEstimation::UpdateReceiverBlock(uint8_t fraction_loss,
                                                      int64_t rtt,
                                                      int number_of_packets,
                                                      int64_t now_ms) {
  if (first_report_time_ms_ == -1)
    first_report_time_ms_ = now_ms;

  // Update RTT.
  last_round_trip_time_ms_ = rtt;

  // Check sequence number diff and weight loss report
  if (number_of_packets > 0) {
    // Calculate number of lost packets.
    const int num_lost_packets_Q8 = fraction_loss * number_of_packets;
    // Accumulate reports.
    accumulate_lost_packets_Q8_ += num_lost_packets_Q8;
    accumulate_expected_packets_ += number_of_packets;

    // Report loss if the total report is based on sufficiently many packets.
    if (accumulate_expected_packets_ >= kLimitNumPackets) {
      last_fraction_loss_ =
          accumulate_lost_packets_Q8_ / accumulate_expected_packets_;

      // Reset accumulators.
      accumulate_lost_packets_Q8_ = 0;
      accumulate_expected_packets_ = 0;
    } else {
      // Early return without updating estimate.
      return;
    }
  }
  time_last_receiver_block_ms_ = now_ms;
  UpdateEstimate(now_ms);
  UpdateUmaStats(now_ms, rtt, (fraction_loss * number_of_packets) >> 8);
}

void SendSideBandwidthEstimation::UpdateUmaStats(int64_t now_ms,
                                                 int64_t rtt,
                                                 int lost_packets) {
  int bitrate_kbps = static_cast<int>((bitrate_ + 500) / 1000);
  for (size_t i = 0; i < kNumUmaRampupMetrics; ++i) {
    if (!rampup_uma_stats_updated_[i] &&
        bitrate_kbps >= kUmaRampupMetrics[i].bitrate_kbps) {
      RTC_HISTOGRAM_COUNTS_100000(kUmaRampupMetrics[i].metric_name,
                                  now_ms - first_report_time_ms_);
      rampup_uma_stats_updated_[i] = true;
    }
  }
  if (IsInStartPhase(now_ms)) {
    initially_lost_packets_ += lost_packets;
  } else if (uma_update_state_ == kNoUpdate) {
    uma_update_state_ = kFirstDone;
    bitrate_at_2_seconds_kbps_ = bitrate_kbps;
    RTC_HISTOGRAM_COUNTS(
        "WebRTC.BWE.InitiallyLostPackets", initially_lost_packets_, 0, 100, 50);
    RTC_HISTOGRAM_COUNTS(
        "WebRTC.BWE.InitialRtt", static_cast<int>(rtt), 0, 2000, 50);
    RTC_HISTOGRAM_COUNTS("WebRTC.BWE.InitialBandwidthEstimate",
                         bitrate_at_2_seconds_kbps_,
                         0,
                         2000,
                         50);
  } else if (uma_update_state_ == kFirstDone &&
             now_ms - first_report_time_ms_ >= kBweConverganceTimeMs) {
    uma_update_state_ = kDone;
    int bitrate_diff_kbps =
        std::max(bitrate_at_2_seconds_kbps_ - bitrate_kbps, 0);
    RTC_HISTOGRAM_COUNTS(
        "WebRTC.BWE.InitialVsConvergedDiff", bitrate_diff_kbps, 0, 2000, 50);
  }
}

void SendSideBandwidthEstimation::UpdateEstimate(int64_t now_ms) {
  // We trust the REMB during the first 2 seconds if we haven't had any
  // packet loss reported, to allow startup bitrate probing.
  if (last_fraction_loss_ == 0 && IsInStartPhase(now_ms) &&
      bwe_incoming_ > bitrate_) {
    bitrate_ = CapBitrateToThresholds(bwe_incoming_);
    min_bitrate_history_.clear();
    min_bitrate_history_.push_back(std::make_pair(now_ms, bitrate_));
    return;
  }
  UpdateMinHistory(now_ms);
  // Only start updating bitrate when receiving receiver blocks.
  if (time_last_receiver_block_ms_ != 0) {
    if (last_fraction_loss_ <= 5) {
      // Loss < 2%: Increase rate by 8% of the min bitrate in the last
      // kBweIncreaseIntervalMs.
      // Note that by remembering the bitrate over the last second one can
      // rampup up one second faster than if only allowed to start ramping
      // at 8% per second rate now. E.g.:
      //   If sending a constant 100kbps it can rampup immediatly to 108kbps
      //   whenever a receiver report is received with lower packet loss.
      //   If instead one would do: bitrate_ *= 1.08^(delta time), it would
      //   take over one second since the lower packet loss to achieve 108kbps.
      bitrate_ = static_cast<uint32_t>(
          min_bitrate_history_.front().second * 1.08 + 0.5);

      // Add 1 kbps extra, just to make sure that we do not get stuck
      // (gives a little extra increase at low rates, negligible at higher
      // rates).
      bitrate_ += 1000;

    } else if (last_fraction_loss_ <= 26) {
      // Loss between 2% - 10%: Do nothing.

    } else {
      // Loss > 10%: Limit the rate decreases to once a kBweDecreaseIntervalMs +
      // rtt.
      if ((now_ms - time_last_decrease_ms_) >=
          (kBweDecreaseIntervalMs + last_round_trip_time_ms_)) {
        time_last_decrease_ms_ = now_ms;

        // Reduce rate:
        //   newRate = rate * (1 - 0.5*lossRate);
        //   where packetLoss = 256*lossRate;
        bitrate_ = static_cast<uint32_t>(
            (bitrate_ * static_cast<double>(512 - last_fraction_loss_)) /
            512.0);

        // Calculate what rate TFRC would apply in this situation and to not
        // reduce further than it.
        bitrate_ = std::max(
            bitrate_,
            CalcTfrcBps(last_round_trip_time_ms_, last_fraction_loss_));
      }
    }
  }
  bitrate_ = CapBitrateToThresholds(bitrate_);
}

bool SendSideBandwidthEstimation::IsInStartPhase(int64_t now_ms) const {
  return first_report_time_ms_ == -1 ||
         now_ms - first_report_time_ms_ < kStartPhaseMs;
}

void SendSideBandwidthEstimation::UpdateMinHistory(int64_t now_ms) {
  // Remove old data points from history.
  // Since history precision is in ms, add one so it is able to increase
  // bitrate if it is off by as little as 0.5ms.
  while (!min_bitrate_history_.empty() &&
         now_ms - min_bitrate_history_.front().first + 1 >
             kBweIncreaseIntervalMs) {
    min_bitrate_history_.pop_front();
  }

  // Typical minimum sliding-window algorithm: Pop values higher than current
  // bitrate before pushing it.
  while (!min_bitrate_history_.empty() &&
         bitrate_ <= min_bitrate_history_.back().second) {
    min_bitrate_history_.pop_back();
  }

  min_bitrate_history_.push_back(std::make_pair(now_ms, bitrate_));
}

uint32_t SendSideBandwidthEstimation::CapBitrateToThresholds(uint32_t bitrate) {
  if (bwe_incoming_ > 0 && bitrate > bwe_incoming_) {
    bitrate = bwe_incoming_;
  }
  if (bitrate > max_bitrate_configured_) {
    bitrate = max_bitrate_configured_;
  }
  if (bitrate < min_bitrate_configured_) {
    LOG(LS_WARNING) << "Estimated available bandwidth " << bitrate / 1000
                    << " kbps is below configured min bitrate "
                    << min_bitrate_configured_ / 1000 << " kbps.";
    bitrate = min_bitrate_configured_;
  }
  return bitrate;
}
}  // namespace webrtc
