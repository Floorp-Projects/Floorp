/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoTaskTracerImpl.h"
#include "TracedTaskCommon.h"

namespace mozilla {
namespace tasktracer {

TracedTaskCommon::TracedTaskCommon()
  : mSourceEventId(0)
  , mSourceEventType(SourceEventType::UNKNOWN)
{
  Init();
}

void
TracedTaskCommon::Init()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  mTaskId = GenNewUniqueTaskId();
  mSourceEventId = info->mCurTraceSourceId;
  mSourceEventType = info->mCurTraceSourceType;

  LogDispatch(mTaskId, info->mCurTaskId, mSourceEventId, mSourceEventType);
}

void
TracedTaskCommon::SetTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = mSourceEventId;
  info->mCurTraceSourceType = mSourceEventType;
  info->mCurTaskId = mTaskId;
}

void
TracedTaskCommon::ClearTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = 0;
  info->mCurTraceSourceType = SourceEventType::UNKNOWN;
  info->mCurTaskId = 0;
}

/**
 * Implementation of class TracedRunnable.
 */
TracedRunnable::TracedRunnable(nsIRunnable* aOriginalObj)
  : TracedTaskCommon()
  , mOriginalObj(aOriginalObj)
{
  LogVirtualTablePtr(mTaskId, mSourceEventId, *(int**)(aOriginalObj));
}

NS_IMETHODIMP
TracedRunnable::Run()
{
  LogBegin(mTaskId, mSourceEventId);

  SetTraceInfo();
  nsresult rv = mOriginalObj->Run();
  ClearTraceInfo();

  LogEnd(mTaskId, mSourceEventId);
  return rv;
}

/**
 * Implementation of class TracedTask.
 */
TracedTask::TracedTask(Task* aOriginalObj)
  : TracedTaskCommon()
  , mOriginalObj(aOriginalObj)
{
  LogVirtualTablePtr(mTaskId, mSourceEventId, *(int**)(aOriginalObj));
}

void
TracedTask::Run()
{
  LogBegin(mTaskId, mSourceEventId);

  SetTraceInfo();
  mOriginalObj->Run();
  ClearTraceInfo();

  LogEnd(mTaskId, mSourceEventId);
}

FakeTracedTask::FakeTracedTask(int* aVptr)
  : TracedTaskCommon()
{
  LogVirtualTablePtr(mTaskId, mSourceEventId, aVptr);
}

FakeTracedTask::FakeTracedTask(const FakeTracedTask& aTask)
{
  mTaskId = aTask.mTaskId;
  mSourceEventId = aTask.mSourceEventId;
  mSourceEventType = aTask.mSourceEventType;
}

void
FakeTracedTask::BeginFakeTracedTask()
{
  LogBegin(mTaskId, mSourceEventId);
  SetTraceInfo();
}

void
FakeTracedTask::EndFakeTracedTask()
{
  ClearTraceInfo();
  LogEnd(mTaskId, mSourceEventId);
}

AutoRunFakeTracedTask::AutoRunFakeTracedTask(FakeTracedTask* aFakeTracedTask)
  : mInitialized(false)
{
  if (aFakeTracedTask) {
    mInitialized = true;
    mFakeTracedTask = *aFakeTracedTask;
    mFakeTracedTask.BeginFakeTracedTask();
  }
}

AutoRunFakeTracedTask::~AutoRunFakeTracedTask()
{
  if (mInitialized) {
    mFakeTracedTask.EndFakeTracedTask();
  }
}

/**
 * CreateTracedRunnable() returns a TracedRunnable wrapping the original
 * nsIRunnable object, aRunnable.
 */
already_AddRefed<nsIRunnable>
CreateTracedRunnable(nsIRunnable* aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable = new TracedRunnable(aRunnable);
  return runnable.forget();
}

/**
 * CreateTracedTask() returns a TracedTask wrapping the original Task object,
 * aTask.
 */
Task*
CreateTracedTask(Task* aTask)
{
  Task* task = new TracedTask(aTask);
  return task;
}

/**
 * CreateFakeTracedTask() returns a FakeTracedTask tracking the event which is
 * not dispatched from its parent task directly, such as timer events.
 */
FakeTracedTask*
CreateFakeTracedTask(int* aVptr)
{
  nsAutoPtr<FakeTracedTask> task(new FakeTracedTask(aVptr));
  return task.forget();
}

} // namespace tasktracer
} // namespace mozilla
