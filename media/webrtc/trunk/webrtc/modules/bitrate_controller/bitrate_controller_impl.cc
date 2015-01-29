/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/bitrate_controller/bitrate_controller_impl.h"

#include <algorithm>
#include <utility>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"

namespace webrtc {

class BitrateControllerImpl::RtcpBandwidthObserverImpl
    : public RtcpBandwidthObserver {
 public:
  explicit RtcpBandwidthObserverImpl(BitrateControllerImpl* owner)
      : owner_(owner) {
  }
  virtual ~RtcpBandwidthObserverImpl() {
  }
  // Received RTCP REMB or TMMBR.
  virtual void OnReceivedEstimatedBitrate(const uint32_t bitrate) OVERRIDE {
    owner_->OnReceivedEstimatedBitrate(bitrate);
  }
  // Received RTCP receiver block.
  virtual void OnReceivedRtcpReceiverReport(
      const ReportBlockList& report_blocks,
      uint16_t rtt,
      int64_t now_ms) OVERRIDE {
    if (report_blocks.empty())
      return;

    int fraction_lost_aggregate = 0;
    int total_number_of_packets = 0;

    // Compute the a weighted average of the fraction loss from all report
    // blocks.
    for (ReportBlockList::const_iterator it = report_blocks.begin();
        it != report_blocks.end(); ++it) {
      std::map<uint32_t, uint32_t>::iterator seq_num_it =
          ssrc_to_last_received_extended_high_seq_num_.find(it->sourceSSRC);

      int number_of_packets = 0;
      if (seq_num_it != ssrc_to_last_received_extended_high_seq_num_.end())
        number_of_packets = it->extendedHighSeqNum -
            seq_num_it->second;

      fraction_lost_aggregate += number_of_packets * it->fractionLost;
      total_number_of_packets += number_of_packets;

      // Update last received for this SSRC.
      ssrc_to_last_received_extended_high_seq_num_[it->sourceSSRC] =
          it->extendedHighSeqNum;
    }
    if (total_number_of_packets == 0)
      fraction_lost_aggregate = 0;
    else
      fraction_lost_aggregate  = (fraction_lost_aggregate +
          total_number_of_packets / 2) / total_number_of_packets;
    if (fraction_lost_aggregate > 255)
      return;

    owner_->OnReceivedRtcpReceiverReport(fraction_lost_aggregate, rtt,
                                         total_number_of_packets, now_ms);
  }

 private:
  std::map<uint32_t, uint32_t> ssrc_to_last_received_extended_high_seq_num_;
  BitrateControllerImpl* owner_;
};

BitrateController* BitrateController::CreateBitrateController(
    Clock* clock,
    bool enforce_min_bitrate) {
  return new BitrateControllerImpl(clock, enforce_min_bitrate);
}

BitrateControllerImpl::BitrateControllerImpl(Clock* clock, bool enforce_min_bitrate)
    : clock_(clock),
      last_bitrate_update_ms_(clock_->TimeInMilliseconds()),
      critsect_(CriticalSectionWrapper::CreateCriticalSection()),
      bandwidth_estimation_(),
      bitrate_observers_(),
      enforce_min_bitrate_(enforce_min_bitrate),
      reserved_bitrate_bps_(0),
      last_bitrate_bps_(0),
      last_fraction_loss_(0),
      last_rtt_ms_(0),
      last_enforce_min_bitrate_(!enforce_min_bitrate_),
      bitrate_observers_modified_(false),
      last_reserved_bitrate_bps_(0) {}

BitrateControllerImpl::~BitrateControllerImpl() {
  BitrateObserverConfList::iterator it = bitrate_observers_.begin();
  while (it != bitrate_observers_.end()) {
    delete it->second;
    bitrate_observers_.erase(it);
    it = bitrate_observers_.begin();
  }
  delete critsect_;
}

RtcpBandwidthObserver* BitrateControllerImpl::CreateRtcpBandwidthObserver() {
  return new RtcpBandwidthObserverImpl(this);
}

BitrateControllerImpl::BitrateObserverConfList::iterator
BitrateControllerImpl::FindObserverConfigurationPair(const BitrateObserver*
                                                     observer) {
  BitrateObserverConfList::iterator it = bitrate_observers_.begin();
  for (; it != bitrate_observers_.end(); ++it) {
    if (it->first == observer) {
      return it;
    }
  }
  return bitrate_observers_.end();
}

void BitrateControllerImpl::SetBitrateObserver(
    BitrateObserver* observer,
    const uint32_t start_bitrate,
    const uint32_t min_bitrate,
    const uint32_t max_bitrate) {
  CriticalSectionScoped cs(critsect_);

  BitrateObserverConfList::iterator it = FindObserverConfigurationPair(
      observer);

  if (it != bitrate_observers_.end()) {
    // Update current configuration.
    it->second->start_bitrate_ = start_bitrate;
    it->second->min_bitrate_ = min_bitrate;
    it->second->max_bitrate_ = max_bitrate;
    // Set the send-side bandwidth to the max of the sum of start bitrates and
    // the current estimate, so that if the user wants to immediately use more
    // bandwidth, that can be enforced.
    uint32_t sum_start_bitrate = 0;
    BitrateObserverConfList::iterator it;
    for (it = bitrate_observers_.begin(); it != bitrate_observers_.end();
         ++it) {
      sum_start_bitrate += it->second->start_bitrate_;
    }
    uint32_t current_estimate;
    uint8_t loss;
    uint32_t rtt;
    bandwidth_estimation_.CurrentEstimate(&current_estimate, &loss, &rtt);
    bandwidth_estimation_.SetSendBitrate(std::max(sum_start_bitrate,
                                                  current_estimate));
  } else {
    // Add new settings.
    bitrate_observers_.push_back(BitrateObserverConfiguration(observer,
        new BitrateConfiguration(start_bitrate, min_bitrate, max_bitrate)));
    bitrate_observers_modified_ = true;

    // TODO(andresp): This is a ugly way to set start bitrate.
    //
    // Only change start bitrate if we have exactly one observer. By definition
    // you can only have one start bitrate, once we have our first estimate we
    // will adapt from there.
    if (bitrate_observers_.size() == 1) {
      bandwidth_estimation_.SetSendBitrate(start_bitrate);
    }
  }

  UpdateMinMaxBitrate();
}

void BitrateControllerImpl::UpdateMinMaxBitrate() {
  uint32_t sum_min_bitrate = 0;
  uint32_t sum_max_bitrate = 0;
  BitrateObserverConfList::iterator it;
  for (it = bitrate_observers_.begin(); it != bitrate_observers_.end(); ++it) {
    sum_min_bitrate += it->second->min_bitrate_;
    sum_max_bitrate += it->second->max_bitrate_;
  }
  if (sum_max_bitrate == 0) {
    // No max configured use 1Gbit/s.
    sum_max_bitrate = 1000000000;
  }
  if (enforce_min_bitrate_ == false) {
    // If not enforcing min bitrate, allow the bandwidth estimation to
    // go as low as 10 kbps.
    sum_min_bitrate = std::min(sum_min_bitrate, 10000u);
  }
  bandwidth_estimation_.SetMinMaxBitrate(sum_min_bitrate,
                                         sum_max_bitrate);
}

void BitrateControllerImpl::RemoveBitrateObserver(BitrateObserver* observer) {
  CriticalSectionScoped cs(critsect_);
  BitrateObserverConfList::iterator it = FindObserverConfigurationPair(
      observer);
  if (it != bitrate_observers_.end()) {
    delete it->second;
    bitrate_observers_.erase(it);
    bitrate_observers_modified_ = true;
  }
}

void BitrateControllerImpl::EnforceMinBitrate(bool enforce_min_bitrate) {
  CriticalSectionScoped cs(critsect_);
  enforce_min_bitrate_ = enforce_min_bitrate;
  UpdateMinMaxBitrate();
}

void BitrateControllerImpl::SetReservedBitrate(uint32_t reserved_bitrate_bps) {
  CriticalSectionScoped cs(critsect_);
  reserved_bitrate_bps_ = reserved_bitrate_bps;
  MaybeTriggerOnNetworkChanged();
}

void BitrateControllerImpl::OnReceivedEstimatedBitrate(const uint32_t bitrate) {
  CriticalSectionScoped cs(critsect_);
  bandwidth_estimation_.UpdateReceiverEstimate(bitrate);
  MaybeTriggerOnNetworkChanged();
}

int32_t BitrateControllerImpl::TimeUntilNextProcess() {
  enum { kBitrateControllerUpdateIntervalMs = 25 };
  CriticalSectionScoped cs(critsect_);
  int time_since_update_ms =
      clock_->TimeInMilliseconds() - last_bitrate_update_ms_;
  return std::max(0, kBitrateControllerUpdateIntervalMs - time_since_update_ms);
}

int32_t BitrateControllerImpl::Process() {
  if (TimeUntilNextProcess() > 0)
    return 0;
  {
    CriticalSectionScoped cs(critsect_);
    bandwidth_estimation_.UpdateEstimate(clock_->TimeInMilliseconds());
    MaybeTriggerOnNetworkChanged();
  }
  last_bitrate_update_ms_ = clock_->TimeInMilliseconds();
  return 0;
}

void BitrateControllerImpl::OnReceivedRtcpReceiverReport(
    const uint8_t fraction_loss,
    const uint32_t rtt,
    const int number_of_packets,
    const uint32_t now_ms) {
  CriticalSectionScoped cs(critsect_);
  bandwidth_estimation_.UpdateReceiverBlock(
      fraction_loss, rtt, number_of_packets, now_ms);
  MaybeTriggerOnNetworkChanged();
}

void BitrateControllerImpl::MaybeTriggerOnNetworkChanged() {
  uint32_t bitrate;
  uint8_t fraction_loss;
  uint32_t rtt;
  bandwidth_estimation_.CurrentEstimate(&bitrate, &fraction_loss, &rtt);
  bitrate -= std::min(bitrate, reserved_bitrate_bps_);

  if (bitrate_observers_modified_ ||
      bitrate != last_bitrate_bps_ ||
      fraction_loss != last_fraction_loss_ ||
      rtt != last_rtt_ms_ ||
      last_enforce_min_bitrate_ != enforce_min_bitrate_ ||
      last_reserved_bitrate_bps_ != reserved_bitrate_bps_) {
    last_bitrate_bps_ = bitrate;
    last_fraction_loss_ = fraction_loss;
    last_rtt_ms_ = rtt;
    last_enforce_min_bitrate_ = enforce_min_bitrate_;
    last_reserved_bitrate_bps_ = reserved_bitrate_bps_;
    bitrate_observers_modified_ = false;
    OnNetworkChanged(bitrate, fraction_loss, rtt);
  }
}

void BitrateControllerImpl::OnNetworkChanged(const uint32_t bitrate,
                                             const uint8_t fraction_loss,
                                             const uint32_t rtt) {
  // Sanity check.
  if (bitrate_observers_.empty())
    return;

  uint32_t sum_min_bitrates = 0;
  BitrateObserverConfList::iterator it;
  for (it = bitrate_observers_.begin(); it != bitrate_observers_.end(); ++it) {
    sum_min_bitrates += it->second->min_bitrate_;
  }
  if (bitrate <= sum_min_bitrates)
    return LowRateAllocation(bitrate, fraction_loss, rtt, sum_min_bitrates);
  else
    return NormalRateAllocation(bitrate, fraction_loss, rtt, sum_min_bitrates);
}

void BitrateControllerImpl::NormalRateAllocation(uint32_t bitrate,
                                                 uint8_t fraction_loss,
                                                 uint32_t rtt,
                                                 uint32_t sum_min_bitrates) {
  uint32_t number_of_observers = bitrate_observers_.size();
  uint32_t bitrate_per_observer = (bitrate - sum_min_bitrates) /
      number_of_observers;
  // Use map to sort list based on max bitrate.
  ObserverSortingMap list_max_bitrates;
  BitrateObserverConfList::iterator it;
  for (it = bitrate_observers_.begin(); it != bitrate_observers_.end(); ++it) {
    list_max_bitrates.insert(std::pair<uint32_t, ObserverConfiguration*>(
        it->second->max_bitrate_,
        new ObserverConfiguration(it->first, it->second->min_bitrate_)));
  }
  ObserverSortingMap::iterator max_it = list_max_bitrates.begin();
  while (max_it != list_max_bitrates.end()) {
    number_of_observers--;
    uint32_t observer_allowance = max_it->second->min_bitrate_ +
        bitrate_per_observer;
    if (max_it->first < observer_allowance) {
      // We have more than enough for this observer.
      // Carry the remainder forward.
      uint32_t remainder = observer_allowance - max_it->first;
      if (number_of_observers != 0) {
        bitrate_per_observer += remainder / number_of_observers;
      }
      max_it->second->observer_->OnNetworkChanged(max_it->first, fraction_loss,
                                                  rtt);
    } else {
      max_it->second->observer_->OnNetworkChanged(observer_allowance,
                                                  fraction_loss, rtt);
    }
    delete max_it->second;
    list_max_bitrates.erase(max_it);
    // Prepare next iteration.
    max_it = list_max_bitrates.begin();
  }
}

void BitrateControllerImpl::LowRateAllocation(uint32_t bitrate,
                                              uint8_t fraction_loss,
                                              uint32_t rtt,
                                              uint32_t sum_min_bitrates) {
  if (enforce_min_bitrate_) {
    // Min bitrate to all observers.
    BitrateControllerImpl::BitrateObserverConfList::iterator it;
    for (it = bitrate_observers_.begin(); it != bitrate_observers_.end();
         ++it) {
      it->first->OnNetworkChanged(it->second->min_bitrate_, fraction_loss, rtt);
    }
    // Set sum of min to current send bitrate.
    bandwidth_estimation_.SetSendBitrate(sum_min_bitrates);
  } else {
    // Allocate up to |min_bitrate_| to one observer at a time, until
    // |bitrate| is depleted.
    uint32_t remainder = bitrate;
    BitrateControllerImpl::BitrateObserverConfList::iterator it;
    for (it = bitrate_observers_.begin(); it != bitrate_observers_.end();
         ++it) {
      uint32_t allocation = std::min(remainder, it->second->min_bitrate_);
      it->first->OnNetworkChanged(allocation, fraction_loss, rtt);
      remainder -= allocation;
    }
    // Set |bitrate| to current send bitrate.
    bandwidth_estimation_.SetSendBitrate(bitrate);
  }
}

bool BitrateControllerImpl::AvailableBandwidth(uint32_t* bandwidth) const {
  CriticalSectionScoped cs(critsect_);
  uint32_t bitrate;
  uint8_t fraction_loss;
  uint32_t rtt;
  bandwidth_estimation_.CurrentEstimate(&bitrate, &fraction_loss, &rtt);
  if (bitrate) {
    *bandwidth = bitrate - std::min(bitrate, reserved_bitrate_bps_);
    return true;
  }
  return false;
}

}  // namespace webrtc
