/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef __NSTIMER_H
#define __NSTIMER_H

#ifdef MOZ_PERF_METRICS

#include "stopwatch.h"

class nsStackBasedTimer 
{
public:
  nsStackBasedTimer(Stopwatch* aStopwatch) { sw = aStopwatch; }
  ~nsStackBasedTimer() { if (sw) sw->Stop(); }

  void Start(PRBool aReset) { if (sw) sw->Start(aReset); }
  void Stop(void) { if (sw) sw->Stop(); }
  void Reset(void) { if (sw) sw->Reset(); }
  void SaveState(void) { if (sw) sw->SaveState(); }
  void RestoreState(void) { if (sw) sw->RestoreState(); }
  void Print(void) { if (sw) sw->Print(); }
  
private:
  Stopwatch* sw;
};

// This should be set from preferences at runtime.  For now, make it a compile time flag.
#define ENABLE_DEBUG_OUTPUT PR_FALSE

// Uncomment and re-build to use the Mac Instrumentation SDK on a Mac.
// #define MOZ_TIMER_USE_MAC_ISDK

// Uncomment and re-build to use Quantify on Windows
// #define MOZ_TIMER_USE_QUANTIFY

// Timer macros for the Mac
#ifdef XP_MAC

#ifdef MOZ_TIMER_USE_MAC_ISDK

#include "InstrumentationHelpers.h"
#  define MOZ_TIMER_DECLARE(name)  
#  define MOZ_TIMER_CREATE(name)   \
  static InstTraceClassRef name = 0;  StInstrumentationLog __traceLog("Creating name..."), name)

#  define MOZ_TIMER_RESET(name, msg)
#  define MOZ_TIMER_START(name, msg)
#  define MOZ_TIMER_STOP(name, msg)
#  define MOZ_TIMER_SAVE(name, msg)
#  define MOZ_TIMER_RESTORE(name, msg)
#  define MOZ_TIMER_LOG(msg) \
  do { __traceLog.LogMiddleEvent(); } while(0)

#  define MOZ_TIMER_DEBUGLOG(msg) \
  if (ENABLE_DEBUG_OUTPUT) printf msg

#  define MOZ_TIMER_MACISDK_LOGDATA(msg, data) \
  do { traceLog.LogMiddleEventWithData((msg), (data)); } while (0)

#else

#define MOZ_TIMER_USE_STOPWATCH

#endif  // MOZ_TIMER_USE_MAC_ISDK

#endif  // XP_MAC


// Timer macros for Windows
#ifdef XP_WIN

#ifdef MOZ_TIMER_USE_QUANTIFY

#include "pure.h"
#include "prprf.h"

#  define MOZ_TIMER_DECLARE(name)
#  define MOZ_TIMER_CREATE(name)
#  define MOZ_TIMER_RESET(name)  \
  QuantifyClearData()

#  define MOZ_TIMER_START(name)  \
  QuantifyStartRecordingData()
  
#  define MOZ_TIMER_STOP(name) \
  QuantifyStopRecordingData()

#  define MOZ_TIMER_SAVE(name)
#  define MOZ_TIMER_RESTORE(name)

#  define MOZ_TIMER_PRINT(name)
 
#  define MOZ_TIMER_LOG(msg)    \
do {                            \
  char* str = PR_smprintf msg;  \
  QuantifyAddAnnotation(str);   \
  PR_smprintf_free(str);        \
} while (0)

#  define MOZ_TIMER_DEBUGLOG(msg) \
  if (ENABLE_DEBUG_OUTPUT) printf msg

#  define MOZ_TIMER_MACISDK_LOGDATA(msg, data)

#else

#define MOZ_TIMER_USE_STOPWATCH

#endif  // MOZ_TIMER_USE_QUANTIFY

#endif  // XP_WIN

// Timer macros for Unix
#ifdef XP_UNIX

#define MOZ_TIMER_USE_STOPWATCH

#endif  // XP_UNIX

#ifdef MOZ_TIMER_USE_STOPWATCH

#  define MOZ_TIMER_DECLARE(name)  \
  Stopwatch name;

#  define MOZ_TIMER_CREATE(name)    \
  static Stopwatch __sw_name;  nsStackBasedTimer name(&__sw_name)

#  define MOZ_TIMER_RESET(name)  \
  name.Reset();

#  define MOZ_TIMER_START(name)  \
  name.Start(PR_FALSE);

#  define MOZ_TIMER_STOP(name) \
  name.Stop();

#  define MOZ_TIMER_SAVE(name) \
  name.SaveState();

#  define MOZ_TIMER_RESTORE(name)  \
  name.RestoreState();

#  define MOZ_TIMER_PRINT(name)   \
  name.Print();

#  define MOZ_TIMER_LOG(msg)  \
  printf msg

#  define MOZ_TIMER_DEBUGLOG(msg) \
  if (ENABLE_DEBUG_OUTPUT) printf msg

#  define MOZ_TIMER_MACISDK_LOGDATA(msg, data)

#endif // MOZ_TIMER_USE_STOPWATCH

#else
#  define MOZ_TIMER_DECLARE(name)
#  define MOZ_TIMER_CREATE(name)
#  define MOZ_TIMER_RESET(name)
#  define MOZ_TIMER_START(name)
#  define MOZ_TIMER_STOP(name)
#  define MOZ_TIMER_SAVE(name)
#  define MOZ_TIMER_RESTORE(name)
#  define MOZ_TIMER_PRINT(name)
#  define MOZ_TIMER_LOG(msg)
#  define MOZ_TIMER_DEBUGLOG(msg)
#  define MOZ_TIMER_MACISDK_LOGDATA(msg, data)
#endif  // MOZ_PERF_METRICS

#endif  // __NSTIMER_H

