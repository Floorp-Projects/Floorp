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


#include "nsFTPChannel.h"
#include "nsFtpControlConnection.h"
#include "prlog.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIStreamProvider.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

class nsFtpStreamProvider : public nsIStreamProvider {
public:
    NS_DECL_ISUPPORTS

    nsFtpStreamProvider() { NS_INIT_ISUPPORTS(); }
    virtual ~nsFtpStreamProvider() {}

    //
    // nsIRequestObserver implementation ...
    //
    NS_IMETHODIMP OnStartRequest(nsIRequest *req, nsISupports *ctxt) { return NS_OK; }
    NS_IMETHODIMP OnStopRequest(nsIRequest *req, nsISupports *ctxt, nsresult status) { return NS_OK; }

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
            NS_STATIC_CAST(nsFtpControlConnection*, (nsIStreamListener*)aContext)->mSuspendedWrite = 1;
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
        PRUint32 bytesWritten;
        return aOutStream->WriteFrom(mInStream, PR_MIN(avail, aCount), &bytesWritten);
    }

    nsCOMPtr<nsIInputStream> mInStream;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsFtpStreamProvider,
                              nsIStreamProvider,
                              nsIRequestObserver)


//
// nsFtpControlConnection implementation ...
//

NS_IMPL_THREADSAFE_ISUPPORTS2(nsFtpControlConnection, 
                              nsIStreamListener, 
                              nsIRequestObserver);

nsFtpControlConnection::nsFtpControlConnection(const char* host, PRUint32 port)
{
    NS_INIT_REFCNT();
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpControlConnection created", this));

    mHost.Adopt(nsCRT::strdup(host));
    mPort = port;
    mServerType = 0;

    mLock = PR_NewLock();
    NS_ASSERTION(mLock, "null lock");
}

nsFtpControlConnection::~nsFtpControlConnection() 
{
    if (mLock) PR_DestroyLock(mLock);
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpControlConnection destroyed", this));
}


PRBool
nsFtpControlConnection::IsAlive()
{
    if (!mCPipe) 
        return PR_FALSE;

    PRBool isAlive = PR_FALSE;
    nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface(mCPipe);
    if (!sTrans) return PR_FALSE;

    sTrans->IsAlive(0, &isAlive);
    return isAlive;
}
nsresult 
nsFtpControlConnection::Connect(nsIProxyInfo* proxyInfo)
{
    nsresult rv;

    if (!mCPipe) {        
        nsCOMPtr<nsITransport> transport;
        // build our own
        nsCOMPtr<nsISocketTransportService> sts = do_GetService(kSocketTransportServiceCID, &rv);

        rv = sts->CreateTransport(mHost, 
                                  mPort,
                                  proxyInfo,
                                  FTP_COMMAND_CHANNEL_SEG_SIZE, 
                                  FTP_COMMAND_CHANNEL_MAX_SIZE, 
                                  getter_AddRefs(mCPipe)); // the command transport
        if (NS_FAILED(rv)) return rv;
    } 

    nsCOMPtr<nsISocketTransport> sTrans (do_QueryInterface(mCPipe));
    if (!sTrans)
        return NS_ERROR_FAILURE;
    
    sTrans->SetReuseConnection(PR_TRUE);
    
    nsCOMPtr<nsIInputStream> inStream;
    
    rv = NS_NewPipe(getter_AddRefs(inStream),
                    getter_AddRefs(mOutStream),
                    1024,  // segmentSize
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
                            NS_STATIC_CAST(nsISupports*, (nsIStreamListener*)this),
                            0, PRUint32(-1),
                            nsITransport::DONT_PROXY_PROVIDER |
                            nsITransport::DONT_PROXY_OBSERVER, 
                            getter_AddRefs(mWriteRequest));
    if (NS_FAILED(rv)) return rv;

    // get the ball rolling by reading on the control socket.
    rv = mCPipe->AsyncRead(NS_STATIC_CAST(nsIStreamListener*, this), 
                           nsnull, 0, PRUint32(-1), 0, 
                           getter_AddRefs(mReadRequest));

    return rv;
}

nsresult 
nsFtpControlConnection::Disconnect(nsresult status)
{
    if (!mCPipe) return NS_ERROR_FAILURE;
    
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpControlConnection disconnecting (%x)", this, status));

    if (mWriteRequest) {
        mWriteRequest->Cancel(status);    
        mWriteRequest = nsnull;
    }
    if (mReadRequest) {
        mReadRequest->Cancel(status);
        mReadRequest = nsnull;
    }
    return NS_OK;
}

nsresult 
nsFtpControlConnection::Write(nsCString& command, PRBool suspend)
{
    if (!mCPipe)
        return NS_ERROR_FAILURE;

    PRUint32 len = command.Length();
    PRUint32 cnt;
    nsresult rv = mOutStream->Write(command.get(), len, &cnt);
    
    if (NS_FAILED(rv))
        return rv;
    
    if (len!=cnt) 
        return NS_ERROR_FAILURE;

    if (suspend)
        return NS_OK;

    PRInt32 writeWasSuspended = PR_AtomicSet(&mSuspendedWrite, 0);
    if (writeWasSuspended)
        mWriteRequest->Resume();

    return NS_OK;
    
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
    if (!mCPipe)
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
                            nsresult aStatus)
{
    
    if (!mCPipe) 
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

    return myListener->OnStopRequest(request, aContext, aStatus);
}


NS_IMETHODIMP
nsFtpControlConnection::OnDataAvailable(nsIRequest *request,
                                       nsISupports *aContext,
                                       nsIInputStream *aInStream,
                                       PRUint32 aOffset, 
                                       PRUint32 aCount)
{
    if (!mCPipe) 
        return NS_OK;
    
    PR_Lock(mLock);
    nsCOMPtr<nsIStreamListener> myListener =  mListener;   
    PR_Unlock(mLock);
    
    if (!myListener)
        return NS_OK;

    return myListener->OnDataAvailable(request, aContext, aInStream,
                                      aOffset,  aCount);
}

