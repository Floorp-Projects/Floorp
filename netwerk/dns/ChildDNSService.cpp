/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/ChildDNSService.h"
#include "nsDNSPrefetch.h"
#include "nsIDNSListener.h"
#include "nsIOService.h"
#include "nsThreadUtils.h"
#include "nsIXPConnect.h"
#include "nsIProtocolProxyService.h"
#include "nsNetCID.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/DNSListenerProxy.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// ChildDNSService
//-----------------------------------------------------------------------------

static StaticRefPtr<ChildDNSService> gChildDNSService;
static const char kPrefNameDisablePrefetch[] = "network.dns.disablePrefetch";

already_AddRefed<ChildDNSService> ChildDNSService::GetSingleton() {
  MOZ_ASSERT_IF(nsIOService::UseSocketProcess(),
                XRE_IsContentProcess() || XRE_IsParentProcess());
  MOZ_ASSERT_IF(!nsIOService::UseSocketProcess(),
                XRE_IsContentProcess() || XRE_IsSocketProcess());

  if (!gChildDNSService) {
    gChildDNSService = new ChildDNSService();
    ClearOnShutdown(&gChildDNSService);
  }

  return do_AddRef(gChildDNSService);
}

NS_IMPL_ISUPPORTS(ChildDNSService, nsIDNSService, nsPIDNSService, nsIObserver)

ChildDNSService::ChildDNSService()
    : mFirstTime(true),
      mDisablePrefetch(false),
      mPendingRequestsLock("DNSPendingRequestsLock") {
  MOZ_ASSERT_IF(nsIOService::UseSocketProcess(),
                XRE_IsContentProcess() || XRE_IsParentProcess());
  MOZ_ASSERT_IF(!nsIOService::UseSocketProcess(),
                XRE_IsContentProcess() || XRE_IsSocketProcess());
  if (XRE_IsParentProcess() && nsIOService::UseSocketProcess()) {
    nsDNSPrefetch::Initialize(this);
  }
}

void ChildDNSService::GetDNSRecordHashKey(
    const nsACString& aHost, const nsACString& aTrrServer, uint16_t aType,
    const OriginAttributes& aOriginAttributes, uint32_t aFlags,
    nsIDNSListener* aListener, nsACString& aHashKey) {
  aHashKey.Assign(aHost);
  aHashKey.Assign(aTrrServer);
  aHashKey.AppendInt(aType);

  nsAutoCString originSuffix;
  aOriginAttributes.CreateSuffix(originSuffix);
  aHashKey.Append(originSuffix);

  aHashKey.AppendInt(aFlags);
  aHashKey.AppendPrintf("%p", aListener);
}

nsresult ChildDNSService::AsyncResolveInternal(
    const nsACString& hostname, const nsACString& aTrrServer, uint16_t type,
    uint32_t flags, nsIDNSListener* listener, nsIEventTarget* target_,
    const OriginAttributes& aOriginAttributes, nsICancelable** result) {
  if (XRE_IsContentProcess()) {
    NS_ENSURE_TRUE(gNeckoChild != nullptr, NS_ERROR_FAILURE);
  }

  bool resolveDNSInSocketProcess = false;
  if (XRE_IsParentProcess() && nsIOService::UseSocketProcess()) {
    resolveDNSInSocketProcess = true;
  }

  if (mDisablePrefetch && (flags & RESOLVE_SPECULATE)) {
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
  }

  // We need original listener for the pending requests hash.
  nsIDNSListener* originalListener = listener;

  // make sure JS callers get notification on the main thread
  nsCOMPtr<nsIEventTarget> target = target_;
  nsCOMPtr<nsIXPConnectWrappedJS> wrappedListener = do_QueryInterface(listener);
  if (wrappedListener && !target) {
    target = GetMainThreadSerialEventTarget();
  }
  if (target) {
    // Guarantee listener freed on main thread.  Not sure we need this in child
    // (or in parent in nsDNSService.cpp) but doesn't hurt.
    listener = new DNSListenerProxy(listener, target);
  }

  RefPtr<DNSRequestSender> sender = new DNSRequestSender(
      hostname, aTrrServer, type, aOriginAttributes, flags, listener, target);
  RefPtr<DNSRequestActor> dnsReq;
  if (resolveDNSInSocketProcess) {
    dnsReq = new DNSRequestParent(sender);
  } else {
    dnsReq = new DNSRequestChild(sender);
  }

  {
    MutexAutoLock lock(mPendingRequestsLock);
    nsCString key;
    GetDNSRecordHashKey(hostname, aTrrServer, type, aOriginAttributes, flags,
                        originalListener, key);
    auto entry = mPendingRequests.LookupForAdd(key);
    if (entry) {
      entry.Data()->AppendElement(sender);
    } else {
      entry.OrInsert([&]() {
        auto* hashEntry = new nsTArray<RefPtr<DNSRequestSender>>();
        hashEntry->AppendElement(sender);
        return hashEntry;
      });
    }
  }

  sender->StartRequest();

  sender.forget(result);
  return NS_OK;
}

