/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
#include "nsIStandardURL.h"
#include "prlog.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIObserverService.h"
#include "nsEscape.h"
#include "nsAlgorithm.h"
#include "nsICacheSession.h"

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
PRLogModuleInfo* gFTPLog = nullptr;
#endif
#undef LOG
#define LOG(args) PR_LOG(gFTPLog, PR_LOG_DEBUG, args)

//-----------------------------------------------------------------------------

#define IDLE_TIMEOUT_PREF     "network.ftp.idleConnectionTimeout"
#define IDLE_CONNECTION_LIMIT 8 /* TODO pref me */

#define QOS_DATA_PREF         "network.ftp.data.qos"
#define QOS_CONTROL_PREF      "network.ftp.control.qos"

nsFtpProtocolHandler *gFtpHandler = nullptr;

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

    gFtpHandler = nullptr;
}

NS_IMPL_ISUPPORTS(nsFtpProtocolHandler,
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
        nsCOMPtr<nsIPrefBranch> branch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = branch->GetIntPref(IDLE_TIMEOUT_PREF, &mIdleTimeout);
        if (NS_FAILED(rv))
            mIdleTimeout = 5*60; // 5 minute default

        rv = branch->AddObserver(IDLE_TIMEOUT_PREF, this, true);
        if (NS_FAILED(rv)) return rv;

	int32_t val;
	rv = branch->GetIntPref(QOS_DATA_PREF, &val);
	if (NS_SUCCEEDED(rv))
	    mDataQoSBits = (uint8_t) clamped(val, 0, 0xff);

	rv = branch->AddObserver(QOS_DATA_PREF, this, true);
	if (NS_FAILED(rv)) return rv;

	rv = branch->GetIntPref(QOS_CONTROL_PREF, &val);
	if (NS_SUCCEEDED(rv))
	    mControlQoSBits = (uint8_t) clamped(val, 0, 0xff);

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
nsFtpProtocolHandler::GetDefaultPort(int32_t *result)
{
    *result = 21; 
    return NS_OK;
}

NS_IMETHODIMP
nsFtpProtocolHandler::GetProtocolFlags(uint32_t *result)
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
    nsAutoCString spec(aSpec);
    spec.Trim(" \t\n\r"); // Match NS_IsAsciiWhitespace instead of HTML5

    char *fwdPtr = spec.BeginWriting();

    // now unescape it... %xx reduced inline to resulting character

    int32_t len = NS_UnescapeURL(fwdPtr);

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
nsFtpProtocolHandler::NewChannel2(nsIURI* url,
                                  nsILoadInfo* aLoadInfo,
                                  nsIChannel** result)
{
    return NewProxiedChannel(url, nullptr, 0, nullptr, result);
}

NS_IMETHODIMP
nsFtpProtocolHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
    return NewChannel2(url, nullptr, result);
}

NS_IMETHODIMP
nsFtpProtocolHandler::NewProxiedChannel2(nsIURI* uri, nsIProxyInfo* proxyInfo,
                                         uint32_t proxyResolveFlags,
                                         nsIURI *proxyURI,
                                         nsILoadInfo* aLoadInfo,
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
nsFtpProtocolHandler::NewProxiedChannel(nsIURI* uri, nsIProxyInfo* proxyInfo,
                                        uint32_t proxyResolveFlags,
                                        nsIURI *proxyURI,
                                        nsIChannel* *result)
{
  return NewProxiedChannel2(uri, proxyInfo, proxyResolveFlags,
                            proxyURI, nullptr /*loadinfo*/,
                            result);
}

NS_IMETHODIMP 
nsFtpProtocolHandler::AllowPort(int32_t port, const char *scheme, bool *_retval)
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
    
    *_retval = nullptr;

    nsAutoCString spec;
    aKey->GetPrePath(spec);
    
    LOG(("FTP:removing connection for %s\n", spec.get()));
   
    timerStruct* ts = nullptr;
    uint32_t i;
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
    ts->conn = nullptr;
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

    nsAutoCString spec;
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
        uint32_t i;
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
                              const char16_t *aData)
{
    LOG(("FTP:observing [%s]\n", aTopic));

    if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
        nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(aSubject);
        if (!branch) {
            NS_ERROR("no prefbranch");
            return NS_ERROR_UNEXPECTED;
        }
        int32_t val;
        nsresult rv = branch->GetIntPref(IDLE_TIMEOUT_PREF, &val);
        if (NS_SUCCEEDED(rv))
            mIdleTimeout = val;

	rv = branch->GetIntPref(QOS_DATA_PREF, &val);
	if (NS_SUCCEEDED(rv))
	    mDataQoSBits = (uint8_t) clamped(val, 0, 0xff);

	rv = branch->GetIntPref(QOS_CONTROL_PREF, &val);
	if (NS_SUCCEEDED(rv))
	    mControlQoSBits = (uint8_t) clamped(val, 0, 0xff);
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
    uint32_t i;
    for (i=0;i<mRootConnectionList.Length();++i)
        delete mRootConnectionList[i];
    mRootConnectionList.Clear();
}
