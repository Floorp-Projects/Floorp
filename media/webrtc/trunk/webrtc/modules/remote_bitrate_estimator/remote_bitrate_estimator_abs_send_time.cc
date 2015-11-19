/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <map>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/inter_arrival.h"
#include "webrtc/modules/remote_bitrate_estimator/overuse_detector.h"
#include "webrtc/modules/remote_bitrate_estimator/overuse_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_rate_control.h"
#include "webrtc/modules/remote_bitrate_estimator/rate_statistics.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/typedefs.h"

namespace webrtc {

enum {
  kTimestampGroupLengthMs = 5,
  kAbsSendTimeFraction = 18,
  kAbsSendTimeInterArrivalUpshift = 8,
  kInterArrivalShift = kAbsSendTimeFraction + kAbsSendTimeInterArrivalUpshift,
  kInitialProbingIntervalMs = 2000,
  kMinClusterSize = 4,
  kMaxProbePackets = 15,
  kExpectedNumberOfProbes = 3
};

static const size_t kPropagationDeltaQueueMaxSize = 1000;
static const int64_t kPropagationDeltaQueueMaxTimeMs = 1000;
static const double kTimestampToMs = 1000.0 /
    static_cast<double>(1 << kInterArrivalShift);

// Removes the entries at |index| of |time| and |value|, if time[index] is
// smaller than or equal to |deadline|. |time| must be sorted ascendingly.
static void RemoveStaleEntries(
    std::vector<int64_t>* time, std::vector<int>* value, int64_t deadline) {
  assert(time->size() == value->size());
  std::vector<int64_t>::iterator end_of_removal = std::upper_bound(
      time->begin(), time->end(), deadline);
  size_t end_of_removal_index = end_of_removal - time->begin();

  time->erase(time->begin(), end_of_removal);
  value->erase(value->begin(), value->begin() + end_of_removal_index);
}

template<typename K, typename V>
std::vector<K> Keys(const std::map<K, V>& map) {
  std::vector<K> keys;
  keys.reserve(map.size());
  for (typename std::map<K, V>::const_iterator it = map.begin();
      it != map.end(); ++it) {
    keys.push_back(it->first);
  }
  return keys;
}

struct Probe {
  Probe(int64_t send_time_ms, int64_t recv_time_ms, size_t payload_size)
      : send_time_ms(send_time_ms),
        recv_time_ms(recv_time_ms),
        payload_size(payload_size) {}
  int64_t send_time_ms;
  int64_t recv_time_ms;
  size_t payload_size;
};

struct Cluster {
  Cluster()
      : send_mean_ms(0.0f),
        recv_mean_ms(0.0f),
        mean_size(0),
        count(0),
        num_above_min_delta(0) {}

  int GetSendBitrateBps() const {
    assert(send_mean_ms > 0);
    return mean_size * 8 * 1000 / send_mean_ms;
  }

  int GetRecvBitrateBps() const {
    assert(recv_mean_ms > 0);
    return mean_size * 8 * 1000 / recv_mean_ms;
  }

  float send_mean_ms;
  float recv_mean_ms;
  // TODO(holmer): Add some variance metric as well?
  size_t mean_size;
  int count;
  int num_above_min_delta;
};

class RemoteBitrateEstimatorAbsSendTimeImpl : public RemoteBitrateEstimator {
 public:
  RemoteBitrateEstimatorAbsSendTimeImpl(RemoteBitrateObserver* observer,
                                        Clock* clock,
                                        RateControlType control_type,
                                        uint32_t min_bitrate_bps);
  virtual ~RemoteBitrateEstimatorAbsSendTimeImpl() {}

