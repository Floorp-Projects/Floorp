/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DelayedRunnable.h"
#include "mozilla/Atomics.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TaskQueue.h"

#include "gtest/gtest.h"
#include "mozilla/gtest/MozAssertions.h"
#include "MediaTimer.h"
#include "mozilla/media/MediaUtils.h"
#include "VideoUtils.h"

using mozilla::Atomic;
using mozilla::MakeRefPtr;
using mozilla::Monitor;
using mozilla::MonitorAutoLock;
using mozilla::TaskQueue;

namespace {
struct ReleaseDetector {
  explicit ReleaseDetector(Atomic<bool>* aActive) : mActive(aActive) {
    *mActive = true;
  }
  ReleaseDetector(ReleaseDetector&& aOther) noexcept : mActive(aOther.mActive) {
    aOther.mActive = nullptr;
  }
  ReleaseDetector(const ReleaseDetector&) = delete;
  ~ReleaseDetector() {
    if (mActive) {
      *mActive = false;
    }
  }
  Atomic<bool>* mActive;
};
}  // namespace

TEST(DelayedRunnable, TaskQueueShutdownLeak)
{
  Atomic<bool> active{false};
  auto taskQueue = TaskQueue::Create(
      GetMediaThreadPool(mozilla::MediaThreadType::SUPERVISOR),
      "TestDelayedRunnable TaskQueueShutdownLeak");
  taskQueue->DelayedDispatch(
      NS_NewRunnableFunction(__func__, [release = ReleaseDetector(&active)] {}),
      60e3 /* 1 minute */);
  EXPECT_TRUE(active);
  taskQueue->BeginShutdown();
  taskQueue->AwaitIdle();
  // Leaks are often detected after process shutdown. This doesn't wait that
  // long, but leaking past thread shutdown would be equally bad since the
  // runnable can no longer be released on the target thread. This is also the
  // reason why timers assert that they don't release the last reference to
  // their callbacks when dispatch fails (like when the target has been
  // shutdown).
  EXPECT_FALSE(active);
}

TEST(DelayedRunnable, nsThreadShutdownLeak)
{
  Atomic<bool> active{false};
  nsCOMPtr<nsIThread> thread;
  ASSERT_EQ(NS_NewNamedThread("Test Thread", getter_AddRefs(thread)), NS_OK);
  thread->DelayedDispatch(
      NS_NewRunnableFunction(__func__, [release = ReleaseDetector(&active)] {}),
      60e3 /* 1 minute */);
  EXPECT_TRUE(active);
  ASSERT_EQ(thread->Shutdown(), NS_OK);
  // Leaks are often detected after process shutdown. This doesn't wait that
  // long, but leaking past thread shutdown would be equally bad since the
  // runnable can no longer be released on the target thread. This is also the
  // reason why timers assert that they don't release the last reference to
  // their callbacks when dispatch fails (like when the target has been
  // shutdown).
  EXPECT_FALSE(active);
}

/*
 * This tests a case where we create a background TaskQueue that lives until
 * xpcom shutdown. This test will fail (by assertion failure) if the TaskQueue
 * shutdown task is dispatched too late in the shutdown sequence, or:
 * If the background thread pool is then empty, the TaskQueue shutdown task will
 * when dispatched require creating a new nsThread, which is forbidden too late
 * in the shutdown sequence.
 */
TEST(DelayedRunnable, BackgroundTaskQueueShutdownTask)
{
  nsCOMPtr<nsISerialEventTarget> taskQueue;
  nsresult rv = NS_CreateBackgroundTaskQueue("TestDelayedRunnable",
                                             getter_AddRefs(taskQueue));
  ASSERT_NS_SUCCEEDED(rv);

  // Leak the queue, so it gets cleaned up by xpcom-shutdown.
  nsISerialEventTarget* tq = taskQueue.forget().take();
  mozilla::Unused << tq;
}

/*
 * Like BackgroundTaskQueueShutdownTask but for nsThread, since both background
 * TaskQueues and nsThreads are managed by nsThreadManager. For nsThread things
 * are different and the shutdown task doesn't use Dispatch, but timings are
 * similar.
 */
TEST(DelayedRunnable, nsThreadShutdownTask)
{
  nsCOMPtr<nsIThread> thread;
  ASSERT_EQ(NS_NewNamedThread("Test Thread", getter_AddRefs(thread)), NS_OK);

  // Leak the thread, so it gets cleaned up by xpcom-shutdown.
  nsIThread* t = thread.forget().take();
  mozilla::Unused << t;
}

TEST(DelayedRunnable, TimerFiresBeforeRunnableRuns)
{
  RefPtr<mozilla::SharedThreadPool> pool =
      mozilla::SharedThreadPool::Get("Test Pool"_ns);
  auto tailTaskQueue1 =
      TaskQueue::Create(do_AddRef(pool), "TestDelayedRunnable tailTaskQueue1",
                        /* aSupportsTailDispatch = */ true);
  auto tailTaskQueue2 =
      TaskQueue::Create(do_AddRef(pool), "TestDelayedRunnable tailTaskQueue2",
                        /* aSupportsTailDispatch = */ true);
  auto noTailTaskQueue =
      TaskQueue::Create(do_AddRef(pool), "TestDelayedRunnable noTailTaskQueue",
                        /* aSupportsTailDispatch = */ false);
  enum class State : uint8_t {
    Start,
    TimerRan,
    TasksFinished,
  } state = State::Start;
  Monitor monitor MOZ_UNANNOTATED(__func__);
  MonitorAutoLock lock(monitor);
  MOZ_ALWAYS_SUCCEEDS(
      tailTaskQueue1->Dispatch(NS_NewRunnableFunction(__func__, [&] {
        // This will tail dispatch the delayed runnable, making it prone to
        // lose a race against the directly-initiated timer firing (and
        // dispatching another non-tail-dispatched runnable).
        EXPECT_TRUE(tailTaskQueue1->RequiresTailDispatch(tailTaskQueue2));
        tailTaskQueue2->DelayedDispatch(
            NS_NewRunnableFunction(__func__, [&] {}), 1);
        MonitorAutoLock lock(monitor);
        auto timer = MakeRefPtr<mozilla::MediaTimer>();
        timer->WaitFor(mozilla::TimeDuration::FromMilliseconds(1), __func__)
            ->Then(noTailTaskQueue, __func__, [&] {
              MonitorAutoLock lock(monitor);
              state = State::TimerRan;
              monitor.NotifyAll();
            });
        // Wait until the timer has run. It should have dispatched the
        // TimerEvent to tailTaskQueue2 by then. The tail dispatch happens when
        // we leave scope.
        while (state != State::TimerRan) {
          monitor.Wait();
        }
        // Notify main thread that we've finished the async steps.
        state = State::TasksFinished;
        monitor.Notify();
      })));
  // Wait for async steps before wrapping up the test case.
  while (state != State::TasksFinished) {
    monitor.Wait();
  }
}
