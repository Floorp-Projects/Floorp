/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IdleTaskRunner.h"
#include "nsRefreshDriver.h"

namespace mozilla {

already_AddRefed<IdleTaskRunner>
IdleTaskRunner::Create(IdleTaskRunnerCallback aCallback, uint32_t aDelay,
                       int64_t aBudget, bool aRepeating, void* aData)
{
  if (sShuttingDown) {
    return nullptr;
  }

  RefPtr<IdleTaskRunner> runner =
    new IdleTaskRunner(aCallback, aDelay, aBudget, aRepeating, aData);
  runner->Schedule(false); // Initial scheduling shouldn't use idle dispatch.
  return runner.forget();
}

IdleTaskRunner::IdleTaskRunner(IdleTaskRunnerCallback aCallback,
                               uint32_t aDelay, int64_t aBudget,
                               bool aRepeating, void* aData)
  : mCallback(aCallback), mDelay(aDelay)
  , mBudget(TimeDuration::FromMilliseconds(aBudget))
  , mRepeating(aRepeating), mTimerActive(false), mData(aData)
{
}

NS_IMETHODIMP
IdleTaskRunner::Run()
{
  if (!mCallback) {
    return NS_OK;
  }

  // Deadline is null when called from timer.
  bool deadLineWasNull = mDeadline.IsNull();
  bool didRun = false;
  if (deadLineWasNull || ((TimeStamp::Now() + mBudget) < mDeadline)) {
    CancelTimer();
    didRun = mCallback(mDeadline, mData);
  }

  if (mCallback && (mRepeating || !didRun)) {
    // If we didn't do meaningful work, don't schedule using immediate
    // idle dispatch, since that could lead to a loop until the idle
    // period ends.
    Schedule(didRun);
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
  if (mTimerActive) {
    return;
  }
  mTarget = aTarget;
  if (!mTimer) {
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  } else {
    mTimer->Cancel();
  }

  if (mTimer) {
    mTimer->SetTarget(mTarget);
    mTimer->InitWithNamedFuncCallback(TimedOut, this, aDelay,
                                      nsITimer::TYPE_ONE_SHOT);
    mTimerActive = true;
  }
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

  if (sShuttingDown) {
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
    SetTimer(mDelay, mTarget);
  } else {
    // RefreshDriver doesn't seem to be running.
    if (aAllowIdleDispatch) {
      nsCOMPtr<nsIRunnable> runnable = this;
      NS_IdleDispatchToCurrentThread(runnable.forget(), mDelay);
      SetTimer(mDelay, mTarget);
    } else {
      if (!mScheduleTimer) {
        mScheduleTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
        if (!mScheduleTimer) {
          return;
        }
      } else {
        mScheduleTimer->Cancel();
      }

      // We weren't allowed to do idle dispatch immediately, do it after a
      // short timeout.
      mScheduleTimer->InitWithNamedFuncCallback(ScheduleTimedOut, this, 16,
                                                nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY);
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

} // end of namespace mozilla
