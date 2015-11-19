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

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/remote_bitrate_estimator/rate_statistics.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/inter_arrival.h"
#include "webrtc/modules/remote_bitrate_estimator/overuse_detector.h"
#include "webrtc/modules/remote_bitrate_estimator/overuse_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_rate_control.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/typedefs.h"

namespace webrtc {

enum { kTimestampGroupLengthMs = 5 };
static const double kTimestampToMs = 1.0 / 90.0;

class RemoteBitrateEstimatorImpl : public RemoteBitrateEstimator {
 public:
  RemoteBitrateEstimatorImpl(RemoteBitrateObserver* observer,
                             Clock* clock,
                             RateControlType control_type,
                             uint32_t min_bitrate_bps);
  virtual ~RemoteBitrateEstimatorImpl();

  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header) override;
  int32_t Process() override;
  int64_t TimeUntilNextProcess() override;
  void OnRttUpdate(int64_t rtt) override;
  void RemoveStream(unsigned int ssrc) override;
  bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                      unsigned int* bitrate_bps) const override;
  bool GetStats(ReceiveBandwidthEstimatorStats* output) const override;

 private:
  struct Detector {
    explicit Detector(int64_t last_packet_time_ms,
                      const OverUseDetectorOptions& options,
                      bool enable_burst_grouping)
        : last_packet_time_ms(last_packet_time_ms),
          inter_arrival(90 * kTimestampGroupLengthMs, kTimestampToMs,
                        enable_burst_grouping),
          estimator(options),
          detector(options) {}
    int64_t last_packet_time_ms;
    InterArrival inter_arrival;
    OveruseEstimator estimator;
    OveruseDetector detector;
  };

  typedef std::map<unsigned int, Detector*> SsrcOveruseEstimatorMap;

  // Triggers a new estimate calculation.
  void UpdateEstimate(int64_t time_now)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get());

  void GetSsrcs(std::vector<unsigned int>* ssrcs) const
      SHARED_LOCKS_REQUIRED(crit_sect_.get());

  Clock* clock_;
  SsrcOveruseEstimatorMap overuse_detectors_ GUARDED_BY(crit_sect_.get());
  RateStatistics incoming_bitrate_ GUARDED_BY(crit_sect_.get());
  rtc::scoped_ptr<RemoteRateControl> remote_rate_ GUARDED_BY(crit_sect_.get());
  RemoteBitrateObserver* observer_ GUARDED_BY(crit_sect_.get());
  rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;
  int64_t last_process_time_;
  int64_t process_interval_ms_ GUARDED_BY(crit_sect_.get());

  DISALLOW_IMPLICIT_CONSTRUCTORS(RemoteBitrateEstimatorImpl);
};

RemoteBitrateEstimatorImpl::RemoteBitrateEstimatorImpl(
    RemoteBitrateObserver* observer,
    Clock* clock,
    RateControlType control_type,
    uint32_t min_bitrate_bps)
    : clock_(clock),
      incoming_bitrate_(1000, 8000),
      remote_rate_(RemoteRateControl::Create(control_type, min_bitrate_bps)),
      observer_(observer),
      crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      last_process_time_(-1),
      process_interval_ms_(kProcessIntervalMs) {
  assert(observer_);
}

RemoteBitrateEstimatorImpl::~RemoteBitrateEstimatorImpl() {
  while (!overuse_detectors_.empty()) {
    SsrcOveruseEstimatorMap::iterator it = overuse_detectors_.begin();
    delete it->second;
    overuse_detectors_.erase(it);
  }
}

void RemoteBitrateEstimatorImpl::IncomingPacket(
    int64_t arrival_time_ms,
    size_t payload_size,
    const RTPHeader& header) {
  uint32_t ssrc = header.ssrc;
  uint32_t rtp_timestamp = header.timestamp +
      header.extension.transmissionTimeOffset;
  int64_t now_ms = clock_->TimeInMilliseconds();
  CriticalSectionScoped cs(crit_sect_.get());
  SsrcOveruseEstimatorMap::iterator it = overuse_detectors_.find(ssrc);
  if (it == overuse_detectors_.end()) {
    // This is a new SSRC. Adding to map.
    // TODO(holmer): If the channel changes SSRC the old SSRC will still be
    // around in this map until the channel is deleted. This is OK since the
    // callback will no longer be called for the old SSRC. This will be
    // automatically cleaned up when we have one RemoteBitrateEstimator per REMB
    // group.
    std::pair<SsrcOveruseEstimatorMap::iterator, bool> insert_result =
        overuse_detectors_.insert(std::make_pair(ssrc, new Detector(
            now_ms,
            OverUseDetectorOptions(),
            remote_rate_->GetControlType() == kAimdControl)));
    it = insert_result.first;
  }
  Detector* estimator = it->second;
  estimator->last_packet_time_ms = now_ms;
  incoming_bitrate_.Update(payload_size, now_ms);
  const BandwidthUsage prior_state = estimator->detector.State();
  uint32_t timestamp_delta = 0;
  int64_t time_delta = 0;
  int size_delta = 0;
  if (estimator->inter_arrival.ComputeDeltas(rtp_timestamp, arrival_time_ms,
                                             payload_size, &timestamp_delta,
                                             &time_delta, &size_delta)) {
    double timestamp_delta_ms = timestamp_delta * kTimestampToMs;
    estimator->estimator.Update(time_delta, timestamp_delta_ms, size_delta,
                                estimator->detector.State());
    estimator->detector.Detect(estimator->estimator.offset(),
                               timestamp_delta_ms,
                               estimator->estimator.num_of_deltas());
  }
  if (estimator->detector.State() == kBwOverusing) {
    uint32_t incoming_bitrate = incoming_bitrate_.Rate(now_ms);
    if (prior_state != kBwOverusing ||
        remote_rate_->TimeToReduceFurther(now_ms, incoming_bitrate)) {
      // The first overuse should immediately trigger a new estimate.
      // We also have to update the estimate immediately if we are overusing
      // and the target bitrate is too high compared to what we are receiving.
      UpdateEstimate(now_ms);
    }
  }
}

