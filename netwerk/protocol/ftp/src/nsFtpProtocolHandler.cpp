/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
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
#include "nsNetUtil.h"
#include "nsIErrorService.h" 
#include "nsIPrefService.h"
#include "nsIPrefBranchInternal.h"
#include "nsIObserverService.h"
#include "nsEscape.h"

//-----------------------------------------------------------------------------

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
#endif
#define LOG(args) PR_LOG(gFTPLog, PR_LOG_DEBUG, args)

//-----------------------------------------------------------------------------

#define IDLE_TIMEOUT_PREF     "network.ftp.idleConnectionTimeout"
#define IDLE_CONNECTION_LIMIT 8 /* XXX pref me */

static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kErrorServiceCID, NS_ERRORSERVICE_CID);
static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);

nsFtpProtocolHandler *gFtpHandler = nsnull;

//-----------------------------------------------------------------------------

nsFtpProtocolHandler::nsFtpProtocolHandler()
    : mIdleTimeout(-1)
{
#if defined(PR_LOGGING)
    if (!gFTPLog) gFTPLog = PR_NewLogModule("nsFTPProtocol");
#endif
    LOG(("Creating nsFtpProtocolHandler @%x\n", this));

    gFtpHandler = this;
}

nsFtpProtocolHandler::~nsFtpProtocolHandler()
{
    LOG(("Destroying nsFtpProtocolHandler @%x\n", this));

    NS_ASSERTION(mRootConnectionList.Count() == 0, "why wasn't Observe called?");

    gFtpHandler = nsnull;
}

NS_IMPL_THREADSAFE_ISUPPORTS4(nsFtpProtocolHandler,
                              nsIProtocolHandler,
                              nsIProxiedProtocolHandler,
                              nsIObserver,
                              nsISupportsWeakReference)

nsresult
nsFtpProtocolHandler::Init()
{
    nsresult rv;

    // XXX hack until xpidl supports error info directly (http://bugzilla.mozilla.org/show_bug.cgi?id=13423)
    nsCOMPtr<nsIErrorService> errorService = do_GetService(kErrorServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_BEGIN_FTP_TRANSACTION, "BeginFTPTransaction");
        if (NS_FAILED(rv)) return rv;
        rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_END_FTP_TRANSACTION, "EndFTPTransaction");
        if (NS_FAILED(rv)) return rv;
    }

    if (mIdleTimeout == -1) {
        nsCOMPtr<nsIPrefBranchInternal> branch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = branch->GetIntPref(IDLE_TIMEOUT_PREF, &mIdleTimeout);
        if (NS_FAILED(rv))
            mIdleTimeout = 5*60; // 5 minute default

        rv = branch->AddObserver(IDLE_TIMEOUT_PREF, this, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIObserverService> observerService =
        do_GetService("@mozilla.org/observer-service;1");
    if (observerService)
        observerService->AddObserver(this,
                                     "network:offline-about-to-go-offline",
                                     PR_FALSE);
    
    return NS_OK;
}

    
//-----------------------------------------------------------------------------
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsFtpProtocolHandler::GetScheme(nsACString &result)
{
    result.AssignLiteral("ftp");
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
    nsCAutoString spec(aSpec);
    char *fwdPtr = spec.BeginWriting();

    // now unescape it... %xx reduced inline to resulting character

    PRInt32 len = NS_UnescapeURL(fwdPtr);

    // NS_UnescapeURL() modified spec's buffer, truncate to ensure
    // spec knows its new length.
    spec.Truncate(len);

    // return an error if we find a NUL, CR, or LF in the path
    if (spec.FindCharInSet(CRLF) >= 0 || spec.FindChar('\0') >= 0)
        return NS_ERROR_MALFORMED_URI;

    nsresult rv;
    nsCOMPtr<nsIStandardURL> url = do_CreateInstance(kStandardURLCID, &rv);
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
    nsFTPChannel *channel = new nsFTPChannel();
    if (!channel)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(channel);

    nsCOMPtr<nsICacheService> cache = do_GetService(kCacheServiceCID);
    if (cache) {
        cache->CreateSession("FTP",
                             nsICache::STORE_ANYWHERE,
                             nsICache::STREAM_BASED,
                             getter_AddRefs(mCacheSession));
        if (mCacheSession)
            mCacheSession->SetDoomEntriesIfExpired(PR_FALSE);
    }

    nsresult rv = channel->Init(url, proxyInfo, mCacheSession);
    if (NS_FAILED(rv)) {
        LOG(("nsFtpProtocolHandler::NewChannel() FAILED\n"));
        NS_RELEASE(channel);
        return rv;
    }
    
    *result = channel;
    return rv;
}

NS_IMETHODIMP 
nsFtpProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    *_retval = (port == 21 || port == 22);
    return NS_OK;
}

