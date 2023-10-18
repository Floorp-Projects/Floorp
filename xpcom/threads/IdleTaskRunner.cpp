/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IdleTaskRunner.h"
#include "mozilla/TaskController.h"
#include "nsRefreshDriver.h"

namespace mozilla {

already_AddRefed<IdleTaskRunner> IdleTaskRunner::Create(
    const CallbackType& aCallback, const char* aRunnableName,
    TimeDuration aStartDelay, TimeDuration aMaxDelay,
    TimeDuration aMinimumUsefulBudget, bool aRepeating,
    const MayStopProcessingCallbackType& aMayStopProcessing,
    const RequestInterruptCallbackType& aRequestInterrupt) {
  if (aMayStopProcessing && aMayStopProcessing()) {
    return nullptr;
  }

  RefPtr<IdleTaskRunner> runner = new IdleTaskRunner(
      aCallback, aRunnableName, aStartDelay, aMaxDelay, aMinimumUsefulBudget,
      aRepeating, aMayStopProcessing, aRequestInterrupt);
  runner->Schedule(false);  // Initial scheduling shouldn't use idle dispatch.
  return runner.forget();
}

class IdleTaskRunnerTask : public Task {
 public:
  explicit IdleTaskRunnerTask(IdleTaskRunner* aRunner)
      : Task(Kind::MainThreadOnly, EventQueuePriority::Idle),
        mRunner(aRunner),
        mRequestInterrupt(aRunner->mRequestInterrupt) {
    SetManager(TaskController::Get()->GetIdleTaskManager());
  }

  TaskResult Run() override {
    if (mRunner) {
      // IdleTaskRunner::Run can actually trigger the destruction of the
      // IdleTaskRunner. Make sure it doesn't get destroyed before the method
      // finished.
      RefPtr<IdleTaskRunner> runner(mRunner);
      runner->Run();
    }
    return TaskResult::Complete;
  }

  void SetIdleDeadline(TimeStamp aDeadline) override {
    if (mRunner) {
      mRunner->SetIdleDeadline(aDeadline);
    }
  }

  void Cancel() { mRunner = nullptr; }

  bool GetName(nsACString& aName) override {
    if (mRunner) {
      aName.Assign(mRunner->GetName());
    } else {
      aName.Assign("ExpiredIdleTaskRunner");
    }
    return true;
  }

  void RequestInterrupt(uint32_t aInterruptPriority) override {
    if (mRequestInterrupt) {
      mRequestInterrupt(aInterruptPriority);
    }
  }

 private:
  IdleTaskRunner* mRunner;

  // Copied here and invoked even if there is no mRunner currently, to avoid
  // race conditions checking mRunner when an interrupt is requested.
  IdleTaskRunner::RequestInterruptCallbackType mRequestInterrupt;
};

IdleTaskRunner::IdleTaskRunner(
    const CallbackType& aCallback, const char* aRunnableName,
    TimeDuration aStartDelay, TimeDuration aMaxDelay,
    TimeDuration aMinimumUsefulBudget, bool aRepeating,
    const MayStopProcessingCallbackType& aMayStopProcessing,
    const RequestInterruptCallbackType& aRequestInterrupt)
    : mCallback(aCallback),
      mStartTime(TimeStamp::Now() + aStartDelay),
      mMaxDelay(aMaxDelay),
      mMinimumUsefulBudget(aMinimumUsefulBudget),
      mRepeating(aRepeating),
      mTimerActive(false),
      mMayStopProcessing(aMayStopProcessing),
      mRequestInterrupt(aRequestInterrupt),
      mName(aRunnableName) {}

void IdleTaskRunner::Run() {
  if (!mCallback) {
    return;
  }

  // Deadline is null when called from timer or RunNextCollectorTimer rather
  // than during idle time.
  TimeStamp now = TimeStamp::Now();

  // Note that if called from RunNextCollectorTimer, we may not have reached
  // mStartTime yet. Pretend we are overdue for idle time.
  bool overdueForIdle = mDeadline.IsNull();
  bool didRun = false;
  bool allowIdleDispatch = false;

  if (mTask) {
    // If we find ourselves here we should usually be running from this task,
    // but there are exceptions. In any case we're doing the work now and don't
    // need our task going forward unless we're re-scheduled.
    nsRefreshDriver::CancelIdleTask(mTask);
    // Extra safety, make sure a task can never have a dangling ptr.
    mTask->Cancel();
    mTask = nullptr;
  }

  if (overdueForIdle || ((now + mMinimumUsefulBudget) < mDeadline)) {
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
}

static void TimedOut(nsITimer* aTimer, void* aClosure) {
  RefPtr<IdleTaskRunner> runner = static_cast<IdleTaskRunner*>(aClosure);
  runner->Run();
}

void IdleTaskRunner::SetIdleDeadline(mozilla::TimeStamp aDeadline) {
  mDeadline = aDeadline;
}

void IdleTaskRunner::SetMinimumUsefulBudget(int64_t aMinimumUsefulBudget) {
  mMinimumUsefulBudget = TimeDuration::FromMilliseconds(aMinimumUsefulBudget);
}

void IdleTaskRunner::SetTimer(TimeDuration aDelay, nsIEventTarget* aTarget) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTarget->IsOnCurrentThread());
  // aTarget is always the main thread event target provided from
  // NS_DispatchToCurrentThreadQueue(). We ignore aTarget here to ensure that
  // CollectorRunner always run specifically the main thread.
  SetTimerInternal(aDelay);
}

