/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CPUUsageWatcher.h"
#include "mozilla/Try.h"

#include "prsystem.h"

#ifdef XP_MACOSX
#  include <sys/resource.h>
#  include <mach/clock.h>
#  include <mach/mach_host.h>
#endif

#ifdef CPU_USAGE_WATCHER_ACTIVE
#  include "mozilla/BackgroundHangMonitor.h"
#endif

namespace mozilla {

#ifdef CPU_USAGE_WATCHER_ACTIVE

// Even if the machine only has one processor, tolerate up to 50%
// external CPU usage.
static const float kTolerableExternalCPUUsageFloor = 0.5f;

struct CPUStats {
  // The average CPU usage time, which can be summed across all cores in the
  // system, or averaged between them. Whichever it is, it needs to be in the
  // same units as updateTime.
  uint64_t usageTime;
  // A monotonically increasing value in the same units as usageTime, which can
  // be used to determine the percentage of active vs idle time
  uint64_t updateTime;
};

#  ifdef XP_MACOSX

static const uint64_t kMicrosecondsPerSecond = 1000000LL;
static const uint64_t kNanosecondsPerMicrosecond = 1000LL;

static uint64_t GetMicroseconds(timeval time) {
  return ((uint64_t)time.tv_sec) * kMicrosecondsPerSecond +
         (uint64_t)time.tv_usec;
}

static uint64_t GetMicroseconds(mach_timespec_t time) {
  return ((uint64_t)time.tv_sec) * kMicrosecondsPerSecond +
         ((uint64_t)time.tv_nsec) / kNanosecondsPerMicrosecond;
}

static Result<CPUStats, CPUUsageWatcherError> GetProcessCPUStats(
    int32_t numCPUs) {
  CPUStats result = {};
  rusage usage;
  int32_t rusageResult = getrusage(RUSAGE_SELF, &usage);
  if (rusageResult == -1) {
    return Err(GetProcessTimesError);
  }
  result.usageTime =
      GetMicroseconds(usage.ru_utime) + GetMicroseconds(usage.ru_stime);

  clock_serv_t realtimeClock;
  kern_return_t errorResult =
      host_get_clock_service(mach_host_self(), REALTIME_CLOCK, &realtimeClock);
  if (errorResult != KERN_SUCCESS) {
    return Err(GetProcessTimesError);
  }
  mach_timespec_t time;
  errorResult = clock_get_time(realtimeClock, &time);
  if (errorResult != KERN_SUCCESS) {
    return Err(GetProcessTimesError);
  }
  result.updateTime = GetMicroseconds(time);

  // getrusage will give us the sum of the values across all
  // of our cores. Divide by the number of CPUs to get an average.
  result.usageTime /= numCPUs;
  return result;
}

static Result<CPUStats, CPUUsageWatcherError> GetGlobalCPUStats() {
  CPUStats result = {};
  host_cpu_load_info_data_t loadInfo;
  mach_msg_type_number_t loadInfoCount = HOST_CPU_LOAD_INFO_COUNT;
  kern_return_t statsResult =
      host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                      (host_info_t)&loadInfo, &loadInfoCount);
  if (statsResult != KERN_SUCCESS) {
    return Err(HostStatisticsError);
  }

  result.usageTime = loadInfo.cpu_ticks[CPU_STATE_USER] +
                     loadInfo.cpu_ticks[CPU_STATE_NICE] +
                     loadInfo.cpu_ticks[CPU_STATE_SYSTEM];
  result.updateTime = result.usageTime + loadInfo.cpu_ticks[CPU_STATE_IDLE];
  return result;
}

#  endif  // XP_MACOSX

#  ifdef XP_WIN

// A FILETIME represents the number of 100-nanosecond ticks since 1/1/1601 UTC
uint64_t FiletimeToInteger(FILETIME filetime) {
  return ((uint64_t)filetime.dwLowDateTime) | (uint64_t)filetime.dwHighDateTime
                                                  << 32;
}

Result<CPUStats, CPUUsageWatcherError> GetProcessCPUStats(int32_t numCPUs) {
  CPUStats result = {};
  FILETIME creationFiletime;
  FILETIME exitFiletime;
  FILETIME kernelFiletime;
  FILETIME userFiletime;
  bool success = GetProcessTimes(GetCurrentProcess(), &creationFiletime,
                                 &exitFiletime, &kernelFiletime, &userFiletime);
  if (!success) {
    return Err(GetProcessTimesError);
  }

  result.usageTime =
      FiletimeToInteger(kernelFiletime) + FiletimeToInteger(userFiletime);

  FILETIME nowFiletime;
  GetSystemTimeAsFileTime(&nowFiletime);
  result.updateTime = FiletimeToInteger(nowFiletime);

  result.usageTime /= numCPUs;

  return result;
}

