/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TaskQueue_h_
#define TaskQueue_h_

#include "mozilla/Monitor.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TaskDispatcher.h"
#include "mozilla/Unused.h"

#include <queue>

#include "nsThreadUtils.h"

class nsIEventTarget;
class nsIRunnable;

namespace mozilla {

typedef MozPromise<bool, bool, false> ShutdownPromise;

// Abstracts executing runnables in order on an arbitrary event target. The
// runnables dispatched to the TaskQueue will be executed in the order in which
// they're received, and are guaranteed to not be executed concurrently.
// They may be executed on different threads, and a memory barrier is used
// to make this threadsafe for objects that aren't already threadsafe.
//
// Note, since a TaskQueue can also be converted to an nsIEventTarget using
// WrapAsEventTarget() its possible to construct a hierarchy of TaskQueues.
// Consider these three TaskQueues:
//
//  TQ1 dispatches to the main thread
//  TQ2 dispatches to TQ1
//  TQ3 dispatches to TQ1
//
// This ensures there is only ever a single runnable from the entire chain on
// the main thread.  It also ensures that TQ2 and TQ3 only have a single runnable
// in TQ1 at any time.
//
// This arrangement lets you prioritize work by dispatching runnables directly
// to TQ1.  You can issue many runnables for important work.  Meanwhile the TQ2
// and TQ3 work will always execute at most one runnable and then yield.
class TaskQueue : public AbstractThread
{
  class EventTargetWrapper;

public:
  explicit TaskQueue(already_AddRefed<nsIEventTarget> aTarget,
                     bool aSupportsTailDispatch = false);

  TaskDispatcher& TailDispatcher() override;

  TaskQueue* AsTaskQueue() override { return this; }

  void Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                DispatchFailureHandling aFailureHandling = AssertDispatchSuccess,
                DispatchReason aReason = NormalDispatch) override
  {
    nsCOMPtr<nsIRunnable> r = aRunnable;
    {
      MonitorAutoLock mon(mQueueMonitor);
      nsresult rv = DispatchLocked(/* passed by ref */r, AbortIfFlushing, aFailureHandling, aReason);
      MOZ_DIAGNOSTIC_ASSERT(aFailureHandling == DontAssertDispatchSuccess || NS_SUCCEEDED(rv));
      Unused << rv;
    }
    // If the ownership of |r| is not transferred in DispatchLocked() due to
    // dispatch failure, it will be deleted here outside the lock. We do so
    // since the destructor of the runnable might access TaskQueue and result
    // in deadlocks.
  }

  // Puts the queue in a shutdown state and returns immediately. The queue will
  // remain alive at least until all the events are drained, because the Runners
  // hold a strong reference to the task queue, and one of them is always held
  // by the target event queue when the task queue is non-empty.
  //
  // The returned promise is resolved when the queue goes empty.
  RefPtr<ShutdownPromise> BeginShutdown();

  // Blocks until all task finish executing.
  void AwaitIdle();

  // Blocks until the queue is flagged for shutdown and all tasks have finished
  // executing.
  void AwaitShutdownAndIdle();

  bool IsEmpty();

  // Returns true if the current thread is currently running a Runnable in
  // the task queue.
  bool IsCurrentThreadIn() override;

  // Create a new nsIEventTarget wrapper object that dispatches to this
  // TaskQueue.
  already_AddRefed<nsIEventTarget> WrapAsEventTarget();

protected:
  virtual ~TaskQueue();


  // Blocks until all task finish executing. Called internally by methods
  // that need to wait until the task queue is idle.
  // mQueueMonitor must be held.
  void AwaitIdleLocked();

  enum DispatchMode { AbortIfFlushing, IgnoreFlushing };

  nsresult DispatchLocked(nsCOMPtr<nsIRunnable>& aRunnable,
                          DispatchMode aMode,
                          DispatchFailureHandling aFailureHandling,
                          DispatchReason aReason = NormalDispatch);

  void MaybeResolveShutdown()
  {
    mQueueMonitor.AssertCurrentThreadOwns();
    if (mIsShutdown && !mIsRunning) {
      mShutdownPromise.ResolveIfExists(true, __func__);
      mTarget = nullptr;
    }
  }

  nsCOMPtr<nsIEventTarget> mTarget;

  // Monitor that protects the queue and mIsRunning;
  Monitor mQueueMonitor;

  // Queue of tasks to run.
  std::queue<nsCOMPtr<nsIRunnable>> mTasks;

  // The thread currently running the task queue. We store a reference
  // to this so that IsCurrentThreadIn() can tell if the current thread
  // is the thread currently running in the task queue.
  //
  // This may be read on any thread, but may only be written on mRunningThread.
  // The thread can't die while we're running in it, and we only use it for
  // pointer-comparison with the current thread anyway - so we make it atomic
  // and don't refcount it.
  Atomic<nsIThread*> mRunningThread;

  // RAII class that gets instantiated for each dispatched task.
  class AutoTaskGuard : public AutoTaskDispatcher
  {
  public:
    explicit AutoTaskGuard(TaskQueue* aQueue)
      : AutoTaskDispatcher(/* aIsTailDispatcher = */ true), mQueue(aQueue)
      , mLastCurrentThread(nullptr)
    {
      // NB: We don't hold the lock to aQueue here. Don't do anything that
      // might require it.
      MOZ_ASSERT(!mQueue->mTailDispatcher);
      mQueue->mTailDispatcher = this;

      mLastCurrentThread = sCurrentThreadTLS.get();
      sCurrentThreadTLS.set(aQueue);

      MOZ_ASSERT(mQueue->mRunningThread == nullptr);
      mQueue->mRunningThread = NS_GetCurrentThread();
    }

    ~AutoTaskGuard()
    {
      DrainDirectTasks();

      MOZ_ASSERT(mQueue->mRunningThread == NS_GetCurrentThread());
      mQueue->mRunningThread = nullptr;

      sCurrentThreadTLS.set(mLastCurrentThread);
      mQueue->mTailDispatcher = nullptr;
    }

  private:
  TaskQueue* mQueue;
  AbstractThread* mLastCurrentThread;
  };

  TaskDispatcher* mTailDispatcher;

  // True if we've dispatched an event to the target to execute events from
  // the queue.
  bool mIsRunning;

  // True if we've started our shutdown process.
  bool mIsShutdown;
  MozPromiseHolder<ShutdownPromise> mShutdownPromise;

  // True if we're flushing; we reject new tasks if we're flushing.
  bool mIsFlushing;

  class Runner : public Runnable {
  public:
    explicit Runner(TaskQueue* aQueue)
      : mQueue(aQueue)
    {
    }
    NS_IMETHOD Run() override;
  private:
    RefPtr<TaskQueue> mQueue;
  };
};

} // namespace mozilla

#endif // TaskQueue_h_
