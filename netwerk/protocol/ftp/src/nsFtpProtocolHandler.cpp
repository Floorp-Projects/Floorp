/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Bradley Baetz <bbaetz@cs.mcgill.ca>
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#include "nsFtpProtocolHandler.h"
#include "nsFTPChannel.h"
#include "nsIURL.h"
#include "nsIStandardURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "prlog.h"
#include "nsIPref.h"
#include "nsNetUtil.h"
#include "nsIErrorService.h" 
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"

#if defined(PR_LOGGING)
//
// Log module for FTP Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsFTPProtocol:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gFTPLog = nsnull;

#endif /* PR_LOGGING */

#define IDLE_TIMEOUT_PREF "network.ftp.idleConnectionTimeout"

static NS_DEFINE_IID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID,       NS_STANDARDURL_CID);
static NS_DEFINE_CID(kErrorServiceCID, NS_ERRORSERVICE_CID);
static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);

nsVoidArray* nsFtpProtocolHandler::mRootConnectionList = nsnull;
PRInt32 nsFtpProtocolHandler::mIdleTimeout = -1;
////////////////////////////////////////////////////////////////////////////////
nsFtpProtocolHandler::nsFtpProtocolHandler() {
}

nsFtpProtocolHandler::~nsFtpProtocolHandler() {
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("~nsFtpProtocolHandler() called"));
    if (mRootConnectionList) {
        PRInt32 i;
        for (i=0;i<mRootConnectionList->Count();++i)
            delete (timerStruct*)mRootConnectionList->ElementAt(i);
        NS_DELETEXPCOM(mRootConnectionList);
        mRootConnectionList = nsnull;
    }
    mIdleTimeout = -1;
    mIOSvc = 0;
}

NS_IMPL_THREADSAFE_ISUPPORTS4(nsFtpProtocolHandler,
                              nsIProtocolHandler,
                              nsIProxiedProtocolHandler,
                              nsIObserver,
                              nsISupportsWeakReference);

nsresult
nsFtpProtocolHandler::Init() {
    nsresult rv = NS_OK;

#if defined(PR_LOGGING)
    if (!gFTPLog) gFTPLog = PR_NewLogModule("nsFTPProtocol");
#endif /* PR_LOGGING */

    mIOSvc = do_GetService(kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    mRootConnectionList = new nsVoidArray(8);
    if (!mRootConnectionList) return NS_ERROR_OUT_OF_MEMORY;
    NS_LOG_NEW_XPCOM(mRootConnectionList, "nsVoidArray", sizeof(nsVoidArray), __FILE__, __LINE__);

    // XXX hack until xpidl supports error info directly (http://bugzilla.mozilla.org/show_bug.cgi?id=13423)
    nsCOMPtr<nsIErrorService> errorService = do_GetService(kErrorServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_BEGIN_FTP_TRANSACTION, "BeginFTPTransaction");
        if (NS_FAILED(rv)) return rv;
        rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_END_FTP_TRANSACTION, "EndFTPTransaction");
        if (NS_FAILED(rv)) return rv;
    }

    if (mIdleTimeout == -1) {
        nsCOMPtr<nsIPrefService> prefSrv = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsIPrefBranch> branch;
        // I have to get the root pref branch because of bug 107617
        rv = prefSrv->GetBranch(nsnull, getter_AddRefs(branch));
        if (NS_FAILED(rv)) return rv;
        rv = branch->GetIntPref(IDLE_TIMEOUT_PREF, &mIdleTimeout);
        if (NS_FAILED(rv))
            mIdleTimeout = 5*60; // 5 minute default
        prefSrv->GetBranch(nsnull, getter_AddRefs(branch));
        nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(branch);
        rv = pbi->AddObserver(IDLE_TIMEOUT_PREF, this, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }
    
    return NS_OK;
}


    
////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsFtpProtocolHandler::GetScheme(nsACString &result)
{
    result = "ftp";
    return NS_OK;
}

NS_IMETHODIMP
nsFtpProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = 21; 
    return NS_OK;
}

NS_IMETHODIMP
nsFtpProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_STD | ALLOWS_PROXY | ALLOWS_PROXY_HTTP; 
    return NS_OK;
}

NS_IMETHODIMP
nsFtpProtocolHandler::NewURI(const nsACString &aSpec,
                             const char *aCharset,
                             nsIURI *aBaseURI,
                             nsIURI **result)
{
    // FindCharInSet isn't available right now for nsACstrings
    // so we use FindChar instead

    // ftp urls should not have \r or \n in them
    if (aSpec.FindChar('\r') >= 0 || aSpec.FindChar('\n') >= 0)
        return NS_ERROR_MALFORMED_URI;

    nsresult rv = NS_OK;
    nsCOMPtr<nsIStandardURL> url;
    rv = nsComponentManager::CreateInstance(kStandardURLCID, 
                                            nsnull, NS_GET_IID(nsIStandardURL),
                                            getter_AddRefs(url));
    if (NS_FAILED(rv)) return rv;
    rv = url->Init(nsIStandardURL::URLTYPE_AUTHORITY, 21, aSpec, aCharset, aBaseURI);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(url, result);
}

