/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IdleTaskRunner.h"
#include "nsRefreshDriver.h"
#include "nsComponentManagerUtils.h"

namespace mozilla {

already_AddRefed<IdleTaskRunner> IdleTaskRunner::Create(
    const CallbackType& aCallback, const char* aRunnableName,
    uint32_t aStartDelay, uint32_t aMaxDelay, int64_t aMinimumUsefulBudget,
    bool aRepeating, const MayStopProcessingCallbackType& aMayStopProcessing) {
  if (aMayStopProcessing && aMayStopProcessing()) {
    return nullptr;
  }

  RefPtr<IdleTaskRunner> runner =
      new IdleTaskRunner(aCallback, aRunnableName, aStartDelay, aMaxDelay,
                         aMinimumUsefulBudget, aRepeating, aMayStopProcessing);
  runner->Schedule(false);  // Initial scheduling shouldn't use idle dispatch.
  return runner.forget();
}

class IdleTaskRunnerTask : public Task {
 public:
  explicit IdleTaskRunnerTask(IdleTaskRunner* aRunner)
      : Task(true, EventQueuePriority::Idle), mRunner(aRunner) {
    SetManager(TaskController::Get()->GetIdleTaskManager());
  }

  bool Run() override {
    if (mRunner) {
      // IdleTaskRunner::Run can actually trigger the destruction of the
      // IdleTaskRunner. Make sure it doesn't get destroyed before the method
      // finished.
      RefPtr<IdleTaskRunner> runner(mRunner);
      runner->Run();
    }
    return true;
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

 private:
  IdleTaskRunner* mRunner;
};

IdleTaskRunner::IdleTaskRunner(
    const CallbackType& aCallback, const char* aRunnableName,
    uint32_t aStartDelay, uint32_t aMaxDelay, int64_t aMinimumUsefulBudget,
    bool aRepeating, const MayStopProcessingCallbackType& aMayStopProcessing)
    : mCallback(aCallback),
      mStartTime(TimeStamp::Now() +
                 TimeDuration::FromMilliseconds(aStartDelay)),
      mMaxDelay(aMaxDelay),
      mMinimumUsefulBudget(
          TimeDuration::FromMilliseconds(aMinimumUsefulBudget)),
      mRepeating(aRepeating),
      mTimerActive(false),
      mMayStopProcessing(aMayStopProcessing),
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

void IdleTaskRunner::SetTimer(uint32_t aDelay, nsIEventTarget* aTarget) {
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
  bool useRefreshDriver = false;
  if (now >= mStartTime) {
    // Detect whether the refresh driver is ticking by checking if
    // GetIdleDeadlineHint returns its input parameter.
    useRefreshDriver = (nsRefreshDriver::GetIdleDeadlineHint(now) != now);
  } else {
    NS_WARNING_ASSERTION(!aAllowIdleDispatch,
                         "early callback, or time went backwards");
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
    if (aAllowIdleDispatch) {
      SetTimerInternal(mMaxDelay);
      if (!mTask) {
        // If we have mTask we've already scheduled one, and the refresh driver
        // shouldn't be running if we hit this code path.
        mTask = new IdleTaskRunnerTask(this);
        RefPtr<Task> task(mTask);
        TaskController::Get()->AddTask(task.forget());
      }
    } else {
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

void IdleTaskRunner::SetTimerInternal(uint32_t aDelay) {
  if (mTimerActive) {
    return;
  }

  if (!mTimer) {
    mTimer = NS_NewTimer();
  } else {
    mTimer->Cancel();
  }

  if (mTimer) {
    mTimer->InitWithNamedFuncCallback(TimedOut, this, aDelay,
                                      nsITimer::TYPE_ONE_SHOT, mName);
    mTimerActive = true;
  }
}

}  // end of namespace mozilla
