/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIClassInfoImpl.h"
#include "nsThreadPool.h"
#include "nsThreadManager.h"
#include "nsThread.h"
#include "nsMemory.h"
#include "nsAutoPtr.h"
#include "prinrval.h"
#include "prlog.h"

using namespace mozilla;

#ifdef PR_LOGGING
static PRLogModuleInfo *
GetThreadPoolLog()
{
  static PRLogModuleInfo *sLog;
  if (!sLog)
    sLog = PR_NewLogModule("nsThreadPool");
  return sLog;
}
#endif
#define LOG(args) PR_LOG(GetThreadPoolLog(), PR_LOG_DEBUG, args)

// DESIGN:
//  o  Allocate anonymous threads.
//  o  Use nsThreadPool::Run as the main routine for each thread.
//  o  Each thread waits on the event queue's monitor, checking for
//     pending events and rescheduling itself as an idle thread.

#define DEFAULT_THREAD_LIMIT 4
#define DEFAULT_IDLE_THREAD_LIMIT 1
#define DEFAULT_IDLE_THREAD_TIMEOUT PR_SecondsToInterval(60)

class ShutdownHelper MOZ_FINAL : public nsRunnable
{
public:
  NS_DECL_NSIRUNNABLE

  ShutdownHelper(nsCOMArray<nsIThread>& aThreads,
                 already_AddRefed<nsIThreadPoolListener> aListener)
  : mListener(aListener)
  {
    MOZ_ASSERT(!aThreads.IsEmpty());
    mThreads.SwapElements(aThreads);
  }

private:
  nsCOMArray<nsIThread> mThreads;
  nsCOMPtr<nsIThreadPoolListener> mListener;
};

NS_IMPL_ADDREF(nsThreadPool)
NS_IMPL_RELEASE(nsThreadPool)
NS_IMPL_CLASSINFO(nsThreadPool, NULL, nsIClassInfo::THREADSAFE,
                  NS_THREADPOOL_CID)
NS_IMPL_QUERY_INTERFACE3_CI(nsThreadPool, nsIThreadPool, nsIEventTarget,
                            nsIRunnable)
NS_IMPL_CI_INTERFACE_GETTER2(nsThreadPool, nsIThreadPool, nsIEventTarget)

nsThreadPool::nsThreadPool()
  : mThreadLimit(DEFAULT_THREAD_LIMIT)
  , mIdleThreadLimit(DEFAULT_IDLE_THREAD_LIMIT)
  , mIdleThreadTimeout(DEFAULT_IDLE_THREAD_TIMEOUT)
  , mIdleCount(0)
  , mShutdown(false)
{
}

