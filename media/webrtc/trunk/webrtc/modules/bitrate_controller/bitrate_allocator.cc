/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/bitrate_controller/include/bitrate_allocator.h"

#include <algorithm>
#include <utility>

#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"

namespace webrtc {

// Allow packets to be transmitted in up to 2 times max video bitrate if the
// bandwidth estimate allows it.
const int kTransmissionMaxBitrateMultiplier = 2;
const int kDefaultBitrateBps = 300000;

BitrateAllocator::BitrateAllocator()
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      bitrate_observers_(),
      enforce_min_bitrate_(true),
      last_bitrate_bps_(kDefaultBitrateBps),
      last_fraction_loss_(0),
      last_rtt_(0) {
}


void BitrateAllocator::OnNetworkChanged(uint32_t bitrate,
                                        uint8_t fraction_loss,
                                        int64_t rtt) {
  CriticalSectionScoped lock(crit_sect_.get());
  last_bitrate_bps_ = bitrate;
  last_fraction_loss_ = fraction_loss;
  last_rtt_ = rtt;
  ObserverBitrateMap allocation = AllocateBitrates();
  for (const auto& kv : allocation)
    kv.first->OnNetworkChanged(kv.second, last_fraction_loss_, last_rtt_);
}

BitrateAllocator::ObserverBitrateMap BitrateAllocator::AllocateBitrates() {
  if (bitrate_observers_.empty())
    return ObserverBitrateMap();

  uint32_t sum_min_bitrates = 0;
  for (const auto& observer : bitrate_observers_)
    sum_min_bitrates += observer.second.min_bitrate_;
  if (last_bitrate_bps_ <= sum_min_bitrates)
    return LowRateAllocation(last_bitrate_bps_);
  else
    return NormalRateAllocation(last_bitrate_bps_, sum_min_bitrates);
}

int BitrateAllocator::AddBitrateObserver(BitrateObserver* observer,
                                         uint32_t start_bitrate_bps,
                                         uint32_t min_bitrate_bps,
                                         uint32_t max_bitrate_bps,
                                         int* new_observer_bitrate_bps) {
  CriticalSectionScoped lock(crit_sect_.get());

  BitrateObserverConfList::iterator it =
      FindObserverConfigurationPair(observer);

  // Allow the max bitrate to be exceeded for FEC and retransmissions.
  // TODO(holmer): We have to get rid of this hack as it makes it difficult to
  // properly allocate bitrate. The allocator should instead distribute any
  // extra bitrate after all streams have maxed out.
  max_bitrate_bps *= kTransmissionMaxBitrateMultiplier;
  int new_bwe_candidate_bps = 0;
  if (it != bitrate_observers_.end()) {
    // Update current configuration.
    it->second.start_bitrate_ = start_bitrate_bps;
    it->second.min_bitrate_ = min_bitrate_bps;
    it->second.max_bitrate_ = max_bitrate_bps;
    // Set the send-side bandwidth to the max of the sum of start bitrates and
    // the current estimate, so that if the user wants to immediately use more
    // bandwidth, that can be enforced.
    for (const auto& observer : bitrate_observers_)
      new_bwe_candidate_bps += observer.second.start_bitrate_;
  } else {
    // Add new settings.
    bitrate_observers_.push_back(BitrateObserverConfiguration(
        observer, BitrateConfiguration(start_bitrate_bps, min_bitrate_bps,
                                       max_bitrate_bps)));
    bitrate_observers_modified_ = true;

    // TODO(andresp): This is a ugly way to set start bitrate.
    //
    // Only change start bitrate if we have exactly one observer. By definition
    // you can only have one start bitrate, once we have our first estimate we
    // will adapt from there.
    if (bitrate_observers_.size() == 1)
      new_bwe_candidate_bps = start_bitrate_bps;
  }

  last_bitrate_bps_ = std::max<int>(new_bwe_candidate_bps, last_bitrate_bps_);

  ObserverBitrateMap allocation = AllocateBitrates();
  *new_observer_bitrate_bps = 0;
  for (auto& kv : allocation) {
    kv.first->OnNetworkChanged(kv.second, last_fraction_loss_, last_rtt_);
    if (kv.first == observer)
      *new_observer_bitrate_bps = kv.second;
  }
  return last_bitrate_bps_;
}

void BitrateAllocator::RemoveBitrateObserver(BitrateObserver* observer) {
  CriticalSectionScoped lock(crit_sect_.get());
  BitrateObserverConfList::iterator it =
      FindObserverConfigurationPair(observer);
  if (it != bitrate_observers_.end()) {
    bitrate_observers_.erase(it);
    bitrate_observers_modified_ = true;
  }
}

void BitrateAllocator::GetMinMaxBitrateSumBps(int* min_bitrate_sum_bps,
                                              int* max_bitrate_sum_bps) const {
  *min_bitrate_sum_bps = 0;
  *max_bitrate_sum_bps = 0;

  CriticalSectionScoped lock(crit_sect_.get());
  for (const auto& observer : bitrate_observers_) {
    *min_bitrate_sum_bps += observer.second.min_bitrate_;
    *max_bitrate_sum_bps += observer.second.max_bitrate_;
  }
}

BitrateAllocator::BitrateObserverConfList::iterator
BitrateAllocator::FindObserverConfigurationPair(
    const BitrateObserver* observer) {
  for (auto it = bitrate_observers_.begin(); it != bitrate_observers_.end();
       ++it) {
    if (it->first == observer)
      return it;
  }
  return bitrate_observers_.end();
}

void BitrateAllocator::EnforceMinBitrate(bool enforce_min_bitrate) {
  CriticalSectionScoped lock(crit_sect_.get());
  enforce_min_bitrate_ = enforce_min_bitrate;
}

BitrateAllocator::ObserverBitrateMap BitrateAllocator::NormalRateAllocation(
    uint32_t bitrate,
    uint32_t sum_min_bitrates) {
  uint32_t number_of_observers = bitrate_observers_.size();
  uint32_t bitrate_per_observer =
      (bitrate - sum_min_bitrates) / number_of_observers;
  // Use map to sort list based on max bitrate.
  ObserverSortingMap list_max_bitrates;
  for (const auto& observer : bitrate_observers_) {
    list_max_bitrates.insert(std::pair<uint32_t, ObserverConfiguration>(
        observer.second.max_bitrate_,
        ObserverConfiguration(observer.first, observer.second.min_bitrate_)));
  }
  ObserverBitrateMap allocation;
  ObserverSortingMap::iterator max_it = list_max_bitrates.begin();
  while (max_it != list_max_bitrates.end()) {
    number_of_observers--;
    uint32_t observer_allowance =
        max_it->second.min_bitrate_ + bitrate_per_observer;
    if (max_it->first < observer_allowance) {
      // We have more than enough for this observer.
      // Carry the remainder forward.
      uint32_t remainder = observer_allowance - max_it->first;
      if (number_of_observers != 0) {
        bitrate_per_observer += remainder / number_of_observers;
      }
      allocation[max_it->second.observer_] = max_it->first;
    } else {
      allocation[max_it->second.observer_] = observer_allowance;
    }
    list_max_bitrates.erase(max_it);
    // Prepare next iteration.
    max_it = list_max_bitrates.begin();
  }
  return allocation;
}

BitrateAllocator::ObserverBitrateMap BitrateAllocator::LowRateAllocation(
    uint32_t bitrate) {
  ObserverBitrateMap allocation;
  if (enforce_min_bitrate_) {
    // Min bitrate to all observers.
    for (const auto& observer : bitrate_observers_)
      allocation[observer.first] = observer.second.min_bitrate_;
  } else {
    // Allocate up to |min_bitrate_| to one observer at a time, until
    // |bitrate| is depleted.
    uint32_t remainder = bitrate;
    for (const auto& observer : bitrate_observers_) {
      uint32_t allocated_bitrate =
          std::min(remainder, observer.second.min_bitrate_);
      allocation[observer.first] = allocated_bitrate;
      remainder -= allocated_bitrate;
    }
  }
  return allocation;
}
}  // namespace webrtc
