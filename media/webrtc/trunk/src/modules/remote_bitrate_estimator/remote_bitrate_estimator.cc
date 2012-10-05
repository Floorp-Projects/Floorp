/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"

#include "system_wrappers/interface/tick_util.h"

namespace webrtc {

RemoteBitrateEstimator::RemoteBitrateEstimator(
    RemoteBitrateObserver* observer,
    const OverUseDetectorOptions& options)
    : options_(options),
      observer_(observer),
      crit_sect_(CriticalSectionWrapper::CreateCriticalSection()) {
  assert(observer_);
}

void RemoteBitrateEstimator::IncomingPacket(unsigned int ssrc,
                                            int packet_size,
                                            int64_t arrival_time,
                                            uint32_t rtp_timestamp,
                                            int64_t packet_send_time) {
  CriticalSectionScoped cs(crit_sect_.get());
  SsrcBitrateControlsMap::iterator it = bitrate_controls_.find(ssrc);
  if (it == bitrate_controls_.end()) {
    // This is a new SSRC. Adding to map.
    // TODO(holmer): If the channel changes SSRC the old SSRC will still be
    // around in this map until the channel is deleted. This is OK since the
    // callback will no longer be called for the old SSRC. This will be
    // automatically cleaned up when we have one RemoteBitrateEstimator per REMB
    // group.
    bitrate_controls_.insert(std::make_pair(ssrc, BitrateControls(options_)));
    it = bitrate_controls_.find(ssrc);
  }
  OverUseDetector* overuse_detector = &it->second.overuse_detector;
  it->second.incoming_bitrate.Update(packet_size, arrival_time);
  const BandwidthUsage prior_state = overuse_detector->State();
  overuse_detector->Update(packet_size, rtp_timestamp, arrival_time);
  if (prior_state != overuse_detector->State() &&
      overuse_detector->State() == kBwOverusing) {
    // The first overuse should immediately trigger a new estimate.
    UpdateEstimate(ssrc, arrival_time);
  }
}

void RemoteBitrateEstimator::UpdateEstimate(unsigned int ssrc,
                                            int64_t time_now) {
  CriticalSectionScoped cs(crit_sect_.get());
  SsrcBitrateControlsMap::iterator it = bitrate_controls_.find(ssrc);
  if (it == bitrate_controls_.end()) {
    return;
  }
  OverUseDetector* overuse_detector = &it->second.overuse_detector;
  RemoteRateControl* remote_rate = &it->second.remote_rate;
  const RateControlInput input(overuse_detector->State(),
                               it->second.incoming_bitrate.BitRate(time_now),
                               overuse_detector->NoiseVar());
  const RateControlRegion region = remote_rate->Update(&input, time_now);
  unsigned int target_bitrate = remote_rate->UpdateBandwidthEstimate(time_now);
  if (remote_rate->ValidEstimate()) {
    observer_->OnReceiveBitrateChanged(ssrc, target_bitrate);
  }
  overuse_detector->SetRateControlRegion(region);
}

void RemoteBitrateEstimator::SetRtt(unsigned int rtt) {
  CriticalSectionScoped cs(crit_sect_.get());
  for (SsrcBitrateControlsMap::iterator it = bitrate_controls_.begin();
      it != bitrate_controls_.end(); ++it) {
    it->second.remote_rate.SetRtt(rtt);
  }
}

void RemoteBitrateEstimator::RemoveStream(unsigned int ssrc) {
  CriticalSectionScoped cs(crit_sect_.get());
  // Ignoring the return value which is the number of elements erased.
  bitrate_controls_.erase(ssrc);
}

bool RemoteBitrateEstimator::LatestEstimate(unsigned int ssrc,
                                            unsigned int* bitrate_bps) const {
  CriticalSectionScoped cs(crit_sect_.get());
  assert(bitrate_bps != NULL);
  SsrcBitrateControlsMap::const_iterator it = bitrate_controls_.find(ssrc);
  if (it == bitrate_controls_.end()) {
    return false;
  }
  if (!it->second.remote_rate.ValidEstimate()) {
    return false;
  }
  *bitrate_bps = it->second.remote_rate.LatestEstimate();
  return true;
}

}  // namespace webrtc
