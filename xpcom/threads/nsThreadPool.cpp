/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMArray.h"
#include "ThreadDelay.h"
#include "nsThreadPool.h"
#include "nsThreadManager.h"
#include "nsThread.h"
#include "nsMemory.h"
#include "prinrval.h"
#include "mozilla/Logging.h"
#include "mozilla/SchedulerGroup.h"
#include "nsThreadSyncDispatch.h"

#include <mutex>

using namespace mozilla;

static LazyLogModule sThreadPoolLog("nsThreadPool");
#ifdef LOG
#  undef LOG
#endif
#define LOG(args) MOZ_LOG(sThreadPoolLog, mozilla::LogLevel::Debug, args)

static MOZ_THREAD_LOCAL(nsThreadPool*) gCurrentThreadPool;

// DESIGN:
//  o  Allocate anonymous threads.
//  o  Use nsThreadPool::Run as the main routine for each thread.
//  o  Each thread waits on the event queue's monitor, checking for
//     pending events and rescheduling itself as an idle thread.

#define DEFAULT_THREAD_LIMIT 4
#define DEFAULT_IDLE_THREAD_LIMIT 1
#define DEFAULT_IDLE_THREAD_TIMEOUT PR_SecondsToInterval(60)

NS_IMPL_ADDREF(nsThreadPool)
NS_IMPL_RELEASE(nsThreadPool)
NS_IMPL_QUERY_INTERFACE(nsThreadPool, nsIThreadPool, nsIEventTarget,
                        nsIRunnable)

nsThreadPool::nsThreadPool()
    : mMutex("[nsThreadPool.mMutex]"),
      mEventsAvailable(mMutex, "[nsThreadPool.mEventsAvailable]"),
      mThreadLimit(DEFAULT_THREAD_LIMIT),
      mIdleThreadLimit(DEFAULT_IDLE_THREAD_LIMIT),
      mIdleThreadTimeout(DEFAULT_IDLE_THREAD_TIMEOUT),
      mIdleCount(0),
      mStackSize(nsIThreadManager::DEFAULT_STACK_SIZE),
      mShutdown(false),
      mRegressiveMaxIdleTime(false),
      mIsAPoolThreadFree(true) {
  static std::once_flag flag;
  std::call_once(flag, [] { gCurrentThreadPool.infallibleInit(); });

  LOG(("THRD-P(%p) constructor!!!\n", this));
}

nsThreadPool::~nsThreadPool() {
  // Threads keep a reference to the nsThreadPool until they return from Run()
  // after removing themselves from mThreads.
  MOZ_ASSERT(mThreads.IsEmpty());
}

nsresult nsThreadPool::PutEvent(nsIRunnable* aEvent) {
  nsCOMPtr<nsIRunnable> event(aEvent);
  return PutEvent(event.forget(), 0);
}

