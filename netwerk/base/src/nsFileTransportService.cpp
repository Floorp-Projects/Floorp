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

#include "nsFileTransport.h"
#include "nsFileTransportService.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventSinkGetter.h"
#include "nsIProgressEventSink.h"
#include "nsIThreadPool.h"
#include "nsISupportsArray.h"
#include "nsFileSpec.h"
#include "nsAutoLock.h"

static NS_DEFINE_CID(kStandardURLCID,            NS_STANDARDURL_CID);

////////////////////////////////////////////////////////////////////////////////

nsFileTransportService::nsFileTransportService()
    : mPool(nsnull), mSuspended(nsnull)
{
    NS_INIT_REFCNT();
}

#define NS_FILE_TRANSPORT_WORKER_STACK_SIZE     (8*1024)

nsresult
nsFileTransportService::Init()
{
    nsresult rv;
    rv = NS_NewThreadPool(&mPool, NS_FILE_TRANSPORT_WORKER_COUNT,
                          NS_FILE_TRANSPORT_WORKER_COUNT,
                          NS_FILE_TRANSPORT_WORKER_STACK_SIZE);
    return rv;
}

nsFileTransportService::~nsFileTransportService()
{
    // this will wait for all outstanding requests to be processed, then
    // join with the worker threads, and finally free the pool:
    NS_IF_RELEASE(mPool);
    NS_IF_RELEASE(mSuspended);
}

NS_IMPL_ISUPPORTS(nsFileTransportService, NS_GET_IID(nsISupports));

NS_METHOD
nsFileTransportService::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsFileTransportService* ph = new nsFileTransportService();
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

nsresult
nsFileTransportService::ProcessPendingRequests(void)
{
    return mPool->ProcessPendingRequests();
}

nsresult
nsFileTransportService::DispatchRequest(nsIRunnable* runnable)
{
    return mPool->DispatchRequest(runnable);
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsFileTransportService::Suspend(nsIRunnable* request)
{
    nsresult rv;
    nsAutoCMonitor mon(this);   // protect mSuspended
    if (mSuspended == nsnull) {
        rv = NS_NewISupportsArray(&mSuspended);
        if (NS_FAILED(rv)) return rv;
    }
    return mSuspended->AppendElement(request) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
}

nsresult
nsFileTransportService::Resume(nsIRunnable* request)
{
    nsresult rv;
    nsAutoCMonitor mon(this);   // protect mSuspended
    if (mSuspended == nsnull)
        return NS_ERROR_FAILURE;
    // XXX RemoveElement returns a bool instead of nsresult!
    PRBool removed = mSuspended->RemoveElement(request);
    rv = removed ? NS_OK : NS_ERROR_FAILURE;
    if (NS_FAILED(rv)) return rv;

    // restart the request
    rv = mPool->DispatchRequest(request);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