Result<CPUStats, CPUUsageWatcherError> GetGlobalCPUStats() {
  CPUStats result = {};
  FILETIME idleFiletime;
  FILETIME kernelFiletime;
  FILETIME userFiletime;
  bool success = GetSystemTimes(&idleFiletime, &kernelFiletime, &userFiletime);

  if (!success) {
    return Err(GetSystemTimesError);
  }

  result.usageTime =
      FiletimeToInteger(kernelFiletime) + FiletimeToInteger(userFiletime);
  result.updateTime = result.usageTime + FiletimeToInteger(idleFiletime);

  return result;
}

#  endif  // XP_WIN

Result<Ok, CPUUsageWatcherError> CPUUsageWatcher::Init() {
  mNumCPUs = PR_GetNumberOfProcessors();
  if (mNumCPUs <= 0) {
    mExternalUsageThreshold = 1.0f;
    return Err(GetNumberOfProcessorsError);
  }
  mExternalUsageThreshold =
      std::max(1.0f - 1.0f / (float)mNumCPUs, kTolerableExternalCPUUsageFloor);

  CPUStats processTimes;
  MOZ_TRY_VAR(processTimes, GetProcessCPUStats(mNumCPUs));
  mProcessUpdateTime = processTimes.updateTime;
  mProcessUsageTime = processTimes.usageTime;

  CPUStats globalTimes;
  MOZ_TRY_VAR(globalTimes, GetGlobalCPUStats());
  mGlobalUpdateTime = globalTimes.updateTime;
  mGlobalUsageTime = globalTimes.usageTime;

  mInitialized = true;

  CPUUsageWatcher* self = this;
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "CPUUsageWatcher::Init",
      [=]() { BackgroundHangMonitor::RegisterAnnotator(*self); }));

  return Ok();
}

void CPUUsageWatcher::Uninit() {
  if (mInitialized) {
    BackgroundHangMonitor::UnregisterAnnotator(*this);
  }
  mInitialized = false;
}

Result<Ok, CPUUsageWatcherError> CPUUsageWatcher::CollectCPUUsage() {
  if (!mInitialized) {
    return Ok();
  }

  mExternalUsageRatio = 0.0f;

  CPUStats processTimes;
  MOZ_TRY_VAR(processTimes, GetProcessCPUStats(mNumCPUs));
  CPUStats globalTimes;
  MOZ_TRY_VAR(globalTimes, GetGlobalCPUStats());

  uint64_t processUsageDelta = processTimes.usageTime - mProcessUsageTime;
  uint64_t processUpdateDelta = processTimes.updateTime - mProcessUpdateTime;
  float processUsageNormalized =
      processUsageDelta > 0
          ? (float)processUsageDelta / (float)processUpdateDelta
          : 0.0f;

  uint64_t globalUsageDelta = globalTimes.usageTime - mGlobalUsageTime;
  uint64_t globalUpdateDelta = globalTimes.updateTime - mGlobalUpdateTime;
  float globalUsageNormalized =
      globalUsageDelta > 0 ? (float)globalUsageDelta / (float)globalUpdateDelta
                           : 0.0f;

  mProcessUsageTime = processTimes.usageTime;
  mProcessUpdateTime = processTimes.updateTime;
  mGlobalUsageTime = globalTimes.usageTime;
  mGlobalUpdateTime = globalTimes.updateTime;

  mExternalUsageRatio =
      std::max(0.0f, globalUsageNormalized - processUsageNormalized);

  return Ok();
}

void CPUUsageWatcher::AnnotateHang(BackgroundHangAnnotations& aAnnotations) {
  if (!mInitialized) {
    return;
  }

  if (mExternalUsageRatio > mExternalUsageThreshold) {
    aAnnotations.AddAnnotation(u"ExternalCPUHigh"_ns, true);
  }
}

#else  // !CPU_USAGE_WATCHER_ACTIVE

Result<Ok, CPUUsageWatcherError> CPUUsageWatcher::Init() { return Ok(); }

void CPUUsageWatcher::Uninit() {}

Result<Ok, CPUUsageWatcherError> CPUUsageWatcher::CollectCPUUsage() {
  return Ok();
}

void CPUUsageWatcher::AnnotateHang(BackgroundHangAnnotations& aAnnotations) {}

#endif  // CPU_USAGE_WATCHER_ACTIVE

}  // namespace mozilla
