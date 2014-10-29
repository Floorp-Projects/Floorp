/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoTaskTracer.h"
#include "GeckoTaskTracerImpl.h"

#include "mozilla/StaticMutex.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/unused.h"

#include "nsString.h"
#include "nsThreadUtils.h"
#include "prtime.h"

#include <stdarg.h>

#if defined(__GLIBC__)
// glibc doesn't implement gettid(2).
#include <sys/syscall.h>
static pid_t gettid()
{
  return (pid_t) syscall(SYS_gettid);
}
#endif

using mozilla::TimeStamp;

namespace mozilla {
namespace tasktracer {

static mozilla::ThreadLocal<TraceInfo*>* sTraceInfoTLS = nullptr;
static mozilla::StaticMutex sMutex;
static nsTArray<nsAutoPtr<TraceInfo>>* sTraceInfos = nullptr;
static bool sIsLoggingStarted = false;

static TimeStamp sStartTime;

namespace {

static TraceInfo*
AllocTraceInfo(int aTid)
{
  StaticMutexAutoLock lock(sMutex);

  nsAutoPtr<TraceInfo>* info = sTraceInfos->AppendElement(
                                 new TraceInfo(aTid, sIsLoggingStarted));

  return info->get();
}

static bool
IsInitialized()
{
  return sTraceInfoTLS ? sTraceInfoTLS->initialized() : false;
}

static void
SaveCurTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  if (!info) {
    return;
  }

  info->mSavedCurTraceSourceId = info->mCurTraceSourceId;
  info->mSavedCurTraceSourceType = info->mCurTraceSourceType;
  info->mSavedCurTaskId = info->mCurTaskId;
}

static void
RestoreCurTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  if (!info) {
    return;
  }

  info->mCurTraceSourceId = info->mSavedCurTraceSourceId;
  info->mCurTraceSourceType = info->mSavedCurTraceSourceType;
  info->mCurTaskId = info->mSavedCurTaskId;
}

static void
CreateSourceEvent(SourceEventType aType)
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  // Save the currently traced source event info.
  SaveCurTraceInfo();

  // Create a new unique task id.
  uint64_t newId = GenNewUniqueTaskId();
  TraceInfo* info = GetOrCreateTraceInfo();
  info->mCurTraceSourceId = newId;
  info->mCurTraceSourceType = aType;
  info->mCurTaskId = newId;

  // Log a fake dispatch and start for this source event.
  LogDispatch(newId, newId,newId, aType);
  LogBegin(newId, newId);
}

static void
DestroySourceEvent()
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  // Log a fake end for this source event.
  TraceInfo* info = GetOrCreateTraceInfo();
  LogEnd(info->mCurTraceSourceId, info->mCurTraceSourceId);

  // Restore the previously saved source event info.
  RestoreCurTraceInfo();
}

static void
CleanUp()
{
  StaticMutexAutoLock lock(sMutex);

  if (sTraceInfos) {
    delete sTraceInfos;
    sTraceInfos = nullptr;
  }

  // pthread_key_delete() is not called at the destructor of
  // mozilla::ThreadLocal (Bug 1064672).
  if (sTraceInfoTLS) {
    delete sTraceInfoTLS;
    sTraceInfoTLS = nullptr;
  }
}

static void
SetLogStarted(bool aIsStartLogging)
{
  // TODO: This is called from a signal handler. Use semaphore instead.
  StaticMutexAutoLock lock(sMutex);

  for (uint32_t i = 0; i < sTraceInfos->Length(); ++i) {
    (*sTraceInfos)[i]->mStartLogging = aIsStartLogging;
  }

  sIsLoggingStarted = aIsStartLogging;
}

static bool
IsStartLogging(TraceInfo* aInfo)
{
  StaticMutexAutoLock lock(sMutex);
  return aInfo ? aInfo->mStartLogging : false;
}

static PRInt64
DurationFromStart()
{
  return static_cast<PRInt64>((TimeStamp::Now() - sStartTime).ToMilliseconds());
}

} // namespace anonymous

nsCString*
TraceInfo::AppendLog()
{
  MutexAutoLock lock(mLogsMutex);
  return mLogs.AppendElement();
}

void
TraceInfo::MoveLogsInto(TraceInfoLogsType& aResult)
{
  MutexAutoLock lock(mLogsMutex);
  aResult.MoveElementsFrom(mLogs);
}

void
InitTaskTracer(uint32_t aFlags)
{
  if (aFlags & FORKED_AFTER_NUWA) {
    CleanUp();
  }

  MOZ_ASSERT(!sTraceInfoTLS);
  sTraceInfoTLS = new ThreadLocal<TraceInfo*>();

  MOZ_ASSERT(!sTraceInfos);
  sTraceInfos = new nsTArray<nsAutoPtr<TraceInfo>>();

  if (!sTraceInfoTLS->initialized()) {
    unused << sTraceInfoTLS->init();
  }
}

void
ShutdownTaskTracer()
{
  CleanUp();
}

