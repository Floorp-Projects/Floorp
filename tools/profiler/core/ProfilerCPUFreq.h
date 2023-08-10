/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLS_PROFILERCPUFREQ_H_
#define TOOLS_PROFILERCPUFREQ_H_

#include "PlatformMacros.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

#if defined(GP_OS_windows) || defined(GP_OS_linux) || defined(GP_OS_android)
#  define HAVE_CPU_FREQ_SUPPORT
#endif

#if defined(GP_OS_windows)
#  include <windows.h>

struct CPUCounterInfo {
  ULONGLONG data = 0;
  DWORD base = 0;
  uint32_t freq = 0;
  DWORD nominalFrequency = 0;
};
#endif

#if defined(GP_OS_linux) || defined(GP_OS_android)
struct CPUCounterInfo {
  int fd = 0;
};
#endif

class ProfilerCPUFreq {
 public:
#if defined(HAVE_CPU_FREQ_SUPPORT)
  explicit ProfilerCPUFreq();
  ~ProfilerCPUFreq();
#  if defined(GP_OS_windows)
  void Sample();
  uint32_t GetCPUSpeedMHz(unsigned cpuId) {
    MOZ_ASSERT(cpuId < mCPUCounters.length());
    return mCPUCounters[cpuId].freq;
  }
#  else
  void Sample() {}
  uint32_t GetCPUSpeedMHz(unsigned cpuId);
#  endif
#else
  explicit ProfilerCPUFreq(){};
  ~ProfilerCPUFreq(){};
  void Sample(){};
#endif

 private:
#if defined(HAVE_CPU_FREQ_SUPPORT)
#  if defined(GP_OS_windows)
  LPWSTR mBlockIndex = nullptr;
  DWORD mCounterNameIndex = 0;
  // The size of the counter block is about 8kB for a machine with 20 cores,
  // so 32kB should be plenty.
  DWORD mBufferSize = 32768;
  LPBYTE mBuffer = nullptr;
#  endif
  mozilla::Vector<CPUCounterInfo> mCPUCounters;
#endif
};

#endif /* ndef TOOLS_PROFILERCPUFREQ_H_ */
