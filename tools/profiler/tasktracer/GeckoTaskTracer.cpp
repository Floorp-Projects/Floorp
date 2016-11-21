/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoTaskTracer.h"
#include "GeckoTaskTracerImpl.h"

#include "mozilla/MathAlgorithms.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

#include "nsString.h"
#include "nsThreadUtils.h"
#include "prtime.h"

#include <stdarg.h>

// NS_ENSURE_TRUE_VOID() without the warning on the debug build.
#define ENSURE_TRUE_VOID(x)   \
  do {                        \
    if (MOZ_UNLIKELY(!(x))) { \
       return;                \
    }                         \
  } while(0)

// NS_ENSURE_TRUE() without the warning on the debug build.
#define ENSURE_TRUE(x, ret)   \
  do {                        \
    if (MOZ_UNLIKELY(!(x))) { \
       return ret;            \
    }                         \
  } while(0)

namespace mozilla {
namespace tasktracer {

#define SOURCE_EVENT_NAME(type) \
  const char* CreateSourceEvent##type () { return "SourceEvent" #type; }
#include "SourceEventTypeMap.h"
#undef SOURCE_EVENT_NAME

static MOZ_THREAD_LOCAL(TraceInfo*) sTraceInfoTLS;
static mozilla::StaticMutex sMutex;

// The generation of TraceInfo. It will be > 0 if the Task Tracer is started and
// <= 0 if stopped.
static mozilla::Atomic<bool> sStarted;
static nsTArray<UniquePtr<TraceInfo>>* sTraceInfos = nullptr;
static PRTime sStartTime;

static const char sJSLabelPrefix[] = "#tt#";

namespace {

static PRTime
GetTimestamp()
{
  return PR_Now() / 1000;
}

static TraceInfo*
AllocTraceInfo(int aTid)
{
  StaticMutexAutoLock lock(sMutex);

  auto* info = sTraceInfos->AppendElement(MakeUnique<TraceInfo>(aTid));

  return info->get();
}

static void
SaveCurTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  info->mSavedCurTraceSourceId = info->mCurTraceSourceId;
  info->mSavedCurTraceSourceType = info->mCurTraceSourceType;
  info->mSavedCurTaskId = info->mCurTaskId;
}

static void
RestoreCurTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = info->mSavedCurTraceSourceId;
  info->mCurTraceSourceType = info->mSavedCurTraceSourceType;
  info->mCurTaskId = info->mSavedCurTaskId;
}

static void
CreateSourceEvent(SourceEventType aType)
{
  // Save the currently traced source event info.
  SaveCurTraceInfo();

  // Create a new unique task id.
  uint64_t newId = GenNewUniqueTaskId();
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = newId;
  info->mCurTraceSourceType = aType;
  info->mCurTaskId = newId;

  uintptr_t namePtr;
#define SOURCE_EVENT_NAME(type)         \
  case SourceEventType::type:           \
  {                                     \
    namePtr = (uintptr_t)&CreateSourceEvent##type; \
    break;                              \
  }

  switch (aType) {
#include "SourceEventTypeMap.h"
    default:
      MOZ_CRASH("Unknown SourceEvent.");
  }
#undef SOURCE_EVENT_NAME

  // Log a fake dispatch and start for this source event.
  LogDispatch(newId, newId, newId, aType);
  LogVirtualTablePtr(newId, newId, &namePtr);
  LogBegin(newId, newId);
}

static void
DestroySourceEvent()
{
  // Log a fake end for this source event.
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  LogEnd(info->mCurTraceSourceId, info->mCurTraceSourceId);

  // Restore the previously saved source event info.
  RestoreCurTraceInfo();
}

inline static bool
IsStartLogging()
{
  return sStarted;
}

static void
SetLogStarted(bool aIsStartLogging)
{
  MOZ_ASSERT(aIsStartLogging != sStarted);
  MOZ_ASSERT(sTraceInfos != nullptr);
  sStarted = aIsStartLogging;

  StaticMutexAutoLock lock(sMutex);
  if (!aIsStartLogging && sTraceInfos) {
    for (uint32_t i = 0; i < sTraceInfos->Length(); ++i) {
      (*sTraceInfos)[i]->mObsolete = true;
    }
  }
}

inline static void
ObsoleteCurrentTraceInfos()
{
  // Note that we can't and don't need to acquire sMutex here because this
  // function is called before the other threads are recreated.
  for (uint32_t i = 0; i < sTraceInfos->Length(); ++i) {
    (*sTraceInfos)[i]->mObsolete = true;
  }
}

} // namespace anonymous

nsCString*
TraceInfo::AppendLog()
{
  return mLogs.AppendElement();
}

void
TraceInfo::MoveLogsInto(TraceInfoLogsType& aResult)
{
  MutexAutoLock lock(mLogsMutex);
  aResult.AppendElements(Move(mLogs));
}

void
InitTaskTracer(uint32_t aFlags)
{
  StaticMutexAutoLock lock(sMutex);

  if (aFlags & FORKED_AFTER_NUWA) {
    ObsoleteCurrentTraceInfos();
    return;
  }

  MOZ_ASSERT(!sTraceInfos);

  sTraceInfoTLS.init();
  // A memory barrier is necessary here.
  sTraceInfos = new nsTArray<UniquePtr<TraceInfo>>();
}

void
ShutdownTaskTracer()
{
  if (IsStartLogging()) {
    SetLogStarted(false);
  }
}

