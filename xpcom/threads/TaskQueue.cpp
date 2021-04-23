/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TaskQueue.h"

#include "mozilla/DelayedRunnable.h"
#include "nsThreadUtils.h"

namespace mozilla {

TaskQueue::TaskQueue(already_AddRefed<nsIEventTarget> aTarget,
                     const char* aName, bool aRequireTailDispatch)
    : AbstractThread(aRequireTailDispatch),
      mTarget(aTarget),
      mQueueMonitor("TaskQueue::Queue"),
      mTailDispatcher(nullptr),
      mIsRunning(false),
      mIsShutdown(false),
      mName(aName) {}

TaskQueue::TaskQueue(already_AddRefed<nsIEventTarget> aTarget,
                     bool aSupportsTailDispatch)
    : TaskQueue(std::move(aTarget), "Unnamed", aSupportsTailDispatch) {}

TaskQueue::~TaskQueue() {
  // No one is referencing this TaskQueue anymore, meaning no tasks can be
  // pending as all Runner hold a reference to this TaskQueue.
  MOZ_ASSERT(mScheduledDelayedRunnables.IsEmpty());
}

NS_IMPL_ISUPPORTS_INHERITED(TaskQueue, AbstractThread, nsIDirectTaskDispatcher,
                            nsIDelayedRunnableObserver);

TaskDispatcher& TaskQueue::TailDispatcher() {
  MOZ_ASSERT(IsCurrentThreadIn());
  MOZ_ASSERT(mTailDispatcher);
  return *mTailDispatcher;
}

// Note aRunnable is passed by ref to support conditional ownership transfer.
// See Dispatch() in TaskQueue.h for more details.
nsresult TaskQueue::DispatchLocked(nsCOMPtr<nsIRunnable>& aRunnable,
                                   uint32_t aFlags, DispatchReason aReason) {
  mQueueMonitor.AssertCurrentThreadOwns();
  if (mIsShutdown) {
    return NS_ERROR_FAILURE;
  }

  AbstractThread* currentThread;
  if (aReason != TailDispatch && (currentThread = GetCurrent()) &&
      RequiresTailDispatch(currentThread) &&
      currentThread->IsTailDispatcherAvailable()) {
    MOZ_ASSERT(aFlags == NS_DISPATCH_NORMAL,
               "Tail dispatch doesn't support flags");
    return currentThread->TailDispatcher().AddTask(this, aRunnable.forget());
  }

  LogRunnable::LogDispatch(aRunnable);
  mTasks.push({std::move(aRunnable), aFlags});

  if (mIsRunning) {
    return NS_OK;
  }
  RefPtr<nsIRunnable> runner(new Runner(this));
  nsresult rv = mTarget->Dispatch(runner.forget(), aFlags);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch runnable to run TaskQueue");
    return rv;
  }
  mIsRunning = true;

  return NS_OK;
}

void TaskQueue::AwaitIdle() {
  MonitorAutoLock mon(mQueueMonitor);
  AwaitIdleLocked();
}

void TaskQueue::AwaitIdleLocked() {
  // Make sure there are no tasks for this queue waiting in the caller's tail
  // dispatcher.
  MOZ_ASSERT_IF(AbstractThread::GetCurrent(),
                !AbstractThread::GetCurrent()->HasTailTasksFor(this));

  mQueueMonitor.AssertCurrentThreadOwns();
  MOZ_ASSERT(mIsRunning || mTasks.empty());
  while (mIsRunning) {
    mQueueMonitor.Wait();
  }
}

void TaskQueue::AwaitShutdownAndIdle() {
  MOZ_ASSERT(!IsCurrentThreadIn());
  // Make sure there are no tasks for this queue waiting in the caller's tail
  // dispatcher.
  MOZ_ASSERT_IF(AbstractThread::GetCurrent(),
                !AbstractThread::GetCurrent()->HasTailTasksFor(this));

  MonitorAutoLock mon(mQueueMonitor);
  while (!mIsShutdown) {
    mQueueMonitor.Wait();
  }
  AwaitIdleLocked();
}

void TaskQueue::OnDelayedRunnableCreated(DelayedRunnable* aRunnable) {
#ifdef DEBUG
  MonitorAutoLock mon(mQueueMonitor);
  MOZ_ASSERT(!mDelayedRunnablesCancelPromise);
#endif
}

void TaskQueue::OnDelayedRunnableScheduled(DelayedRunnable* aRunnable) {
  MOZ_ASSERT(IsOnCurrentThread());
  mScheduledDelayedRunnables.AppendElement(aRunnable);
}

void TaskQueue::OnDelayedRunnableRan(DelayedRunnable* aRunnable) {
  MOZ_ASSERT(IsOnCurrentThread());
  MOZ_ALWAYS_TRUE(mScheduledDelayedRunnables.RemoveElement(aRunnable));
}

auto TaskQueue::CancelDelayedRunnables() -> RefPtr<CancelPromise> {
  MonitorAutoLock mon(mQueueMonitor);
  return CancelDelayedRunnablesLocked();
}

