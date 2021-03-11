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
#include "nsQueryObject.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/DNSListenerProxy.h"
#include "mozilla/net/TRRServiceParent.h"
#include "nsHostResolver.h"
#include "nsServiceManagerUtils.h"
#include "prsystem.h"
#include "DNSResolverInfo.h"

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
    if (NS_WARN_IF(!NS_IsMainThread())) {
      return nullptr;
    }
    gChildDNSService = new ChildDNSService();
    gChildDNSService->Init();
    ClearOnShutdown(&gChildDNSService);
  }

  return do_AddRef(gChildDNSService);
}

NS_IMPL_ISUPPORTS(ChildDNSService, nsIDNSService, nsPIDNSService, nsIObserver)

ChildDNSService::ChildDNSService()
    : mPendingRequestsLock("DNSPendingRequestsLock") {
  MOZ_ASSERT_IF(nsIOService::UseSocketProcess(),
                XRE_IsContentProcess() || XRE_IsParentProcess());
  MOZ_ASSERT_IF(!nsIOService::UseSocketProcess(),
                XRE_IsContentProcess() || XRE_IsSocketProcess());
  if (XRE_IsParentProcess() && nsIOService::UseSocketProcess()) {
    nsDNSPrefetch::Initialize(this);
    mTRRServiceParent = new TRRServiceParent();
    mTRRServiceParent->Init();
  }
}

void ChildDNSService::GetDNSRecordHashKey(
    const nsACString& aHost, const nsACString& aTrrServer, uint16_t aType,
    const OriginAttributes& aOriginAttributes, uint32_t aFlags,
    uintptr_t aListenerAddr, nsACString& aHashKey) {
  aHashKey.Assign(aHost);
  aHashKey.Assign(aTrrServer);
  aHashKey.AppendInt(aType);

  nsAutoCString originSuffix;
  aOriginAttributes.CreateSuffix(originSuffix);
  aHashKey.Append(originSuffix);

  aHashKey.AppendInt(aFlags);
  aHashKey.AppendPrintf("0x%" PRIxPTR, aListenerAddr);
}

nsresult ChildDNSService::AsyncResolveInternal(
    const nsACString& hostname, uint16_t type, uint32_t flags,
    nsIDNSResolverInfo* aResolver, nsIDNSListener* listener,
    nsIEventTarget* target_, const OriginAttributes& aOriginAttributes,
    nsICancelable** result) {
  if (XRE_IsContentProcess()) {
    NS_ENSURE_TRUE(gNeckoChild != nullptr, NS_ERROR_FAILURE);
  }

  bool resolveDNSInSocketProcess = false;
  if (XRE_IsParentProcess() && nsIOService::UseSocketProcess()) {
    resolveDNSInSocketProcess = true;
    if (type != nsIDNSService::RESOLVE_TYPE_DEFAULT &&
        (mTRRServiceParent->Mode() != nsIDNSService::MODE_TRRFIRST &&
         mTRRServiceParent->Mode() != nsIDNSService::MODE_TRRONLY)) {
      return NS_ERROR_UNKNOWN_HOST;
    }
  }

  if (mDisablePrefetch && (flags & RESOLVE_SPECULATE)) {
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
  }

  // We need original listener for the pending requests hash.
  uintptr_t originalListenerAddr = reinterpret_cast<uintptr_t>(listener);

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

  RefPtr<DNSRequestSender> sender =
      new DNSRequestSender(hostname, DNSResolverInfo::URL(aResolver), type,
                           aOriginAttributes, flags, listener, target);
  RefPtr<DNSRequestActor> dnsReq;
  if (resolveDNSInSocketProcess) {
    dnsReq = new DNSRequestParent(sender);
  } else {
    dnsReq = new DNSRequestChild(sender);
  }

  {
    MutexAutoLock lock(mPendingRequestsLock);
    nsCString key;
    GetDNSRecordHashKey(hostname, DNSResolverInfo::URL(aResolver), type,
                        aOriginAttributes, flags, originalListenerAddr, key);
    mPendingRequests.WithEntryHandle(key, [&](auto&& entry) {
      entry
          .OrInsertWith(
              [] { return MakeUnique<nsTArray<RefPtr<DNSRequestSender>>>(); })
          ->AppendElement(sender);
    });
  }

  sender->StartRequest();

  sender.forget(result);
  return NS_OK;
}

nsresult ChildDNSService::CancelAsyncResolveInternal(
    const nsACString& aHostname, uint16_t aType, uint32_t aFlags,
    nsIDNSResolverInfo* aResolver, nsIDNSListener* aListener, nsresult aReason,
    const OriginAttributes& aOriginAttributes) {
  if (mDisablePrefetch && (aFlags & RESOLVE_SPECULATE)) {
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
  }

  MutexAutoLock lock(mPendingRequestsLock);
  nsTArray<RefPtr<DNSRequestSender>>* hashEntry;
  nsCString key;
  uintptr_t listenerAddr = reinterpret_cast<uintptr_t>(aListener);
  GetDNSRecordHashKey(aHostname, DNSResolverInfo::URL(aResolver), aType,
                      aOriginAttributes, aFlags, listenerAddr, key);
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
ChildDNSService::AsyncResolve(const nsACString& hostname,
                              nsIDNSService::ResolveType aType, uint32_t flags,
                              nsIDNSResolverInfo* aResolver,
                              nsIDNSListener* listener, nsIEventTarget* target_,
                              JS::HandleValue aOriginAttributes, JSContext* aCx,
                              uint8_t aArgc, nsICancelable** result) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return AsyncResolveInternal(hostname, aType, flags, aResolver, listener,
                              target_, attrs, result);
}

