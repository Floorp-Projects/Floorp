/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TaskQueue_h_
#define TaskQueue_h_

#include <queue>

#include "mozilla/AbstractThread.h"
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TaskDispatcher.h"
#include "nsIDelayedRunnableObserver.h"
#include "nsIDirectTaskDispatcher.h"
#include "nsThreadUtils.h"

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
// the main thread.  It also ensures that TQ2 and TQ3 only have a single
// runnable in TQ1 at any time.
//
// This arrangement lets you prioritize work by dispatching runnables directly
// to TQ1.  You can issue many runnables for important work.  Meanwhile the TQ2
// and TQ3 work will always execute at most one runnable and then yield.
//
// A TaskQueue does not require explicit shutdown, however it provides a
// BeginShutdown() method that places TaskQueue in a shut down state and returns
// a promise that gets resolved once all pending tasks have completed
class TaskQueue : public AbstractThread,
                  public nsIDirectTaskDispatcher,
                  public nsIDelayedRunnableObserver {
  class EventTargetWrapper;

 public:
  explicit TaskQueue(already_AddRefed<nsIEventTarget> aTarget,
                     bool aSupportsTailDispatch = false);

  TaskQueue(already_AddRefed<nsIEventTarget> aTarget, const char* aName,
            bool aSupportsTailDispatch = false);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDIRECTTASKDISPATCHER

  TaskDispatcher& TailDispatcher() override;

  NS_IMETHOD Dispatch(already_AddRefed<nsIRunnable> aEvent,
                      uint32_t aFlags) override {
    nsCOMPtr<nsIRunnable> runnable = aEvent;
    {
      MonitorAutoLock mon(mQueueMonitor);
      return DispatchLocked(/* passed by ref */ runnable, aFlags,
                            NormalDispatch);
    }
    // If the ownership of |r| is not transferred in DispatchLocked() due to
    // dispatch failure, it will be deleted here outside the lock. We do so
    // since the destructor of the runnable might access TaskQueue and result
    // in deadlocks.
  }

  [[nodiscard]] nsresult Dispatch(
      already_AddRefed<nsIRunnable> aRunnable,
      DispatchReason aReason = NormalDispatch) override {
    nsCOMPtr<nsIRunnable> r = aRunnable;
    {
      MonitorAutoLock mon(mQueueMonitor);
      return DispatchLocked(/* passed by ref */ r, NS_DISPATCH_NORMAL, aReason);
    }
    // If the ownership of |r| is not transferred in DispatchLocked() due to
    // dispatch failure, it will be deleted here outside the lock. We do so
    // since the destructor of the runnable might access TaskQueue and result
    // in deadlocks.
  }

  // So we can access nsIEventTarget::Dispatch(nsIRunnable*, uint32_t aFlags)
  using nsIEventTarget::Dispatch;

  // nsIDelayedRunnableObserver
  void OnDelayedRunnableCreated(DelayedRunnable* aRunnable) override;
  void OnDelayedRunnableScheduled(DelayedRunnable* aRunnable) override;
  void OnDelayedRunnableRan(DelayedRunnable* aRunnable) override;

  using CancelPromise = MozPromise<bool, bool, false>;

  // Dispatches a task to cancel any pending DelayedRunnables. Idempotent. Only
  // dispatches the task on the first call. Creating DelayedRunnables after this
  // is called will result in assertion failures.
  RefPtr<CancelPromise> CancelDelayedRunnables();

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
  bool IsCurrentThreadIn() const override;
  using nsISerialEventTarget::IsOnCurrentThread;

 protected:
  virtual ~TaskQueue();

  // Blocks until all task finish executing. Called internally by methods
  // that need to wait until the task queue is idle.
  // mQueueMonitor must be held.
  void AwaitIdleLocked();

  nsresult DispatchLocked(nsCOMPtr<nsIRunnable>& aRunnable, uint32_t aFlags,
                          DispatchReason aReason = NormalDispatch);

  RefPtr<CancelPromise> CancelDelayedRunnablesLocked();

  // Cancels any scheduled DelayedRunnables on this TaskQueue.
  void CancelDelayedRunnablesImpl();

  void MaybeResolveShutdown() {
    mQueueMonitor.AssertCurrentThreadOwns();
    if (mIsShutdown && !mIsRunning) {
      mShutdownPromise.ResolveIfExists(true, __func__);
      mTarget = nullptr;
    }
  }

  nsCOMPtr<nsIEventTarget> mTarget;

  // Monitor that protects the queue, mIsRunning and
  // mDelayedRunnablesCancelPromise;
  Monitor mQueueMonitor;

  typedef struct TaskStruct {
    nsCOMPtr<nsIRunnable> event;
    uint32_t flags;
  } TaskStruct;

  // Queue of tasks to run.
  std::queue<TaskStruct> mTasks;

  // DelayedRunnables (from DelayedDispatch) that are managed by their
  // respective timers, but have not yet run. Accessed only on this
  // TaskQueue.
  nsTArray<RefPtr<DelayedRunnable>> mScheduledDelayedRunnables;

  // Manages resolving mDelayedRunnablesCancelPromise.
  MozPromiseHolder<CancelPromise> mDelayedRunnablesCancelHolder;

  // Set once the task to cancel all pending DelayedRunnables has been
  // dispatched. Guarded by mQueueMonitor.
  RefPtr<CancelPromise> mDelayedRunnablesCancelPromise;

  // The thread currently running the task queue. We store a reference
  // to this so that IsCurrentThreadIn() can tell if the current thread
  // is the thread currently running in the task queue.
  //
  // This may be read on any thread, but may only be written on mRunningThread.
  // The thread can't die while we're running in it, and we only use it for
  // pointer-comparison with the current thread anyway - so we make it atomic
  // and don't refcount it.
  Atomic<PRThread*> mRunningThread;

  // RAII class that gets instantiated for each dispatched task.
  class AutoTaskGuard {
   public:
    explicit AutoTaskGuard(TaskQueue* aQueue)
        : mQueue(aQueue), mLastCurrentThread(nullptr) {
      // NB: We don't hold the lock to aQueue here. Don't do anything that
      // might require it.
      MOZ_ASSERT(!mQueue->mTailDispatcher);
      mTaskDispatcher.emplace(aQueue,
                              /* aIsTailDispatcher = */ true);
      mQueue->mTailDispatcher = mTaskDispatcher.ptr();

      mLastCurrentThread = sCurrentThreadTLS.get();
      sCurrentThreadTLS.set(aQueue);

      MOZ_ASSERT(mQueue->mRunningThread == nullptr);
      mQueue->mRunningThread = PR_GetCurrentThread();
    }

    ~AutoTaskGuard() {
      mTaskDispatcher->DrainDirectTasks();
      mTaskDispatcher.reset();

      MOZ_ASSERT(mQueue->mRunningThread == PR_GetCurrentThread());
      mQueue->mRunningThread = nullptr;

      sCurrentThreadTLS.set(mLastCurrentThread);
      mQueue->mTailDispatcher = nullptr;
    }

   private:
    Maybe<AutoTaskDispatcher> mTaskDispatcher;
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

  // The name of this TaskQueue. Useful when debugging dispatch failures.
  const char* const mName;

  SimpleTaskQueue mDirectTasks;

  class Runner : public Runnable {
   public:
    explicit Runner(TaskQueue* aQueue)
        : Runnable("TaskQueue::Runner"), mQueue(aQueue) {}
    NS_IMETHOD Run() override;

   private:
    RefPtr<TaskQueue> mQueue;
  };
};

}  // namespace mozilla

#endif  // TaskQueue_h_