  void IncomingPacketFeedbackVector(
      const std::vector<PacketInfo>& packet_feedback_vector) override;

  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header) override;
  // This class relies on Process() being called periodically (at least once
  // every other second) for streams to be timed out properly. Therefore it
  // shouldn't be detached from the ProcessThread except if it's about to be
  // deleted.
  int32_t Process() override;
  int64_t TimeUntilNextProcess() override;
  void OnRttUpdate(int64_t rtt) override;
  void RemoveStream(unsigned int ssrc) override;
  bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                      unsigned int* bitrate_bps) const override;
  bool GetStats(ReceiveBandwidthEstimatorStats* output) const override;

 private:
  typedef std::map<unsigned int, int64_t> Ssrcs;

  static bool IsWithinClusterBounds(int send_delta_ms,
                                    const Cluster& cluster_aggregate) {
    if (cluster_aggregate.count == 0)
      return true;
    float cluster_mean = cluster_aggregate.send_mean_ms /
                         static_cast<float>(cluster_aggregate.count);
    return fabs(static_cast<float>(send_delta_ms) - cluster_mean) < 2.5f;
  }

  static void AddCluster(std::list<Cluster>* clusters, Cluster* cluster) {
    cluster->send_mean_ms /= static_cast<float>(cluster->count);
    cluster->recv_mean_ms /= static_cast<float>(cluster->count);
    cluster->mean_size /= cluster->count;
    clusters->push_back(*cluster);
  }

  int Id() const {
    return static_cast<int>(reinterpret_cast<uint64_t>(this));
  }

  void IncomingPacketInfo(int64_t arrival_time_ms,
                          uint32_t send_time_24bits,
                          size_t payload_size,
                          uint32_t ssrc);

  bool IsProbe(int64_t send_time_ms, int payload_size) const
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get());

  // Triggers a new estimate calculation.
  void UpdateEstimate(int64_t now_ms)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get());

  void UpdateStats(int propagation_delta_ms, int64_t now_ms)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get());

  void ComputeClusters(std::list<Cluster>* clusters) const;

  std::list<Cluster>::const_iterator FindBestProbe(
      const std::list<Cluster>& clusters) const
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get());

  void ProcessClusters(int64_t now_ms)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get());

  bool IsBitrateImproving(int probe_bitrate_bps) const
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get());

  rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;
  RemoteBitrateObserver* observer_ GUARDED_BY(crit_sect_.get());
  Clock* clock_;
  Ssrcs ssrcs_ GUARDED_BY(crit_sect_.get());
  rtc::scoped_ptr<InterArrival> inter_arrival_ GUARDED_BY(crit_sect_.get());
  OveruseEstimator estimator_ GUARDED_BY(crit_sect_.get());
  OveruseDetector detector_ GUARDED_BY(crit_sect_.get());
  RateStatistics incoming_bitrate_ GUARDED_BY(crit_sect_.get());
  rtc::scoped_ptr<RemoteRateControl> remote_rate_ GUARDED_BY(crit_sect_.get());
  int64_t last_process_time_;
  std::vector<int> recent_propagation_delta_ms_ GUARDED_BY(crit_sect_.get());
  std::vector<int64_t> recent_update_time_ms_ GUARDED_BY(crit_sect_.get());
  int64_t process_interval_ms_ GUARDED_BY(crit_sect_.get());
  int total_propagation_delta_ms_ GUARDED_BY(crit_sect_.get());

  std::list<Probe> probes_;
  size_t total_probes_received_;
  int64_t first_packet_time_ms_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RemoteBitrateEstimatorAbsSendTimeImpl);
};

RemoteBitrateEstimatorAbsSendTimeImpl::RemoteBitrateEstimatorAbsSendTimeImpl(
    RemoteBitrateObserver* observer,
    Clock* clock,
    RateControlType control_type,
    uint32_t min_bitrate_bps)
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      observer_(observer),
      clock_(clock),
      ssrcs_(),
      inter_arrival_(),
      estimator_(OverUseDetectorOptions()),
      detector_(OverUseDetectorOptions()),
      incoming_bitrate_(1000, 8000),
      remote_rate_(RemoteRateControl::Create(control_type, min_bitrate_bps)),
      last_process_time_(-1),
      process_interval_ms_(kProcessIntervalMs),
      total_propagation_delta_ms_(0),
      total_probes_received_(0),
      first_packet_time_ms_(-1) {
  assert(observer_);
  assert(clock_);
}

void RemoteBitrateEstimatorAbsSendTimeImpl::ComputeClusters(
    std::list<Cluster>* clusters) const {
  Cluster current;
  int64_t prev_send_time = -1;
  int64_t prev_recv_time = -1;
  for (std::list<Probe>::const_iterator it = probes_.begin();
       it != probes_.end();
       ++it) {
    if (prev_send_time >= 0) {
      int send_delta_ms = it->send_time_ms - prev_send_time;
      int recv_delta_ms = it->recv_time_ms - prev_recv_time;
      if (send_delta_ms >= 1 && recv_delta_ms >= 1) {
        ++current.num_above_min_delta;
      }
      if (!IsWithinClusterBounds(send_delta_ms, current)) {
        if (current.count >= kMinClusterSize)
          AddCluster(clusters, &current);
        current = Cluster();
      }
      current.send_mean_ms += send_delta_ms;
      current.recv_mean_ms += recv_delta_ms;
      current.mean_size += it->payload_size;
      ++current.count;
    }
    prev_send_time = it->send_time_ms;
    prev_recv_time = it->recv_time_ms;
  }
  if (current.count >= kMinClusterSize)
    AddCluster(clusters, &current);
}