nsresult nsThreadPool::PutEvent(already_AddRefed<nsIRunnable> aEvent,
                                uint32_t aFlags) {
  // Avoid spawning a new thread while holding the event queue lock...

  bool spawnThread = false;
  uint32_t stackSize = 0;
  {
    MutexAutoLock lock(mMutex);

    if (NS_WARN_IF(mShutdown)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    LOG(("THRD-P(%p) put [%d %d %d]\n", this, mIdleCount, mThreads.Count(),
         mThreadLimit));
    MOZ_ASSERT(mIdleCount <= (uint32_t)mThreads.Count(), "oops");

    // Make sure we have a thread to service this event.
    if (mThreads.Count() < (int32_t)mThreadLimit &&
        !(aFlags & NS_DISPATCH_AT_END) &&
        // Spawn a new thread if we don't have enough idle threads to serve
        // pending events immediately.
        mEvents.Count(lock) >= mIdleCount) {
      spawnThread = true;
    }

    mEvents.PutEvent(std::move(aEvent), EventQueuePriority::Normal, lock);
    mEventsAvailable.Notify();
    stackSize = mStackSize;
  }

  auto delay = MakeScopeExit([&]() {
    // Delay to encourage the receiving task to run before we do work.
    DelayForChaosMode(ChaosFeature::TaskDispatching, 1000);
  });

  LOG(("THRD-P(%p) put [spawn=%d]\n", this, spawnThread));
  if (!spawnThread) {
    return NS_OK;
  }

  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread(mThreadNaming.GetNextThreadName(mName),
                                  getter_AddRefs(thread), nullptr, stackSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_UNEXPECTED;
  }

  bool killThread = false;
  {
    MutexAutoLock lock(mMutex);
    if (mShutdown) {
      killThread = true;
    } else if (mThreads.Count() < (int32_t)mThreadLimit) {
      mThreads.AppendObject(thread);
      if (mThreads.Count() >= (int32_t)mThreadLimit) {
        mIsAPoolThreadFree = false;
      }
    } else {
      // Someone else may have also been starting a thread
      killThread = true;  // okay, we don't need this thread anymore
    }
  }
  LOG(("THRD-P(%p) put [%p kill=%d]\n", this, thread.get(), killThread));
  if (killThread) {
    // We never dispatched any events to the thread, so we can shut it down
    // asynchronously without worrying about anything.
    ShutdownThread(thread);
  } else {
    thread->Dispatch(this, NS_DISPATCH_NORMAL);
  }

  return NS_OK;
}

void nsThreadPool::ShutdownThread(nsIThread* aThread) {
  LOG(("THRD-P(%p) shutdown async [%p]\n", this, aThread));

  // This is either called by a threadpool thread that is out of work, or
  // a thread that attempted to create a threadpool thread and raced in
  // such a way that the newly created thread is no longer necessary.
  // In the first case, we must go to another thread to shut aThread down
  // (because it is the current thread).  In the second case, we cannot
  // synchronously shut down the current thread (because then Dispatch() would
  // spin the event loop, and that could blow up the world), and asynchronous
  // shutdown requires this thread have an event loop (and it may not, see bug
  // 10204784).  The simplest way to cover all cases is to asynchronously
  // shutdown aThread from the main thread.
  SchedulerGroup::Dispatch(
      TaskCategory::Other,
      NewRunnableMethod("nsIThread::AsyncShutdown", aThread,
                        &nsIThread::AsyncShutdown));
}

// This event 'runs' for the lifetime of the worker thread.  The actual
// eventqueue is mEvents, and is shared by all the worker threads.  This
// means that the set of threads together define the delay seen by a new
// event sent to the pool.
//
// To model the delay experienced by the pool, we can have each thread in
// the pool report 0 if it's idle OR if the pool is below the threadlimit;
// or otherwise the current event's queuing delay plus current running
// time.
//
// To reconstruct the delays for the pool, the profiler can look at all the
// threads that are part of a pool (pools have defined naming patterns that
// can be user to connect them).  If all threads have delays at time X,
// that means that all threads saturated at that point and any event
// dispatched to the pool would get a delay.
//
// The delay experienced by an event dispatched when all pool threads are
// busy is based on the calculations shown in platform.cpp.  Run that
// algorithm for each thread in the pool, and the delay at time X is the
// longest value for time X of any of the threads, OR the time from X until
// any one of the threads reports 0 (i.e. it's not busy), whichever is
// shorter.

// In order to record this when the profiler samples threads in the pool,
// each thread must (effectively) override GetRunnningEventDelay, by
// resetting the mLastEventDelay/Start values in the nsThread when we start
// to run an event (or when we run out of events to run).  Note that handling
// the shutdown of a thread may be a little tricky.

NS_IMETHODIMP
nsThreadPool::Run() {
  LOG(("THRD-P(%p) enter %s\n", this, mName.BeginReading()));

  nsCOMPtr<nsIThread> current;
  nsThreadManager::get().GetCurrentThread(getter_AddRefs(current));

  bool shutdownThreadOnExit = false;
  bool exitThread = false;
  bool wasIdle = false;
  TimeStamp idleSince;

  // This thread is an nsThread created below with NS_NewNamedThread()
  static_cast<nsThread*>(current.get())
      ->SetPoolThreadFreePtr(&mIsAPoolThreadFree);

  nsCOMPtr<nsIThreadPoolListener> listener;
  {
    MutexAutoLock lock(mMutex);
    listener = mListener;
  }

  if (listener) {
    listener->OnThreadCreated();
  }

  MOZ_ASSERT(!gCurrentThreadPool.get());
  gCurrentThreadPool.set(this);

  do {
    nsCOMPtr<nsIRunnable> event;
    TimeDuration delay;
    {
      MutexAutoLock lock(mMutex);

      event = mEvents.GetEvent(nullptr, lock, &delay);
      if (!event) {
        TimeStamp now = TimeStamp::Now();
        uint32_t idleTimeoutDivider =
            (mIdleCount && mRegressiveMaxIdleTime) ? mIdleCount : 1;
        TimeDuration timeout = TimeDuration::FromMilliseconds(
            static_cast<double>(mIdleThreadTimeout) / idleTimeoutDivider);

        // If we are shutting down, then don't keep any idle threads
        if (mShutdown) {
          exitThread = true;
        } else {
          if (wasIdle) {
            // if too many idle threads or idle for too long, then bail.
            if (mIdleCount > mIdleThreadLimit ||
                (mIdleThreadTimeout != UINT32_MAX &&
                 (now - idleSince) >= timeout)) {
              exitThread = true;
            }
          } else {
            // if would be too many idle threads...
            if (mIdleCount == mIdleThreadLimit) {
              exitThread = true;
            } else {
              ++mIdleCount;
              idleSince = now;
              wasIdle = true;
            }
          }
        }

        if (exitThread) {
          if (wasIdle) {
            --mIdleCount;
          }
          shutdownThreadOnExit = mThreads.RemoveObject(current);

          // keep track if there are threads available to start
          mIsAPoolThreadFree = (mThreads.Count() < (int32_t)mThreadLimit);
        } else {
          current->SetRunningEventDelay(TimeDuration(), TimeStamp());

          AUTO_PROFILER_LABEL("nsThreadPool::Run::Wait", IDLE);

          TimeDuration delta = timeout - (now - idleSince);
          LOG(("THRD-P(%p) %s waiting [%f]\n", this, mName.BeginReading(),
               delta.ToMilliseconds()));
          {
            AUTO_PROFILER_THREAD_SLEEP;
            mEventsAvailable.Wait(delta);
          }
          LOG(("THRD-P(%p) done waiting\n", this));
        }
      } else if (wasIdle) {
        wasIdle = false;
        --mIdleCount;
      }
    }
    if (event) {
      LOG(("THRD-P(%p) %s running [%p]\n", this, mName.BeginReading(),
           event.get()));

      // Delay event processing to encourage whoever dispatched this event
      // to run.
      DelayForChaosMode(ChaosFeature::TaskRunning, 1000);

      // We'll handle the case of unstarted threads available
      // when we sample.
      current->SetRunningEventDelay(delay, TimeStamp::Now());

      event->Run();
    }
  } while (!exitThread);

  if (listener) {
    listener->OnThreadShuttingDown();
  }

  MOZ_ASSERT(gCurrentThreadPool.get() == this);
  gCurrentThreadPool.set(nullptr);

  if (shutdownThreadOnExit) {
    ShutdownThread(current);
  }

  LOG(("THRD-P(%p) leave\n", this));
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> event(aEvent);
  return Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
nsThreadPool::Dispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aFlags) {
  LOG(("THRD-P(%p) dispatch [%p %x]\n", this, /* XXX aEvent*/ nullptr, aFlags));

  if (NS_WARN_IF(mShutdown)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (aFlags & DISPATCH_SYNC) {
    nsCOMPtr<nsIThread> thread;
    nsThreadManager::get().GetCurrentThread(getter_AddRefs(thread));
    if (NS_WARN_IF(!thread)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    RefPtr<nsThreadSyncDispatch> wrapper =
        new nsThreadSyncDispatch(thread.forget(), std::move(aEvent));
    PutEvent(wrapper);

    SpinEventLoopUntil(
        [&, wrapper]() -> bool { return !wrapper->IsPending(); });
  } else {
    NS_ASSERTION(aFlags == NS_DISPATCH_NORMAL || aFlags == NS_DISPATCH_AT_END,
                 "unexpected dispatch flags");
    PutEvent(std::move(aEvent), aFlags);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(bool)
nsThreadPool::IsOnCurrentThreadInfallible() {
  return gCurrentThreadPool.get() == this;
}

NS_IMETHODIMP
nsThreadPool::IsOnCurrentThread(bool* aResult) {
  MutexAutoLock lock(mMutex);
  if (NS_WARN_IF(mShutdown)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aResult = IsOnCurrentThreadInfallible();
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::Shutdown() {
  nsCOMArray<nsIThread> threads;
  nsCOMPtr<nsIThreadPoolListener> listener;
  {
    MutexAutoLock lock(mMutex);
    mShutdown = true;
    mEventsAvailable.NotifyAll();

    threads.AppendObjects(mThreads);
    mThreads.Clear();

    // Swap in a null listener so that we release the listener at the end of
    // this method. The listener will be kept alive as long as the other threads
    // that were created when it was set.
    mListener.swap(listener);
  }

  // It's important that we shutdown the threads while outside the event queue
  // monitor.  Otherwise, we could end up dead-locking.

  for (int32_t i = 0; i < threads.Count(); ++i) {
    threads[i]->Shutdown();
  }

  return NS_OK;
}

template <typename Pred>
static void SpinMTEventLoopUntil(Pred&& aPredicate, nsIThread* aThread,
                                 TimeDuration aTimeout) {
  MOZ_ASSERT(NS_IsMainThread(), "Must be run on the main thread");

  // From a latency perspective, spinning the event loop is like leaving script
  // and returning to the event loop. Tell the watchdog we stopped running
  // script (until we return).
  mozilla::Maybe<xpc::AutoScriptActivity> asa;
  asa.emplace(false);

  TimeStamp deadline = TimeStamp::Now() + aTimeout;
  while (!aPredicate() && TimeStamp::Now() < deadline) {
    if (!NS_ProcessNextEvent(aThread, false)) {
      PR_Sleep(PR_MillisecondsToInterval(1));
    }
  }
}

NS_IMETHODIMP
nsThreadPool::ShutdownWithTimeout(int32_t aTimeoutMs) {
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMArray<nsIThread> threads;
  nsCOMPtr<nsIThreadPoolListener> listener;
  {
    MutexAutoLock lock(mMutex);
    mShutdown = true;
    mEventsAvailable.NotifyAll();

    threads.AppendObjects(mThreads);
    mThreads.Clear();

    // Swap in a null listener so that we release the listener at the end of
    // this method. The listener will be kept alive as long as the other threads
    // that were created when it was set.
    mListener.swap(listener);
  }

  // IMPORTANT! Never dereference these pointers, as the objects may go away at
  // any time. We just use the pointers values for comparison, to check if the
  // thread has been shut down or not.
  nsTArray<nsThreadShutdownContext*> contexts;

  // It's important that we shutdown the threads while outside the event queue
  // monitor.  Otherwise, we could end up dead-locking.
  for (int32_t i = 0; i < threads.Count(); ++i) {
    // Shutdown async
    nsThreadShutdownContext* maybeContext =
        static_cast<nsThread*>(threads[i])->ShutdownInternal(false);
    contexts.AppendElement(maybeContext);
  }

  NotNull<nsThread*> currentThread =
      WrapNotNull(nsThreadManager::get().GetCurrentThread());

  // We spin the event loop until all of the threads in the thread pool
  // have shut down, or the timeout expires.
  SpinMTEventLoopUntil(
      [&]() {
        for (nsIThread* thread : threads) {
          if (static_cast<nsThread*>(thread)->mThread) {
            return false;
          }
        }
        return true;
      },
      currentThread, TimeDuration::FromMilliseconds(aTimeoutMs));

  // For any threads that have not shutdown yet, we need to remove them from
  // mRequestedShutdownContexts so the thread manager does not wait for them
  // at shutdown.
  static const nsThread::ShutdownContextsComp comparator{};
  for (int32_t i = 0; i < threads.Count(); ++i) {
    nsThread* thread = static_cast<nsThread*>(threads[i]);
    // If mThread is not null on the thread it means that it hasn't shutdown
    // context[i] corresponds to thread[i]
    if (thread->mThread && contexts[i]) {
      auto index = currentThread->mRequestedShutdownContexts.IndexOf(
          contexts[i], 0, comparator);
      if (index != nsThread::ShutdownContexts::NoIndex) {
        // We must leak the shutdown context just in case the leaked thread
        // does get unstuck and completes before the main thread is done.
        Unused << currentThread->mRequestedShutdownContexts[index].release();
        currentThread->mRequestedShutdownContexts.RemoveElementsAt(index, 1);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetThreadLimit(uint32_t* aValue) {
  *aValue = mThreadLimit;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetThreadLimit(uint32_t aValue) {
  MutexAutoLock lock(mMutex);
  LOG(("THRD-P(%p) thread limit [%u]\n", this, aValue));
  mThreadLimit = aValue;
  if (mIdleThreadLimit > mThreadLimit) {
    mIdleThreadLimit = mThreadLimit;
  }

  if (static_cast<uint32_t>(mThreads.Count()) > mThreadLimit) {
    mEventsAvailable
        .NotifyAll();  // wake up threads so they observe this change
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetIdleThreadLimit(uint32_t* aValue) {
  *aValue = mIdleThreadLimit;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetIdleThreadLimit(uint32_t aValue) {
  MutexAutoLock lock(mMutex);
  LOG(("THRD-P(%p) idle thread limit [%u]\n", this, aValue));
  mIdleThreadLimit = aValue;
  if (mIdleThreadLimit > mThreadLimit) {
    mIdleThreadLimit = mThreadLimit;
  }

  // Do we need to kill some idle threads?
  if (mIdleCount > mIdleThreadLimit) {
    mEventsAvailable
        .NotifyAll();  // wake up threads so they observe this change
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetIdleThreadTimeout(uint32_t* aValue) {
  *aValue = mIdleThreadTimeout;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetIdleThreadTimeout(uint32_t aValue) {
  MutexAutoLock lock(mMutex);
  uint32_t oldTimeout = mIdleThreadTimeout;
  mIdleThreadTimeout = aValue;

  // Do we need to notify any idle threads that their sleep time has shortened?
  if (mIdleThreadTimeout < oldTimeout && mIdleCount > 0) {
    mEventsAvailable
        .NotifyAll();  // wake up threads so they observe this change
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetIdleThreadTimeoutRegressive(bool* aValue) {
  *aValue = mRegressiveMaxIdleTime;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetIdleThreadTimeoutRegressive(bool aValue) {
  MutexAutoLock lock(mMutex);
  bool oldRegressive = mRegressiveMaxIdleTime;
  mRegressiveMaxIdleTime = aValue;

  // Would setting regressive timeout effect idle threads?
  if (mRegressiveMaxIdleTime > oldRegressive && mIdleCount > 1) {
    mEventsAvailable
        .NotifyAll();  // wake up threads so they observe this change
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetThreadStackSize(uint32_t* aValue) {
  MutexAutoLock lock(mMutex);
  *aValue = mStackSize;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetThreadStackSize(uint32_t aValue) {
  MutexAutoLock lock(mMutex);
  mStackSize = aValue;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetListener(nsIThreadPoolListener** aListener) {
  MutexAutoLock lock(mMutex);
  NS_IF_ADDREF(*aListener = mListener);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetListener(nsIThreadPoolListener* aListener) {
  nsCOMPtr<nsIThreadPoolListener> swappedListener(aListener);
  {
    MutexAutoLock lock(mMutex);
    mListener.swap(swappedListener);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetName(const nsACString& aName) {
  {
    MutexAutoLock lock(mMutex);
    if (mThreads.Count()) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  mName = aName;
  return NS_OK;
}
