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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsAboutProtocolHandler.h"
#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIAboutModule.h"
#include "nsString.h"

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

NS_IMPL_ISUPPORTS(nsAboutProtocolHandler, NS_GET_IID(nsIProtocolHandler));

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
nsAboutProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                               nsIURI **result)
{
    nsresult rv;

    // no concept of a relative about url
    NS_ASSERTION(!aBaseURI, "base url passed into about protocol handler");

    nsIURI* url;
    rv = nsComponentManager::CreateInstance(kSimpleURICID, nsnull,
                                            NS_GET_IID(nsIURI),
                                            (void**)&url);
    if (NS_FAILED(rv)) return rv;
    rv = url->SetSpec((char*)aSpec);
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsAboutProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    // about:what you ask?
    nsresult rv;
    char* whatStr;
    rv = uri->GetPath(&whatStr);
    if (NS_FAILED(rv)) return rv;
    
    // look up a handler to deal with "whatStr"
    nsAutoString contractID; contractID.AssignWithConversion(NS_ABOUT_MODULE_CONTRACTID_PREFIX);
    nsAutoString what; what.AssignWithConversion(whatStr);
    nsCRT::free(whatStr);

    // only take up to a question-mark if there is one:
    PRInt32 amt = what.Find("?");
      // STRING USE WARNING: this use needs to be examined -- scc
    contractID.Append(what.GetUnicode(), amt);   // if amt == -1, take it all
    
    char* contractIDStr = contractID.ToNewCString();
    if (contractIDStr == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_WITH_SERVICE(nsIAboutModule, aboutMod, contractIDStr, &rv);
    nsCRT::free(contractIDStr);
    if (NS_SUCCEEDED(rv)) {
        // The standard return case:
        return aboutMod->NewChannel(uri, result);
    }

    // mumble...

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
