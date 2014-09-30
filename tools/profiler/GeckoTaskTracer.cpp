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

#include "nsClassHashtable.h"
#include "nsThreadUtils.h"

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#if defined(__GLIBC__)
// glibc doesn't implement gettid(2).
#include <sys/syscall.h>
static pid_t gettid()
{
  return (pid_t) syscall(SYS_gettid);
}
#endif

#define MAX_USER_LABEL_LEN 512

namespace mozilla {
namespace tasktracer {

static mozilla::ThreadLocal<TraceInfo*> sTraceInfoTLS;
static StaticMutex sMutex;
static nsClassHashtable<nsUint32HashKey, TraceInfo>* sTraceInfos = nullptr;

namespace {

static TraceInfo*
AllocTraceInfo(int aTid)
{
  StaticMutexAutoLock lock(sMutex);

  sTraceInfos->Put(aTid, new TraceInfo(aTid));
  return sTraceInfos->Get(aTid);
}

static void
FreeTraceInfo(int aTid)
{
  StaticMutexAutoLock lock(sMutex);

  sTraceInfos->Remove(aTid);
}

static bool
IsInitialized()
{
  return sTraceInfoTLS.initialized();
}

static void
SaveCurTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mSavedCurTraceSourceId = info->mCurTraceSourceId;
  info->mSavedCurTraceSourceType = info->mCurTraceSourceType;
  info->mSavedCurTaskId = info->mCurTaskId;
}

static void
RestoreCurTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

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

} // namespace anonymous

void
InitTaskTracer()
{
  MOZ_ASSERT(!sTraceInfos);

  sTraceInfos = new nsClassHashtable<nsUint32HashKey, TraceInfo>();

  if (!sTraceInfoTLS.initialized()) {
    unused << sTraceInfoTLS.init();
  }
}

void
ShutdownTaskTracer()
{
  delete sTraceInfos;
  sTraceInfos = nullptr;
}

TraceInfo*
GetOrCreateTraceInfo()
{
  NS_ENSURE_TRUE(IsInitialized(), nullptr);

  TraceInfo* info = sTraceInfoTLS.get();
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
  NS_ENSURE_TRUE_VOID(IsInitialized());

  // Log format:
  // [0 taskId dispatchTime sourceEventId sourceEventType parentTaskId]
}

void
LogBegin(uint64_t aTaskId, uint64_t aSourceEventId)
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  // Log format:
  // [1 taskId beginTime processId threadId]
}

void
LogEnd(uint64_t aTaskId, uint64_t aSourceEventId)
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  // Log format:
  // [2 taskId endTime]
}

void
LogVirtualTablePtr(uint64_t aTaskId, uint64_t aSourceEventId, int* aVptr)
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  // Log format:
  // [4 taskId address]
}

void
FreeTraceInfo()
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  FreeTraceInfo(gettid());
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
  NS_ENSURE_TRUE_VOID(IsInitialized());

  va_list args;
  va_start(args, aFormat);
  char buffer[MAX_USER_LABEL_LEN] = {0};
  vsnprintf(buffer, MAX_USER_LABEL_LEN, aFormat, args);
  va_end(args);

  // Log format:
  // [3 taskId "label"]
}

} // namespace tasktracer
} // namespace mozilla
