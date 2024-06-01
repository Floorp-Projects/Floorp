/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadPool.h"

#include "nsCOMArray.h"
#include "ThreadDelay.h"
#include "nsThreadManager.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "prinrval.h"
#include "mozilla/Logging.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerRunnable.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StickyTimeDuration.h"
#include "nsThreadSyncDispatch.h"

#include <mutex>

using namespace mozilla;

static LazyLogModule sThreadPoolLog("nsThreadPool");
#ifdef LOG
#  undef LOG
#endif
#define LOG(args) MOZ_LOG(sThreadPoolLog, mozilla::LogLevel::Debug, args)

static MOZ_THREAD_LOCAL(nsThreadPool*) gCurrentThreadPool;

void nsThreadPool::InitTLS() { gCurrentThreadPool.infallibleInit(); }

// DESIGN:
//  o  Allocate anonymous threads.
//  o  Use nsThreadPool::Run as the main routine for each thread.
//  o  Each thread waits on the event queue's monitor, checking for
//     pending events and rescheduling itself as an idle thread.

#define DEFAULT_THREAD_LIMIT 4
#define DEFAULT_IDLE_THREAD_LIMIT 1
#define DEFAULT_IDLE_THREAD_GRACE_TIMEOUT_MS 100
#define DEFAULT_IDLE_THREAD_MAX_TIMEOUT_MS 60000

NS_IMPL_ISUPPORTS_INHERITED(nsThreadPool, Runnable, nsIThreadPool,
                            nsIEventTarget)

nsThreadPool* nsThreadPool::GetCurrentThreadPool() {
  return gCurrentThreadPool.get();
}

nsThreadPool::nsThreadPool()
    : Runnable("nsThreadPool"),
      mMutex("[nsThreadPool.mMutex]"),
      mThreadLimit(DEFAULT_THREAD_LIMIT),
      mIdleThreadLimit(DEFAULT_IDLE_THREAD_LIMIT),
      mIdleThreadGraceTimeout(
          TimeDuration::FromMilliseconds(DEFAULT_IDLE_THREAD_GRACE_TIMEOUT_MS)),
      mIdleThreadMaxTimeout(
          TimeDuration::FromMilliseconds(DEFAULT_IDLE_THREAD_MAX_TIMEOUT_MS)),
      mQoSPriority(nsIThread::QOS_PRIORITY_NORMAL),
      mStackSize(nsIThreadManager::DEFAULT_STACK_SIZE),
      mShutdown(false),
      mIsAPoolThreadFree(true) {
  LOG(("THRD-P(%p) constructor!!!\n", this));
}

nsThreadPool::~nsThreadPool() {
  // Threads keep a reference to the nsThreadPool until they return from Run()
  // after removing themselves from mThreads.
  MOZ_ASSERT(mThreads.IsEmpty());
}

// Each thread has its own MRUIdleEntry instance. If it is element of the
// mMRUIdleThreads list, it can be notified for event processing.
struct nsThreadPool::MRUIdleEntry
    : public mozilla::LinkedListElement<MRUIdleEntry> {
  // Created from thread (as local variable).
  explicit MRUIdleEntry(mozilla::Mutex& aMutex)
      : mEventsAvailable(aMutex,
                         "[nsThreadPool.MRUIdleStatus.mEventsAvailable]") {}

  // Keep track of the moment the thread finished its last event.
  mozilla::TimeStamp mIdleSince;
  // Each thread has its own cond var.
  mozilla::CondVar mEventsAvailable;
#ifdef DEBUG
  // If we were notified for work, keeps track when.
  mozilla::TimeStamp mNotifiedSince;
  // If we are going to sleep, keeps track for how long.
  mozilla::TimeDuration mLastWaitDelay;
#endif
};

