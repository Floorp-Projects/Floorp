/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/interface/cpu_wrapper.h"
#include "system_wrappers/interface/event_wrapper.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "system_wrappers/source/cpu_measurement_harness.h"

const int kCpuCheckPeriodMs = 100;

namespace webrtc {

CpuMeasurementHarness* CpuMeasurementHarness::Create(
    CpuTarget* target,
    int work_period_ms,
    int work_iterations_per_period,
    int duration_ms) {
  if (target == NULL) {
    return NULL;
  }
  if (work_period_ms > duration_ms) {
    return NULL;
  }
  if (work_period_ms < 0) {
    return NULL;
  }
  if (duration_ms < 0) {
    return NULL;
  }
  if (work_iterations_per_period < 1) {
    return NULL;
  }
  return new CpuMeasurementHarness(target, work_period_ms,
                                   work_iterations_per_period, duration_ms);
}

CpuMeasurementHarness::CpuMeasurementHarness(CpuTarget* target,
                                             int work_period_ms,
                                             int work_iterations_per_period,
                                             int duration_ms)
    : cpu_target_(target),
      work_period_ms_(work_period_ms),
      work_iterations_per_period_(work_iterations_per_period),
      duration_ms_(duration_ms),
      cpu_sum_(0),
      cpu_iterations_(0),
      cpu_(CpuWrapper::CreateCpu()),
      event_(EventWrapper::Create()) {
}

CpuMeasurementHarness::~CpuMeasurementHarness() {
}

bool CpuMeasurementHarness::Run() {
  if (!WaitForCpuInit()) {
    return false;
  }
  // No need for precision. Run for approximately the asked for duration.
  // TODO(hellner): very low prio if at all, the actual duration of the test
  // will be longer if calling DoWork() is not negligable and/or called many
  // times. It may make sense to compensate for drift here. This will,
  // however, only add complexity with minimal gains. Perhaps renaming the
  // duration_ms_ to something more fuzzy is a better idea. However, the name
  // would be very convoluted if it is to be self documenting.
  int elapsed_time_ms = 0;
  int last_measured_time = 0;
  while (elapsed_time_ms < duration_ms_) {
    if (((elapsed_time_ms - last_measured_time) / kCpuCheckPeriodMs) >= 1) {
      last_measured_time = elapsed_time_ms;
      Measure();
    }
    if (!DoWork()) {
      return false;
    }
    event_->Wait(work_period_ms_);
    elapsed_time_ms += work_period_ms_;
  }
  return true;
}

int CpuMeasurementHarness::AverageCpu() {
  if (cpu_iterations_ == 0) {
    return 0;
  }
  assert(cpu_sum_ >= 0);
  assert(cpu_iterations_ >= 0);
  return cpu_sum_ / cpu_iterations_;
}

bool CpuMeasurementHarness::WaitForCpuInit() {
  bool cpu_usage_available = false;
  int num_iterations = 0;
  // Initializing the CPU measurements may take a couple of seconds on Windows.
  // Since the initialization is lazy we need to wait until it is completed.
  // Should not take more than 10000 ms.
  while (!cpu_usage_available && (++num_iterations < 10000)) {
    event_->Wait(1);
    cpu_usage_available = cpu_->CpuUsage() != -1;
  }
  return cpu_usage_available;
}

void CpuMeasurementHarness::Measure() {
  WebRtc_UWord32 num_cores = 0;
  WebRtc_UWord32* cores = NULL;
  // Return the average CPU for now.
  cpu_sum_ = cpu_->CpuUsageMultiCore(num_cores, cores);
  ++cpu_iterations_;
}

bool CpuMeasurementHarness::DoWork() {
  for (int i = 0; i < work_iterations_per_period_; ++i) {
    if (!cpu_target_->DoWork()) {
      return false;
    }
  }
  return true;
}

}  // namespace webrtc
