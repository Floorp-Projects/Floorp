/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Gopher protocol code.
 *
 * The Initial Developer of the Original Code is Bradley Baetz.
 * Portions created by Bradley Baetz are Copyright (C) 2000 Bradley Baetz.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *  Bradley Baetz <bbaetz@student.usyd.edu.au>
 */

// This is based rather heavily on the datetime and finger implementations
// and documentation

#include "nspr.h"
#include "nsGopherChannel.h"
#include "nsGopherHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIHTTPProtocolHandler.h"
#include "nsIHTTPChannel.h"
#include "nsIErrorService.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kProtocolProxyServiceCID, NS_PROTOCOLPROXYSERVICE_CID);
static NS_DEFINE_CID(kHTTPHandlerCID, NS_IHTTPHANDLER_CID);

////////////////////////////////////////////////////////////////////////////////

nsGopherHandler::nsGopherHandler() : mProxyPort(-1) {
    NS_INIT_REFCNT();
    nsresult rv;
    mProxySvc = do_GetService(kProtocolProxyServiceCID, &rv);
    if (!mProxySvc)
        NS_WARNING("Failed to get proxy service!\n");
}

nsGopherHandler::~nsGopherHandler() {
}

NS_IMPL_ISUPPORTS2(nsGopherHandler, nsIProtocolHandler, nsIProxy);

NS_METHOD
nsGopherHandler::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult) {
    nsGopherHandler* ph = new nsGopherHandler();
    if (!ph)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->QueryInterface(aIID, aResult);
    NS_RELEASE(ph);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProxy methods:
NS_IMETHODIMP
nsGopherHandler::GetProxyHost(char **aProxyHost) {
    *aProxyHost = mProxyHost.ToNewCString();
    if (!*aProxyHost) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherHandler::SetProxyHost(const char * aProxyHost) {
    mProxyHost = aProxyHost;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherHandler::GetProxyPort(PRInt32 *aProxyPort) {
    *aProxyPort = mProxyPort;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherHandler::SetProxyPort(PRInt32 aProxyPort) {
    mProxyPort = aProxyPort;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherHandler::GetProxyType(char **aProxyType) {
    *aProxyType = mProxyType.ToNewCString();
    if (!*aProxyType) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherHandler::SetProxyType(const char * aProxyType) {
    mProxyType = aProxyType;
    return NS_OK;
}
    
////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsGopherHandler::GetScheme(char* *result) {
    *result = nsCRT::strdup("gopher");
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherHandler::GetDefaultPort(PRInt32 *result) {
    *result = GOPHER_PORT;
    return NS_OK;
}

NS_IMETHODIMP
nsGopherHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                             nsIURI **result) {
    nsresult rv;

    // All gopher URLs are absolute by definition
    NS_ASSERTION(!aBaseURI, "base url passed into gopher protocol handler");

    // I probably need an nsIGopherURL
    nsCOMPtr<nsIStandardURL> url;
    rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                            NS_GET_IID(nsIStandardURL),
                                            getter_AddRefs(url));
    if (NS_FAILED(rv)) return rv;
    rv = url->Init(nsIStandardURL::URLTYPE_STANDARD, GOPHER_PORT,
                   aSpec, aBaseURI);
    if (NS_FAILED(rv)) return rv;

    return url->QueryInterface(NS_GET_IID(nsIURI), (void**)result);
}

NS_IMETHODIMP
nsGopherHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
    nsresult rv;
    
    nsGopherChannel* channel;
    rv = nsGopherChannel::Create(nsnull, NS_GET_IID(nsIChannel),
                                 (void**)&channel);
    if (NS_FAILED(rv)) return rv;
    
    rv = channel->Init(url);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    // Proxy code is largly taken from the ftp and http protocol handlers

    PRBool useProxy = PR_FALSE;
    
    if (mProxySvc &&
        NS_SUCCEEDED(mProxySvc->GetProxyEnabled(&useProxy)) && useProxy) {

        rv = mProxySvc->ExamineForProxy(url, this);
        if (NS_FAILED(rv)) return rv;

        if (mProxyHost.IsEmpty() || !mProxyType.Compare("socks")) {
            *result = channel;
            return NS_OK;
        }

        nsCOMPtr<nsIChannel> proxyChannel;
        // if an gopher proxy is enabled, push things off to HTTP.
        
        nsCOMPtr<nsIHTTPProtocolHandler> httpHandler =
            do_GetService(kHTTPHandlerCID, &rv);
        if (NS_FAILED(rv)) return rv;
        
        // rjc says: the dummy URI (for the HTTP layer) needs to be a syntactically valid URI

        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), "http://example.com");
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
        
        rv = proxyHTTP->SetProxyHost(mProxyHost);
        if (NS_FAILED(rv)) return rv;
        
        rv = proxyHTTP->SetProxyPort(mProxyPort);
        if (NS_FAILED(rv)) return rv;
        
        rv = proxyHTTP->SetProxyType(mProxyType);
        if (NS_FAILED(rv)) return rv;

        *result = proxyChannel;
        NS_ADDREF(*result);
    } else {
        *result = channel;
    }
    return NS_OK;
}
