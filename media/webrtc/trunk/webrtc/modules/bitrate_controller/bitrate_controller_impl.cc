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

class RtcpBandwidthObserverImpl : public RtcpBandwidthObserver {
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

class LowRateStrategy {
 public:
  LowRateStrategy(
      SendSideBandwidthEstimation* bandwidth_estimation,
      BitrateControllerImpl::BitrateObserverConfList* bitrate_observers)
      : bandwidth_estimation_(bandwidth_estimation),
        bitrate_observers_(bitrate_observers) {}

  virtual ~LowRateStrategy() {}

  virtual void LowRateAllocation(uint32_t bitrate,
                                 uint8_t fraction_loss,
                                 uint32_t rtt,
                                 uint32_t sum_min_bitrates) = 0;

 protected:
  SendSideBandwidthEstimation* bandwidth_estimation_;
  BitrateControllerImpl::BitrateObserverConfList* bitrate_observers_;
};

class EnforceMinRateStrategy : public LowRateStrategy {
 public:
  EnforceMinRateStrategy(
      SendSideBandwidthEstimation* bandwidth_estimation,
      BitrateControllerImpl::BitrateObserverConfList* bitrate_observers)
      : LowRateStrategy(bandwidth_estimation, bitrate_observers) {}

  void LowRateAllocation(uint32_t bitrate,
                         uint8_t fraction_loss,
                         uint32_t rtt,
                         uint32_t sum_min_bitrates) {
    // Min bitrate to all observers.
    BitrateControllerImpl::BitrateObserverConfList::iterator it;
    for (it = bitrate_observers_->begin(); it != bitrate_observers_->end();
        ++it) {
      it->first->OnNetworkChanged(it->second->min_bitrate_, fraction_loss,
                                  rtt);
    }
    // Set sum of min to current send bitrate.
    bandwidth_estimation_->SetSendBitrate(sum_min_bitrates);
  }
};

class NoEnforceMinRateStrategy : public LowRateStrategy {
 public:
  NoEnforceMinRateStrategy(
      SendSideBandwidthEstimation* bandwidth_estimation,
      BitrateControllerImpl::BitrateObserverConfList* bitrate_observers)
      : LowRateStrategy(bandwidth_estimation, bitrate_observers) {}

  void LowRateAllocation(uint32_t bitrate,
                         uint8_t fraction_loss,
                         uint32_t rtt,
                         uint32_t sum_min_bitrates) {
    // Allocate up to |min_bitrate_| to one observer at a time, until
    // |bitrate| is depleted.
    uint32_t remainder = bitrate;
    BitrateControllerImpl::BitrateObserverConfList::iterator it;
    for (it = bitrate_observers_->begin(); it != bitrate_observers_->end();
        ++it) {
      uint32_t allocation = std::min(remainder, it->second->min_bitrate_);
      it->first->OnNetworkChanged(allocation, fraction_loss, rtt);
      remainder -= allocation;
    }
    // Set |bitrate| to current send bitrate.
    bandwidth_estimation_->SetSendBitrate(bitrate);
  }
};

BitrateController* BitrateController::CreateBitrateController(
    bool enforce_min_bitrate) {
  return new BitrateControllerImpl(enforce_min_bitrate);
}

BitrateControllerImpl::BitrateControllerImpl(bool enforce_min_bitrate)
    : critsect_(CriticalSectionWrapper::CreateCriticalSection()) {
  if (enforce_min_bitrate) {
    low_rate_strategy_.reset(new EnforceMinRateStrategy(
        &bandwidth_estimation_, &bitrate_observers_));
  } else {
    low_rate_strategy_.reset(new NoEnforceMinRateStrategy(
        &bandwidth_estimation_, &bitrate_observers_));
  }
}

BitrateControllerImpl::~BitrateControllerImpl() {
  BitrateObserverConfList::iterator it =
      bitrate_observers_.begin();
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
  } else {
    // Add new settings.
    bitrate_observers_.push_back(BitrateObserverConfiguration(observer,
        new BitrateConfiguration(start_bitrate, min_bitrate, max_bitrate)));
  }
  uint32_t sum_start_bitrate = 0;
  uint32_t sum_min_bitrate = 0;
  uint32_t sum_max_bitrate = 0;

  // Summarize all configurations.
  for (it = bitrate_observers_.begin(); it != bitrate_observers_.end(); ++it) {
    sum_start_bitrate += it->second->start_bitrate_;
    sum_min_bitrate += it->second->min_bitrate_;
    sum_max_bitrate += it->second->max_bitrate_;
  }
  // Only change start bitrate if we have exactly one observer. By definition
  // you can only have one start bitrate, once we have our first estimate we
  // will adapt from there.
  if (bitrate_observers_.size() == 1) {
    bandwidth_estimation_.SetSendBitrate(sum_start_bitrate);
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
  }
}

void BitrateControllerImpl::EnforceMinBitrate(bool enforce_min_bitrate) {
  CriticalSectionScoped cs(critsect_);
  if (enforce_min_bitrate) {
    low_rate_strategy_.reset(new EnforceMinRateStrategy(
        &bandwidth_estimation_, &bitrate_observers_));
  } else {
    low_rate_strategy_.reset(new NoEnforceMinRateStrategy(
        &bandwidth_estimation_, &bitrate_observers_));
  }
}

void BitrateControllerImpl::OnReceivedEstimatedBitrate(const uint32_t bitrate) {
  uint32_t new_bitrate = 0;
  uint8_t fraction_lost = 0;
  uint16_t rtt = 0;
  CriticalSectionScoped cs(critsect_);
  if (bandwidth_estimation_.UpdateBandwidthEstimate(bitrate,
                                                    &new_bitrate,
                                                    &fraction_lost,
                                                    &rtt)) {
    OnNetworkChanged(new_bitrate, fraction_lost, rtt);
  }
}

void BitrateControllerImpl::OnReceivedRtcpReceiverReport(
    const uint8_t fraction_loss,
    const uint32_t rtt,
    const int number_of_packets,
    const uint32_t now_ms) {
  uint32_t new_bitrate = 0;
  uint8_t loss = fraction_loss;
  CriticalSectionScoped cs(critsect_);
  if (bandwidth_estimation_.UpdatePacketLoss(number_of_packets, rtt, now_ms,
                                             &loss, &new_bitrate)) {
    OnNetworkChanged(new_bitrate, loss, rtt);
  }
}

// We have the lock here.
void BitrateControllerImpl::OnNetworkChanged(const uint32_t bitrate,
                                             const uint8_t fraction_loss,
                                             const uint32_t rtt) {
  // Sanity check.
  uint32_t number_of_observers = bitrate_observers_.size();
  if (number_of_observers == 0) {
    return;
  }
  uint32_t sum_min_bitrates = 0;
  BitrateObserverConfList::iterator it;
  for (it = bitrate_observers_.begin(); it != bitrate_observers_.end(); ++it) {
    sum_min_bitrates += it->second->min_bitrate_;
  }
  if (bitrate <= sum_min_bitrates) {
    return low_rate_strategy_->LowRateAllocation(bitrate, fraction_loss, rtt,
                                                 sum_min_bitrates);
  }
  uint32_t bitrate_per_observer = (bitrate - sum_min_bitrates) /
      number_of_observers;
  // Use map to sort list based on max bitrate.
  ObserverSortingMap list_max_bitrates;
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

bool BitrateControllerImpl::AvailableBandwidth(uint32_t* bandwidth) const {
  return bandwidth_estimation_.AvailableBandwidth(bandwidth);
}

}  // namespace webrtc
