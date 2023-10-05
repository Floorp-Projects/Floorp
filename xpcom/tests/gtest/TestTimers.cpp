/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIThread.h"
#include "nsITimer.h"

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
#include "prinrval.h"
#include "prmon.h"
#include "prthread.h"
#include "mozilla/Attributes.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/Services.h"

#include "mozilla/Monitor.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticPrefs_timer.h"

#include <list>
#include <vector>

#include "gtest/gtest.h"

using namespace mozilla;

typedef nsresult (*TestFuncPtr)();

class AutoTestThread {
 public:
  AutoTestThread() {
    nsCOMPtr<nsIThread> newThread;
    nsresult rv =
        NS_NewNamedThread("AutoTestThread", getter_AddRefs(newThread));
    if (NS_FAILED(rv)) return;

    newThread.swap(mThread);
  }

  ~AutoTestThread() { mThread->Shutdown(); }

  operator nsIThread*() const { return mThread; }

  nsIThread* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN {
    return mThread;
  }

 private:
  nsCOMPtr<nsIThread> mThread;
};

class AutoCreateAndDestroyReentrantMonitor {
 public:
  AutoCreateAndDestroyReentrantMonitor() {
    mReentrantMonitor = new ReentrantMonitor("TestTimers::AutoMon");
    MOZ_RELEASE_ASSERT(mReentrantMonitor, "Out of memory!");
  }

  ~AutoCreateAndDestroyReentrantMonitor() { delete mReentrantMonitor; }

  operator ReentrantMonitor*() const { return mReentrantMonitor; }

 private:
  ReentrantMonitor* mReentrantMonitor;
};

class TimerCallback final : public nsITimerCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  TimerCallback(nsIThread** aThreadPtr, ReentrantMonitor* aReentrantMonitor)
      : mThreadPtr(aThreadPtr), mReentrantMonitor(aReentrantMonitor) {}

  NS_IMETHOD Notify(nsITimer* aTimer) override {
    MOZ_RELEASE_ASSERT(mThreadPtr, "Callback was not supposed to be called!");
    nsCOMPtr<nsIThread> current(do_GetCurrentThread());

    ReentrantMonitorAutoEnter mon(*mReentrantMonitor);

    MOZ_RELEASE_ASSERT(!*mThreadPtr, "Timer called back more than once!");
    *mThreadPtr = current;

    mon.Notify();

    return NS_OK;
  }

 private:
  ~TimerCallback() = default;

  nsIThread** mThreadPtr;
  ReentrantMonitor* mReentrantMonitor;
};

NS_IMPL_ISUPPORTS(TimerCallback, nsITimerCallback)

class TimerHelper {
 public:
  explicit TimerHelper(nsIEventTarget* aTarget)
      : mStart(TimeStamp::Now()),
        mTimer(NS_NewTimer(aTarget)),
        mMonitor(__func__),
        mTarget(aTarget) {}

  ~TimerHelper() { Cancel(); }

  static void ClosureCallback(nsITimer*, void* aClosure) {
    reinterpret_cast<TimerHelper*>(aClosure)->Notify();
  }

  // We do not use nsITimerCallback, because that results in a circular
  // reference. One of the properties we want from TimerHelper is for the
  // timer to be canceled when it goes out of scope.
  void Notify() {
    MonitorAutoLock lock(mMonitor);
    EXPECT_TRUE(mTarget->IsOnCurrentThread());
    TimeDuration elapsed = TimeStamp::Now() - mStart;
    mStart = TimeStamp::Now();
    mLastDelay = Some(elapsed.ToMilliseconds());
    if (mBlockTime) {
      PR_Sleep(mBlockTime);
    }
    mMonitor.Notify();
  }

  nsresult SetTimer(uint32_t aDelay, uint8_t aType) {
    Cancel();
    MonitorAutoLock lock(mMonitor);
    mStart = TimeStamp::Now();
    return mTimer->InitWithNamedFuncCallback(
        ClosureCallback, this, aDelay, aType, "TimerHelper::ClosureCallback");
  }

  Maybe<uint32_t> Wait(uint32_t aLimitMs) {
    return WaitAndBlockCallback(aLimitMs, 0);
  }

