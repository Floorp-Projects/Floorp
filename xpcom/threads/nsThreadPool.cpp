/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIProxyObjectManager.h"
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
static PRLogModuleInfo *sLog = PR_NewLogModule("nsThreadPool");
#endif
#define LOG(args) PR_LOG(sLog, PR_LOG_DEBUG, args)

// DESIGN:
//  o  Allocate anonymous threads.
//  o  Use nsThreadPool::Run as the main routine for each thread.
//  o  Each thread waits on the event queue's monitor, checking for
//     pending events and rescheduling itself as an idle thread.

#define DEFAULT_THREAD_LIMIT 4
#define DEFAULT_IDLE_THREAD_LIMIT 1
#define DEFAULT_IDLE_THREAD_TIMEOUT PR_SecondsToInterval(60)

NS_IMPL_THREADSAFE_ADDREF(nsThreadPool)
NS_IMPL_THREADSAFE_RELEASE(nsThreadPool)
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
  , mShutdown(PR_FALSE)
{
}

nsThreadPool::~nsThreadPool()
{
  Shutdown();
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
    NS_ASSERTION(mIdleCount <= (PRUint32) mThreads.Count(), "oops");

    // Make sure we have a thread to service this event.
    if (mIdleCount == 0 && mThreads.Count() < (PRInt32) mThreadLimit)
      spawnThread = PR_TRUE;

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
    if (mThreads.Count() < (PRInt32) mThreadLimit) {
      mThreads.AppendObject(thread);
    } else {
      killThread = PR_TRUE;  // okay, we don't need this thread anymore
    }
  }
  LOG(("THRD-P(%p) put [%p kill=%d]\n", this, thread.get(), killThread));
  if (killThread) {
    thread->Shutdown();
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

  NS_ASSERTION(!NS_IsMainThread(), "wrong thread");

  nsCOMPtr<nsIThread> doomed;
  NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD, NS_GET_IID(nsIThread), thread,
                       NS_PROXY_ASYNC, getter_AddRefs(doomed));
  if (doomed) {
    doomed->Shutdown();
  } else {
    NS_WARNING("failed to construct proxy to main thread");
  }
}

NS_IMETHODIMP
nsThreadPool::Run()
{
  LOG(("THRD-P(%p) enter\n", this));

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
          exitThread = PR_TRUE;
        } else {
          if (wasIdle) {
            // if too many idle threads or idle for too long, then bail.
            if (mIdleCount > mIdleThreadLimit || (now - idleSince) >= timeout)
              exitThread = PR_TRUE;
          } else {
            // if would be too many idle threads...
            if (mIdleCount == mIdleThreadLimit) {
              exitThread = PR_TRUE;
            } else {
              ++mIdleCount;
              idleSince = now;
              wasIdle = PR_TRUE;
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
        wasIdle = PR_FALSE;
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
nsThreadPool::Dispatch(nsIRunnable *event, PRUint32 flags)
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

  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::Shutdown()
{
  nsCOMArray<nsIThread> threads;
  nsCOMPtr<nsIThreadPoolListener> listener;
  {
    ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
    mShutdown = PR_TRUE;
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

  for (PRInt32 i = 0; i < threads.Count(); ++i)
    threads[i]->Shutdown();

  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetThreadLimit(PRUint32 *value)
{
  *value = mThreadLimit;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetThreadLimit(PRUint32 value)
{
  ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
  mThreadLimit = value;
  if (mIdleThreadLimit > mThreadLimit)
    mIdleThreadLimit = mThreadLimit;
  mon.NotifyAll();  // wake up threads so they observe this change
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetIdleThreadLimit(PRUint32 *value)
{
  *value = mIdleThreadLimit;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetIdleThreadLimit(PRUint32 value)
{
  ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
  mIdleThreadLimit = value;
  if (mIdleThreadLimit > mThreadLimit)
    mIdleThreadLimit = mThreadLimit;
  mon.NotifyAll();  // wake up threads so they observe this change
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::GetIdleThreadTimeout(PRUint32 *value)
{
  *value = mIdleThreadTimeout;
  return NS_OK;
}

NS_IMETHODIMP
nsThreadPool::SetIdleThreadTimeout(PRUint32 value)
{
  ReentrantMonitorAutoEnter mon(mEvents.GetReentrantMonitor());
  mIdleThreadTimeout = value;
  mon.NotifyAll();  // wake up threads so they observe this change
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
