/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Services.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsIThread.h"
#include "nsNamedPipeService.h"
#include "nsNetCID.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

static mozilla::LazyLogModule gNamedPipeServiceLog("NamedPipeWin");
#define LOG_NPS_DEBUG(...) \
  MOZ_LOG(gNamedPipeServiceLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#define LOG_NPS_ERROR(...) \
  MOZ_LOG(gNamedPipeServiceLog, mozilla::LogLevel::Error, (__VA_ARGS__))

NS_IMPL_ISUPPORTS(NamedPipeService,
                  nsINamedPipeService,
                  nsIObserver,
                  nsIRunnable)

NamedPipeService::NamedPipeService()
  : mIocp(nullptr)
  , mIsShutdown(false)
  , mLock("NamedPipeServiceLock")
{
}

nsresult
NamedPipeService::Init()
{
  MOZ_ASSERT(!mIsShutdown);

  nsresult rv;

  // nsIObserverService must be accessed in main thread.
  // register shutdown event to stop NamedPipeSrv thread.
  nsCOMPtr<nsIObserver> self(this);
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction("NamedPipeService::Init",
                                                   [self = std::move(self)] () -> void {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIObserverService> svc = mozilla::services::GetObserverService();

    if (NS_WARN_IF(!svc)) {
      return;
    }

    if (NS_WARN_IF(NS_FAILED(svc->AddObserver(self,
                                              NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                              false)))) {
      return;
    }
  });

  if (NS_IsMainThread()) {
    rv = r->Run();
  } else {
    rv = NS_DispatchToMainThread(r);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
  if (NS_WARN_IF(!mIocp || mIocp == INVALID_HANDLE_VALUE)) {
    Shutdown();
    return NS_ERROR_FAILURE;
  }

  rv = NS_NewNamedThread("NamedPipeSrv", getter_AddRefs(mThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Shutdown();
    return rv;
  }

  return NS_OK;
}

void
NamedPipeService::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  // remove observer
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }

  // stop thread
  if (mThread && !mIsShutdown) {
    mIsShutdown = true;

    // invoke ERROR_ABANDONED_WAIT_0 to |GetQueuedCompletionStatus|
    CloseHandle(mIocp);
    mIocp = nullptr;

    mThread->Shutdown();
  }

  // close I/O Completion Port
  if (mIocp && mIocp != INVALID_HANDLE_VALUE) {
    CloseHandle(mIocp);
    mIocp = nullptr;
  }
}

void
NamedPipeService::RemoveRetiredObjects()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mThread);
  mLock.AssertCurrentThreadOwns();

  if (!mRetiredHandles.IsEmpty()) {
    for (auto& handle : mRetiredHandles) {
      CloseHandle(handle);
    }
    mRetiredHandles.Clear();
  }

  mRetiredObservers.Clear();
}

/**
 * Implement nsINamedPipeService
 */

