/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/bitrate_controller/send_side_bandwidth_estimation.h"

#include <math.h>  // sqrt()

#include "system_wrappers/interface/trace.h"

namespace webrtc {

SendSideBandwidthEstimation::SendSideBandwidthEstimation()
    : critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      accumulate_lost_packets_Q8_(0),
      accumulate_expected_packets_(0),
      bitrate_(0),
      min_bitrate_configured_(0),
      max_bitrate_configured_(0),
      last_fraction_loss_(0),
      last_round_trip_time_(0),
      bwe_incoming_(0),
      time_last_increase_(0),
      time_last_decrease_(0) {
}

SendSideBandwidthEstimation::~SendSideBandwidthEstimation() {
    delete critsect_;
}

void SendSideBandwidthEstimation::SetSendBitrate(const uint32_t bitrate) {
  CriticalSectionScoped cs(critsect_);
  bitrate_ = bitrate;
}

void SendSideBandwidthEstimation::SetMinMaxBitrate(const uint32_t min_bitrate,
                                                   const uint32_t max_bitrate) {
  CriticalSectionScoped cs(critsect_);
  min_bitrate_configured_ = min_bitrate;
  if (max_bitrate == 0) {
    // no max configured use 1Gbit/s
    max_bitrate_configured_ = 1000000000;
  } else {
    max_bitrate_configured_ = max_bitrate;
  }
}

bool SendSideBandwidthEstimation::UpdateBandwidthEstimate(
    const uint32_t bandwidth,
    uint32_t* new_bitrate,
    uint8_t* fraction_lost,
    uint16_t* rtt) {
  *new_bitrate = 0;
  CriticalSectionScoped cs(critsect_);

  bwe_incoming_ = bandwidth;

  if (bitrate_ == 0) {
    // SendSideBandwidthEstimation off
    return false;
  }
  if (bwe_incoming_ > 0 && bitrate_ > bwe_incoming_) {
    bitrate_ = bwe_incoming_;
    *new_bitrate = bitrate_;
    *fraction_lost = last_fraction_loss_;
    *rtt = last_round_trip_time_;
    return true;
  }
  return false;
}

bool SendSideBandwidthEstimation::UpdatePacketLoss(
    const int number_of_packets,
    const uint32_t rtt,
    const uint32_t now_ms,
    uint8_t* loss,
    uint32_t* new_bitrate) {
  CriticalSectionScoped cs(critsect_);

  if (bitrate_ == 0) {
    // SendSideBandwidthEstimation off
    return false;
  }
  // Update RTT.
  last_round_trip_time_ = rtt;

  // Check sequence number diff and weight loss report
  if (number_of_packets > 0) {
    // Calculate number of lost packets.
    const int num_lost_packets_Q8 = *loss * number_of_packets;
    // Accumulate reports.
    accumulate_lost_packets_Q8_ += num_lost_packets_Q8;
    accumulate_expected_packets_ += number_of_packets;

    // Report loss if the total report is based on sufficiently many packets.
    if (accumulate_expected_packets_ >= kLimitNumPackets) {
      *loss = accumulate_lost_packets_Q8_ / accumulate_expected_packets_;

      // Reset accumulators
      accumulate_lost_packets_Q8_ = 0;
      accumulate_expected_packets_ = 0;
    } else {
      // Report zero loss until we have enough data to estimate
      // the loss rate.
      return false;
    }
  }
  // Keep for next time.
  last_fraction_loss_ = *loss;
  uint32_t bitrate = 0;
  if (!ShapeSimple(*loss, rtt, now_ms, &bitrate)) {
    // No change.
    return false;
  }
  bitrate_ = bitrate;
  *new_bitrate = bitrate;
  return true;
}

bool SendSideBandwidthEstimation::AvailableBandwidth(
    uint32_t* bandwidth) const {
  CriticalSectionScoped cs(critsect_);
  if (bitrate_ == 0) {
    return false;
  }
  *bandwidth = bitrate_;
  return true;
}

/*
 * Calculate the rate that TCP-Friendly Rate Control (TFRC) would apply.
 * The formula in RFC 3448, Section 3.1, is used.
 */
uint32_t SendSideBandwidthEstimation::CalcTFRCbps(uint16_t rtt, uint8_t loss) {
  if (rtt == 0 || loss == 0) {
    // input variables out of range
    return 0;
  }
  double R = static_cast<double>(rtt) / 1000;  // RTT in seconds
  int b = 1;  // number of packets acknowledged by a single TCP acknowledgement;
              // recommended = 1
  double t_RTO = 4.0 * R;  // TCP retransmission timeout value in seconds
                           // recommended = 4*R
  double p = static_cast<double>(loss) / 255;  // packet loss rate in [0, 1)
  double s = static_cast<double>(kAvgPacketSizeBytes);

  // calculate send rate in bytes/second
  double X = s / (R * sqrt(2 * b * p / 3) +
      (t_RTO * (3 * sqrt(3 * b * p / 8) * p * (1 + 32 * p * p))));

  return (static_cast<uint32_t>(X * 8));  // bits/second
}

bool SendSideBandwidthEstimation::ShapeSimple(const uint8_t loss,
                                              const uint32_t rtt,
                                              const uint32_t now_ms,
                                              uint32_t* bitrate) {
  uint32_t new_bitrate = 0;
  bool reducing = false;

  // Limit the rate increases to once a kBWEIncreaseIntervalMs.
  if (loss <= 5) {
    if ((now_ms - time_last_increase_) < kBWEIncreaseIntervalMs) {
      return false;
    }
    time_last_increase_ = now_ms;
  }
  // Limit the rate decreases to once a kBWEDecreaseIntervalMs + rtt.
  if (loss > 26) {
    if ((now_ms - time_last_decrease_) < kBWEDecreaseIntervalMs + rtt) {
      return false;
    }
    time_last_decrease_ = now_ms;
  }

  if (loss > 5 && loss <= 26) {
    // 2% - 10%
    new_bitrate = bitrate_;
  } else if (loss > 26) {
    // 26/256 ~= 10%
    // reduce rate: newRate = rate * (1 - 0.5*lossRate)
    // packetLoss = 256*lossRate
    new_bitrate = static_cast<uint32_t>((bitrate_ *
        static_cast<double>(512 - loss)) / 512.0);
    reducing = true;
  } else {
    // increase rate by 8%
    new_bitrate = static_cast<uint32_t>(bitrate_ * 1.08 + 0.5);

    // add 1 kbps extra, just to make sure that we do not get stuck
    // (gives a little extra increase at low rates, negligible at higher rates)
    new_bitrate += 1000;
  }
  if (reducing) {
    // Calculate what rate TFRC would apply in this situation
    // scale loss to Q0 (back to [0, 255])
    uint32_t tfrc_bitrate = CalcTFRCbps(rtt, loss);
    if (tfrc_bitrate > new_bitrate) {
      // do not reduce further if rate is below TFRC rate
      new_bitrate = tfrc_bitrate;
    }
  }
  if (bwe_incoming_ > 0 && new_bitrate > bwe_incoming_) {
    new_bitrate = bwe_incoming_;
  }
  if (new_bitrate > max_bitrate_configured_) {
    new_bitrate = max_bitrate_configured_;
  }
  if (new_bitrate < min_bitrate_configured_) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, -1,
                 "The configured min bitrate (%u kbps) is greater than the "
                 "estimated available bandwidth (%u kbps).\n",
                 min_bitrate_configured_ / 1000, new_bitrate / 1000);
    new_bitrate = min_bitrate_configured_;
  }
  *bitrate = new_bitrate;
  return true;
}
}  // namespace webrtc
