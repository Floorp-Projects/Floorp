/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/report_block_data.h"

#include "rtc_base/checks.h"

namespace webrtc {

TimeDelta ReportBlockData::jitter(int rtp_clock_rate_hz) const {
  RTC_DCHECK_GT(rtp_clock_rate_hz, 0);
  // Conversion to TimeDelta and division are swapped to avoid conversion
  // to/from floating point types.
  return TimeDelta::Seconds(jitter()) / rtp_clock_rate_hz;
}

void ReportBlockData::SetReportBlock(uint32_t sender_ssrc,
                                     const rtcp::ReportBlock& report_block,
                                     Timestamp report_block_timestamp_utc) {
  report_block_.sender_ssrc = sender_ssrc;
  report_block_.source_ssrc = report_block.source_ssrc();
  report_block_.fraction_lost = report_block.fraction_lost();
  report_block_.packets_lost = report_block.cumulative_lost();
  report_block_.extended_highest_sequence_number =
      report_block.extended_high_seq_num();
  report_block_.jitter = report_block.jitter();
  report_block_.delay_since_last_sender_report =
      report_block.delay_since_last_sr();
  report_block_.last_sender_report_timestamp = report_block.last_sr();

  report_block_timestamp_utc_ = report_block_timestamp_utc;
}

void ReportBlockData::AddRoundTripTimeSample(TimeDelta rtt) {
  if (rtt > max_rtt_)
    max_rtt_ = rtt;
  if (num_rtts_ == 0 || rtt < min_rtt_)
    min_rtt_ = rtt;
  last_rtt_ = rtt;
  sum_rtt_ += rtt;
  ++num_rtts_;
}

}  // namespace webrtc
