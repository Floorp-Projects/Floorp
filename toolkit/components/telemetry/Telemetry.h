/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Telemetry_h__
#define Telemetry_h__

#include "mozilla/GuardObjects.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/StartupTimeline.h"
#include "nsTArray.h"
#include "nsStringGlue.h"

namespace base {
  class Histogram;
}

namespace mozilla {
namespace Telemetry {

#include "TelemetryHistogramEnums.h"

enum TimerResolution {
  Millisecond,
  Microsecond
};

/**
 * Initialize the Telemetry service on the main thread at startup.
 */
void Init();

/**
 * Adds sample to a histogram defined in TelemetryHistograms.h
 *
 * @param id - histogram id
 * @param sample - value to record.
 */
void Accumulate(ID id, uint32_t sample);

/**
 * Adds a sample to a histogram defined in TelemetryHistograms.h.
 * This function is here to support telemetry measurements from Java,
 * where we have only names and not numeric IDs.  You should almost
 * certainly be using the by-enum-id version instead of this one.
 *
 * @param name - histogram name
 * @param sample - value to record
 */
void Accumulate(const char* name, uint32_t sample);

/**
 * Adds time delta in milliseconds to a histogram defined in TelemetryHistograms.h
 *
 * @param id - histogram id
 * @param start - start time
 * @param end - end time
 */
void AccumulateTimeDelta(ID id, TimeStamp start, TimeStamp end = TimeStamp::Now());

/**
 * Return a raw Histogram for direct manipulation for users who can not use Accumulate().
 */
base::Histogram* GetHistogramById(ID id);

/**
 * Those wrappers are needed because the VS versions we use do not support free
 * functions with default template arguments.
 */
template<TimerResolution res>
struct AccumulateDelta_impl
{
  static void compute(ID id, TimeStamp start, TimeStamp end = TimeStamp::Now());
};

template<>
struct AccumulateDelta_impl<Millisecond>
{
  static void compute(ID id, TimeStamp start, TimeStamp end = TimeStamp::Now()) {
    Accumulate(id, static_cast<uint32_t>((end - start).ToMilliseconds()));
  }
};

template<>
struct AccumulateDelta_impl<Microsecond>
{
  static void compute(ID id, TimeStamp start, TimeStamp end = TimeStamp::Now()) {
    Accumulate(id, static_cast<uint32_t>((end - start).ToMicroseconds()));
  }
};


template<ID id, TimerResolution res = Millisecond>
class AutoTimer {
public:
  AutoTimer(TimeStamp aStart = TimeStamp::Now() MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
     : start(aStart)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  ~AutoTimer() {
    AccumulateDelta_impl<res>::compute(id, start);
  }

private:
  const TimeStamp start;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

template<ID id>
class AutoCounter {
public:
  AutoCounter(uint32_t counterStart = 0 MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : counter(counterStart)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  ~AutoCounter() {
    Accumulate(id, counter);
  }

  // Prefix increment only, to encourage good habits.
  void operator++() {
    ++counter;
  }

  // Chaining doesn't make any sense, don't return anything.
  void operator+=(int increment) {
    counter += increment;
  }

private:
  uint32_t counter;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/**
 * Indicates whether Telemetry recording is turned on.  This is intended
 * to guard calls to Accumulate when the statistic being recorded is
 * expensive to compute.
 */
bool CanRecord();

/**
 * Records slow SQL statements for Telemetry reporting.
 *
 * @param statement - offending SQL statement to record
 * @param dbName - DB filename
 * @param delay - execution time in milliseconds
 */
void RecordSlowSQLStatement(const nsACString &statement,
                            const nsACString &dbName,
                            uint32_t delay);

/**
 * Thresholds for a statement to be considered slow, in milliseconds
 */
const uint32_t kSlowSQLThresholdForMainThread = 50;
const uint32_t kSlowSQLThresholdForHelperThreads = 100;

class ProcessedStack;

/**
 * Record the main thread's call stack after it hangs.
 *
 * @param aDuration - Approximate duration of main thread hang, in seconds
 * @param aStack - Array of PCs from the hung call stack
 * @param aSystemUptime - System uptime at the time of the hang, in minutes
 * @param aFirefoxUptime - Firefox uptime at the time of the hang, in minutes
 */
#if defined(MOZ_ENABLE_PROFILER_SPS)
void RecordChromeHang(uint32_t aDuration,
                      ProcessedStack &aStack,
                      int32_t aSystemUptime,
                      int32_t aFirefoxUptime);
#endif

class ThreadHangStats;

/**
 * Move a ThreadHangStats to Telemetry storage. Normally Telemetry queries
 * for active ThreadHangStats through BackgroundHangMonitor, but once a
 * thread exits, the thread's copy of ThreadHangStats needs to be moved to
 * inside Telemetry using this function.
 *
 * @param aStats ThreadHangStats to save; the data inside aStats
 *               will be moved and aStats should be treated as
 *               invalid after this function returns
 */
void RecordThreadHangStats(ThreadHangStats& aStats);

/**
 * Record a failed attempt at locking the user's profile.
 *
 * @param aProfileDir The profile directory whose lock attempt failed
 */
void WriteFailedProfileLock(nsIFile* aProfileDir);

} // namespace Telemetry
} // namespace mozilla
#endif // Telemetry_h__
