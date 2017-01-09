/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKO_TASK_TRACER_H
#define GECKO_TASK_TRACER_H

#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsTArrayForwardDeclare.h"

/**
 * TaskTracer provides a way to trace the correlation between different tasks
 * across threads and processes. Unlike sampling based profilers, TaskTracer can
 * tell you where a task is dispatched from, what its original source was, how
 * long it waited in the event queue, and how long it took to execute.
 *
 * Source Events are usually some kinds of I/O events we're interested in, such
 * as touch events, timer events, network events, etc. When a source event is
 * created, TaskTracer records the entire chain of Tasks and nsRunnables as they
 * are dispatched to different threads and processes. It records latency,
 * execution time, etc. for each Task and nsRunnable that chains back to the
 * original source event.
 */

class nsIRunnable;
class nsCString;

namespace mozilla {

class TimeStamp;
class Runnable;

namespace tasktracer {

enum {
  FORKED_AFTER_NUWA = 1 << 0
};

enum SourceEventType {
#define SOURCE_EVENT_NAME(x) x,
#include "SourceEventTypeMap.h"
#undef SOURCE_EVENT_NAME
};

class AutoSaveCurTraceInfo
{
  uint64_t mSavedTaskId;
  uint64_t mSavedSourceEventId;
  SourceEventType mSavedSourceEventType;
public:
  AutoSaveCurTraceInfo();
  ~AutoSaveCurTraceInfo();
};

class AutoSourceEvent : public AutoSaveCurTraceInfo
{
public:
  explicit AutoSourceEvent(SourceEventType aType);
  ~AutoSourceEvent();
};

void InitTaskTracer(uint32_t aFlags = 0);
void ShutdownTaskTracer();

// Add a label to the currently running task, aFormat is the message to log,
// followed by corresponding parameters.
void AddLabel(const char* aFormat, ...);

void StartLogging();
void StopLogging();
UniquePtr<nsTArray<nsCString>> GetLoggedData(TimeStamp aStartTime);

// Returns the timestamp when Task Tracer is enabled in this process.
PRTime GetStartTime();

/**
 * Internal functions.
 */

already_AddRefed<Runnable>
CreateTracedRunnable(already_AddRefed<nsIRunnable>&& aRunnable);

// Free the TraceInfo allocated on a thread's TLS. Currently we are wrapping
// tasks running on nsThreads and base::thread, so FreeTraceInfo is called at
// where nsThread and base::thread release themselves.
void FreeTraceInfo();

const char* GetJSLabelPrefix();

void GetCurTraceInfo(uint64_t* aOutSourceEventId, uint64_t* aOutParentTaskId,
                     SourceEventType* aOutSourceEventType);

class AutoScopedLabel
{
  char* mLabel;
public:
  explicit AutoScopedLabel(const char* aFormat, ...);
  ~AutoScopedLabel();
};

} // namespace tasktracer
} // namespace mozilla.

#endif