  // Waits for callback, and if it occurs within the limit, causes the callback
  // to block for the specified time. Useful for testing cases where the
  // callback takes a long time to return.
  Maybe<uint32_t> WaitAndBlockCallback(uint32_t aLimitMs, uint32_t aBlockTime) {
    MonitorAutoLock lock(mMonitor);
    mBlockTime = aBlockTime;
    TimeStamp start = TimeStamp::Now();
    while (!mLastDelay.isSome()) {
      mMonitor.Wait(TimeDuration::FromMilliseconds(aLimitMs));
      TimeDuration elapsed = TimeStamp::Now() - start;
      uint32_t elapsedMs = static_cast<uint32_t>(elapsed.ToMilliseconds());
      if (elapsedMs >= aLimitMs) {
        break;
      }
      aLimitMs -= elapsedMs;
      start = TimeStamp::Now();
    }
    mBlockTime = 0;
    return std::move(mLastDelay);
  }

  void Cancel() {
    NS_DispatchAndSpinEventLoopUntilComplete(
        "~TimerHelper timer cancel"_ns, mTarget,
        NS_NewRunnableFunction("~TimerHelper timer cancel", [this] {
          MonitorAutoLock lock(mMonitor);
          mTimer->Cancel();
        }));
  }

 private:
  TimeStamp mStart;
  RefPtr<nsITimer> mTimer;
  mutable Monitor mMonitor MOZ_UNANNOTATED;
  uint32_t mBlockTime = 0;
  Maybe<uint32_t> mLastDelay;
  RefPtr<nsIEventTarget> mTarget;
};

class SimpleTimerTest : public ::testing::Test {
 public:
  std::unique_ptr<TimerHelper> MakeTimer(uint32_t aDelay, uint8_t aType) {
    std::unique_ptr<TimerHelper> timer(new TimerHelper(mThread));
    timer->SetTimer(aDelay, aType);
    return timer;
  }

  void PauseTimerThread() {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    observerService->NotifyObservers(nullptr, "sleep_notification", nullptr);
  }

  void ResumeTimerThread() {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    observerService->NotifyObservers(nullptr, "wake_notification", nullptr);
  }

 protected:
  AutoTestThread mThread;
};

#ifdef XP_MACOSX
// For some reason, our OS X testers fire timed condition waits _extremely_
// late (as much as 200ms).
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1726915
const unsigned kSlowdownFactor = 50;
#elif XP_WIN
// Windows also needs some extra leniency, but not nearly as much as our OS X
// testers
// See https://bugzilla.mozilla.org/show_bug.cgi?id=1729035
const unsigned kSlowdownFactor = 5;
#else
const unsigned kSlowdownFactor = 1;
#endif

TEST_F(SimpleTimerTest, OneShot) {
  auto timer = MakeTimer(100 * kSlowdownFactor, nsITimer::TYPE_ONE_SHOT);
  auto res = timer->Wait(110 * kSlowdownFactor);
  ASSERT_TRUE(res.isSome());
  ASSERT_LT(*res, 110U * kSlowdownFactor);
  ASSERT_GT(*res, 95U * kSlowdownFactor);
}

TEST_F(SimpleTimerTest, TimerWithStoppedTarget) {
  mThread->Shutdown();
  auto timer = MakeTimer(100 * kSlowdownFactor, nsITimer::TYPE_ONE_SHOT);
  auto res = timer->Wait(110 * kSlowdownFactor);
  ASSERT_FALSE(res.isSome());
}

TEST_F(SimpleTimerTest, SlackRepeating) {
  auto timer = MakeTimer(100 * kSlowdownFactor, nsITimer::TYPE_REPEATING_SLACK);
  auto delay =
      timer->WaitAndBlockCallback(110 * kSlowdownFactor, 50 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 110U * kSlowdownFactor);
  ASSERT_GT(*delay, 95U * kSlowdownFactor);
  // REPEATING_SLACK timers re-schedule with the full duration when the timer
  // callback completes

  delay = timer->Wait(110 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 160U * kSlowdownFactor);
  ASSERT_GT(*delay, 145U * kSlowdownFactor);
}

