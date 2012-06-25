/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_SYSTEM_WRAPPERS_SOURCE_CPU_MEASUREMENT_HARNESS_H_
#define SRC_SYSTEM_WRAPPERS_SOURCE_CPU_MEASUREMENT_HARNESS_H_

#include "system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class CpuWrapper;
class EventWrapper;
class ThreadWrapper;

// This abstract class provides an interface that should be passed to
// CpuMeasurementHarness. CpuMeasurementHarness will call it with the
// frequency requested and measure the CPU usage for all calls.
class CpuTarget {
 public:
  // Callback function for which the CPU usage should be calculated.
  virtual bool DoWork() = 0;

 protected:
  CpuTarget() {}
  virtual ~CpuTarget() {}
};

class CpuMeasurementHarness {
 public:
  static CpuMeasurementHarness* Create(CpuTarget* target,
                                       int work_period_ms,
                                       int work_iterations_per_period,
                                       int duration_ms);
  ~CpuMeasurementHarness();
  bool Run();
  int AverageCpu();

 protected:
  CpuMeasurementHarness(CpuTarget* target, int work_period_ms,
                        int work_iterations_per_period, int duration_ms);

 private:
  bool WaitForCpuInit();
  void Measure();
  bool DoWork();

  CpuTarget* cpu_target_;
  const int work_period_ms_;
  const int work_iterations_per_period_;
  const int duration_ms_;
  int cpu_sum_;
  int cpu_iterations_;
  scoped_ptr<CpuWrapper> cpu_;
  scoped_ptr<EventWrapper> event_;
};

}  // namespace webrtc

#endif  // SRC_SYSTEM_WRAPPERS_SOURCE_CPU_MEASUREMENT_HARNESS_H_
