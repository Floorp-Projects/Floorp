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
#include "DNSAdditionalInfo.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// ChildDNSService
//-----------------------------------------------------------------------------

static StaticRefPtr<ChildDNSService> gChildDNSService;

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

NS_IMPL_ISUPPORTS_INHERITED(ChildDNSService, DNSServiceBase, nsIDNSService,
                            nsPIDNSService)

ChildDNSService::ChildDNSService() {
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
    const nsACString& aHost, const nsACString& aTrrServer, int32_t aPort,
    uint16_t aType, const OriginAttributes& aOriginAttributes, uint32_t aFlags,
    uintptr_t aListenerAddr, nsACString& aHashKey) {
  aHashKey.Assign(aHost);
  aHashKey.Assign(aTrrServer);
  aHashKey.AppendInt(aPort);
  aHashKey.AppendInt(aType);

  nsAutoCString originSuffix;
  aOriginAttributes.CreateSuffix(originSuffix);
  aHashKey.Append(originSuffix);

  aHashKey.AppendInt(aFlags);
  aHashKey.AppendPrintf("0x%" PRIxPTR, aListenerAddr);
}

nsresult ChildDNSService::AsyncResolveInternal(
    const nsACString& hostname, uint16_t type, uint32_t flags,
    nsIDNSAdditionalInfo* aInfo, nsIDNSListener* listener,
    nsIEventTarget* target_, const OriginAttributes& aOriginAttributes,
    nsICancelable** result) {
  if (XRE_IsContentProcess()) {
    NS_ENSURE_TRUE(gNeckoChild != nullptr, NS_ERROR_FAILURE);
  }

  if (DNSForbiddenByActiveProxy(hostname, flags)) {
    // nsHostResolver returns NS_ERROR_UNKNOWN_HOST for lots of reasons.
    // We use a different error code to differentiate this failure and to make
    // it clear(er) where this error comes from.
    return NS_ERROR_UNKNOWN_PROXY_HOST;
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

  RefPtr<DNSRequestSender> sender = new DNSRequestSender(
      hostname, DNSAdditionalInfo::URL(aInfo), DNSAdditionalInfo::Port(aInfo),
      type, aOriginAttributes, flags, listener, target);
  RefPtr<DNSRequestActor> dnsReq;
  if (resolveDNSInSocketProcess) {
    dnsReq = new DNSRequestParent(sender);
    if (!mTRRServiceParent->TRRConnectionInfoInited()) {
      mTRRServiceParent->InitTRRConnectionInfo();
    }
  } else {
    dnsReq = new DNSRequestChild(sender);
  }

  {
    MutexAutoLock lock(mPendingRequestsLock);
    nsCString key;
    GetDNSRecordHashKey(hostname, DNSAdditionalInfo::URL(aInfo),
                        DNSAdditionalInfo::Port(aInfo), type, aOriginAttributes,
                        flags, originalListenerAddr, key);
    mPendingRequests.GetOrInsertNew(key)->AppendElement(sender);
  }

  sender->StartRequest();

  sender.forget(result);
  return NS_OK;
}

nsresult ChildDNSService::CancelAsyncResolveInternal(
    const nsACString& aHostname, uint16_t aType, uint32_t aFlags,
    nsIDNSAdditionalInfo* aInfo, nsIDNSListener* aListener, nsresult aReason,
    const OriginAttributes& aOriginAttributes) {
  if (mDisablePrefetch && (aFlags & RESOLVE_SPECULATE)) {
    return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
  }

  MutexAutoLock lock(mPendingRequestsLock);
  nsTArray<RefPtr<DNSRequestSender>>* hashEntry;
  nsCString key;
  uintptr_t listenerAddr = reinterpret_cast<uintptr_t>(aListener);
  GetDNSRecordHashKey(aHostname, DNSAdditionalInfo::URL(aInfo),
                      DNSAdditionalInfo::Port(aInfo), aType, aOriginAttributes,
                      aFlags, listenerAddr, key);
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
                              nsIDNSAdditionalInfo* aInfo,
                              nsIDNSListener* listener, nsIEventTarget* target_,
                              JS::Handle<JS::Value> aOriginAttributes,
                              JSContext* aCx, uint8_t aArgc,
                              nsICancelable** result) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return AsyncResolveInternal(hostname, aType, flags, aInfo, listener, target_,
                              attrs, result);
}

NS_IMETHODIMP
ChildDNSService::AsyncResolveNative(const nsACString& hostname,
                                    nsIDNSService::ResolveType aType,
                                    uint32_t flags, nsIDNSAdditionalInfo* aInfo,
                                    nsIDNSListener* listener,
                                    nsIEventTarget* target_,
                                    const OriginAttributes& aOriginAttributes,
                                    nsICancelable** result) {
  return AsyncResolveInternal(hostname, aType, flags, aInfo, listener, target_,
                              aOriginAttributes, result);
}

NS_IMETHODIMP
ChildDNSService::NewAdditionalInfo(const nsACString& aTrrURL, int32_t aPort,
                                   nsIDNSAdditionalInfo** aInfo) {
  RefPtr<DNSAdditionalInfo> res = new DNSAdditionalInfo(aTrrURL, aPort);
  res.forget(aInfo);
  return NS_OK;
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolve(const nsACString& aHostname,
                                    nsIDNSService::ResolveType aType,
                                    uint32_t aFlags,
                                    nsIDNSAdditionalInfo* aInfo,
                                    nsIDNSListener* aListener, nsresult aReason,
                                    JS::Handle<JS::Value> aOriginAttributes,
                                    JSContext* aCx, uint8_t aArgc) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return CancelAsyncResolveInternal(aHostname, aType, aFlags, aInfo, aListener,
                                    aReason, attrs);
}

NS_IMETHODIMP
ChildDNSService::CancelAsyncResolveNative(
    const nsACString& aHostname, nsIDNSService::ResolveType aType,
    uint32_t aFlags, nsIDNSAdditionalInfo* aInfo, nsIDNSListener* aListener,
    nsresult aReason, const OriginAttributes& aOriginAttributes) {
  return CancelAsyncResolveInternal(aHostname, aType, aFlags, aInfo, aListener,
                                    aReason, aOriginAttributes);
}

NS_IMETHODIMP
ChildDNSService::Resolve(const nsACString& hostname, uint32_t flags,
                         JS::Handle<JS::Value> aOriginAttributes,
                         JSContext* aCx, uint8_t aArgc, nsIDNSRecord** result) {
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

  mTRRServiceParent->GetURI(aURI);
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
ChildDNSService::GetCurrentTrrConfirmationState(uint32_t* aConfirmationState) {
  if (!mTRRServiceParent) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aConfirmationState = mTRRServiceParent->GetConfirmationState();
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
                      aDnsRequest->mPort, aDnsRequest->mType,
                      aDnsRequest->mOriginAttributes, originalFlags,
                      originalListenerAddr, key);

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
  ReadPrefs(nullptr);

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    AddPrefObserver(prefs);
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "odoh-service-activated", false);
  }

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
    ReadPrefs(NS_ConvertUTF16toUTF8(data).get());
  } else if (!strcmp(topic, "odoh-service-activated")) {
    mODoHActivated = u"true"_ns.Equals(data);
  }

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