TEST_F(SimpleTimerTest, RepeatingPrecise) {
  auto timer = MakeTimer(100 * kSlowdownFactor,
                         nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP);
  auto delay =
      timer->WaitAndBlockCallback(110 * kSlowdownFactor, 50 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 110U * kSlowdownFactor);
  ASSERT_GT(*delay, 95U * kSlowdownFactor);

  // Delays smaller than the timer's period do not effect the period.
  delay = timer->Wait(110 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 110U * kSlowdownFactor);
  ASSERT_GT(*delay, 95U * kSlowdownFactor);

  // Delays larger than the timer's period should result in the skipping of
  // firings, but the cadence should remain the same.
  delay =
      timer->WaitAndBlockCallback(110 * kSlowdownFactor, 150 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 110U * kSlowdownFactor);
  ASSERT_GT(*delay, 95U * kSlowdownFactor);

  delay = timer->Wait(110 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 210U * kSlowdownFactor);
  ASSERT_GT(*delay, 195U * kSlowdownFactor);
}

// gtest on 32bit Win7 debug build is unstable and somehow this test
// makes it even worse.
#if !defined(XP_WIN) || !defined(DEBUG) || defined(HAVE_64BIT_BUILD)

class FindExpirationTimeState final {
 public:
  // We'll offset the timers 10 seconds into the future to assure that they
  // won't fire
  const uint32_t kTimerOffset = 10 * 1000;
  // And we'll set the timers spaced by 5 seconds.
  const uint32_t kTimerInterval = 5 * 1000;
  // We'll use 20 timers
  const uint32_t kNumTimers = 20;

  TimeStamp mBefore;
  TimeStamp mMiddle;

  std::list<nsCOMPtr<nsITimer>> mTimers;

  ~FindExpirationTimeState() {
    while (!mTimers.empty()) {
      nsCOMPtr<nsITimer> t = mTimers.front().get();
      mTimers.pop_front();
      t->Cancel();
    }
  }

  // Create timers, with aNumLowPriority low priority timers first in the queue
  void InitTimers(uint32_t aNumLowPriority, uint32_t aType) {
    // aType is just for readability.
    MOZ_ASSERT(aType == nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY);
    InitTimers(aNumLowPriority, nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY, nullptr);
  }

  // Create timers, with aNumDifferentTarget timers with target aTarget first in
  // the queue
  void InitTimers(uint32_t aNumDifferentTarget, nsIEventTarget* aTarget) {
    InitTimers(aNumDifferentTarget, nsITimer::TYPE_ONE_SHOT, aTarget);
  }

  void InitTimers(uint32_t aNumDifferingTimers, uint32_t aType,
                  nsIEventTarget* aTarget) {
    do {
      TimeStamp clearUntil =
          TimeStamp::Now() + TimeDuration::FromMilliseconds(
                                 kTimerOffset + kNumTimers * kTimerInterval);

      // NS_GetTimerDeadlineHintOnCurrentThread returns clearUntil if there are
      // no pending timers before clearUntil.
      // Passing 0 ensures that we examine the next timer to fire, regardless
      // of its thread target. This is important, because lots of the checks
      // we perform in the test get confused by timers targeted at other
      // threads.
      TimeStamp t = NS_GetTimerDeadlineHintOnCurrentThread(clearUntil, 0);
      if (t >= clearUntil) {
        break;
      }

      // Clear whatever random timers there might be pending.
      uint32_t waitTime = 10;
      if (t > TimeStamp::Now()) {
        waitTime = uint32_t((t - TimeStamp::Now()).ToMilliseconds());
      }
      PR_Sleep(PR_MillisecondsToInterval(waitTime));
    } while (true);

    mBefore = TimeStamp::Now();
    // To avoid getting exactly the same time for a timer and mMiddle, subtract
    // 50 ms.
    mMiddle = mBefore +
              TimeDuration::FromMilliseconds(kTimerOffset +
                                             kTimerInterval * kNumTimers / 2) -
              TimeDuration::FromMilliseconds(50);
    for (uint32_t i = 0; i < kNumTimers; ++i) {
      nsCOMPtr<nsITimer> timer = NS_NewTimer();
      ASSERT_TRUE(timer);

      if (i < aNumDifferingTimers) {
        if (aTarget) {
          timer->SetTarget(aTarget);
        }

        timer->InitWithNamedFuncCallback(
            &UnusedCallbackFunc, nullptr, kTimerOffset + kTimerInterval * i,
            aType, "FindExpirationTimeState::InitTimers");
      } else {
        timer->InitWithNamedFuncCallback(
            &UnusedCallbackFunc, nullptr, kTimerOffset + kTimerInterval * i,
            nsITimer::TYPE_ONE_SHOT, "FindExpirationTimeState::InitTimers");
      }
      mTimers.push_front(timer.get());
    }
  }

