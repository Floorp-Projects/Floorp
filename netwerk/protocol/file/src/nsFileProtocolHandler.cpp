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

#include "nsFileChannel.h"
#include "nsFileProtocolHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventSinkGetter.h"
#include "nsIProgressEventSink.h"
#include "nsIThread.h"
#include "nsISupportsArray.h"
#include "nsFileSpec.h"
#include "nsAutoLock.h"

static NS_DEFINE_CID(kStandardURLCID,            NS_STANDARDURL_CID);

////////////////////////////////////////////////////////////////////////////////

nsFileProtocolHandler::nsFileProtocolHandler()
    : mPool(nsnull), mSuspended(nsnull)
{
    NS_INIT_REFCNT();
}

#define NS_FILE_TRANSPORT_WORKER_STACK_SIZE     (8*1024)

nsresult
nsFileProtocolHandler::Init()
{
    nsresult rv;
    rv = NS_NewThreadPool(&mPool, NS_FILE_TRANSPORT_WORKER_COUNT,
                          NS_FILE_TRANSPORT_WORKER_COUNT,
                          NS_FILE_TRANSPORT_WORKER_STACK_SIZE);
    return rv;
}

nsFileProtocolHandler::~nsFileProtocolHandler()
{
    // this will wait for all outstanding requests to be processed, then
    // join with the worker threads, and finally free the pool:
    NS_IF_RELEASE(mPool);
    NS_IF_RELEASE(mSuspended);
}

NS_IMPL_ISUPPORTS(nsFileProtocolHandler, nsCOMTypeInfo<nsIProtocolHandler>::GetIID());

NS_METHOD
nsFileProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsFileProtocolHandler* ph = new nsFileProtocolHandler();
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
nsFileProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("file");
    if (*result == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for file: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::MakeAbsolute(const char* aSpec,
                                    nsIURI* aBaseURI,
                                    char* *result)
{
    // XXX optimize this to not needlessly construct the URL

    nsresult rv;
    nsIURI* url;
    rv = NewURI(aSpec, aBaseURI, &url);
    if (NS_FAILED(rv)) return rv;

    rv = url->GetSpec(result);
    NS_RELEASE(url);
    return rv;
}

NS_IMETHODIMP
nsFileProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                              nsIURI **result)
{
    nsresult rv;

    // file: URLs (currently) have no additional structure beyond that provided by standard
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
nsFileProtocolHandler::NewChannel(const char* verb, nsIURI* url,
                                  nsIEventSinkGetter* eventSinkGetter,
                                  nsIChannel* *result)
{
    nsresult rv;
    
    nsFileChannel* channel;
    rv = nsFileChannel::Create(nsnull, nsCOMTypeInfo<nsIFileChannel>::GetIID(), (void**)&channel);
    if (NS_FAILED(rv)) return rv;

    rv = channel->Init(this, verb, url, eventSinkGetter);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = channel;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileProtocolHandler::NewChannelFromNativePath(const char* nativePath, 
                                                nsIFileChannel* *result)
{
    nsresult rv;
    nsFileSpec spec(nativePath);
    nsFileURL fileURL(spec);
    const char* urlStr = fileURL.GetURLString();
    nsIURI* uri;

    rv = NewURI(urlStr, nsnull, &uri);
    if (NS_FAILED(rv)) return rv;

    rv = NewChannel("load",  // XXX what should this be?
                    uri,
                    nsnull,  // XXX bogus getter
                    (nsIChannel**)result);
    NS_RELEASE(uri);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsFileProtocolHandler::ProcessPendingRequests(void)
{
    return mPool->ProcessPendingRequests();
}

nsresult
nsFileProtocolHandler::DispatchRequest(nsIRunnable* runnable)
{
    return mPool->DispatchRequest(runnable);
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsFileProtocolHandler::Suspend(nsFileChannel* request)
{
    nsresult rv;
    nsAutoCMonitor mon(this);   // protect mSuspended
    if (mSuspended == nsnull) {
        rv = NS_NewISupportsArray(&mSuspended);
        if (NS_FAILED(rv)) return rv;
    }
    return mSuspended->AppendElement(NS_STATIC_CAST(nsIChannel*, request));
}

nsresult
nsFileProtocolHandler::Resume(nsFileChannel* request)
{
    nsresult rv;
    nsAutoCMonitor mon(this);   // protect mSuspended
    if (mSuspended == nsnull)
        return NS_ERROR_FAILURE;
    // XXX RemoveElement returns a bool instead of nsresult!
    PRBool removed = mSuspended->RemoveElement(NS_STATIC_CAST(nsIChannel*, request));
    rv = removed ? NS_OK : NS_ERROR_FAILURE;
    if (NS_FAILED(rv)) return rv;

    // restart the request
    rv = mPool->DispatchRequest(NS_STATIC_CAST(nsIRunnable*, request));
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
