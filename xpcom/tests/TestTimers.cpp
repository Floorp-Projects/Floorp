/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"

#include "nsIThread.h"
#include "nsITimer.h"

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "prinrval.h"
#include "prmon.h"
#include "prthread.h"
#include "mozilla/Attributes.h"

#include "mozilla/ReentrantMonitor.h"

#include <list>
#include <vector>
#include <stdio.h>

using namespace mozilla;

typedef nsresult(*TestFuncPtr)();

class AutoTestThread
{
public:
  AutoTestThread() {
    nsCOMPtr<nsIThread> newThread;
    nsresult rv = NS_NewThread(getter_AddRefs(newThread));
    if (NS_FAILED(rv))
      return;

    newThread.swap(mThread);
  }

  ~AutoTestThread() {
    mThread->Shutdown();
  }

  operator nsIThread*() const {
    return mThread;
  }

  nsIThread* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN {
    return mThread;
  }

private:
  nsCOMPtr<nsIThread> mThread;
};

class AutoCreateAndDestroyReentrantMonitor
{
public:
  AutoCreateAndDestroyReentrantMonitor() {
    mReentrantMonitor = new ReentrantMonitor("TestTimers::AutoMon");
    MOZ_ASSERT(mReentrantMonitor, "Out of memory!");
  }

  ~AutoCreateAndDestroyReentrantMonitor() {
    delete mReentrantMonitor;
  }

  operator ReentrantMonitor* () {
    return mReentrantMonitor;
  }

private:
  ReentrantMonitor* mReentrantMonitor;
};

class TimerCallback final : public nsITimerCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  TimerCallback(nsIThread** aThreadPtr, ReentrantMonitor* aReentrantMonitor)
  : mThreadPtr(aThreadPtr), mReentrantMonitor(aReentrantMonitor) { }

  NS_IMETHOD Notify(nsITimer* aTimer) override {
    MOZ_ASSERT(mThreadPtr, "Callback was not supposed to be called!");
    nsCOMPtr<nsIThread> current(do_GetCurrentThread());

    ReentrantMonitorAutoEnter mon(*mReentrantMonitor);

    MOZ_ASSERT(!*mThreadPtr, "Timer called back more than once!");
    *mThreadPtr = current;

    mon.Notify();

    return NS_OK;
  }
private:
  ~TimerCallback() {}

  nsIThread** mThreadPtr;
  ReentrantMonitor* mReentrantMonitor;
};

NS_IMPL_ISUPPORTS(TimerCallback, nsITimerCallback)