  static void UnusedCallbackFunc(nsITimer* aTimer, void* aClosure) {
    FAIL() << "Timer shouldn't fire.";
  }
};

TEST_F(SimpleTimerTest, FindExpirationTime) {
  {
    FindExpirationTimeState state;
    // 0 low priority timers
    state.InitTimers(0, nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY);
    TimeStamp before = state.mBefore;
    TimeStamp middle = state.mMiddle;

    TimeStamp t;
    t = NS_GetTimerDeadlineHintOnCurrentThread(before, 0);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, before) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(before, 20);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, before) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 0);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_LT(t, middle) << "Found time should be less than default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 10);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_LT(t, middle) << "Found time should be less than default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 20);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_LT(t, middle) << "Found time should be less than default";
  }

  {
    FindExpirationTimeState state;
    // 5 low priority timers
    state.InitTimers(5, nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY);
    TimeStamp before = state.mBefore;
    TimeStamp middle = state.mMiddle;

    TimeStamp t;
    t = NS_GetTimerDeadlineHintOnCurrentThread(before, 0);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, before) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(before, 20);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, before) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 0);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_LT(t, middle) << "Found time should be less than default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 10);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_LT(t, middle) << "Found time should be less than default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 20);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_LT(t, middle) << "Found time should be less than default";
  }

  {
    FindExpirationTimeState state;
    // 15 low priority timers
    state.InitTimers(15, nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY);
    TimeStamp before = state.mBefore;
    TimeStamp middle = state.mMiddle;

    TimeStamp t;
    t = NS_GetTimerDeadlineHintOnCurrentThread(before, 0);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, before) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(before, 20);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, before) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 0);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_LT(t, middle) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 10);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, middle) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 20);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, middle) << "Found time should be equal to default";
  }

  {
    AutoTestThread testThread;
    FindExpirationTimeState state;
    // 5 other targets
    state.InitTimers(5, static_cast<nsIEventTarget*>(testThread));
    TimeStamp before = state.mBefore;
    TimeStamp middle = state.mMiddle;

    TimeStamp t;
    t = NS_GetTimerDeadlineHintOnCurrentThread(before, 0);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, before) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(before, 20);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, before) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 0);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_LT(t, middle) << "Found time should be less than default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 10);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_LT(t, middle) << "Found time should be less than default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 20);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_LT(t, middle) << "Found time should be less than default";
  }

  {
    AutoTestThread testThread;
    FindExpirationTimeState state;
    // 15 other targets
    state.InitTimers(15, static_cast<nsIEventTarget*>(testThread));
    TimeStamp before = state.mBefore;
    TimeStamp middle = state.mMiddle;

    TimeStamp t;
    t = NS_GetTimerDeadlineHintOnCurrentThread(before, 0);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, before) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(before, 20);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, before) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 0);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_LT(t, middle) << "Found time should be less than default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 10);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, middle) << "Found time should be equal to default";

    t = NS_GetTimerDeadlineHintOnCurrentThread(middle, 20);
    EXPECT_TRUE(t) << "We should find a time";
    EXPECT_EQ(t, middle) << "Found time should be equal to default";
  }
}

#endif

