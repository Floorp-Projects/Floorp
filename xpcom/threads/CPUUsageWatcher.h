/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CPUUsageWatcher_h
#define mozilla_CPUUsageWatcher_h

#include <stdint.h>

#include "mozilla/HangAnnotations.h"

// We only support OSX and Windows, because on Linux we're forced to read
// from /proc/stat in order to get global CPU values. We would prefer to not
// eat that cost for this.
#if defined(NIGHTLY_BUILD) && (defined(XP_WIN) || defined(XP_MACOSX))
#define CPU_USAGE_WATCHER_ACTIVE
#endif

namespace mozilla {

enum CPUUsageWatcherError : uint8_t
{
  ClockGetTimeError,
  GetNumberOfProcessorsError,
  GetProcessTimesError,
  GetSystemTimesError,
  HostStatisticsError,
  ProcStatError,
};

class CPUUsageHangAnnotator
  : public HangMonitor::Annotator
{
public:
};

class CPUUsageWatcher
  : public HangMonitor::Annotator
{
public:
#ifdef CPU_USAGE_WATCHER_ACTIVE
  CPUUsageWatcher()
    : mInitialized(false)
    , mExternalUsageThreshold(0)
    , mExternalUsageRatio(0)
    , mProcessUsageTime(0)
    , mProcessUpdateTime(0)
    , mGlobalUsageTime(0)
    , mGlobalUpdateTime(0)
  {}
#endif

  Result<Ok, CPUUsageWatcherError> Init();

  void Uninit();

  // Updates necessary values to allow AnnotateHang to function. This must be
  // called on some semi-regular basis, as it will calculate the mean CPU
  // usage values between now and the last time it was called.
  Result<Ok, CPUUsageWatcherError> CollectCPUUsage();

  void AnnotateHang(HangMonitor::HangAnnotations& aAnnotations) final override;
private:
#ifdef CPU_USAGE_WATCHER_ACTIVE
  bool mInitialized;
  // The threshold above which we will mark a hang as occurring under high
  // external CPU usage conditions
  float mExternalUsageThreshold;
  // The CPU usage (0-1) external to our process, averaged between the two
  // most recent monitor thread runs
  float mExternalUsageRatio;
  // The total cumulative CPU usage time by our process as of the last
  // CollectCPUUsage or Startup
  uint64_t mProcessUsageTime;
  // A time value in the same units as mProcessUsageTime used to
  // determine the ratio of CPU usage time to idle time
  uint64_t mProcessUpdateTime;
  // The total cumulative CPU usage time by all processes as of the last
  // CollectCPUUsage or Startup
  uint64_t mGlobalUsageTime;
  // A time value in the same units as mGlobalUsageTime used to
  // determine the ratio of CPU usage time to idle time
  uint64_t mGlobalUpdateTime;
  // The number of virtual cores on our machine
  uint64_t mNumCPUs;
#endif
};

} // namespace mozilla

#endif // mozilla_CPUUsageWatcher_h
