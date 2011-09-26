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
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ted Mielczarek <ted.mielczarek@gmail.com>
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

/*
 * Event loop instrumentation. This code attempts to measure the
 * latency of the UI-thread event loop by firing native events at it from
 * a background thread, and measuring how long it takes for them
 * to be serviced. The measurement interval (kMeasureInterval, below)
 * is also used as the upper bound of acceptable response time.
 * When an event takes longer than that interval to be serviced,
 * a sample will be written to the log.
 *
 * Usage:
 *
 * Set MOZ_INSTRUMENT_EVENT_LOOP=1 in the environment to enable
 * this instrumentation. Currently only the UI process is instrumented.
 *
 * Set MOZ_INSTRUMENT_EVENT_LOOP_OUTPUT in the environment to a
 * file path to contain the log output, the default is to log to stdout.
 *
 * Set MOZ_INSTRUMENT_EVENT_LOOP_THRESHOLD in the environment to an
 * integer number of milliseconds to change the threshold for reporting.
 * The default is 20 milliseconds. Unresponsive periods shorter than this
 * threshold will not be reported.
 *
 * Set MOZ_INSTRUMENT_EVENT_LOOP_INTERVAL in the environment to an
 * integer number of milliseconds to change the maximum sampling frequency.
 * This variable controls how often events will be sent to the main
 * thread's event loop to sample responsiveness. The sampler will not
 * send events twice within LOOP_INTERVAL milliseconds.
 * The default is 10 milliseconds.
 *
 * All logged output lines start with MOZ_EVENT_TRACE. All timestamps
 * output are milliseconds since the epoch (PRTime / 1000).
 *
 * On startup, a line of the form:
 *   MOZ_EVENT_TRACE start <timestamp>
 * will be output.
 *
 * On shutdown, a line of the form:
 *   MOZ_EVENT_TRACE stop <timestamp>
 * will be output.
 *
 * When an event servicing time exceeds the threshold, a line of the form:
 *   MOZ_EVENT_TRACE sample <timestamp> <duration>
 * will be output, where <duration> is the number of milliseconds that
 * it took for the event to be serviced.
 */

#include "EventTracer.h"

#include <stdio.h>

#include "mozilla/TimeStamp.h"
#include "mozilla/WidgetTraceEvent.h"
#include <limits.h>
#include <prenv.h>
#include <prinrval.h>
#include <prthread.h>
#include <prtime.h>

using mozilla::TimeDuration;
using mozilla::TimeStamp;
using mozilla::FireAndWaitForTracerEvent;

namespace {

PRThread* sTracerThread = NULL;
bool sExit = false;

/*
 * The tracer thread fires events at the native event loop roughly
 * every kMeasureInterval. It will sleep to attempt not to send them
 * more quickly, but if the response time is longer than kMeasureInterval
 * it will not send another event until the previous response is received.
 *
 * The output defaults to stdout, but can be redirected to a file by
 * settting the environment variable MOZ_INSTRUMENT_EVENT_LOOP_OUTPUT
 * to the name of a file to use.
 */
void TracerThread(void *arg)
{
  // These are the defaults. They can be overridden by environment vars.
  // This should be set to the maximum latency we'd like to allow
  // for responsiveness.
  PRIntervalTime threshold = PR_MillisecondsToInterval(20);
  // This is the sampling interval.
  PRIntervalTime interval = PR_MillisecondsToInterval(10);

  FILE* log = NULL;
  char* envfile = PR_GetEnv("MOZ_INSTRUMENT_EVENT_LOOP_OUTPUT");
  if (envfile) {
    log = fopen(envfile, "w");
  }
  if (log == NULL)
    log = stdout;

  char* thresholdenv = PR_GetEnv("MOZ_INSTRUMENT_EVENT_LOOP_THRESHOLD");
  if (thresholdenv && *thresholdenv) {
    int val = atoi(thresholdenv);
    if (val != 0 && val != INT_MAX && val != INT_MIN) {
      threshold = PR_MillisecondsToInterval(val);
    }
  }

  char* intervalenv = PR_GetEnv("MOZ_INSTRUMENT_EVENT_LOOP_INTERVAL");
  if (intervalenv && *intervalenv) {
    int val = atoi(intervalenv);
    if (val != 0 && val != INT_MAX && val != INT_MIN) {
      interval = PR_MillisecondsToInterval(val);
    }
  }

  fprintf(log, "MOZ_EVENT_TRACE start %llu\n", PR_Now() / PR_USEC_PER_MSEC);

  while (!sExit) {
    TimeStamp start(TimeStamp::Now());
    PRIntervalTime next_sleep = interval;

    //TODO: only wait up to a maximum of interval; return
    // early if that threshold is exceeded and dump a stack trace
    // or do something else useful.
    if (FireAndWaitForTracerEvent()) {
      TimeDuration duration = TimeStamp::Now() - start;
      // Only report samples that exceed our measurement threshold.
      if (duration.ToMilliseconds() > threshold) {
        fprintf(log, "MOZ_EVENT_TRACE sample %llu %d\n",
                PR_Now() / PR_USEC_PER_MSEC,
                int(duration.ToSecondsSigDigits() * 1000));
      }

      if (next_sleep > duration.ToMilliseconds()) {
        next_sleep -= int(duration.ToMilliseconds());
      }
      else {
        // Don't sleep at all if this event took longer than the measure
        // interval to deliver.
        next_sleep = 0;
      }
    }

    if (next_sleep != 0 && !sExit) {
      PR_Sleep(next_sleep);
    }
  }

  fprintf(log, "MOZ_EVENT_TRACE stop %llu\n", PR_Now() / PR_USEC_PER_MSEC);

  if (log != stdout)
    fclose(log);
}

} // namespace

namespace mozilla {

bool InitEventTracing()
{
  // Initialize the widget backend.
  if (!InitWidgetTracing())
    return false;

  // Create a thread that will fire events back at the
  // main thread to measure responsiveness.
  NS_ABORT_IF_FALSE(!sTracerThread, "Event tracing already initialized!");
  sTracerThread = PR_CreateThread(PR_USER_THREAD,
                                  TracerThread,
                                  NULL,
                                  PR_PRIORITY_NORMAL,
                                  PR_GLOBAL_THREAD,
                                  PR_JOINABLE_THREAD,
                                  0);
  return sTracerThread != NULL;
}

void ShutdownEventTracing()
{
  sExit = true;
  if (sTracerThread)
    PR_JoinThread(sTracerThread);
  sTracerThread = NULL;

  // Allow the widget backend to clean up.
  CleanUpWidgetTracing();
}

}  // namespace mozilla