NS_IMETHODIMP
NamedPipeService::AddDataObserver(void* aHandle,
                                  nsINamedPipeDataObserver* aObserver)
{
  if (!aHandle || aHandle == INVALID_HANDLE_VALUE || !aObserver) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsresult rv;

  HANDLE h = CreateIoCompletionPort(aHandle,
                                    mIocp,
                                    reinterpret_cast<ULONG_PTR>(aObserver),
                                    1);
  if (NS_WARN_IF(!h)) {
    LOG_NPS_ERROR("CreateIoCompletionPort error (%d)", GetLastError());
    return NS_ERROR_FAILURE;
  }
  if (NS_WARN_IF(h != mIocp)) {
    LOG_NPS_ERROR("CreateIoCompletionPort got unexpected value %p (should be %p)",
              h,
              mIocp);
    CloseHandle(h);
    return NS_ERROR_FAILURE;
  }

  {
    MutexAutoLock lock(mLock);
    MOZ_ASSERT(!mObservers.Contains(aObserver));

    mObservers.AppendElement(aObserver);

    // start event loop
    if (mObservers.Length() == 1) {
      rv = mThread->Dispatch(this, NS_DISPATCH_NORMAL);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        LOG_NPS_ERROR("Dispatch to thread failed (%08x)", rv);
        mObservers.Clear();
        return rv;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
NamedPipeService::RemoveDataObserver(void* aHandle,
                                     nsINamedPipeDataObserver* aObserver)
{
  MutexAutoLock lock(mLock);
  mObservers.RemoveElement(aObserver);

  mRetiredHandles.AppendElement(aHandle);
  mRetiredObservers.AppendElement(aObserver);

  return NS_OK;
}

NS_IMETHODIMP
NamedPipeService::IsOnCurrentThread(bool* aRetVal)
{
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(aRetVal);

  if (!mThread) {
    *aRetVal = false;
    return NS_OK;
  }

  return mThread->IsOnCurrentThread(aRetVal);
}

/**
 * Implement nsIObserver
 */

NS_IMETHODIMP
NamedPipeService::Observe(nsISupports* aSubject,
                          const char* aTopic,
                          const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic)) {
    Shutdown();
  }

  return NS_OK;
}

/**
 * Implement nsIRunnable
 */

NS_IMETHODIMP
NamedPipeService::Run()
{
  MOZ_ASSERT(NS_GetCurrentThread() == mThread);
  MOZ_ASSERT(mIocp && mIocp != INVALID_HANDLE_VALUE);

  while (!mIsShutdown) {
    {
      MutexAutoLock lock(mLock);
      if (mObservers.IsEmpty()) {
        LOG_NPS_DEBUG("no observer, stop loop");
        break;
      }

      RemoveRetiredObjects();
    }

    DWORD bytesTransferred = 0;
    ULONG_PTR key = 0;
    LPOVERLAPPED overlapped = nullptr;
    BOOL success = GetQueuedCompletionStatus(mIocp,
                                             &bytesTransferred,
                                             &key,
                                             &overlapped,
                                             1000); // timeout, 1s
    auto err = GetLastError();
    if (!success) {
      if (err == WAIT_TIMEOUT) {
        continue;
      } else if (err == ERROR_ABANDONED_WAIT_0) { // mIocp was closed
        break;
      } else if (!overlapped) {
        /**
         * Did not dequeue a completion packet from the completion port, and
         * bytesTransferred/key are meaningless.
         * See remarks of |GetQueuedCompletionStatus| API.
         */

        LOG_NPS_ERROR("invalid overlapped (%d)", err);
        continue;
      }

      MOZ_ASSERT(key);
    }

    /**
     * Windows doesn't provide a method to remove created I/O Completion Port,
     * all we can do is just close the handle we monitored before.
     * In some cases, there's race condition that the monitored handle has an
     * I/O status after the observer is being removed and destroyed.
     * To avoid changing the ref-count of a dangling pointer, don't use nsCOMPtr
     * here.
     */
    nsINamedPipeDataObserver* target =
      reinterpret_cast<nsINamedPipeDataObserver*>(key);

    nsCOMPtr<nsINamedPipeDataObserver> obs;
    {
      MutexAutoLock lock(mLock);

      auto idx = mObservers.IndexOf(target);
      if (idx == decltype(mObservers)::NoIndex) {
        LOG_NPS_ERROR("observer %p not found", target);
        continue;
      }
      obs = target;
    }

    MOZ_ASSERT(obs.get());

    if (success) {
      LOG_NPS_DEBUG("OnDataAvailable: obs=%p, bytes=%d",
                    obs.get(),
                    bytesTransferred);
      obs->OnDataAvailable(bytesTransferred, overlapped);
    } else {
      LOG_NPS_ERROR("GetQueuedCompletionStatus %p failed, error=%d",
                    obs.get(),
                    err);
      obs->OnError(err, overlapped);
    }

  }

  {
    MutexAutoLock lock(mLock);
    RemoveRetiredObjects();
  }

  return NS_OK;
}

static NS_DEFINE_CID(kNamedPipeServiceCID, NS_NAMEDPIPESERVICE_CID);

} // namespace net
} // namespace mozilla
