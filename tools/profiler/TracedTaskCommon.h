/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRACED_TASK_COMMON_H
#define TRACED_TASK_COMMON_H

#include "base/task.h"
#include "GeckoTaskTracer.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace tasktracer {

class TracedTaskCommon
{
public:
  TracedTaskCommon();
  virtual ~TracedTaskCommon() {}

protected:
  void Init();

  // Sets up the metadata on the current thread's TraceInfo for this task.
  // After Run(), ClearTraceInfo is called to reset the metadata.
  void SetTraceInfo();
  void ClearTraceInfo();

  // Its own task Id, an unique number base on its thread Id and a last unique
  // task Id stored in its TraceInfo.
  uint64_t mTaskId;

  uint64_t mSourceEventId;
  SourceEventType mSourceEventType;
};

class TracedRunnable : public TracedTaskCommon
                     , public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  TracedRunnable(nsIRunnable* aOriginalObj);

private:
  virtual ~TracedRunnable() {}

  nsCOMPtr<nsIRunnable> mOriginalObj;
};

class TracedTask : public TracedTaskCommon
                 , public Task
{
public:
  TracedTask(Task* aOriginalObj);
  ~TracedTask()
  {
    if (mOriginalObj) {
      delete mOriginalObj;
      mOriginalObj = nullptr;
    }
  }

  virtual void Run();

private:
  Task* mOriginalObj;
};

// FakeTracedTask is for tracking events that are not directly dispatched from
// their parents, e.g. The timer events.
class FakeTracedTask : public TracedTaskCommon
{
public:
  FakeTracedTask() : TracedTaskCommon() {}
  FakeTracedTask(int* aVptr);
  FakeTracedTask(const FakeTracedTask& aTask);
  void BeginFakeTracedTask();
  void EndFakeTracedTask();
};

class AutoRunFakeTracedTask
{
public:
  AutoRunFakeTracedTask(FakeTracedTask* aFakeTracedTask);
  ~AutoRunFakeTracedTask();
private:
  FakeTracedTask mFakeTracedTask;
  bool mInitialized;
};

} // namespace tasktracer
} // namespace mozilla

#endif
