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
 * Contributor(s): 
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
#include "nsIURLParser.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "prlog.h"
#include "nsNetUtil.h"

// For proxification of FTP URLs
#include "nsIHTTPProtocolHandler.h"
#include "nsIHTTPChannel.h"
#include "nsIErrorService.h" 

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

static NS_DEFINE_IID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID,       NS_STANDARDURL_CID);
static NS_DEFINE_CID(kProtocolProxyServiceCID, NS_PROTOCOLPROXYSERVICE_CID);
static NS_DEFINE_CID(kHTTPHandlerCID, NS_IHTTPHANDLER_CID);
static NS_DEFINE_CID(kErrorServiceCID, NS_ERRORSERVICE_CID);

nsSupportsHashtable* nsFtpProtocolHandler::mRootConnectionList = nsnull;
////////////////////////////////////////////////////////////////////////////////
nsFtpProtocolHandler::nsFtpProtocolHandler() {
        NS_INIT_REFCNT();
}

nsFtpProtocolHandler::~nsFtpProtocolHandler() {
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("~nsFtpProtocolHandler() called"));
     if (mRootConnectionList) {
        NS_DELETEXPCOM(mRootConnectionList);
        mRootConnectionList = nsnull;
    }
    mIOSvc = 0;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsFtpProtocolHandler, nsIProtocolHandler);

nsresult
nsFtpProtocolHandler::Init() {
    nsresult rv = NS_OK;

#if defined(PR_LOGGING)
    if (!gFTPLog) gFTPLog = PR_NewLogModule("nsFTPProtocol");
#endif /* PR_LOGGING */

    mProxySvc = do_GetService(kProtocolProxyServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
   
    mIOSvc = do_GetService(kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    mRootConnectionList = new nsSupportsHashtable(16, PR_TRUE);  /* xxx what should be the size of this hashtable?? */
    if (!mRootConnectionList) return NS_ERROR_OUT_OF_MEMORY;
    NS_LOG_NEW_XPCOM(mRootConnectionList, "nsSupportsHashtable", sizeof(nsSupportsHashtable), __FILE__, __LINE__);

    // XXX hack until xpidl supports error info directly (http://bugzilla.mozilla.org/show_bug.cgi?id=13423)
    nsCOMPtr<nsIErrorService> errorService = do_GetService(kErrorServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_BEGIN_FTP_TRANSACTION, "BeginFTPTransaction");
        if (NS_FAILED(rv)) return rv;
        rv = errorService->RegisterErrorStringBundleKey(NS_NET_STATUS_END_FTP_TRANSACTION, "EndFTPTransaction");
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}


    
////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsFtpProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("ftp");
    if (*result == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = 21; 
    return NS_OK;
}

NS_IMETHODIMP
nsFtpProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                             nsIURI **result)
{
    nsresult rv = NS_OK;
    nsCOMPtr<nsIStandardURL> url;
    rv = nsComponentManager::CreateInstance(kStandardURLCID, 
                                            nsnull, NS_GET_IID(nsIStandardURL),
                                            getter_AddRefs(url));
    if (NS_FAILED(rv)) return rv;
    rv = url->Init(nsIStandardURL::URLTYPE_AUTHORITY, 21, aSpec, aBaseURI);
    if (NS_FAILED(rv)) return rv;

    return url->QueryInterface(NS_GET_IID(nsIURI), (void**)result);
}

NS_IMETHODIMP
nsFtpProtocolHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
    nsresult rv = NS_OK;

    PRBool useProxy = PR_FALSE;

    nsFTPChannel* channel = nsnull;
    rv = nsFTPChannel::Create(nsnull, NS_GET_IID(nsIChannel), (void**)&channel);
    if (NS_FAILED(rv)) return rv;
    
    rv = channel->Init(url);
    if (NS_FAILED(rv)) {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpProtocolHandler::NewChannel() FAILED\n"));
        return rv;
    }

    if (NS_SUCCEEDED(mProxySvc->GetProxyEnabled(&useProxy)) && useProxy)
    {
        rv = mProxySvc->ExamineForProxy(url, channel);
        if (NS_FAILED(rv)) return rv;
    }
    
    useProxy = PR_FALSE;
    if (NS_SUCCEEDED(channel->GetUsingProxy(&useProxy)) && useProxy) {
        
        nsCOMPtr<nsIChannel> proxyChannel;
        // if an FTP proxy is enabled, push things off to HTTP.
        
        nsCOMPtr<nsIHTTPProtocolHandler> httpHandler = do_GetService(kHTTPHandlerCID, &rv);
        if (NS_FAILED(rv)) return rv;
        
        // rjc says: the dummy URI (for the HTTP layer) needs to be a syntactically valid URI
        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), "http://test.com/");
        if (NS_FAILED(rv)) return rv;
        
        rv = httpHandler->NewChannel(uri, getter_AddRefs(proxyChannel));
        if (NS_FAILED(rv)) return rv;
        
        nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(proxyChannel, &rv);
        if (NS_FAILED(rv)) return rv;
        
        nsXPIDLCString spec;
        rv = url->GetSpec(getter_Copies(spec));
        if (NS_FAILED(rv)) return rv;
        
        rv = httpChannel->SetProxyRequestURI((const char*)spec);
        if (NS_FAILED(rv)) return rv;
        
        nsCOMPtr<nsIProxy> proxyHTTP = do_QueryInterface(httpChannel, &rv);
        if (NS_FAILED(rv)) return rv;
        
        nsXPIDLCString proxyHost;
        rv = channel->GetProxyHost(getter_Copies(proxyHost));
        if (NS_FAILED(rv)) return rv;
        
        rv = proxyHTTP->SetProxyHost(proxyHost);
        if (NS_FAILED(rv)) return rv;
        
        PRInt32 proxyPort;
        rv = channel->GetProxyPort(&proxyPort);
        if (NS_FAILED(rv)) return rv;
        
        rv = proxyHTTP->SetProxyPort(proxyPort);
        if (NS_FAILED(rv)) return rv;
        
        nsXPIDLCString proxyType;
        rv = channel->GetProxyType(getter_Copies(proxyType));
        
        if (NS_SUCCEEDED(rv)) 
            proxyHTTP->SetProxyType(proxyType);
        
        rv = channel->SetProxyChannel(proxyChannel);
    }
    
    *result = channel;
    return rv;
}