void IdleTaskRunner::Cancel() {
  CancelTimer();
  mTimer = nullptr;
  mScheduleTimer = nullptr;
  mCallback = nullptr;
}

static void ScheduleTimedOut(nsITimer* aTimer, void* aClosure) {
  RefPtr<IdleTaskRunner> runnable = static_cast<IdleTaskRunner*>(aClosure);
  runnable->Schedule(true);
}

void IdleTaskRunner::Schedule(bool aAllowIdleDispatch) {
  if (!mCallback) {
    return;
  }

  if (mMayStopProcessing && mMayStopProcessing()) {
    Cancel();
    return;
  }

  mDeadline = TimeStamp();

  TimeStamp now = TimeStamp::Now();

  if (aAllowIdleDispatch) {
    SetTimerInternal(mMaxDelay);
    if (!mTask) {
      mTask = new IdleTaskRunnerTask(this);
      RefPtr<Task> task(mTask);
      TaskController::Get()->AddTask(task.forget());
    }
    return;
  }

  bool useRefreshDriver = false;
  if (now >= mStartTime) {
    // Detect whether the refresh driver is ticking by checking if
    // GetIdleDeadlineHint returns its input parameter.
    useRefreshDriver =
        (nsRefreshDriver::GetIdleDeadlineHint(
             now, nsRefreshDriver::IdleCheck::OnlyThisProcessRefreshDriver) !=
         now);
  }

  if (useRefreshDriver) {
    if (!mTask) {
      // If a task was already scheduled, no point rescheduling.
      mTask = new IdleTaskRunnerTask(this);
      // RefreshDriver is ticking, let it schedule the idle dispatch.
      nsRefreshDriver::DispatchIdleTaskAfterTickUnlessExists(mTask);
    }
    // Ensure we get called at some point, even if RefreshDriver is stopped.
    SetTimerInternal(mMaxDelay);
  } else {
    // RefreshDriver doesn't seem to be running.
    if (!mScheduleTimer) {
      mScheduleTimer = NS_NewTimer();
      if (!mScheduleTimer) {
        return;
      }
    } else {
      mScheduleTimer->Cancel();
    }
    // We weren't allowed to do idle dispatch immediately, do it after a
    // short timeout. (Or wait for our start time if we haven't started yet.)
    uint32_t waitToSchedule = 16; /* ms */
    if (now < mStartTime) {
      // + 1 to round milliseconds up to be sure to wait until after
      // mStartTime.
      waitToSchedule = (mStartTime - now).ToMilliseconds() + 1;
    }
    mScheduleTimer->InitWithNamedFuncCallback(
        ScheduleTimedOut, this, waitToSchedule,
        nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY, mName);
  }
}

IdleTaskRunner::~IdleTaskRunner() { CancelTimer(); }

void IdleTaskRunner::CancelTimer() {
  if (mTask) {
    nsRefreshDriver::CancelIdleTask(mTask);
    mTask->Cancel();
    mTask = nullptr;
  }
  if (mTimer) {
    mTimer->Cancel();
  }
  if (mScheduleTimer) {
    mScheduleTimer->Cancel();
  }
  mTimerActive = false;
}

void IdleTaskRunner::SetTimerInternal(TimeDuration aDelay) {
  if (mTimerActive) {
    return;
  }

  if (!mTimer) {
    mTimer = NS_NewTimer();
  } else {
    mTimer->Cancel();
  }

  if (mTimer) {
    mTimer->InitWithNamedFuncCallback(TimedOut, this, aDelay.ToMilliseconds(),
                                      nsITimer::TYPE_ONE_SHOT, mName);
    mTimerActive = true;
  }
}

}  // end of namespace mozilla
