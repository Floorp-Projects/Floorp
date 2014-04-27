/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/ChildDNSService.h"
#include "nsIDNSListener.h"
#include "nsNetUtil.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsIXPConnect.h"
#include "nsIPrefService.h"
#include "nsIProtocolProxyService.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/DNSRequestChild.h"
#include "mozilla/net/DNSListenerProxy.h"

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
{
  MOZ_ASSERT(IsNeckoChild());
}

ChildDNSService::~ChildDNSService()
{

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
  NS_ENSURE_TRUE(gNeckoChild != nullptr, NS_ERROR_FAILURE);

  if (mDisablePrefetch && (flags & RESOLVE_SPECULATE)) {
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
  }

  // Support apps being 'offline' even if parent is not: avoids DNS traffic by
  // apps that have been told they are offline.
  if (mOffline) {
    flags |= RESOLVE_OFFLINE;
  }

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
    new DNSRequestChild(nsCString(hostname), flags, listener, target);

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
  if (mDisablePrefetch && (aFlags & RESOLVE_SPECULATE)) {
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
  }

  // TODO: keep a hashtable of pending requests, so we can obey cancel semantics
  // (call OnLookupComplete with aReason).  Also possible we could send IPDL to
  // parent to cancel.
  return NS_ERROR_NOT_AVAILABLE;
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
