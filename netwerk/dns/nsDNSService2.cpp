/* vim:set ts=4 sw=4 sts=4 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2003
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDNSService2.h"
#include "nsIDNSRecord.h"
#include "nsIDNSListener.h"
#include "nsICancelable.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranch2.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsAutoPtr.h"
#include "nsNetCID.h"
#include "nsNetError.h"
#include "nsDNSPrefetch.h"
#include "nsThreadUtils.h"
#include "nsIProtocolProxyService.h"
#include "prsystem.h"
#include "prnetdb.h"
#include "prmon.h"
#include "prio.h"
#include "plstr.h"
#include "nsIOService.h"

#include "mozilla/FunctionTimer.h"

using namespace mozilla;

static const char kPrefDnsCacheEntries[]    = "network.dnsCacheEntries";
static const char kPrefDnsCacheExpiration[] = "network.dnsCacheExpiration";
static const char kPrefEnableIDN[]          = "network.enableIDN";
static const char kPrefIPv4OnlyDomains[]    = "network.dns.ipv4OnlyDomains";
static const char kPrefDisableIPv6[]        = "network.dns.disableIPv6";
static const char kPrefDisablePrefetch[]    = "network.dns.disablePrefetch";

//-----------------------------------------------------------------------------

class nsDNSRecord : public nsIDNSRecord
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDNSRECORD

    nsDNSRecord(nsHostRecord *hostRecord)
        : mHostRecord(hostRecord)
        , mIter(nsnull)
        , mLastIter(nsnull)
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
nsDNSRecord::GetNextAddr(PRUint16 port, PRNetAddr *addr)
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
            mIter = nsnull;
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
            mLastIter = nsnull;
            mIter = PR_EnumerateAddrInfo(nsnull, mHostRecord->addr_info,
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
    mIter = nsnull;
    mLastIter = nsnull;
    mIterGenCnt = -1;
    mDone = false;
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::ReportUnusable(PRUint16 aPort)
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

class nsDNSAsyncRequest : public nsResolveHostCallback
                        , public nsICancelable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICANCELABLE

    nsDNSAsyncRequest(nsHostResolver   *res,
                      const nsACString &host,
                      nsIDNSListener   *listener,
                      PRUint16          flags,
                      PRUint16          af)
        : mResolver(res)
        , mHost(host)
        , mListener(listener)
        , mFlags(flags)
        , mAF(af) {}
    ~nsDNSAsyncRequest() {}

    void OnLookupComplete(nsHostResolver *, nsHostRecord *, nsresult);

    nsRefPtr<nsHostResolver> mResolver;
    nsCString                mHost; // hostname we're resolving
    nsCOMPtr<nsIDNSListener> mListener;
    PRUint16                 mFlags;
    PRUint16                 mAF;
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
    mListener = nsnull;

    // release the reference to ourselves that was added before we were
    // handed off to the host resolver.
    NS_RELEASE_THIS();
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

//-----------------------------------------------------------------------------

nsDNSService::nsDNSService()
    : mLock("nsDNSServer.mLock")
    , mFirstTime(true)
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
    NS_TIME_FUNCTION;

    NS_ENSURE_TRUE(!mResolver, NS_ERROR_ALREADY_INITIALIZED);

    // prefs
    PRUint32 maxCacheEntries  = 400;
    PRUint32 maxCacheLifetime = 3; // minutes
    bool     enableIDN        = true;
    bool     disableIPv6      = false;
    bool     disablePrefetch  = false;
    int      proxyType        = nsIProtocolProxyService::PROXYCONFIG_DIRECT;
    
    nsAdoptingCString ipv4OnlyDomains;

    // read prefs
    nsCOMPtr<nsIPrefBranch2> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
        PRInt32 val;
        if (NS_SUCCEEDED(prefs->GetIntPref(kPrefDnsCacheEntries, &val)))
            maxCacheEntries = (PRUint32) val;
        if (NS_SUCCEEDED(prefs->GetIntPref(kPrefDnsCacheExpiration, &val)))
            maxCacheLifetime = val / 60; // convert from seconds to minutes

        // ASSUMPTION: pref branch does not modify out params on failure
        prefs->GetBoolPref(kPrefEnableIDN, &enableIDN);
        prefs->GetBoolPref(kPrefDisableIPv6, &disableIPv6);
        prefs->GetCharPref(kPrefIPv4OnlyDomains, getter_Copies(ipv4OnlyDomains));
        prefs->GetBoolPref(kPrefDisablePrefetch, &disablePrefetch);

        // If a manual proxy is in use, disable prefetch implicitly
        prefs->GetIntPref("network.proxy.type", &proxyType);
    }

    if (mFirstTime) {
        mFirstTime = false;

        // register as prefs observer
        if (prefs) {
            prefs->AddObserver(kPrefDnsCacheEntries, this, false);
            prefs->AddObserver(kPrefDnsCacheExpiration, this, false);
            prefs->AddObserver(kPrefEnableIDN, this, false);
            prefs->AddObserver(kPrefIPv4OnlyDomains, this, false);
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
        mResolver = nsnull;
    }
    if (res)
        res->Shutdown();
    return NS_OK;
}

namespace {

class DNSListenerProxy : public nsIDNSListener
{
public:
  DNSListenerProxy(nsIDNSListener* aListener, nsIEventTarget* aTargetThread)
    : mListener(aListener)
    , mTargetThread(aTargetThread)
  { }

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
                           PRUint32           flags,
                           nsIDNSListener    *listener,
                           nsIEventTarget    *target,
                           nsICancelable    **result)
{
    // grab reference to global host resolver and IDN service.  beware
    // simultaneous shutdown!!
    nsRefPtr<nsHostResolver> res;
    nsCOMPtr<nsIIDNService> idn;
    {
        MutexAutoLock lock(mLock);

        if (mDisablePrefetch && (flags & RESOLVE_SPECULATE))
            return NS_ERROR_DNS_LOOKUP_QUEUE_FULL;

        res = mResolver;
        idn = mIDN;
    }
    if (!res)
        return NS_ERROR_OFFLINE;

    const nsACString *hostPtr = &hostname;

    nsresult rv;
    nsCAutoString hostACE;
    if (idn && !IsASCII(hostname)) {
        if (NS_SUCCEEDED(idn->ConvertUTF8toACE(hostname, hostACE)))
            hostPtr = &hostACE;
    }

    if (target) {
      listener = new DNSListenerProxy(listener, target);
    }

    PRUint16 af = GetAFForLookup(*hostPtr, flags);

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
nsDNSService::Resolve(const nsACString &hostname,
                      PRUint32          flags,
                      nsIDNSRecord    **result)
{
    // grab reference to global host resolver and IDN service.  beware
    // simultaneous shutdown!!
    nsRefPtr<nsHostResolver> res;
    nsCOMPtr<nsIIDNService> idn;
    {
        MutexAutoLock lock(mLock);
        res = mResolver;
        idn = mIDN;
    }
    NS_ENSURE_TRUE(res, NS_ERROR_OFFLINE);

    const nsACString *hostPtr = &hostname;

    nsresult rv;
    nsCAutoString hostACE;
    if (idn && !IsASCII(hostname)) {
        if (NS_SUCCEEDED(idn->ConvertUTF8toACE(hostname, hostACE)))
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

    PRUint16 af = GetAFForLookup(*hostPtr, flags);

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

PRUint16
nsDNSService::GetAFForLookup(const nsACString &host, PRUint32 flags)
{
    if (mDisableIPv6 || (flags & RESOLVE_DISABLE_IPV6))
        return PR_AF_INET;

    MutexAutoLock lock(mLock);

    PRUint16 af = PR_AF_UNSPEC;

    if (!mIPv4OnlyDomains.IsEmpty()) {
        const char *domain, *domainEnd, *end;
        PRUint32 hostLen, domainLen;

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

    return af;
}