TraceInfo*
GetOrCreateTraceInfo()
{
  NS_ENSURE_TRUE(IsInitialized(), nullptr);

  TraceInfo* info = sTraceInfoTLS->get();
  if (!info) {
    info = AllocTraceInfo(gettid());
    sTraceInfoTLS->set(info);
  }

  return info;
}

uint64_t
GenNewUniqueTaskId()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE(info, 0);

  pid_t tid = gettid();
  uint64_t taskid = ((uint64_t)tid << 32) | ++info->mLastUniqueTaskId;
  return taskid;
}

AutoSaveCurTraceInfo::AutoSaveCurTraceInfo()
{
  SaveCurTraceInfo();
}

AutoSaveCurTraceInfo::~AutoSaveCurTraceInfo()
{
  RestoreCurTraceInfo();
}

void
SetCurTraceInfo(uint64_t aSourceEventId, uint64_t aParentTaskId,
                SourceEventType aSourceEventType)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = aSourceEventId;
  info->mCurTaskId = aParentTaskId;
  info->mCurTraceSourceType = aSourceEventType;
}

void
GetCurTraceInfo(uint64_t* aOutSourceEventId, uint64_t* aOutParentTaskId,
                SourceEventType* aOutSourceEventType)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  *aOutSourceEventId = info->mCurTraceSourceId;
  *aOutParentTaskId = info->mCurTaskId;
  *aOutSourceEventType = info->mCurTraceSourceType;
}

void
LogDispatch(uint64_t aTaskId, uint64_t aParentTaskId, uint64_t aSourceEventId,
            SourceEventType aSourceEventType)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  if (!IsStartLogging(info)) {
    return;
  }

  // Log format:
  // [0 taskId dispatchTime sourceEventId sourceEventType parentTaskId]
  nsCString* log = info->AppendLog();
  if (log) {
    log->AppendPrintf("%d %lld %lld %lld %d %lld",
                      ACTION_DISPATCH, aTaskId, DurationFromStart(),
                      aSourceEventId, aSourceEventType, aParentTaskId);
  }
}

void
LogBegin(uint64_t aTaskId, uint64_t aSourceEventId)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  if (!IsStartLogging(info)) {
    return;
  }

  // Log format:
  // [1 taskId beginTime processId threadId]
  nsCString* log = info->AppendLog();
  if (log) {
    log->AppendPrintf("%d %lld %lld %d %d",
                      ACTION_BEGIN, aTaskId, DurationFromStart(), getpid(), gettid());
  }
}

void
LogEnd(uint64_t aTaskId, uint64_t aSourceEventId)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  if (!IsStartLogging(info)) {
    return;
  }

  // Log format:
  // [2 taskId endTime]
  nsCString* log = info->AppendLog();
  if (log) {
    log->AppendPrintf("%d %lld %lld", ACTION_END, aTaskId,
                      DurationFromStart());
  }
}

void
LogVirtualTablePtr(uint64_t aTaskId, uint64_t aSourceEventId, int* aVptr)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  if (!IsStartLogging(info)) {
    return;
  }

  // Log format:
  // [4 taskId address]
  nsCString* log = info->AppendLog();
  if (log) {
    log->AppendPrintf("%d %lld %p", ACTION_GET_VTABLE, aTaskId, aVptr);
  }
}

void
FreeTraceInfo()
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  StaticMutexAutoLock lock(sMutex);
  TraceInfo* info = GetOrCreateTraceInfo();
  if (info) {
    sTraceInfos->RemoveElement(info);
  }
}

AutoSourceEvent::AutoSourceEvent(SourceEventType aType)
{
  CreateSourceEvent(aType);
}

AutoSourceEvent::~AutoSourceEvent()
{
  DestroySourceEvent();
}

void AddLabel(const char* aFormat, ...)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  if (!IsStartLogging(info)) {
    return;
  }

  va_list args;
  va_start(args, aFormat);
  nsAutoCString buffer;
  buffer.AppendPrintf(aFormat, args);
  va_end(args);

  // Log format:
  // [3 taskId "label"]
  nsCString* log = info->AppendLog();
  if (log) {
    log->AppendPrintf("%d %lld %lld \"%s\"", ACTION_ADD_LABEL, info->mCurTaskId,
                      DurationFromStart(), buffer.get());
  }
}

// Functions used by GeckoProfiler.

void
StartLogging(TimeStamp aStartTime)
{
  sStartTime = aStartTime;
  SetLogStarted(true);
}

void
StopLogging()
{
  SetLogStarted(false);
}

TraceInfoLogsType*
GetLoggedData(TimeStamp aStartTime)
{
  TraceInfoLogsType* result = new TraceInfoLogsType();

  // TODO: This is called from a signal handler. Use semaphore instead.
  StaticMutexAutoLock lock(sMutex);

  for (uint32_t i = 0; i < sTraceInfos->Length(); ++i) {
    (*sTraceInfos)[i]->MoveLogsInto(*result);
  }

  return result;
}

} // namespace tasktracer
} // namespace mozilla