nsresult ChildDNSService::CancelAsyncResolveInternal(
    const nsACString& aHostname, const nsACString& aTrrServer, uint16_t aType,
    uint32_t aFlags, nsIDNSListener* aListener, nsresult aReason,
    const OriginAttributes& aOriginAttributes) {
  if (mDisablePrefetch && (aFlags & RESOLVE_SPECULATE)) {
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
  }

  MutexAutoLock lock(mPendingRequestsLock);
  nsTArray<RefPtr<DNSRequestSender>>* hashEntry;
  nsCString key;
  GetDNSRecordHashKey(aHostname, aTrrServer, aType, aOriginAttributes, aFlags,
                      aListener, key);
  if (mPendingRequests.Get(key, &hashEntry)) {
    // We cancel just one.
    hashEntry->ElementAt(0)->Cancel(aReason);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// ChildDNSService::nsIDNSService
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ChildDNSService::AsyncResolve(const nsACString& hostname, uint32_t flags,
                              nsIDNSListener* listener, nsIEventTarget* target_,
                              JS::HandleValue aOriginAttributes, JSContext* aCx,
                              uint8_t aArgc, nsICancelable** result) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return AsyncResolveInternal(hostname, EmptyCString(),
                              nsIDNSService::RESOLVE_TYPE_DEFAULT, flags,
                              listener, target_, attrs, result);
}

NS_IMETHODIMP
ChildDNSService::AsyncResolveNative(const nsACString& hostname, uint32_t flags,
                                    nsIDNSListener* listener,
                                    nsIEventTarget* target_,
                                    const OriginAttributes& aOriginAttributes,
                                    nsICancelable** result) {
  return AsyncResolveInternal(hostname, EmptyCString(),
                              nsIDNSService::RESOLVE_TYPE_DEFAULT, flags,
                              listener, target_, aOriginAttributes, result);
}

NS_IMETHODIMP
ChildDNSService::AsyncResolveWithTrrServer(
    const nsACString& hostname, const nsACString& trrServer, uint32_t flags,
    nsIDNSListener* listener, nsIEventTarget* target_,
    JS::HandleValue aOriginAttributes, JSContext* aCx, uint8_t aArgc,
    nsICancelable** result) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return AsyncResolveInternal(hostname, trrServer,
                              nsIDNSService::RESOLVE_TYPE_DEFAULT, flags,
                              listener, target_, attrs, result);
}

NS_IMETHODIMP
ChildDNSService::AsyncResolveWithTrrServerNative(
    const nsACString& hostname, const nsACString& trrServer, uint32_t flags,
    nsIDNSListener* listener, nsIEventTarget* target_,
    const OriginAttributes& aOriginAttributes, nsICancelable** result) {
  return AsyncResolveInternal(hostname, trrServer,
                              nsIDNSService::RESOLVE_TYPE_DEFAULT, flags,
                              listener, target_, aOriginAttributes, result);
}

NS_IMETHODIMP
ChildDNSService::AsyncResolveByType(const nsACString& hostname, uint16_t type,
                                    uint32_t flags, nsIDNSListener* listener,
                                    nsIEventTarget* target_,
                                    JS::HandleValue aOriginAttributes,
                                    JSContext* aCx, uint8_t aArgc,
                                    nsICancelable** result) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return AsyncResolveInternal(hostname, EmptyCString(), type, flags, listener,
                              target_, attrs, result);
}

