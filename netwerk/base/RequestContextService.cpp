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
#include "RequestContextService.h"

#include "mozilla/Atomics.h"
#include "mozilla/Services.h"

#include "mozilla/net/PSpdyPush.h"

namespace mozilla {
namespace net {

// nsIRequestContext
class RequestContext final : public nsIRequestContext
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTCONTEXT

  explicit RequestContext(const nsID& id);
private:
  virtual ~RequestContext();

  nsID mID;
  char mCID[NSID_LENGTH];
  Atomic<uint32_t>       mBlockingTransactionCount;
  nsAutoPtr<SpdyPushCache> mSpdyCache;
  nsCString mUserAgentOverride;
};

NS_IMPL_ISUPPORTS(RequestContext, nsIRequestContext)

RequestContext::RequestContext(const nsID& aID)
  : mBlockingTransactionCount(0)
{
  mID = aID;
  mID.ToProvidedString(mCID);
}

RequestContext::~RequestContext()
{
}

NS_IMETHODIMP
RequestContext::GetBlockingTransactionCount(uint32_t *aBlockingTransactionCount)
{
  NS_ENSURE_ARG_POINTER(aBlockingTransactionCount);
  *aBlockingTransactionCount = mBlockingTransactionCount;
  return NS_OK;
}

NS_IMETHODIMP
RequestContext::AddBlockingTransaction()
{
  mBlockingTransactionCount++;
  return NS_OK;
}

NS_IMETHODIMP
RequestContext::RemoveBlockingTransaction(uint32_t *outval)
{
  NS_ENSURE_ARG_POINTER(outval);
  mBlockingTransactionCount--;
  *outval = mBlockingTransactionCount;
  return NS_OK;
}

NS_IMETHODIMP
RequestContext::GetSpdyPushCache(mozilla::net::SpdyPushCache **aSpdyPushCache)
{
  *aSpdyPushCache = mSpdyCache.get();
  return NS_OK;
}

NS_IMETHODIMP
RequestContext::SetSpdyPushCache(mozilla::net::SpdyPushCache *aSpdyPushCache)
{
  mSpdyCache = aSpdyPushCache;
  return NS_OK;
}

NS_IMETHODIMP
RequestContext::GetID(nsID *outval)
{
  NS_ENSURE_ARG_POINTER(outval);
  *outval = mID;
  return NS_OK;
}

NS_IMETHODIMP
RequestContext::GetUserAgentOverride(nsACString& aUserAgentOverride)
{
  aUserAgentOverride = mUserAgentOverride;
  return NS_OK;
}

NS_IMETHODIMP
RequestContext::SetUserAgentOverride(const nsACString& aUserAgentOverride)
{
  mUserAgentOverride = aUserAgentOverride;
  return NS_OK;
}


//nsIRequestContextService
RequestContextService *RequestContextService::sSelf = nullptr;

NS_IMPL_ISUPPORTS(RequestContextService, nsIRequestContextService, nsIObserver)

RequestContextService::RequestContextService()
{
  MOZ_ASSERT(!sSelf, "multiple rcs instances!");
  MOZ_ASSERT(NS_IsMainThread());
  sSelf = this;
}

RequestContextService::~RequestContextService()
{
  MOZ_ASSERT(NS_IsMainThread());
  Shutdown();
  sSelf = nullptr;
}

nsresult
RequestContextService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}

void
RequestContextService::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  mTable.Clear();
}

/* static */ nsresult
RequestContextService::Create(nsISupports *aOuter, const nsIID& aIID, void **aResult)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (aOuter != nullptr) {
    return NS_ERROR_NO_AGGREGATION;
  }

  RefPtr<RequestContextService> svc = new RequestContextService();
  nsresult rv = svc->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return svc->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
RequestContextService::GetRequestContext(const nsID& rcID, nsIRequestContext **rc)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(rc);
  *rc = nullptr;

  if (!mTable.Get(rcID, rc)) {
    nsCOMPtr<nsIRequestContext> newSC = new RequestContext(rcID);
    mTable.Put(rcID, newSC);
    newSC.swap(*rc);
  }

  return NS_OK;
}

NS_IMETHODIMP
RequestContextService::NewRequestContext(nsIRequestContext **rc)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(rc);
  *rc = nullptr;

  nsresult rv;
  if (!mUUIDGen) {
    mUUIDGen = do_GetService("@mozilla.org/uuid-generator;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsID rcID;
  rv = mUUIDGen->GenerateUUIDInPlace(&rcID);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRequestContext> newSC = new RequestContext(rcID);
  mTable.Put(rcID, newSC);
  newSC.swap(*rc);

  return NS_OK;
}

NS_IMETHODIMP
RequestContextService::RemoveRequestContext(const nsID& rcID)
{
  MOZ_ASSERT(NS_IsMainThread());
  mTable.Remove(rcID);
  return NS_OK;
}

NS_IMETHODIMP
RequestContextService::Observe(nsISupports *subject, const char *topic,
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
