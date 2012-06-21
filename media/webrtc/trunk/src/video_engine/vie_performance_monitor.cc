/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_performance_monitor.h"

#include "cpu_wrapper.h"
#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"
#include "tick_util.h"
#include "trace.h"
#include "vie_base.h"
#include "vie_defines.h"

namespace webrtc {

enum { kVieMonitorPeriodMs = 975 };
enum { kVieCpuStartValue = 75 };

ViEPerformanceMonitor::ViEPerformanceMonitor(int engine_id)
    : engine_id_(engine_id),
      pointer_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      monitor_thread_(NULL),
      monitor_event_(*EventWrapper::Create()),
      average_application_cpu_(kVieCpuStartValue),
      average_system_cpu_(kVieCpuStartValue),
      cpu_(NULL),
      vie_base_observer_(NULL) {
}

ViEPerformanceMonitor::~ViEPerformanceMonitor() {
  Terminate();
  delete pointer_cs_;
  delete &monitor_event_;
}

int ViEPerformanceMonitor::Init(ViEBaseObserver* vie_base_observer) {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s", __FUNCTION__);

  CriticalSectionScoped cs(pointer_cs_);
  if (!vie_base_observer || vie_base_observer_) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                 "%s: Bad input argument or observer already set",
                 __FUNCTION__);
    return -1;
  }

  cpu_ = CpuWrapper::CreateCpu();
  if (cpu_ == NULL) {
    // Performance monitoring not supported
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo,
                 ViEId(engine_id_), "%s: Not supported", __FUNCTION__);
    return 0;
  }

  if (monitor_thread_ == NULL) {
    monitor_event_.StartTimer(true, kVieMonitorPeriodMs);
    monitor_thread_ = ThreadWrapper::CreateThread(ViEMonitorThreadFunction,
                                                  this, kNormalPriority,
                                                  "ViEPerformanceMonitor");
    unsigned int t_id = 0;
    if (monitor_thread_->Start(t_id)) {
      WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
                   "%s: Performance monitor thread started %u",
                   __FUNCTION__, t_id);
    } else {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(engine_id_),
                   "%s: Could not start performance monitor", __FUNCTION__);
      monitor_event_.StopTimer();
      return -1;
    }
  }
  vie_base_observer_ = vie_base_observer;
  return 0;
}

void ViEPerformanceMonitor::Terminate() {
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, ViEId(engine_id_),
               "%s", __FUNCTION__);

  pointer_cs_->Enter();
  if (!vie_base_observer_) {
    pointer_cs_->Leave();
    return;
  }

  vie_base_observer_ = NULL;
  monitor_event_.StopTimer();
  if (monitor_thread_) {
    ThreadWrapper* tmp_thread = monitor_thread_;
    monitor_thread_ = NULL;
    monitor_event_.Set();
    pointer_cs_->Leave();
    if (tmp_thread->Stop()) {
      pointer_cs_->Enter();
      delete tmp_thread;
      tmp_thread = NULL;
      delete cpu_;
    }
    cpu_ = NULL;
  }
  pointer_cs_->Leave();
}

bool ViEPerformanceMonitor::ViEBaseObserverRegistered() {
  CriticalSectionScoped cs(pointer_cs_);
  return vie_base_observer_ != NULL;
}

bool ViEPerformanceMonitor::ViEMonitorThreadFunction(void* obj) {
  return static_cast<ViEPerformanceMonitor*>(obj)->ViEMonitorProcess();
}

bool ViEPerformanceMonitor::ViEMonitorProcess() {
  // Periodically triggered with time KViEMonitorPeriodMs
  monitor_event_.Wait(kVieMonitorPeriodMs);
  if (monitor_thread_ == NULL) {
    // Thread removed, exit
    return false;
  }

  CriticalSectionScoped cs(pointer_cs_);
  if (cpu_) {
    int cpu_load = cpu_->CpuUsage();
    if (cpu_load > 75) {
      if (vie_base_observer_) {
        vie_base_observer_->PerformanceAlarm(cpu_load);
      }
    }
  }
  return true;
}

}  // namespace webrtc
