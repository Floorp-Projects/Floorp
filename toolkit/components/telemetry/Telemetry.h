/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Taras Glek <tglek@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
