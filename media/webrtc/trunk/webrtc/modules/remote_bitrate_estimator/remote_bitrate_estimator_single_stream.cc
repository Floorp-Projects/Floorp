/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"

#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {

RemoteBitrateEstimatorSingleStream::RemoteBitrateEstimatorSingleStream(
    const OverUseDetectorOptions& options,
    RemoteBitrateObserver* observer,
    Clock* clock)
    : options_(options),
      clock_(clock),
      observer_(observer),
      crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      last_process_time_(-1) {
  assert(observer_);
}

void RemoteBitrateEstimatorSingleStream::IncomingPacket(
    unsigned int ssrc,
    int payload_size,
    int64_t arrival_time,
    uint32_t rtp_timestamp) {
  CriticalSectionScoped cs(crit_sect_.get());
  SsrcOveruseDetectorMap::iterator it = overuse_detectors_.find(ssrc);
  if (it == overuse_detectors_.end()) {
    // This is a new SSRC. Adding to map.
    // TODO(holmer): If the channel changes SSRC the old SSRC will still be
    // around in this map until the channel is deleted. This is OK since the
    // callback will no longer be called for the old SSRC. This will be
    // automatically cleaned up when we have one RemoteBitrateEstimator per REMB
    // group.
    std::pair<SsrcOveruseDetectorMap::iterator, bool> insert_result =
        overuse_detectors_.insert(std::make_pair(ssrc, OveruseDetector(
            options_)));
    it = insert_result.first;
  }
  OveruseDetector* overuse_detector = &it->second;
  incoming_bitrate_.Update(payload_size, arrival_time);
  const BandwidthUsage prior_state = overuse_detector->State();
  overuse_detector->Update(payload_size, -1, rtp_timestamp, arrival_time);
  if (overuse_detector->State() == kBwOverusing) {
    unsigned int incoming_bitrate = incoming_bitrate_.BitRate(arrival_time);
    if (prior_state != kBwOverusing ||
        remote_rate_.TimeToReduceFurther(arrival_time, incoming_bitrate)) {
      // The first overuse should immediately trigger a new estimate.
      // We also have to update the estimate immediately if we are overusing
      // and the target bitrate is too high compared to what we are receiving.
      UpdateEstimate(arrival_time);
    }
  }
}

int32_t RemoteBitrateEstimatorSingleStream::Process() {
  if (TimeUntilNextProcess() > 0) {
    return 0;
  }
  UpdateEstimate(clock_->TimeInMilliseconds());
  last_process_time_ = clock_->TimeInMilliseconds();
  return 0;
}

int32_t RemoteBitrateEstimatorSingleStream::TimeUntilNextProcess() {
  if (last_process_time_ < 0) {
    return 0;
  }
  return last_process_time_ + kProcessIntervalMs - clock_->TimeInMilliseconds();
}

void RemoteBitrateEstimatorSingleStream::UpdateEstimate(int64_t time_now) {
  CriticalSectionScoped cs(crit_sect_.get());
  BandwidthUsage bw_state = kBwNormal;
  double sum_noise_var = 0.0;
  SsrcOveruseDetectorMap::iterator it = overuse_detectors_.begin();
  while (it != overuse_detectors_.end()) {
    const int64_t time_of_last_received_packet =
         it->second.time_of_last_received_packet();
    if (time_of_last_received_packet >= 0 &&
        time_now - time_of_last_received_packet > kStreamTimeOutMs) {
      // This over-use detector hasn't received packets for |kStreamTimeOutMs|
      // milliseconds and is considered stale.
      overuse_detectors_.erase(it++);
    } else {
      sum_noise_var += it->second.NoiseVar();
      // Make sure that we trigger an over-use if any of the over-use detectors
      // is detecting over-use.
      if (it->second.State() > bw_state) {
        bw_state = it->second.State();
      }
      ++it;
    }
  }
  // We can't update the estimate if we don't have any active streams.
  if (overuse_detectors_.empty()) {
    remote_rate_.Reset();
    return;
  }
  double mean_noise_var = sum_noise_var /
      static_cast<double>(overuse_detectors_.size());
  const RateControlInput input(bw_state,
                               incoming_bitrate_.BitRate(time_now),
                               mean_noise_var);
  const RateControlRegion region = remote_rate_.Update(&input, time_now);
  unsigned int target_bitrate = remote_rate_.UpdateBandwidthEstimate(time_now);
  if (remote_rate_.ValidEstimate()) {
    std::vector<unsigned int> ssrcs;
    GetSsrcs(&ssrcs);
    observer_->OnReceiveBitrateChanged(&ssrcs, target_bitrate);
  }
  for (it = overuse_detectors_.begin(); it != overuse_detectors_.end(); ++it) {
    it->second.SetRateControlRegion(region);
  }
}

void RemoteBitrateEstimatorSingleStream::OnRttUpdate(uint32_t rtt) {
  CriticalSectionScoped cs(crit_sect_.get());
  remote_rate_.SetRtt(rtt);
}

void RemoteBitrateEstimatorSingleStream::RemoveStream(unsigned int ssrc) {
  CriticalSectionScoped cs(crit_sect_.get());
  // Ignoring the return value which is the number of elements erased.
  overuse_detectors_.erase(ssrc);
}

bool RemoteBitrateEstimatorSingleStream::LatestEstimate(
    std::vector<unsigned int>* ssrcs,
    unsigned int* bitrate_bps) const {
  CriticalSectionScoped cs(crit_sect_.get());
  assert(bitrate_bps);
  if (!remote_rate_.ValidEstimate()) {
    return false;
  }
  GetSsrcs(ssrcs);
  if (ssrcs->empty())
    *bitrate_bps = 0;
  else
    *bitrate_bps = remote_rate_.LatestEstimate();
  return true;
}

void RemoteBitrateEstimatorSingleStream::GetSsrcs(
    std::vector<unsigned int>* ssrcs) const {
  assert(ssrcs);
  ssrcs->resize(overuse_detectors_.size());
  int i = 0;
  for (SsrcOveruseDetectorMap::const_iterator it = overuse_detectors_.begin();
      it != overuse_detectors_.end(); ++it, ++i) {
    (*ssrcs)[i] = it->first;
  }
}

}  // namespace webrtc