std::list<Cluster>::const_iterator
RemoteBitrateEstimatorAbsSendTimeImpl::FindBestProbe(
    const std::list<Cluster>& clusters) const {
  int highest_probe_bitrate_bps = 0;
  std::list<Cluster>::const_iterator best_it = clusters.end();
  for (std::list<Cluster>::const_iterator it = clusters.begin();
       it != clusters.end();
       ++it) {
    if (it->send_mean_ms == 0 || it->recv_mean_ms == 0)
      continue;
    int send_bitrate_bps = it->mean_size * 8 * 1000 / it->send_mean_ms;
    int recv_bitrate_bps = it->mean_size * 8 * 1000 / it->recv_mean_ms;
    if (it->num_above_min_delta > it->count / 2 &&
        (it->recv_mean_ms - it->send_mean_ms <= 2.0f &&
         it->send_mean_ms - it->recv_mean_ms <= 5.0f)) {
      int probe_bitrate_bps =
          std::min(it->GetSendBitrateBps(), it->GetRecvBitrateBps());
      if (probe_bitrate_bps > highest_probe_bitrate_bps) {
        highest_probe_bitrate_bps = probe_bitrate_bps;
        best_it = it;
      }
    } else {
      LOG(LS_INFO) << "Probe failed, sent at " << send_bitrate_bps
                   << " bps, received at " << recv_bitrate_bps
                   << " bps. Mean send delta: " << it->send_mean_ms
                   << " ms, mean recv delta: " << it->recv_mean_ms
                   << " ms, num probes: " << it->count;
      break;
    }
  }
  return best_it;
}

void RemoteBitrateEstimatorAbsSendTimeImpl::ProcessClusters(int64_t now_ms) {
  std::list<Cluster> clusters;
  ComputeClusters(&clusters);
  if (clusters.empty()) {
    // If we reach the max number of probe packets and still have no clusters,
    // we will remove the oldest one.
    if (probes_.size() >= kMaxProbePackets)
      probes_.pop_front();
    return;
  }

  std::list<Cluster>::const_iterator best_it = FindBestProbe(clusters);
  if (best_it != clusters.end()) {
    int probe_bitrate_bps =
        std::min(best_it->GetSendBitrateBps(), best_it->GetRecvBitrateBps());
    if (IsBitrateImproving(probe_bitrate_bps)) {
      LOG(LS_INFO) << "Probe successful, sent at "
                   << best_it->GetSendBitrateBps() << " bps, received at "
                   << best_it->GetRecvBitrateBps()
                   << " bps. Mean send delta: " << best_it->send_mean_ms
                   << " ms, mean recv delta: " << best_it->recv_mean_ms
                   << " ms, num probes: " << best_it->count;
      remote_rate_->SetEstimate(probe_bitrate_bps, now_ms);
    }
  }

  // Not probing and received non-probe packet, or finished with current set
  // of probes.
  if (clusters.size() >= kExpectedNumberOfProbes)
    probes_.clear();
}

bool RemoteBitrateEstimatorAbsSendTimeImpl::IsBitrateImproving(
    int new_bitrate_bps) const {
  bool initial_probe = !remote_rate_->ValidEstimate() && new_bitrate_bps > 0;
  bool bitrate_above_estimate =
      remote_rate_->ValidEstimate() &&
      new_bitrate_bps > static_cast<int>(remote_rate_->LatestEstimate());
  return initial_probe || bitrate_above_estimate;
}

void RemoteBitrateEstimatorAbsSendTimeImpl::IncomingPacketFeedbackVector(
    const std::vector<PacketInfo>& packet_feedback_vector) {
  for (const auto& packet_info : packet_feedback_vector) {
    // TODO(holmer): We should get rid of this conversion if possible as we may
    // lose precision.
    uint32_t send_time_32bits = (packet_info.send_time_ms) / kTimestampToMs;
    uint32_t send_time_24bits =
        send_time_32bits >> kAbsSendTimeInterArrivalUpshift;
    IncomingPacketInfo(packet_info.arrival_time_ms, send_time_24bits,
                       packet_info.payload_size, 0);
  }
}

