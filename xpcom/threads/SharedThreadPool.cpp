/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SharedThreadPool.h"
#include "mozilla/Monitor.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsDataHashtable.h"
#include "nsXPCOMCIDInternal.h"
#include "nsComponentManagerUtils.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIThreadManager.h"
#ifdef XP_WIN
#include "ThreadPoolCOMListener.h"
#endif

namespace mozilla {

// Created and destroyed on the main thread.
static StaticAutoPtr<ReentrantMonitor> sMonitor;

// Hashtable, maps thread pool name to SharedThreadPool instance.
// Modified only on the main thread.
static StaticAutoPtr<nsDataHashtable<nsCStringHashKey, SharedThreadPool*>> sPools;

static already_AddRefed<nsIThreadPool>
CreateThreadPool(const nsCString& aName);

class SharedThreadPoolShutdownObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
protected:
  virtual ~SharedThreadPoolShutdownObserver() {}
};

NS_IMPL_ISUPPORTS(SharedThreadPoolShutdownObserver, nsIObserver, nsISupports)

NS_IMETHODIMP
SharedThreadPoolShutdownObserver::Observe(nsISupports* aSubject, const char *aTopic,
                                          const char16_t *aData)
{
  MOZ_RELEASE_ASSERT(!strcmp(aTopic, "xpcom-shutdown-threads"));
#ifdef EARLY_BETA_OR_EARLIER
  {
    ReentrantMonitorAutoEnter mon(*sMonitor);
    if (!sPools->Iter().Done()) {
      nsAutoCString str;
      for (auto i = sPools->Iter(); !i.Done(); i.Next()) {
        str.AppendPrintf("\"%s\" ", nsAutoCString(i.Key()).get());
      }
      printf_stderr("SharedThreadPool in xpcom-shutdown-threads. Waiting for "
                    "pools %s\n", str.get());
    }
  }
#endif
  SharedThreadPool::SpinUntilEmpty();
  sMonitor = nullptr;
  sPools = nullptr;
  return NS_OK;
}

void
SharedThreadPool::InitStatics()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sMonitor && !sPools);
  sMonitor = new ReentrantMonitor("SharedThreadPool");
  sPools = new nsDataHashtable<nsCStringHashKey, SharedThreadPool*>();
  nsCOMPtr<nsIObserverService> obsService = mozilla::services::GetObserverService();
  nsCOMPtr<nsIObserver> obs = new SharedThreadPoolShutdownObserver();
  obsService->AddObserver(obs, "xpcom-shutdown-threads", false);
}

/* static */
bool
SharedThreadPool::IsEmpty()
{
  ReentrantMonitorAutoEnter mon(*sMonitor);
  return !sPools->Count();
}

/* static */
void
SharedThreadPool::SpinUntilEmpty()
{
  MOZ_ASSERT(NS_IsMainThread());
  SpinEventLoopUntil([]() -> bool {
      sMonitor->AssertNotCurrentThreadIn();
      return IsEmpty();
  });
}

already_AddRefed<SharedThreadPool>
SharedThreadPool::Get(const nsCString& aName, uint32_t aThreadLimit)
{
  MOZ_ASSERT(sMonitor && sPools);
  ReentrantMonitorAutoEnter mon(*sMonitor);
  RefPtr<SharedThreadPool> pool;
  nsresult rv;

  if (auto entry = sPools->LookupForAdd(aName)) {
    pool = entry.Data();
    if (NS_FAILED(pool->EnsureThreadLimitIsAtLeast(aThreadLimit))) {
      NS_WARNING("Failed to set limits on thread pool");
    }
  } else {
    nsCOMPtr<nsIThreadPool> threadPool(CreateThreadPool(aName));
    if (NS_WARN_IF(!threadPool)) {
      sPools->Remove(aName); // XXX entry.Remove()
      return nullptr;
    }
    pool = new SharedThreadPool(aName, threadPool);

    // Set the thread and idle limits. Note that we don't rely on the
    // EnsureThreadLimitIsAtLeast() call below, as the default thread limit
    // is 4, and if aThreadLimit is less than 4 we'll end up with a pool
    // with 4 threads rather than what we expected; so we'll have unexpected
    // behaviour.
    rv = pool->SetThreadLimit(aThreadLimit);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      sPools->Remove(aName); // XXX entry.Remove()
      return nullptr;
    }

    rv = pool->SetIdleThreadLimit(aThreadLimit);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      sPools->Remove(aName); // XXX entry.Remove()
      return nullptr;
    }

    entry.OrInsert([pool] () { return pool.get(); });
  }

  return pool.forget();
}