// connection cache methods
nsresult
nsFtpProtocolHandler::RemoveConnection(nsIURI *aKey, nsISupports* *_retval) {
    NS_ASSERTION(_retval, "null pointer");
    NS_ASSERTION(aKey, "null pointer");
    
    *_retval = nsnull;

    if (!mRootConnectionList)
        return NS_ERROR_NULL_POINTER;

    nsXPIDLCString spec;
    aKey->GetPrePath(getter_Copies(spec));
    
    nsCStringKey stringKey(spec);
    
    // Do not have to addRef since there is only one connection (with this key)
    // in this hash table at any time and that one has been addRef'ed
    // by the Put().  If we change connection caching, we will have 
    // to re-address this.
    if (mRootConnectionList)
        mRootConnectionList->Remove(&stringKey, (nsISupports**)_retval);

    if (*_retval)
        return NS_OK;

    return NS_ERROR_FAILURE;
}

nsresult
nsFtpProtocolHandler::InsertConnection(nsIURI *aKey, nsISupports *aConn) {
    NS_ASSERTION(aConn, "null pointer");
    NS_ASSERTION(aKey, "null pointer");
    
    if (!mRootConnectionList)
        return NS_ERROR_NULL_POINTER;

    nsXPIDLCString spec;
    aKey->GetPrePath(getter_Copies(spec));
    nsCStringKey stringKey(spec);
    
    if (mRootConnectionList)
    {
        mRootConnectionList->Put(&stringKey, aConn);
        return NS_OK;
    }        
    return NS_ERROR_FAILURE;    
}

////////////////////////////////////////////////////////////////////////////////
