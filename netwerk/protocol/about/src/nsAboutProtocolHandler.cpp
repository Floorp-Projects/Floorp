/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsAboutProtocolHandler.h"
#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIAboutModule.h"

static NS_DEFINE_CID(kSimpleURICID,     NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kIOServiceCID,     NS_IOSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsAboutProtocolHandler::nsAboutProtocolHandler()
{
    NS_INIT_REFCNT();
}

nsresult
nsAboutProtocolHandler::Init()
{
    return NS_OK;
}

nsAboutProtocolHandler::~nsAboutProtocolHandler()
{
}

NS_IMPL_ISUPPORTS(nsAboutProtocolHandler, nsIProtocolHandler::GetIID());

NS_METHOD
nsAboutProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsAboutProtocolHandler* ph = new nsAboutProtocolHandler();
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
nsAboutProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("about");
    if (*result == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for about: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::MakeAbsolute(const char* aSpec,
                                     nsIURI* aBaseURI,
                                     char* *result)
{
    // presumably, there's no such thing as a relative about: URI,
    // so just copy the input spec
    char* dup = nsCRT::strdup(aSpec);
    if (dup == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = dup;
    return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                               nsIURI **result)
{
    nsresult rv;

    // about: URIs are implemented by the "Simple URI" implementation

    nsIURI* url;
    if (aBaseURI) {
        rv = aBaseURI->Clone(&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetRelativePath(aSpec);
    }
    else {
        rv = nsComponentManager::CreateInstance(kSimpleURICID, nsnull,
                                                nsIURI::GetIID(),
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
nsAboutProtocolHandler::NewChannel(const char* verb, nsIURI* uri,
                                   nsILoadGroup *aGroup,
                                   nsIEventSinkGetter* eventSinkGetter,
                                   nsIChannel* *result)
{
    // about:what you ask?
    nsresult rv;
    char* whatStr;
    rv = uri->GetPath(&whatStr);
    if (NS_FAILED(rv)) return rv;
    
    // look up a handler to deal with "whatStr"
    nsAutoString progID(NS_ABOUT_MODULE_PROGID_PREFIX);
    nsAutoString what(whatStr);
    nsCRT::free(whatStr);

    // only take up to a question-mark if there is one:
    PRInt32 amt = what.Find("?");
    progID.Append(what, amt);   // if amt == -1, take it all
    
    char* progIDStr = progID.ToNewCString();
    if (progIDStr == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_WITH_SERVICE(nsIAboutModule, aboutMod, progIDStr, &rv);
    nsCRT::free(progIDStr);
    if (NS_SUCCEEDED(rv)) {
        // The standard return case:
        return aboutMod->NewChannel(verb, uri, aGroup, eventSinkGetter, 
                                    result);
    }

    // mumble...

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
