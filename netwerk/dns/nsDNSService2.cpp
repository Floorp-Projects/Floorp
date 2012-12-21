/* vim:set ts=4 sw=4 sts=4 et cin: */
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

#include "mozilla/Attributes.h"

using namespace mozilla;

static const char kPrefDnsCacheEntries[]    = "network.dnsCacheEntries";
static const char kPrefDnsCacheExpiration[] = "network.dnsCacheExpiration";
static const char kPrefDnsCacheGrace[]      = "network.dnsCacheExpirationGracePeriod";
static const char kPrefEnableIDN[]          = "network.enableIDN";
static const char kPrefIPv4OnlyDomains[]    = "network.dns.ipv4OnlyDomains";
static const char kPrefDisableIPv6[]        = "network.dns.disableIPv6";
static const char kPrefDisablePrefetch[]    = "network.dns.disablePrefetch";
static const char kPrefDnsLocalDomains[]    = "network.dns.localDomains";

//-----------------------------------------------------------------------------

class nsDNSRecord : public nsIDNSRecord
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDNSRECORD

    nsDNSRecord(nsHostRecord *hostRecord)
        : mHostRecord(hostRecord)
        , mIter(nullptr)
        , mLastIter(nullptr)
        , mIterGenCnt(-1)
        , mDone(false) {}

