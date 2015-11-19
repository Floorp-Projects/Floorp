/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/bitrate_prober.h"

#include <assert.h>
#include <limits>
#include <sstream>

#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {

namespace {
int ComputeDeltaFromBitrate(size_t packet_size, int bitrate_bps) {
  assert(bitrate_bps > 0);
  // Compute the time delta needed to send packet_size bytes at bitrate_bps
  // bps. Result is in milliseconds.
  return static_cast<int>(1000ll * static_cast<int64_t>(packet_size) * 8ll /
      bitrate_bps);
}
}  // namespace

BitrateProber::BitrateProber()
    : probing_state_(kDisabled),
      packet_size_last_send_(0),
      time_last_send_ms_(-1) {
}

void BitrateProber::SetEnabled(bool enable) {
  if (enable) {
    if (probing_state_ == kDisabled) {
      probing_state_ = kAllowedToProbe;
      LOG(LS_INFO) << "Initial bandwidth probing enabled";
    }
  } else {
    probing_state_ = kDisabled;
    LOG(LS_INFO) << "Initial bandwidth probing disabled";
  }
}

bool BitrateProber::IsProbing() const {
  return probing_state_ == kProbing;
}

void BitrateProber::MaybeInitializeProbe(int bitrate_bps) {
  if (probing_state_ != kAllowedToProbe)
    return;
  probe_bitrates_.clear();
  // Max number of packets used for probing.
  const int kMaxNumProbes = 2;
  const int kPacketsPerProbe = 5;
  const float kProbeBitrateMultipliers[kMaxNumProbes] = {3, 6};
  int bitrates_bps[kMaxNumProbes];
  std::stringstream bitrate_log;
  bitrate_log << "Start probing for bandwidth, bitrates:";
  for (int i = 0; i < kMaxNumProbes; ++i) {
    bitrates_bps[i] = kProbeBitrateMultipliers[i] * bitrate_bps;
    bitrate_log << " " << bitrates_bps[i];
    // We need one extra to get 5 deltas for the first probe.
    if (i == 0)
      probe_bitrates_.push_back(bitrates_bps[i]);
    for (int j = 0; j < kPacketsPerProbe; ++j)
      probe_bitrates_.push_back(bitrates_bps[i]);
  }
  bitrate_log << ", num packets: " << probe_bitrates_.size();
  LOG(LS_INFO) << bitrate_log.str().c_str();
  probing_state_ = kProbing;
}

int BitrateProber::TimeUntilNextProbe(int64_t now_ms) {
  if (probing_state_ != kDisabled && probe_bitrates_.empty()) {
    probing_state_ = kWait;
  }
  if (probe_bitrates_.empty()) {
    // No probe started, or waiting for next probe.
    return -1;
  }
  int64_t elapsed_time_ms = now_ms - time_last_send_ms_;
  // We will send the first probe packet immediately if no packet has been
  // sent before.
  int time_until_probe_ms = 0;
  if (packet_size_last_send_ > 0 && probing_state_ == kProbing) {
    int next_delta_ms = ComputeDeltaFromBitrate(packet_size_last_send_,
                                                probe_bitrates_.front());
    time_until_probe_ms = next_delta_ms - elapsed_time_ms;
    // There is no point in trying to probe with less than 1 ms between packets
    // as it essentially means trying to probe at infinite bandwidth.
    const int kMinProbeDeltaMs = 1;
    // If we have waited more than 3 ms for a new packet to probe with we will
    // consider this probing session over.
    const int kMaxProbeDelayMs = 3;
    if (next_delta_ms < kMinProbeDeltaMs ||
        time_until_probe_ms < -kMaxProbeDelayMs) {
      // We currently disable probing after the first probe, as we only want
      // to probe at the beginning of a connection. We should set this to
      // kWait if we later want to probe periodically.
      probing_state_ = kWait;
      LOG(LS_INFO) << "Next delta too small, stop probing.";
      time_until_probe_ms = 0;
    }
  }
  return time_until_probe_ms;
}

void BitrateProber::PacketSent(int64_t now_ms, size_t packet_size) {
  assert(packet_size > 0);
  packet_size_last_send_ = packet_size;
  time_last_send_ms_ = now_ms;
  if (probing_state_ != kProbing)
    return;
  if (!probe_bitrates_.empty())
    probe_bitrates_.pop_front();
}
}  // namespace webrtc
