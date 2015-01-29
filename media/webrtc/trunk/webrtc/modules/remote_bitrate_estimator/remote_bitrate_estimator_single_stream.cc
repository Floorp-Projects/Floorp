/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>

#include "webrtc/modules/remote_bitrate_estimator/rate_statistics.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/overuse_detector.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_rate_control.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace {
class RemoteBitrateEstimatorSingleStream : public RemoteBitrateEstimator {
 public:
  RemoteBitrateEstimatorSingleStream(RemoteBitrateObserver* observer,
                                     Clock* clock,
                                     uint32_t min_bitrate_bps);
  virtual ~RemoteBitrateEstimatorSingleStream() {}

  // Called for each incoming packet. If this is a new SSRC, a new
  // BitrateControl will be created. Updates the incoming payload bitrate
  // estimate and the over-use detector. If an over-use is detected the
  // remote bitrate estimate will be updated. Note that |payload_size| is the
  // packet size excluding headers.
  virtual void IncomingPacket(int64_t arrival_time_ms,
                              int payload_size,
                              const RTPHeader& header) OVERRIDE;

  // Triggers a new estimate calculation.
  // Implements the Module interface.
  virtual int32_t Process() OVERRIDE;
  virtual int32_t TimeUntilNextProcess() OVERRIDE;
  // Set the current round-trip time experienced by the stream.
  // Implements the StatsObserver interface.
  virtual void OnRttUpdate(uint32_t rtt) OVERRIDE;

  // Removes all data for |ssrc|.
  virtual void RemoveStream(unsigned int ssrc) OVERRIDE;

  // Returns true if a valid estimate exists and sets |bitrate_bps| to the
  // estimated payload bitrate in bits per second. |ssrcs| is the list of ssrcs
  // currently being received and of which the bitrate estimate is based upon.
  virtual bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                              unsigned int* bitrate_bps) const OVERRIDE;

  virtual bool GetStats(
      ReceiveBandwidthEstimatorStats* output) const OVERRIDE;

 private:
  // Map from SSRC to over-use detector and last incoming packet time in
  // milliseconds, taken from clock_.
  typedef std::map<unsigned int, std::pair<OveruseDetector, int64_t> >
      SsrcOveruseDetectorMap;

  static OveruseDetector* GetDetector(
      const SsrcOveruseDetectorMap::iterator it) {
    return &it->second.first;
  }

  static int64_t GetPacketTimeMs(const SsrcOveruseDetectorMap::iterator it) {
    return it->second.second;
  }

  static void SetPacketTimeMs(SsrcOveruseDetectorMap::iterator it,
                              int64_t time_ms) {
    it->second.second = time_ms;
  }

  // Triggers a new estimate calculation.
  void UpdateEstimate(int64_t now_ms);

  void GetSsrcs(std::vector<unsigned int>* ssrcs) const;

  Clock* clock_;
  SsrcOveruseDetectorMap overuse_detectors_;
  RateStatistics incoming_bitrate_;
  RemoteRateControl remote_rate_;
  RemoteBitrateObserver* observer_;
  scoped_ptr<CriticalSectionWrapper> crit_sect_;
  int64_t last_process_time_;
};

RemoteBitrateEstimatorSingleStream::RemoteBitrateEstimatorSingleStream(
    RemoteBitrateObserver* observer,
    Clock* clock,
    uint32_t min_bitrate_bps)
    : clock_(clock),
      incoming_bitrate_(500, 8000),
      remote_rate_(min_bitrate_bps),
      observer_(observer),
      crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      last_process_time_(-1) {
  assert(observer_);
}

