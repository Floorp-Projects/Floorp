/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// ViEPerformanceMonitor is used to check the current CPU usage and triggers a
// callback when getting over a specified threshold.

#ifndef WEBRTC_VIDEO_ENGINE_VIE_PERFORMANCE_MONITOR_H_
#define WEBRTC_VIDEO_ENGINE_VIE_PERFORMANCE_MONITOR_H_

#include "system_wrappers/interface/scoped_ptr.h"
#include "typedefs.h"
#include "vie_defines.h"

namespace webrtc {

class CriticalSectionWrapper;
class CpuWrapper;
class EventWrapper;
class ThreadWrapper;
class ViEBaseObserver;

class ViEPerformanceMonitor {
 public:
  explicit ViEPerformanceMonitor(int engine_id);
  ~ViEPerformanceMonitor();

  int Init(ViEBaseObserver* vie_base_observer);
  void Terminate();
  bool ViEBaseObserverRegistered();

 protected:
  static bool ViEMonitorThreadFunction(void* obj);
  bool ViEMonitorProcess();

 private:
  const int engine_id_;
  // TODO(mfldoman) Make this one scoped_ptr.
  CriticalSectionWrapper* pointer_cs_;
  ThreadWrapper* monitor_thread_;
  EventWrapper& monitor_event_;
  int average_application_cpu_;
  int average_system_cpu_;
  CpuWrapper* cpu_;
  ViEBaseObserver* vie_base_observer_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_PERFORMANCE_MONITOR_H_
