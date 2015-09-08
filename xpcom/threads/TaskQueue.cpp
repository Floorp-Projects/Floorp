/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TaskQueue.h"

#include "nsThreadUtils.h"
#include "mozilla/SharedThreadPool.h"

namespace mozilla {

TaskQueue::TaskQueue(already_AddRefed<SharedThreadPool> aPool,
                               bool aRequireTailDispatch)
  : AbstractThread(aRequireTailDispatch)
  , mPool(aPool)
  , mQueueMonitor("TaskQueue::Queue")
  , mTailDispatcher(nullptr)
  , mIsRunning(false)
  , mIsShutdown(false)
  , mIsFlushing(false)
{
  MOZ_COUNT_CTOR(TaskQueue);
}

TaskQueue::~TaskQueue()
{
  MonitorAutoLock mon(mQueueMonitor);
  MOZ_ASSERT(mIsShutdown);
  MOZ_COUNT_DTOR(TaskQueue);
}

TaskDispatcher&
TaskQueue::TailDispatcher()
{
  MOZ_ASSERT(IsCurrentThreadIn());
  MOZ_ASSERT(mTailDispatcher);
  return *mTailDispatcher;
}

nsresult
TaskQueue::DispatchLocked(already_AddRefed<nsIRunnable> aRunnable,
                               DispatchMode aMode, DispatchFailureHandling aFailureHandling,
                               DispatchReason aReason)
{
  nsCOMPtr<nsIRunnable> r = aRunnable;
  AbstractThread* currentThread;
  if (aReason != TailDispatch && (currentThread = GetCurrent()) && RequiresTailDispatch(currentThread)) {
    currentThread->TailDispatcher().AddTask(this, r.forget(), aFailureHandling);
    return NS_OK;
  }

  mQueueMonitor.AssertCurrentThreadOwns();
  if (mIsFlushing && aMode == AbortIfFlushing) {
    return NS_ERROR_ABORT;
  }
  if (mIsShutdown) {
    return NS_ERROR_FAILURE;
  }
  mTasks.push(r.forget());
  if (mIsRunning) {
    return NS_OK;
  }
  nsRefPtr<nsIRunnable> runner(new Runner(this));
  nsresult rv = mPool->Dispatch(runner.forget(), NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch runnable to run TaskQueue");
    return rv;
  }
  mIsRunning = true;

  return NS_OK;
}

void
TaskQueue::AwaitIdle()
{
  MonitorAutoLock mon(mQueueMonitor);
  AwaitIdleLocked();
}

void
TaskQueue::AwaitIdleLocked()
{
  // Make sure there are no tasks for this queue waiting in the caller's tail
  // dispatcher.
  MOZ_ASSERT_IF(AbstractThread::GetCurrent(),
                !AbstractThread::GetCurrent()->TailDispatcher().HasTasksFor(this));

  mQueueMonitor.AssertCurrentThreadOwns();
  MOZ_ASSERT(mIsRunning || mTasks.empty());
  while (mIsRunning) {
    mQueueMonitor.Wait();
  }
}

void
TaskQueue::AwaitShutdownAndIdle()
{
  // Make sure there are no tasks for this queue waiting in the caller's tail
  // dispatcher.
  MOZ_ASSERT_IF(AbstractThread::GetCurrent(),
                !AbstractThread::GetCurrent()->TailDispatcher().HasTasksFor(this));

  MonitorAutoLock mon(mQueueMonitor);
  while (!mIsShutdown) {
    mQueueMonitor.Wait();
  }
  AwaitIdleLocked();
}

nsRefPtr<ShutdownPromise>
TaskQueue::BeginShutdown()
{
  // Dispatch any tasks for this queue waiting in the caller's tail dispatcher,
  // since this is the last opportunity to do so.
  if (AbstractThread* currentThread = AbstractThread::GetCurrent()) {
    currentThread->TailDispatcher().DispatchTasksFor(this);
  }

  MonitorAutoLock mon(mQueueMonitor);
  mIsShutdown = true;
  nsRefPtr<ShutdownPromise> p = mShutdownPromise.Ensure(__func__);
  MaybeResolveShutdown();
  mon.NotifyAll();
  return p;
}

bool
TaskQueue::IsEmpty()
{
  MonitorAutoLock mon(mQueueMonitor);
  return mTasks.empty();
}

bool
TaskQueue::IsCurrentThreadIn()
{
  bool in = NS_GetCurrentThread() == mRunningThread;
  MOZ_ASSERT(in == (GetCurrent() == this));
  return in;
}

nsresult
TaskQueue::Runner::Run()
{
  nsRefPtr<nsIRunnable> event;
  {
    MonitorAutoLock mon(mQueue->mQueueMonitor);
    MOZ_ASSERT(mQueue->mIsRunning);
    if (mQueue->mTasks.size() == 0) {
      mQueue->mIsRunning = false;
      mQueue->MaybeResolveShutdown();
      mon.NotifyAll();
      return NS_OK;
    }
    event = mQueue->mTasks.front().forget();
    mQueue->mTasks.pop();
  }
  MOZ_ASSERT(event);

  // Note that dropping the queue monitor before running the task, and
  // taking the monitor again after the task has run ensures we have memory
  // fences enforced. This means that if the object we're calling wasn't
  // designed to be threadsafe, it will be, provided we're only calling it
  // in this task queue.
  {
    AutoTaskGuard g(mQueue);
    event->Run();
  }

  // Drop the reference to event. The event will hold a reference to the
  // object it's calling, and we don't want to keep it alive, it may be
  // making assumptions what holds references to it. This is especially
  // the case if the object is waiting for us to shutdown, so that it
  // can shutdown (like in the MediaDecoderStateMachine's SHUTDOWN case).
  event = nullptr;

  {
    MonitorAutoLock mon(mQueue->mQueueMonitor);
    if (mQueue->mTasks.size() == 0) {
      // No more events to run. Exit the task runner.
      mQueue->mIsRunning = false;
      mQueue->MaybeResolveShutdown();
      mon.NotifyAll();
      return NS_OK;
    }
  }

  // There's at least one more event that we can run. Dispatch this Runner
  // to the thread pool again to ensure it runs again. Note that we don't just
  // run in a loop here so that we don't hog the thread pool. This means we may
  // run on another thread next time, but we rely on the memory fences from
  // mQueueMonitor for thread safety of non-threadsafe tasks.
  nsresult rv = mQueue->mPool->Dispatch(this, NS_DISPATCH_NORMAL);
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

} // namespace mozilla