// Do these _after_ FindExpirationTime; apparently pausing the timer thread
// schedules minute-long timers, which FindExpirationTime waits out before
// starting.
TEST_F(SimpleTimerTest, SleepWakeOneShot) {
  if (StaticPrefs::timer_ignore_sleep_wake_notifications()) {
    return;
  }
  auto timer = MakeTimer(100 * kSlowdownFactor, nsITimer::TYPE_ONE_SHOT);
  PauseTimerThread();
  auto delay = timer->Wait(200 * kSlowdownFactor);
  ResumeTimerThread();
  ASSERT_FALSE(delay.isSome());
}

TEST_F(SimpleTimerTest, SleepWakeRepeatingSlack) {
  if (StaticPrefs::timer_ignore_sleep_wake_notifications()) {
    return;
  }
  auto timer = MakeTimer(100 * kSlowdownFactor, nsITimer::TYPE_REPEATING_SLACK);
  PauseTimerThread();
  auto delay = timer->Wait(200 * kSlowdownFactor);
  ResumeTimerThread();
  ASSERT_FALSE(delay.isSome());

  // Timer thread slept for ~200ms, longer than the duration of the timer, so
  // it should fire pretty much immediately.
  delay = timer->Wait(10 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 210 * kSlowdownFactor);
  ASSERT_GT(*delay, 199 * kSlowdownFactor);

  delay = timer->Wait(110 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 110U * kSlowdownFactor);
  ASSERT_GT(*delay, 95U * kSlowdownFactor);

  PauseTimerThread();
  delay = timer->Wait(50 * kSlowdownFactor);
  ResumeTimerThread();
  ASSERT_FALSE(delay.isSome());

  // Timer thread only slept for ~50 ms, shorter than the duration of the
  // timer, so there should be no effect on the timing.
  delay = timer->Wait(110 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 110U * kSlowdownFactor);
  ASSERT_GT(*delay, 95U * kSlowdownFactor);
}

TEST_F(SimpleTimerTest, SleepWakeRepeatingPrecise) {
  if (StaticPrefs::timer_ignore_sleep_wake_notifications()) {
    return;
  }
  auto timer = MakeTimer(100 * kSlowdownFactor,
                         nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP);
  PauseTimerThread();
  auto delay = timer->Wait(350 * kSlowdownFactor);
  ResumeTimerThread();
  ASSERT_FALSE(delay.isSome());

  // Timer thread slept longer than the duration of the timer, so it should
  // fire pretty much immediately.
  delay = timer->Wait(10 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 360U * kSlowdownFactor);
  ASSERT_GT(*delay, 349U * kSlowdownFactor);

  // After that, we should get back on our original cadence
  delay = timer->Wait(110 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 60U * kSlowdownFactor);
  ASSERT_GT(*delay, 45U * kSlowdownFactor);

  delay = timer->Wait(110 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 110U * kSlowdownFactor);
  ASSERT_GT(*delay, 95U * kSlowdownFactor);

  PauseTimerThread();
  delay = timer->Wait(50 * kSlowdownFactor);
  ResumeTimerThread();
  ASSERT_FALSE(delay.isSome());

  // Timer thread only slept for ~50 ms, shorter than the duration of the
  // timer, so there should be no effect on the timing.
  delay = timer->Wait(110 * kSlowdownFactor);
  ASSERT_TRUE(delay.isSome());
  ASSERT_LT(*delay, 110U * kSlowdownFactor);
  ASSERT_GT(*delay, 95U * kSlowdownFactor);
}