void RemoteBitrateEstimatorSingleStream::IncomingPacket(
    int64_t arrival_time_ms,
    int payload_size,
    const RTPHeader& header) {
  uint32_t ssrc = header.ssrc;
  uint32_t rtp_timestamp = header.timestamp +
      header.extension.transmissionTimeOffset;
  int64_t now_ms = clock_->TimeInMilliseconds();
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
        overuse_detectors_.insert(std::make_pair(ssrc,
            std::make_pair(OveruseDetector(OverUseDetectorOptions()), now_ms)));
    it = insert_result.first;
  }
  SetPacketTimeMs(it, now_ms);
  OveruseDetector* overuse_detector = GetDetector(it);
  incoming_bitrate_.Update(payload_size, now_ms);
  const BandwidthUsage prior_state = overuse_detector->State();
  overuse_detector->Update(payload_size, -1, rtp_timestamp, arrival_time_ms);
  if (overuse_detector->State() == kBwOverusing) {
    unsigned int incoming_bitrate = incoming_bitrate_.Rate(now_ms);
    if (prior_state != kBwOverusing ||
        remote_rate_.TimeToReduceFurther(now_ms, incoming_bitrate)) {
      // The first overuse should immediately trigger a new estimate.
      // We also have to update the estimate immediately if we are overusing
      // and the target bitrate is too high compared to what we are receiving.
      UpdateEstimate(now_ms);
    }
  }
}

int32_t RemoteBitrateEstimatorSingleStream::Process() {
  if (TimeUntilNextProcess() > 0) {
    return 0;
  }
  int64_t now_ms = clock_->TimeInMilliseconds();
  UpdateEstimate(now_ms);
  last_process_time_ = now_ms;
  return 0;
}

int32_t RemoteBitrateEstimatorSingleStream::TimeUntilNextProcess() {
  if (last_process_time_ < 0) {
    return 0;
  }
  return last_process_time_ + kProcessIntervalMs - clock_->TimeInMilliseconds();
}

void RemoteBitrateEstimatorSingleStream::UpdateEstimate(int64_t now_ms) {
  CriticalSectionScoped cs(crit_sect_.get());
  BandwidthUsage bw_state = kBwNormal;
  double sum_noise_var = 0.0;
  SsrcOveruseDetectorMap::iterator it = overuse_detectors_.begin();
  while (it != overuse_detectors_.end()) {
    if (GetPacketTimeMs(it) >= 0 &&
        now_ms - GetPacketTimeMs(it) > kStreamTimeOutMs) {
      // This over-use detector hasn't received packets for |kStreamTimeOutMs|
      // milliseconds and is considered stale.
      overuse_detectors_.erase(it++);
    } else {
      OveruseDetector* overuse_detector = GetDetector(it);
      sum_noise_var += overuse_detector->NoiseVar();
      // Make sure that we trigger an over-use if any of the over-use detectors
      // is detecting over-use.
      if (overuse_detector->State() > bw_state) {
        bw_state = overuse_detector->State();
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
                               incoming_bitrate_.Rate(now_ms),
                               mean_noise_var);
  const RateControlRegion region = remote_rate_.Update(&input, now_ms);
  unsigned int target_bitrate = remote_rate_.UpdateBandwidthEstimate(now_ms);
  if (remote_rate_.ValidEstimate()) {
    std::vector<unsigned int> ssrcs;
    GetSsrcs(&ssrcs);
    observer_->OnReceiveBitrateChanged(ssrcs, target_bitrate);
  }
  for (it = overuse_detectors_.begin(); it != overuse_detectors_.end(); ++it) {
    GetDetector(it)->SetRateControlRegion(region);
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

bool RemoteBitrateEstimatorSingleStream::GetStats(
    ReceiveBandwidthEstimatorStats* output) const {
  // Not implemented.
  return false;
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
}  // namespace

RemoteBitrateEstimator* RemoteBitrateEstimatorFactory::Create(
    RemoteBitrateObserver* observer,
    Clock* clock,
    RateControlType control_type,
    uint32_t min_bitrate_bps) const {
  LOG(LS_INFO) << "RemoteBitrateEstimatorFactory: Instantiating.";
  return new RemoteBitrateEstimatorSingleStream(observer, clock,
                                                min_bitrate_bps);
}

RemoteBitrateEstimator* AbsoluteSendTimeRemoteBitrateEstimatorFactory::Create(
    RemoteBitrateObserver* observer,
    Clock* clock,
    RateControlType control_type,
    uint32_t min_bitrate_bps) const {
  LOG(LS_INFO) << "AbsoluteSendTimeRemoteBitrateEstimatorFactory: "
      "Instantiating.";
  return new RemoteBitrateEstimatorSingleStream(observer, clock,
                                                min_bitrate_bps);
}
}  // namespace webrtc
