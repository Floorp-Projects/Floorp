/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoTaskTracerImpl.h"
#include "TracedTaskCommon.h"

// NS_ENSURE_TRUE_VOID() without the warning on the debug build.
#define ENSURE_TRUE_VOID(x)   \
  do {                        \
    if (MOZ_UNLIKELY(!(x))) { \
       return;                \
    }                         \
  } while(0)

namespace mozilla {
namespace tasktracer {

TracedTaskCommon::TracedTaskCommon()
  : mSourceEventType(SourceEventType::Unknown)
  , mSourceEventId(0)
  , mParentTaskId(0)
  , mTaskId(0)
  , mIsTraceInfoInit(false)
{
}

TracedTaskCommon::~TracedTaskCommon()
{
}

void
TracedTaskCommon::Init()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  mTaskId = GenNewUniqueTaskId();
  mSourceEventId = info->mCurTraceSourceId;
  mSourceEventType = info->mCurTraceSourceType;
  mParentTaskId = info->mCurTaskId;
  mIsTraceInfoInit = true;
}

void
TracedTaskCommon::DispatchTask(int aDelayTimeMs)
{
  LogDispatch(mTaskId, mParentTaskId, mSourceEventId, mSourceEventType,
              aDelayTimeMs);
}

void
TracedTaskCommon::GetTLSTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  mSourceEventType = info->mCurTraceSourceType;
  mSourceEventId = info->mCurTraceSourceId;
  mTaskId = info->mCurTaskId;
  mIsTraceInfoInit = true;
}

void
TracedTaskCommon::SetTLSTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  if (mIsTraceInfoInit) {
    info->mCurTraceSourceId = mSourceEventId;
    info->mCurTraceSourceType = mSourceEventType;
    info->mCurTaskId = mTaskId;
  }
}

void
TracedTaskCommon::ClearTLSTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = 0;
  info->mCurTraceSourceType = SourceEventType::Unknown;
  info->mCurTaskId = 0;
}

/**
 * Implementation of class TracedRunnable.
 */
TracedRunnable::TracedRunnable(already_AddRefed<nsIRunnable>&& aOriginalObj)
  : TracedTaskCommon()
  , mOriginalObj(Move(aOriginalObj))
{
  Init();
  LogVirtualTablePtr(mTaskId, mSourceEventId, *reinterpret_cast<uintptr_t**>(mOriginalObj.get()));
}

TracedRunnable::~TracedRunnable()
{
}

NS_IMETHODIMP
TracedRunnable::Run()
{
  SetTLSTraceInfo();
  LogBegin(mTaskId, mSourceEventId);
  nsresult rv = mOriginalObj->Run();
  LogEnd(mTaskId, mSourceEventId);
  ClearTLSTraceInfo();

  return rv;
}

/**
 * CreateTracedRunnable() returns a TracedRunnable wrapping the original
 * nsIRunnable object, aRunnable.
 */
already_AddRefed<nsIRunnable>
CreateTracedRunnable(already_AddRefed<nsIRunnable>&& aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable = new TracedRunnable(Move(aRunnable));
  return runnable.forget();
}

VirtualTask::AutoRunTask::AutoRunTask(VirtualTask* aTask)
  : AutoSaveCurTraceInfo()
  , mTask(aTask)
{
  mTask->SetTLSTraceInfo();
  LogBegin(mTask->mTaskId, mTask->mSourceEventId);
}

VirtualTask::AutoRunTask::~AutoRunTask()
{
  LogEnd(mTask->mTaskId, mTask->mSourceEventId);
}

} // namespace tasktracer
} // namespace mozilla
