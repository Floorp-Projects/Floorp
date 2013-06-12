/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "system_wrappers/source/cpu_mac.h"

#include <iostream>
#include <mach/mach.h>
#include <mach/mach_error.h>

#include "tick_util.h"

namespace webrtc {

CpuWrapperMac::CpuWrapperMac()
    : cpu_count_(0),
      cpu_usage_(NULL),
      total_cpu_usage_(0),
      last_tick_count_(NULL),
      last_time_(0) {
  natural_t cpu_count;
  processor_info_array_t info_array;
  mach_msg_type_number_t info_count;

  kern_return_t error = host_processor_info(mach_host_self(),
                                            PROCESSOR_CPU_LOAD_INFO,
                                            &cpu_count,
                                            &info_array,
                                            &info_count);
  if (error) {
    return;
  }

  cpu_count_ = cpu_count;
  cpu_usage_ = new WebRtc_UWord32[cpu_count];
  last_tick_count_ = new WebRtc_Word64[cpu_count];
  last_time_ = TickTime::MillisecondTimestamp();

  processor_cpu_load_info_data_t* cpu_load_info =
    (processor_cpu_load_info_data_t*) info_array;
  for (unsigned int cpu = 0; cpu < cpu_count; ++cpu) {
    WebRtc_Word64 ticks = 0;
    for (int state = 0; state < 2; ++state) {
      ticks += cpu_load_info[cpu].cpu_ticks[state];
    }
    last_tick_count_[cpu] = ticks;
    cpu_usage_[cpu] = 0;
  }
  vm_deallocate(mach_task_self(), (vm_address_t)info_array, info_count);
}

CpuWrapperMac::~CpuWrapperMac() {
  delete[] cpu_usage_;
  delete[] last_tick_count_;
}

WebRtc_Word32 CpuWrapperMac::CpuUsage() {
  WebRtc_UWord32 num_cores;
  WebRtc_UWord32* array = NULL;
  return CpuUsageMultiCore(num_cores, array);
}

WebRtc_Word32
CpuWrapperMac::CpuUsageMultiCore(WebRtc_UWord32& num_cores,
                                 WebRtc_UWord32*& array) {
  // sanity check
  if (cpu_usage_ == NULL) {
    return -1;
  }

  WebRtc_Word64 now = TickTime::MillisecondTimestamp();
  WebRtc_Word64 time_diff_ms = now - last_time_;
  if (time_diff_ms >= 500) {
    if (Update(time_diff_ms) != 0) {
      return -1;
    }
    last_time_ = now;
  }

  num_cores = cpu_count_;
  array = cpu_usage_;
  return total_cpu_usage_ / cpu_count_;
}

WebRtc_Word32 CpuWrapperMac::Update(WebRtc_Word64 time_diff_ms) {
  natural_t cpu_count;
  processor_info_array_t info_array;
  mach_msg_type_number_t info_count;

  kern_return_t error = host_processor_info(mach_host_self(),
                                            PROCESSOR_CPU_LOAD_INFO,
                                            &cpu_count,
                                            &info_array,
                                            &info_count);
  if (error) {
    return -1;
  }

  processor_cpu_load_info_data_t* cpu_load_info =
    (processor_cpu_load_info_data_t*) info_array;

  total_cpu_usage_ = 0;
  for (unsigned int cpu = 0; cpu < cpu_count; ++cpu) {
    WebRtc_Word64 ticks = 0;
    for (int state = 0; state < 2; ++state) {
      ticks += cpu_load_info[cpu].cpu_ticks[state];
    }
    if (time_diff_ms <= 0) {
      cpu_usage_[cpu] = 0;
    } else {
      cpu_usage_[cpu] = (WebRtc_UWord32)((1000 *
                                          (ticks - last_tick_count_[cpu])) /
                                         time_diff_ms);
    }
    last_tick_count_[cpu] = ticks;
    total_cpu_usage_ += cpu_usage_[cpu];
  }

  vm_deallocate(mach_task_self(), (vm_address_t)info_array, info_count);

  return 0;
}

} // namespace webrtc
