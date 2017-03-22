/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRACED_TASK_COMMON_H
#define TRACED_TASK_COMMON_H

#include "GeckoTaskTracer.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace tasktracer {

class TracedTaskCommon
{
public:
  TracedTaskCommon();
  TracedTaskCommon(const TracedTaskCommon& aSrc)
    : mSourceEventType(aSrc.mSourceEventType)
    , mSourceEventId(aSrc.mSourceEventId)
    , mParentTaskId(aSrc.mParentTaskId)
    , mTaskId(aSrc.mTaskId)
    , mIsTraceInfoInit(aSrc.mIsTraceInfoInit) {}
  virtual ~TracedTaskCommon();

  void DispatchTask(int aDelayTimeMs = 0);

  void SetTLSTraceInfo() {
    if (mIsTraceInfoInit) {
      DoSetTLSTraceInfo();
    }
  }
  void GetTLSTraceInfo() {
    if (IsStartLogging()) {
      DoGetTLSTraceInfo();
    }
  }
  void ClearTLSTraceInfo();

private:
  void DoSetTLSTraceInfo();
  void DoGetTLSTraceInfo();

protected:
  void Init();

  // TraceInfo of TLS will be set by the following parameters, including source
  // event type, source event ID, parent task ID, and task ID of this traced
  // task/runnable.
  SourceEventType mSourceEventType;
  uint64_t mSourceEventId;
  uint64_t mParentTaskId;
  uint64_t mTaskId;
  bool mIsTraceInfoInit;
};

class TracedRunnable : public TracedTaskCommon
                     , public Runnable
{
public:
  NS_DECL_NSIRUNNABLE

  explicit TracedRunnable(already_AddRefed<nsIRunnable>&& aOriginalObj);

private:
  virtual ~TracedRunnable();

  nsCOMPtr<nsIRunnable> mOriginalObj;
};

/**
 * This class is used to create a logical task, without a real
 * runnable.
 */
class VirtualTask : public TracedTaskCommon {
public:
  VirtualTask() : TracedTaskCommon() {}

  VirtualTask(const VirtualTask& aSrc) : TracedTaskCommon(aSrc) {}

  /**
   * Initialize the task to create an unique ID, and store other
   * information.
   *
   * This method may be called for one or more times.
   */
  void Init(uintptr_t* aVPtr = nullptr) {
    TracedTaskCommon::Init();
    if (aVPtr) {
      extern void LogVirtualTablePtr(uint64_t aTaskId, uint64_t aSourceEventId, uintptr_t* aVptr);
      LogVirtualTablePtr(mTaskId, mSourceEventId, aVPtr);
    }
    DispatchTask();
  }

  /**
   * Define the life-span of a VirtualTask.
   *
   * VirtualTask is not a real task, goes without a runnable, it's
   * instances are never dispatched and ran by event loops.  This
   * class used to define running time as the life-span of it's
   * instance.
   */
  class AutoRunTask : public AutoSaveCurTraceInfo {
    VirtualTask* mTask;
    void StartScope(VirtualTask *aTask);
    void StopScope();
  public:
    explicit AutoRunTask(VirtualTask *aTask)
      : AutoSaveCurTraceInfo()
      , mTask(aTask) {
      if (HasSavedTraceInfo()) {
        StartScope(aTask);
      }
    }
    ~AutoRunTask() {
      if (HasSavedTraceInfo()) {
        StopScope();
      }
    }
  };
};

} // namespace tasktracer
} // namespace mozilla

#endif
