/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsNetCID.h"

////////////////////////////////////////////////////////////////////////////////

nsFileTransportService::nsFileTransportService()    :
    mConnectedTransports (0),
    mTotalTransports (0),
    mInUseTransports (0),
    mShuttingDown(PR_FALSE),
    mLock(nsnull)
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
    if (NS_FAILED(rv))
        return rv;

    mLock = PR_NewLock();
    if (!mLock)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

nsFileTransportService::~nsFileTransportService()
{
    if (mLock)
        PR_DestroyLock(mLock);
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
                                                  PRBool closeStreamWhenDone,
                                                  nsITransport** result)
{
    nsresult rv;
    nsFileTransport* trans = new nsFileTransport();
    if (trans == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(trans);
    rv = trans->Init(this, name, fromStream, contentType, contentLength, closeStreamWhenDone);
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
    PR_Lock(mLock);
    mShuttingDown = 1;
    PR_Unlock(mLock);

    PRUint32 count;
    mSuspendedTransportList.Count(&count);

    for (PRUint32 i=0; i<count; i++) {
        nsCOMPtr<nsISupports> supports;
        mSuspendedTransportList.GetElementAt(i, getter_AddRefs(supports));
        nsCOMPtr<nsIRequest> trans = do_QueryInterface(supports);
        trans->Cancel(NS_BINDING_ABORTED);
    }

    mSuspendedTransportList.Clear();
    return mPool->Shutdown();
}

nsresult
nsFileTransportService::DispatchRequest(nsIRunnable* runnable)
{
    return mPool->DispatchRequest(runnable);
}

////////////////////////////////////////////////////////////////////////////////
nsresult 
nsFileTransportService::AddSuspendedTransport(nsITransport* trans)
{
    nsAutoLock lock(mLock);

    if (mShuttingDown)
        return NS_ERROR_FAILURE;

    NS_STATIC_CAST(nsISupportsArray*, &mSuspendedTransportList)->AppendElement(trans);
    return NS_OK;
}

nsresult 
nsFileTransportService::RemoveSuspendedTransport(nsITransport* trans)
{
    nsAutoLock lock(mLock);

    if (mShuttingDown)
        return NS_ERROR_FAILURE;

    NS_STATIC_CAST(nsISupportsArray*, &mSuspendedTransportList)->RemoveElement(trans);
    return NS_OK;
}

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

