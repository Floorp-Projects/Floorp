/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_multi_stream.h"

#include "modules/remote_bitrate_estimator/include/rtp_to_ntp.h"
#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"
#include "system_wrappers/interface/tick_util.h"

namespace webrtc {

RemoteBitrateEstimator* RemoteBitrateEstimator::Create(
    RemoteBitrateObserver* observer,
    const OverUseDetectorOptions& options,
    EstimationMode mode) {
  switch (mode) {
    case kMultiStreamEstimation:
      return new RemoteBitrateEstimatorMultiStream(observer, options);
    case kSingleStreamEstimation:
      return new RemoteBitrateEstimatorSingleStream(observer, options);
  }
  return NULL;
}

RemoteBitrateEstimatorMultiStream::RemoteBitrateEstimatorMultiStream(
    RemoteBitrateObserver* observer,
    const OverUseDetectorOptions& options)
    : remote_rate_(),
      overuse_detector_(options),
      incoming_bitrate_(),
      observer_(observer),
      streams_(),
      crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      initial_ssrc_(0),
      multi_stream_(false) {
  assert(observer_);
}

void RemoteBitrateEstimatorMultiStream::IncomingRtcp(unsigned int ssrc,
                                                     uint32_t ntp_secs,
                                                     uint32_t ntp_frac,
                                                     uint32_t timestamp) {
  CriticalSectionScoped cs(crit_sect_.get());
  if (ntp_secs == 0 && ntp_frac == 0) {
    return;
  }
  // Insert a new RTCP list mapped to this SSRC if one doesn't already exist.
  std::pair<StreamMap::iterator, bool> stream_insert_result =
      streams_.insert(std::make_pair(ssrc, synchronization::RtcpList()));
  StreamMap::iterator stream_it = stream_insert_result.first;
  synchronization::RtcpList* rtcp_list = &stream_it->second;
  synchronization::RtcpMeasurement measurement(ntp_secs, ntp_frac, timestamp);
  // Make sure this RTCP is unique as we need two unique data points to
  // calculate the RTP timestamp frequency.
  for (synchronization::RtcpList::iterator it = rtcp_list->begin();
      it != rtcp_list->end(); ++it) {
    if ((measurement.ntp_secs == (*it).ntp_secs &&
        measurement.ntp_frac == (*it).ntp_frac) ||
        measurement.rtp_timestamp == (*it).rtp_timestamp) {
      return;
    }
  }
  // If this stream will get two RTCPs when the new one is added we can switch
  // to multi-stream mode.
  if (!multi_stream_ && rtcp_list->size() >= 1) {
    multi_stream_ = true;
  }
  if (rtcp_list->size() >= 2) {
    rtcp_list->pop_back();
  }
  rtcp_list->push_front(measurement);
}

void RemoteBitrateEstimatorMultiStream::IncomingPacket(unsigned int ssrc,
                                                       int payload_size,
                                                       int64_t arrival_time,
                                                       uint32_t rtp_timestamp) {
  CriticalSectionScoped cs(crit_sect_.get());
  incoming_bitrate_.Update(payload_size, arrival_time);
  // Add this stream to the map of streams if it doesn't already exist.
  std::pair<StreamMap::iterator, bool> stream_insert_result =
      streams_.insert(std::make_pair(ssrc, synchronization::RtcpList()));
  synchronization::RtcpList* rtcp_list = &stream_insert_result.first->second;
  if (initial_ssrc_ == 0) {
    initial_ssrc_ = ssrc;
  }
  if (!multi_stream_) {
    if (ssrc != initial_ssrc_) {
      // We can only handle the initial stream until we get into multi stream
      // mode.
      return;
    }
  } else if (rtcp_list->size() < 2) {
    // We can't use this stream until we have received two RTCP SR reports.
    return;
  }
  const BandwidthUsage prior_state = overuse_detector_.State();
  int64_t timestamp_in_ms = -1;
  if (multi_stream_) {
    synchronization::RtpToNtpMs(rtp_timestamp, *rtcp_list, &timestamp_in_ms);
  }
  overuse_detector_.Update(payload_size, timestamp_in_ms, rtp_timestamp,
                           arrival_time);
  if (prior_state != kBwOverusing &&
      overuse_detector_.State() == kBwOverusing) {
    // The first overuse should immediately trigger a new estimate.
    UpdateEstimate(1, arrival_time);
  }
}

void RemoteBitrateEstimatorMultiStream::UpdateEstimate(unsigned int ssrc,
                                                       int64_t time_now) {
  CriticalSectionScoped cs(crit_sect_.get());
  const RateControlInput input(overuse_detector_.State(),
                               incoming_bitrate_.BitRate(time_now),
                               overuse_detector_.NoiseVar());
  const RateControlRegion region = remote_rate_.Update(&input, time_now);
  unsigned int target_bitrate = remote_rate_.UpdateBandwidthEstimate(time_now);
  if (remote_rate_.ValidEstimate()) {
    std::vector<unsigned int> ssrcs;
    GetSsrcs(&ssrcs);
    if (!ssrcs.empty()) {
      observer_->OnReceiveBitrateChanged(&ssrcs, target_bitrate);
    }
  }
  overuse_detector_.SetRateControlRegion(region);
}

void RemoteBitrateEstimatorMultiStream::SetRtt(unsigned int rtt) {
  CriticalSectionScoped cs(crit_sect_.get());
  remote_rate_.SetRtt(rtt);
}

void RemoteBitrateEstimatorMultiStream::RemoveStream(unsigned int ssrc) {
  CriticalSectionScoped cs(crit_sect_.get());
  streams_.erase(ssrc);
}

bool RemoteBitrateEstimatorMultiStream::LatestEstimate(
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

void RemoteBitrateEstimatorMultiStream::GetSsrcs(
    std::vector<unsigned int>* ssrcs) const {
  assert(ssrcs);
  ssrcs->resize(streams_.size());
  int i = 0;
  for (StreamMap::const_iterator it = streams_.begin(); it != streams_.end();
      ++it, ++i) {
    (*ssrcs)[i] = it->first;
  }
}

}  // namespace webrtc
