/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/ChildDNSService.h"
#include "nsIDNSListener.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsIXPConnect.h"
#include "nsIPrefService.h"
#include "nsIProtocolProxyService.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/DNSListenerProxy.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// ChildDNSService
//-----------------------------------------------------------------------------

static ChildDNSService *gChildDNSService;
static const char kPrefNameDisablePrefetch[] = "network.dns.disablePrefetch";

ChildDNSService* ChildDNSService::GetSingleton()
{
  MOZ_ASSERT(IsNeckoChild());

  if (!gChildDNSService) {
    gChildDNSService = new ChildDNSService();
  }

  NS_ADDREF(gChildDNSService);
  return gChildDNSService;
}

NS_IMPL_ISUPPORTS(ChildDNSService,
                  nsIDNSService,
                  nsPIDNSService,
                  nsIObserver)

ChildDNSService::ChildDNSService()
  : mFirstTime(true)
  , mOffline(false)
  , mPendingRequestsLock("DNSPendingRequestsLock")
{
  MOZ_ASSERT(IsNeckoChild());
}

ChildDNSService::~ChildDNSService()
{

}

void
ChildDNSService::GetDNSRecordHashKey(const nsACString &aHost,
                                     uint32_t aFlags,
                                     const nsACString &aNetworkInterface,
                                     nsIDNSListener* aListener,
                                     nsACString &aHashKey)
{
  aHashKey.Assign(aHost);
  aHashKey.AppendInt(aFlags);
  if (!aNetworkInterface.IsEmpty()) {
    aHashKey.Append(aNetworkInterface);
  }
  aHashKey.AppendPrintf("%p", aListener);
}

//-----------------------------------------------------------------------------
// ChildDNSService::nsIDNSService
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ChildDNSService::AsyncResolve(const nsACString  &hostname,
                              uint32_t           flags,
                              nsIDNSListener    *listener,
                              nsIEventTarget    *target_,
                              nsICancelable    **result)
{
  return AsyncResolveExtended(hostname, flags, EmptyCString(), listener,
                              target_, result);
}

