/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDNSService2.h"
#include "nsIDNSRecord.h"
#include "nsIDNSListener.h"
#include "nsIDNSByTypeRecord.h"
#include "nsICancelable.h"
#include "nsIPrefBranch.h"
#include "nsIOService.h"
#include "nsIXPConnect.h"
#include "nsProxyRelease.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsNetCID.h"
#include "nsError.h"
#include "nsDNSPrefetch.h"
#include "nsThreadUtils.h"
#include "nsIProtocolProxyService.h"
#include "prsystem.h"
#include "prnetdb.h"
#include "prmon.h"
#include "prio.h"
#include "plstr.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsNetAddr.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsIObserverService.h"
#include "nsINetworkLinkService.h"
#include "DNSResolverInfo.h"
#include "TRRService.h"

#include "mozilla/Attributes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/ChildDNSService.h"
#include "mozilla/net/DNSListenerProxy.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Utf8.h"

using namespace mozilla;
using namespace mozilla::net;

static const char kPrefDnsCacheEntries[] = "network.dnsCacheEntries";
static const char kPrefDnsCacheExpiration[] = "network.dnsCacheExpiration";
static const char kPrefDnsCacheGrace[] =
    "network.dnsCacheExpirationGracePeriod";
static const char kPrefIPv4OnlyDomains[] = "network.dns.ipv4OnlyDomains";
static const char kPrefDisableIPv6[] = "network.dns.disableIPv6";
static const char kPrefDisablePrefetch[] = "network.dns.disablePrefetch";
static const char kPrefBlockDotOnion[] = "network.dns.blockDotOnion";
static const char kPrefDnsLocalDomains[] = "network.dns.localDomains";
static const char kPrefDnsForceResolve[] = "network.dns.forceResolve";
static const char kPrefDnsOfflineLocalhost[] = "network.dns.offline-localhost";
static const char kPrefDnsNotifyResolution[] = "network.dns.notifyResolution";
static const char kPrefNetworkProxySOCKS[] = "network.proxy.socks";

//-----------------------------------------------------------------------------

class nsDNSRecord : public nsIDNSAddrRecord {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSRECORD
  NS_DECL_NSIDNSADDRRECORD

  explicit nsDNSRecord(nsHostRecord* hostRecord) {
    mHostRecord = do_QueryObject(hostRecord);
  }

 private:
  virtual ~nsDNSRecord() = default;

  RefPtr<AddrHostRecord> mHostRecord;
  // Since mIter is holding a weak reference to the NetAddr array we must
  // make sure it is not released. So we also keep a RefPtr to the AddrInfo
  // which is immutable.
  RefPtr<AddrInfo> mAddrInfo;
  nsTArray<NetAddr>::const_iterator mIter;
  const NetAddr* iter() {
    if (!mIter.GetArray()) {
      return nullptr;
    }
    if (mIter.GetArray()->end() == mIter) {
      return nullptr;
    }
    return &*mIter;
  }

  int mIterGenCnt = -1;  // the generation count of
                         // mHostRecord->addr_info when we
                         // start iterating
  bool mDone = false;
};

NS_IMPL_ISUPPORTS(nsDNSRecord, nsIDNSRecord, nsIDNSAddrRecord)

