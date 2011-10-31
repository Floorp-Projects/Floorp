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

#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/FTPChannelChild.h"
using namespace mozilla;
using namespace mozilla::net;

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
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsIObserverService.h"
#include "nsEscape.h"
#include "nsAlgorithm.h"

//-----------------------------------------------------------------------------

#if defined(PR_LOGGING)
//
// Log module for FTP Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsFtp:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gFTPLog = nsnull;
#endif
#undef LOG
#define LOG(args) PR_LOG(gFTPLog, PR_LOG_DEBUG, args)

//-----------------------------------------------------------------------------

#define IDLE_TIMEOUT_PREF     "network.ftp.idleConnectionTimeout"
#define IDLE_CONNECTION_LIMIT 8 /* TODO pref me */

#define QOS_DATA_PREF         "network.ftp.data.qos"
#define QOS_CONTROL_PREF      "network.ftp.control.qos"

nsFtpProtocolHandler *gFtpHandler = nsnull;

//-----------------------------------------------------------------------------

nsFtpProtocolHandler::nsFtpProtocolHandler()
    : mIdleTimeout(-1)
    , mSessionId(0)
    , mControlQoSBits(0x00)
    , mDataQoSBits(0x00)
{
#if defined(PR_LOGGING)
    if (!gFTPLog)
        gFTPLog = PR_NewLogModule("nsFtp");
#endif
    LOG(("FTP:creating handler @%x\n", this));

    gFtpHandler = this;
}

nsFtpProtocolHandler::~nsFtpProtocolHandler()
{
    LOG(("FTP:destroying handler @%x\n", this));

    NS_ASSERTION(mRootConnectionList.Length() == 0, "why wasn't Observe called?");

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
    if (IsNeckoChild())
        NeckoChild::InitNeckoChild();

    if (mIdleTimeout == -1) {
        nsresult rv;
        nsCOMPtr<nsIPrefBranch2> branch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = branch->GetIntPref(IDLE_TIMEOUT_PREF, &mIdleTimeout);
        if (NS_FAILED(rv))
            mIdleTimeout = 5*60; // 5 minute default

        rv = branch->AddObserver(IDLE_TIMEOUT_PREF, this, true);
        if (NS_FAILED(rv)) return rv;

	PRInt32 val;
	rv = branch->GetIntPref(QOS_DATA_PREF, &val);
	if (NS_SUCCEEDED(rv))
	    mDataQoSBits = (PRUint8) clamped(val, 0, 0xff);

	rv = branch->AddObserver(QOS_DATA_PREF, this, true);
	if (NS_FAILED(rv)) return rv;

	rv = branch->GetIntPref(QOS_CONTROL_PREF, &val);
	if (NS_SUCCEEDED(rv))
	    mControlQoSBits = (PRUint8) clamped(val, 0, 0xff);

	rv = branch->AddObserver(QOS_CONTROL_PREF, this, true);
	if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
        observerService->AddObserver(this,
                                     "network:offline-about-to-go-offline",
                                     true);

        observerService->AddObserver(this,
                                     "net:clear-active-logins",
                                     true);
    }

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
    *result = URI_STD | ALLOWS_PROXY | ALLOWS_PROXY_HTTP |
        URI_LOADABLE_BY_ANYONE; 
    return NS_OK;
}

NS_IMETHODIMP
nsFtpProtocolHandler::NewURI(const nsACString &aSpec,
                             const char *aCharset,
                             nsIURI *aBaseURI,
                             nsIURI **result)
{
    nsCAutoString spec(aSpec);
    spec.Trim(" \t\n\r"); // Match NS_IsAsciiWhitespace instead of HTML5

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
    nsCOMPtr<nsIStandardURL> url = do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
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
nsFtpProtocolHandler::NewProxiedChannel(nsIURI* uri, nsIProxyInfo* proxyInfo,
                                        nsIChannel* *result)
{
    NS_ENSURE_ARG_POINTER(uri);
    nsRefPtr<nsBaseChannel> channel;
    if (IsNeckoChild())
        channel = new FTPChannelChild(uri);
    else
        channel = new nsFtpChannel(uri, proxyInfo);

    nsresult rv = channel->Init();
    if (NS_FAILED(rv)) {
        return rv;
    }
    
    channel.forget(result);
    return rv;
}

NS_IMETHODIMP 
nsFtpProtocolHandler::AllowPort(PRInt32 port, const char *scheme, bool *_retval)
{
    *_retval = (port == 21 || port == 22);
    return NS_OK;
}

// connection cache methods

void
nsFtpProtocolHandler::Timeout(nsITimer *aTimer, void *aClosure)
{
    LOG(("FTP:timeout reached for %p\n", aClosure));

    bool found = gFtpHandler->mRootConnectionList.RemoveElement(aClosure);
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
    
    LOG(("FTP:removing connection for %s\n", spec.get()));
   
    timerStruct* ts = nsnull;
    PRUint32 i;
    bool found = false;
    
    for (i=0;i<mRootConnectionList.Length();++i) {
        ts = mRootConnectionList[i];
        if (strcmp(spec.get(), ts->key) == 0) {
            found = true;
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

    if (aConn->mSessionId != mSessionId)
        return NS_ERROR_FAILURE;

    nsCAutoString spec;
    aKey->GetPrePath(spec);

    LOG(("FTP:inserting connection for %s\n", spec.get()));

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
    if (mRootConnectionList.Length() == IDLE_CONNECTION_LIMIT) {
        PRUint32 i;
        for (i=0;i<mRootConnectionList.Length();++i) {
            timerStruct *candidate = mRootConnectionList[i];
            if (strcmp(candidate->key, ts->key) == 0) {
                mRootConnectionList.RemoveElementAt(i);
                delete candidate;
                break;
            }
        }
        if (mRootConnectionList.Length() == IDLE_CONNECTION_LIMIT) {
            timerStruct *eldest = mRootConnectionList[0];
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
    LOG(("FTP:observing [%s]\n", aTopic));

    if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(aSubject);
        if (!branch) {
            NS_ERROR("no prefbranch");
            return NS_ERROR_UNEXPECTED;
        }
        PRInt32 val;
        nsresult rv = branch->GetIntPref(IDLE_TIMEOUT_PREF, &val);
        if (NS_SUCCEEDED(rv))
            mIdleTimeout = val;

	rv = branch->GetIntPref(QOS_DATA_PREF, &val);
	if (NS_SUCCEEDED(rv))
	    mDataQoSBits = (PRUint8) clamped(val, 0, 0xff);

	rv = branch->GetIntPref(QOS_CONTROL_PREF, &val);
	if (NS_SUCCEEDED(rv))
	    mControlQoSBits = (PRUint8) clamped(val, 0, 0xff);
    } else if (!strcmp(aTopic, "network:offline-about-to-go-offline")) {
        ClearAllConnections();
    } else if (!strcmp(aTopic, "net:clear-active-logins")) {
        ClearAllConnections();
        mSessionId++;
    } else {
        NS_NOTREACHED("unexpected topic");
    }

    return NS_OK;
}

void
nsFtpProtocolHandler::ClearAllConnections()
{
    PRUint32 i;
    for (i=0;i<mRootConnectionList.Length();++i)
        delete mRootConnectionList[i];
    mRootConnectionList.Clear();
}
