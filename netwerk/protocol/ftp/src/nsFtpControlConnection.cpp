/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsFTPChannel.h"
#include "nsFtpControlConnection.h"
#include "prlog.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsNetUtil.h"
#include "nsEventQueueUtils.h"
#include "nsCRT.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

//
// nsFtpControlConnection implementation ...
//

NS_IMPL_THREADSAFE_ISUPPORTS2(nsFtpControlConnection, 
                              nsIStreamListener, 
                              nsIRequestObserver)

nsFtpControlConnection::nsFtpControlConnection(const char* host, 
                                               PRUint32 port) 
    : mServerType(0), mPort(port)
{
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpControlConnection created", this));

    mHost.Adopt(nsCRT::strdup(host));

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
    mCPipe->IsAlive(&isAlive);
    return isAlive;
}
nsresult 
nsFtpControlConnection::Connect(nsIProxyInfo* proxyInfo,
                                nsITransportEventSink* eventSink)
{
    nsresult rv;

    if (!mCPipe) {
        // build our own
        nsCOMPtr<nsISocketTransportService> sts =
                do_GetService(kSocketTransportServiceCID, &rv);

        rv = sts->CreateTransport(nsnull, 0, mHost, mPort, proxyInfo,
                                  getter_AddRefs(mCPipe)); // the command transport
        if (NS_FAILED(rv)) return rv;

        if (eventSink) {
            nsCOMPtr<nsIEventQueue> eventQ;
            rv = NS_GetCurrentEventQ(getter_AddRefs(eventQ));
            if (NS_SUCCEEDED(rv))
                mCPipe->SetEventSink(eventSink, eventQ);
        }

        // open buffered, blocking output stream to socket.  so long as commands
        // do not exceed 1024 bytes in length, the writing thread (the main thread)
        // will not block.  this should be OK.
        rv = mCPipe->OpenOutputStream(nsITransport::OPEN_BLOCKING, 1024, 1,
                                      getter_AddRefs(mOutStream));
        if (NS_FAILED(rv)) return rv;

        // open buffered, non-blocking/asynchronous input stream to socket.
        nsCOMPtr<nsIInputStream> inStream;
        rv = mCPipe->OpenInputStream(0,
                                     FTP_COMMAND_CHANNEL_SEG_SIZE, 
                                     FTP_COMMAND_CHANNEL_SEG_COUNT,
                                     getter_AddRefs(inStream));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIInputStreamPump> pump;
        rv = NS_NewInputStreamPump(getter_AddRefs(pump), inStream);
        if (NS_FAILED(rv)) return rv;

        // get the ball rolling by reading on the control socket.
        rv = pump->AsyncRead(NS_STATIC_CAST(nsIStreamListener*, this), nsnull);
        if (NS_FAILED(rv)) return rv;

        // cyclic reference!
        mReadRequest = pump;
    }
    return NS_OK;
}

nsresult 
nsFtpControlConnection::Disconnect(nsresult status)
{
    if (!mCPipe) return NS_ERROR_FAILURE;
    
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpControlConnection disconnecting (%x)", this, status));

    if (NS_FAILED(status)) {
        // break cyclic reference!
        mOutStream = 0;
        mReadRequest->Cancel(status);
        mReadRequest = 0;
        mCPipe->Close(status);
        mCPipe = 0;
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

    if (len != cnt)
        return NS_ERROR_FAILURE;
    
    if (suspend)
        return NS_OK;

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

