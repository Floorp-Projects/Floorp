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

#include "nsFileTransportService.h"
#include "nsFileTransport.h"
#include "nsIThread.h"
#include "nsIFileStream.h"
#include "prcmon.h"
#include "prmem.h"
#include "nsIStreamListener.h"

////////////////////////////////////////////////////////////////////////////////
// nsFileTransportService methods:

nsFileTransportService::nsFileTransportService()
    : mPool(nsnull)
{
    NS_INIT_REFCNT();
}

nsresult
nsFileTransportService::Init()
{
    nsresult rv;
    rv = NS_NewThreadPool(&mPool, NS_FILE_TRANSPORT_WORKER_COUNT,
                          NS_FILE_TRANSPORT_WORKER_COUNT, 8*1024);
    return rv;
}

nsFileTransportService::~nsFileTransportService()
{
    // this will wait for all outstanding requests to be processed, then
    // join with the worker threads, and finally free the pool:
    NS_IF_RELEASE(mPool);
}

NS_IMPL_ISUPPORTS(nsFileTransportService, nsIFileTransportService::GetIID());

NS_IMETHODIMP
nsFileTransportService::AsyncRead(const char* path, 
                                  nsISupports* context,
                                  PLEventQueue* appEventQueue,
                                  nsIStreamListener* listener,
                                  nsITransport* *result)
{
    nsresult rv;
    nsFileTransport* trans = new nsFileTransport();
    if (trans == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsIStreamListener* asyncListener;
    rv = NS_NewAsyncStreamListener(&asyncListener, appEventQueue, listener);
    if (NS_FAILED(rv)) return rv;

    rv = trans->Init(path, context, asyncListener, this);
    NS_RELEASE(asyncListener);
    if (NS_FAILED(rv)) {
        delete trans;
        return rv;
    }
    NS_ADDREF(trans);

    rv = mPool->DispatchRequest(NS_STATIC_CAST(nsIRunnable*, trans));
    if (NS_FAILED(rv)) {
        NS_RELEASE(trans);
        return rv;
    }

    *result = trans;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransportService::AsyncWrite(nsIInputStream* fromStream,
                                   const char* path,
                                   nsISupports* context,
                                   PLEventQueue* appEventQueue,
                                   nsIStreamObserver* observer,
                                   nsITransport* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransportService::OpenInputStream(const char* path, 
                                        nsISupports* context,
                                        nsIInputStream* *result)
{
    nsresult rv;
    nsFileTransport* trans = new nsFileTransport();
    if (trans == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsIStreamListener* syncListener;
    nsIInputStream* inStr;
    rv = NS_NewSyncStreamListener(&syncListener, &inStr);
    if (NS_FAILED(rv)) return rv;

    rv = trans->Init(path, context, syncListener, this);
    NS_RELEASE(syncListener);
    if (NS_FAILED(rv)) {
        NS_RELEASE(inStr);
        delete trans;
        return rv;
    }
    NS_ADDREF(trans);

    rv = mPool->DispatchRequest(NS_STATIC_CAST(nsIRunnable*, trans));
    NS_RELEASE(trans);
    if (NS_FAILED(rv)) {
        NS_RELEASE(inStr);
        return rv;
    }

    *result = inStr;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransportService::OpenOutputStream(const char* path, 
                                         nsISupports* context,
                                         nsIOutputStream* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransportService::ProcessPendingRequests(void)
{
    return mPool->ProcessPendingRequests();
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsFileTransportService::Suspend(nsFileTransport* request)
{
    nsresult rv;
    if (mSuspended == nsnull) {
        rv = NS_NewISupportsArray(&mSuspended);
        if (NS_FAILED(rv)) return rv;
    }
    return mSuspended->AppendElement(NS_STATIC_CAST(nsITransport*, request));
}

nsresult
nsFileTransportService::Resume(nsFileTransport* request)
{
    nsresult rv;
    if (mSuspended == nsnull)
        return NS_ERROR_FAILURE;
    rv = mSuspended->RemoveElement(NS_STATIC_CAST(nsITransport*, request));
    if (NS_FAILED(rv)) return rv;

    // restart the request
    rv = mPool->DispatchRequest(NS_STATIC_CAST(nsIRunnable*, request));
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
