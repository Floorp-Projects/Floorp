/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDNSService2.h"
#include "nsIDNSRecord.h"
#include "nsIDNSListener.h"
#include "nsICancelable.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManager.h"
#include "nsIXPConnect.h"
#include "nsProxyRelease.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsAutoPtr.h"
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
#include "nsIOService.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsNetAddr.h"
#include "nsProxyRelease.h"

#include "mozilla/Attributes.h"
#include "mozilla/VisualEventTracer.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/ChildDNSService.h"
#include "mozilla/net/DNSListenerProxy.h"

using namespace mozilla;
using namespace mozilla::net;

static const char kPrefDnsCacheEntries[]     = "network.dnsCacheEntries";
static const char kPrefDnsCacheExpiration[]  = "network.dnsCacheExpiration";
static const char kPrefDnsCacheGrace[]       = "network.dnsCacheExpirationGracePeriod";
static const char kPrefIPv4OnlyDomains[]     = "network.dns.ipv4OnlyDomains";
static const char kPrefDisableIPv6[]         = "network.dns.disableIPv6";
static const char kPrefDisablePrefetch[]     = "network.dns.disablePrefetch";
static const char kPrefDnsLocalDomains[]     = "network.dns.localDomains";
static const char kPrefDnsNotifyResolution[] = "network.dns.notifyResolution";

//-----------------------------------------------------------------------------

class nsDNSRecord : public nsIDNSRecord
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIDNSRECORD

    nsDNSRecord(nsHostRecord *hostRecord)
        : mHostRecord(hostRecord)
        , mIter(nullptr)
        , mIterGenCnt(-1)
        , mDone(false) {}

private:
    virtual ~nsDNSRecord() {}

    nsRefPtr<nsHostRecord>  mHostRecord;
    NetAddrElement         *mIter;
    int                     mIterGenCnt; // the generation count of
                                         // mHostRecord->addr_info when we
                                         // start iterating
    bool                    mDone;
};

NS_IMPL_ISUPPORTS(nsDNSRecord, nsIDNSRecord)

