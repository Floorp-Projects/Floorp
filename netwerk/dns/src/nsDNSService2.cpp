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

static const char kPrefDnsCacheEntries[]    = "network.dnsCacheEntries";
static const char kPrefDnsCacheExpiration[] = "network.dnsCacheExpiration";
static const char kPrefEnableIDN[]          = "network.enableIDN";

//-----------------------------------------------------------------------------

class nsDNSRecord : public nsIDNSRecord
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDNSRECORD

    nsDNSRecord(nsAddrInfo *ai)
        : mAddrInfo(ai)
        , mIter(nsnull)
        , mDone(PR_FALSE) {}

private:
    virtual ~nsDNSRecord() {}

    nsRefPtr<nsAddrInfo>  mAddrInfo;
    void                 *mIter;
    PRBool                mDone;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDNSRecord, nsIDNSRecord)

NS_IMETHODIMP
nsDNSRecord::GetCanonicalName(nsACString &result)
{
    NS_ENSURE_TRUE(mAddrInfo, NS_ERROR_NOT_AVAILABLE);
    result.Assign(PR_GetCanonNameFromAddrInfo(mAddrInfo->get()));
    return NS_OK;
}

NS_IMETHODIMP
nsDNSRecord::GetNextAddr(PRUint16 port, PRNetAddr *addr)
{
    NS_ENSURE_TRUE(mAddrInfo, NS_ERROR_NOT_AVAILABLE);

    // not a programming error to poke the DNS record when it has
    // no more entries.  just silently fail.  this enables consumers
    // to enumerate the DNS record without calling HasMore.
    if (mDone)
        return NS_ERROR_NOT_AVAILABLE;

    mIter = PR_EnumerateAddrInfo(mIter, mAddrInfo->get(), port, addr);
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
    *result = !mDone;
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

class nsDNSAsyncRequest : public nsResolveHostCB
                        , public nsIDNSRequest
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDNSREQUEST

    nsDNSAsyncRequest(nsHostResolver   *res,
                      const nsACString &host,
                      nsIDNSListener   *listener)
        : mResolver(res)
        , mHost(host)
        , mListener(listener) {}
    virtual ~nsDNSAsyncRequest() {}

    void OnLookupComplete(nsHostResolver *, const char *, nsresult, nsAddrInfo *);

    nsRefPtr<nsHostResolver> mResolver;
    nsCString                mHost; // hostname we're resolving
    nsCOMPtr<nsIDNSListener> mListener;
};

void
nsDNSAsyncRequest::OnLookupComplete(nsHostResolver *resolver,
                                    const char     *host,
                                    nsresult        status,
                                    nsAddrInfo     *ai)
{
    nsDNSRecord *rec;
    if (!ai)
        rec = nsnull;
    else {
        rec = new nsDNSRecord(ai);
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
    mResolver->DetachCallback(mHost.get(), this);
    return NS_OK;
}

//-----------------------------------------------------------------------------

class nsDNSSyncRequest : public nsResolveHostCB
{
public:
    nsDNSSyncRequest(PRMonitor *mon)
        : mDone(PR_FALSE)
        , mStatus(NS_OK)
        , mMonitor(mon) {}
    virtual ~nsDNSSyncRequest() {}

    void OnLookupComplete(nsHostResolver *, const char *, nsresult, nsAddrInfo *);

    PRBool                mDone;
    nsresult              mStatus;
    nsRefPtr<nsAddrInfo>  mAddrInfo;

private:
    PRMonitor            *mMonitor;
};

void
nsDNSSyncRequest::OnLookupComplete(nsHostResolver *resolver,
                                   const char     *host,
                                   nsresult        status,
                                   nsAddrInfo     *ai)
{
    // store results, and wake up nsDNSService::Resolve to process results.
    PR_EnterMonitor(mMonitor);
    mDone = PR_TRUE;
    mStatus = status;
    mAddrInfo = ai;
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
    PRUint32 maxCacheLifetime = 5; // minutes
    PRBool   enableIDN        = PR_TRUE;

    // read prefs
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefs) {
        PRInt32 val;
        if (NS_SUCCEEDED(prefs->GetIntPref(kPrefDnsCacheEntries, &val)))
            maxCacheEntries = val;
        if (NS_SUCCEEDED(prefs->GetIntPref(kPrefDnsCacheExpiration, &val)))
            maxCacheLifetime = (val / 60); // convert from seconds to minutes
        if (NS_SUCCEEDED(prefs->GetBoolPref(kPrefEnableIDN, (PRBool*)&val)))
            enableIDN = (PRBool) val;
    }

    // we have to null out mIDN since we might be getting re-initialized
    // as a result of a pref change.
    if (enableIDN)
        mIDN = do_GetService(NS_IDNSERVICE_CONTRACTID);
    else
        mIDN = nsnull;

    if (firstTime) {
        mLock = PR_NewLock();
        if (!mLock)
            return NS_ERROR_OUT_OF_MEMORY;
        
        // register as prefs observer
        nsCOMPtr<nsIPrefBranchInternal> prefsInt = do_QueryInterface(prefs);
        if (prefsInt) {
            prefsInt->AddObserver(kPrefDnsCacheEntries, this, PR_FALSE);
            prefsInt->AddObserver(kPrefDnsCacheExpiration, this, PR_FALSE);
            prefsInt->AddObserver(kPrefEnableIDN, this, PR_FALSE);
        }
    }

    return nsHostResolver::Create(maxCacheEntries,
                                  maxCacheLifetime,
                                  getter_AddRefs(mResolver));
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
                           PRBool            bypassCache,
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
    nsCAutoString hostBuf;
    if (idn && !IsASCII(hostname)) {
        rv = idn->ConvertUTF8toACE(hostname, hostBuf);
        if (NS_SUCCEEDED(rv))
            hostPtr = &hostBuf;
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

    nsDNSAsyncRequest *req = new nsDNSAsyncRequest(res, *hostPtr, listener);
    if (!req)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*result = req);

    // addref for resolver; will be released when OnLookupComplete is called.
    NS_ADDREF(req);
    rv = res->ResolveHost(req->mHost.get(), bypassCache, req); 
    if (NS_FAILED(rv)) {
        NS_RELEASE(req);
        NS_RELEASE(*result);
    }
    return rv;
}

NS_IMETHODIMP
nsDNSService::Resolve(const nsACString &hostname,
                      PRBool            bypassCache,
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
    nsCAutoString hostBuf;
    if (idn && !IsASCII(hostname)) {
        rv = idn->ConvertUTF8toACE(hostname, hostBuf);
        if (NS_SUCCEEDED(rv))
            hostPtr = &hostBuf;
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

    rv = res->ResolveHost(PromiseFlatCString(*hostPtr).get(), bypassCache, &syncReq);
    if (NS_SUCCEEDED(rv)) {
        // wait for result
        while (!syncReq.mDone)
            PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);

        if (NS_FAILED(syncReq.mStatus))
            rv = syncReq.mStatus;
        else {
            nsDNSRecord *rec = new nsDNSRecord(syncReq.mAddrInfo);
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
