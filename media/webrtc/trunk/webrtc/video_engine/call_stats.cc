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

namespace webrtc {
namespace {
// Time interval for updating the observers.
const int64_t kUpdateIntervalMs = 1000;
// Weight factor to apply to the average rtt.
const float kWeightFactor = 0.3f;

void RemoveOldReports(int64_t now, std::list<CallStats::RttTime>* reports) {
  // A rtt report is considered valid for this long.
  const int64_t kRttTimeoutMs = 1500;
  while (!reports->empty() &&
         (now - reports->front().time) > kRttTimeoutMs) {
    reports->pop_front();
  }
}

int64_t GetMaxRttMs(std::list<CallStats::RttTime>* reports) {
  int64_t max_rtt_ms = 0;
  for (std::list<CallStats::RttTime>::const_iterator it = reports->begin();
       it != reports->end(); ++it) {
    max_rtt_ms = std::max(it->rtt, max_rtt_ms);
  }
  return max_rtt_ms;
}

int64_t GetAvgRttMs(std::list<CallStats::RttTime>* reports) {
  if (reports->empty()) {
    return 0;
  }
  int64_t sum = 0;
  for (std::list<CallStats::RttTime>::const_iterator it = reports->begin();
       it != reports->end(); ++it) {
    sum += it->rtt;
  }
  return sum / reports->size();
}

void UpdateAvgRttMs(std::list<CallStats::RttTime>* reports, int64_t* avg_rtt) {
  uint32_t cur_rtt_ms = GetAvgRttMs(reports);
  if (cur_rtt_ms == 0) {
    // Reset.
    *avg_rtt = 0;
    return;
  }
  if (*avg_rtt == 0) {
    // Initialize.
    *avg_rtt = cur_rtt_ms;
    return;
  }
  *avg_rtt = *avg_rtt * (1.0f - kWeightFactor) + cur_rtt_ms * kWeightFactor;
}
}  // namespace

class RtcpObserver : public RtcpRttStats {
 public:
  explicit RtcpObserver(CallStats* owner) : owner_(owner) {}
  virtual ~RtcpObserver() {}

  virtual void OnRttUpdate(int64_t rtt) {
    owner_->OnRttUpdate(rtt);
  }

  // Returns the average RTT.
  virtual int64_t LastProcessedRtt() const {
    return owner_->avg_rtt_ms();
  }

 private:
  CallStats* owner_;

  DISALLOW_COPY_AND_ASSIGN(RtcpObserver);
};

CallStats::CallStats()
    : crit_(CriticalSectionWrapper::CreateCriticalSection()),
      rtcp_rtt_stats_(new RtcpObserver(this)),
      last_process_time_(TickTime::MillisecondTimestamp()),
      max_rtt_ms_(0),
      avg_rtt_ms_(0) {
}

CallStats::~CallStats() {
  assert(observers_.empty());
}

int64_t CallStats::TimeUntilNextProcess() {
  return last_process_time_ + kUpdateIntervalMs -
      TickTime::MillisecondTimestamp();
}

int32_t CallStats::Process() {
  CriticalSectionScoped cs(crit_.get());
  int64_t now = TickTime::MillisecondTimestamp();
  if (now < last_process_time_ + kUpdateIntervalMs)
    return 0;

  last_process_time_ = now;

  RemoveOldReports(now, &reports_);
  max_rtt_ms_ = GetMaxRttMs(&reports_);
  UpdateAvgRttMs(&reports_, &avg_rtt_ms_);

  // If there is a valid rtt, update all observers with the max rtt.
  // TODO(asapersson): Consider changing this to report the average rtt.
  if (max_rtt_ms_ > 0) {
    for (std::list<CallStatsObserver*>::iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      (*it)->OnRttUpdate(max_rtt_ms_);
    }
  }
  return 0;
}

int64_t CallStats::avg_rtt_ms() const {
  CriticalSectionScoped cs(crit_.get());
  return avg_rtt_ms_;
}

RtcpRttStats* CallStats::rtcp_rtt_stats() const {
  return rtcp_rtt_stats_.get();
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

void CallStats::OnRttUpdate(int64_t rtt) {
  CriticalSectionScoped cs(crit_.get());
  reports_.push_back(RttTime(rtt, TickTime::MillisecondTimestamp()));
}

}  // namespace webrtc
