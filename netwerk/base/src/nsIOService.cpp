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

#include "nsIOService.h"
#include "nsIProtocolHandler.h"
#include "nscore.h"
#include "nsString2.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIFileTransportService.h"
#include "nsIURI.h"
#include "nsCOMPtr.h"
#include <ctype.h>      // for isalpha

static NS_DEFINE_CID(kFileTransportService, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsIOService::nsIOService()
{
    NS_INIT_REFCNT();
}

nsresult
nsIOService::Init()
{
    return NS_OK;
}

nsIOService::~nsIOService()
{
}

NS_METHOD
nsIOService::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsIOService* ios = new nsIOService();
    if (ios == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ios);
    nsresult rv = ios->QueryInterface(aIID, aResult);
    NS_RELEASE(ios);
    return rv;
}

NS_IMPL_ISUPPORTS(nsIOService, nsIIOService::GetIID());

////////////////////////////////////////////////////////////////////////////////

#define MAX_SCHEME_LENGTH       64      // XXX big enough?

#define MAX_NET_PROGID_LENGTH   (MAX_SCHEME_LENGTH + NS_NETWORK_PROTOCOL_PROGID_PREFIX_LENGTH + 1)

NS_IMETHODIMP
nsIOService::GetProtocolHandler(const char* scheme, nsIProtocolHandler* *result)
{
    nsresult rv;

    NS_ASSERTION(NS_NETWORK_PROTOCOL_PROGID_PREFIX_LENGTH
                 == nsCRT::strlen(NS_NETWORK_PROTOCOL_PROGID_PREFIX),
                 "need to fix NS_NETWORK_PROTOCOL_PROGID_PREFIX_LENGTH");

    // XXX we may want to speed this up by introducing our own protocol 
    // scheme -> protocol handler mapping, avoiding the string manipulation
    // and service manager stuff

    char buf[MAX_NET_PROGID_LENGTH];
    nsAutoString2 progID(NS_NETWORK_PROTOCOL_PROGID_PREFIX);
    progID += scheme;
    progID.ToCString(buf, MAX_NET_PROGID_LENGTH);

    nsIProtocolHandler* handler;
    rv = nsServiceManager::GetService(buf, nsIProtocolHandler::GetIID(),
                                      (nsISupports**)&handler);
    
    if (NS_FAILED(rv)) return rv;

    *result = handler;
    return NS_OK;
}

static nsresult
GetScheme(const char* inURI, char* *scheme)
{
    // search for something up to a colon, and call it the scheme
    char c;
    const char* URI = inURI;
    PRUint32 i = 0;
    PRUint32 length = 0;
    while ((c = *URI++) != '\0') {
        if (c == ':') {
            char* newScheme = new char[length];
            if (newScheme == nsnull)
                return NS_ERROR_OUT_OF_MEMORY;

            nsCRT::memcpy(newScheme, inURI, length);
            newScheme[length] = '\0';
            *scheme = newScheme;
            return NS_OK;
        }
        else if (isalpha(c)) {
            length++;
        }
    }
    return NS_ERROR_FAILURE;    // no colon
}

NS_IMETHODIMP
nsIOService::NewURI(const char* aSpec, nsIURI* aBaseURI,
                    nsIURI* *result)
{
    nsresult rv;
    char* scheme;
    rv = GetScheme(aSpec, &scheme);
    if (NS_FAILED(rv)) {
        if (aBaseURI)
            rv = aBaseURI->GetScheme(&scheme);
        if (NS_FAILED(rv)) return rv;
    }
    
    nsCOMPtr<nsIProtocolHandler> handler;
    rv = GetProtocolHandler(scheme, getter_AddRefs(handler));
    nsCRT::free(scheme);
    if (NS_FAILED(rv)) return rv;

    rv = handler->NewURI(aSpec, aBaseURI, result);
    //NS_RELEASE(handler);
    return rv;
}

NS_IMETHODIMP
nsIOService::NewChannelFromURI(const char* verb, nsIURI *aURI,
                               nsIEventSinkGetter *eventSinkGetter,
                               nsIChannel **result)
{
    nsresult rv;

    char* scheme;
    rv = aURI->GetScheme(&scheme);
    if (NS_FAILED(rv)) return rv;

    nsIProtocolHandler* handler;
    rv = GetProtocolHandler(scheme, &handler);
    nsCRT::free(scheme);
    if (NS_FAILED(rv)) return rv;

    nsIEventQueue* eventQ;
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQ);
    }
    if (NS_FAILED(rv)) return rv;

    nsIChannel* channel;
    rv = handler->NewChannel(verb, aURI, eventSinkGetter, eventQ,
                             &channel);
    NS_RELEASE(handler);
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    return rv;
}

NS_IMETHODIMP
nsIOService::NewChannel(const char* verb, const char *aSpec,
                        nsIURI *aBaseURI,
                        nsIEventSinkGetter *eventSinkGetter,
                        nsIChannel **result)
{
    nsresult rv;
    nsIURI* uri;
    rv = NewURI(aSpec, aBaseURI, &uri);
    if (NS_FAILED(rv)) return rv;
    return NewChannelFromURI(verb, uri, eventSinkGetter, result);
}

NS_IMETHODIMP
nsIOService::MakeAbsolute(const char *aRelativeSpec,
                          nsIURI *aBaseURI,
                          char **result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIOService::GetAppCodeName(nsIString * *aAppCodeName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIOService::GetAppVersion(nsIString * *aAppVersion)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIOService::GetAppName(nsIString * *aAppName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIOService::GetLanguage(nsIString * *aLanguage)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIOService::GetPlatform(nsIString * *aPlatform)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIOService::GetUserAgent(nsIString * *aUserAgent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// HELPER ROUTINES
////////////////////////////////////////////////////////////////////////////////
#if 0
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);

nsresult NS_NewURI(nsIURI** aInstancePtrResult,
                   const char *aSpec,
                   nsIURI* aBaseURI)
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if (nsnull == aInstancePtrResult) {
        return NS_ERROR_NULL_POINTER;
    }

    nsINetService *inet = nsnull;
    nsresult rv = nsServiceManager::GetService(kNetServiceCID,
                                               kINetServiceIID,
                                               (nsISupports **)&inet);
    if (NS_FAILED(rv)) return rv;

    rv = inet->NewURI(aSpec, aInstancePtrResult, aBaseURI);

    if (NS_FAILED(rv)) return rv;

    nsServiceManager::ReleaseService(kNetServiceCID, inet);
    return rv;
}
#endif
////////////////////////////////////////////////////////////////////////////////

    
