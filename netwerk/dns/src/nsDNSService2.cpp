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
#include "nsIProxyObjectManager.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsAutoLock.h"
#include "nsAutoPtr.h"
#include "nsNetCID.h"
#include "nsNetError.h"
#include "prsystem.h"
#include "prnetdb.h"
#include "prmon.h"
#include "prio.h"
#include "plstr.h"

static const char kPrefDnsCacheEntries[]    = "network.dnsCacheEntries";
static const char kPrefDnsCacheExpiration[] = "network.dnsCacheExpiration";
static const char kPrefEnableIDN[]          = "network.enableIDN";
static const char kPrefIPv4OnlyDomains[]    = "network.dns.ipv4OnlyDomains";
static const char kPrefDisableIPv6[]        = "network.dns.disableIPv6";

//-----------------------------------------------------------------------------

class nsDNSRecord : public nsIDNSRecord
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDNSRECORD

    nsDNSRecord(nsHostRecord *hostRecord)
        : mHostRecord(hostRecord)
        , mIter(nsnull)
        , mDone(PR_FALSE) {}

private:
    virtual ~nsDNSRecord() {}

    nsRefPtr<nsHostRecord>  mHostRecord;
    void                   *mIter;
    PRBool                  mDone;
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
    if (mHostRecord->addr_info)
        cname = PR_GetCanonNameFromAddrInfo(mHostRecord->addr_info);
    else
        cname = mHostRecord->host;
    result.Assign(cname);
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

    if (mHostRecord->addr_info) {
        mIter = PR_EnumerateAddrInfo(mIter, mHostRecord->addr_info, port, addr);
        if (!mIter)
            return NS_ERROR_NOT_AVAILABLE;
    }
    else {
        mIter = nsnull; // no iterations
        NS_ASSERTION(mHostRecord->addr, "no addr");
        memcpy(addr, mHostRecord->addr, sizeof(PRNetAddr));
        // set given port
        port = PR_htons(port);
        if (addr->raw.family == PR_AF_INET)
            addr->inet.port = port;
        else
            addr->ipv6.port = port;
    }
        
    mDone = !mIter;
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
nsDNSRecord::HasMore(PRBool *result)
{
    if (mDone)
        *result = PR_FALSE;
    else {
        // unfortunately, NSPR does not provide a way for us to determine if
        // there is another address other than to simply get the next address.
        void *iterCopy = mIter;
        PRNetAddr addr;
        *result = NS_SUCCEEDED(GetNextAddr(0, &addr));
        mIter = iterCopy; // backup iterator
        mDone = PR_FALSE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::Rewind()
{
    mIter = nsnull;
    mDone = PR_FALSE;
    return NS_OK;
}

//-----------------------------------------------------------------------------

class nsDNSAsyncRequest : public nsResolveHostCallback
                        , public nsIDNSRequest
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDNSREQUEST

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

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDNSAsyncRequest, nsIDNSRequest)

NS_IMETHODIMP
nsDNSAsyncRequest::Cancel()
{
    mResolver->DetachCallback(mHost.get(), mFlags, mAF, this);
    return NS_OK;
}

//-----------------------------------------------------------------------------

class nsDNSSyncRequest : public nsResolveHostCallback
{
public:
    nsDNSSyncRequest(PRMonitor *mon)
        : mDone(PR_FALSE)
        , mStatus(NS_OK)
        , mMonitor(mon) {}
    virtual ~nsDNSSyncRequest() {}

    void OnLookupComplete(nsHostResolver *, nsHostRecord *, nsresult);

    PRBool                 mDone;
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
    mDone = PR_TRUE;
    mStatus = status;
    mHostRecord = hostRecord;
    PR_Notify(mMonitor);
    PR_ExitMonitor(mMonitor);
}

//-----------------------------------------------------------------------------

nsDNSService::nsDNSService()
    : mLock(nsnull)
{
}

nsDNSService::~nsDNSService()
{
    if (mLock)
        PR_DestroyLock(mLock);
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsDNSService, nsIDNSService, nsIObserver)

NS_IMETHODIMP
nsDNSService::Init()
{
    NS_ENSURE_TRUE(!mResolver, NS_ERROR_ALREADY_INITIALIZED);

    PRBool firstTime = (mLock == nsnull);

    // prefs
    PRUint32 maxCacheEntries  = 20;
    PRUint32 maxCacheLifetime = 1; // minutes
    PRBool   enableIDN        = PR_TRUE;
    PRBool   disableIPv6      = PR_FALSE;
    nsAdoptingCString ipv4OnlyDomains;

    // read prefs
    nsCOMPtr<nsIPrefBranchInternal> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
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
    }

    if (firstTime) {
        mLock = PR_NewLock();
        if (!mLock)
            return NS_ERROR_OUT_OF_MEMORY;

        // register as prefs observer
        prefs->AddObserver(kPrefDnsCacheEntries, this, PR_FALSE);
        prefs->AddObserver(kPrefDnsCacheExpiration, this, PR_FALSE);
        prefs->AddObserver(kPrefEnableIDN, this, PR_FALSE);
        prefs->AddObserver(kPrefIPv4OnlyDomains, this, PR_FALSE);
        prefs->AddObserver(kPrefDisableIPv6, this, PR_FALSE);
    }

    // we have to null out mIDN since we might be getting re-initialized
    // as a result of a pref change.
    nsCOMPtr<nsIIDNService> idn;
    if (enableIDN)
        idn = do_GetService(NS_IDNSERVICE_CONTRACTID);

    nsRefPtr<nsHostResolver> res;
    nsresult rv = nsHostResolver::Create(maxCacheEntries,
                                         maxCacheLifetime,
                                         getter_AddRefs(res));
    if (NS_SUCCEEDED(rv)) {
        // now, set all of our member variables while holding the lock
        nsAutoLock lock(mLock);
        mResolver = res;
        mIDN = idn;
        mIPv4OnlyDomains = ipv4OnlyDomains; // exchanges buffer ownership
        mDisableIPv6 = disableIPv6;
    }

    return rv;
}

