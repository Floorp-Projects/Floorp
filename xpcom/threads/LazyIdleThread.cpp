/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LazyIdleThread.h"

#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

#ifdef DEBUG
#  define ASSERT_OWNING_THREAD()                           \
    do {                                                   \
      MOZ_ASSERT(mOwningEventTarget->IsOnCurrentThread()); \
    } while (0)
#else
#  define ASSERT_OWNING_THREAD() /* nothing */
#endif

namespace mozilla {

LazyIdleThread::LazyIdleThread(uint32_t aIdleTimeoutMS, const char* aName,
                               ShutdownMethod aShutdownMethod)
    : mOwningEventTarget(GetCurrentSerialEventTarget()),
      mThreadPool(new nsThreadPool()),
      mTaskQueue(TaskQueue::Create(do_AddRef(mThreadPool), aName)) {
  // Configure the threadpool to host a single thread. It will be responsible
  // for managing the thread's lifetime.
  MOZ_ALWAYS_SUCCEEDS(mThreadPool->SetThreadLimit(1));
  MOZ_ALWAYS_SUCCEEDS(mThreadPool->SetIdleThreadLimit(1));
  MOZ_ALWAYS_SUCCEEDS(mThreadPool->SetIdleThreadTimeout(aIdleTimeoutMS));
  MOZ_ALWAYS_SUCCEEDS(mThreadPool->SetName(nsDependentCString(aName)));

  if (aShutdownMethod == ShutdownMethod::AutomaticShutdown &&
      NS_IsMainThread()) {
    if (nsCOMPtr<nsIObserverService> obs =
            do_GetService(NS_OBSERVERSERVICE_CONTRACTID)) {
      MOZ_ALWAYS_SUCCEEDS(
          obs->AddObserver(this, "xpcom-shutdown-threads", false));
    }
  }
}

static void LazyIdleThreadShutdown(nsThreadPool* aThreadPool,
                                   TaskQueue* aTaskQueue) {
  aTaskQueue->BeginShutdown();
  aTaskQueue->AwaitShutdownAndIdle();
  aThreadPool->Shutdown();
}

LazyIdleThread::~LazyIdleThread() {
  if (!mShutdown) {
    mOwningEventTarget->Dispatch(NS_NewRunnableFunction(
        "LazyIdleThread::~LazyIdleThread",
        [threadPool = mThreadPool, taskQueue = mTaskQueue] {
          LazyIdleThreadShutdown(threadPool, taskQueue);
        }));
  }
}

void LazyIdleThread::Shutdown() {
  ASSERT_OWNING_THREAD();

  if (!mShutdown) {
    mShutdown = true;
    LazyIdleThreadShutdown(mThreadPool, mTaskQueue);
  }
}

nsresult LazyIdleThread::SetListener(nsIThreadPoolListener* aListener) {
  return mThreadPool->SetListener(aListener);
}

NS_IMPL_ISUPPORTS(LazyIdleThread, nsIEventTarget, nsISerialEventTarget,
                  nsIObserver)

NS_IMETHODIMP
LazyIdleThread::DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> event(aEvent);
  return Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
LazyIdleThread::Dispatch(already_AddRefed<nsIRunnable> aEvent,
                         uint32_t aFlags) {
  return mTaskQueue->Dispatch(std::move(aEvent), aFlags);
}

NS_IMETHODIMP
LazyIdleThread::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LazyIdleThread::RegisterShutdownTask(nsITargetShutdownTask* aTask) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LazyIdleThread::UnregisterShutdownTask(nsITargetShutdownTask* aTask) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LazyIdleThread::IsOnCurrentThread(bool* aIsOnCurrentThread) {
  return mTaskQueue->IsOnCurrentThread(aIsOnCurrentThread);
}

NS_IMETHODIMP_(bool)
LazyIdleThread::IsOnCurrentThreadInfallible() {
  return mTaskQueue->IsOnCurrentThreadInfallible();
}

NS_IMETHODIMP
LazyIdleThread::Observe(nsISupports* /* aSubject */, const char* aTopic,
                        const char16_t* /* aData */) {
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(!strcmp("xpcom-shutdown-threads", aTopic), "Bad topic!");

  Shutdown();
  return NS_OK;
}

}  // namespace mozilla