NS_IMETHODIMP
ChildDNSService::AsyncResolveExtended(const nsACString  &hostname,
                                      uint32_t           flags,
                                      const nsACString  &aNetworkInterface,
                                      nsIDNSListener    *listener,
                                      nsIEventTarget    *target_,
                                      nsICancelable    **result)
{
  NS_ENSURE_TRUE(gNeckoChild != nullptr, NS_ERROR_FAILURE);

  if (mDisablePrefetch && (flags & RESOLVE_SPECULATE)) {
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
  }

  // We need original flags for the pending requests hash.
  uint32_t originalFlags = flags;

  // Support apps being 'offline' even if parent is not: avoids DNS traffic by
  // apps that have been told they are offline.
  if (mOffline) {
    flags |= RESOLVE_OFFLINE;
  }

  // We need original listener for the pending requests hash.
  nsIDNSListener *originalListener = listener;

  // make sure JS callers get notification on the main thread
  nsCOMPtr<nsIEventTarget> target = target_;
  nsCOMPtr<nsIXPConnectWrappedJS> wrappedListener = do_QueryInterface(listener);
  if (wrappedListener && !target) {
    nsCOMPtr<nsIThread> mainThread;
    NS_GetMainThread(getter_AddRefs(mainThread));
    target = do_QueryInterface(mainThread);
  }
  if (target) {
    // Guarantee listener freed on main thread.  Not sure we need this in child
    // (or in parent in nsDNSService.cpp) but doesn't hurt.
    listener = new DNSListenerProxy(listener, target);
  }

  nsRefPtr<DNSRequestChild> childReq =
    new DNSRequestChild(nsCString(hostname), flags,
                        nsCString(aNetworkInterface),
                        listener, target);

  {
    MutexAutoLock lock(mPendingRequestsLock);
    nsCString key;
    GetDNSRecordHashKey(hostname, originalFlags, aNetworkInterface,
                        originalListener, key);
    nsTArray<nsRefPtr<DNSRequestChild>> *hashEntry;
    if (mPendingRequests.Get(key, &hashEntry)) {
      hashEntry->AppendElement(childReq);
    } else {
      hashEntry = new nsTArray<nsRefPtr<DNSRequestChild>>();
      hashEntry->AppendElement(childReq);
      mPendingRequests.Put(key, hashEntry);
    }
  }

  childReq->StartRequest();

  childReq.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolve(const nsACString  &aHostname,
                                    uint32_t           aFlags,
                                    nsIDNSListener    *aListener,
                                    nsresult           aReason)
{
  return CancelAsyncResolveExtended(aHostname, aFlags, EmptyCString(),
                                    aListener, aReason);
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolveExtended(const nsACString &aHostname,
                                            uint32_t          aFlags,
                                            const nsACString &aNetworkInterface,
                                            nsIDNSListener   *aListener,
                                            nsresult          aReason)
{
  if (mDisablePrefetch && (aFlags & RESOLVE_SPECULATE)) {
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
  }

  MutexAutoLock lock(mPendingRequestsLock);
  nsTArray<nsRefPtr<DNSRequestChild>> *hashEntry;
  nsCString key;
  GetDNSRecordHashKey(aHostname, aFlags, aNetworkInterface, aListener, key);
  if (mPendingRequests.Get(key, &hashEntry)) {
    // We cancel just one.
    hashEntry->ElementAt(0)->Cancel(aReason);
  }

  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::Resolve(const nsACString &hostname,
                         uint32_t          flags,
                         nsIDNSRecord    **result)
{
  // not planning to ever support this, since sync IPDL is evil.
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ChildDNSService::GetDNSCacheEntries(nsTArray<mozilla::net::DNSCacheEntries> *args)
{
  // Only used by networking dashboard, so may not ever need this in child.
  // (and would provide a way to spy on what hosts other apps are connecting to,
  // unless we start keeping per-app DNS caches).
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ChildDNSService::GetMyHostName(nsACString &result)
{
  // TODO: get value from parent during PNecko construction?
  return NS_ERROR_NOT_AVAILABLE;
}

void
ChildDNSService::NotifyRequestDone(DNSRequestChild *aDnsRequest)
{
  // We need the original flags and listener for the pending requests hash.
  uint32_t originalFlags = aDnsRequest->mFlags & ~RESOLVE_OFFLINE;
  nsCOMPtr<nsIDNSListener> originalListener = aDnsRequest->mListener;
  nsCOMPtr<nsIDNSListenerProxy> wrapper = do_QueryInterface(originalListener);
  if (wrapper) {
    wrapper->GetOriginalListener(getter_AddRefs(originalListener));
    if (NS_WARN_IF(!originalListener)) {
      MOZ_ASSERT(originalListener);
      return;
    }
  }

  MutexAutoLock lock(mPendingRequestsLock);

  nsCString key;
  GetDNSRecordHashKey(aDnsRequest->mHost, originalFlags,
                      aDnsRequest->mNetworkInterface, originalListener, key);

  nsTArray<nsRefPtr<DNSRequestChild>> *hashEntry;

  if (mPendingRequests.Get(key, &hashEntry)) {
    int idx;
    if ((idx = hashEntry->IndexOf(aDnsRequest))) {
      hashEntry->RemoveElementAt(idx);
      if (hashEntry->IsEmpty()) {
        mPendingRequests.Remove(key);
      }
    }
  }
}

//-----------------------------------------------------------------------------
// ChildDNSService::nsPIDNSService
//-----------------------------------------------------------------------------

nsresult
ChildDNSService::Init()
{
  // Disable prefetching either by explicit preference or if a manual proxy
  // is configured
  bool disablePrefetch = false;
  int  proxyType = nsIProtocolProxyService::PROXYCONFIG_DIRECT;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  prefs->GetBoolPref(kPrefNameDisablePrefetch, &disablePrefetch);
  if (prefs) {
    prefs->GetIntPref("network.proxy.type", &proxyType);
    prefs->GetBoolPref(kPrefNameDisablePrefetch, &disablePrefetch);
  }

  if (mFirstTime) {
    mFirstTime = false;
    if (prefs) {
      prefs->AddObserver(kPrefNameDisablePrefetch, this, false);

      // Monitor these to see if there is a change in proxy configuration
      // If a manual proxy is in use, disable prefetch implicitly
      prefs->AddObserver("network.proxy.type", this, false);
    }
  }

  mDisablePrefetch = disablePrefetch ||
                     (proxyType == nsIProtocolProxyService::PROXYCONFIG_MANUAL);

  return NS_OK;
}

nsresult
ChildDNSService::Shutdown()
{
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::GetPrefetchEnabled(bool *outVal)
{
  *outVal = !mDisablePrefetch;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::SetPrefetchEnabled(bool inVal)
{
  mDisablePrefetch = !inVal;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::GetOffline(bool* aResult)
{
  *aResult = mOffline;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::SetOffline(bool value)
{
  mOffline = value;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// ChildDNSService::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ChildDNSService::Observe(nsISupports *subject, const char *topic,
                         const char16_t *data)
{
  // we are only getting called if a preference has changed.
  NS_ASSERTION(strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0,
               "unexpected observe call");

  // Reread prefs
  Init();
  return NS_OK;
}

} // namespace net
} // namespace mozilla