#ifdef DEBUG
// This logging relies on extra members we do not want to bake into release.
void nsThreadPool::DebugLogPoolStatus(MutexAutoLock& aProofOfLock,
                                      MRUIdleEntry* aWakingEntry) {
  if (!MOZ_LOG_TEST(sThreadPoolLog, mozilla::LogLevel::Debug)) {
    return;
  }

  LOG(
      ("THRD-P(%p) \"%s\" (entry %p) status ---- mThreads(%u), mEvents(%u), "
       "mThreadLimit(%u), mIdleThreadLimit(%u), mIdleCount(%zd), "
       "mMRUIdleThreads(%u), mShutdown(%u)\n",
       this, mName.get(), aWakingEntry, mThreads.Length(),
       (uint32_t)mEvents.Count(aProofOfLock), mThreadLimit, mIdleThreadLimit,
       mMRUIdleThreads.length(), (uint32_t)mMRUIdleThreads.length(),
       (uint32_t)mShutdown));

  auto logEntry = [&](MRUIdleEntry* entry, const char* msg) {
    LOG(
        (" - (entry %p) %s, IdleSince(%d), "
         "NotifiedSince(%d) LastWaitDelay(%d)\n",
         entry, msg,
         (int)((entry->mIdleSince.IsNull())
                   ? -1
                   : (TimeStamp::Now() - entry->mIdleSince).ToMilliseconds()),
         (int)((entry->mNotifiedSince.IsNull())
                   ? -1
                   : (TimeStamp::Now() - entry->mNotifiedSince)
                         .ToMilliseconds()),
         (int)entry->mLastWaitDelay.ToMilliseconds()));
  };

  if (aWakingEntry) {
    logEntry(aWakingEntry, "woke up");
  }
  for (auto* idle : mMRUIdleThreads) {
    logEntry(idle, "in idle list");
  }
}
#endif

nsresult nsThreadPool::PutEvent(nsIRunnable* aEvent) {
  nsCOMPtr<nsIRunnable> event(aEvent);
  return PutEvent(event.forget(), 0);
}