#define FUZZ_MAX_TIMEOUT 9
class FuzzTestThreadState final : public nsITimerCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit FuzzTestThreadState(nsIThread* thread)
      : mThread(thread), mStopped(false) {}

  class StartRunnable final : public mozilla::Runnable {
   public:
    explicit StartRunnable(FuzzTestThreadState* threadState)
        : mozilla::Runnable("FuzzTestThreadState::StartRunnable"),
          mThreadState(threadState) {}

    NS_IMETHOD Run() override {
      mThreadState->ScheduleOrCancelTimers();
      return NS_OK;
    }

   private:
    RefPtr<FuzzTestThreadState> mThreadState;
  };

  void Start() {
    nsCOMPtr<nsIRunnable> runnable = new StartRunnable(this);
    nsresult rv = mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv), "Failed to dispatch StartRunnable.");
  }

  void Stop() { mStopped = true; }

  NS_IMETHOD Notify(nsITimer* aTimer) override {
    bool onCorrectThread;
    nsresult rv = mThread->IsOnCurrentThread(&onCorrectThread);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv), "Failed to perform thread check.");
    MOZ_RELEASE_ASSERT(onCorrectThread, "Notify invoked on wrong thread.");

    uint32_t delay;
    rv = aTimer->GetDelay(&delay);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv), "GetDelay failed.");

    MOZ_RELEASE_ASSERT(delay <= FUZZ_MAX_TIMEOUT,
                       "Delay was an invalid value for this test.");

    uint32_t type;
    rv = aTimer->GetType(&type);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv), "Failed to get timer type.");
    MOZ_RELEASE_ASSERT(type <= nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP);

    if (type == nsITimer::TYPE_ONE_SHOT) {
      MOZ_RELEASE_ASSERT(!mOneShotTimersByDelay[delay].empty(),
                         "Unexpected one-shot timer.");

      MOZ_RELEASE_ASSERT(mOneShotTimersByDelay[delay].front().get() == aTimer,
                         "One-shot timers have been reordered.");

      mOneShotTimersByDelay[delay].pop_front();
      --mTimersOutstanding;
    } else if (mStopped) {
      CancelRepeatingTimer(aTimer);
    }

    ScheduleOrCancelTimers();
    RescheduleSomeTimers();
    return NS_OK;
  }

  bool HasTimersOutstanding() const { return !!mTimersOutstanding; }

 private:
  ~FuzzTestThreadState() {
    for (size_t i = 0; i <= FUZZ_MAX_TIMEOUT; ++i) {
      MOZ_RELEASE_ASSERT(mOneShotTimersByDelay[i].empty(),
                         "Timers remain at end of test.");
    }
  }

  uint32_t GetRandomType() const {
    return rand() % (nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP + 1);
  }

  size_t CountOneShotTimers() const {
    size_t count = 0;
    for (size_t i = 0; i <= FUZZ_MAX_TIMEOUT; ++i) {
      count += mOneShotTimersByDelay[i].size();
    }
    return count;
  }

  void ScheduleOrCancelTimers() {
    if (mStopped) {
      return;
    }

    const size_t numTimersDesired = (rand() % 100) + 100;
    MOZ_RELEASE_ASSERT(numTimersDesired >= 100);
    MOZ_RELEASE_ASSERT(numTimersDesired < 200);
    int adjustment = numTimersDesired - mTimersOutstanding;

    while (adjustment > 0) {
      CreateRandomTimer();
      --adjustment;
    }

    while (adjustment < 0) {
      CancelRandomTimer();
      ++adjustment;
    }

    MOZ_RELEASE_ASSERT(numTimersDesired == mTimersOutstanding);
  }

  void RescheduleSomeTimers() {
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

  void CreateRandomTimer() {
    nsCOMPtr<nsITimer> timer =
        NS_NewTimer(static_cast<nsIEventTarget*>(mThread.get()));
    MOZ_RELEASE_ASSERT(timer, "Failed to create timer.");

    InitRandomTimer(timer.get());
  }

  nsCOMPtr<nsITimer> CancelRandomTimer() {
    nsCOMPtr<nsITimer> timer(RemoveRandomTimer());
    timer->Cancel();
    return timer;
  }

  nsCOMPtr<nsITimer> RemoveRandomTimer() {
    MOZ_RELEASE_ASSERT(mTimersOutstanding);

    if ((GetRandomType() == nsITimer::TYPE_ONE_SHOT && CountOneShotTimers()) ||
        mRepeatingTimers.empty()) {
      uint32_t delayToRemove = rand() % (FUZZ_MAX_TIMEOUT + 1);
      while (mOneShotTimersByDelay[delayToRemove].empty()) {
        // ++delayToRemove mod FUZZ_MAX_TIMEOUT + 1
        delayToRemove = (delayToRemove + 1) % (FUZZ_MAX_TIMEOUT + 1);
      }

      uint32_t indexToRemove =
          rand() % mOneShotTimersByDelay[delayToRemove].size();

      for (auto it = mOneShotTimersByDelay[delayToRemove].begin();
           it != mOneShotTimersByDelay[delayToRemove].end(); ++it) {
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

    MOZ_CRASH("Unable to remove a timer");
  }

  void InitRandomTimer(nsITimer* aTimer) {
    // Between 0 and FUZZ_MAX_TIMEOUT
    uint32_t delay = rand() % (FUZZ_MAX_TIMEOUT + 1);
    uint32_t type = GetRandomType();
    nsresult rv = aTimer->InitWithCallback(this, delay, type);
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv), "Failed to set timer.");

    if (type == nsITimer::TYPE_ONE_SHOT) {
      mOneShotTimersByDelay[delay].push_back(aTimer);
    } else {
      mRepeatingTimers.push_back(aTimer);
    }
    ++mTimersOutstanding;
  }

  void CancelRepeatingTimer(nsITimer* aTimer) {
    for (auto it = mRepeatingTimers.begin(); it != mRepeatingTimers.end();
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

TEST(Timers, FuzzTestTimers)
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
      uint32_t elapsedMs = PR_IntervalToMilliseconds(PR_IntervalNow() - start);
      ASSERT_LE(elapsedMs, uint32_t(10000))
          << "Timed out waiting for all timers to pop";
      PR_Sleep(PR_MillisecondsToInterval(10));
    }
  }
}

