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
#if defined(MOZ_ENABLE_PROFILER_SPS)
#include "shared-libraries.h"
#endif

namespace base {
  class Histogram;
}

namespace mozilla {
namespace Telemetry {

enum ID {
#define HISTOGRAM(name, a, b, c, d, e) name,

#include "TelemetryHistograms.h"

#undef HISTOGRAM
HistogramCount
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
void Accumulate(ID id, PRUint32 sample);

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

template<ID id>
class AutoTimer {
public:
  AutoTimer(TimeStamp aStart = TimeStamp::Now() MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
     : start(aStart)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  }

  ~AutoTimer() {
    AccumulateTimeDelta(id, start);
  }

private:
  const TimeStamp start;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

template<ID id>
class AutoCounter {
public:
  AutoCounter(PRUint32 counterStart = 0 MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
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
  PRUint32 counter;
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
 * @param isDynamicString - prepared statement or a dynamic string
 */
void RecordSlowSQLStatement(const nsACString &statement,
                            const nsACString &dbName,
                            PRUint32 delay,
                            bool isDynamicString);

/**
 * Threshold for a statement to be considered slow, in milliseconds
 */
const PRUint32 kSlowStatementThreshold = 100;

/**
 * nsTArray of pointers representing PCs on a call stack
 */
typedef nsTArray<uintptr_t> HangStack;

/**
 * Record the main thread's call stack after it hangs.
 *
 * @param duration - Approximate duration of main thread hang in seconds
 * @param callStack - Array of PCs from the hung call stack
 * @param moduleMap - Array of info about modules in memory (for symbolication)
 */
#if defined(MOZ_ENABLE_PROFILER_SPS)
void RecordChromeHang(PRUint32 duration,
                      const HangStack &callStack,
                      SharedLibraryInfo &moduleMap);
#endif

} // namespace Telemetry
} // namespace mozilla
#endif // Telemetry_h__
