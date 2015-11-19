/*
 *  Copyright 2008 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_SYSTEMINFO_H__
#define WEBRTC_BASE_SYSTEMINFO_H__

#include <string>

#include "webrtc/base/basictypes.h"

namespace rtc {

class SystemInfo {
 public:
  enum Architecture {
    SI_ARCH_UNKNOWN = -1,
    SI_ARCH_X86 = 0,
    SI_ARCH_X64 = 1,
    SI_ARCH_ARM = 2
  };

  SystemInfo();

  // The number of CPU Cores in the system.
  int GetMaxPhysicalCpus();
  // The number of CPU Threads in the system.
  int GetMaxCpus();
  // The number of CPU Threads currently available to this process.
  int GetCurCpus();
  // Identity of the CPUs.
  Architecture GetCpuArchitecture();
  std::string GetCpuVendor();
  int GetCpuFamily();
  int GetCpuModel();
  int GetCpuStepping();
  // Return size of CPU cache in bytes.  Uses largest available cache (L3).
  int GetCpuCacheSize();
  // Estimated speed of the CPUs, in MHz.  e.g. 2400 for 2.4 GHz
  int GetMaxCpuSpeed();
  int GetCurCpuSpeed();
  // Total amount of physical memory, in bytes.
  int64 GetMemorySize();
  // The model name of the machine, e.g. "MacBookAir1,1"
  std::string GetMachineModel();

  // The gpu identifier
  struct GpuInfo {
    GpuInfo();
    ~GpuInfo();
    std::string device_name;
    std::string description;
    int vendor_id;
    int device_id;
    std::string driver;
    std::string driver_version;
  };
  bool GetGpuInfo(GpuInfo *info);

 private:
  int physical_cpus_;
  int logical_cpus_;
  int cache_size_;
  Architecture cpu_arch_;
  std::string cpu_vendor_;
  int cpu_family_;
  int cpu_model_;
  int cpu_stepping_;
  int cpu_speed_;
  int64 memory_;
  std::string machine_model_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_SYSTEMINFO_H__
