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
 */

#include "nsFileTransport.h"
#include "nsFileTransportService.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIProgressEventSink.h"
#include "nsIThreadPool.h"
#include "nsISupportsArray.h"
#include "nsFileSpec.h"
#include "nsAutoLock.h"

static NS_DEFINE_CID(kStandardURLCID,            NS_STANDARDURL_CID);

////////////////////////////////////////////////////////////////////////////////

nsFileTransportService::nsFileTransportService()
{
    NS_INIT_REFCNT();
}

#define NS_FILE_TRANSPORT_WORKER_STACK_SIZE     (64 * 1024) /* (8*1024) */

nsresult
nsFileTransportService::Init()
{
    nsresult rv;
    rv = NS_NewThreadPool(getter_AddRefs(mPool), 
                          NS_FILE_TRANSPORT_WORKER_COUNT_MIN,
                          NS_FILE_TRANSPORT_WORKER_COUNT_MAX,
                          NS_FILE_TRANSPORT_WORKER_STACK_SIZE);
    return rv;
}

nsFileTransportService::~nsFileTransportService()
{
    mPool->Shutdown();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsFileTransportService, nsFileTransportService);

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

NS_IMETHODIMP
nsFileTransportService::CreateTransport(nsIFile* file,
                                        PRInt32 ioFlags,
                                        PRInt32 perm,
                                        nsIChannel** result)
{
    nsresult rv;
    nsFileTransport* trans = new nsFileTransport();
    if (trans == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(trans);
    rv = trans->Init(file, ioFlags, perm);
    if (NS_FAILED(rv)) {
        NS_RELEASE(trans);
        return rv;
    }
    *result = trans;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransportService::CreateTransportFromStream(const char* name,
                                                  nsIInputStream *fromStream,
                                                  const char* contentType,
                                                  PRInt32 contentLength,
                                                  nsIChannel** result)
{
    nsresult rv;
    nsFileTransport* trans = new nsFileTransport();
    if (trans == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(trans);
    rv = trans->Init(name, fromStream, contentType, contentLength);
    if (NS_FAILED(rv)) {
        NS_RELEASE(trans);
        return rv;
    }
    *result = trans;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransportService::CreateTransportFromStreamIO(nsIStreamIO *io,
                                                    nsIChannel **result)
{
    nsresult rv;
    nsFileTransport* trans = new nsFileTransport();
    if (trans == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(trans);
    rv = trans->Init(io);
    if (NS_FAILED(rv)) {
        NS_RELEASE(trans);
        return rv;
    }
    *result = trans;
    return NS_OK;
}

nsresult
nsFileTransportService::ProcessPendingRequests(void)
{
    return mPool->ProcessPendingRequests();
}

nsresult
nsFileTransportService::Shutdown(void)
{
    return mPool->Shutdown();
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
        rv = NS_NewISupportsArray(getter_AddRefs(mSuspended));
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
