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

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

//NS_IMPL_THREADSAFE_ISUPPORTS2(nsFtpControlConnection, 
//                                    nsIStreamListener, 
//                                    nsIStreamObserver);


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

nsFtpControlConnection::nsFtpControlConnection(nsIChannel* socketTransport)
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

    // so that we can share the nsIStreamObserver implemention, we will be checking the context
    // to indicate between the read and the write transport.  We will be passing the a non-null
    // context for the write, and a null context for the read.  

    rv = mCPipe->AsyncWrite(inStream, NS_STATIC_CAST(nsIStreamObserver*, this), NS_STATIC_CAST(nsISupports*, this));
    if (NS_FAILED(rv)) return rv;

    // get the ball rolling by reading on the control socket.
    rv = mCPipe->AsyncRead(NS_STATIC_CAST(nsIStreamListener*, this), nsnull);
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
    if (mCPipe) mCPipe->Cancel(NS_BINDING_ABORTED);
    return NS_OK;
}

nsresult 
nsFtpControlConnection::Write(nsCString& command)
{
    if (!mConnected)
        return NS_ERROR_FAILURE;
#if defined(PR_LOGGING)
    nsCString logString(command);
    logString.ReplaceChar(CRLF, ' ');
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) Writing \"%s\"\n", this, logString.GetBuffer()));
#endif

    PRUint32 len = command.Length();
    PRUint32 cnt;
    nsresult rv = mOutStream->Write(command.GetBuffer(), len, &cnt);
    if (NS_SUCCEEDED(rv) && len==cnt)
        return NS_OK;

    return NS_ERROR_FAILURE;

}

nsresult 
nsFtpControlConnection::GetChannel(nsIChannel** controlChannel)
{
    NS_IF_ADDREF(*controlChannel = mCPipe);
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
nsFtpControlConnection::OnStartRequest(nsIChannel *aChannel, nsISupports *aContext)
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
    
    return mListener->OnStartRequest(aChannel, aContext);
}

NS_IMETHODIMP
nsFtpControlConnection::OnStopRequest(nsIChannel *aChannel, nsISupports *aContext,
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

    return mListener->OnStopRequest(aChannel, aContext, aStatus, aStatusArg);
}


NS_IMETHODIMP
nsFtpControlConnection::OnDataAvailable(nsIChannel *aChannel,
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

    return myListener->OnDataAvailable(aChannel, aContext, aInStream,
                                      aOffset,  aCount);
}
