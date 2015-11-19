/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Usage: this class will register multiple RtcpBitrateObserver's one at each
 *  RTCP module. It will aggregate the results and run one bandwidth estimation
 *  and push the result to the encoders via BitrateObserver(s).
 */

#ifndef WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_ALLOCATOR_H_
#define WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_ALLOCATOR_H_

#include <list>
#include <map>
#include <utility>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

class BitrateObserver;

class BitrateAllocator {
 public:
  BitrateAllocator();

  void OnNetworkChanged(uint32_t target_bitrate,
                        uint8_t fraction_loss,
                        int64_t rtt);

  // Set the start and max send bitrate used by the bandwidth management.
  //
  // observer, updates bitrates if already in use.
  // min_bitrate_bps = 0 equals no min bitrate.
  // max_bitrate_bps = 0 equals no max bitrate.
  // TODO(holmer): Remove start_bitrate_bps when old API is gone.
  int AddBitrateObserver(BitrateObserver* observer,
                         uint32_t start_bitrate_bps,
                         uint32_t min_bitrate_bps,
                         uint32_t max_bitrate_bps,
                         int* new_observer_bitrate_bps);

  void RemoveBitrateObserver(BitrateObserver* observer);

  void GetMinMaxBitrateSumBps(int* min_bitrate_sum_bps,
                              int* max_bitrate_sum_bps) const;

  // This method controls the behavior when the available bitrate is lower than
  // the minimum bitrate, or the sum of minimum bitrates.
  // When true, the bitrate will never be set lower than the minimum bitrate(s).
  // When false, the bitrate observers will be allocated rates up to their
  // respective minimum bitrate, satisfying one observer after the other.
  void EnforceMinBitrate(bool enforce_min_bitrate);

 private:
  struct BitrateConfiguration {
    BitrateConfiguration(uint32_t start_bitrate,
                         uint32_t min_bitrate,
                         uint32_t max_bitrate)
        : start_bitrate_(start_bitrate),
          min_bitrate_(min_bitrate),
          max_bitrate_(max_bitrate) {}
    uint32_t start_bitrate_;
    uint32_t min_bitrate_;
    uint32_t max_bitrate_;
  };
  struct ObserverConfiguration {
    ObserverConfiguration(BitrateObserver* observer, uint32_t bitrate)
        : observer_(observer), min_bitrate_(bitrate) {}
    BitrateObserver* observer_;
    uint32_t min_bitrate_;
  };
  typedef std::pair<BitrateObserver*, BitrateConfiguration>
      BitrateObserverConfiguration;
  typedef std::list<BitrateObserverConfiguration> BitrateObserverConfList;
  typedef std::multimap<uint32_t, ObserverConfiguration> ObserverSortingMap;
  typedef std::map<BitrateObserver*, int> ObserverBitrateMap;

  BitrateObserverConfList::iterator FindObserverConfigurationPair(
      const BitrateObserver* observer) EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  ObserverBitrateMap AllocateBitrates() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  ObserverBitrateMap NormalRateAllocation(uint32_t bitrate,
                                          uint32_t sum_min_bitrates)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  ObserverBitrateMap LowRateAllocation(uint32_t bitrate)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;
  // Stored in a list to keep track of the insertion order.
  BitrateObserverConfList bitrate_observers_ GUARDED_BY(crit_sect_);
  bool bitrate_observers_modified_ GUARDED_BY(crit_sect_);
  bool enforce_min_bitrate_ GUARDED_BY(crit_sect_);
  uint32_t last_bitrate_bps_ GUARDED_BY(crit_sect_);
  uint8_t last_fraction_loss_ GUARDED_BY(crit_sect_);
  int64_t last_rtt_ GUARDED_BY(crit_sect_);
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_BITRATE_CONTROLLER_INCLUDE_BITRATE_ALLOCATOR_H_
