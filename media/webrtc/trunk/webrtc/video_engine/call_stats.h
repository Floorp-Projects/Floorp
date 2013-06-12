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

#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class CriticalSectionWrapper;
class RtcpRttObserver;

// Interface used to distribute call statistics. Callbacks will be triggered as
// soon as the class has been registered using RegisterStatsObserver.
class StatsObserver {
 public:
  virtual void OnRttUpdate(uint32_t rtt) = 0;

  virtual ~StatsObserver() {}
};

// CallStats keeps track of statistics for a call.
class CallStats : public Module {
 public:
  friend class RtcpObserver;

  CallStats();
  ~CallStats();

  // Implements Module, to use the process thread.
  virtual int32_t TimeUntilNextProcess();
  virtual int32_t Process();

  // Returns a RtcpRttObserver to register at a statistics provider. The object
  // has the same lifetime as the CallStats instance.
  RtcpRttObserver* rtcp_rtt_observer() const;

  // Registers/deregisters a new observer to receive statistics updates.
  void RegisterStatsObserver(StatsObserver* observer);
  void DeregisterStatsObserver(StatsObserver* observer);

 protected:
  void OnRttUpdate(uint32_t rtt);

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
  scoped_ptr<RtcpRttObserver> rtcp_rtt_observer_;
  // The last time 'Process' resulted in statistic update.
  int64_t last_process_time_;

  // All Rtt reports within valid time interval, oldest first.
  std::list<RttTime> reports_;

  // Observers getting stats reports.
  std::list<StatsObserver*> observers_;

  DISALLOW_COPY_AND_ASSIGN(CallStats);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_CALL_STATS_H_
