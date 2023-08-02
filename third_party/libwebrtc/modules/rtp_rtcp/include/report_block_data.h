/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_REPORT_BLOCK_DATA_H_
#define MODULES_RTP_RTCP_INCLUDE_REPORT_BLOCK_DATA_H_

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"

namespace webrtc {

class ReportBlockData {
 public:
  ReportBlockData() = default;

  ReportBlockData(const ReportBlockData&) = default;
  ReportBlockData& operator=(const ReportBlockData&) = default;

  const RTCPReportBlock& report_block() const { return report_block_; }

  [[deprecated]] int64_t report_block_timestamp_utc_us() const {
    return report_block_timestamp_utc_.us();
  }
  [[deprecated]] int64_t last_rtt_ms() const { return last_rtt_.ms(); }
  [[deprecated]] int64_t min_rtt_ms() const { return min_rtt_.ms(); }
  [[deprecated]] int64_t max_rtt_ms() const { return max_rtt_.ms(); }
  [[deprecated]] int64_t sum_rtt_ms() const { return sum_rtt_.ms(); }
  [[deprecated]] double AvgRttMs() const { return AvgRtt().ms<double>(); }

  Timestamp report_block_timestamp_utc() const {
    return report_block_timestamp_utc_;
  }
  TimeDelta last_rtt() const { return last_rtt_; }
  TimeDelta min_rtt() const { return min_rtt_; }
  TimeDelta max_rtt() const { return max_rtt_; }
  TimeDelta sum_rtts() const { return sum_rtt_; }
  size_t num_rtts() const { return num_rtts_; }
  bool has_rtt() const { return num_rtts_ != 0; }

  TimeDelta AvgRtt() const;

  void SetReportBlock(uint32_t sender_ssrc,
                      const rtcp::ReportBlock& report_block,
                      Timestamp report_block_timestamp_utc_us);
  void AddRoundTripTimeSample(TimeDelta rtt);

 private:
  RTCPReportBlock report_block_;
  Timestamp report_block_timestamp_utc_ = Timestamp::Zero();
  TimeDelta last_rtt_ = TimeDelta::Zero();
  TimeDelta min_rtt_ = TimeDelta::Zero();
  TimeDelta max_rtt_ = TimeDelta::Zero();
  TimeDelta sum_rtt_ = TimeDelta::Zero();
  size_t num_rtts_ = 0;
};

class ReportBlockDataObserver {
 public:
  virtual ~ReportBlockDataObserver() = default;

  virtual void OnReportBlockDataUpdated(ReportBlockData report_block_data) = 0;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_INCLUDE_REPORT_BLOCK_DATA_H_