NS_IMETHODIMP_(MozExternalRefCountType) SharedThreadPool::AddRef(void)
{
  MOZ_ASSERT(sMonitor);
  ReentrantMonitorAutoEnter mon(*sMonitor);
  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
  nsrefcnt count = ++mRefCnt;
  NS_LOG_ADDREF(this, count, "SharedThreadPool", sizeof(*this));
  return count;
}

NS_IMETHODIMP_(MozExternalRefCountType) SharedThreadPool::Release(void)
{
  MOZ_ASSERT(sMonitor);
  ReentrantMonitorAutoEnter mon(*sMonitor);
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "SharedThreadPool");
  if (count) {
    return count;
  }

  // Remove SharedThreadPool from table of pools.
  sPools->Remove(mName);
  MOZ_ASSERT(!sPools->Get(mName));

  // Dispatch an event to the main thread to call Shutdown() on
  // the nsIThreadPool. The Runnable here will add a refcount to the pool,
  // and when the Runnable releases the nsIThreadPool it will be deleted.
  NS_DispatchToMainThread(NewRunnableMethod(
    "nsIThreadPool::Shutdown", mPool, &nsIThreadPool::Shutdown));

  // Stabilize refcount, so that if something in the dtor QIs, it won't explode.
  mRefCnt = 1;
  delete this;
  return 0;
}

NS_IMPL_QUERY_INTERFACE(SharedThreadPool, nsIThreadPool, nsIEventTarget)

SharedThreadPool::SharedThreadPool(const nsCString& aName,
                                   nsIThreadPool* aPool)
  : mName(aName)
  , mPool(aPool)
  , mRefCnt(0)
{
  mEventTarget = do_QueryInterface(aPool);
}

SharedThreadPool::~SharedThreadPool()
{
}

nsresult
SharedThreadPool::EnsureThreadLimitIsAtLeast(uint32_t aLimit)
{
  // We limit the number of threads that we use. Note that we
  // set the thread limit to the same as the idle limit so that we're not
  // constantly creating and destroying threads (see Bug 881954). When the
  // thread pool threads shutdown they dispatch an event to the main thread
  // to call nsIThread::Shutdown(), and if we're very busy that can take a
  // while to run, and we end up with dozens of extra threads. Note that
  // threads that are idle for 60 seconds are shutdown naturally.
  uint32_t existingLimit = 0;
  nsresult rv;

  rv = mPool->GetThreadLimit(&existingLimit);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aLimit > existingLimit) {
    rv = mPool->SetThreadLimit(aLimit);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mPool->GetIdleThreadLimit(&existingLimit);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aLimit > existingLimit) {
    rv = mPool->SetIdleThreadLimit(aLimit);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

static already_AddRefed<nsIThreadPool>
CreateThreadPool(const nsCString& aName)
{
  nsresult rv;
  nsCOMPtr<nsIThreadPool> pool = do_CreateInstance(NS_THREADPOOL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = pool->SetName(aName);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = pool->SetThreadStackSize(nsIThreadManager::kThreadPoolStackSize);
  NS_ENSURE_SUCCESS(rv, nullptr);

#ifdef XP_WIN
  // Ensure MSCOM is initialized on the thread pools threads.
  nsCOMPtr<nsIThreadPoolListener> listener = new MSCOMInitThreadPoolListener();
  rv = pool->SetListener(listener);
  NS_ENSURE_SUCCESS(rv, nullptr);
#endif

  return pool.forget();
}

} // namespace mozilla