nsresult
TestTargetedTimers()
{
  AutoCreateAndDestroyReentrantMonitor newMon;
  NS_ENSURE_TRUE(newMon, NS_ERROR_OUT_OF_MEMORY);

  AutoTestThread testThread;
  NS_ENSURE_TRUE(testThread, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIEventTarget* target = static_cast<nsIEventTarget*>(testThread);

  rv = timer->SetTarget(target);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIThread* notifiedThread = nullptr;

  nsCOMPtr<nsITimerCallback> callback =
    new TimerCallback(&notifiedThread, newMon);
  NS_ENSURE_TRUE(callback, NS_ERROR_OUT_OF_MEMORY);

  rv = timer->InitWithCallback(callback, 2000, nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  ReentrantMonitorAutoEnter mon(*newMon);
  while (!notifiedThread) {
    mon.Wait();
  }
  NS_ENSURE_TRUE(notifiedThread == testThread, NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
TestTimerWithStoppedTarget()
{
  AutoTestThread testThread;
  NS_ENSURE_TRUE(testThread, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIEventTarget* target = static_cast<nsIEventTarget*>(testThread);

  rv = timer->SetTarget(target);
  NS_ENSURE_SUCCESS(rv, rv);

  // If this is called, we'll assert
  nsCOMPtr<nsITimerCallback> callback =
    new TimerCallback(nullptr, nullptr);
  NS_ENSURE_TRUE(callback, NS_ERROR_OUT_OF_MEMORY);

  rv = timer->InitWithCallback(callback, 100, nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  testThread->Shutdown();

  PR_Sleep(400);

  return NS_OK;
}

#define FUZZ_MAX_TIMEOUT 9
class FuzzTestThreadState final : public nsITimerCallback {
  public:
    NS_DECL_THREADSAFE_ISUPPORTS

    explicit FuzzTestThreadState(nsIThread* thread) :
      mThread(thread),
      mStopped(false)
    {}

    class StartRunnable final : public mozilla::Runnable {
      public:
        explicit StartRunnable(FuzzTestThreadState* threadState) :
          mThreadState(threadState)
        {}

        NS_IMETHOD Run() override
        {
          mThreadState->ScheduleOrCancelTimers();
          return NS_OK;
        }

      private:
        RefPtr<FuzzTestThreadState> mThreadState;
    };

    void Start()
    {
      nsCOMPtr<nsIRunnable> runnable = new StartRunnable(this);
      nsresult rv = mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
      if (NS_FAILED(rv)) {
        MOZ_ASSERT(false, "Failed to dispatch StartRunnable.");
      }
    }

    void Stop()
    {
      mStopped = true;
    }

    NS_IMETHOD Notify(nsITimer* aTimer) override
    {
      bool onCorrectThread;
      nsresult rv = mThread->IsOnCurrentThread(&onCorrectThread);
      MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed to perform thread check.");
      MOZ_ASSERT(onCorrectThread, "Notify invoked on wrong thread.");

      uint32_t delay;
      rv = aTimer->GetDelay(&delay);
      if (NS_FAILED(rv)) {
        MOZ_ASSERT(false, "GetDelay failed.");
        return rv;
      }

      if (delay > FUZZ_MAX_TIMEOUT) {
        MOZ_ASSERT(false, "Delay was an invalid value for this test.");
        return NS_ERROR_FAILURE;
      }

      uint32_t type;
      rv = aTimer->GetType(&type);
      MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed to get timer type.");
      MOZ_ASSERT(type <= nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP);

      if (type == nsITimer::TYPE_ONE_SHOT) {
        if (mOneShotTimersByDelay[delay].empty()) {
          MOZ_ASSERT(false, "Unexpected one-shot timer.");
          return NS_ERROR_FAILURE;
        }

        if (mOneShotTimersByDelay[delay].front().get() != aTimer) {
          MOZ_ASSERT(false,
                     "One-shot timers for a given duration have been reordered.");
          return NS_ERROR_FAILURE;
        }

        mOneShotTimersByDelay[delay].pop_front();
        --mTimersOutstanding;
      } else if (mStopped) {
        CancelRepeatingTimer(aTimer);
      }

      ScheduleOrCancelTimers();
      RescheduleSomeTimers();
      return NS_OK;
    }

    bool HasTimersOutstanding() const
    {
      return !!mTimersOutstanding;
    }

  private:
    ~FuzzTestThreadState()
    {
      for (size_t i = 0; i <= FUZZ_MAX_TIMEOUT; ++i) {
        MOZ_ASSERT(mOneShotTimersByDelay[i].empty(),
                   "Timers remain at end of test.");
      }
    }

    uint32_t GetRandomType() const
    {
      return rand() % (nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP + 1);
    }

    size_t CountOneShotTimers() const
    {
      size_t count = 0;
      for (size_t i = 0; i <= FUZZ_MAX_TIMEOUT; ++i) {
        count += mOneShotTimersByDelay[i].size();
      }
      return count;
    }

    void ScheduleOrCancelTimers()
    {
      if (mStopped) {
        return;
      }

      const size_t numTimersDesired = (rand() % 100) + 100;
      MOZ_ASSERT(numTimersDesired >= 100);
      MOZ_ASSERT(numTimersDesired < 200);
      int adjustment = numTimersDesired - mTimersOutstanding;

      while (adjustment > 0) {
        CreateRandomTimer();
        --adjustment;
      }

      while (adjustment < 0) {
        CancelRandomTimer();
        ++adjustment;
      }

      MOZ_ASSERT(numTimersDesired == mTimersOutstanding);
    }

    void RescheduleSomeTimers()
    {
      if (mStopped) {
        return;
      }

      static const size_t kNumRescheduled = 40;

      // Reschedule some timers with a Cancel first.
      for (size_t i = 0; i < kNumRescheduled; ++i) {
        InitRandomTimer(CancelRandomTimer().get());
      }
      // Reschedule some timers without a Cancel first.
      for (size_t i = 0; i < kNumRescheduled; ++i) {
        InitRandomTimer(RemoveRandomTimer().get());
      }
    }

    void CreateRandomTimer()
    {
      nsresult rv;
      nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
      if (NS_FAILED(rv)) {
        MOZ_ASSERT(false, "Failed to create timer.");
        return;
      }

      rv = timer->SetTarget(static_cast<nsIEventTarget*>(mThread.get()));
      if (NS_FAILED(rv)) {
        MOZ_ASSERT(false, "Failed to set target.");
        return;
      }

      InitRandomTimer(timer.get());
    }

    nsCOMPtr<nsITimer> CancelRandomTimer()
    {
      nsCOMPtr<nsITimer> timer(RemoveRandomTimer());
      timer->Cancel();
      return timer;
    }

    nsCOMPtr<nsITimer> RemoveRandomTimer()
    {
      MOZ_ASSERT(mTimersOutstanding);

      if ((GetRandomType() == nsITimer::TYPE_ONE_SHOT && CountOneShotTimers())
          || mRepeatingTimers.empty()) {
        uint32_t delayToRemove = rand() % (FUZZ_MAX_TIMEOUT + 1);
        while (mOneShotTimersByDelay[delayToRemove].empty()) {
          // ++delayToRemove mod FUZZ_MAX_TIMEOUT + 1
          delayToRemove = (delayToRemove + 1) % (FUZZ_MAX_TIMEOUT + 1);
        }

        uint32_t indexToRemove =
          rand() % mOneShotTimersByDelay[delayToRemove].size();

        for (auto it = mOneShotTimersByDelay[delayToRemove].begin();
             it != mOneShotTimersByDelay[delayToRemove].end();
             ++it) {
          if (indexToRemove) {
            --indexToRemove;
            continue;
          }

          nsCOMPtr<nsITimer> removed = *it;
          mOneShotTimersByDelay[delayToRemove].erase(it);
          --mTimersOutstanding;
          return removed;
        }
      } else {
        size_t indexToRemove = rand() % mRepeatingTimers.size();
        nsCOMPtr<nsITimer> removed(mRepeatingTimers[indexToRemove]);
        mRepeatingTimers.erase(mRepeatingTimers.begin() + indexToRemove);
        --mTimersOutstanding;
        return removed;
      }

      MOZ_ASSERT_UNREACHABLE("Unable to remove a timer");
      return nullptr;
    }

    void InitRandomTimer(nsITimer* aTimer)
    {
      // Between 0 and FUZZ_MAX_TIMEOUT
      uint32_t delay = rand() % (FUZZ_MAX_TIMEOUT + 1);
      uint32_t type = GetRandomType();
      nsresult rv = aTimer->InitWithCallback(this, delay, type);
      if (NS_FAILED(rv)) {
        MOZ_ASSERT(false, "Failed to set timer.");
        return;
      }

      if (type == nsITimer::TYPE_ONE_SHOT) {
        mOneShotTimersByDelay[delay].push_back(aTimer);
      } else {
        mRepeatingTimers.push_back(aTimer);
      }
      ++mTimersOutstanding;
    }

    void CancelRepeatingTimer(nsITimer* aTimer)
    {
      for (auto it = mRepeatingTimers.begin();
           it != mRepeatingTimers.end();
           ++it) {
        if (it->get() == aTimer) {
          mRepeatingTimers.erase(it);
          aTimer->Cancel();
          --mTimersOutstanding;
          return;
        }
      }
    }

    nsCOMPtr<nsIThread> mThread;
    // Scheduled timers, indexed by delay between 0-9 ms, in lists
    // with most recently scheduled last.
    std::list<nsCOMPtr<nsITimer>> mOneShotTimersByDelay[FUZZ_MAX_TIMEOUT + 1];
    std::vector<nsCOMPtr<nsITimer>> mRepeatingTimers;
    Atomic<bool> mStopped;
    Atomic<size_t> mTimersOutstanding;
};

NS_IMPL_ISUPPORTS(FuzzTestThreadState, nsITimerCallback)

nsresult
FuzzTestTimers()
{
  static const size_t kNumThreads(10);
  AutoTestThread threads[kNumThreads];
  RefPtr<FuzzTestThreadState> threadStates[kNumThreads];

  for (size_t i = 0; i < kNumThreads; ++i) {
    threadStates[i] = new FuzzTestThreadState(&*threads[i]);
    threadStates[i]->Start();
  }

  PR_Sleep(PR_MillisecondsToInterval(20000));

  for (size_t i = 0; i < kNumThreads; ++i) {
    threadStates[i]->Stop();
  }

  // Wait at most 10 seconds for all outstanding timers to pop
  PRIntervalTime start = PR_IntervalNow();
  for (auto& threadState : threadStates) {
    while (threadState->HasTimersOutstanding()) {
      if (PR_IntervalToMilliseconds(PR_IntervalNow() - start) > 10000) {
        MOZ_ASSERT(false, "Timed out waiting for all timers to pop");
        return NS_ERROR_FAILURE;
      }
      PR_Sleep(PR_MillisecondsToInterval(10));
    }
  }

  return NS_OK;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("TestTimers");
  NS_ENSURE_FALSE(xpcom.failed(), 1);

  static TestFuncPtr testsToRun[] = {
    TestTargetedTimers,
    TestTimerWithStoppedTarget,
    FuzzTestTimers
  };
  static uint32_t testCount = sizeof(testsToRun) / sizeof(testsToRun[0]);

  for (uint32_t i = 0; i < testCount; i++) {
    nsresult rv = testsToRun[i]();
    NS_ENSURE_SUCCESS(rv, 1);
  }

  return 0;
}
