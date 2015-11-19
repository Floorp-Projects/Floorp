/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_CPUMONITOR_H_
#define WEBRTC_BASE_CPUMONITOR_H_

#include "webrtc/base/basictypes.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/sigslot.h"
#if defined(WEBRTC_LINUX)
#include "webrtc/base/stream.h"
#endif // defined(WEBRTC_LINUX)

namespace rtc {
class Thread;
class SystemInfo;

struct CpuStats {
  CpuStats()
      : prev_total_times_(0),
        prev_cpu_times_(0),
        prev_load_(0.f),
        prev_load_time_(0u) {
  }

  uint64 prev_total_times_;
  uint64 prev_cpu_times_;
  float prev_load_;  // Previous load value.
  uint32 prev_load_time_;  // Time previous load value was taken.
};

// CpuSampler samples the process and system load.
class CpuSampler {
 public:
  CpuSampler();
  ~CpuSampler();

  // Initialize CpuSampler.  Returns true if successful.
  bool Init();

  // Set minimum interval in ms between computing new load values.
  // Default 950 ms.  Set to 0 to disable interval.
  void set_load_interval(int min_load_interval);

  // Return CPU load of current process as a float from 0 to 1.
  float GetProcessLoad();

  // Return CPU load of current process as a float from 0 to 1.
  float GetSystemLoad();

  // Return number of cpus. Includes hyperthreads.
  int GetMaxCpus() const;

  // Return current number of cpus available to this process.
  int GetCurrentCpus();

  // For testing. Allows forcing of fallback to using NTDLL functions.
  void set_force_fallback(bool fallback) {
#if defined(WEBRTC_WIN)
    force_fallback_ = fallback;
#endif
  }

 private:
  float UpdateCpuLoad(uint64 current_total_times,
                      uint64 current_cpu_times,
                      uint64 *prev_total_times,
                      uint64 *prev_cpu_times);
  CpuStats process_;
  CpuStats system_;
  int cpus_;
  int min_load_interval_;  // Minimum time between computing new load.
  scoped_ptr<SystemInfo> sysinfo_;
#if defined(WEBRTC_WIN)
  void* get_system_times_;
  void* nt_query_system_information_;
  bool force_fallback_;
#endif
#if defined(WEBRTC_LINUX)
  // File for reading /proc/stat
  scoped_ptr<FileStream> sfile_;
#endif // defined(WEBRTC_LINUX)
};

// CpuMonitor samples and signals the CPU load periodically.
class CpuMonitor
    : public rtc::MessageHandler, public sigslot::has_slots<> {
 public:
  explicit CpuMonitor(Thread* thread);
  ~CpuMonitor() override;
  void set_thread(Thread* thread);

  bool Start(int period_ms);
  void Stop();
  // Signal parameters are current cpus, max cpus, process load and system load.
  sigslot::signal4<int, int, float, float> SignalUpdate;

 protected:
  // Override virtual method of parent MessageHandler.
  void OnMessage(rtc::Message* msg) override;
  // Clear the monitor thread and stop sending it messages if the thread goes
  // away before our lifetime.
  void OnMessageQueueDestroyed() { monitor_thread_ = NULL; }

 private:
  Thread* monitor_thread_;
  CpuSampler sampler_;
  int period_ms_;

  DISALLOW_COPY_AND_ASSIGN(CpuMonitor);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_CPUMONITOR_H_
