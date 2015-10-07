/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 ;*; */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "nsIObserverService.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "SchedulingContextService.h"

#include "mozilla/Atomics.h"
#include "mozilla/Services.h"

#include "mozilla/net/PSpdyPush.h"

namespace mozilla {
namespace net {

// nsISchedulingContext
class SchedulingContext final : public nsISchedulingContext
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISCHEDULINGCONTEXT

  explicit SchedulingContext(const nsID& id);
private:
  virtual ~SchedulingContext();

  nsID mID;
  char mCID[NSID_LENGTH];
  Atomic<uint32_t>       mBlockingTransactionCount;
  nsAutoPtr<SpdyPushCache> mSpdyCache;
};

NS_IMPL_ISUPPORTS(SchedulingContext, nsISchedulingContext)

SchedulingContext::SchedulingContext(const nsID& aID)
  : mBlockingTransactionCount(0)
{
  mID = aID;
  mID.ToProvidedString(mCID);
}

SchedulingContext::~SchedulingContext()
{
}

NS_IMETHODIMP
SchedulingContext::GetBlockingTransactionCount(uint32_t *aBlockingTransactionCount)
{
  NS_ENSURE_ARG_POINTER(aBlockingTransactionCount);
  *aBlockingTransactionCount = mBlockingTransactionCount;
  return NS_OK;
}

NS_IMETHODIMP
SchedulingContext::AddBlockingTransaction()
{
  mBlockingTransactionCount++;
  return NS_OK;
}

NS_IMETHODIMP
SchedulingContext::RemoveBlockingTransaction(uint32_t *outval)
{
  NS_ENSURE_ARG_POINTER(outval);
  mBlockingTransactionCount--;
  *outval = mBlockingTransactionCount;
  return NS_OK;
}

NS_IMETHODIMP
SchedulingContext::GetSpdyPushCache(mozilla::net::SpdyPushCache **aSpdyPushCache)
{
  *aSpdyPushCache = mSpdyCache.get();
  return NS_OK;
}

NS_IMETHODIMP
SchedulingContext::SetSpdyPushCache(mozilla::net::SpdyPushCache *aSpdyPushCache)
{
  mSpdyCache = aSpdyPushCache;
  return NS_OK;
}

NS_IMETHODIMP
SchedulingContext::GetID(nsID *outval)
{
  NS_ENSURE_ARG_POINTER(outval);
  *outval = mID;
  return NS_OK;
}

//nsISchedulingContextService
SchedulingContextService *SchedulingContextService::sSelf = nullptr;

NS_IMPL_ISUPPORTS(SchedulingContextService, nsISchedulingContextService, nsIObserver)

SchedulingContextService::SchedulingContextService()
{
  MOZ_ASSERT(!sSelf, "multiple scs instances!");
  MOZ_ASSERT(NS_IsMainThread());
  sSelf = this;
}

SchedulingContextService::~SchedulingContextService()
{
  MOZ_ASSERT(NS_IsMainThread());
  Shutdown();
  sSelf = nullptr;
}

nsresult
SchedulingContextService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}

void
SchedulingContextService::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  mTable.Clear();
}

/* static */ nsresult
SchedulingContextService::Create(nsISupports *aOuter, const nsIID& aIID, void **aResult)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }

  RefPtr<SchedulingContextService> svc = new SchedulingContextService();
  nsresult rv = svc->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return svc->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
SchedulingContextService::GetSchedulingContext(const nsID& scID, nsISchedulingContext **sc)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(sc);
  *sc = nullptr;

  if (!mTable.Get(scID, sc)) {
    nsCOMPtr<nsISchedulingContext> newSC = new SchedulingContext(scID);
    mTable.Put(scID, newSC);
    newSC.swap(*sc);
  }

  return NS_OK;
}

NS_IMETHODIMP
SchedulingContextService::NewSchedulingContextID(nsID *scID)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mUUIDGen) {
    nsresult rv;
    mUUIDGen = do_GetService("@mozilla.org/uuid-generator;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return mUUIDGen->GenerateUUIDInPlace(scID);
}

NS_IMETHODIMP
SchedulingContextService::RemoveSchedulingContext(const nsID& scID)
{
  MOZ_ASSERT(NS_IsMainThread());
  mTable.Remove(scID);
  return NS_OK;
}

NS_IMETHODIMP
SchedulingContextService::Observe(nsISupports *subject, const char *topic,
                                  const char16_t *data_unicode)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, topic)) {
    Shutdown();
  }

  return NS_OK;
}

} // ::mozilla::net
} // ::mozilla
