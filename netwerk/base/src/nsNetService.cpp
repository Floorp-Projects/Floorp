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
#include "nsURL.h"
#include "nscore.h"
#include "nsString2.h"
#include "nsIServiceManager.h"
#include "nsIFileTransportService.h"
#include "nsConnectionGroup.h"

static NS_DEFINE_CID(kFileTransportService, NS_FILETRANSPORTSERVICE_CID);

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
nsNetService::GetProtocolHandler(nsIURL* url, nsIProtocolHandler* *result)
{
#if 0 // will fix later... 
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
#endif // fix TODO
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
nsNetService::NewURL(nsIURL* *result, 
                     const char* aSpec,
                     const nsIURL* aBaseURL,
                     nsISupports* aContainer)
{
#if 0
    nsresult rv;
    nsURL* url;
    if (aBaseURL)
    {
        aBaseURL->Clone(&url);
        PR_ASSERT(url);
        if (!url)
            return NS_ERROR_OUT_OF_MEMORY;
        rv = url->MakeAbsoluteFrom(aSpec);
        if (NS_FAILED(rv))
        {
            delete url;
            url = 0;
            return rv;
        }
    }
    else
    {
        url = new nsURL(aSpec);
        if (nsnull == url)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(url);
    *result = url;
#endif // TODO 
    return NS_OK;
}


NS_IMETHODIMP
nsNetService::Open(nsIURL* url, nsISupports* eventSink,
                   nsIConnectionGroup* group,
                   nsIProtocolInstance* *result)
{
#if 0
    nsresult rv;
    nsIProtocolHandler* handler;
    rv = GetProtocolHandler(url, &handler);
    if (NS_FAILED(rv)) return rv;
    return handler->Open(url, eventSink, group, result);
#endif // TODO - there is no open on the handler.
    return NS_OK;
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
