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

#include "nsNetService.h"
#include "nsIProtocolHandler.h"
#include "nsIProtocolConnection.h"
#include "nsUrl.h"
#include "nscore.h"
#include "nsString2.h"
#include "nsXPComCIID.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIFileTransportService.h"
#include "nsConnectionGroup.h"
#include <ctype.h>      // for isalpha

static NS_DEFINE_CID(kFileTransportService, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsNetService::nsNetService()
{
    NS_INIT_REFCNT();
}

nsresult
nsNetService::Init()
{
    return NS_OK;
}

nsNetService::~nsNetService()
{
}

NS_IMPL_ISUPPORTS(nsNetService, nsINetService::GetIID());

////////////////////////////////////////////////////////////////////////////////

#define MAX_SCHEME_LENGTH       64      // XXX big enough?

#define MAX_NET_PROGID_LENGTH   (MAX_SCHEME_LENGTH + NS_NETWORK_PROTOCOL_PROGID_PREFIX_LENGTH + 1)

NS_IMETHODIMP
nsNetService::GetProtocolHandler(const char* scheme, nsIProtocolHandler* *result)
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

NS_IMETHODIMP
nsNetService::NewConnectionGroup(nsIConnectionGroup* *result)
{
    nsConnectionGroup* group = new nsConnectionGroup();
    if (group == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = group->Init();
    if (NS_FAILED(rv)) {
        delete group;
        return rv;
    }
    NS_ADDREF(group);
    *result = group;
    return NS_OK;
}

static nsresult
GetScheme(const char* url, char* schemeBuf)
{
    // search for something up to a colon, and call it the scheme
    PRUint32 i = 0;
    char c;
    while ((c = *url++) != '\0') {
        if (c == ':') {
            schemeBuf[i] = '\0';
            return NS_OK;
        }
        else if (isalpha(c)) {
            schemeBuf[i] = c;
        }

        if (++i == MAX_SCHEME_LENGTH)
            return NS_ERROR_FAILURE;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNetService::MakeAbsoluteUrl(const char* aSpec,
                              nsIUrl* aBaseUrl,
                              char* *result)
{
    nsresult rv;
    char schemeBuf[MAX_SCHEME_LENGTH];
    const char* scheme = schemeBuf;
    rv = GetScheme(aSpec, schemeBuf);
    // XXX if this succeeds, we could return a copy of aSpec
    if (NS_FAILED(rv)) {
        rv = aBaseUrl->GetScheme(&scheme);
        if (NS_FAILED(rv)) return rv;
    }

    nsIProtocolHandler* handler;
    rv = GetProtocolHandler(scheme, &handler);
    if (NS_FAILED(rv)) return rv;

    rv = handler->MakeAbsoluteUrl(aSpec, aBaseUrl, result);
    NS_RELEASE(handler);
    return rv;
}

NS_IMETHODIMP
nsNetService::NewUrl(const char* aSpec,
                     nsIUrl* aBaseUrl,
                     nsIUrl* *result)
{
    nsresult rv;
    char schemeBuf[MAX_SCHEME_LENGTH];
    const char* scheme = schemeBuf;
    rv = GetScheme(aSpec, schemeBuf);
    if (NS_FAILED(rv)) {
        rv = aBaseUrl->GetScheme(&scheme);
        if (NS_FAILED(rv)) return rv;
    }
    
    nsIProtocolHandler* handler;
    rv = GetProtocolHandler(scheme, &handler);
    if (NS_FAILED(rv)) return rv;

    rv = handler->NewUrl(aSpec, aBaseUrl, result);
    NS_RELEASE(handler);
    return rv;
}

NS_IMETHODIMP
nsNetService::NewConnection(nsIUrl* url,
                            nsISupports* eventSink,
                            nsIConnectionGroup* group,
                            nsIProtocolConnection* *result)
{
    nsresult rv;

    const char* scheme;
    rv = url->GetScheme(&scheme);
    if (NS_FAILED(rv)) return rv;

    nsIProtocolHandler* handler;
    rv = GetProtocolHandler(scheme, &handler);
    if (NS_FAILED(rv)) return rv;

    PLEventQueue* eventQ;
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQ);
    }
    if (NS_FAILED(rv)) return rv;

    nsIProtocolConnection* connection;
    rv = handler->NewConnection(url, eventSink, eventQ, &connection);
    NS_RELEASE(handler);
    if (NS_FAILED(rv)) return rv;

    if (group) {
        rv = group->AppendElement(connection);
        if (NS_FAILED(rv)) {
            NS_RELEASE(connection);
            return rv;
        }
    }
    *result = connection;
    return rv;
}

NS_IMETHODIMP
nsNetService::HasActiveConnections()
{
#if 0
    nsresult rv;
    NS_WITH_SERVICE(nsIFileTransportService, trans, kFileTransportService, &rv);
    if (NS_FAILED(rv)) return rv;

    return trans->HasActiveTransports();
#else
    return NS_OK;
#endif
}

////////////////////////////////////////////////////////////////////////////////

    
