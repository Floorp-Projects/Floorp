/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LazyIdleThread.h"

#include "nsIObserverService.h"

#include "GeckoProfiler.h"
#include "nsComponentManagerUtils.h"
#include "nsIIdlePeriod.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"

#ifdef DEBUG
#define ASSERT_OWNING_THREAD()                                                 \
  do {                                                                         \
    nsIThread* currentThread = NS_GetCurrentThread();                          \
    if (currentThread) {                                                       \
      nsCOMPtr<nsISupports> current(do_QueryInterface(currentThread));         \
      nsCOMPtr<nsISupports> test(do_QueryInterface(mOwningThread));            \
      MOZ_ASSERT(current == test, "Wrong thread!");                            \
    }                                                                          \
  } while(0)
#else
#define ASSERT_OWNING_THREAD() /* nothing */
#endif

namespace mozilla {

LazyIdleThread::LazyIdleThread(uint32_t aIdleTimeoutMS,
                               const nsCSubstring& aName,
                               ShutdownMethod aShutdownMethod,
                               nsIObserver* aIdleObserver)
  : mMutex("LazyIdleThread::mMutex")
  , mOwningThread(NS_GetCurrentThread())
  , mIdleObserver(aIdleObserver)
  , mQueuedRunnables(nullptr)
  , mIdleTimeoutMS(aIdleTimeoutMS)
  , mPendingEventCount(0)
  , mIdleNotificationCount(0)
  , mShutdownMethod(aShutdownMethod)
  , mShutdown(false)
  , mThreadIsShuttingDown(false)
  , mIdleTimeoutEnabled(true)
  , mName(aName)
{
  MOZ_ASSERT(mOwningThread, "Need owning thread!");
}

LazyIdleThread::~LazyIdleThread()
{
  ASSERT_OWNING_THREAD();

  Shutdown();
}

void
LazyIdleThread::SetWeakIdleObserver(nsIObserver* aObserver)
{
  ASSERT_OWNING_THREAD();

  if (mShutdown) {
    NS_WARNING_ASSERTION(!aObserver,
                         "Setting an observer after Shutdown was called!");
    return;
  }

  mIdleObserver = aObserver;
}

void
LazyIdleThread::DisableIdleTimeout()
{
  ASSERT_OWNING_THREAD();
  if (!mIdleTimeoutEnabled) {
    return;
  }
  mIdleTimeoutEnabled = false;

  if (mIdleTimer && NS_FAILED(mIdleTimer->Cancel())) {
    NS_WARNING("Failed to cancel timer!");
  }

  MutexAutoLock lock(mMutex);

  // Pretend we have a pending event to keep the idle timer from firing.
  MOZ_ASSERT(mPendingEventCount < UINT32_MAX, "Way too many!");
  mPendingEventCount++;
}

void
LazyIdleThread::EnableIdleTimeout()
{
  ASSERT_OWNING_THREAD();
  if (mIdleTimeoutEnabled) {
    return;
  }
  mIdleTimeoutEnabled = true;

  {
    MutexAutoLock lock(mMutex);

    MOZ_ASSERT(mPendingEventCount, "Mismatched calls to observer methods!");
    --mPendingEventCount;
  }

  if (mThread) {
    nsCOMPtr<nsIRunnable> runnable(new Runnable());
    if (NS_FAILED(Dispatch(runnable.forget(), NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch!");
    }
  }
}

void
LazyIdleThread::PreDispatch()
{
  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(mPendingEventCount < UINT32_MAX, "Way too many!");
  mPendingEventCount++;
}

nsresult
LazyIdleThread::EnsureThread()
{
  ASSERT_OWNING_THREAD();

  if (mShutdown) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mThread) {
    return NS_OK;
  }

  MOZ_ASSERT(!mPendingEventCount, "Shouldn't have events yet!");
  MOZ_ASSERT(!mIdleNotificationCount, "Shouldn't have idle events yet!");
  MOZ_ASSERT(!mIdleTimer, "Should have killed this long ago!");
  MOZ_ASSERT(!mThreadIsShuttingDown, "Should have cleared that!");

  nsresult rv;

  if (mShutdownMethod == AutomaticShutdown && NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> obs =
      do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = obs->AddObserver(this, "xpcom-shutdown-threads", false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mIdleTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_WARN_IF(!mIdleTimer)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod(this, &LazyIdleThread::InitThread);
  if (NS_WARN_IF(!runnable)) {
    return NS_ERROR_UNEXPECTED;
  }

  rv = NS_NewNamedThread("Lazy Idle", getter_AddRefs(mThread), runnable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
LazyIdleThread::InitThread()
{
  // Happens on mThread but mThread may not be set yet...

  nsCOMPtr<nsIThreadInternal> thread(do_QueryInterface(NS_GetCurrentThread()));
  MOZ_ASSERT(thread, "This should always succeed!");

  if (NS_FAILED(thread->SetObserver(this))) {
    NS_WARNING("Failed to set thread observer!");
  }
}

void
LazyIdleThread::CleanupThread()
{
  nsCOMPtr<nsIThreadInternal> thread(do_QueryInterface(NS_GetCurrentThread()));
  MOZ_ASSERT(thread, "This should always succeed!");

  if (NS_FAILED(thread->SetObserver(nullptr))) {
    NS_WARNING("Failed to set thread observer!");
  }

  {
    MutexAutoLock lock(mMutex);

    MOZ_ASSERT(!mThreadIsShuttingDown, "Shouldn't be true ever!");
    mThreadIsShuttingDown = true;
  }
}

void
LazyIdleThread::ScheduleTimer()
{
  ASSERT_OWNING_THREAD();

  bool shouldSchedule;
  {
    MutexAutoLock lock(mMutex);

    MOZ_ASSERT(mIdleNotificationCount, "Should have at least one!");
    --mIdleNotificationCount;

    shouldSchedule = !mIdleNotificationCount && !mPendingEventCount;
  }

  if (mIdleTimer) {
    if (NS_FAILED(mIdleTimer->Cancel())) {
      NS_WARNING("Failed to cancel timer!");
    }

    if (shouldSchedule &&
        NS_FAILED(mIdleTimer->InitWithCallback(this, mIdleTimeoutMS,
                                               nsITimer::TYPE_ONE_SHOT))) {
      NS_WARNING("Failed to schedule timer!");
    }
  }
}

nsresult
LazyIdleThread::ShutdownThread()
{
  ASSERT_OWNING_THREAD();

  // Before calling Shutdown() on the real thread we need to put a queue in
  // place in case a runnable is posted to the thread while it's in the
  // process of shutting down. This will be our queue.
  AutoTArray<nsCOMPtr<nsIRunnable>, 10> queuedRunnables;

  nsresult rv;

  // Make sure to cancel the shutdown timer before spinning the event loop
  // during |mThread->Shutdown()| below. Otherwise the timer might fire and we
  // could reenter here.
  if (mIdleTimer) {
    rv = mIdleTimer->Cancel();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mIdleTimer = nullptr;
  }

  if (mThread) {
    if (mShutdownMethod == AutomaticShutdown && NS_IsMainThread()) {
      nsCOMPtr<nsIObserverService> obs =
        mozilla::services::GetObserverService();
      NS_WARNING_ASSERTION(obs, "Failed to get observer service!");

      if (obs &&
          NS_FAILED(obs->RemoveObserver(this, "xpcom-shutdown-threads"))) {
        NS_WARNING("Failed to remove observer!");
      }
    }

    if (mIdleObserver) {
      mIdleObserver->Observe(static_cast<nsIThread*>(this), IDLE_THREAD_TOPIC,
                             nullptr);
    }

#ifdef DEBUG
    {
      MutexAutoLock lock(mMutex);
      MOZ_ASSERT(!mThreadIsShuttingDown, "Huh?!");
    }
#endif

    nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod(this, &LazyIdleThread::CleanupThread);
    if (NS_WARN_IF(!runnable)) {
      return NS_ERROR_UNEXPECTED;
    }

    PreDispatch();

    rv = mThread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Put the temporary queue in place before calling Shutdown().
    mQueuedRunnables = &queuedRunnables;

    if (NS_FAILED(mThread->Shutdown())) {
      NS_ERROR("Failed to shutdown the thread!");
    }

    // Now unset the queue.
    mQueuedRunnables = nullptr;

    mThread = nullptr;

    {
      MutexAutoLock lock(mMutex);

      MOZ_ASSERT(!mPendingEventCount, "Huh?!");
      MOZ_ASSERT(!mIdleNotificationCount, "Huh?!");
      MOZ_ASSERT(mThreadIsShuttingDown, "Huh?!");
      mThreadIsShuttingDown = false;
    }
  }

  // If our temporary queue has any runnables then we need to dispatch them.
  if (queuedRunnables.Length()) {
    // If the thread manager has gone away then these runnables will never run.
    if (mShutdown) {
      NS_ERROR("Runnables dispatched to LazyIdleThread will never run!");
      return NS_OK;
    }

    // Re-dispatch the queued runnables.
    for (uint32_t index = 0; index < queuedRunnables.Length(); index++) {
      nsCOMPtr<nsIRunnable> runnable;
      runnable.swap(queuedRunnables[index]);
      MOZ_ASSERT(runnable, "Null runnable?!");

      if (NS_FAILED(Dispatch(runnable.forget(), NS_DISPATCH_NORMAL))) {
        NS_ERROR("Failed to re-dispatch queued runnable!");
      }
    }
  }

  return NS_OK;
}

void
LazyIdleThread::SelfDestruct()
{
  MOZ_ASSERT(mRefCnt == 1, "Bad refcount!");
  delete this;
}

NS_IMPL_ADDREF(LazyIdleThread)

NS_IMETHODIMP_(MozExternalRefCountType)
LazyIdleThread::Release()
{
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "LazyIdleThread");

  if (!count) {
    // Stabilize refcount.
    mRefCnt = 1;

    nsCOMPtr<nsIRunnable> runnable =
      NewNonOwningRunnableMethod(this, &LazyIdleThread::SelfDestruct);
    NS_WARNING_ASSERTION(runnable, "Couldn't make runnable!");

    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
      // The only way this could fail is if we're in shutdown, and in that case
      // threads should have been joined already. Deleting here isn't dangerous
      // anymore because we won't spin the event loop waiting to join the
      // thread.
      SelfDestruct();
    }
  }

  return count;
}

NS_IMPL_QUERY_INTERFACE(LazyIdleThread, nsIThread,
                        nsIEventTarget,
                        nsISerialEventTarget,
                        nsITimerCallback,
                        nsIThreadObserver,
                        nsIObserver)

NS_IMETHODIMP
LazyIdleThread::DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> event(aEvent);
  return Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
LazyIdleThread::Dispatch(already_AddRefed<nsIRunnable> aEvent,
                         uint32_t aFlags)
{
  ASSERT_OWNING_THREAD();
  nsCOMPtr<nsIRunnable> event(aEvent); // avoid leaks

  // LazyIdleThread can't always support synchronous dispatch currently.
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (NS_WARN_IF(mShutdown)) {
    return NS_ERROR_UNEXPECTED;
  }

  // If our thread is shutting down then we can't actually dispatch right now.
  // Queue this runnable for later.
  if (UseRunnableQueue()) {
    mQueuedRunnables->AppendElement(event);
    return NS_OK;
  }

  nsresult rv = EnsureThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  PreDispatch();

  return mThread->Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
LazyIdleThread::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LazyIdleThread::IsOnCurrentThread(bool* aIsOnCurrentThread)
{
  if (mThread) {
    return mThread->IsOnCurrentThread(aIsOnCurrentThread);
  }

  *aIsOnCurrentThread = false;
  return NS_OK;
}

NS_IMETHODIMP_(bool)
LazyIdleThread::IsOnCurrentThreadInfallible()
{
  if (mThread) {
    return mThread->IsOnCurrentThread();
  }

  return false;
}

NS_IMETHODIMP
LazyIdleThread::GetPRThread(PRThread** aPRThread)
{
  if (mThread) {
    return mThread->GetPRThread(aPRThread);
  }

  *aPRThread = nullptr;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
LazyIdleThread::GetCanInvokeJS(bool* aCanInvokeJS)
{
  *aCanInvokeJS = false;
  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::SetCanInvokeJS(bool aCanInvokeJS)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LazyIdleThread::AsyncShutdown()
{
  ASSERT_OWNING_THREAD();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LazyIdleThread::Shutdown()
{
  ASSERT_OWNING_THREAD();

  mShutdown = true;

  nsresult rv = ShutdownThread();
  MOZ_ASSERT(!mThread, "Should have destroyed this by now!");

  mIdleObserver = nullptr;

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::HasPendingEvents(bool* aHasPendingEvents)
{
  // This is only supposed to be called from the thread itself so it's not
  // implemented here.
  NS_NOTREACHED("Shouldn't ever call this!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LazyIdleThread::IdleDispatch(already_AddRefed<nsIRunnable> aEvent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LazyIdleThread::IdleDispatchFromScript(nsIRunnable* aEvent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LazyIdleThread::RegisterIdlePeriod(already_AddRefed<nsIIdlePeriod> aIdlePeriod)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LazyIdleThread::ProcessNextEvent(bool aMayWait,
                                 bool* aEventWasProcessed)
{
  // This is only supposed to be called from the thread itself so it's not
  // implemented here.
  NS_NOTREACHED("Shouldn't ever call this!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
LazyIdleThread::Notify(nsITimer* aTimer)
{
  ASSERT_OWNING_THREAD();

  {
    MutexAutoLock lock(mMutex);

    if (mPendingEventCount || mIdleNotificationCount) {
      // Another event was scheduled since this timer was set. Don't do
      // anything and wait for the timer to fire again.
      return NS_OK;
    }
  }

  nsresult rv = ShutdownThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::OnDispatchedEvent(nsIThreadInternal* /*aThread */)
{
  MOZ_ASSERT(NS_GetCurrentThread() == mOwningThread, "Wrong thread!");
  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::OnProcessNextEvent(nsIThreadInternal* /* aThread */,
                                   bool /* aMayWait */)
{
  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::AfterProcessNextEvent(nsIThreadInternal* /* aThread */,
                                      bool aEventWasProcessed)
{
  bool shouldNotifyIdle;
  {
    MutexAutoLock lock(mMutex);

    if (aEventWasProcessed) {
      MOZ_ASSERT(mPendingEventCount, "Mismatched calls to observer methods!");
      --mPendingEventCount;
    }

    if (mThreadIsShuttingDown) {
      // We're shutting down, no need to fire any timer.
      return NS_OK;
    }

    shouldNotifyIdle = !mPendingEventCount;
    if (shouldNotifyIdle) {
      MOZ_ASSERT(mIdleNotificationCount < UINT32_MAX, "Way too many!");
      mIdleNotificationCount++;
    }
  }

  if (shouldNotifyIdle) {
    nsCOMPtr<nsIRunnable> runnable =
      NewRunnableMethod(this, &LazyIdleThread::ScheduleTimer);
    if (NS_WARN_IF(!runnable)) {
      return NS_ERROR_UNEXPECTED;
    }

    nsresult rv = mOwningThread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
LazyIdleThread::Observe(nsISupports* /* aSubject */,
                        const char*  aTopic,
                        const char16_t* /* aData */)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(mShutdownMethod == AutomaticShutdown,
             "Should not receive notifications if not AutomaticShutdown!");
  MOZ_ASSERT(!strcmp("xpcom-shutdown-threads", aTopic), "Bad topic!");

  Shutdown();
  return NS_OK;
}

} // namespace mozilla