NS_IMETHODIMP
nsDNSRecord::GetCanonicalName(nsACString &result)
{
    // this method should only be called if we have a CNAME
    NS_ENSURE_TRUE(mHostRecord->flags & nsHostResolver::RES_CANON_NAME,
                   NS_ERROR_NOT_AVAILABLE);

    // if the record is for an IP address literal, then the canonical
    // host name is the IP address literal.
    const char *cname;
    {
        MutexAutoLock lock(mHostRecord->addr_info_lock);
        if (mHostRecord->addr_info)
            cname = mHostRecord->addr_info->mCanonicalName ?
                mHostRecord->addr_info->mCanonicalName :
                mHostRecord->addr_info->mHostName;
        else
            cname = mHostRecord->host;
        result.Assign(cname);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetNextAddr(uint16_t port, NetAddr *addr)
{
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
        }
        while (mIter && mHostRecord->Blacklisted(&mIter->mAddress));

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
    }
    else {
        mHostRecord->addr_info_lock.Unlock();

        if (!mHostRecord->addr) {
            // Both mHostRecord->addr_info and mHostRecord->addr are null.
            // This can happen if mHostRecord->addr_info expired and the
            // attempt to reresolve it failed.
            return NS_ERROR_NOT_AVAILABLE;
        }
        memcpy(addr, mHostRecord->addr, sizeof(NetAddr));
        mDone = true;
    }

    // set given port
    port = htons(port);
    if (addr->raw.family == AF_INET) {
        addr->inet.port = port;
    }
    else if (addr->raw.family == AF_INET6) {
        addr->inet6.port = port;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetScriptableNextAddr(uint16_t port, nsINetAddr * *result)
{
    NetAddr addr;
    nsresult rv = GetNextAddr(port, &addr);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = new nsNetAddr(&addr));

    return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetNextAddrAsString(nsACString &result)
{
    NetAddr addr;
    nsresult rv = GetNextAddr(0, &addr);
    if (NS_FAILED(rv)) return rv;

    char buf[kIPv6CStrBufSize];
    if (NetAddrToString(&addr, buf, sizeof(buf))) {
        result.Assign(buf);
        return NS_OK;
    }
    NS_ERROR("NetAddrToString failed unexpectedly");
    return NS_ERROR_FAILURE; // conversion failed for some reason
}

NS_IMETHODIMP
nsDNSRecord::HasMore(bool *result)
{
    if (mDone) {
        *result = false;
        return NS_OK;
    }

    NetAddrElement *iterCopy = mIter;

    NetAddr addr;
    *result = NS_SUCCEEDED(GetNextAddr(0, &addr));

    mIter = iterCopy;
    mDone = false;

    return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::Rewind()
{
    mIter = nullptr;
    mIterGenCnt = -1;
    mDone = false;
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::ReportUnusable(uint16_t aPort)
{
    // right now we don't use the port in the blacklist

    MutexAutoLock lock(mHostRecord->addr_info_lock);

    // Check that we are using a real addr_info (as opposed to a single
    // constant address), and that the generation count is valid. Otherwise,
    // ignore the report.

    if (mHostRecord->addr_info &&
        mIterGenCnt == mHostRecord->addr_info_gencnt &&
        mIter) {
        mHostRecord->ReportUnusable(&mIter->mAddress);
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------

class nsDNSAsyncRequest MOZ_FINAL : public nsResolveHostCallback
                                  , public nsICancelable
{
    ~nsDNSAsyncRequest() {}

public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSICANCELABLE

    nsDNSAsyncRequest(nsHostResolver   *res,
                      const nsACString &host,
                      nsIDNSListener   *listener,
                      uint16_t          flags,
                      uint16_t          af)
        : mResolver(res)
        , mHost(host)
        , mListener(listener)
        , mFlags(flags)
        , mAF(af) {}

    void OnLookupComplete(nsHostResolver *, nsHostRecord *, nsresult);
    // Returns TRUE if the DNS listener arg is the same as the member listener
    // Used in Cancellations to remove DNS requests associated with a
    // particular hostname and nsIDNSListener
    bool EqualsAsyncListener(nsIDNSListener *aListener);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf) const;

    nsRefPtr<nsHostResolver> mResolver;
    nsCString                mHost; // hostname we're resolving
    nsCOMPtr<nsIDNSListener> mListener;
    uint16_t                 mFlags;
    uint16_t                 mAF;
};

void
nsDNSAsyncRequest::OnLookupComplete(nsHostResolver *resolver,
                                    nsHostRecord   *hostRecord,
                                    nsresult        status)
{
    // need to have an owning ref when we issue the callback to enable
    // the caller to be able to addref/release multiple times without
    // destroying the record prematurely.
    nsCOMPtr<nsIDNSRecord> rec;
    if (NS_SUCCEEDED(status)) {
        NS_ASSERTION(hostRecord, "no host record");
        rec = new nsDNSRecord(hostRecord);
        if (!rec)
            status = NS_ERROR_OUT_OF_MEMORY;
    }

    MOZ_EVENT_TRACER_DONE(this, "net::dns::lookup");

    mListener->OnLookupComplete(this, rec, status);
    mListener = nullptr;

    // release the reference to ourselves that was added before we were
    // handed off to the host resolver.
    NS_RELEASE_THIS();
}

bool
nsDNSAsyncRequest::EqualsAsyncListener(nsIDNSListener *aListener)
{
    return (aListener == mListener);
}

size_t
nsDNSAsyncRequest::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const
{
    size_t n = mallocSizeOf(this);

    // The following fields aren't measured.
    // - mHost, because it's a non-owning pointer
    // - mResolver, because it's a non-owning pointer
    // - mListener, because it's a non-owning pointer

    return n;
}

NS_IMPL_ISUPPORTS(nsDNSAsyncRequest, nsICancelable)

NS_IMETHODIMP
nsDNSAsyncRequest::Cancel(nsresult reason)
{
    NS_ENSURE_ARG(NS_FAILED(reason));
    mResolver->DetachCallback(mHost.get(), mFlags, mAF, this, reason);
    return NS_OK;
}

//-----------------------------------------------------------------------------

class nsDNSSyncRequest : public nsResolveHostCallback
{
public:
    nsDNSSyncRequest(PRMonitor *mon)
        : mDone(false)
        , mStatus(NS_OK)
        , mMonitor(mon) {}
    virtual ~nsDNSSyncRequest() {}

    void OnLookupComplete(nsHostResolver *, nsHostRecord *, nsresult);
    bool EqualsAsyncListener(nsIDNSListener *aListener);
    size_t SizeOfIncludingThis(mozilla::MallocSizeOf) const;

    bool                   mDone;
    nsresult               mStatus;
    nsRefPtr<nsHostRecord> mHostRecord;

private:
    PRMonitor             *mMonitor;
};

void
nsDNSSyncRequest::OnLookupComplete(nsHostResolver *resolver,
                                   nsHostRecord   *hostRecord,
                                   nsresult        status)
{
    // store results, and wake up nsDNSService::Resolve to process results.
    PR_EnterMonitor(mMonitor);
    mDone = true;
    mStatus = status;
    mHostRecord = hostRecord;
    PR_Notify(mMonitor);
    PR_ExitMonitor(mMonitor);
}

bool
nsDNSSyncRequest::EqualsAsyncListener(nsIDNSListener *aListener)
{
    // Sync request: no listener to compare
    return false;
}

size_t
nsDNSSyncRequest::SizeOfIncludingThis(MallocSizeOf mallocSizeOf) const
{
    size_t n = mallocSizeOf(this);

    // The following fields aren't measured.
    // - mHostRecord, because it's a non-owning pointer

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - mMonitor

    return n;
}

class NotifyDNSResolution: public nsRunnable
{
public:
    NotifyDNSResolution(nsMainThreadPtrHandle<nsIObserverService> &aObs,
                        const nsACString &aHostname)
        : mObs(aObs)
        , mHostname(aHostname)
    {
        MOZ_ASSERT(mObs);
    }

    NS_IMETHOD Run()
    {
        MOZ_ASSERT(NS_IsMainThread());
        mObs->NotifyObservers(nullptr,
                              "dns-resolution-request",
                              NS_ConvertUTF8toUTF16(mHostname).get());
        return NS_OK;
    }

private:
    nsMainThreadPtrHandle<nsIObserverService> mObs;
    nsCString                                 mHostname;
};

//-----------------------------------------------------------------------------

nsDNSService::nsDNSService()
    : mLock("nsDNSServer.mLock")
    , mFirstTime(true)
    , mOffline(false)
{
}

nsDNSService::~nsDNSService()
{
}

NS_IMPL_ISUPPORTS(nsDNSService, nsIDNSService, nsPIDNSService, nsIObserver,
                  nsIMemoryReporter)

/******************************************************************************
 * nsDNSService impl:
 * singleton instance ctor/dtor methods
 ******************************************************************************/
static nsDNSService *gDNSService;

nsIDNSService*
nsDNSService::GetXPCOMSingleton()
{
    if (IsNeckoChild()) {
        return ChildDNSService::GetSingleton();
    }

    return GetSingleton();
}

nsDNSService*
nsDNSService::GetSingleton()
{
    NS_ASSERTION(!IsNeckoChild(), "not a parent process");

    if (gDNSService) {
        NS_ADDREF(gDNSService);
        return gDNSService;
    }

    gDNSService = new nsDNSService();
    if (gDNSService) {
        NS_ADDREF(gDNSService);
        if (NS_FAILED(gDNSService->Init())) {
              NS_RELEASE(gDNSService);
        }
    }

    return gDNSService;
}

NS_IMETHODIMP
nsDNSService::Init()
{
    if (mResolver)
        return NS_OK;
    NS_ENSURE_TRUE(!mResolver, NS_ERROR_ALREADY_INITIALIZED);

    // prefs
    uint32_t maxCacheEntries  = 400;
    uint32_t maxCacheLifetime = 120; // seconds
    uint32_t lifetimeGracePeriod = 60; // seconds
    bool     disableIPv6      = false;
    bool     disablePrefetch  = false;
    int      proxyType        = nsIProtocolProxyService::PROXYCONFIG_DIRECT;
    bool     notifyResolution = false;

    nsAdoptingCString ipv4OnlyDomains;
    nsAdoptingCString localDomains;

    // read prefs
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
        int32_t val;
        if (NS_SUCCEEDED(prefs->GetIntPref(kPrefDnsCacheEntries, &val)))
            maxCacheEntries = (uint32_t) val;
        if (NS_SUCCEEDED(prefs->GetIntPref(kPrefDnsCacheExpiration, &val)))
            maxCacheLifetime = val;
        if (NS_SUCCEEDED(prefs->GetIntPref(kPrefDnsCacheGrace, &val)))
            lifetimeGracePeriod = val;

        // ASSUMPTION: pref branch does not modify out params on failure
        prefs->GetBoolPref(kPrefDisableIPv6, &disableIPv6);
        prefs->GetCharPref(kPrefIPv4OnlyDomains, getter_Copies(ipv4OnlyDomains));
        prefs->GetCharPref(kPrefDnsLocalDomains, getter_Copies(localDomains));
        prefs->GetBoolPref(kPrefDisablePrefetch, &disablePrefetch);

        // If a manual proxy is in use, disable prefetch implicitly
        prefs->GetIntPref("network.proxy.type", &proxyType);
        prefs->GetBoolPref(kPrefDnsNotifyResolution, &notifyResolution);
    }

    if (mFirstTime) {
        mFirstTime = false;

        // register as prefs observer
        if (prefs) {
            prefs->AddObserver(kPrefDnsCacheEntries, this, false);
            prefs->AddObserver(kPrefDnsCacheExpiration, this, false);
            prefs->AddObserver(kPrefDnsCacheGrace, this, false);
            prefs->AddObserver(kPrefIPv4OnlyDomains, this, false);
            prefs->AddObserver(kPrefDnsLocalDomains, this, false);
            prefs->AddObserver(kPrefDisableIPv6, this, false);
            prefs->AddObserver(kPrefDisablePrefetch, this, false);
            prefs->AddObserver(kPrefDnsNotifyResolution, this, false);

            // Monitor these to see if there is a change in proxy configuration
            // If a manual proxy is in use, disable prefetch implicitly
            prefs->AddObserver("network.proxy.type", this, false);
        }

        nsresult rv;
        nsCOMPtr<nsIObserverService> observerService =
            do_GetService("@mozilla.org/observer-service;1", &rv);
        if (NS_SUCCEEDED(rv)) {
            observerService->AddObserver(this, "last-pb-context-exited", false);
        }
    }

    nsDNSPrefetch::Initialize(this);

    // Don't initialize the resolver if we're in offline mode.
    // Later on, the IO service will reinitialize us when going online.
    if (gIOService->IsOffline() && !gIOService->IsComingOnline())
        return NS_OK;

    nsCOMPtr<nsIIDNService> idn = do_GetService(NS_IDNSERVICE_CONTRACTID);

    nsCOMPtr<nsIObserverService> obs =
        do_GetService(NS_OBSERVERSERVICE_CONTRACTID);

    nsRefPtr<nsHostResolver> res;
    nsresult rv = nsHostResolver::Create(maxCacheEntries,
                                         maxCacheLifetime,
                                         lifetimeGracePeriod,
                                         getter_AddRefs(res));
    if (NS_SUCCEEDED(rv)) {
        // now, set all of our member variables while holding the lock
        MutexAutoLock lock(mLock);
        mResolver = res;
        mIDN = idn;
        mIPv4OnlyDomains = ipv4OnlyDomains; // exchanges buffer ownership
        mDisableIPv6 = disableIPv6;

        // Disable prefetching either by explicit preference or if a manual proxy is configured 
        mDisablePrefetch = disablePrefetch || (proxyType == nsIProtocolProxyService::PROXYCONFIG_MANUAL);

        mLocalDomains.Clear();
        if (localDomains) {
            nsAdoptingString domains;
            domains.AssignASCII(nsDependentCString(localDomains).get());
            nsCharSeparatedTokenizer tokenizer(domains, ',',
                                               nsCharSeparatedTokenizerTemplate<>::SEPARATOR_OPTIONAL);

            while (tokenizer.hasMoreTokens()) {
                const nsSubstring& domain = tokenizer.nextToken();
                mLocalDomains.PutEntry(nsDependentCString(NS_ConvertUTF16toUTF8(domain).get()));
            }
        }
        mNotifyResolution = notifyResolution;
        if (mNotifyResolution) {
            mObserverService =
              new nsMainThreadPtrHolder<nsIObserverService>(obs);
        }
    }

    RegisterWeakMemoryReporter(this);

    return rv;
}

NS_IMETHODIMP
nsDNSService::Shutdown()
{
    UnregisterWeakMemoryReporter(this);

    nsRefPtr<nsHostResolver> res;
    {
        MutexAutoLock lock(mLock);
        res = mResolver;
        mResolver = nullptr;
    }
    if (res)
        res->Shutdown();
    return NS_OK;
}

NS_IMETHODIMP
nsDNSService::GetOffline(bool *offline)
{
    *offline = mOffline;
    return NS_OK;
}

NS_IMETHODIMP
nsDNSService::SetOffline(bool offline)
{
    mOffline = offline;
    return NS_OK;
}

NS_IMETHODIMP
nsDNSService::GetPrefetchEnabled(bool *outVal)
{
    *outVal = !mDisablePrefetch;
    return NS_OK;
}

NS_IMETHODIMP
nsDNSService::SetPrefetchEnabled(bool inVal)
{
    mDisablePrefetch = !inVal;
    return NS_OK;
}


NS_IMETHODIMP
nsDNSService::AsyncResolve(const nsACString  &hostname,
                           uint32_t           flags,
                           nsIDNSListener    *listener,
                           nsIEventTarget    *target_,
                           nsICancelable    **result)
{
    // grab reference to global host resolver and IDN service.  beware
    // simultaneous shutdown!!
    nsRefPtr<nsHostResolver> res;
    nsCOMPtr<nsIIDNService> idn;
    nsCOMPtr<nsIEventTarget> target = target_;
    bool localDomain = false;
    {
        MutexAutoLock lock(mLock);

        if (mDisablePrefetch && (flags & RESOLVE_SPECULATE))
            return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;

        res = mResolver;
        idn = mIDN;
        localDomain = mLocalDomains.GetEntry(hostname);
    }

    if (mNotifyResolution) {
        NS_DispatchToMainThread(new NotifyDNSResolution(mObserverService,
                                                        hostname));
    }

    if (!res)
        return NS_ERROR_OFFLINE;

    if (mOffline)
        flags |= RESOLVE_OFFLINE;

    const nsACString *hostPtr = &hostname;

    if (localDomain) {
        hostPtr = &(NS_LITERAL_CSTRING("localhost"));
    }

    nsresult rv;
    nsAutoCString hostACE;
    if (idn && !IsASCII(*hostPtr)) {
        if (NS_SUCCEEDED(idn->ConvertUTF8toACE(*hostPtr, hostACE)))
            hostPtr = &hostACE;
    }

    // make sure JS callers get notification on the main thread
    nsCOMPtr<nsIXPConnectWrappedJS> wrappedListener = do_QueryInterface(listener);
    if (wrappedListener && !target) {
        nsCOMPtr<nsIThread> mainThread;
        NS_GetMainThread(getter_AddRefs(mainThread));
        target = do_QueryInterface(mainThread);
    }

    if (target) {
      listener = new DNSListenerProxy(listener, target);
    }

    uint16_t af = GetAFForLookup(*hostPtr, flags);

    nsDNSAsyncRequest *req =
            new nsDNSAsyncRequest(res, *hostPtr, listener, flags, af);
    if (!req)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*result = req);

    MOZ_EVENT_TRACER_NAME_OBJECT(req, hostname.BeginReading());
    MOZ_EVENT_TRACER_WAIT(req, "net::dns::lookup");

    // addref for resolver; will be released when OnLookupComplete is called.
    NS_ADDREF(req);
    rv = res->ResolveHost(req->mHost.get(), flags, af, req);
    if (NS_FAILED(rv)) {
        NS_RELEASE(req);
        NS_RELEASE(*result);
    }
    return rv;
}

NS_IMETHODIMP
nsDNSService::CancelAsyncResolve(const nsACString  &aHostname,
                                 uint32_t           aFlags,
                                 nsIDNSListener    *aListener,
                                 nsresult           aReason)
{
    // grab reference to global host resolver and IDN service.  beware
    // simultaneous shutdown!!
    nsRefPtr<nsHostResolver> res;
    nsCOMPtr<nsIIDNService> idn;
    {
        MutexAutoLock lock(mLock);

        if (mDisablePrefetch && (aFlags & RESOLVE_SPECULATE))
            return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;

        res = mResolver;
        idn = mIDN;
    }
    if (!res)
        return NS_ERROR_OFFLINE;

    nsCString hostname(aHostname);

    nsAutoCString hostACE;
    if (idn && !IsASCII(aHostname)) {
        if (NS_SUCCEEDED(idn->ConvertUTF8toACE(aHostname, hostACE)))
            hostname = hostACE;
    }

    uint16_t af = GetAFForLookup(hostname, aFlags);

    res->CancelAsyncRequest(hostname.get(), aFlags, af, aListener, aReason);
    return NS_OK;
}

NS_IMETHODIMP
nsDNSService::Resolve(const nsACString &hostname,
                      uint32_t          flags,
                      nsIDNSRecord    **result)
{
    // grab reference to global host resolver and IDN service.  beware
    // simultaneous shutdown!!
    nsRefPtr<nsHostResolver> res;
    nsCOMPtr<nsIIDNService> idn;
    bool localDomain = false;
    {
        MutexAutoLock lock(mLock);
        res = mResolver;
        idn = mIDN;
        localDomain = mLocalDomains.GetEntry(hostname);
    }

    if (mNotifyResolution) {
        NS_DispatchToMainThread(new NotifyDNSResolution(mObserverService,
                                                        hostname));
    }

    NS_ENSURE_TRUE(res, NS_ERROR_OFFLINE);

    if (mOffline)
        flags |= RESOLVE_OFFLINE;

    const nsACString *hostPtr = &hostname;

    if (localDomain) {
        hostPtr = &(NS_LITERAL_CSTRING("localhost"));
    }

    nsresult rv;
    nsAutoCString hostACE;
    if (idn && !IsASCII(*hostPtr)) {
        if (NS_SUCCEEDED(idn->ConvertUTF8toACE(*hostPtr, hostACE)))
            hostPtr = &hostACE;
    }

    //
    // sync resolve: since the host resolver only works asynchronously, we need
    // to use a mutex and a condvar to wait for the result.  however, since the
    // result may be in the resolvers cache, we might get called back recursively
    // on the same thread.  so, our mutex needs to be re-entrant.  in other words,
    // we need to use a monitor! ;-)
    //
    
    PRMonitor *mon = PR_NewMonitor();
    if (!mon)
        return NS_ERROR_OUT_OF_MEMORY;

    PR_EnterMonitor(mon);
    nsDNSSyncRequest syncReq(mon);

    uint16_t af = GetAFForLookup(*hostPtr, flags);

    rv = res->ResolveHost(PromiseFlatCString(*hostPtr).get(), flags, af, &syncReq);
    if (NS_SUCCEEDED(rv)) {
        // wait for result
        while (!syncReq.mDone)
            PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);

        if (NS_FAILED(syncReq.mStatus))
            rv = syncReq.mStatus;
        else {
            NS_ASSERTION(syncReq.mHostRecord, "no host record");
            nsDNSRecord *rec = new nsDNSRecord(syncReq.mHostRecord);
            if (!rec)
                rv = NS_ERROR_OUT_OF_MEMORY;
            else
                NS_ADDREF(*result = rec);
        }
    }

    PR_ExitMonitor(mon);
    PR_DestroyMonitor(mon);
    return rv;
}

NS_IMETHODIMP
nsDNSService::GetMyHostName(nsACString &result)
{
    char name[100];
    if (PR_GetSystemInfo(PR_SI_HOSTNAME, name, sizeof(name)) == PR_SUCCESS) {
        result = name;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDNSService::Observe(nsISupports *subject, const char *topic, const char16_t *data)
{
    // we are only getting called if a preference has changed. 
    NS_ASSERTION(strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0 ||
        strcmp(topic, "last-pb-context-exited") == 0,
        "unexpected observe call");

    //
    // Shutdown and this function are both only called on the UI thread, so we don't
    // have to worry about mResolver being cleared out from under us.
    //
    // NOTE Shutting down and reinitializing the service like this is obviously
    // suboptimal if Observe gets called several times in a row, but we don't
    // expect that to be the case.
    //

    if (mResolver) {
        Shutdown();
    }
    Init();
    return NS_OK;
}

uint16_t
nsDNSService::GetAFForLookup(const nsACString &host, uint32_t flags)
{
    if (mDisableIPv6 || (flags & RESOLVE_DISABLE_IPV6))
        return PR_AF_INET;

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
            while (*domain == ' ' || *domain == '\t')
                ++domain;

            // find end of this domain in the string
            end = strchr(domain, ',');
            if (!end)
                end = domainEnd;

            // to see if the hostname is in the domain, check if the domain
            // matches the end of the hostname.
            domainLen = end - domain;
            if (domainLen && hostLen >= domainLen) {
                const char *hostTail = hostStart.get() + hostLen - domainLen;
                if (PL_strncasecmp(domain, hostTail, domainLen) == 0) {
                    // now, make sure either that the hostname is a direct match or
                    // that the hostname begins with a dot.
                    if (hostLen == domainLen ||
                            *hostTail == '.' || *(hostTail - 1) == '.') {
                        af = PR_AF_INET;
                        break;
                    }
                }
            }

            domain = end + 1;
        } while (*end);
    }

    if ((af != PR_AF_INET) && (flags & RESOLVE_DISABLE_IPV4))
        af = PR_AF_INET6;

    return af;
}

NS_IMETHODIMP
nsDNSService::GetDNSCacheEntries(nsTArray<mozilla::net::DNSCacheEntries> *args)
{
    NS_ENSURE_TRUE(mResolver, NS_ERROR_NOT_INITIALIZED);
    mResolver->GetDNSCacheEntries(args);
    return NS_OK;
}

static size_t
SizeOfLocalDomainsEntryExcludingThis(nsCStringHashKey* entry,
                                     MallocSizeOf mallocSizeOf, void*)
{
    return entry->GetKey().SizeOfExcludingThisMustBeUnshared(mallocSizeOf);
}

size_t
nsDNSService::SizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - mIDN
    // - mLock

    size_t n = mallocSizeOf(this);
    n += mResolver->SizeOfIncludingThis(mallocSizeOf);
    n += mIPv4OnlyDomains.SizeOfExcludingThisMustBeUnshared(mallocSizeOf);
    n += mLocalDomains.SizeOfExcludingThis(SizeOfLocalDomainsEntryExcludingThis,
                                           mallocSizeOf);
    return n;
}

MOZ_DEFINE_MALLOC_SIZE_OF(DNSServiceMallocSizeOf)

NS_IMETHODIMP
nsDNSService::CollectReports(nsIHandleReportCallback* aHandleReport,
                             nsISupports* aData, bool aAnonymize)
{
    return MOZ_COLLECT_REPORT(
        "explicit/network/dns-service", KIND_HEAP, UNITS_BYTES,
        SizeOfIncludingThis(DNSServiceMallocSizeOf),
        "Memory used for the DNS service.");
}

