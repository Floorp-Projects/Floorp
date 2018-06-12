/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IdleTaskRunner.h"
#include "nsRefreshDriver.h"
#include "mozilla/SystemGroup.h"
#include "nsComponentManagerUtils.h"

namespace mozilla {

already_AddRefed<IdleTaskRunner>
IdleTaskRunner::Create(const CallbackType& aCallback,
                       const char* aRunnableName, uint32_t aDelay,
                       int64_t aBudget, bool aRepeating,
                       const MayStopProcessingCallbackType& aMayStopProcessing,
                       TaskCategory aTaskCategory)
{
  if (aMayStopProcessing && aMayStopProcessing()) {
    return nullptr;
  }

  RefPtr<IdleTaskRunner> runner =
    new IdleTaskRunner(aCallback, aRunnableName, aDelay,
                       aBudget, aRepeating, aMayStopProcessing,
                       aTaskCategory);
  runner->Schedule(false); // Initial scheduling shouldn't use idle dispatch.
  return runner.forget();
}

IdleTaskRunner::IdleTaskRunner(const CallbackType& aCallback,
                               const char* aRunnableName,
                               uint32_t aDelay, int64_t aBudget,
                               bool aRepeating,
                               const MayStopProcessingCallbackType& aMayStopProcessing,
                               TaskCategory aTaskCategory)
  : IdleRunnable(aRunnableName)
  , mCallback(aCallback), mDelay(aDelay)
  , mBudget(TimeDuration::FromMilliseconds(aBudget))
  , mRepeating(aRepeating), mTimerActive(false)
  , mMayStopProcessing(aMayStopProcessing)
  , mTaskCategory(aTaskCategory)
  , mName(aRunnableName)
{
}

NS_IMETHODIMP
IdleTaskRunner::Run()
{
  if (!mCallback) {
    return NS_OK;
  }

  // Deadline is null when called from timer.
  TimeStamp now = TimeStamp::Now();
  bool deadLineWasNull = mDeadline.IsNull();
  bool didRun = false;
  bool allowIdleDispatch = false;
  if (deadLineWasNull || ((now + mBudget) < mDeadline)) {
    CancelTimer();
    didRun = mCallback(mDeadline);
    // If we didn't do meaningful work, don't schedule using immediate
    // idle dispatch, since that could lead to a loop until the idle
    // period ends.
    allowIdleDispatch = didRun;
  } else if (now >= mDeadline) {
    allowIdleDispatch = true;
  }

  if (mCallback && (mRepeating || !didRun)) {
    Schedule(allowIdleDispatch);
  } else {
    mCallback = nullptr;
  }

  return NS_OK;
}

static void
TimedOut(nsITimer* aTimer, void* aClosure)
{
  RefPtr<IdleTaskRunner> runnable = static_cast<IdleTaskRunner*>(aClosure);
  runnable->Run();
}

void
IdleTaskRunner::SetDeadline(mozilla::TimeStamp aDeadline)
{
  mDeadline = aDeadline;
};

void IdleTaskRunner::SetTimer(uint32_t aDelay, nsIEventTarget* aTarget)
{
  MOZ_ASSERT(NS_IsMainThread());
  // aTarget is always the main thread event target provided from
  // NS_IdleDispatchToCurrentThread(). We ignore aTarget here to ensure that
  // CollectorRunner always run specifically on SystemGroup::EventTargetFor(
  // TaskCategory::GarbageCollection) of the main thread.
  SetTimerInternal(aDelay);
}

nsresult
IdleTaskRunner::Cancel()
{
  CancelTimer();
  mTimer = nullptr;
  mScheduleTimer = nullptr;
  mCallback = nullptr;
  return NS_OK;
}

static void
ScheduleTimedOut(nsITimer* aTimer, void* aClosure)
{
  RefPtr<IdleTaskRunner> runnable = static_cast<IdleTaskRunner*>(aClosure);
  runnable->Schedule(true);
}

void
IdleTaskRunner::Schedule(bool aAllowIdleDispatch)
{
  if (!mCallback) {
    return;
  }

  if (mMayStopProcessing && mMayStopProcessing()) {
    Cancel();
    return;
  }

  mDeadline = TimeStamp();
  TimeStamp now = TimeStamp::Now();
  TimeStamp hint = nsRefreshDriver::GetIdleDeadlineHint(now);
  if (hint != now) {
    // RefreshDriver is ticking, let it schedule the idle dispatch.
    nsRefreshDriver::DispatchIdleRunnableAfterTick(this, mDelay);
    // Ensure we get called at some point, even if RefreshDriver is stopped.
    SetTimerInternal(mDelay);
  } else {
    // RefreshDriver doesn't seem to be running.
    if (aAllowIdleDispatch) {
      nsCOMPtr<nsIRunnable> runnable = this;
      SetTimerInternal(mDelay);
      NS_IdleDispatchToCurrentThread(runnable.forget());
    } else {
      if (!mScheduleTimer) {
        mScheduleTimer = NS_NewTimer();
        if (!mScheduleTimer) {
          return;
        }
      } else {
        mScheduleTimer->Cancel();
      }
      if (TaskCategory::Count != mTaskCategory) {
        mScheduleTimer->SetTarget(SystemGroup::EventTargetFor(mTaskCategory));
      }
      // We weren't allowed to do idle dispatch immediately, do it after a
      // short timeout.
      mScheduleTimer->InitWithNamedFuncCallback(ScheduleTimedOut, this, 16,
                                                nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
                                                mName);
    }
  }
}

IdleTaskRunner::~IdleTaskRunner()
{
  CancelTimer();
}

void
IdleTaskRunner::CancelTimer()
{
  nsRefreshDriver::CancelIdleRunnable(this);
  if (mTimer) {
    mTimer->Cancel();
  }
  if (mScheduleTimer) {
    mScheduleTimer->Cancel();
  }
  mTimerActive = false;
}

void
IdleTaskRunner::SetTimerInternal(uint32_t aDelay)
{
  if (mTimerActive) {
    return;
  }

  if (!mTimer) {
    mTimer = NS_NewTimer();
  } else {
    mTimer->Cancel();
  }

  if (mTimer) {
    if (TaskCategory::Count != mTaskCategory) {
      mTimer->SetTarget(SystemGroup::EventTargetFor(mTaskCategory));
    }
    mTimer->InitWithNamedFuncCallback(TimedOut, this, aDelay,
                                      nsITimer::TYPE_ONE_SHOT,
                                      mName);
    mTimerActive = true;
  }
}

} // end of namespace mozilla
