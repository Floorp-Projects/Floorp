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
 */
 
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsChromeProtocolHandler.h"
#include "nsIChannel.h"
#include "nsIChromeRegistry.h"
#include "nsIComponentManager.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamListener.h"
#include "nsIServiceManager.h"
#include "nsIXULDocument.h"
#include "nsIXULPrototypeCache.h"
#include "nsIXULPrototypeDocument.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kChromeRegistryCID,         NS_CHROMEREGISTRY_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID,            NS_STANDARDURL_CID);
static NS_DEFINE_CID(kXULDocumentCID,            NS_XULDOCUMENT_CID);
static NS_DEFINE_CID(kXULPrototypeCacheCID,      NS_XULPROTOTYPECACHE_CID);

//----------------------------------------------------------------------
//
//  A channel that's used for loading cached chrome documents.
//

class nsCachedChromeChannel : public nsIChannel
{
protected:
    nsCachedChromeChannel(const char* aCommand, nsIURI* aURI);
    virtual ~nsCachedChromeChannel();

    const char*                 mCommand;
    nsCOMPtr<nsIURI>            mURI;
    nsCOMPtr<nsILoadGroup>      mLoadGroup;

public:
    static nsresult
    Create(const char* aCommand, nsIURI* aURI, nsIChannel** aResult);
	
    NS_DECL_ISUPPORTS

    // nsIRequest
    NS_IMETHOD IsPending(PRBool *_retval) { *_retval = PR_TRUE; return NS_OK; }
    NS_IMETHOD Cancel(void)  { return NS_OK; }
    NS_IMETHOD Suspend(void) { return NS_OK; }
    NS_IMETHOD Resume(void)  { return NS_OK; }

    // nsIChannel    
    NS_DECL_NSICHANNEL
};

NS_IMPL_ADDREF(nsCachedChromeChannel);
NS_IMPL_RELEASE(nsCachedChromeChannel);
NS_IMPL_QUERY_INTERFACE2(nsCachedChromeChannel, nsIRequest, nsIChannel);

nsresult
nsCachedChromeChannel::Create(const char* aCommand, nsIURI* aURI, nsIChannel** aResult)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    nsCachedChromeChannel* channel = new nsCachedChromeChannel(aCommand, aURI);
    if (! channel)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = channel;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsCachedChromeChannel::nsCachedChromeChannel(const char* aCommand, nsIURI* aURI)
    : mCommand(aCommand),
      mURI(aURI)
{
    NS_INIT_REFCNT();
}


nsCachedChromeChannel::~nsCachedChromeChannel()
{
}


NS_IMETHODIMP
nsCachedChromeChannel::GetOriginalURI(nsIURI * *aOriginalURI)
{
    *aOriginalURI = mURI;
    NS_ADDREF(*aOriginalURI);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetURI(nsIURI * *aURI)
{
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount, nsIInputStream **_retval)
{
    NS_NOTREACHED("don't do that");
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    NS_NOTREACHED("don't do that");
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::AsyncOpen(nsIStreamObserver *observer, nsISupports *ctxt)
{
    NS_NOTREACHED("don't do that");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount, nsISupports *ctxt, nsIStreamListener *listener)
{
    if (listener) {
        nsresult rv;
        rv = listener->OnStartRequest(this, ctxt);
        (void) listener->OnStopRequest(this, ctxt, rv, nsnull);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::AsyncWrite(nsIInputStream *fromStream, PRUint32 startPosition, PRInt32 writeCount, nsISupports *ctxt, nsIStreamObserver *observer)
{
    NS_NOTREACHED("don't do that");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
    *aLoadAttributes = nsIChannel::LOAD_NORMAL;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
    NS_NOTREACHED("don't do that");
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetContentType(char * *aContentType)
{
    *aContentType = nsXPIDLCString::Copy("text/xul");
    return *aContentType ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetContentLength(PRInt32 *aContentLength)
{
    NS_NOTREACHED("don't do that");
    *aContentLength = 0;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetOwner(nsISupports * *aOwner)
{
    NS_NOTREACHED("don't do that");
    *aOwner = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetOwner(nsISupports * aOwner)
{
    NS_NOTREACHED("don't do that");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsCachedChromeChannel::GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks)
{
    NS_NOTREACHED("don't do that");
    *aNotificationCallbacks = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCachedChromeChannel::SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks)
{
    NS_NOTREACHED("don't do that");
    return NS_ERROR_FAILURE;
}



////////////////////////////////////////////////////////////////////////////////

nsChromeProtocolHandler::nsChromeProtocolHandler()
{
    NS_INIT_REFCNT();
}

nsresult
nsChromeProtocolHandler::Init()
{
    return NS_OK;
}

nsChromeProtocolHandler::~nsChromeProtocolHandler()
{
}

NS_IMPL_ISUPPORTS(nsChromeProtocolHandler, nsCOMTypeInfo<nsIProtocolHandler>::GetIID());

NS_METHOD
nsChromeProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsChromeProtocolHandler* ph = new nsChromeProtocolHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = ph->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(ph);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsChromeProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("chrome");
    if (*result == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for chrome: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsChromeProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                                nsIURI **result)
{
    nsresult rv;

    // Chrome: URLs (currently) have no additional structure beyond that provided by standard
    // URLs, so there is no "outer" given to CreateInstance 

    nsIURI* url;
    if (aBaseURI) {
        rv = aBaseURI->Clone(&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetRelativePath(aSpec);
    }
    else {
        rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                                nsCOMTypeInfo<nsIURI>::GetIID(),
                                                (void**)&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetSpec((char*)aSpec);
    }

    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsChromeProtocolHandler::NewChannel(const char* aVerb, nsIURI* aURI,
                                    nsILoadGroup* aLoadGroup,
                                    nsIInterfaceRequestor* aNotificationCallbacks,
                                    nsLoadFlags aLoadAttributes,
                                    nsIURI* aOriginalURI,
                                    nsIChannel* *aResult)
{
    nsresult rv;
    nsCOMPtr<nsIChannel> result;

    // First check the prototype cache to see if we've already got the
    // document in the cache.
    NS_WITH_SERVICE(nsIXULPrototypeCache, cache, kXULPrototypeCacheCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIXULPrototypeDocument> proto;
    rv = cache->GetPrototype(aURI, getter_AddRefs(proto));
    if (NS_FAILED(rv)) return rv;

    if (proto) {
        // ...in which case, we'll create a dummy stream that'll just
        // load the thing.
        rv = nsCachedChromeChannel::Create(aVerb, aURI, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // Miss. Resolve the chrome URL using the registry and do a
        // normal necko load.
        NS_WITH_SERVICE(nsIChromeRegistry, reg, kChromeRegistryCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIURI> chromeURI;
        rv = aURI->Clone(getter_AddRefs(chromeURI));        // don't mangle the original
        if (NS_FAILED(rv)) return rv;

        rv = reg->ConvertChromeURL(chromeURI);
        if (NS_FAILED(rv)) return rv;

        // now fetch the converted URI
        NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = serv->NewChannelFromURI(aVerb, chromeURI,
                                     aLoadGroup,
                                     aNotificationCallbacks,
                                     aLoadAttributes,
                                     aOriginalURI ? aOriginalURI : aURI,
                                     getter_AddRefs(result));

        if (NS_FAILED(rv)) return rv;

        // Get a system principal for chrome and set the owner
        // property of the result
        NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIPrincipal> principal;
        rv = securityManager->GetSystemPrincipal(getter_AddRefs(principal));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsISupports> owner = do_QueryInterface(principal);
        result->SetOwner(owner);
    }

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