NS_IMETHODIMP
nsDNSRecord::GetCanonicalName(nsACString& result) {
  // this method should only be called if we have a CNAME
  NS_ENSURE_TRUE(mHostRecord->flags & nsHostResolver::RES_CANON_NAME,
                 NS_ERROR_NOT_AVAILABLE);

  MutexAutoLock lock(mHostRecord->addr_info_lock);

  // if the record is for an IP address literal, then the canonical
  // host name is the IP address literal.
  if (!mHostRecord->addr_info) {
    result = mHostRecord->host;
    return NS_OK;
  }

  if (mHostRecord->addr_info->CanonicalHostname().IsEmpty()) {
    result = mHostRecord->addr_info->Hostname();
  } else {
    result = mHostRecord->addr_info->CanonicalHostname();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::IsTRR(bool* retval) {
  MutexAutoLock lock(mHostRecord->addr_info_lock);
  if (mHostRecord->addr_info) {
    // TODO: Let the consumers of nsIDNSRecord be unaware of the difference of
    // TRR and ODoH. Will let them know the truth when needed.
    *retval = mHostRecord->addr_info->IsTRROrODoH();
  } else {
    *retval = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetTrrFetchDuration(double* aTime) {
  MutexAutoLock lock(mHostRecord->addr_info_lock);
  if (mHostRecord->addr_info && mHostRecord->addr_info->IsTRROrODoH()) {
    *aTime = mHostRecord->addr_info->GetTrrFetchDuration();
  } else {
    *aTime = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetTrrFetchDurationNetworkOnly(double* aTime) {
  MutexAutoLock lock(mHostRecord->addr_info_lock);
  if (mHostRecord->addr_info && mHostRecord->addr_info->IsTRROrODoH()) {
    *aTime = mHostRecord->addr_info->GetTrrFetchDurationNetworkOnly();
  } else {
    *aTime = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetNextAddr(uint16_t port, NetAddr* addr) {
  if (mDone) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mHostRecord->addr_info_lock.Lock();
  if (mHostRecord->addr_info) {
    if (mIterGenCnt != mHostRecord->addr_info_gencnt) {
      // mHostRecord->addr_info has changed, restart the iteration.
      mIter = nsTArray<NetAddr>::const_iterator();
      mIterGenCnt = mHostRecord->addr_info_gencnt;
      // Make sure to hold a RefPtr to the AddrInfo so we can iterate through
      // the NetAddr array.
      mAddrInfo = mHostRecord->addr_info;
    }

    bool startedFresh = !iter();

    do {
      if (!iter()) {
        mIter = mAddrInfo->Addresses().begin();
      } else {
        mIter++;
      }
    } while (iter() && mHostRecord->Blocklisted(iter()));

    if (!iter() && startedFresh) {
      // If everything was blocklisted we want to reset the blocklist (and
      // likely relearn it) and return the first address. That is better
      // than nothing.
      mHostRecord->ResetBlocklist();
      mIter = mAddrInfo->Addresses().begin();
    }

    if (iter()) {
      *addr = *mIter;
    }

    mHostRecord->addr_info_lock.Unlock();

    if (!iter()) {
      mDone = true;
      mIter = nsTArray<NetAddr>::const_iterator();
      mAddrInfo = nullptr;
      mIterGenCnt = -1;
      return NS_ERROR_NOT_AVAILABLE;
    }
  } else {
    mHostRecord->addr_info_lock.Unlock();

    if (!mHostRecord->addr) {
      // Both mHostRecord->addr_info and mHostRecord->addr are null.
      // This can happen if mHostRecord->addr_info expired and the
      // attempt to reresolve it failed.
      return NS_ERROR_NOT_AVAILABLE;
    }
    memcpy(addr, mHostRecord->addr.get(), sizeof(NetAddr));
    mDone = true;
  }

  // set given port
  port = htons(port);
  if (addr->raw.family == AF_INET) {
    addr->inet.port = port;
  } else if (addr->raw.family == AF_INET6) {
    addr->inet6.port = port;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetAddresses(nsTArray<NetAddr>& aAddressArray) {
  if (mDone) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mHostRecord->addr_info_lock.Lock();
  if (mHostRecord->addr_info) {
    for (const auto& address : mHostRecord->addr_info->Addresses()) {
      if (mHostRecord->Blocklisted(&address)) {
        continue;
      }
      NetAddr* addr = aAddressArray.AppendElement(address);
      if (addr->raw.family == AF_INET) {
        addr->inet.port = 0;
      } else if (addr->raw.family == AF_INET6) {
        addr->inet6.port = 0;
      }
    }
    mHostRecord->addr_info_lock.Unlock();
  } else {
    mHostRecord->addr_info_lock.Unlock();

    if (!mHostRecord->addr) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    NetAddr* addr = aAddressArray.AppendElement(NetAddr());
    memcpy(addr, mHostRecord->addr.get(), sizeof(NetAddr));
    if (addr->raw.family == AF_INET) {
      addr->inet.port = 0;
    } else if (addr->raw.family == AF_INET6) {
      addr->inet6.port = 0;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetScriptableNextAddr(uint16_t port, nsINetAddr** result) {
  NetAddr addr;
  nsresult rv = GetNextAddr(port, &addr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  RefPtr<nsNetAddr> netaddr = new nsNetAddr(&addr);
  netaddr.forget(result);

  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetNextAddrAsString(nsACString& result) {
  NetAddr addr;
  nsresult rv = GetNextAddr(0, &addr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  char buf[kIPv6CStrBufSize];
  if (addr.ToStringBuffer(buf, sizeof(buf))) {
    result.Assign(buf);
    return NS_OK;
  }
  NS_ERROR("NetAddrToString failed unexpectedly");
  return NS_ERROR_FAILURE;  // conversion failed for some reason
}

NS_IMETHODIMP
nsDNSRecord::HasMore(bool* result) {
  if (mDone) {
    *result = false;
    return NS_OK;
  }

  nsTArray<NetAddr>::const_iterator iterCopy = mIter;
  int iterGenCntCopy = mIterGenCnt;

  NetAddr addr;
  *result = NS_SUCCEEDED(GetNextAddr(0, &addr));

  mIter = iterCopy;
  mIterGenCnt = iterGenCntCopy;
  mDone = false;

  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::Rewind() {
  mIter = nsTArray<NetAddr>::const_iterator();
  mIterGenCnt = -1;
  mDone = false;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::ReportUnusable(uint16_t aPort) {
  // right now we don't use the port in the blocklist

  MutexAutoLock lock(mHostRecord->addr_info_lock);

  // Check that we are using a real addr_info (as opposed to a single
  // constant address), and that the generation count is valid. Otherwise,
  // ignore the report.

  if (mHostRecord->addr_info && mIterGenCnt == mHostRecord->addr_info_gencnt &&
      iter()) {
    mHostRecord->ReportUnusable(iter());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetEffectiveTRRMode(uint32_t* aMode) {
  *aMode = mHostRecord->EffectiveTRRMode();
  return NS_OK;
}

class nsDNSByTypeRecord : public nsIDNSByTypeRecord,
                          public nsIDNSTXTRecord,
                          public nsIDNSHTTPSSVCRecord {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSRECORD
  NS_DECL_NSIDNSBYTYPERECORD
  NS_DECL_NSIDNSTXTRECORD
  NS_DECL_NSIDNSHTTPSSVCRECORD

  explicit nsDNSByTypeRecord(nsHostRecord* hostRecord) {
    mHostRecord = do_QueryObject(hostRecord);
  }

 private:
  virtual ~nsDNSByTypeRecord() = default;
  RefPtr<TypeHostRecord> mHostRecord;
};

NS_IMPL_ISUPPORTS(nsDNSByTypeRecord, nsIDNSRecord, nsIDNSByTypeRecord,
                  nsIDNSTXTRecord, nsIDNSHTTPSSVCRecord)

NS_IMETHODIMP
nsDNSByTypeRecord::GetType(uint32_t* aType) {
  *aType = mHostRecord->GetType();
  return NS_OK;
}

NS_IMETHODIMP
nsDNSByTypeRecord::GetRecords(CopyableTArray<nsCString>& aRecords) {
  // deep copy
  return mHostRecord->GetRecords(aRecords);
}

NS_IMETHODIMP
nsDNSByTypeRecord::GetRecordsAsOneString(nsACString& aRecords) {
  // deep copy
  return mHostRecord->GetRecordsAsOneString(aRecords);
}

NS_IMETHODIMP
nsDNSByTypeRecord::GetRecords(nsTArray<RefPtr<nsISVCBRecord>>& aRecords) {
  return mHostRecord->GetRecords(aRecords);
}

NS_IMETHODIMP
nsDNSByTypeRecord::GetServiceModeRecord(bool aNoHttp2, bool aNoHttp3,
                                        nsISVCBRecord** aRecord) {
  return mHostRecord->GetServiceModeRecord(aNoHttp2, aNoHttp3, aRecord);
}

NS_IMETHODIMP
nsDNSByTypeRecord::GetAllRecordsWithEchConfig(
    bool aNoHttp2, bool aNoHttp3, bool* aAllRecordsHaveEchConfig,
    bool* aAllRecordsInH3ExcludedList,
    nsTArray<RefPtr<nsISVCBRecord>>& aResult) {
  return mHostRecord->GetAllRecordsWithEchConfig(
      aNoHttp2, aNoHttp3, aAllRecordsHaveEchConfig, aAllRecordsInH3ExcludedList,
      aResult);
}

NS_IMETHODIMP
nsDNSByTypeRecord::GetHasIPAddresses(bool* aResult) {
  return mHostRecord->GetHasIPAddresses(aResult);
}

NS_IMETHODIMP
nsDNSByTypeRecord::GetAllRecordsExcluded(bool* aResult) {
  return mHostRecord->GetAllRecordsExcluded(aResult);
}

NS_IMETHODIMP
nsDNSByTypeRecord::GetResults(mozilla::net::TypeRecordResultType* aResults) {
  *aResults = mHostRecord->GetResults();
  return NS_OK;
}

NS_IMETHODIMP
nsDNSByTypeRecord::GetTtl(uint32_t* aTtl) { return mHostRecord->GetTtl(aTtl); }

//-----------------------------------------------------------------------------

class nsDNSAsyncRequest final : public nsResolveHostCallback,
                                public nsICancelable {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICANCELABLE

  nsDNSAsyncRequest(nsHostResolver* res, const nsACString& host,
                    const nsACString& trrServer, uint16_t type,
                    const OriginAttributes& attrs, nsIDNSListener* listener,
                    uint16_t flags, uint16_t af)
      : mResolver(res),
        mHost(host),
        mTrrServer(trrServer),
        mType(type),
        mOriginAttributes(attrs),
        mListener(listener),
        mFlags(flags),
        mAF(af) {}

  void OnResolveHostComplete(nsHostResolver*, nsHostRecord*, nsresult) override;
  // Returns TRUE if the DNS listener arg is the same as the member listener
  // Used in Cancellations to remove DNS requests associated with a
  // particular hostname and nsIDNSListener
  bool EqualsAsyncListener(nsIDNSListener* aListener) override;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf) const override;

  RefPtr<nsHostResolver> mResolver;
  nsCString mHost;       // hostname we're resolving
  nsCString mTrrServer;  // A trr server to be used.
  uint16_t mType = 0;
  const OriginAttributes
      mOriginAttributes;  // The originAttributes for this resolving
  nsCOMPtr<nsIDNSListener> mListener;
  uint16_t mFlags = 0;
  uint16_t mAF = 0;

 private:
  virtual ~nsDNSAsyncRequest() = default;
};

NS_IMPL_ISUPPORTS(nsDNSAsyncRequest, nsICancelable)

void nsDNSAsyncRequest::OnResolveHostComplete(nsHostResolver* resolver,
                                              nsHostRecord* hostRecord,
                                              nsresult status) {
  // need to have an owning ref when we issue the callback to enable
  // the caller to be able to addref/release multiple times without
  // destroying the record prematurely.
  nsCOMPtr<nsIDNSRecord> rec;
  if (NS_SUCCEEDED(status)) {
    MOZ_ASSERT(hostRecord, "no host record");
    if (hostRecord->type != nsDNSService::RESOLVE_TYPE_DEFAULT) {
      rec = new nsDNSByTypeRecord(hostRecord);
    } else {
      rec = new nsDNSRecord(hostRecord);
    }
  }

  mListener->OnLookupComplete(this, rec, status);
  mListener = nullptr;
}

bool nsDNSAsyncRequest::EqualsAsyncListener(nsIDNSListener* aListener) {
  uintptr_t originalListenerAddr = reinterpret_cast<uintptr_t>(mListener.get());
  RefPtr<DNSListenerProxy> wrapper = do_QueryObject(mListener);
  if (wrapper) {
    originalListenerAddr = wrapper->GetOriginalListenerAddress();
  }

  uintptr_t listenerAddr = reinterpret_cast<uintptr_t>(aListener);
  return (listenerAddr == originalListenerAddr);
}

size_t nsDNSAsyncRequest::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);

  // The following fields aren't measured.
  // - mHost, because it's a non-owning pointer
  // - mResolver, because it's a non-owning pointer
  // - mListener, because it's a non-owning pointer

  return n;
}

NS_IMETHODIMP
nsDNSAsyncRequest::Cancel(nsresult reason) {
  NS_ENSURE_ARG(NS_FAILED(reason));
  MOZ_DIAGNOSTIC_ASSERT(mResolver, "mResolver should not be null");
  mResolver->DetachCallback(mHost, mTrrServer, mType, mOriginAttributes, mFlags,
                            mAF, this, reason);
  return NS_OK;
}

//-----------------------------------------------------------------------------

class nsDNSSyncRequest : public nsResolveHostCallback {
  NS_DECL_THREADSAFE_ISUPPORTS
 public:
  explicit nsDNSSyncRequest(PRMonitor* mon) : mMonitor(mon) {}

  void OnResolveHostComplete(nsHostResolver*, nsHostRecord*, nsresult) override;
  bool EqualsAsyncListener(nsIDNSListener* aListener) override;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf) const override;

  bool mDone = false;
  nsresult mStatus = NS_OK;
  RefPtr<nsHostRecord> mHostRecord;

 private:
  virtual ~nsDNSSyncRequest() = default;

  PRMonitor* mMonitor = nullptr;
};

NS_IMPL_ISUPPORTS0(nsDNSSyncRequest)

void nsDNSSyncRequest::OnResolveHostComplete(nsHostResolver* resolver,
                                             nsHostRecord* hostRecord,
                                             nsresult status) {
  // store results, and wake up nsDNSService::Resolve to process results.
  PR_EnterMonitor(mMonitor);
  mDone = true;
  mStatus = status;
  mHostRecord = hostRecord;
  PR_Notify(mMonitor);
  PR_ExitMonitor(mMonitor);
}

bool nsDNSSyncRequest::EqualsAsyncListener(nsIDNSListener* aListener) {
  // Sync request: no listener to compare
  return false;
}

size_t nsDNSSyncRequest::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const {
  size_t n = mallocSizeOf(this);

  // The following fields aren't measured.
  // - mHostRecord, because it's a non-owning pointer

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mMonitor

  return n;
}

class NotifyDNSResolution : public Runnable {
 public:
  explicit NotifyDNSResolution(const nsACString& aHostname)
      : mozilla::Runnable("NotifyDNSResolution"), mHostname(aHostname) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(nullptr, "dns-resolution-request",
                           NS_ConvertUTF8toUTF16(mHostname).get());
    }
    return NS_OK;
  }

 private:
  nsCString mHostname;
};

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsDNSService, nsIDNSService, nsPIDNSService, nsIObserver,
                  nsIMemoryReporter)

/******************************************************************************
 * nsDNSService impl:
 * singleton instance ctor/dtor methods
 ******************************************************************************/
static StaticRefPtr<nsDNSService> gDNSService;
static Atomic<bool> gInited(false);

already_AddRefed<nsIDNSService> GetOrInitDNSService() {
  if (gInited) {
    return nsDNSService::GetXPCOMSingleton();
  }

  nsCOMPtr<nsIDNSService> dns = nullptr;
  auto initTask = [&dns]() { dns = do_GetService(NS_DNSSERVICE_CID); };
  if (!NS_IsMainThread()) {
    // Forward to the main thread synchronously.
    RefPtr<nsIThread> mainThread = do_GetMainThread();
    if (!mainThread) {
      return nullptr;
    }

    SyncRunnable::DispatchToThread(mainThread,
                                   new SyncRunnable(NS_NewRunnableFunction(
                                       "GetOrInitDNSService", initTask)));
  } else {
    initTask();
  }

  return dns.forget();
}

already_AddRefed<nsIDNSService> nsDNSService::GetXPCOMSingleton() {
  auto getDNSHelper = []() -> already_AddRefed<nsIDNSService> {
    if (nsIOService::UseSocketProcess()) {
      if (XRE_IsSocketProcess()) {
        return GetSingleton();
      }

      if (XRE_IsContentProcess() || XRE_IsParentProcess()) {
        return ChildDNSService::GetSingleton();
      }

      return nullptr;
    }

    if (XRE_IsParentProcess()) {
      return GetSingleton();
    }

    if (XRE_IsContentProcess() || XRE_IsSocketProcess()) {
      return ChildDNSService::GetSingleton();
    }

    return nullptr;
  };

  if (gInited) {
    return getDNSHelper();
  }

  nsCOMPtr<nsIDNSService> dns = getDNSHelper();
  if (dns) {
    gInited = true;
  }
  return dns.forget();
}

already_AddRefed<nsDNSService> nsDNSService::GetSingleton() {
  MOZ_ASSERT_IF(nsIOService::UseSocketProcess(), XRE_IsSocketProcess());
  MOZ_ASSERT_IF(!nsIOService::UseSocketProcess(), XRE_IsParentProcess());

  if (!gDNSService) {
    if (!NS_IsMainThread()) {
      return nullptr;
    }
    gDNSService = new nsDNSService();
    if (NS_SUCCEEDED(gDNSService->Init())) {
      ClearOnShutdown(&gDNSService);
    } else {
      gDNSService = nullptr;
    }
  }

  return do_AddRef(gDNSService);
}

nsresult nsDNSService::ReadPrefs(const char* name) {
  bool tmpbool;
  uint32_t tmpint;
  mResolverPrefsUpdated = false;

  // resolver-specific prefs first
  if (!name || !strcmp(name, kPrefDnsCacheEntries)) {
    if (NS_SUCCEEDED(Preferences::GetUint(kPrefDnsCacheEntries, &tmpint))) {
      if (!name || (tmpint != mResCacheEntries)) {
        mResCacheEntries = tmpint;
        mResolverPrefsUpdated = true;
      }
    }
  }
  if (!name || !strcmp(name, kPrefDnsCacheExpiration)) {
    if (NS_SUCCEEDED(Preferences::GetUint(kPrefDnsCacheExpiration, &tmpint))) {
      if (!name || (tmpint != mResCacheExpiration)) {
        mResCacheExpiration = tmpint;
        mResolverPrefsUpdated = true;
      }
    }
  }
  if (!name || !strcmp(name, kPrefDnsCacheGrace)) {
    if (NS_SUCCEEDED(Preferences::GetUint(kPrefDnsCacheGrace, &tmpint))) {
      if (!name || (tmpint != mResCacheGrace)) {
        mResCacheGrace = tmpint;
        mResolverPrefsUpdated = true;
      }
    }
  }

  // DNSservice prefs
  if (!name || !strcmp(name, kPrefDisableIPv6)) {
    if (NS_SUCCEEDED(Preferences::GetBool(kPrefDisableIPv6, &tmpbool))) {
      mDisableIPv6 = tmpbool;
    }
  }
  if (!name || !strcmp(name, kPrefDnsOfflineLocalhost)) {
    if (NS_SUCCEEDED(
            Preferences::GetBool(kPrefDnsOfflineLocalhost, &tmpbool))) {
      mOfflineLocalhost = tmpbool;
    }
  }
  if (!name || !strcmp(name, kPrefDisablePrefetch)) {
    if (NS_SUCCEEDED(Preferences::GetBool(kPrefDisablePrefetch, &tmpbool))) {
      mDisablePrefetch = tmpbool;
    }
  }
  if (!name || !strcmp(name, kPrefBlockDotOnion)) {
    if (NS_SUCCEEDED(Preferences::GetBool(kPrefBlockDotOnion, &tmpbool))) {
      mBlockDotOnion = tmpbool;
    }
  }
  if (!name || !strcmp(name, kPrefDnsNotifyResolution)) {
    if (NS_SUCCEEDED(
            Preferences::GetBool(kPrefDnsNotifyResolution, &tmpbool))) {
      mNotifyResolution = tmpbool;
    }
  }
  if (!name || !strcmp(name, kPrefNetworkProxySOCKS)) {
    nsAutoCString socks;
    if (NS_SUCCEEDED(Preferences::GetCString(kPrefNetworkProxySOCKS, socks))) {
      mHasSocksProxy = !socks.IsEmpty();
    }
  }
  if (!name || !strcmp(name, kPrefIPv4OnlyDomains)) {
    Preferences::GetCString(kPrefIPv4OnlyDomains, mIPv4OnlyDomains);
  }
  if (!name || !strcmp(name, kPrefDnsLocalDomains)) {
    nsCString localDomains;
    Preferences::GetCString(kPrefDnsLocalDomains, localDomains);
    MutexAutoLock lock(mLock);
    mLocalDomains.Clear();
    for (const auto& token :
         nsCCharSeparatedTokenizerTemplate<NS_IsAsciiWhitespace,
                                           nsTokenizerFlags::SeparatorOptional>(
             localDomains, ',')
             .ToRange()) {
      mLocalDomains.Insert(token);
    }
  }
  if (!name || !strcmp(name, kPrefDnsForceResolve)) {
    Preferences::GetCString(kPrefDnsForceResolve, mForceResolve);
    mForceResolveOn = !mForceResolve.IsEmpty();
  }

  if (StaticPrefs::network_proxy_type() ==
      nsIProtocolProxyService::PROXYCONFIG_MANUAL) {
    // Disable prefetching either by explicit preference or if a
    // manual proxy is configured
    mDisablePrefetch = true;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::Init() {
  MOZ_ASSERT(!mResolver);
  MOZ_ASSERT(NS_IsMainThread());

  ReadPrefs(nullptr);

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->AddObserver(this, "last-pb-context-exited", false);
    observerService->AddObserver(this, NS_NETWORK_LINK_TOPIC, false);
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    observerService->AddObserver(this, "odoh-service-activated", false);
  }

  RefPtr<nsHostResolver> res;
  nsresult rv = nsHostResolver::Create(mResCacheEntries, mResCacheExpiration,
                                       mResCacheGrace, getter_AddRefs(res));
  if (NS_SUCCEEDED(rv)) {
    // now, set all of our member variables while holding the lock
    MutexAutoLock lock(mLock);
    mResolver = res;
  }

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    // register as prefs observer
    prefs->AddObserver(kPrefDnsCacheEntries, this, false);
    prefs->AddObserver(kPrefDnsCacheExpiration, this, false);
    prefs->AddObserver(kPrefDnsCacheGrace, this, false);
    prefs->AddObserver(kPrefIPv4OnlyDomains, this, false);
    prefs->AddObserver(kPrefDnsLocalDomains, this, false);
    prefs->AddObserver(kPrefDnsForceResolve, this, false);
    prefs->AddObserver(kPrefDisableIPv6, this, false);
    prefs->AddObserver(kPrefDnsOfflineLocalhost, this, false);
    prefs->AddObserver(kPrefDisablePrefetch, this, false);
    prefs->AddObserver(kPrefBlockDotOnion, this, false);
    prefs->AddObserver(kPrefDnsNotifyResolution, this, false);

    // Monitor these to see if there is a change in proxy configuration
    prefs->AddObserver(kPrefNetworkProxySOCKS, this, false);
  }

  nsDNSPrefetch::Initialize(this);

  RegisterWeakMemoryReporter(this);

  mTrrService = new TRRService();
  if (NS_FAILED(mTrrService->Init())) {
    mTrrService = nullptr;
  }

  nsCOMPtr<nsIIDNService> idn = do_GetService(NS_IDNSERVICE_CONTRACTID);
  mIDN = idn;

  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::Shutdown() {
  UnregisterWeakMemoryReporter(this);

  RefPtr<nsHostResolver> res;
  {
    MutexAutoLock lock(mLock);
    res = std::move(mResolver);
  }
  if (res) {
    // Shutdown outside lock.
    res->Shutdown();
  }

  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, NS_NETWORK_LINK_TOPIC);
    observerService->RemoveObserver(this, "last-pb-context-exited");
    observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }

  return NS_OK;
}

bool nsDNSService::GetOffline() const {
  bool offline = false;
  nsCOMPtr<nsIIOService> io = do_GetService(NS_IOSERVICE_CONTRACTID);
  if (io) {
    io->GetOffline(&offline);
  }
  return offline;
}

NS_IMETHODIMP
nsDNSService::GetPrefetchEnabled(bool* outVal) {
  MutexAutoLock lock(mLock);
  *outVal = !mDisablePrefetch;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::SetPrefetchEnabled(bool inVal) {
  MutexAutoLock lock(mLock);
  mDisablePrefetch = !inVal;
  return NS_OK;
}

bool nsDNSService::DNSForbiddenByActiveProxy(const nsACString& aHostname,
                                             uint32_t flags) {
  if (flags & nsIDNSService::RESOLVE_IGNORE_SOCKS_DNS) {
    return false;
  }

  // We should avoid doing DNS when a proxy is in use.
  PRNetAddr tempAddr;
  if (StaticPrefs::network_proxy_type() ==
          nsIProtocolProxyService::PROXYCONFIG_MANUAL &&
      mHasSocksProxy && StaticPrefs::network_proxy_socks_remote_dns()) {
    // Allow IP lookups through, but nothing else.
    if (PR_StringToNetAddr(nsCString(aHostname).get(), &tempAddr) !=
        PR_SUCCESS) {
      return true;
    }
  }
  return false;
}

already_AddRefed<nsHostResolver> nsDNSService::GetResolverLocked() {
  MutexAutoLock lock(mLock);
  return do_AddRef(mResolver);
}

nsresult nsDNSService::PreprocessHostname(bool aLocalDomain,
                                          const nsACString& aInput,
                                          nsIIDNService* aIDN,
                                          nsACString& aACE) {
  // Enforce RFC 7686
  if (mBlockDotOnion && StringEndsWith(aInput, ".onion"_ns)) {
    return NS_ERROR_UNKNOWN_HOST;
  }

  if (aLocalDomain) {
    aACE.AssignLiteral("localhost");
    return NS_OK;
  }

  if (mTrrService && mTrrService->MaybeBootstrap(aInput, aACE)) {
    return NS_OK;
  }

  if (mForceResolveOn) {
    MutexAutoLock lock(mLock);
    if (!aInput.LowerCaseEqualsASCII("localhost") &&
        !aInput.LowerCaseEqualsASCII("127.0.0.1")) {
      aACE.Assign(mForceResolve);
      return NS_OK;
    }
  }

  if (!aIDN || IsAscii(aInput)) {
    aACE = aInput;
    return NS_OK;
  }

  if (!(IsUtf8(aInput) && NS_SUCCEEDED(aIDN->ConvertUTF8toACE(aInput, aACE)))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult nsDNSService::AsyncResolveInternal(
    const nsACString& aHostname, uint16_t type, uint32_t flags,
    nsIDNSResolverInfo* aResolver, nsIDNSListener* aListener,
    nsIEventTarget* target_, const OriginAttributes& aOriginAttributes,
    nsICancelable** result) {
  // grab reference to global host resolver and IDN service.  beware
  // simultaneous shutdown!!
  RefPtr<nsHostResolver> res;
  nsCOMPtr<nsIIDNService> idn;
  nsCOMPtr<nsIEventTarget> target = target_;
  nsCOMPtr<nsIDNSListener> listener = aListener;
  bool localDomain = false;
  {
    MutexAutoLock lock(mLock);

    if (mDisablePrefetch && (flags & RESOLVE_SPECULATE)) {
      return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
    }

    res = mResolver;
    idn = mIDN;
    localDomain = mLocalDomains.Contains(aHostname);
  }

  if (mNotifyResolution) {
    NS_DispatchToMainThread(new NotifyDNSResolution(aHostname));
  }

  if (!res) {
    return NS_ERROR_OFFLINE;
  }

  if ((type != RESOLVE_TYPE_DEFAULT) && (type != RESOLVE_TYPE_TXT) &&
      (type != RESOLVE_TYPE_HTTPSSVC)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (DNSForbiddenByActiveProxy(aHostname, flags)) {
    // nsHostResolver returns NS_ERROR_UNKNOWN_HOST for lots of reasons.
    // We use a different error code to differentiate this failure and to make
    // it clear(er) where this error comes from.
    return NS_ERROR_UNKNOWN_PROXY_HOST;
  }

  nsCString hostname;
  nsresult rv = PreprocessHostname(localDomain, aHostname, idn, hostname);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (GetOffline() &&
      (!mOfflineLocalhost || !hostname.LowerCaseEqualsASCII("localhost"))) {
    flags |= RESOLVE_OFFLINE;
  }

  // make sure JS callers get notification on the main thread
  nsCOMPtr<nsIXPConnectWrappedJS> wrappedListener = do_QueryInterface(listener);
  if (wrappedListener && !target) {
    target = GetMainThreadEventTarget();
  }

  if (target) {
    listener = new DNSListenerProxy(listener, target);
  }

  uint16_t af =
      (type != RESOLVE_TYPE_DEFAULT) ? 0 : GetAFForLookup(hostname, flags);

  MOZ_ASSERT(listener);
  RefPtr<nsDNSAsyncRequest> req =
      new nsDNSAsyncRequest(res, hostname, DNSResolverInfo::URL(aResolver),
                            type, aOriginAttributes, listener, flags, af);
  if (!req) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = res->ResolveHost(req->mHost, DNSResolverInfo::URL(aResolver), type,
                        req->mOriginAttributes, flags, af, req);
  req.forget(result);
  return rv;
}

nsresult nsDNSService::CancelAsyncResolveInternal(
    const nsACString& aHostname, uint16_t aType, uint32_t aFlags,
    nsIDNSResolverInfo* aResolver, nsIDNSListener* aListener, nsresult aReason,
    const OriginAttributes& aOriginAttributes) {
  // grab reference to global host resolver and IDN service.  beware
  // simultaneous shutdown!!
  RefPtr<nsHostResolver> res;
  nsCOMPtr<nsIIDNService> idn;
  bool localDomain = false;
  {
    MutexAutoLock lock(mLock);

    if (mDisablePrefetch && (aFlags & RESOLVE_SPECULATE)) {
      return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;
    }

    res = mResolver;
    idn = mIDN;
    localDomain = mLocalDomains.Contains(aHostname);
  }
  if (!res) {
    return NS_ERROR_OFFLINE;
  }

  nsCString hostname;
  nsresult rv = PreprocessHostname(localDomain, aHostname, idn, hostname);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint16_t af =
      (aType != RESOLVE_TYPE_DEFAULT) ? 0 : GetAFForLookup(hostname, aFlags);

  res->CancelAsyncRequest(hostname, DNSResolverInfo::URL(aResolver), aType,
                          aOriginAttributes, aFlags, af, aListener, aReason);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::AsyncResolve(const nsACString& aHostname,
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

  return AsyncResolveInternal(aHostname, aType, flags, aResolver, listener,
                              target_, attrs, result);
}

NS_IMETHODIMP
nsDNSService::AsyncResolveNative(const nsACString& aHostname,
                                 nsIDNSService::ResolveType aType,
                                 uint32_t flags, nsIDNSResolverInfo* aResolver,
                                 nsIDNSListener* aListener,
                                 nsIEventTarget* target_,
                                 const OriginAttributes& aOriginAttributes,
                                 nsICancelable** result) {
  return AsyncResolveInternal(aHostname, aType, flags, aResolver, aListener,
                              target_, aOriginAttributes, result);
}

NS_IMETHODIMP
nsDNSService::NewTRRResolverInfo(const nsACString& aTrrURL,
                                 nsIDNSResolverInfo** aResolver) {
  RefPtr<DNSResolverInfo> res = new DNSResolverInfo(aTrrURL);
  res.forget(aResolver);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::CancelAsyncResolve(const nsACString& aHostname,
                                 nsIDNSService::ResolveType aType,
                                 uint32_t aFlags, nsIDNSResolverInfo* aResolver,
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
nsDNSService::CancelAsyncResolveNative(
    const nsACString& aHostname, nsIDNSService::ResolveType aType,
    uint32_t aFlags, nsIDNSResolverInfo* aResolver, nsIDNSListener* aListener,
    nsresult aReason, const OriginAttributes& aOriginAttributes) {
  return CancelAsyncResolveInternal(aHostname, aType, aFlags, aResolver,
                                    aListener, aReason, aOriginAttributes);
}

NS_IMETHODIMP
nsDNSService::Resolve(const nsACString& aHostname, uint32_t flags,
                      JS::HandleValue aOriginAttributes, JSContext* aCx,
                      uint8_t aArgc, nsIDNSRecord** result) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return ResolveNative(aHostname, flags, attrs, result);
}

NS_IMETHODIMP
nsDNSService::ResolveNative(const nsACString& aHostname, uint32_t flags,
                            const OriginAttributes& aOriginAttributes,
                            nsIDNSRecord** result) {
  // Synchronous resolution is not available on the main thread.
  if (NS_IsMainThread()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return ResolveInternal(aHostname, flags, aOriginAttributes, result);
}

nsresult nsDNSService::DeprecatedSyncResolve(
    const nsACString& aHostname, uint32_t flags,
    const OriginAttributes& aOriginAttributes, nsIDNSRecord** result) {
  return ResolveInternal(aHostname, flags, aOriginAttributes, result);
}

nsresult nsDNSService::ResolveInternal(
    const nsACString& aHostname, uint32_t flags,
    const OriginAttributes& aOriginAttributes, nsIDNSRecord** result) {
  // grab reference to global host resolver and IDN service.  beware
  // simultaneous shutdown!!
  RefPtr<nsHostResolver> res;
  nsCOMPtr<nsIIDNService> idn;
  bool localDomain = false;
  {
    MutexAutoLock lock(mLock);
    res = mResolver;
    idn = mIDN;
    localDomain = mLocalDomains.Contains(aHostname);
  }

  if (mNotifyResolution) {
    NS_DispatchToMainThread(new NotifyDNSResolution(aHostname));
  }

  NS_ENSURE_TRUE(res, NS_ERROR_OFFLINE);

  nsCString hostname;
  nsresult rv = PreprocessHostname(localDomain, aHostname, idn, hostname);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (GetOffline() &&
      (!mOfflineLocalhost || !hostname.LowerCaseEqualsASCII("localhost"))) {
    flags |= RESOLVE_OFFLINE;
  }

  if (DNSForbiddenByActiveProxy(aHostname, flags)) {
    return NS_ERROR_UNKNOWN_PROXY_HOST;
  }

  //
  // sync resolve: since the host resolver only works asynchronously, we need
  // to use a mutex and a condvar to wait for the result.  however, since the
  // result may be in the resolvers cache, we might get called back recursively
  // on the same thread.  so, our mutex needs to be re-entrant.  in other words,
  // we need to use a monitor! ;-)
  //

  PRMonitor* mon = PR_NewMonitor();
  if (!mon) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PR_EnterMonitor(mon);
  RefPtr<nsDNSSyncRequest> syncReq = new nsDNSSyncRequest(mon);

  uint16_t af = GetAFForLookup(hostname, flags);

  // TRR uses the main thread for the HTTPS channel to the DoH server.
  // If this were to block the main thread while waiting for TRR it would
  // likely cause a deadlock. Instead we intentionally choose to not use TRR
  // for this.
  if (NS_IsMainThread()) {
    flags |= RESOLVE_DISABLE_TRR;
  }

  rv = res->ResolveHost(hostname, ""_ns, RESOLVE_TYPE_DEFAULT,
                        aOriginAttributes, flags, af, syncReq);
  if (NS_SUCCEEDED(rv)) {
    // wait for result
    while (!syncReq->mDone) {
      PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
    }

    if (NS_FAILED(syncReq->mStatus)) {
      rv = syncReq->mStatus;
    } else {
      NS_ASSERTION(syncReq->mHostRecord, "no host record");
      RefPtr<nsDNSRecord> rec = new nsDNSRecord(syncReq->mHostRecord);
      rec.forget(result);
    }
  }

  PR_ExitMonitor(mon);
  PR_DestroyMonitor(mon);
  return rv;
}

NS_IMETHODIMP
nsDNSService::GetMyHostName(nsACString& result) {
  char name[100];
  if (PR_GetSystemInfo(PR_SI_HOSTNAME, name, sizeof(name)) == PR_SUCCESS) {
    result = name;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDNSService::GetODoHActivated(bool* aResult) {
  NS_ENSURE_ARG(aResult);

  *aResult = mODoHActivated;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::Observe(nsISupports* subject, const char* topic,
                      const char16_t* data) {
  bool flushCache = false;
  RefPtr<nsHostResolver> resolver = GetResolverLocked();

  if (!strcmp(topic, NS_NETWORK_LINK_TOPIC)) {
    nsAutoCString converted = NS_ConvertUTF16toUTF8(data);
    if (!strcmp(converted.get(), NS_NETWORK_LINK_DATA_CHANGED)) {
      flushCache = true;
    }
  } else if (!strcmp(topic, "last-pb-context-exited")) {
    flushCache = true;
  } else if (!strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    ReadPrefs(NS_ConvertUTF16toUTF8(data).get());
    NS_ENSURE_TRUE(resolver, NS_ERROR_NOT_INITIALIZED);
    if (mResolverPrefsUpdated && resolver) {
      resolver->SetCacheLimits(mResCacheEntries, mResCacheExpiration,
                               mResCacheGrace);
    }
  } else if (!strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Shutdown();
  } else if (!strcmp(topic, "odoh-service-activated")) {
    mODoHActivated = u"true"_ns.Equals(data);
  }

  if (flushCache && resolver) {
    resolver->FlushCache(false);
    return NS_OK;
  }

  return NS_OK;
}

uint16_t nsDNSService::GetAFForLookup(const nsACString& host, uint32_t flags) {
  if (mDisableIPv6 || (flags & RESOLVE_DISABLE_IPV6)) {
    return PR_AF_INET;
  }

  MutexAutoLock lock(mLock);

  uint16_t af = PR_AF_UNSPEC;

  if (!mIPv4OnlyDomains.IsEmpty()) {
    const char *domain, *domainEnd, *end;
    uint32_t hostLen, domainLen;

    // see if host is in one of the IPv4-only domains
    domain = mIPv4OnlyDomains.BeginReading();
    domainEnd = mIPv4OnlyDomains.EndReading();

    nsACString::const_iterator hostStart;
    host.BeginReading(hostStart);
    hostLen = host.Length();

    do {
      // skip any whitespace
      while (*domain == ' ' || *domain == '\t') {
        ++domain;
      }

      // find end of this domain in the string
      end = strchr(domain, ',');
      if (!end) {
        end = domainEnd;
      }

      // to see if the hostname is in the domain, check if the domain
      // matches the end of the hostname.
      domainLen = end - domain;
      if (domainLen && hostLen >= domainLen) {
        const char* hostTail = hostStart.get() + hostLen - domainLen;
        if (PL_strncasecmp(domain, hostTail, domainLen) == 0) {
          // now, make sure either that the hostname is a direct match or
          // that the hostname begins with a dot.
          if (hostLen == domainLen || *hostTail == '.' ||
              *(hostTail - 1) == '.') {
            af = PR_AF_INET;
            break;
          }
        }
      }

      domain = end + 1;
    } while (*end);
  }

  if ((af != PR_AF_INET) && (flags & RESOLVE_DISABLE_IPV4)) {
    af = PR_AF_INET6;
  }

  return af;
}

NS_IMETHODIMP
nsDNSService::GetDNSCacheEntries(
    nsTArray<mozilla::net::DNSCacheEntries>* args) {
  RefPtr<nsHostResolver> resolver = GetResolverLocked();
  NS_ENSURE_TRUE(resolver, NS_ERROR_NOT_INITIALIZED);
  resolver->GetDNSCacheEntries(args);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::ClearCache(bool aTrrToo) {
  RefPtr<nsHostResolver> resolver = GetResolverLocked();
  NS_ENSURE_TRUE(resolver, NS_ERROR_NOT_INITIALIZED);
  resolver->FlushCache(aTrrToo);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::ReloadParentalControlEnabled() {
  if (mTrrService) {
    mTrrService->mParentalControlEnabled =
        TRRService::GetParentalControlEnabledInternal();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::SetDetectedTrrURI(const nsACString& aURI) {
  if (mTrrService) {
    mTrrService->SetDetectedTrrURI(aURI);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::GetCurrentTrrURI(nsACString& aURI) {
  if (mTrrService) {
    return mTrrService->GetURI(aURI);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::GetCurrentTrrMode(nsIDNSService::ResolverMode* aMode) {
  *aMode = nsIDNSService::MODE_NATIVEONLY;  // The default mode.
  if (mTrrService) {
    *aMode = mTrrService->Mode();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::GetCurrentTrrConfirmationState(uint32_t* aConfirmationState) {
  *aConfirmationState = uint32_t(TRRService::CONFIRM_OFF);
  if (mTrrService) {
    *aConfirmationState = mTrrService->ConfirmationState();
  }
  return NS_OK;
}

size_t nsDNSService::SizeOfIncludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mIDN
  // - mLock

  size_t n = mallocSizeOf(this);
  n += mResolver ? mResolver->SizeOfIncludingThis(mallocSizeOf) : 0;
  n += mIPv4OnlyDomains.SizeOfExcludingThisIfUnshared(mallocSizeOf);
  n += mLocalDomains.SizeOfExcludingThis(mallocSizeOf);
  n += mFailedSVCDomainNames.ShallowSizeOfExcludingThis(mallocSizeOf);
  for (const auto& data : mFailedSVCDomainNames.Values()) {
    n += data->ShallowSizeOfExcludingThis(mallocSizeOf);
    for (const auto& name : *data) {
      n += name.SizeOfExcludingThisIfUnshared(mallocSizeOf);
    }
  }
  return n;
}

MOZ_DEFINE_MALLOC_SIZE_OF(DNSServiceMallocSizeOf)

NS_IMETHODIMP
nsDNSService::CollectReports(nsIHandleReportCallback* aHandleReport,
                             nsISupports* aData, bool aAnonymize) {
  MOZ_COLLECT_REPORT("explicit/network/dns-service", KIND_HEAP, UNITS_BYTES,
                     SizeOfIncludingThis(DNSServiceMallocSizeOf),
                     "Memory used for the DNS service.");

  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::ReportFailedSVCDomainName(const nsACString& aOwnerName,
                                        const nsACString& aSVCDomainName) {
  MutexAutoLock lock(mLock);

  mFailedSVCDomainNames.GetOrInsertNew(aOwnerName, 1)
      ->AppendElement(aSVCDomainName);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::IsSVCDomainNameFailed(const nsACString& aOwnerName,
                                    const nsACString& aSVCDomainName,
                                    bool* aResult) {
  NS_ENSURE_ARG(aResult);

  MutexAutoLock lock(mLock);
  *aResult = false;
  nsTArray<nsCString>* failedList = mFailedSVCDomainNames.Get(aOwnerName);
  if (!failedList) {
    return NS_OK;
  }

  *aResult = failedList->Contains(aSVCDomainName);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::ResetExcludedSVCDomainName(const nsACString& aOwnerName) {
  MutexAutoLock lock(mLock);
  mFailedSVCDomainNames.Remove(aOwnerName);
  return NS_OK;
}