NS_IMETHODIMP
nsFtpProtocolHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
    return NewProxiedChannel(url, nsnull, result);
}

NS_IMETHODIMP
nsFtpProtocolHandler::NewProxiedChannel(nsIURI* url, nsIProxyInfo* proxyInfo, nsIChannel* *result)
{
    nsresult rv = NS_OK;

    nsFTPChannel* channel = nsnull;
    rv = nsFTPChannel::Create(nsnull, NS_GET_IID(nsIChannel), (void**)&channel);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICacheService> serv = do_GetService(kCacheServiceCID, &rv);
    if (serv)
        serv->CreateSession("FTP",
                            nsICache::STORE_ANYWHERE,
                            nsICache::STREAM_BASED,
                            getter_AddRefs(mCacheSession));
    
    if (mCacheSession)
        rv = mCacheSession->SetDoomEntriesIfExpired(PR_FALSE);

    rv = channel->Init(url, proxyInfo, mCacheSession);
    if (NS_FAILED(rv)) {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpProtocolHandler::NewChannel() FAILED\n"));
        return rv;
    }
    
    *result = channel;
    return rv;
}

NS_IMETHODIMP 
nsFtpProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    if (port == 21 || port == 22)
        *_retval = PR_TRUE;
    else
        *_retval = PR_FALSE;
    return NS_OK;
}

// connection cache methods

void
nsFtpProtocolHandler::Timeout(nsITimer *aTimer, void *aClosure)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("Timeout reached for %0x\n", aClosure));

    NS_ASSERTION(mRootConnectionList, "Timeout called without a connection list!");
    
    PRBool found = mRootConnectionList->RemoveElement(aClosure);
    NS_ASSERTION(found, "Timeout without entry!");
    if (!found)
        return;

    timerStruct* s = (timerStruct*)aClosure;
    delete s;
}

nsresult
nsFtpProtocolHandler::RemoveConnection(nsIURI *aKey, nsISupports* *_retval) {
    NS_ASSERTION(_retval, "null pointer");
    NS_ASSERTION(aKey, "null pointer");
    
    *_retval = nsnull;

    if (!mRootConnectionList)
        return NS_ERROR_NULL_POINTER;

    nsCAutoString spec;
    aKey->GetPrePath(spec);
    
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("Removing connection for %s\n", spec.get()));
   
    timerStruct* ts = nsnull;
    PRInt32 i;
    PRBool found = PR_FALSE;
    
    for (i=0;i<mRootConnectionList->Count();++i) {
        ts = (timerStruct*)mRootConnectionList->ElementAt(i);
        if (!nsCRT::strcmp(spec.get(), ts->key)) {
            found = PR_TRUE;
            mRootConnectionList->RemoveElementAt(i);
            break;
        }
    }

    if (!found)
        return NS_ERROR_FAILURE;

    NS_ADDREF(*_retval = ts->conn);
    delete ts;

    return NS_OK;
}

nsresult
nsFtpProtocolHandler::InsertConnection(nsIURI *aKey, nsISupports *aConn)
{
    NS_ASSERTION(aConn, "null pointer");
    NS_ASSERTION(aKey, "null pointer");
    
    if (!mRootConnectionList)
        return NS_ERROR_NULL_POINTER;

    nsCAutoString spec;
    aKey->GetPrePath(spec);
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("Inserting connection for %s\n", spec.get()));

    if (!mRootConnectionList)
        return NS_ERROR_FAILURE;
    
    nsresult rv;
    nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_FAILED(rv)) return rv;
    
    timerStruct* ts = new timerStruct();
    if (!ts)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = timer->InitWithFuncCallback(nsFtpProtocolHandler::Timeout,
                                     ts,
                                     mIdleTimeout*1000,
                                     nsITimer::TYPE_REPEATING_SLACK);
    if (NS_FAILED(rv)) {
        delete ts;
        return rv;
    }
    
    ts->key = nsCRT::strdup(spec.get());
    if (!ts->key) {
        delete ts;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    ts->conn = aConn;
    ts->timer = timer;

    mRootConnectionList->AppendElement(ts);
    
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
nsFtpProtocolHandler::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const PRUnichar* aData)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpProtocolHandler::Observe\n"));
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(aSubject);
    NS_ASSERTION(branch, "Didn't get a prefBranch in Observe");
    if (!branch)
        return NS_ERROR_UNEXPECTED;

    NS_ASSERTION(!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID),
                 "Wrong aTopic passed to Observer");

    PRInt32 timeout;
    nsresult rv = branch->GetIntPref(IDLE_TIMEOUT_PREF, &timeout);
    if (NS_SUCCEEDED(rv))
        mIdleTimeout = timeout;

    return NS_OK;
}
