/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_CALL_STATS_H_
#define WEBRTC_VIDEO_ENGINE_CALL_STATS_H_

#include <list>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class CallStatsObserver;
class CriticalSectionWrapper;
class RtcpRttStats;

// CallStats keeps track of statistics for a call.
class CallStats : public Module {
 public:
  friend class RtcpObserver;

  CallStats();
  ~CallStats();

  // Implements Module, to use the process thread.
  virtual int32_t TimeUntilNextProcess() OVERRIDE;
  virtual int32_t Process() OVERRIDE;

  // Returns a RtcpRttStats to register at a statistics provider. The object
  // has the same lifetime as the CallStats instance.
  RtcpRttStats* rtcp_rtt_stats() const;

  // Registers/deregisters a new observer to receive statistics updates.
  void RegisterStatsObserver(CallStatsObserver* observer);
  void DeregisterStatsObserver(CallStatsObserver* observer);

 protected:
  void OnRttUpdate(uint32_t rtt);

  uint32_t last_processed_rtt_ms() const;

 private:
  // Helper struct keeping track of the time a rtt value is reported.
  struct RttTime {
    RttTime(uint32_t new_rtt, int64_t rtt_time)
        : rtt(new_rtt), time(rtt_time) {}
    const uint32_t rtt;
    const int64_t time;
  };

  // Protecting all members.
  scoped_ptr<CriticalSectionWrapper> crit_;
  // Observer receiving statistics updates.
  scoped_ptr<RtcpRttStats> rtcp_rtt_stats_;
  // The last time 'Process' resulted in statistic update.
  int64_t last_process_time_;
  // The last RTT in the statistics update (zero if there is no valid estimate).
  uint32_t last_processed_rtt_ms_;

  // All Rtt reports within valid time interval, oldest first.
  std::list<RttTime> reports_;

  // Observers getting stats reports.
  std::list<CallStatsObserver*> observers_;

  DISALLOW_COPY_AND_ASSIGN(CallStats);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_CALL_STATS_H_