static void
FreeTraceInfo(TraceInfo* aTraceInfo)
{
  StaticMutexAutoLock lock(sMutex);
  if (aTraceInfo) {
    UniquePtr<TraceInfo> traceinfo(aTraceInfo);
    mozilla::DebugOnly<bool> removed =
      sTraceInfos->RemoveElement(traceinfo);
    MOZ_ASSERT(removed);
    Unused << traceinfo.release(); // A dirty hack to prevent double free.
  }
}

void FreeTraceInfo()
{
  FreeTraceInfo(sTraceInfoTLS.get());
}

TraceInfo*
GetOrCreateTraceInfo()
{
  ENSURE_TRUE(IsStartLogging(), nullptr);

  TraceInfo* info = sTraceInfoTLS.get();
  if (info && info->mObsolete) {
    // TraceInfo is obsolete: remove it.
    FreeTraceInfo(info);
    info = nullptr;
  }

  if (!info) {
    info = AllocTraceInfo(gettid());
    sTraceInfoTLS.set(info);
  }

  return info;
}

uint64_t
GenNewUniqueTaskId()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE(info, 0);

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
  ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = aSourceEventId;
  info->mCurTaskId = aParentTaskId;
  info->mCurTraceSourceType = aSourceEventType;
}

void
GetCurTraceInfo(uint64_t* aOutSourceEventId, uint64_t* aOutParentTaskId,
                SourceEventType* aOutSourceEventType)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  *aOutSourceEventId = info->mCurTraceSourceId;
  *aOutParentTaskId = info->mCurTaskId;
  *aOutSourceEventType = info->mCurTraceSourceType;
}

void
LogDispatch(uint64_t aTaskId, uint64_t aParentTaskId, uint64_t aSourceEventId,
            SourceEventType aSourceEventType)
{
  LogDispatch(aTaskId, aParentTaskId, aSourceEventId, aSourceEventType, 0);
}

void
LogDispatch(uint64_t aTaskId, uint64_t aParentTaskId, uint64_t aSourceEventId,
            SourceEventType aSourceEventType, int aDelayTimeMs)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  // aDelayTimeMs is the expected delay time in milliseconds, thus the dispatch
  // time calculated of it might be slightly off in the real world.
  uint64_t time = (aDelayTimeMs <= 0) ? GetTimestamp() :
                  GetTimestamp() + aDelayTimeMs;

  MutexAutoLock lock(info->mLogsMutex);
  // Log format:
  // [0 taskId dispatchTime sourceEventId sourceEventType parentTaskId]
  nsCString* log = info->AppendLog();
  if (log) {
    log->AppendPrintf("%d %lld %lld %lld %d %lld",
                      ACTION_DISPATCH, aTaskId, time, aSourceEventId,
                      aSourceEventType, aParentTaskId);
  }
}

void
LogBegin(uint64_t aTaskId, uint64_t aSourceEventId)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  MutexAutoLock lock(info->mLogsMutex);
  // Log format:
  // [1 taskId beginTime processId threadId]
  nsCString* log = info->AppendLog();
  if (log) {
    log->AppendPrintf("%d %lld %lld %d %d",
                      ACTION_BEGIN, aTaskId, GetTimestamp(), getpid(), gettid());
  }
}

void
LogEnd(uint64_t aTaskId, uint64_t aSourceEventId)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  MutexAutoLock lock(info->mLogsMutex);
  // Log format:
  // [2 taskId endTime]
  nsCString* log = info->AppendLog();
  if (log) {
    log->AppendPrintf("%d %lld %lld", ACTION_END, aTaskId, GetTimestamp());
  }
}

void
LogVirtualTablePtr(uint64_t aTaskId, uint64_t aSourceEventId, uintptr_t* aVptr)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  MutexAutoLock lock(info->mLogsMutex);
  // Log format:
  // [4 taskId address]
  nsCString* log = info->AppendLog();
  if (log) {
    // Since addr2line used by SPS addon can not solve non-function
    // addresses, we use the first entry of vtable as the symbol to
    // solve.  We should find a better solution later.
    log->AppendPrintf("%d %lld %p", ACTION_GET_VTABLE, aTaskId, *aVptr);
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
  ENSURE_TRUE_VOID(info);

  va_list args;
  va_start(args, aFormat);
  nsAutoCString buffer;
  buffer.AppendPrintf(aFormat, args);
  va_end(args);

  MutexAutoLock lock(info->mLogsMutex);
  // Log format:
  // [3 taskId "label"]
  nsCString* log = info->AppendLog();
  if (log) {
    log->AppendPrintf("%d %lld %lld \"%s\"", ACTION_ADD_LABEL, info->mCurTaskId,
                      GetTimestamp(), buffer.get());
  }
}

// Functions used by GeckoProfiler.

void
StartLogging()
{
  sStartTime = GetTimestamp();
  SetLogStarted(true);
}

void
StopLogging()
{
  SetLogStarted(false);
}

UniquePtr<TraceInfoLogsType>
GetLoggedData(TimeStamp aTimeStamp)
{
  auto result = MakeUnique<TraceInfoLogsType>();

  // TODO: This is called from a signal handler. Use semaphore instead.
  StaticMutexAutoLock lock(sMutex);

  for (uint32_t i = 0; i < sTraceInfos->Length(); ++i) {
    if (!(*sTraceInfos)[i]->mObsolete) {
      (*sTraceInfos)[i]->MoveLogsInto(*result);
    }
  }

  return result;
}

const PRTime
GetStartTime()
{
  return sStartTime;
}

const char*
GetJSLabelPrefix()
{
  return sJSLabelPrefix;
}

#undef ENSURE_TRUE_VOID
#undef ENSURE_TRUE

} // namespace tasktracer
} // namespace mozilla