NS_IMETHODIMP
ChildDNSService::AsyncResolveByTypeNative(
    const nsACString& hostname, uint16_t type, uint32_t flags,
    nsIDNSListener* listener, nsIEventTarget* target_,
    const OriginAttributes& aOriginAttributes, nsICancelable** result) {
  return AsyncResolveInternal(hostname, EmptyCString(), type, flags, listener,
                              target_, aOriginAttributes, result);
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolve(const nsACString& aHostname,
                                    uint32_t aFlags, nsIDNSListener* aListener,
                                    nsresult aReason,
                                    JS::HandleValue aOriginAttributes,
                                    JSContext* aCx, uint8_t aArgc) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return CancelAsyncResolveInternal(aHostname, EmptyCString(),
                                    nsIDNSService::RESOLVE_TYPE_DEFAULT, aFlags,
                                    aListener, aReason, attrs);
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolveNative(
    const nsACString& aHostname, uint32_t aFlags, nsIDNSListener* aListener,
    nsresult aReason, const OriginAttributes& aOriginAttributes) {
  return CancelAsyncResolveInternal(aHostname, EmptyCString(),
                                    nsIDNSService::RESOLVE_TYPE_DEFAULT, aFlags,
                                    aListener, aReason, aOriginAttributes);
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolveWithTrrServer(
    const nsACString& aHostname, const nsACString& aTrrServer, uint32_t aFlags,
    nsIDNSListener* aListener, nsresult aReason,
    JS::HandleValue aOriginAttributes, JSContext* aCx, uint8_t aArgc) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return CancelAsyncResolveInternal(aHostname, aTrrServer,
                                    nsIDNSService::RESOLVE_TYPE_DEFAULT, aFlags,
                                    aListener, aReason, attrs);
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolveWithTrrServerNative(
    const nsACString& aHostname, const nsACString& aTrrServer, uint32_t aFlags,
    nsIDNSListener* aListener, nsresult aReason,
    const OriginAttributes& aOriginAttributes) {
  return CancelAsyncResolveInternal(aHostname, aTrrServer,
                                    nsIDNSService::RESOLVE_TYPE_DEFAULT, aFlags,
                                    aListener, aReason, aOriginAttributes);
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolveByType(const nsACString& aHostname,
                                          uint16_t aType, uint32_t aFlags,
                                          nsIDNSListener* aListener,
                                          nsresult aReason,
                                          JS::HandleValue aOriginAttributes,
                                          JSContext* aCx, uint8_t aArgc) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return CancelAsyncResolveInternal(aHostname, EmptyCString(), aType, aFlags,
                                    aListener, aReason, attrs);
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolveByTypeNative(
    const nsACString& aHostname, uint16_t aType, uint32_t aFlags,
    nsIDNSListener* aListener, nsresult aReason,
    const OriginAttributes& aOriginAttributes) {
  return CancelAsyncResolveInternal(aHostname, EmptyCString(), aType, aFlags,
                                    aListener, aReason, aOriginAttributes);
}

NS_IMETHODIMP
ChildDNSService::Resolve(const nsACString& hostname, uint32_t flags,
                         JS::HandleValue aOriginAttributes, JSContext* aCx,
                         uint8_t aArgc, nsIDNSRecord** result) {
  // not planning to ever support this, since sync IPDL is evil.
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ChildDNSService::ResolveNative(const nsACString& hostname, uint32_t flags,
                               const OriginAttributes& aOriginAttributes,
                               nsIDNSRecord** result) {
  // not planning to ever support this, since sync IPDL is evil.
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ChildDNSService::GetDNSCacheEntries(
    nsTArray<mozilla::net::DNSCacheEntries>* args) {
  // Only used by networking dashboard, so may not ever need this in child.
  // (and would provide a way to spy on what hosts other apps are connecting to,
  // unless we start keeping per-app DNS caches).
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ChildDNSService::ClearCache(bool aTrrToo) { return NS_ERROR_NOT_AVAILABLE; }

NS_IMETHODIMP
ChildDNSService::ReloadParentalControlEnabled() {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ChildDNSService::GetMyHostName(nsACString& result) {
  // TODO: get value from parent during PNecko construction?
  return NS_ERROR_NOT_AVAILABLE;
}

void ChildDNSService::NotifyRequestDone(DNSRequestSender* aDnsRequest) {
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
  GetDNSRecordHashKey(aDnsRequest->mHost, aDnsRequest->mTrrServer,
                      aDnsRequest->mType, aDnsRequest->mOriginAttributes,
                      originalFlags, originalListener, key);

  nsTArray<RefPtr<DNSRequestSender>>* hashEntry;

  if (mPendingRequests.Get(key, &hashEntry)) {
    auto idx = hashEntry->IndexOf(aDnsRequest);
    if (idx != nsTArray<RefPtr<DNSRequestSender>>::NoIndex) {
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

nsresult ChildDNSService::Init() {
  // Disable prefetching either by explicit preference or if a manual proxy
  // is configured
  bool disablePrefetch = false;
  int proxyType = nsIProtocolProxyService::PROXYCONFIG_DIRECT;

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
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

nsresult ChildDNSService::Shutdown() { return NS_OK; }

NS_IMETHODIMP
ChildDNSService::GetPrefetchEnabled(bool* outVal) {
  *outVal = !mDisablePrefetch;
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::SetPrefetchEnabled(bool inVal) {
  mDisablePrefetch = !inVal;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// ChildDNSService::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ChildDNSService::Observe(nsISupports* subject, const char* topic,
                         const char16_t* data) {
  // we are only getting called if a preference has changed.
  NS_ASSERTION(strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0,
               "unexpected observe call");

  // Reread prefs
  Init();
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