void RemoteBitrateEstimatorAbsSendTimeImpl::IncomingPacket(
    int64_t arrival_time_ms,
    size_t payload_size,
    const RTPHeader& header) {
  if (!header.extension.hasAbsoluteSendTime) {
    LOG(LS_WARNING) << "RemoteBitrateEstimatorAbsSendTimeImpl: Incoming packet "
                       "is missing absolute send time extension!";
  }
  IncomingPacketInfo(arrival_time_ms, header.extension.absoluteSendTime,
                     payload_size, header.ssrc);
}

void RemoteBitrateEstimatorAbsSendTimeImpl::IncomingPacketInfo(
    int64_t arrival_time_ms,
    uint32_t send_time_24bits,
    size_t payload_size,
    uint32_t ssrc) {
  assert(send_time_24bits < (1ul << 24));
  // Shift up send time to use the full 32 bits that inter_arrival works with,
  // so wrapping works properly.
  uint32_t timestamp = send_time_24bits << kAbsSendTimeInterArrivalUpshift;
  int64_t send_time_ms = static_cast<int64_t>(timestamp) * kTimestampToMs;

  CriticalSectionScoped cs(crit_sect_.get());
  int64_t now_ms = clock_->TimeInMilliseconds();
  // TODO(holmer): SSRCs are only needed for REMB, should be broken out from
  // here.
  ssrcs_[ssrc] = now_ms;
  incoming_bitrate_.Update(payload_size, now_ms);
  const BandwidthUsage prior_state = detector_.State();

  if (first_packet_time_ms_ == -1)
    first_packet_time_ms_ = clock_->TimeInMilliseconds();

  uint32_t ts_delta = 0;
  int64_t t_delta = 0;
  int size_delta = 0;
  // For now only try to detect probes while we don't have a valid estimate.
  if (!remote_rate_->ValidEstimate() ||
      now_ms - first_packet_time_ms_ < kInitialProbingIntervalMs) {
    // TODO(holmer): Use a map instead to get correct order?
    if (total_probes_received_ < kMaxProbePackets) {
      int send_delta_ms = -1;
      int recv_delta_ms = -1;
      if (!probes_.empty()) {
        send_delta_ms = send_time_ms - probes_.back().send_time_ms;
        recv_delta_ms = arrival_time_ms - probes_.back().recv_time_ms;
      }
      LOG(LS_INFO) << "Probe packet received: send time=" << send_time_ms
                   << " ms, recv time=" << arrival_time_ms
                   << " ms, send delta=" << send_delta_ms
                   << " ms, recv delta=" << recv_delta_ms << " ms.";
    }
    probes_.push_back(Probe(send_time_ms, arrival_time_ms, payload_size));
    ++total_probes_received_;
    ProcessClusters(now_ms);
  }
  if (!inter_arrival_.get()) {
    inter_arrival_.reset(new InterArrival(
        (kTimestampGroupLengthMs << kInterArrivalShift) / 1000, kTimestampToMs,
        remote_rate_->GetControlType() == kAimdControl));
  }
  if (inter_arrival_->ComputeDeltas(timestamp, arrival_time_ms, payload_size,
                                    &ts_delta, &t_delta, &size_delta)) {
    double ts_delta_ms = (1000.0 * ts_delta) / (1 << kInterArrivalShift);
    estimator_.Update(t_delta, ts_delta_ms, size_delta, detector_.State());
    detector_.Detect(estimator_.offset(), ts_delta_ms,
                     estimator_.num_of_deltas());
    UpdateStats(static_cast<int>(t_delta - ts_delta_ms), now_ms);
  }
  if (detector_.State() == kBwOverusing) {
    unsigned int incoming_bitrate = incoming_bitrate_.Rate(now_ms);
    if (prior_state != kBwOverusing ||
        remote_rate_->TimeToReduceFurther(now_ms, incoming_bitrate)) {
      // The first overuse should immediately trigger a new estimate.
      // We also have to update the estimate immediately if we are overusing
      // and the target bitrate is too high compared to what we are receiving.
      UpdateEstimate(now_ms);
    }
  }
}

