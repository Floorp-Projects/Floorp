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

nsFileTransportService::nsFileTransportService()    :
    mConnectedTransports (0),
    mTotalTransports (0),
    mInUseTransports (0)
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
                                        nsITransport** result)
{
    nsresult rv;
    nsFileTransport* trans = new nsFileTransport();
    if (trans == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(trans);
    rv = trans->Init(this, file, ioFlags, perm);
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
                                                  nsITransport** result)
{
    nsresult rv;
    nsFileTransport* trans = new nsFileTransport();
    if (trans == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(trans);
    rv = trans->Init(this, name, fromStream, contentType, contentLength);
    if (NS_FAILED(rv)) {
        NS_RELEASE(trans);
        return rv;
    }
    *result = trans;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransportService::CreateTransportFromStreamIO(nsIStreamIO *io,
                                                    nsITransport **result)
{
    nsresult rv;
    nsFileTransport* trans = new nsFileTransport();
    if (trans == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(trans);
    rv = trans->Init(this, io);
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

NS_IMETHODIMP
nsFileTransportService::GetTotalTransportCount    (PRUint32 * o_TransCount)
{
    if (!o_TransCount)
        return NS_ERROR_NULL_POINTER;

    *o_TransCount = (PRUint32) mTotalTransports;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransportService::GetConnectedTransportCount (PRUint32 * o_TransCount)
{
    if (!o_TransCount)
        return NS_ERROR_NULL_POINTER;

    *o_TransCount = (PRUint32) mConnectedTransports;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransportService::GetInUseTransportCount     (PRUint32 * o_TransCount)
{
    if (!o_TransCount)
        return NS_ERROR_NULL_POINTER;

    *o_TransCount = (PRUint32) mInUseTransports;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