auto TaskQueue::CancelDelayedRunnablesLocked() -> RefPtr<CancelPromise> {
  mQueueMonitor.AssertCurrentThreadOwns();
  if (mDelayedRunnablesCancelPromise) {
    return mDelayedRunnablesCancelPromise;
  }
  mDelayedRunnablesCancelPromise =
      mDelayedRunnablesCancelHolder.Ensure(__func__);
  nsCOMPtr<nsIRunnable> cancelRunnable =
      NewRunnableMethod("TaskQueue::CancelDelayedRunnablesImpl", this,
                        &TaskQueue::CancelDelayedRunnablesImpl);
  MOZ_ALWAYS_SUCCEEDS(DispatchLocked(/* passed by ref */ cancelRunnable,
                                     NS_DISPATCH_NORMAL, TailDispatch));
  return mDelayedRunnablesCancelPromise;
}

void TaskQueue::CancelDelayedRunnablesImpl() {
  MOZ_ASSERT(IsOnCurrentThread());
  for (const auto& runnable : mScheduledDelayedRunnables) {
    runnable->CancelTimer();
  }
  mScheduledDelayedRunnables.Clear();
  mDelayedRunnablesCancelHolder.Resolve(true, __func__);
}

RefPtr<ShutdownPromise> TaskQueue::BeginShutdown() {
  // Dispatch any tasks for this queue waiting in the caller's tail dispatcher,
  // since this is the last opportunity to do so.
  if (AbstractThread* currentThread = AbstractThread::GetCurrent()) {
    currentThread->TailDispatchTasksFor(this);
  }
  MonitorAutoLock mon(mQueueMonitor);
  Unused << CancelDelayedRunnablesLocked();
  mIsShutdown = true;
  RefPtr<ShutdownPromise> p = mShutdownPromise.Ensure(__func__);
  MaybeResolveShutdown();
  mon.NotifyAll();
  return p;
}

bool TaskQueue::IsEmpty() {
  MonitorAutoLock mon(mQueueMonitor);
  return mTasks.empty();
}

bool TaskQueue::IsCurrentThreadIn() const {
  bool in = mRunningThread == PR_GetCurrentThread();
  return in;
}

nsresult TaskQueue::Runner::Run() {
  TaskStruct event;
  {
    MonitorAutoLock mon(mQueue->mQueueMonitor);
    MOZ_ASSERT(mQueue->mIsRunning);
    if (mQueue->mTasks.empty()) {
      mQueue->mIsRunning = false;
      mQueue->MaybeResolveShutdown();
      mon.NotifyAll();
      return NS_OK;
    }
    event = std::move(mQueue->mTasks.front());
    mQueue->mTasks.pop();
  }
  MOZ_ASSERT(event.event);

  // Note that dropping the queue monitor before running the task, and
  // taking the monitor again after the task has run ensures we have memory
  // fences enforced. This means that if the object we're calling wasn't
  // designed to be threadsafe, it will be, provided we're only calling it
  // in this task queue.
  {
    AutoTaskGuard g(mQueue);
    SerialEventTargetGuard tg(mQueue);
    {
      LogRunnable::Run log(event.event);

      event.event->Run();

      // Drop the reference to event. The event will hold a reference to the
      // object it's calling, and we don't want to keep it alive, it may be
      // making assumptions what holds references to it. This is especially
      // the case if the object is waiting for us to shutdown, so that it
      // can shutdown (like in the MediaDecoderStateMachine's SHUTDOWN case).
      event.event = nullptr;
    }
  }

  {
    MonitorAutoLock mon(mQueue->mQueueMonitor);
    if (mQueue->mTasks.empty()) {
      // No more events to run. Exit the task runner.
      mQueue->mIsRunning = false;
      mQueue->MaybeResolveShutdown();
      mon.NotifyAll();
      return NS_OK;
    }
  }

  // There's at least one more event that we can run. Dispatch this Runner
  // to the target again to ensure it runs again. Note that we don't just
  // run in a loop here so that we don't hog the target. This means we may
  // run on another thread next time, but we rely on the memory fences from
  // mQueueMonitor for thread safety of non-threadsafe tasks.
  nsresult rv;
  {
    MonitorAutoLock mon(mQueue->mQueueMonitor);
    rv = mQueue->mTarget->Dispatch(
        this, mQueue->mTasks.front().flags | NS_DISPATCH_AT_END);
  }
  if (NS_FAILED(rv)) {
    // Failed to dispatch, shutdown!
    MonitorAutoLock mon(mQueue->mQueueMonitor);
    mQueue->mIsRunning = false;
    mQueue->mIsShutdown = true;
    mQueue->MaybeResolveShutdown();
    mon.NotifyAll();
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIDirectTaskDispatcher
//-----------------------------------------------------------------------------

NS_IMETHODIMP
TaskQueue::DispatchDirectTask(already_AddRefed<nsIRunnable> aEvent) {
  if (!IsCurrentThreadIn()) {
    return NS_ERROR_FAILURE;
  }
  mDirectTasks.AddTask(std::move(aEvent));
  return NS_OK;
}

NS_IMETHODIMP TaskQueue::DrainDirectTasks() {
  if (!IsCurrentThreadIn()) {
    return NS_ERROR_FAILURE;
  }
  mDirectTasks.DrainTasks();
  return NS_OK;
}

NS_IMETHODIMP TaskQueue::HaveDirectTasks(bool* aValue) {
  if (!IsCurrentThreadIn()) {
    return NS_ERROR_FAILURE;
  }

  *aValue = mDirectTasks.HaveTasks();
  return NS_OK;
}

}  // namespace mozilla
