/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  FEC and NACK added bitrate is handled outside class
 */

#ifndef WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
#define WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_

#include "modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {
class SendSideBandwidthEstimation {
 public:
  SendSideBandwidthEstimation();
  virtual ~SendSideBandwidthEstimation();

  // Call when we receive a RTCP message with TMMBR or REMB
  // Return true if new_bitrate is valid.
  bool UpdateBandwidthEstimate(const uint32_t bandwidth,
                               uint32_t* new_bitrate,
                               uint8_t* fraction_lost,
                               uint16_t* rtt);

  // Call when we receive a RTCP message with a ReceiveBlock
  // Return true if new_bitrate is valid.
  bool UpdatePacketLoss(const int number_of_packets,
                        const uint32_t rtt,
                        const uint32_t now_ms,
                        uint8_t* loss,
                        uint32_t* new_bitrate);

  // Return false if no bandwidth estimate is available
  bool AvailableBandwidth(uint32_t* bandwidth) const;
  void SetSendBitrate(const uint32_t bitrate);
  void SetMinMaxBitrate(const uint32_t min_bitrate, const uint32_t max_bitrate);

 private:
  bool ShapeSimple(const uint8_t loss, const uint32_t rtt,
                   const uint32_t now_ms, uint32_t* bitrate);

  uint32_t CalcTFRCbps(uint16_t rtt, uint8_t loss);

  enum { kBWEIncreaseIntervalMs = 1000 };
  enum { kBWEDecreaseIntervalMs = 300 };
  enum { kLimitNumPackets = 20 };
  enum { kAvgPacketSizeBytes = 1000 };

  CriticalSectionWrapper* critsect_;

  // incoming filters
  int accumulate_lost_packets_Q8_;
  int accumulate_expected_packets_;

  uint32_t bitrate_;
  uint32_t min_bitrate_configured_;
  uint32_t max_bitrate_configured_;

  uint8_t last_fraction_loss_;
  uint16_t last_round_trip_time_;

  uint32_t bwe_incoming_;
  uint32_t time_last_increase_;
  uint32_t time_last_decrease_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
