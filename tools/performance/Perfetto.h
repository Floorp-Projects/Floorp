/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Perfetto_h
#define mozilla_Perfetto_h

#ifdef MOZ_PERFETTO
#  include "perfetto/perfetto.h"
#  include "mozilla/TimeStamp.h"

// Initialization is called when a content process is created.
// This can be called multiple times.
extern void InitPerfetto();

/* Perfetto Tracing:
 *
 * This file provides an interface to the perfetto tracing API.  The API from
 * the perfetto sdk can be used directly, but an additional set of macros
 * prefixed with PERFETTO_* have been defined to help minimize the use of
 * ifdef's.
 *
 * The common perfetto macros require a category and name at the very least.
 * These must be static strings, or wrapped with perfetto::DynamicString if
 * dynamic. If the string is static, but provided through a runtime pointer,
 * then it must be wrapped with perfetto::StaticString.
 *
 * You can also provide additional parameters such as a timestamp,
 * or a lambda to add additional information to the trace marker.
 * For more info, see https://perfetto.dev/docs/instrumentation/tracing-sdk
 *
 * Examples:
 *
 *  // Add a trace event to measure work inside a block,
 *  // using static strings only.
 *
 *  {
 *    PERFETTO_TRACE_EVENT("js", "JS::RunScript");
 *    run_script();
 *  }
 *
 *  // Add a trace event to measure work inside a block,
 *  // using a dynamic string.
 *
 *  void runScript(nsCString& scriptName)
 *  {
 *    PERFETTO_TRACE_EVENT("js", perfetto::DynamicString{scriptName.get()});
 *    run_script();
 *  }
 *
 *  // Add a trace event using a dynamic category and name.
 *
 *  void runScript(nsCString& categoryName, nsCString& scriptName)
 *  {
 *    perfetto::DynamicCategory category{category.get()};
 *    PERFETTO_TRACE_EVENT(category, perfetto::DynamicString{scriptName.get()});
 *    run_script();
 *  }
 *
 *  // Add a trace event to measure two arbitrary points of code.
 *  // Events in the same category must always be nested.
 *
 *  void startWork() {
 *    PERFETTO_TRACE_EVENT_BEGIN("js", "StartWork");
 *    ...
 *    PERFETTO_TRACE_EVENT_END("js");
 *  }
 *
 *  // Create a trace marker for an event that has already occurred
 *  // using previously saved timestamps.
 *
 *  void record_event(TimeStamp startTimeStamp, TimeStamp endTimeStamp)
 *  {
 *    PERFETTO_TRACE_EVENT_BEGIN("js", "Some Event", startTimeStamp);
 *    PERFETTO_TRACE_EVENT_END("js", endTimeStamp);
 *  }
 */

// Wrap the common trace event macros from perfetto so
// they can be called without #ifdef's.
#  define PERFETTO_TRACE_EVENT(...) TRACE_EVENT(__VA_ARGS__)
#  define PERFETTO_TRACE_EVENT_BEGIN(...) TRACE_EVENT_BEGIN(__VA_ARGS__)
#  define PERFETTO_TRACE_EVENT_END(...) TRACE_EVENT_END(__VA_ARGS__)

namespace perfetto {
// Specialize custom timestamps for mozilla::TimeStamp.
template <>
struct TraceTimestampTraits<mozilla::TimeStamp> {
  static inline TraceTimestamp ConvertTimestampToTraceTimeNs(
      const mozilla::TimeStamp& timestamp) {
    return {protos::gen::BuiltinClock::BUILTIN_CLOCK_MONOTONIC,
            timestamp.RawClockMonotonicNanosecondsSinceBoot()};
  }
};
}  // namespace perfetto

// Categories can be added dynamically, but to minimize overhead
// all categories should be pre-defined here whenever possible.
PERFETTO_DEFINE_CATEGORIES(perfetto::Category("task"),
                           perfetto::Category("usertiming"));

#else  // !defined(MOZ_PERFETTO)
#  define PERFETTO_TRACE_EVENT(...) \
    do {                            \
    } while (0)
#  define PERFETTO_TRACE_EVENT_BEGIN(...) \
    do {                                  \
    } while (0)
#  define PERFETTO_TRACE_EVENT_END(...) \
    do {                                \
    } while (0)
inline void InitPerfetto() {}
#endif  // MOZ_PERFETTO

#endif  // mozilla_Perfetto_h
