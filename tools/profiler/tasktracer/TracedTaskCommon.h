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
  virtual ~TracedTaskCommon();

  void DispatchTask(int aDelayTimeMs = 0);

  void SetTLSTraceInfo();
  void GetTLSTraceInfo();
  void ClearTLSTraceInfo();

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

  TracedRunnable(already_AddRefed<nsIRunnable>&& aOriginalObj);

private:
  virtual ~TracedRunnable();

  nsCOMPtr<nsIRunnable> mOriginalObj;
};

} // namespace tasktracer
} // namespace mozilla

#endif
