/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/retransmission_timeout.h"

#include <algorithm>
#include <cstdint>

#include "net/dcsctp/public/dcsctp_options.h"

namespace dcsctp {

RetransmissionTimeout::RetransmissionTimeout(const DcSctpOptions& options)
    : min_rto_(*options.rto_min),
      max_rto_(*options.rto_max),
      max_rtt_(*options.rtt_max),
      rto_(*options.rto_initial) {}

void RetransmissionTimeout::ObserveRTT(DurationMs measured_rtt) {
  const int32_t rtt = *measured_rtt;

  // Unrealistic values will be skipped. If a wrongly measured (or otherwise
  // corrupt) value was processed, it could change the state in a way that would
  // take a very long time to recover.
  if (rtt < 0 || rtt > max_rtt_) {
    return;
  }

  // From https://tools.ietf.org/html/rfc4960#section-6.3.1, but avoiding
  // floating point math by implementing algorithm from "V. Jacobson: Congestion
  // avoidance and control", but adapted for SCTP.
  if (first_measurement_) {
    scaled_srtt_ = rtt << kRttShift;
    scaled_rtt_var_ = (rtt / 2) << kRttVarShift;
    first_measurement_ = false;
  } else {
    int32_t rtt_diff = rtt - (scaled_srtt_ >> kRttShift);
    scaled_srtt_ += rtt_diff;
    if (rtt_diff < 0) {
      rtt_diff = -rtt_diff;
    }
    rtt_diff -= (scaled_rtt_var_ >> kRttVarShift);
    scaled_rtt_var_ += rtt_diff;
  }
  rto_ = (scaled_srtt_ >> kRttShift) + scaled_rtt_var_;

  // If the RTO becomes smaller or equal to RTT, expiration timers will be
  // scheduled at the same time as packets are expected. Only happens in
  // extremely stable RTTs, i.e. in simulations.
  rto_ = std::max(rto_, rtt + 1);

  // Clamp RTO between min and max.
  rto_ = std::min(std::max(rto_, min_rto_), max_rto_);
}
}  // namespace dcsctp
