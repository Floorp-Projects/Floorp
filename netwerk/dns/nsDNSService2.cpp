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
#include "TRRService.h"

#include "mozilla/Attributes.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/ChildDNSService.h"
#include "mozilla/net/DNSListenerProxy.h"
#include "mozilla/Services.h"
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
static const char kPrefNetworkProxyType[] = "network.proxy.type";

//-----------------------------------------------------------------------------

class nsDNSRecord : public nsIDNSRecord {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDNSRECORD

  explicit nsDNSRecord(nsHostRecord* hostRecord)
      : mIter(nullptr), mIterGenCnt(-1), mDone(false) {
    mHostRecord = do_QueryObject(hostRecord);
  }

 private:
  virtual ~nsDNSRecord() = default;

  RefPtr<AddrHostRecord> mHostRecord;
  NetAddrElement* mIter;
  int mIterGenCnt;  // the generation count of
                    // mHostRecord->addr_info when we
                    // start iterating
  bool mDone;
};

NS_IMPL_ISUPPORTS(nsDNSRecord, nsIDNSRecord)

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

  if (mHostRecord->addr_info->mCanonicalName.IsEmpty()) {
    result = mHostRecord->addr_info->mHostName;
  } else {
    result = mHostRecord->addr_info->mCanonicalName;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::IsTRR(bool* retval) {
  MutexAutoLock lock(mHostRecord->addr_info_lock);
  if (mHostRecord->addr_info) {
    *retval = mHostRecord->addr_info->IsTRR();
  } else {
    *retval = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetTrrFetchDuration(double* aTime) {
  MutexAutoLock lock(mHostRecord->addr_info_lock);
  if (mHostRecord->addr_info && mHostRecord->addr_info->IsTRR()) {
    *aTime = mHostRecord->addr_info->GetTrrFetchDuration();
  } else {
    *aTime = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetTrrFetchDurationNetworkOnly(double* aTime) {
  MutexAutoLock lock(mHostRecord->addr_info_lock);
  if (mHostRecord->addr_info && mHostRecord->addr_info->IsTRR()) {
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
      mIter = nullptr;
      mIterGenCnt = mHostRecord->addr_info_gencnt;
    }

    bool startedFresh = !mIter;

    do {
      if (!mIter) {
        mIter = mHostRecord->addr_info->mAddresses.getFirst();
      } else {
        mIter = mIter->getNext();
      }
    } while (mIter && mHostRecord->Blacklisted(&mIter->mAddress));

    if (!mIter && startedFresh) {
      // If everything was blacklisted we want to reset the blacklist (and
      // likely relearn it) and return the first address. That is better
      // than nothing.
      mHostRecord->ResetBlacklist();
      mIter = mHostRecord->addr_info->mAddresses.getFirst();
    }

    if (mIter) {
      memcpy(addr, &mIter->mAddress, sizeof(NetAddr));
    }

    mHostRecord->addr_info_lock.Unlock();

    if (!mIter) {
      mDone = true;
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
    for (NetAddrElement* iter = mHostRecord->addr_info->mAddresses.getFirst();
         iter; iter = iter->getNext()) {
      if (mHostRecord->Blacklisted(&iter->mAddress)) {
        continue;
      }
      NetAddr* addr = aAddressArray.AppendElement(NetAddr());
      memcpy(addr, &iter->mAddress, sizeof(NetAddr));
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
  if (NS_FAILED(rv)) return rv;

  RefPtr<nsNetAddr> netaddr = new nsNetAddr(&addr);
  netaddr.forget(result);

  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetNextAddrAsString(nsACString& result) {
  NetAddr addr;
  nsresult rv = GetNextAddr(0, &addr);
  if (NS_FAILED(rv)) return rv;

  char buf[kIPv6CStrBufSize];
  if (NetAddrToString(&addr, buf, sizeof(buf))) {
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

  NetAddrElement* iterCopy = mIter;
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
  mIter = nullptr;
  mIterGenCnt = -1;
  mDone = false;
  return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::ReportUnusable(uint16_t aPort) {
  // right now we don't use the port in the blacklist

  MutexAutoLock lock(mHostRecord->addr_info_lock);

  // Check that we are using a real addr_info (as opposed to a single
  // constant address), and that the generation count is valid. Otherwise,
  // ignore the report.

  if (mHostRecord->addr_info && mIterGenCnt == mHostRecord->addr_info_gencnt &&
      mIter) {
    mHostRecord->ReportUnusable(&mIter->mAddress);
  }

  return NS_OK;
}

class nsDNSByTypeRecord : public nsIDNSByTypeRecord {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_SAFE_NSIDNSRECORD(((nsIDNSRecord*)nullptr))
  NS_DECL_NSIDNSBYTYPERECORD

  explicit nsDNSByTypeRecord(nsHostRecord* hostRecord) {
    mHostRecord = do_QueryObject(hostRecord);
  }

 private:
  virtual ~nsDNSByTypeRecord() = default;
  RefPtr<TypeHostRecord> mHostRecord;
};

NS_IMPL_ISUPPORTS(nsDNSByTypeRecord, nsIDNSRecord, nsIDNSByTypeRecord)

NS_IMETHODIMP
nsDNSByTypeRecord::GetRecords(nsTArray<nsCString>& aRecords) {
  // deep copy
  mHostRecord->GetRecords(aRecords);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSByTypeRecord::GetRecordsAsOneString(nsACString& aRecords) {
  // deep copy
  mHostRecord->GetRecordsAsOneString(aRecords);
  return NS_OK;
}

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
  uint16_t mType;
  const OriginAttributes
      mOriginAttributes;  // The originAttributes for this resolving
  nsCOMPtr<nsIDNSListener> mListener;
  uint16_t mFlags;
  uint16_t mAF;

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
  nsCOMPtr<nsIDNSListenerProxy> wrapper = do_QueryInterface(mListener);
  if (wrapper) {
    nsCOMPtr<nsIDNSListener> originalListener;
    wrapper->GetOriginalListener(getter_AddRefs(originalListener));
    return aListener == originalListener;
  }
  return (aListener == mListener);
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
  mResolver->DetachCallback(mHost, mTrrServer, mType, mOriginAttributes, mFlags,
                            mAF, this, reason);
  return NS_OK;
}

//-----------------------------------------------------------------------------

class nsDNSSyncRequest : public nsResolveHostCallback {
  NS_DECL_THREADSAFE_ISUPPORTS
 public:
  explicit nsDNSSyncRequest(PRMonitor* mon)
      : mDone(false), mStatus(NS_OK), mMonitor(mon) {}

  void OnResolveHostComplete(nsHostResolver*, nsHostRecord*, nsresult) override;
  bool EqualsAsyncListener(nsIDNSListener* aListener) override;
  size_t SizeOfIncludingThis(mozilla::MallocSizeOf) const override;

  bool mDone;
  nsresult mStatus;
  RefPtr<nsHostRecord> mHostRecord;

 private:
  virtual ~nsDNSSyncRequest() = default;

  PRMonitor* mMonitor;
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

nsDNSService::nsDNSService()
    : mLock("nsDNSServer.mLock"),
      mDisableIPv6(false),
      mDisablePrefetch(false),
      mBlockDotOnion(false),
      mNotifyResolution(false),
      mOfflineLocalhost(false),
      mForceResolveOn(false),
      mProxyType(0),
      mTrrService(nullptr),
      mResCacheEntries(0),
      mResCacheExpiration(0),
      mResCacheGrace(0),
      mResolverPrefsUpdated(false) {}

nsDNSService::~nsDNSService() = default;

NS_IMPL_ISUPPORTS(nsDNSService, nsIDNSService, nsPIDNSService, nsIObserver,
                  nsIMemoryReporter)

/******************************************************************************
 * nsDNSService impl:
 * singleton instance ctor/dtor methods
 ******************************************************************************/
static StaticRefPtr<nsDNSService> gDNSService;

already_AddRefed<nsIDNSService> nsDNSService::GetXPCOMSingleton() {
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
}

already_AddRefed<nsDNSService> nsDNSService::GetSingleton() {
  MOZ_ASSERT_IF(nsIOService::UseSocketProcess(), XRE_IsSocketProcess());
  MOZ_ASSERT_IF(!nsIOService::UseSocketProcess(), XRE_IsParentProcess());

  if (!gDNSService) {
    auto initTask = []() {
      gDNSService = new nsDNSService();
      if (NS_SUCCEEDED(gDNSService->Init())) {
        ClearOnShutdown(&gDNSService);
      } else {
        gDNSService = nullptr;
      }
    };

    if (!NS_IsMainThread()) {
      // Forward to the main thread synchronously.
      RefPtr<nsIThread> mainThread = do_GetMainThread();
      if (!mainThread) {
        return nullptr;
      }

      SyncRunnable::DispatchToThread(mainThread,
                                     new SyncRunnable(NS_NewRunnableFunction(
                                         "nsDNSService::Init", initTask)));
    } else {
      initTask();
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
  if (!name || !strcmp(name, kPrefNetworkProxyType)) {
    if (NS_SUCCEEDED(Preferences::GetUint(kPrefNetworkProxyType, &tmpint))) {
      mProxyType = tmpint;
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
    if (!localDomains.IsEmpty()) {
      nsCCharSeparatedTokenizer tokenizer(
          localDomains, ',', nsCCharSeparatedTokenizer::SEPARATOR_OPTIONAL);
      while (tokenizer.hasMoreTokens()) {
        mLocalDomains.PutEntry(tokenizer.nextToken());
      }
    }
  }
  if (!name || !strcmp(name, kPrefDnsForceResolve)) {
    Preferences::GetCString(kPrefDnsForceResolve, mForceResolve);
    mForceResolveOn = !mForceResolve.IsEmpty();
  }

  if (mProxyType == nsIProtocolProxyService::PROXYCONFIG_MANUAL) {
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
    // If a manual proxy is in use, disable prefetch implicitly
    prefs->AddObserver("network.proxy.type", this, false);
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
    res = mResolver;
    mResolver = nullptr;
  }
  if (res) {
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

nsresult nsDNSService::PreprocessHostname(bool aLocalDomain,
                                          const nsACString& aInput,
                                          nsIIDNService* aIDN,
                                          nsACString& aACE) {
  // Enforce RFC 7686
  if (mBlockDotOnion && StringEndsWith(aInput, NS_LITERAL_CSTRING(".onion"))) {
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
    const nsACString& aHostname, const nsACString& aTrrServer, uint16_t type,
    uint32_t flags, nsIDNSListener* aListener, nsIEventTarget* target_,
    const OriginAttributes& aOriginAttributes, nsICancelable** result) {
  // grab reference to global host resolver and IDN service.  beware
  // simultaneous shutdown!!
  RefPtr<nsHostResolver> res;
  nsCOMPtr<nsIIDNService> idn;
  nsCOMPtr<nsIEventTarget> target = target_;
  nsCOMPtr<nsIDNSListener> listener = aListener;
  bool localDomain = false;
  {
    MutexAutoLock lock(mLock);

    if (mDisablePrefetch && (flags & RESOLVE_SPECULATE))
      return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;

    res = mResolver;
    idn = mIDN;
    localDomain = mLocalDomains.GetEntry(aHostname);
  }

  if (mNotifyResolution) {
    NS_DispatchToMainThread(new NotifyDNSResolution(aHostname));
  }

  if (!res) return NS_ERROR_OFFLINE;

  if ((type != RESOLVE_TYPE_DEFAULT) && (type != RESOLVE_TYPE_TXT)) {
    return NS_ERROR_INVALID_ARG;
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
  RefPtr<nsDNSAsyncRequest> req = new nsDNSAsyncRequest(
      res, hostname, aTrrServer, type, aOriginAttributes, listener, flags, af);
  if (!req) return NS_ERROR_OUT_OF_MEMORY;

  rv = res->ResolveHost(req->mHost, req->mTrrServer, type,
                        req->mOriginAttributes, flags, af, req);
  req.forget(result);
  return rv;
}

nsresult nsDNSService::CancelAsyncResolveInternal(
    const nsACString& aHostname, const nsACString& aTrrServer, uint16_t aType,
    uint32_t aFlags, nsIDNSListener* aListener, nsresult aReason,
    const OriginAttributes& aOriginAttributes) {
  // grab reference to global host resolver and IDN service.  beware
  // simultaneous shutdown!!
  RefPtr<nsHostResolver> res;
  nsCOMPtr<nsIIDNService> idn;
  bool localDomain = false;
  {
    MutexAutoLock lock(mLock);

    if (mDisablePrefetch && (aFlags & RESOLVE_SPECULATE))
      return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;

    res = mResolver;
    idn = mIDN;
    localDomain = mLocalDomains.GetEntry(aHostname);
  }
  if (!res) return NS_ERROR_OFFLINE;

  nsCString hostname;
  nsresult rv = PreprocessHostname(localDomain, aHostname, idn, hostname);
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint16_t af =
      (aType != RESOLVE_TYPE_DEFAULT) ? 0 : GetAFForLookup(hostname, aFlags);

  res->CancelAsyncRequest(hostname, aTrrServer, aType, aOriginAttributes,
                          aFlags, af, aListener, aReason);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::AsyncResolve(const nsACString& aHostname, uint32_t flags,
                           nsIDNSListener* listener, nsIEventTarget* target_,
                           JS::HandleValue aOriginAttributes, JSContext* aCx,
                           uint8_t aArgc, nsICancelable** result) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return AsyncResolveInternal(aHostname, EmptyCString(), RESOLVE_TYPE_DEFAULT,
                              flags, listener, target_, attrs, result);
}

NS_IMETHODIMP
nsDNSService::AsyncResolveNative(const nsACString& aHostname, uint32_t flags,
                                 nsIDNSListener* aListener,
                                 nsIEventTarget* target_,
                                 const OriginAttributes& aOriginAttributes,
                                 nsICancelable** result) {
  return AsyncResolveInternal(aHostname, EmptyCString(), RESOLVE_TYPE_DEFAULT,
                              flags, aListener, target_, aOriginAttributes,
                              result);
}

NS_IMETHODIMP
nsDNSService::AsyncResolveWithTrrServer(
    const nsACString& aHostname, const nsACString& aTrrServer, uint32_t flags,
    nsIDNSListener* listener, nsIEventTarget* target_,
    JS::HandleValue aOriginAttributes, JSContext* aCx, uint8_t aArgc,
    nsICancelable** result) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return AsyncResolveInternal(aHostname, aTrrServer, RESOLVE_TYPE_DEFAULT,
                              flags, listener, target_, attrs, result);
}

NS_IMETHODIMP
nsDNSService::AsyncResolveWithTrrServerNative(
    const nsACString& aHostname, const nsACString& aTrrServer, uint32_t flags,
    nsIDNSListener* aListener, nsIEventTarget* target_,
    const OriginAttributes& aOriginAttributes, nsICancelable** result) {
  return AsyncResolveInternal(aHostname, aTrrServer, RESOLVE_TYPE_DEFAULT,
                              flags, aListener, target_, aOriginAttributes,
                              result);
}

NS_IMETHODIMP
nsDNSService::AsyncResolveByType(const nsACString& aHostname, uint16_t aType,
                                 uint32_t aFlags, nsIDNSListener* aListener,
                                 nsIEventTarget* aTarget_,
                                 JS::HandleValue aOriginAttributes,
                                 JSContext* aCx, uint8_t aArgc,
                                 nsICancelable** aResult) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return AsyncResolveInternal(aHostname, EmptyCString(), aType, aFlags,
                              aListener, aTarget_, attrs, aResult);
}

NS_IMETHODIMP
nsDNSService::AsyncResolveByTypeNative(
    const nsACString& aHostname, uint16_t aType, uint32_t aFlags,
    nsIDNSListener* aListener, nsIEventTarget* aTarget_,
    const OriginAttributes& aOriginAttributes, nsICancelable** aResult) {
  return AsyncResolveInternal(aHostname, EmptyCString(), aType, aFlags,
                              aListener, aTarget_, aOriginAttributes, aResult);
}

NS_IMETHODIMP
nsDNSService::CancelAsyncResolve(const nsACString& aHostname, uint32_t aFlags,
                                 nsIDNSListener* aListener, nsresult aReason,
                                 JS::HandleValue aOriginAttributes,
                                 JSContext* aCx, uint8_t aArgc) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return CancelAsyncResolveInternal(aHostname, EmptyCString(),
                                    RESOLVE_TYPE_DEFAULT, aFlags, aListener,
                                    aReason, attrs);
}

NS_IMETHODIMP
nsDNSService::CancelAsyncResolveNative(
    const nsACString& aHostname, uint32_t aFlags, nsIDNSListener* aListener,
    nsresult aReason, const OriginAttributes& aOriginAttributes) {
  return CancelAsyncResolveInternal(aHostname, EmptyCString(),
                                    RESOLVE_TYPE_DEFAULT, aFlags, aListener,
                                    aReason, aOriginAttributes);
}

NS_IMETHODIMP
nsDNSService::CancelAsyncResolveWithTrrServer(
    const nsACString& aHostname, const nsACString& aTrrServer, uint32_t aFlags,
    nsIDNSListener* aListener, nsresult aReason,
    JS::HandleValue aOriginAttributes, JSContext* aCx, uint8_t aArgc) {
  OriginAttributes attrs;

  if (aArgc == 1) {
    if (!aOriginAttributes.isObject() || !attrs.Init(aCx, aOriginAttributes)) {
      return NS_ERROR_INVALID_ARG;
    }
  }

  return CancelAsyncResolveInternal(aHostname, aTrrServer, RESOLVE_TYPE_DEFAULT,
                                    aFlags, aListener, aReason, attrs);
}

NS_IMETHODIMP
nsDNSService::CancelAsyncResolveWithTrrServerNative(
    const nsACString& aHostname, const nsACString& aTrrServer, uint32_t aFlags,
    nsIDNSListener* aListener, nsresult aReason,
    const OriginAttributes& aOriginAttributes) {
  return CancelAsyncResolveInternal(aHostname, aTrrServer, RESOLVE_TYPE_DEFAULT,
                                    aFlags, aListener, aReason,
                                    aOriginAttributes);
}

NS_IMETHODIMP
nsDNSService::CancelAsyncResolveByType(const nsACString& aHostname,
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
nsDNSService::CancelAsyncResolveByTypeNative(
    const nsACString& aHostname, uint16_t aType, uint32_t aFlags,
    nsIDNSListener* aListener, nsresult aReason,
    const OriginAttributes& aOriginAttributes) {
  return CancelAsyncResolveInternal(aHostname, EmptyCString(), aType, aFlags,
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
    localDomain = mLocalDomains.GetEntry(aHostname);
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

  //
  // sync resolve: since the host resolver only works asynchronously, we need
  // to use a mutex and a condvar to wait for the result.  however, since the
  // result may be in the resolvers cache, we might get called back recursively
  // on the same thread.  so, our mutex needs to be re-entrant.  in other words,
  // we need to use a monitor! ;-)
  //

  PRMonitor* mon = PR_NewMonitor();
  if (!mon) return NS_ERROR_OUT_OF_MEMORY;

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

  rv = res->ResolveHost(hostname, EmptyCString(), RESOLVE_TYPE_DEFAULT,
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
nsDNSService::Observe(nsISupports* subject, const char* topic,
                      const char16_t* data) {
  bool flushCache = false;
  if (!strcmp(topic, NS_NETWORK_LINK_TOPIC)) {
    nsAutoCString converted = NS_ConvertUTF16toUTF8(data);
    if (mResolver && !strcmp(converted.get(), NS_NETWORK_LINK_DATA_CHANGED)) {
      flushCache = true;
    }
  } else if (!strcmp(topic, "last-pb-context-exited")) {
    flushCache = true;
  } else if (!strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    ReadPrefs(NS_ConvertUTF16toUTF8(data).get());
    NS_ENSURE_TRUE(mResolver, NS_ERROR_NOT_INITIALIZED);
    if (mResolverPrefsUpdated && mResolver) {
      mResolver->SetCacheLimits(mResCacheEntries, mResCacheExpiration,
                                mResCacheGrace);
    }
  } else if (!strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Shutdown();
  }

  if (flushCache && mResolver) {
    mResolver->FlushCache(false);
    return NS_OK;
  }

  return NS_OK;
}

uint16_t nsDNSService::GetAFForLookup(const nsACString& host, uint32_t flags) {
  if (mDisableIPv6 || (flags & RESOLVE_DISABLE_IPV6)) return PR_AF_INET;

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
      while (*domain == ' ' || *domain == '\t') ++domain;

      // find end of this domain in the string
      end = strchr(domain, ',');
      if (!end) end = domainEnd;

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

  if ((af != PR_AF_INET) && (flags & RESOLVE_DISABLE_IPV4)) af = PR_AF_INET6;

  return af;
}

NS_IMETHODIMP
nsDNSService::GetDNSCacheEntries(
    nsTArray<mozilla::net::DNSCacheEntries>* args) {
  NS_ENSURE_TRUE(mResolver, NS_ERROR_NOT_INITIALIZED);
  mResolver->GetDNSCacheEntries(args);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::ClearCache(bool aTrrToo) {
  NS_ENSURE_TRUE(mResolver, NS_ERROR_NOT_INITIALIZED);
  mResolver->FlushCache(aTrrToo);
  return NS_OK;
}

NS_IMETHODIMP
nsDNSService::ReloadParentalControlEnabled() {
  if (mTrrService) {
    mTrrService->GetParentalControlEnabledInternal();
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
