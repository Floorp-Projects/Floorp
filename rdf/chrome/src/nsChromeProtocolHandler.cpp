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
 
#include "nsChromeProtocolHandler.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIChromeRegistry.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIScriptSecurityManager.h"

static NS_DEFINE_CID(kStandardURLCID,            NS_STANDARDURL_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
static NS_DEFINE_CID(kChromeRegistryCID,         NS_CHROMEREGISTRY_CID);

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
nsChromeProtocolHandler::NewChannel(const char* verb, nsIURI* uri,
                                    nsILoadGroup* aLoadGroup,
                                    nsIInterfaceRequestor* notificationCallbacks,
                                    nsLoadFlags loadAttributes,
                                    nsIURI* originalURI,
                                    nsIChannel* *result)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIChromeRegistry, reg, kChromeRegistryCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIURI* chromeURI;
    rv = uri->Clone(&chromeURI);        // don't mangle the original
    if (NS_FAILED(rv)) return rv;

    rv = reg->ConvertChromeURL(chromeURI);
    if (NS_FAILED(rv)) {
        NS_RELEASE(chromeURI);
        return rv;
    }

    // now fetch the converted URI
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) {
        NS_RELEASE(chromeURI);
        return rv;
    }

    rv = serv->NewChannelFromURI(verb, chromeURI,
                                 aLoadGroup,
                                 notificationCallbacks,
                                 loadAttributes,
                                 originalURI ? originalURI : uri, result);

    // Get a system principal for chrome and set the owner property
    //  of the result
    if (NS_SUCCEEDED(rv)) {
        nsresult rv2;
        NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                        NS_SCRIPTSECURITYMANAGER_PROGID, &rv2);
        if (NS_FAILED(rv2)) 
            return NS_ERROR_FAILURE;
        nsCOMPtr<nsIPrincipal> principal;
        if (NS_FAILED(securityManager->GetSystemPrincipal(getter_AddRefs(principal))))
        {
            return NS_ERROR_FAILURE;
        }
        nsCOMPtr<nsISupports> owner = do_QueryInterface(principal);
        (*result)->SetOwner(owner);
    }

    NS_RELEASE(chromeURI);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