NS_IMETHODIMP
nsDNSService::Shutdown()
{
    nsRefPtr<nsHostResolver> res;
    {
        nsAutoLock lock(mLock);
        res = mResolver;
        mResolver = nsnull;
    }
    if (res)
        res->Shutdown();
    return NS_OK;
}

NS_IMETHODIMP
nsDNSService::AsyncResolve(const nsACString &hostname,
                           PRUint32          flags,
                           nsIDNSListener   *listener,
                           nsIEventQueue    *eventQ,
                           nsIDNSRequest   **result)
{
    // grab reference to global host resolver and IDN service.  beware
    // simultaneous shutdown!!
    nsRefPtr<nsHostResolver> res;
    nsCOMPtr<nsIIDNService> idn;
    {
        nsAutoLock lock(mLock);
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

    nsCOMPtr<nsIDNSListener> listenerProxy;
    if (eventQ) {
        rv = NS_GetProxyForObject(eventQ,
                                  NS_GET_IID(nsIDNSListener),
                                  listener,
                                  PROXY_ASYNC | PROXY_ALWAYS,
                                  getter_AddRefs(listenerProxy));
        if (NS_FAILED(rv)) return rv;
        listener = listenerProxy;
    }

    PRUint16 af = GetAFForLookup(*hostPtr);

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
        nsAutoLock lock(mLock);
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
    // on the same thread.  so, our mutex needs to be re-entrant.  inotherwords,
    // we need to use a monitor! ;-)
    //
    
    PRMonitor *mon = PR_NewMonitor();
    if (!mon)
        return NS_ERROR_OUT_OF_MEMORY;

    PR_EnterMonitor(mon);
    nsDNSSyncRequest syncReq(mon);

    PRUint16 af = GetAFForLookup(*hostPtr);

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
        Init();
    }
    return NS_OK;
}

PRUint16
nsDNSService::GetAFForLookup(const nsACString &host)
{
    if (mDisableIPv6)
        return PR_AF_INET;

    nsAutoLock lock(mLock);

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
