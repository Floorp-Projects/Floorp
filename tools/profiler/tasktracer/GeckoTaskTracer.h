/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKO_TASK_TRACER_H
#define GECKO_TASK_TRACER_H

#include <stdarg.h>

#include "mozilla/UniquePtr.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
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

extern bool gStarted;

/**
 * Check if the TaskTracer has been started.
 */
inline bool IsStartLogging()
{
  // |gStarted| is not an atomic variable, but it is fine for it is a
  // boolean value and will be changed under the protection of
  // |sMutex|.
  //
  // There is a latency between the change of the value and the
  // observation of threads.  |gStarted| would be checked again with
  // the protection of mutex in logging functions; for example,
  // |AddLabel()|, so all false positive would be blocked with the
  // double checks.  For false negative, it is fine to lose some
  // records at begin of logging.
  return gStarted;
}

enum {
  FORKED_AFTER_NUWA = 1 << 0
};

enum SourceEventType {
#define SOURCE_EVENT_NAME(x) x,
#include "SourceEventTypeMap.h"
#undef SOURCE_EVENT_NAME
};

class AutoSaveCurTraceInfoImpl
{
  uint64_t mSavedTaskId;
  uint64_t mSavedSourceEventId;
  SourceEventType mSavedSourceEventType;
public:
  AutoSaveCurTraceInfoImpl();
  ~AutoSaveCurTraceInfoImpl();
};

class AutoSaveCurTraceInfo
{
  Maybe<AutoSaveCurTraceInfoImpl> mSaved;
public:
  AutoSaveCurTraceInfo() {
    if (IsStartLogging()) {
      mSaved.emplace();
    }
  }

  /**
   * The instance had saved TraceInfo.
   *
   * It means that TaskTrace had been enabled when the instance was
   * created.
   */
  bool HasSavedTraceInfo() {
    return !!mSaved;
  }
};

class AutoSourceEvent : public AutoSaveCurTraceInfo
{
  void StartScope(SourceEventType aType);
  void StopScope();
public:
  explicit AutoSourceEvent(SourceEventType aType) : AutoSaveCurTraceInfo() {
    if (HasSavedTraceInfo()) {
      StartScope(aType);
    }
  }

  ~AutoSourceEvent() {
    if (HasSavedTraceInfo()) {
      StopScope();
    }
  }
};

void InitTaskTracer(uint32_t aFlags = 0);
void ShutdownTaskTracer();

void DoAddLabel(const char* aFormat, va_list& aArgs);

// Add a label to the currently running task, aFormat is the message to log,
// followed by corresponding parameters.
inline void AddLabel(const char* aFormat, ...) MOZ_FORMAT_PRINTF(1, 2);
inline void AddLabel(const char* aFormat, ...) {
  if (IsStartLogging()) {
    va_list args;
    va_start(args, aFormat);
    DoAddLabel(aFormat, args);
    va_end(args);
  }
}

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
  void Init(const char* aFormat, va_list& aArgs);

public:
  explicit AutoScopedLabel(const char* aFormat, ...) : mLabel(nullptr)
  {
    if (IsStartLogging()) {
      va_list args;
      va_start(args, aFormat);
      Init(aFormat, args);
      va_end(args);
    }
  }

  ~AutoScopedLabel()
  {
    if (mLabel) {
      AddLabel("End %s", mLabel);
      free(mLabel);
    }
  }
};

} // namespace tasktracer
} // namespace mozilla.

#endif