private:
    virtual ~nsDNSRecord() {}

    nsRefPtr<nsHostRecord>  mHostRecord;
    void                   *mIter;       // enum ptr for PR_EnumerateAddrInfo
    void                   *mLastIter;   // previous enum ptr, for use in
                                         // getting addrinfo in ReportUnusable
    int                     mIterGenCnt; // the generation count of
                                         // mHostRecord->addr_info when we
                                         // start iterating
    bool                    mDone;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDNSRecord, nsIDNSRecord)

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
            cname = PR_GetCanonNameFromAddrInfo(mHostRecord->addr_info);
        else
            cname = mHostRecord->host;
        result.Assign(cname);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetNextAddr(uint16_t port, PRNetAddr *addr)
{
    // not a programming error to poke the DNS record when it has no more
    // entries.  just fail without any debug warnings.  this enables consumers
    // to enumerate the DNS record without calling HasMore.
    if (mDone)
        return NS_ERROR_NOT_AVAILABLE;

    mHostRecord->addr_info_lock.Lock();
    bool startedFresh = !mIter;

    if (mHostRecord->addr_info) {
        if (!mIter)
            mIterGenCnt = mHostRecord->addr_info_gencnt;
        else if (mIterGenCnt != mHostRecord->addr_info_gencnt) {
            // mHostRecord->addr_info has changed, so mIter is invalid.
            // Restart the iteration.  Alternatively, we could just fail.
            mIter = nullptr;
            mIterGenCnt = mHostRecord->addr_info_gencnt;
            startedFresh = true;
        }

        do {
            mLastIter = mIter;
            mIter = PR_EnumerateAddrInfo(mIter, mHostRecord->addr_info,
                                         port, addr);
        }
        while (mIter && mHostRecord->Blacklisted(addr));

        if (startedFresh && !mIter) {
            // if everything was blacklisted we want to reset the blacklist (and
            // likely relearn it) and return the first address. That is better
            // than nothing
            mHostRecord->ResetBlacklist();
            mLastIter = nullptr;
            mIter = PR_EnumerateAddrInfo(nullptr, mHostRecord->addr_info,
                                         port, addr);
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
        memcpy(addr, mHostRecord->addr, sizeof(PRNetAddr));
        // set given port
        port = PR_htons(port);
        if (addr->raw.family == PR_AF_INET)
            addr->inet.port = port;
        else
            addr->ipv6.port = port;
        mDone = true; // no iterations
    }
        
    return NS_OK; 
}

NS_IMETHODIMP
nsDNSRecord::GetNextAddrAsString(nsACString &result)
{
    PRNetAddr addr;
    nsresult rv = GetNextAddr(0, &addr);
    if (NS_FAILED(rv)) return rv;

    char buf[64];
    if (PR_NetAddrToString(&addr, buf, sizeof(buf)) == PR_SUCCESS) {
        result.Assign(buf);
        return NS_OK;
    }
    NS_ERROR("PR_NetAddrToString failed unexpectedly");
    return NS_ERROR_FAILURE; // conversion failed for some reason
}

NS_IMETHODIMP
nsDNSRecord::HasMore(bool *result)
{
    if (mDone)
        *result = false;
    else {
        // unfortunately, NSPR does not provide a way for us to determine if
        // there is another address other than to simply get the next address.
        void *iterCopy = mIter;
        void *iterLastCopy = mLastIter;
        PRNetAddr addr;
        *result = NS_SUCCEEDED(GetNextAddr(0, &addr));
        mIter = iterCopy; // backup iterator
        mLastIter = iterLastCopy; // backup iterator
        mDone = false;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::Rewind()
{
    mIter = nullptr;
    mLastIter = nullptr;
    mIterGenCnt = -1;
    mDone = false;
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::ReportUnusable(uint16_t aPort)
{
    // right now we don't use the port in the blacklist

    mHostRecord->addr_info_lock.Lock();

    // Check that we are using a real addr_info (as opposed to a single
    // constant address), and that the generation count is valid. Otherwise,
    // ignore the report.

    if (mHostRecord->addr_info &&
        mIterGenCnt == mHostRecord->addr_info_gencnt) {
        PRNetAddr addr;
        void *id = PR_EnumerateAddrInfo(mLastIter, mHostRecord->addr_info,
                                        aPort, &addr);
        if (id)
            mHostRecord->ReportUnusable(&addr);
    }
    
    mHostRecord->addr_info_lock.Unlock();
    return NS_OK;
}

//-----------------------------------------------------------------------------

class nsDNSAsyncRequest MOZ_FINAL : public nsResolveHostCallback
                                  , public nsICancelable
{
public:
    NS_DECL_ISUPPORTS
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
    ~nsDNSAsyncRequest() {}

    void OnLookupComplete(nsHostResolver *, nsHostRecord *, nsresult);
    // Returns TRUE if the DNS listener arg is the same as the member listener
    // Used in Cancellations to remove DNS requests associated with a
    // particular hostname and nsIDNSListener
    bool EqualsAsyncListener(nsIDNSListener *aListener);

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

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDNSAsyncRequest, nsICancelable)

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

NS_IMPL_THREADSAFE_ISUPPORTS3(nsDNSService, nsIDNSService, nsPIDNSService,
                              nsIObserver)

NS_IMETHODIMP
nsDNSService::Init()
{
    if (mResolver)
        return NS_OK;
    NS_ENSURE_TRUE(!mResolver, NS_ERROR_ALREADY_INITIALIZED);

    // prefs
    uint32_t maxCacheEntries  = 400;
    uint32_t maxCacheLifetime = 2; // minutes
    uint32_t lifetimeGracePeriod = 1;
    bool     enableIDN        = true;
    bool     disableIPv6      = false;
    bool     disablePrefetch  = false;
    int      proxyType        = nsIProtocolProxyService::PROXYCONFIG_DIRECT;
    
    nsAdoptingCString ipv4OnlyDomains;
    nsAdoptingCString localDomains;

    // read prefs
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
        int32_t val;
        if (NS_SUCCEEDED(prefs->GetIntPref(kPrefDnsCacheEntries, &val)))
            maxCacheEntries = (uint32_t) val;
        if (NS_SUCCEEDED(prefs->GetIntPref(kPrefDnsCacheExpiration, &val)))
            maxCacheLifetime = val / 60; // convert from seconds to minutes
        if (NS_SUCCEEDED(prefs->GetIntPref(kPrefDnsCacheGrace, &val)))
            lifetimeGracePeriod = val / 60; // convert from seconds to minutes

        // ASSUMPTION: pref branch does not modify out params on failure
        prefs->GetBoolPref(kPrefEnableIDN, &enableIDN);
        prefs->GetBoolPref(kPrefDisableIPv6, &disableIPv6);
        prefs->GetCharPref(kPrefIPv4OnlyDomains, getter_Copies(ipv4OnlyDomains));
        prefs->GetCharPref(kPrefDnsLocalDomains, getter_Copies(localDomains));
        prefs->GetBoolPref(kPrefDisablePrefetch, &disablePrefetch);

        // If a manual proxy is in use, disable prefetch implicitly
        prefs->GetIntPref("network.proxy.type", &proxyType);
    }

    if (mFirstTime) {
        mFirstTime = false;

        mLocalDomains.Init();

        // register as prefs observer
        if (prefs) {
            prefs->AddObserver(kPrefDnsCacheEntries, this, false);
            prefs->AddObserver(kPrefDnsCacheExpiration, this, false);
            prefs->AddObserver(kPrefDnsCacheGrace, this, false);
            prefs->AddObserver(kPrefEnableIDN, this, false);
            prefs->AddObserver(kPrefIPv4OnlyDomains, this, false);
            prefs->AddObserver(kPrefDnsLocalDomains, this, false);
            prefs->AddObserver(kPrefDisableIPv6, this, false);
            prefs->AddObserver(kPrefDisablePrefetch, this, false);

            // Monitor these to see if there is a change in proxy configuration
            // If a manual proxy is in use, disable prefetch implicitly
            prefs->AddObserver("network.proxy.type", this, false);
        }
    }

    // we have to null out mIDN since we might be getting re-initialized
    // as a result of a pref change.
    nsCOMPtr<nsIIDNService> idn;
    if (enableIDN)
        idn = do_GetService(NS_IDNSERVICE_CONTRACTID);

    nsDNSPrefetch::Initialize(this);

    // Don't initialize the resolver if we're in offline mode.
    // Later on, the IO service will reinitialize us when going online.
    if (gIOService->IsOffline() && !gIOService->IsComingOnline())
        return NS_OK;

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
    }
    return rv;
}

NS_IMETHODIMP
nsDNSService::Shutdown()
{
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

namespace {

class DNSListenerProxy MOZ_FINAL : public nsIDNSListener
{
public:
  DNSListenerProxy(nsIDNSListener* aListener, nsIEventTarget* aTargetThread)
    : mListener(aListener)
    , mTargetThread(aTargetThread)
  { }

  ~DNSListenerProxy()
  {
    nsCOMPtr<nsIThread> mainThread(do_GetMainThread());
    NS_ProxyRelease(mainThread, mListener);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDNSLISTENER

  class OnLookupCompleteRunnable : public nsRunnable
  {
  public:
    OnLookupCompleteRunnable(nsIDNSListener* aListener,
                             nsICancelable* aRequest,
                             nsIDNSRecord* aRecord,
                             nsresult aStatus)
      : mListener(aListener)
      , mRequest(aRequest)
      , mRecord(aRecord)
      , mStatus(aStatus)
    { }

    ~OnLookupCompleteRunnable()
    {
      nsCOMPtr<nsIThread> mainThread(do_GetMainThread());
      NS_ProxyRelease(mainThread, mListener);
    }

    NS_DECL_NSIRUNNABLE

  private:
    nsCOMPtr<nsIDNSListener> mListener;
    nsCOMPtr<nsICancelable> mRequest;
    nsCOMPtr<nsIDNSRecord> mRecord;
    nsresult mStatus;
  };

private:
  nsCOMPtr<nsIDNSListener> mListener;
  nsCOMPtr<nsIEventTarget> mTargetThread;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(DNSListenerProxy, nsIDNSListener)

NS_IMETHODIMP
DNSListenerProxy::OnLookupComplete(nsICancelable* aRequest,
                                   nsIDNSRecord* aRecord,
                                   nsresult aStatus)
{
  nsRefPtr<OnLookupCompleteRunnable> r =
    new OnLookupCompleteRunnable(mListener, aRequest, aRecord, aStatus);
  return mTargetThread->Dispatch(r, NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
DNSListenerProxy::OnLookupCompleteRunnable::Run()
{
  mListener->OnLookupComplete(mRequest, mRecord, mStatus);
  return NS_OK;
}

} // anonymous namespace

NS_IMETHODIMP
nsDNSService::AsyncResolve(const nsACString  &hostname,
                           uint32_t           flags,
                           nsIDNSListener    *listener,
                           nsIEventTarget    *target,
                           nsICancelable    **result)
{
    // grab reference to global host resolver and IDN service.  beware
    // simultaneous shutdown!!
    nsRefPtr<nsHostResolver> res;
    nsCOMPtr<nsIIDNService> idn;
    bool localDomain = false;
    {
        MutexAutoLock lock(mLock);

        if (mDisablePrefetch && (flags & RESOLVE_SPECULATE))
            return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;

        res = mResolver;
        idn = mIDN;
        localDomain = mLocalDomains.GetEntry(hostname);
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

    if (target) {
      listener = new DNSListenerProxy(listener, target);
    }

    uint16_t af = GetAFForLookup(*hostPtr, flags);

    nsDNSAsyncRequest *req =
            new nsDNSAsyncRequest(res, *hostPtr, listener, flags, af);
    if (!req)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*result = req);

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
    NS_WARNING("Do not use synchronous DNS resolution! This API may be removed soon.");

    // We will not allow this to be called on the main thread. This is transitional
    // and a bit of a test for removing the synchronous API entirely.
    if (NS_IsMainThread()) {
        NS_ERROR("Synchronous DNS resolve failing - not allowed on the main thread!");
        return NS_ERROR_FAILURE;
    }

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
nsDNSService::Observe(nsISupports *subject, const char *topic, const PRUnichar *data)
{
    // we are only getting called if a preference has changed. 
    NS_ASSERTION(strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0,
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
