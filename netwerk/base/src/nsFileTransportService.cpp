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
    NS_IF_RELEASE(mPool);
}

NS_IMPL_ISUPPORTS(nsFileTransportService, nsIFileTransportService::GetIID());

NS_IMETHODIMP
nsFileTransportService::AsyncRead(PLEventQueue* appEventQueue,
                                  nsIProtocolConnection* connection,
                                  nsIStreamListener* listener,
                                  const char* path, 
                                  nsITransport* *result)
{
    nsFileTransport* trans = new nsFileTransport();
    if (trans == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = trans->Init(path, listener, appEventQueue, connection);
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
nsFileTransportService::AsyncWrite(PLEventQueue* appEventQueue,
                                   nsIProtocolConnection* connection,
                                   nsIStreamObserver* observer,
                                   const char* path,
                                   nsITransport* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