TEST(Timers, ClosureCallback)
{
  AutoCreateAndDestroyReentrantMonitor newMon;
  ASSERT_TRUE(newMon);

  AutoTestThread testThread;
  ASSERT_TRUE(testThread);

  nsIThread* notifiedThread = nullptr;

  nsCOMPtr<nsITimer> timer;
  nsresult rv = NS_NewTimerWithCallback(
      getter_AddRefs(timer),
      [&](nsITimer*) {
        nsCOMPtr<nsIThread> current(do_GetCurrentThread());

        ReentrantMonitorAutoEnter mon(*newMon);
        ASSERT_FALSE(notifiedThread);
        notifiedThread = current;
        mon.Notify();
      },
      50, nsITimer::TYPE_ONE_SHOT, "(test) Timers.ClosureCallback", testThread);
  ASSERT_NS_SUCCEEDED(rv);

  ReentrantMonitorAutoEnter mon(*newMon);
  while (!notifiedThread) {
    mon.Wait();
  }
  ASSERT_EQ(notifiedThread, testThread);
}

static void SetTime(nsITimer* aTimer, void* aClosure) {
  *static_cast<TimeStamp*>(aClosure) = TimeStamp::Now();
}

TEST(Timers, HighResFuncCallback)
{
  TimeStamp first;
  TimeStamp second;
  TimeStamp third;
  nsCOMPtr<nsITimer> t1 = NS_NewTimer(GetCurrentSerialEventTarget());
  nsCOMPtr<nsITimer> t2 = NS_NewTimer(GetCurrentSerialEventTarget());
  nsCOMPtr<nsITimer> t3 = NS_NewTimer(GetCurrentSerialEventTarget());

  // Reverse order, since if the timers are not high-res we'd end up
  // out-of-order.
  MOZ_ALWAYS_SUCCEEDS(t3->InitHighResolutionWithNamedFuncCallback(
      &SetTime, &third, TimeDuration::FromMicroseconds(300),
      nsITimer::TYPE_ONE_SHOT, "TestTimers::HighResFuncCallback::third"));
  MOZ_ALWAYS_SUCCEEDS(t2->InitHighResolutionWithNamedFuncCallback(
      &SetTime, &second, TimeDuration::FromMicroseconds(200),
      nsITimer::TYPE_ONE_SHOT, "TestTimers::HighResFuncCallback::second"));
  MOZ_ALWAYS_SUCCEEDS(t1->InitHighResolutionWithNamedFuncCallback(
      &SetTime, &first, TimeDuration::FromMicroseconds(100),
      nsITimer::TYPE_ONE_SHOT, "TestTimers::HighResFuncCallback::first"));

  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      "TestTimers::HighResFuncCallback"_ns,
      [&] { return !first.IsNull() && !second.IsNull() && !third.IsNull(); });
}