int32_t RemoteBitrateEstimatorAbsSendTimeImpl::Process() {
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

int64_t RemoteBitrateEstimatorAbsSendTimeImpl::TimeUntilNextProcess() {
  if (last_process_time_ < 0) {
    return 0;
  }
  {
    CriticalSectionScoped cs(crit_sect_.get());
    return last_process_time_ + process_interval_ms_ -
        clock_->TimeInMilliseconds();
  }
}

void RemoteBitrateEstimatorAbsSendTimeImpl::UpdateEstimate(int64_t now_ms) {
  if (!inter_arrival_.get()) {
    // No packets have been received on the active streams.
    return;
  }
  for (Ssrcs::iterator it = ssrcs_.begin(); it != ssrcs_.end();) {
    if ((now_ms - it->second) > kStreamTimeOutMs) {
      ssrcs_.erase(it++);
    } else {
      ++it;
    }
  }
  if (ssrcs_.empty()) {
    // We can't update the estimate if we don't have any active streams.
    inter_arrival_.reset();
    // We deliberately don't reset the first_packet_time_ms_ here for now since
    // we only probe for bandwidth in the beginning of a call right now.
    return;
  }

  const RateControlInput input(detector_.State(),
                               incoming_bitrate_.Rate(now_ms),
                               estimator_.var_noise());
  const RateControlRegion region = remote_rate_->Update(&input, now_ms);
  unsigned int target_bitrate = remote_rate_->UpdateBandwidthEstimate(now_ms);
  if (remote_rate_->ValidEstimate()) {
    process_interval_ms_ = remote_rate_->GetFeedbackInterval();
    observer_->OnReceiveBitrateChanged(Keys(ssrcs_), target_bitrate);
  }
  detector_.SetRateControlRegion(region);
}

void RemoteBitrateEstimatorAbsSendTimeImpl::OnRttUpdate(int64_t rtt) {
  CriticalSectionScoped cs(crit_sect_.get());
  remote_rate_->SetRtt(rtt);
}

void RemoteBitrateEstimatorAbsSendTimeImpl::RemoveStream(unsigned int ssrc) {
  CriticalSectionScoped cs(crit_sect_.get());
  ssrcs_.erase(ssrc);
}

bool RemoteBitrateEstimatorAbsSendTimeImpl::LatestEstimate(
    std::vector<unsigned int>* ssrcs,
    unsigned int* bitrate_bps) const {
  CriticalSectionScoped cs(crit_sect_.get());
  assert(ssrcs);
  assert(bitrate_bps);
  if (!remote_rate_->ValidEstimate()) {
    return false;
  }
  *ssrcs = Keys(ssrcs_);
  if (ssrcs_.empty()) {
    *bitrate_bps = 0;
  } else {
    *bitrate_bps = remote_rate_->LatestEstimate();
  }
  return true;
}

bool RemoteBitrateEstimatorAbsSendTimeImpl::GetStats(
    ReceiveBandwidthEstimatorStats* output) const {
  {
    CriticalSectionScoped cs(crit_sect_.get());
    output->recent_propagation_time_delta_ms = recent_propagation_delta_ms_;
    output->recent_arrival_time_ms = recent_update_time_ms_;
    output->total_propagation_time_delta_ms = total_propagation_delta_ms_;
  }
  RemoveStaleEntries(
      &output->recent_arrival_time_ms,
      &output->recent_propagation_time_delta_ms,
      clock_->TimeInMilliseconds() - kPropagationDeltaQueueMaxTimeMs);
  return true;
}

void RemoteBitrateEstimatorAbsSendTimeImpl::UpdateStats(
    int propagation_delta_ms, int64_t now_ms) {
  // The caller must enter crit_sect_ before the call.

  // Remove the oldest entry if the size limit is reached.
  if (recent_update_time_ms_.size() == kPropagationDeltaQueueMaxSize) {
    recent_update_time_ms_.erase(recent_update_time_ms_.begin());
    recent_propagation_delta_ms_.erase(recent_propagation_delta_ms_.begin());
  }

  recent_propagation_delta_ms_.push_back(propagation_delta_ms);
  recent_update_time_ms_.push_back(now_ms);

  RemoveStaleEntries(
      &recent_update_time_ms_,
      &recent_propagation_delta_ms_,
      now_ms - kPropagationDeltaQueueMaxTimeMs);

  total_propagation_delta_ms_ =
      std::max(total_propagation_delta_ms_ + propagation_delta_ms, 0);
}

RemoteBitrateEstimator* AbsoluteSendTimeRemoteBitrateEstimatorFactory::Create(
    RemoteBitrateObserver* observer,
    Clock* clock,
    RateControlType control_type,
    uint32_t min_bitrate_bps) const {
  LOG(LS_INFO) << "AbsoluteSendTimeRemoteBitrateEstimatorFactory: "
                  "Instantiating.";
  return new RemoteBitrateEstimatorAbsSendTimeImpl(observer,
                                                   clock,
                                                   control_type,
                                                   min_bitrate_bps);
}
}  // namespace webrtc
