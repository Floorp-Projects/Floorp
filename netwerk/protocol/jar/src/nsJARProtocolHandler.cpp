/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsJARProtocolHandler.h"
#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIJARChannel.h"

static NS_DEFINE_CID(kIOServiceCID,     NS_IOSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsJARProtocolHandler::nsJARProtocolHandler()
{
    NS_INIT_REFCNT();
}

nsresult
nsJARProtocolHandler::Init()
{
    return NS_OK;
}

nsJARProtocolHandler::~nsJARProtocolHandler()
{
}

NS_IMPL_ISUPPORTS(nsJARProtocolHandler, nsIProtocolHandler::GetIID());

NS_METHOD
nsJARProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsJARProtocolHandler* ph = new nsJARProtocolHandler();
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
nsJARProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("jar");
    if (*result == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsJARProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for JAR: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsJARProtocolHandler::MakeAbsolute(const char* aSpec,
                                     nsIURI* aBaseURI,
                                     char* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// JAR urls have the following syntax
//
// jar:<url>!/(entry)
//
// EXAMPLE: jar:http://www.big.com/blue.jar!/ocean.html
NS_IMETHODIMP
nsJARProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                               nsIURI **result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARProtocolHandler::NewChannel(const char* verb, nsIURI* uri,
                                   nsILoadGroup* loadGroup,
                                   nsIEventSinkGetter* eventSinkGetter,
                                   nsIChannel* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