nsresult nsThreadPool::PutEvent(already_AddRefed<nsIRunnable> aEvent,
                                uint32_t aFlags) {
  // Avoid spawning a new thread while holding the event queue lock...

  bool spawnThread = false;
  uint32_t stackSize = 0;
  nsCString name;
  {
    MutexAutoLock lock(mMutex);

    if (NS_WARN_IF(mShutdown)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsCOMPtr<nsIRunnable> event(aEvent);
    LogRunnable::LogDispatch(event);
    mEvents.PutEvent(event.forget(), EventQueuePriority::Normal, lock);

#ifdef DEBUG
    DebugLogPoolStatus(lock, nullptr);
#endif

    // We've added the event to the queue, make sure a thread
    // will wake up to handle it.
    if (aFlags & NS_DISPATCH_AT_END) {
      // If NS_DISPATCH_AT_END is set, this thread is about to
      // become free to process the event, so we don't need to
      // signal another thread.
      MOZ_ASSERT(IsOnCurrentThreadInfallible(),
                 "NS_DISPATCH_AT_END can only be set when "
                 "dispatching from on the thread pool.");
      LOG(("THRD-P(%p) put [%zd %d %d]: NS_DISPATCH_AT_END w/out Notify.\n",
           this, mMRUIdleThreads.length(), mThreads.Count(), mThreadLimit));
    } else if (auto* mruThread = mMRUIdleThreads.getFirst()) {
      // If we have an idle thread, wake it up and remove it
      // from the idle list, so that future dispatches try
      // to wake other threads.
      mruThread->remove();
      mruThread->mEventsAvailable.Notify();
#ifdef DEBUG
      mruThread->mNotifiedSince = TimeStamp::Now();
#endif
      LOG(("THRD-P(%p) put [%zd %d %d]: Notify idle thread via entry(%p).\n",
           this, mMRUIdleThreads.length(), mThreads.Count(), mThreadLimit,
           mruThread));
    } else if (mThreads.Count() < (int32_t)mThreadLimit) {
      // Otherwise we want to start a new thread assuming we
      // haven't hit the thread limit yet.
      spawnThread = true;
      LOG(("THRD-P(%p) put [%zd %d %d]: Spawn a new thread.\n", this,
           mMRUIdleThreads.length(), mThreads.Count(), mThreadLimit));
    } else {
      // If we have no thread available, just leave the event in the queue
      // ready for the next thread about to become idle and pick it up.
      LOG(("THRD-P(%p) put [%zd %d %d]: No idle or new thread available.\n",
           this, mMRUIdleThreads.length(), mThreads.Count(), mThreadLimit));
    }

    MOZ_ASSERT(spawnThread || mThreads.Count() > 0);
    stackSize = mStackSize;
    name = mName;
  }

  auto delay = MakeScopeExit([&]() {
    // Delay to encourage the receiving task to run before we do work.
    DelayForChaosMode(ChaosFeature::TaskDispatching, 1000);
  });

  if (!spawnThread) {
    return NS_OK;
  }

  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread(
      mThreadNaming.GetNextThreadName(name), getter_AddRefs(thread), nullptr,
      {.stackSize = stackSize, .blockDispatch = true});
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
    thread->Dispatch(this, NS_DISPATCH_IGNORE_BLOCK_DISPATCH);
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
  SchedulerGroup::Dispatch(NewRunnableMethod(
      "nsIThread::AsyncShutdown", aThread, &nsIThread::AsyncShutdown));
}

NS_IMETHODIMP
nsThreadPool::SetQoSForThreads(nsIThread::QoSPriority aPriority) {
  MutexAutoLock lock(mMutex);
  mQoSPriority = aPriority;

  // We don't notify threads here to observe the change, because we don't want
  // to create spurious wakeups during idle. Rather, we want threads to simply
  // observe the change on their own if they wake up to do some task.

  return NS_OK;
}

void nsThreadPool::NotifyChangeToAllIdleThreads() {
  for (auto* idleThread : mMRUIdleThreads) {
    idleThread->mEventsAvailable.Notify();
  }
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
  nsCOMPtr<nsIThread> current;
  nsThreadManager::get().GetCurrentThread(getter_AddRefs(current));

  bool shutdownThreadOnExit = false;
  bool exitThread = false;
  MRUIdleEntry idleEntry(mMutex);
  bool wasIdle = false;
  nsIThread::QoSPriority threadPriority = nsIThread::QOS_PRIORITY_NORMAL;

  // This thread is an nsThread created below with NS_NewNamedThread()
  static_cast<nsThread*>(current.get())
      ->SetPoolThreadFreePtr(&mIsAPoolThreadFree);

  nsCOMPtr<nsIThreadPoolListener> listener;
  {
    MutexAutoLock lock(mMutex);
    listener = mListener;
    LOG(("THRD-P(%p) enter %s\n", this, mName.get()));

    // Go ahead and check for thread priority. If priority is normal, do nothing
    // because threads are created with default priority.
    if (threadPriority != mQoSPriority) {
      current->SetThreadQoS(threadPriority);
      threadPriority = mQoSPriority;
    }
  }

  if (listener) {
    listener->OnThreadCreated();
  }

  MOZ_ASSERT(!gCurrentThreadPool.get());
  gCurrentThreadPool.set(this);

  do {
    nsCOMPtr<nsIRunnable> event;
    TimeDuration lastEventDelay;
    {
      MutexAutoLock lock(mMutex);

#ifdef DEBUG
      DebugLogPoolStatus(lock, &idleEntry);
      idleEntry.mNotifiedSince = TimeStamp();
#endif

      // Before getting the next event, we can adjust priority as needed.
      if (threadPriority != mQoSPriority) {
        current->SetThreadQoS(threadPriority);
        threadPriority = mQoSPriority;
      }

      event = mEvents.GetEvent(lock, &lastEventDelay);
      if (!event) {
        TimeStamp now = TimeStamp::Now();
        uint32_t cnt = mMRUIdleThreads.length() + ((wasIdle) ? 0 : 1);
        TimeDuration currentTimeout = (cnt > mIdleThreadLimit)
                                          ? mIdleThreadGraceTimeout
                                          : mIdleThreadMaxTimeout;

        if (mShutdown) {
          exitThread = true;
        } else {
          if (!wasIdle) {
            // Going idle for a new idle period.
            MOZ_ASSERT(!idleEntry.isInList());
            idleEntry.mIdleSince = now;
            wasIdle = true;
            mMRUIdleThreads.insertFront(&idleEntry);
          } else if ((now - idleEntry.mIdleSince) < currentTimeout) {
            // Continue to stay idle without touching mIdleSince.
            if (!idleEntry.isInList()) {
              mMRUIdleThreads.insertFront(&idleEntry);
            }
          } else {
            // We reached our timeout.
            exitThread = true;
          }
        }

        if (exitThread) {
          wasIdle = false;
          if (idleEntry.isInList()) {
            idleEntry.remove();
          }

          shutdownThreadOnExit = mThreads.RemoveObject(current);

          // keep track if there are threads available to start
          mIsAPoolThreadFree = (mThreads.Count() < (int32_t)mThreadLimit);
        } else {
          current->SetRunningEventDelay(TimeDuration(), TimeStamp());

          AUTO_PROFILER_LABEL("nsThreadPool::Run::Wait", IDLE);

          // Depending on the allowed number of idle threads, wait for events
          // at most our grace or max time minus the time we were already idle.
          // Use StickyTimeDuration when performing math to preserve a timeout
          // of TimeDuration::Forever.
          TimeDuration delta{StickyTimeDuration{currentTimeout} -
                             (now - idleEntry.mIdleSince)};
          delta = TimeDuration::Max(delta, TimeDuration::FromMilliseconds(1));
          LOG(("THRD-P(%p) %s waiting [%f]\n", this, mName.get(),
               delta.ToMilliseconds()));
#ifdef DEBUG
          idleEntry.mLastWaitDelay = delta;
#endif
          idleEntry.mEventsAvailable.Wait(delta);
          LOG(("THRD-P(%p) done waiting\n", this));
        }
      } else {
        // We have an event to work on.
        wasIdle = false;
        if (idleEntry.isInList()) {
          idleEntry.remove();
        }
      }
      // Release our lock.
    }

    if (event) {
      if (MOZ_LOG_TEST(sThreadPoolLog, mozilla::LogLevel::Debug)) {
        MutexAutoLock lock(mMutex);
        LOG(("THRD-P(%p) %s running [%p]\n", this, mName.get(), event.get()));
      }

      // Delay event processing to encourage whoever dispatched this event
      // to run.
      DelayForChaosMode(ChaosFeature::TaskRunning, 1000);

      if (profiler_thread_is_being_profiled(
              ThreadProfilingFeatures::Sampling)) {
        // We'll handle the case of unstarted threads available
        // when we sample.
        current->SetRunningEventDelay(lastEventDelay, TimeStamp::Now());
      }

      LogRunnable::Run log(event);
      AUTO_PROFILE_FOLLOWING_RUNNABLE(event);
      event->Run();
      // To cover the event's destructor code in the LogRunnable span
      event = nullptr;
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
    nsCOMPtr<nsIRunnable> event(aEvent);
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(aFlags == NS_DISPATCH_NORMAL || aFlags == NS_DISPATCH_AT_END,
               "unexpected dispatch flags");
  PutEvent(std::move(aEvent), aFlags);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsThreadPool::RegisterShutdownTask(nsITargetShutdownTask*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsThreadPool::UnregisterShutdownTask(nsITargetShutdownTask*) {
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
nsThreadPool::Shutdown() { return ShutdownWithTimeout(-1); }

NS_IMETHODIMP
nsThreadPool::ShutdownWithTimeout(int32_t aTimeoutMs) {
  nsCOMArray<nsIThread> threads;
  nsCOMPtr<nsIThreadPoolListener> listener;
  {
    MutexAutoLock lock(mMutex);
    if (mShutdown) {
      return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    }
    mShutdown = true;
    NotifyChangeToAllIdleThreads();

    threads.AppendObjects(mThreads);
    mThreads.Clear();

    // Swap in a null listener so that we release the listener at the end of
    // this method. The listener will be kept alive as long as the other threads
    // that were created when it was set.
    mListener.swap(listener);
  }

  nsTArray<nsCOMPtr<nsIThreadShutdown>> contexts;
  for (int32_t i = 0; i < threads.Count(); ++i) {
    nsCOMPtr<nsIThreadShutdown> context;
    if (NS_SUCCEEDED(threads[i]->BeginShutdown(getter_AddRefs(context)))) {
      contexts.AppendElement(std::move(context));
    }
  }

  // Start a timer which will stop waiting & leak the thread, forcing
  // onCompletion to be called when it expires.
  nsCOMPtr<nsITimer> timer;
  if (aTimeoutMs >= 0) {
    NS_NewTimerWithCallback(
        getter_AddRefs(timer),
        [&](nsITimer*) {
          for (auto& context : contexts) {
            context->StopWaitingAndLeakThread();
          }
        },
        aTimeoutMs, nsITimer::TYPE_ONE_SHOT,
        "nsThreadPool::ShutdownWithTimeout");
  }

  // Start a counter and register a callback to decrement outstandingThreads
  // when the threads finish exiting. We'll spin an event loop until
  // outstandingThreads reaches 0.
  uint32_t outstandingThreads = contexts.Length();
  RefPtr onCompletion = NS_NewCancelableRunnableFunction(
      "nsThreadPool thread completion", [&] { --outstandingThreads; });
  for (auto& context : contexts) {
    context->OnCompletion(onCompletion);
  }

  mozilla::SpinEventLoopUntil("nsThreadPool::ShutdownWithTimeout"_ns,
                              [&] { return outstandingThreads == 0; });

  if (timer) {
    timer->Cancel();
  }
  onCompletion->Cancel();

  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetThreadLimit(uint32_t* aValue) {
  MutexAutoLock lock(mMutex);
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
  NotifyChangeToAllIdleThreads();
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetIdleThreadLimit(uint32_t* aValue) {
  MutexAutoLock lock(mMutex);
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
  NotifyChangeToAllIdleThreads();
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetIdleThreadGraceTimeout(uint32_t* aValue) {
  MutexAutoLock lock(mMutex);
  *aValue = (uint32_t)mIdleThreadGraceTimeout.ToMilliseconds();
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetIdleThreadGraceTimeout(uint32_t aValue) {
  // We do not want to support forever here.
  MOZ_ASSERT(aValue != UINT32_MAX);

  MutexAutoLock lock(mMutex);
  TimeDuration oldTimeout = mIdleThreadGraceTimeout;
  mIdleThreadGraceTimeout = TimeDuration::FromMilliseconds(aValue);
  // We do not want to clamp here to avoid unexpected results due to the order
  // of calling the setters, but we also do not want to clamp where we use it
  // for performance reasons. Tell the caller.
  MOZ_ASSERT(mIdleThreadGraceTimeout <= mIdleThreadMaxTimeout);

  // Do we need to notify any idle threads that their sleep time has shortened?
  if (mIdleThreadGraceTimeout < oldTimeout) {
    NotifyChangeToAllIdleThreads();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetIdleThreadMaximumTimeout(uint32_t* aValue) {
  MutexAutoLock lock(mMutex);
  *aValue = (uint32_t)mIdleThreadMaxTimeout.ToMilliseconds();
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetIdleThreadMaximumTimeout(uint32_t aValue) {
  MutexAutoLock lock(mMutex);
  TimeDuration oldTimeout = mIdleThreadMaxTimeout;
  if (aValue == UINT32_MAX) {
    mIdleThreadMaxTimeout = TimeDuration::Forever();
  } else {
    mIdleThreadMaxTimeout = TimeDuration::FromMilliseconds(aValue);
  }
  // We do not want to clamp here to avoid unexpected results due to the order
  // of calling the setters, but we also do not want to clamp where we use it
  // for performance reasons. Tell the caller.
  MOZ_ASSERT(mIdleThreadGraceTimeout <= mIdleThreadMaxTimeout);

  // Do we need to notify any idle threads that their sleep time has shortened?
  if (mIdleThreadMaxTimeout < oldTimeout) {
    NotifyChangeToAllIdleThreads();
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
  MutexAutoLock lock(mMutex);
  if (mThreads.Count()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mName = aName;
  return NS_OK;
}