nsThreadPool::~nsThreadPool()
{
  // Calling Shutdown() directly is not safe since it will spin the event loop
  // (perhaps during a GC). Instead we try to delay-shutdown each thread that is
  // still alive.
  nsCOMArray<nsIThread> threads;
  nsCOMPtr<nsIThreadPoolListener> listener;
  {
    ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
    if (!mShutdown) {
      NS_WARNING("nsThreadPool destroyed before Shutdown() was called!");

      mThreads.SwapElements(threads);
      mListener.swap(listener);
    }
  }

  if (!threads.IsEmpty()) {
    nsRefPtr<ShutdownHelper> helper =
      new ShutdownHelper(threads, listener.forget());
    if (NS_FAILED(NS_DispatchToMainThread(helper, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Unable to shut down threads in this thread pool!");
    }
  }
}

nsresult
nsThreadPool::PutEvent(nsIRunnable *event)
{
  // Avoid spawning a new thread while holding the event queue lock...
 
  bool spawnThread = false;
  {
    ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());

    LOG(("THRD-P(%p) put [%d %d %d]\n", this, mIdleCount, mThreads.Count(),
         mThreadLimit));
    MOZ_ASSERT(mIdleCount <= (uint32_t) mThreads.Count(), "oops");

    // Make sure we have a thread to service this event.
    if (mIdleCount == 0 && mThreads.Count() < (int32_t) mThreadLimit)
      spawnThread = true;

    mEvents.PutEvent(event);
  }

  LOG(("THRD-P(%p) put [spawn=%d]\n", this, spawnThread));
  if (!spawnThread)
    return NS_OK;

  nsCOMPtr<nsIThread> thread;
  nsThreadManager::get()->NewThread(0,
                                    nsIThreadManager::DEFAULT_STACK_SIZE,
                                    getter_AddRefs(thread));
  NS_ENSURE_STATE(thread);

  bool killThread = false;
  {
    ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
    if (mThreads.Count() < (int32_t) mThreadLimit) {
      mThreads.AppendObject(thread);
    } else {
      killThread = true;  // okay, we don't need this thread anymore
    }
  }
  LOG(("THRD-P(%p) put [%p kill=%d]\n", this, thread.get(), killThread));
  if (killThread) {
    // Pending events are processed on the current thread during
    // nsIThread::Shutdown() execution, so if nsThreadPool::Dispatch() is called
    // under caller's lock then deadlock could occur. This happens e.g. in case
    // of nsStreamCopier. To prevent this situation, dispatch a shutdown event
    // to the current thread instead of calling nsIThread::Shutdown() directly.

    nsRefPtr<nsIRunnable> r = NS_NewRunnableMethod(thread,
                                                   &nsIThread::Shutdown);
    NS_DispatchToCurrentThread(r);
  } else {
    thread->Dispatch(this, NS_DISPATCH_NORMAL);
  }

  return NS_OK;
}

void
nsThreadPool::ShutdownThread(nsIThread *thread)
{
  LOG(("THRD-P(%p) shutdown async [%p]\n", this, thread));

  // This method is responsible for calling Shutdown on |thread|.  This must be
  // done from some other thread, so we use the main thread of the application.

  MOZ_ASSERT(!NS_IsMainThread(), "wrong thread");

  nsRefPtr<nsIRunnable> r = NS_NewRunnableMethod(thread, &nsIThread::Shutdown);
  NS_DispatchToMainThread(r);
}

NS_IMETHODIMP
nsThreadPool::Run()
{
  LOG(("THRD-P(%p) enter\n", this));

  mThreadNaming.SetThreadPoolName(mName);

  nsCOMPtr<nsIThread> current;
  nsThreadManager::get()->GetCurrentThread(getter_AddRefs(current));

  bool shutdownThreadOnExit = false;
  bool exitThread = false;
  bool wasIdle = false;
  PRIntervalTime idleSince;

  nsCOMPtr<nsIThreadPoolListener> listener;
  {
    ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
    listener = mListener;
  }

  if (listener) {
    listener->OnThreadCreated();
  }

  do {
    nsCOMPtr<nsIRunnable> event;
    {
      ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
      if (!mEvents.GetPendingEvent(getter_AddRefs(event))) {
        PRIntervalTime now     = PR_IntervalNow();
        PRIntervalTime timeout = PR_MillisecondsToInterval(mIdleThreadTimeout);

        // If we are shutting down, then don't keep any idle threads
        if (mShutdown) {
          exitThread = true;
        } else {
          if (wasIdle) {
            // if too many idle threads or idle for too long, then bail.
            if (mIdleCount > mIdleThreadLimit || (now - idleSince) >= timeout)
              exitThread = true;
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
          if (wasIdle)
            --mIdleCount;
          shutdownThreadOnExit = mThreads.RemoveObject(current);
        } else {
          PRIntervalTime delta = timeout - (now - idleSince);
          LOG(("THRD-P(%p) waiting [%d]\n", this, delta));
          mon.Wait(delta);
        }
      } else if (wasIdle) {
        wasIdle = false;
        --mIdleCount;
      }
    }
    if (event) {
      LOG(("THRD-P(%p) running [%p]\n", this, event.get()));
      event->Run();
    }
  } while (!exitThread);

  if (listener) {
    listener->OnThreadShuttingDown();
  }

  if (shutdownThreadOnExit) {
    ShutdownThread(current);
  }

  LOG(("THRD-P(%p) leave\n", this));
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::Dispatch(nsIRunnable *event, uint32_t flags)
{
  LOG(("THRD-P(%p) dispatch [%p %x]\n", this, event, flags));

  NS_ENSURE_STATE(!mShutdown);

  if (flags & DISPATCH_SYNC) {
    nsCOMPtr<nsIThread> thread;
    nsThreadManager::get()->GetCurrentThread(getter_AddRefs(thread));
    NS_ENSURE_STATE(thread);

    nsRefPtr<nsThreadSyncDispatch> wrapper =
        new nsThreadSyncDispatch(thread, event);
    PutEvent(wrapper);

    while (wrapper->IsPending())
      NS_ProcessNextEvent(thread);
  } else {
    NS_ASSERTION(flags == NS_DISPATCH_NORMAL, "unexpected dispatch flags");
    PutEvent(event);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::IsOnCurrentThread(bool *result)
{
  // No one should be calling this method.  If this assertion gets hit, then we
  // need to think carefully about what this method should be returning.
  NS_NOTREACHED("implement me");

  *result = false;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::Shutdown()
{
  nsCOMArray<nsIThread> threads;
  nsCOMPtr<nsIThreadPoolListener> listener;
  {
    ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
    mShutdown = true;
    mon.NotifyAll();

    threads.AppendObjects(mThreads);
    mThreads.Clear();

    // Swap in a null listener so that we release the listener at the end of
    // this method. The listener will be kept alive as long as the other threads
    // that were created when it was set.
    mListener.swap(listener);
  }

  // It's important that we shutdown the threads while outside the event queue
  // monitor.  Otherwise, we could end up dead-locking.

  for (int32_t i = 0; i < threads.Count(); ++i)
    threads[i]->Shutdown();

  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetThreadLimit(uint32_t *value)
{
  *value = mThreadLimit;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetThreadLimit(uint32_t value)
{
  ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
  mThreadLimit = value;
  if (mIdleThreadLimit > mThreadLimit)
    mIdleThreadLimit = mThreadLimit;

  if (static_cast<uint32_t>(mThreads.Count()) > mThreadLimit) {
    mon.NotifyAll();  // wake up threads so they observe this change
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetIdleThreadLimit(uint32_t *value)
{
  *value = mIdleThreadLimit;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetIdleThreadLimit(uint32_t value)
{
  ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
  mIdleThreadLimit = value;
  if (mIdleThreadLimit > mThreadLimit)
    mIdleThreadLimit = mThreadLimit;

  // Do we need to kill some idle threads?
  if (mIdleCount > mIdleThreadLimit) {
    mon.NotifyAll();  // wake up threads so they observe this change
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetIdleThreadTimeout(uint32_t *value)
{
  *value = mIdleThreadTimeout;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetIdleThreadTimeout(uint32_t value)
{
  ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
  uint32_t oldTimeout = mIdleThreadTimeout;
  mIdleThreadTimeout = value;

  // Do we need to notify any idle threads that their sleep time has shortened?
  if (mIdleThreadTimeout < oldTimeout && mIdleCount > 0) {
    mon.NotifyAll();  // wake up threads so they observe this change
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetListener(nsIThreadPoolListener** aListener)
{
  ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
  NS_IF_ADDREF(*aListener = mListener);
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetListener(nsIThreadPoolListener* aListener)
{
  nsCOMPtr<nsIThreadPoolListener> swappedListener(aListener);
  {
    ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
    mListener.swap(swappedListener);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetName(const nsACString& aName)
{
  {
    ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
    if (mThreads.Count())
      return NS_ERROR_NOT_AVAILABLE;
  }

  mName = aName;
  return NS_OK;
}

NS_IMETHODIMP
ShutdownHelper::Run()
{
  MOZ_ASSERT(!mThreads.IsEmpty());

  for (int32_t i = 0; i < mThreads.Count(); ++i)
    mThreads[i]->Shutdown();

  mThreads.Clear();

  mListener = nullptr;
  return NS_OK;
}
