/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef __NSTIMER_H
#define __NSTIMER_H

#ifdef MOZ_PERF_METRICS

#include "stopwatch.h"

// This should be set from preferences at runtime.  For now, make it a compile time flag.
#define ENABLE_DEBUG_OUTPUT PR_FALSE

// Timer macros for the Mac
#ifdef XP_MAC

#ifdef MOZ_TIMER_USE_MAC_ISDK

#include "InstrumentationHelpers.h"

// Macro to create a timer on the stack.
#  define MOZ_TIMER_CREATE(name, msg)    \
  static InstTraceClassRef name = 0;  StInstrumentationLog __traceLog((msg), name)

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
  do { traceLog.LogMiddleEventWithData((s), (d)); } while (0)

#else

#  define MOZ_TIMER_CREATE(name, msg)    \
  static Stopwatch __sw_name;  nsFunctionTimer name(__sw_name, msg)

#  define MOZ_TIMER_RESET(name, msg)  \
  name.Reset(); printf msg

#  define MOZ_TIMER_START(name, msg)  \
  name.Start(PR_FALSE); printf msg
  
#  define MOZ_TIMER_STOP(name, msg) \
  name.Stop(); printf msg

#  define MOZ_TIMER_SAVE(name, msg) \
  name.SaveState(); printf msg

#  define MOZ_TIMER_RESTORE(name, msg)  \
  name.RestoreState(); printf msg

#  define MOZ_TIMER_LOG(msg)  \
  printf msg

#  define MOZ_TIMER_DEBUGLOG(msg) \
  if (ENABLE_DEBUG_OUTPUT) printf msg

#  define MOZ_TIMER_MACISDK_LOGDATA(msg, data)
#endif

#endif


// Timer macros for Windows
#ifdef XP_WIN

#endif

// Timer macros for Unix
#ifdef XP_UNIX

#endif

#else
#  define MOZ_TIMER_CREATE(id)
#  define MOZ_TIMER_RESET(id, msg)
#  define MOZ_TIMER_START(id, msg)
#  define MOZ_TIMER_STOP(id, msg)
#  define MOZ_TIMER_SAVE(id, msg)
#  define MOZ_TIMER_RESTORE(id, msg)
#  define MOZ_TIMER_LOG(id, msg)
#  define MOZ_TIMER_DEBUGLOG(msg)
#  define MOZ_TIMER_MACISDK_LOGDATA(msg, data)
#endif

// To be removed.
#ifdef MOZ_PERF_METRICS

static PRLogModuleInfo* gLogStopwatchModule = PR_NewLogModule("timing");

#if 0
#define RAPTOR_TRACE_STOPWATCHES        0x1

#define RAPTOR_STOPWATCH_TRACE(_args)                               \
  PR_BEGIN_MACRO                                                    \
  PR_LOG(gLogStopwatchModule, RAPTOR_TRACE_STOPWATCHES, _args);     \
  PR_END_MACRO
#endif

#define RAPTOR_STOPWATCH_TRACE(_args)      \
  PR_BEGIN_MACRO                           \
  printf _args ;                           \
  PR_END_MACRO

#else
#define RAPTOR_TRACE_STOPWATCHES 
#define RAPTOR_STOPWATCH_TRACE(_args) 
#endif

#ifdef DEBUG_STOPWATCH
#define RAPTOR_STOPWATCH_DEBUGTRACE(_args)      \
  PR_BEGIN_MACRO                                \
  printf _args ;                                \
  PR_END_MACRO
#else
#define RAPTOR_STOPWATCH_DEBUGTRACE(_args) 
#endif

#endif  // __NSTIMER_H