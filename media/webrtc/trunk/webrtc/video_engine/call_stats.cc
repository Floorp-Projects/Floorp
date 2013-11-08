/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/call_stats.h"

#include <assert.h>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

// A rtt report is considered valid for this long.
const int kRttTimeoutMs = 1500;
// Time interval for updating the observers.
const int kUpdateIntervalMs = 1000;

class RtcpObserver : public RtcpRttObserver {
 public:
  explicit RtcpObserver(CallStats* owner) : owner_(owner) {}
  virtual ~RtcpObserver() {}

  virtual void OnRttUpdate(uint32_t rtt) {
    owner_->OnRttUpdate(rtt);
  }

 private:
  CallStats* owner_;

  DISALLOW_COPY_AND_ASSIGN(RtcpObserver);
};

CallStats::CallStats()
    : crit_(CriticalSectionWrapper::CreateCriticalSection()),
      rtcp_rtt_observer_(new RtcpObserver(this)),
      last_process_time_(TickTime::MillisecondTimestamp()) {
}

CallStats::~CallStats() {
  assert(observers_.empty());
}

int32_t CallStats::TimeUntilNextProcess() {
  return last_process_time_ + kUpdateIntervalMs -
      TickTime::MillisecondTimestamp();
}

int32_t CallStats::Process() {
  CriticalSectionScoped cs(crit_.get());
  if (TickTime::MillisecondTimestamp() < last_process_time_ + kUpdateIntervalMs)
    return 0;

  // Remove invalid, as in too old, rtt values.
  int64_t time_now = TickTime::MillisecondTimestamp();
  while (!reports_.empty() && reports_.front().time + kRttTimeoutMs <
         time_now) {
    reports_.pop_front();
  }

  // Find the max stored RTT.
  uint32_t max_rtt = 0;
  for (std::list<RttTime>::const_iterator it = reports_.begin();
       it != reports_.end(); ++it) {
    if (it->rtt > max_rtt)
      max_rtt = it->rtt;
  }

  // If there is a valid rtt, update all observers.
  if (max_rtt > 0) {
    for (std::list<CallStatsObserver*>::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnRttUpdate(max_rtt);
    }
  }
  last_process_time_ = time_now;
  return 0;
}

RtcpRttObserver* CallStats::rtcp_rtt_observer() const {
  return rtcp_rtt_observer_.get();
}

void CallStats::RegisterStatsObserver(CallStatsObserver* observer) {
  CriticalSectionScoped cs(crit_.get());
  for (std::list<CallStatsObserver*>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    if (*it == observer)
      return;
  }
  observers_.push_back(observer);
}

void CallStats::DeregisterStatsObserver(CallStatsObserver* observer) {
  CriticalSectionScoped cs(crit_.get());
  for (std::list<CallStatsObserver*>::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    if (*it == observer) {
      observers_.erase(it);
      return;
    }
  }
}

void CallStats::OnRttUpdate(uint32_t rtt) {
  CriticalSectionScoped cs(crit_.get());
  int64_t time_now = TickTime::MillisecondTimestamp();
  reports_.push_back(RttTime(rtt, time_now));
}

}  // namespace webrtc
