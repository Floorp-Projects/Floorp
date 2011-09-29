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

#include "nsIOService.h"
#include "nsFTPChannel.h"
#include "nsFtpControlConnection.h"
#include "nsFtpProtocolHandler.h"
#include "prlog.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsCRT.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif
#define LOG(args)         PR_LOG(gFTPLog, PR_LOG_DEBUG, args)
#define LOG_ALWAYS(args)  PR_LOG(gFTPLog, PR_LOG_ALWAYS, args)

//
// nsFtpControlConnection implementation ...
//

NS_IMPL_ISUPPORTS1(nsFtpControlConnection, nsIInputStreamCallback)

NS_IMETHODIMP
nsFtpControlConnection::OnInputStreamReady(nsIAsyncInputStream *stream)
{
    char data[4096];

    // Consume data whether we have a listener or not.
    PRUint32 avail;
    nsresult rv = stream->Available(&avail);
    if (NS_SUCCEEDED(rv)) {
        if (avail > sizeof(data))
            avail = sizeof(data);

        PRUint32 n;
        rv = stream->Read(data, avail, &n);
        if (NS_SUCCEEDED(rv) && n != avail)
            avail = n;
    }

    // It's important that we null out mListener before calling one of its
    // methods as it may call WaitData, which would queue up another read.

    nsRefPtr<nsFtpControlConnectionListener> listener;
    listener.swap(mListener);

    if (!listener)
        return NS_OK;

    if (NS_FAILED(rv)) {
        listener->OnControlError(rv);
    } else {
        listener->OnControlDataAvailable(data, avail);
    }

    return NS_OK;
}

nsFtpControlConnection::nsFtpControlConnection(const nsCSubstring& host,
                                               PRUint32 port)
    : mServerType(0), mSessionId(gFtpHandler->GetSessionId()), mHost(host)
    , mPort(port)
{
    LOG_ALWAYS(("FTP:CC created @%p", this));
}

nsFtpControlConnection::~nsFtpControlConnection() 
{
    LOG_ALWAYS(("FTP:CC destroyed @%p", this));
}

bool
nsFtpControlConnection::IsAlive()
{
    if (!mSocket) 
        return PR_FALSE;

    bool isAlive = false;
    mSocket->IsAlive(&isAlive);
    return isAlive;
}
nsresult 
nsFtpControlConnection::Connect(nsIProxyInfo* proxyInfo,
                                nsITransportEventSink* eventSink)
{
    if (mSocket)
        return NS_OK;

    // build our own
    nsresult rv;
    nsCOMPtr<nsISocketTransportService> sts =
            do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = sts->CreateTransport(nsnull, 0, mHost, mPort, proxyInfo,
                              getter_AddRefs(mSocket)); // the command transport
    if (NS_FAILED(rv))
        return rv;

    mSocket->SetQoSBits(gFtpHandler->GetControlQoSBits());

    // proxy transport events back to current thread
    if (eventSink)
        mSocket->SetEventSink(eventSink, NS_GetCurrentThread());

    // open buffered, blocking output stream to socket.  so long as commands
    // do not exceed 1024 bytes in length, the writing thread (the main thread)
    // will not block.  this should be OK.
    rv = mSocket->OpenOutputStream(nsITransport::OPEN_BLOCKING, 1024, 1,
                                   getter_AddRefs(mSocketOutput));
    if (NS_FAILED(rv))
        return rv;

    // open buffered, non-blocking/asynchronous input stream to socket.
    nsCOMPtr<nsIInputStream> inStream;
    rv = mSocket->OpenInputStream(0,
                                  nsIOService::gDefaultSegmentSize,
                                  nsIOService::gDefaultSegmentCount,
                                  getter_AddRefs(inStream));
    if (NS_SUCCEEDED(rv))
        mSocketInput = do_QueryInterface(inStream);
    
    return rv;
}

nsresult
nsFtpControlConnection::WaitData(nsFtpControlConnectionListener *listener)
{
    LOG(("FTP:(%p) wait data [listener=%p]\n", this, listener));

    // If listener is null, then simply disconnect the listener.  Otherwise,
    // ensure that we are listening.
    if (!listener) {
        mListener = nsnull;
        return NS_OK;
    }

    NS_ENSURE_STATE(mSocketInput);

    mListener = listener;
    return mSocketInput->AsyncWait(this, 0, 0, NS_GetCurrentThread());
}

nsresult 
nsFtpControlConnection::Disconnect(nsresult status)
{
    if (!mSocket)
        return NS_OK;  // already disconnected
    
    LOG_ALWAYS(("FTP:(%p) CC disconnecting (%x)", this, status));

    if (NS_FAILED(status)) {
        // break cyclic reference!
        mSocket->Close(status);
        mSocket = 0;
        mSocketInput->AsyncWait(nsnull, 0, 0, nsnull);  // clear any observer
        mSocketInput = nsnull;
        mSocketOutput = nsnull;
    }

    return NS_OK;
}

nsresult 
nsFtpControlConnection::Write(const nsCSubstring& command)
{
    NS_ENSURE_STATE(mSocketOutput);

    PRUint32 len = command.Length();
    PRUint32 cnt;
    nsresult rv = mSocketOutput->Write(command.Data(), len, &cnt);

    if (NS_FAILED(rv))
        return rv;

    if (len != cnt)
        return NS_ERROR_FAILURE;
    
    return NS_OK;
}