// connection cache methods

void
nsFtpProtocolHandler::Timeout(nsITimer *aTimer, void *aClosure)
{
    LOG(("Timeout reached for %0x\n", aClosure));

    PRBool found = gFtpHandler->mRootConnectionList.RemoveElement(aClosure);
    if (!found) {
        NS_ERROR("timerStruct not found");
        return;
    }

    timerStruct* s = (timerStruct*)aClosure;
    delete s;
}

nsresult
nsFtpProtocolHandler::RemoveConnection(nsIURI *aKey, nsFtpControlConnection* *_retval)
{
    NS_ASSERTION(_retval, "null pointer");
    NS_ASSERTION(aKey, "null pointer");
    
    *_retval = nsnull;

    nsCAutoString spec;
    aKey->GetPrePath(spec);
    
    LOG(("Removing connection for %s\n", spec.get()));
   
    timerStruct* ts = nsnull;
    PRInt32 i;
    PRBool found = PR_FALSE;
    
    for (i=0;i<mRootConnectionList.Count();++i) {
        ts = (timerStruct*)mRootConnectionList[i];
        if (strcmp(spec.get(), ts->key) == 0) {
            found = PR_TRUE;
            mRootConnectionList.RemoveElementAt(i);
            break;
        }
    }

    if (!found)
        return NS_ERROR_FAILURE;

    // swap connection ownership
    *_retval = ts->conn;
    ts->conn = nsnull;
    delete ts;

    return NS_OK;
}

nsresult
nsFtpProtocolHandler::InsertConnection(nsIURI *aKey, nsFtpControlConnection *aConn)
{
    NS_ASSERTION(aConn, "null pointer");
    NS_ASSERTION(aKey, "null pointer");
    
    nsCAutoString spec;
    aKey->GetPrePath(spec);

    LOG(("Inserting connection for %s\n", spec.get()));

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
    
    ts->key = ToNewCString(spec);
    if (!ts->key) {
        delete ts;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(aConn);
    ts->conn = aConn;
    ts->timer = timer;

    //
    // limit number of idle connections.  if limit is reached, then prune
    // eldest connection with matching key.  if none matching, then prune
    // eldest connection.
    //
    if (mRootConnectionList.Count() == IDLE_CONNECTION_LIMIT) {
        PRInt32 i;
        for (i=0;i<mRootConnectionList.Count();++i) {
            timerStruct *candidate = (timerStruct *) mRootConnectionList[i];
            if (strcmp(candidate->key, ts->key) == 0) {
                mRootConnectionList.RemoveElementAt(i);
                delete candidate;
                break;
            }
        }
        if (mRootConnectionList.Count() == IDLE_CONNECTION_LIMIT) {
            timerStruct *eldest = (timerStruct *) mRootConnectionList[0];
            mRootConnectionList.RemoveElementAt(0);
            delete eldest;
        }
    }

    mRootConnectionList.AppendElement(ts);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIObserver

NS_IMETHODIMP
nsFtpProtocolHandler::Observe(nsISupports *aSubject,
                              const char *aTopic,
                              const PRUnichar *aData)
{
    LOG(("nsFtpProtocolHandler::Observe [topic=%s]\n", aTopic));

    if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(aSubject);
        if (!branch) {
            NS_ERROR("no prefbranch");
            return NS_ERROR_UNEXPECTED;
        }
        PRInt32 timeout;
        nsresult rv = branch->GetIntPref(IDLE_TIMEOUT_PREF, &timeout);
        if (NS_SUCCEEDED(rv))
            mIdleTimeout = timeout;
    }
    else if (!strcmp(aTopic, "network:offline-about-to-go-offline")) {
        PRInt32 i;
        for (i=0;i<mRootConnectionList.Count();++i)
            delete (timerStruct*)mRootConnectionList[i];
        mRootConnectionList.Clear();
    }
    else {
        NS_NOTREACHED("unexpected topic");
    }

    return NS_OK;
}
