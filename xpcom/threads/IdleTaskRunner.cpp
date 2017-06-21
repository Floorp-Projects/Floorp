/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IdleTaskRunner.h"
#include "nsRefreshDriver.h"

namespace mozilla {

already_AddRefed<IdleTaskRunner>
IdleTaskRunner::Create(const CallbackType& aCallback, uint32_t aDelay,
                       int64_t aBudget, bool aRepeating,
                       const MayContinueProcessingCallbackType& aMaybeContinueProcessing)
{
  if (aMaybeContinueProcessing && aMaybeContinueProcessing()) {
    return nullptr;
  }

  RefPtr<IdleTaskRunner> runner =
    new IdleTaskRunner(aCallback, aDelay, aBudget, aRepeating, aMaybeContinueProcessing);
  runner->Schedule(false); // Initial scheduling shouldn't use idle dispatch.
  return runner.forget();
}

IdleTaskRunner::IdleTaskRunner(const CallbackType& aCallback,
                               uint32_t aDelay, int64_t aBudget,
                               bool aRepeating,
                               const MayContinueProcessingCallbackType& aMaybeContinueProcessing)
  : mCallback(aCallback), mDelay(aDelay)
  , mBudget(TimeDuration::FromMilliseconds(aBudget))
  , mRepeating(aRepeating), mTimerActive(false)
  , mMaybeContinueProcessing(aMaybeContinueProcessing)
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
    mTimer->InitWithFuncCallback(TimedOut, this, aDelay,
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

  if (mMaybeContinueProcessing && mMaybeContinueProcessing()) {
    Cancel();
    return;
  }

  TimeStamp deadline = mDeadline;
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
      uint32_t lateScheduleDelayMs;
      if (deadline.IsNull()) {
        lateScheduleDelayMs = 16;
      } else {
        lateScheduleDelayMs =
          (uint32_t)((deadline - now).ToMilliseconds());
      }
      mScheduleTimer->InitWithFuncCallback(ScheduleTimedOut, this, lateScheduleDelayMs,
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
