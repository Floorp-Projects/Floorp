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


#include "nsFtpControlConnection.h"
#include "prlog.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIStreamProvider.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

class nsFtpStreamProvider : public nsIStreamProvider {
public:
    NS_DECL_ISUPPORTS

    nsFtpStreamProvider() { NS_INIT_ISUPPORTS(); }
    virtual ~nsFtpStreamProvider() {}

    //
    // nsIStreamObserver implementation ...
    //
    NS_IMETHODIMP OnStartRequest(nsIRequest *req, nsISupports *ctxt) { return NS_OK; }
    NS_IMETHODIMP OnStopRequest(nsIRequest *req, nsISupports *ctxt, nsresult status, const PRUnichar *statusText) { return NS_OK; }

    //
    // nsIStreamProvider implementation ...
    //
    NS_IMETHODIMP OnDataWritable(nsIRequest *aRequest, nsISupports *aContext,
                                 nsIOutputStream *aOutStream,
                                 PRUint32 aOffset, PRUint32 aCount)
    { 
        NS_ASSERTION(mInStream, "not initialized");

        nsresult rv;
        PRUint32 avail;

        // Write whatever is available in the pipe. If the pipe is empty, then
        // return NS_BASE_STREAM_WOULD_BLOCK; we will resume the write when there
        // is more data.

        rv = mInStream->Available(&avail);
        if (NS_FAILED(rv)) return rv;

        if (avail == 0) {
            NS_STATIC_CAST(nsFtpControlConnection*, aContext)->mSuspendedWrite = PR_TRUE;
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
        PRUint32 bytesWritten;
        return aOutStream->WriteFrom(mInStream, PR_MIN(avail, aCount), &bytesWritten);
    }

    nsCOMPtr<nsIInputStream> mInStream;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsFtpStreamProvider,
                              nsIStreamProvider,
                              nsIStreamObserver)


//
// nsFtpControlConnection implementation ...
//

NS_IMPL_THREADSAFE_QUERY_INTERFACE2(nsFtpControlConnection, 
                                    nsIStreamListener, 
                                    nsIStreamObserver);

NS_IMPL_THREADSAFE_ADDREF(nsFtpControlConnection);
nsrefcnt nsFtpControlConnection::Release(void)
{
    nsrefcnt count;
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    count = PR_AtomicDecrement((PRInt32 *)&mRefCnt);
    NS_LOG_RELEASE(this, count, "nsFtpControlConnection");
    if (0 == count) {
        mRefCnt = 1; /* stabilize */
        /* enable this to find non-threadsafe destructors: */
        /* NS_ASSERT_OWNINGTHREAD(_class); */
        NS_DELETEXPCOM(this);
        return 0;
    } 
    else if (1 == count && mConnected)
    {
        // mPipe has a reference to |this| caused by AsyncRead() 
        // Break this cycle by calling disconnect.  
        Disconnect();
    }
    return count;
}

nsFtpControlConnection::nsFtpControlConnection(nsITransport* socketTransport)
:  mCPipe(socketTransport)
{
    NS_INIT_REFCNT();
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpControlConnection created", this));
    mServerType = 0;
    mConnected = mList = PR_FALSE;

    mLock = PR_NewLock();
    NS_ASSERTION(mLock, "null lock");
}

nsFtpControlConnection::~nsFtpControlConnection() 
{
    if (mLock) PR_DestroyLock(mLock);
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpControlConnection destroyed", this));
}

nsresult 
nsFtpControlConnection::Connect()
{
    if (!mCPipe) 
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    nsCOMPtr<nsIInputStream> inStream;

    rv = NS_NewPipe(getter_AddRefs(inStream),
                    getter_AddRefs(mOutStream),
                    64,  // segmentSize
                    256, // maxSize
                    PR_TRUE, 
                    PR_TRUE);

    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamProvider> provider;
    NS_NEWXPCOM(provider, nsFtpStreamProvider);
    if (!provider) return NS_ERROR_OUT_OF_MEMORY;

    // setup the stream provider to get data from the pipe.
    NS_STATIC_CAST(nsFtpStreamProvider*, 
        NS_STATIC_CAST(nsIStreamProvider*, provider))->mInStream = inStream;

    rv = mCPipe->AsyncWrite(provider, 
                            NS_STATIC_CAST(nsISupports*, this),
                            0, 0, 0, 
                            getter_AddRefs(mWriteRequest));
    if (NS_FAILED(rv)) return rv;

    // get the ball rolling by reading on the control socket.
    rv = mCPipe->AsyncRead(NS_STATIC_CAST(nsIStreamListener*, this), 
                           nsnull, 0, -1, 0, 
                           getter_AddRefs(mReadRequest));

    if (NS_FAILED(rv)) return rv;

    mConnected = PR_TRUE;
    return NS_OK;
}

nsresult 
nsFtpControlConnection::Disconnect()
{
    if (!mConnected) return NS_ERROR_FAILURE;
    
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpControlConnection disconnecting", this));
    mConnected = PR_FALSE;
    if (mWriteRequest) mWriteRequest->Cancel(NS_BINDING_ABORTED);
    if (mReadRequest) mReadRequest->Cancel(NS_BINDING_ABORTED);
    return NS_OK;
}

nsresult 
nsFtpControlConnection::Write(nsCString& command)
{
    if (!mConnected)
        return NS_ERROR_FAILURE;

    PRUint32 len = command.Length();
    PRUint32 cnt;
    nsresult rv = mOutStream->Write(command.GetBuffer(), len, &cnt);
    if (NS_SUCCEEDED(rv) && len==cnt) {
        if (mSuspendedWrite) {
            mSuspendedWrite = PR_FALSE;
            mWriteRequest->Resume();
        }
        return NS_OK;
    }

    return NS_ERROR_FAILURE;

}

nsresult 
nsFtpControlConnection::GetTransport(nsITransport** controlTransport)
{
    NS_IF_ADDREF(*controlTransport = mCPipe);
    return NS_OK;
}

nsresult 
nsFtpControlConnection::SetStreamListener(nsIStreamListener *aListener)
{
    nsAutoLock lock(mLock);
    mListener = aListener;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpControlConnection::OnStartRequest(nsIRequest *request, nsISupports *aContext)
{
    if (!mConnected)
        return NS_OK;

    // we do not care about notifications from the write channel.
    // a non null context indicates that this is a write notification.
    if (aContext != nsnull) 
        return NS_OK;
    
    PR_Lock(mLock);
    nsCOMPtr<nsIStreamListener> myListener =  mListener;   
    PR_Unlock(mLock);
    
    if (!myListener)
        return NS_OK;
    
    return myListener->OnStartRequest(request, aContext);
}

NS_IMETHODIMP
nsFtpControlConnection::OnStopRequest(nsIRequest *request, nsISupports *aContext,
                            nsresult aStatus, const PRUnichar* aStatusArg)
{
    
    if (!mConnected) 
        return NS_OK;

    // we do not care about successful notifications from the write channel.
    // a non null context indicates that this is a write notification.
    if (aContext != nsnull && NS_SUCCEEDED(aStatus))
        return NS_OK;
    
    PR_Lock(mLock);
    nsCOMPtr<nsIStreamListener> myListener =  mListener;   
    PR_Unlock(mLock);
    
    if (!myListener)
        return NS_OK;

    return myListener->OnStopRequest(request, aContext, aStatus, aStatusArg);
}


NS_IMETHODIMP
nsFtpControlConnection::OnDataAvailable(nsIRequest *request,
                                       nsISupports *aContext,
                                       nsIInputStream *aInStream,
                                       PRUint32 aOffset, 
                                       PRUint32 aCount)
{
    if (!mConnected) 
        return NS_OK;
    
    PR_Lock(mLock);
    nsCOMPtr<nsIStreamListener> myListener =  mListener;   
    PR_Unlock(mLock);
    
    if (!myListener)
        return NS_OK;

    return myListener->OnDataAvailable(request, aContext, aInStream,
                                      aOffset,  aCount);
}