NS_IMETHODIMP
ChildDNSService::AsyncResolveNative(
    const nsACString& hostname, nsIDNSService::ResolveType aType,
    uint32_t flags, nsIDNSResolverInfo* aResolver, nsIDNSListener* listener,
    nsIEventTarget* target_, const OriginAttributes& aOriginAttributes,
    nsICancelable** result) {
  return AsyncResolveInternal(hostname, aType, flags, aResolver, listener,
                              target_, aOriginAttributes, result);
}

NS_IMETHODIMP
ChildDNSService::NewTRRResolverInfo(const nsACString& aTrrURL,
                                    nsIDNSResolverInfo** aResolver) {
  RefPtr<DNSResolverInfo> res = new DNSResolverInfo(aTrrURL);
  res.forget(aResolver);
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolve(const nsACString& aHostname,
                                    nsIDNSService::ResolveType aType,
                                    uint32_t aFlags,
                                    nsIDNSResolverInfo* aResolver,
                                    nsIDNSListener* aListener, nsresult aReason,
                                    JS::HandleValue aOriginAttributes,
                                    JSContext* aCx, uint8_t aArgc) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return CancelAsyncResolveInternal(aHostname, aType, aFlags, aResolver,
                                    aListener, aReason, attrs);
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolveNative(
    const nsACString& aHostname, nsIDNSService::ResolveType aType,
    uint32_t aFlags, nsIDNSResolverInfo* aResolver, nsIDNSListener* aListener,
    nsresult aReason, const OriginAttributes& aOriginAttributes) {
  return CancelAsyncResolveInternal(aHostname, aType, aFlags, aResolver,
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
ChildDNSService::ClearCache(bool aTrrToo) {
  if (!mTRRServiceParent || !mTRRServiceParent->CanSend()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  Unused << mTRRServiceParent->SendClearDNSCache(aTrrToo);
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::ReloadParentalControlEnabled() {
  if (!mTRRServiceParent) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mTRRServiceParent->UpdateParentalControlEnabled();
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::SetDetectedTrrURI(const nsACString& aURI) {
  if (!mTRRServiceParent) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mTRRServiceParent->SetDetectedTrrURI(aURI);
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::GetCurrentTrrURI(nsACString& aURI) {
  if (!mTRRServiceParent) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mTRRServiceParent->GetTrrURI(aURI);
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::GetCurrentTrrMode(nsIDNSService::ResolverMode* aMode) {
  if (!mTRRServiceParent) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aMode = mTRRServiceParent->Mode();
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::GetMyHostName(nsACString& result) {
  if (XRE_IsParentProcess()) {
    char name[100];
    if (PR_GetSystemInfo(PR_SI_HOSTNAME, name, sizeof(name)) == PR_SUCCESS) {
      result = name;
      return NS_OK;
    }

    return NS_ERROR_FAILURE;
  }
  // TODO: get value from parent during PNecko construction?
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ChildDNSService::GetODoHActivated(bool* aResult) {
  NS_ENSURE_ARG(aResult);

  *aResult = mODoHActivated;
  return NS_OK;
}

void ChildDNSService::NotifyRequestDone(DNSRequestSender* aDnsRequest) {
  // We need the original flags and listener for the pending requests hash.
  uint32_t originalFlags = aDnsRequest->mFlags & ~RESOLVE_OFFLINE;
  uintptr_t originalListenerAddr =
      reinterpret_cast<uintptr_t>(aDnsRequest->mListener.get());
  RefPtr<DNSListenerProxy> wrapper = do_QueryObject(aDnsRequest->mListener);
  if (wrapper) {
    originalListenerAddr = wrapper->GetOriginalListenerAddress();
  }

  MutexAutoLock lock(mPendingRequestsLock);

  nsCString key;
  GetDNSRecordHashKey(aDnsRequest->mHost, aDnsRequest->mTrrServer,
                      aDnsRequest->mType, aDnsRequest->mOriginAttributes,
                      originalFlags, originalListenerAddr, key);

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

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
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

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      observerService->AddObserver(this, "odoh-service-activated", false);
    }
  }

  mDisablePrefetch =
      disablePrefetch || (StaticPrefs::network_proxy_type() ==
                          nsIProtocolProxyService::PROXYCONFIG_MANUAL);

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

NS_IMETHODIMP
ChildDNSService::ReportFailedSVCDomainName(const nsACString& aOwnerName,
                                           const nsACString& aSVCDomainName) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ChildDNSService::IsSVCDomainNameFailed(const nsACString& aOwnerName,
                                       const nsACString& aSVCDomainName,
                                       bool* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ChildDNSService::ResetExcludedSVCDomainName(const nsACString& aOwnerName) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// ChildDNSService::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ChildDNSService::Observe(nsISupports* subject, const char* topic,
                         const char16_t* data) {
  if (!strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    // Reread prefs
    Init();
  } else if (!strcmp(topic, "odoh-service-activated")) {
    mODoHActivated = u"true"_ns.Equals(data);
  }

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