int32_t RemoteBitrateEstimatorImpl::Process() {
  if (TimeUntilNextProcess() > 0) {
    return 0;
  }
  {
    CriticalSectionScoped cs(crit_sect_.get());
    UpdateEstimate(clock_->TimeInMilliseconds());
  }
  last_process_time_ = clock_->TimeInMilliseconds();
  return 0;
}

int64_t RemoteBitrateEstimatorImpl::TimeUntilNextProcess() {
  if (last_process_time_ < 0) {
    return 0;
  }
  {
    CriticalSectionScoped cs_(crit_sect_.get());
    return last_process_time_ + process_interval_ms_ -
        clock_->TimeInMilliseconds();
  }
}

void RemoteBitrateEstimatorImpl::UpdateEstimate(int64_t now_ms) {
  BandwidthUsage bw_state = kBwNormal;
  double sum_var_noise = 0.0;
  SsrcOveruseEstimatorMap::iterator it = overuse_detectors_.begin();
  while (it != overuse_detectors_.end()) {
    const int64_t time_of_last_received_packet =
        it->second->last_packet_time_ms;
    if (time_of_last_received_packet >= 0 &&
        now_ms - time_of_last_received_packet > kStreamTimeOutMs) {
      // This over-use detector hasn't received packets for |kStreamTimeOutMs|
      // milliseconds and is considered stale.
      delete it->second;
      overuse_detectors_.erase(it++);
    } else {
      sum_var_noise += it->second->estimator.var_noise();
      // Make sure that we trigger an over-use if any of the over-use detectors
      // is detecting over-use.
      if (it->second->detector.State() > bw_state) {
        bw_state = it->second->detector.State();
      }
      ++it;
    }
  }
  // We can't update the estimate if we don't have any active streams.
  if (overuse_detectors_.empty()) {
    remote_rate_.reset(RemoteRateControl::Create(
        remote_rate_->GetControlType(), remote_rate_->GetMinBitrate()));
    return;
  }
  double mean_noise_var = sum_var_noise /
      static_cast<double>(overuse_detectors_.size());
  const RateControlInput input(bw_state,
                               incoming_bitrate_.Rate(now_ms),
                               mean_noise_var);
  const RateControlRegion region = remote_rate_->Update(&input, now_ms);
  unsigned int target_bitrate = remote_rate_->UpdateBandwidthEstimate(now_ms);
  if (remote_rate_->ValidEstimate()) {
    process_interval_ms_ = remote_rate_->GetFeedbackInterval();
    std::vector<unsigned int> ssrcs;
    GetSsrcs(&ssrcs);
    observer_->OnReceiveBitrateChanged(ssrcs, target_bitrate);
  }
  for (it = overuse_detectors_.begin(); it != overuse_detectors_.end(); ++it) {
    it->second->detector.SetRateControlRegion(region);
  }
}

void RemoteBitrateEstimatorImpl::OnRttUpdate(int64_t rtt) {
  CriticalSectionScoped cs(crit_sect_.get());
  remote_rate_->SetRtt(rtt);
}

void RemoteBitrateEstimatorImpl::RemoveStream(unsigned int ssrc) {
  CriticalSectionScoped cs(crit_sect_.get());
  SsrcOveruseEstimatorMap::iterator it = overuse_detectors_.find(ssrc);
  if (it != overuse_detectors_.end()) {
    delete it->second;
    overuse_detectors_.erase(it);
  }
}

bool RemoteBitrateEstimatorImpl::LatestEstimate(
    std::vector<unsigned int>* ssrcs,
    unsigned int* bitrate_bps) const {
  CriticalSectionScoped cs(crit_sect_.get());
  assert(bitrate_bps);
  if (!remote_rate_->ValidEstimate()) {
    return false;
  }
  GetSsrcs(ssrcs);
  if (ssrcs->empty())
    *bitrate_bps = 0;
  else
    *bitrate_bps = remote_rate_->LatestEstimate();
  return true;
}

bool RemoteBitrateEstimatorImpl::GetStats(
    ReceiveBandwidthEstimatorStats* output) const {
  // Not implemented.
  return false;
}

void RemoteBitrateEstimatorImpl::GetSsrcs(
    std::vector<unsigned int>* ssrcs) const {
  assert(ssrcs);
  ssrcs->resize(overuse_detectors_.size());
  int i = 0;
  for (SsrcOveruseEstimatorMap::const_iterator it = overuse_detectors_.begin();
      it != overuse_detectors_.end(); ++it, ++i) {
    (*ssrcs)[i] = it->first;
  }
}

RemoteBitrateEstimator* RemoteBitrateEstimatorFactory::Create(
    webrtc::RemoteBitrateObserver* observer,
    webrtc::Clock* clock,
    RateControlType control_type,
    uint32_t min_bitrate_bps) const {
  LOG(LS_INFO) << "RemoteBitrateEstimatorFactory: Instantiating.";
  return new RemoteBitrateEstimatorImpl(observer, clock, control_type,
                                        min_bitrate_bps);
}
}  // namespace webrtc
