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
#include "nsUrl.h"
#include "nscore.h"
#include "nsString2.h"
#include "nsIServiceManager.h"
#include "nsITransportService.h"
#include "nsConnectionGroup.h"

static NS_DEFINE_CID(kTransportService, NS_TRANSPORTSERVICE_CID);

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

NS_IMETHODIMP
nsNetService::GetProtocolHandler(nsIUrl* url, nsIProtocolHandler* *result)
{
    nsresult rv;
    const char* scheme;
    
    rv = url->GetScheme(&scheme);
    if (NS_FAILED(rv)) return rv;

#   define BUF_SIZE 128
    char buf[BUF_SIZE];
    nsAutoString2 progID(NS_NETWORK_PROTOCOL_PROGID_PREFIX);
    progID += scheme;
    progID.ToCString(buf, BUF_SIZE);

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

NS_IMETHODIMP
nsNetService::NewURL(nsIUrl* *result, 
                     const char* aSpec,
                     const nsIUrl* aBaseURL,
                     nsISupports* aContainer)
{
    nsresult rv;
    nsUrl* url = new nsUrl();
    if (url == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = url->Init(aSpec, aBaseURL, aContainer);
    if (NS_FAILED(rv)) {
        delete url;
        return rv;
    }
    NS_ADDREF(url);
    *result = url;
    return NS_OK;
}

NS_IMETHODIMP
nsNetService::Open(nsIUrl* url, nsISupports* eventSink,
                   nsIConnectionGroup* group,
                   nsIProtocolConnection* *result)
{
    nsresult rv;
    nsIProtocolHandler* handler;
    rv = GetProtocolHandler(url, &handler);
    if (NS_FAILED(rv)) return rv;
    return handler->Open(url, eventSink, group, result);
}

NS_IMETHODIMP
nsNetService::HasActiveConnections()
{
    nsresult rv;
    NS_WITH_SERVICE(nsITransportService, trans, kTransportService, &rv);
    if (NS_FAILED(rv)) return rv;

    return trans->HasActiveTransports();
}

////////////////////////////////////////////////////////////////////////////////
