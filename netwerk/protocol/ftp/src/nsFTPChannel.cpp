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

// ftp implementation

#include "nsFTPChannel.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIBufferInputStream.h"
#include "nsFtpConnectionThread.h"
#include "nsIEventQueueService.h"
#include "nsIProgressEventSink.h"
#include "nsIEventSinkGetter.h"
#include "nsILoadGroup.h"

#include "prprf.h" // PR_sscanf

static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);

// There are actually two transport connections established for an 
// ftp connection. One is used for the command channel , and
// the other for the data channel. The command channel is the first
// connection made and is used to negotiate the second, data, channel.
// The data channel is driven by the command channel and is either
// initiated by the server (PORT command) or by the client (PASV command).
// Client initiation is the most command case and is attempted first.

nsFTPChannel::nsFTPChannel()
    : mUrl(nsnull), mConnected(PR_FALSE), mListener(nsnull),
      mLoadAttributes(LOAD_NORMAL)
{

    nsresult rv;
    
    NS_INIT_REFCNT();

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &mEventQueue);
    }
    if (NS_FAILED(rv))
        mEventQueue = nsnull;    
}

nsFTPChannel::~nsFTPChannel() {
    NS_IF_RELEASE(mUrl);
    NS_IF_RELEASE(mListener);
    NS_IF_RELEASE(mEventQueue);
}

NS_IMPL_ADDREF(nsFTPChannel);
NS_IMPL_RELEASE(nsFTPChannel);

NS_IMETHODIMP
nsFTPChannel::QueryInterface(const nsIID& aIID, void** aInstancePtr) {
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(nsCOMTypeInfo<nsIFTPChannel>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIChannel>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()) ) {
        *aInstancePtr = NS_STATIC_CAST(nsIFTPChannel*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsIStreamListener>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIStreamObserver>::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

nsresult
nsFTPChannel::Init(const char* verb, nsIURI* uri, nsIEventSinkGetter* getter,
                   nsIEventQueue* queue)
{
    nsresult rv;

    if (mConnected)
        return NS_ERROR_FAILURE;

    if (!getter) {
        return NS_ERROR_FAILURE;
    }

    mUrl = uri;
    NS_ADDREF(mUrl);

    mEventQueue = queue;
    NS_ADDREF(mEventQueue);

    nsIProgressEventSink* eventSink;
    rv = getter->GetEventSink(verb, nsCOMTypeInfo<nsIProgressEventSink>::GetIID(), 
                              (nsISupports**)&eventSink);
    if (NS_FAILED(rv)) return rv;

    mEventSink = eventSink;
    NS_ADDREF(mEventSink);

    return NS_OK;
}

NS_METHOD
nsFTPChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsFTPChannel* fc = new nsFTPChannel();
    if (fc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(fc);
    nsresult rv = fc->QueryInterface(aIID, aResult);
    NS_RELEASE(fc);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsFTPChannel::IsPending(PRBool *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::Cancel(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::Suspend(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::Resume(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsFTPChannel::GetURI(nsIURI * *aURL)
{
    NS_ADDREF(mUrl);
    *aURL = mUrl;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                              nsIInputStream **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                        nsISupports *ctxt,
                        nsIStreamListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::AsyncWrite(nsIInputStream *fromStream,
                         PRUint32 startPosition,
                         PRInt32 writeCount,
                         nsISupports *ctxt,
                         nsIStreamObserver *observer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetContentType(char* *contentType) {
    
    // XXX for ftp we need to do a file extension-to-type mapping lookup
    // XXX in some hash table/registry of mime-types
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
    mLoadGroup = aLoadGroup;    // releases and addrefs
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIFTPChannel methods:

NS_IMETHODIMP
nsFTPChannel::Get(void) {
    nsresult rv;
    nsIThread* workerThread = nsnull;
    nsFtpConnectionThread* protocolInterpreter = 
        new nsFtpConnectionThread(mEventQueue, this, this, nsnull);
    NS_ASSERTION(protocolInterpreter, "ftp protocol interpreter alloc failed");
    NS_ADDREF(protocolInterpreter);

    if (!protocolInterpreter)
        return NS_ERROR_OUT_OF_MEMORY;

    protocolInterpreter->Init(workerThread, mUrl);
    protocolInterpreter->SetUsePasv(PR_TRUE);

    rv = NS_NewThread(&workerThread, protocolInterpreter);
    NS_ASSERTION(NS_SUCCEEDED(rv), "new thread failed");
    
    // XXX this release should probably be in the destructor.
    NS_RELEASE(protocolInterpreter);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::Put(void) {
    nsresult rv;
    nsIThread* workerThread = nsnull;
    nsFtpConnectionThread* protocolInterpreter = 
        new nsFtpConnectionThread(mEventQueue, this, this, nsnull);
    NS_ASSERTION(protocolInterpreter, "ftp protocol interpreter alloc failed");
    NS_ADDREF(protocolInterpreter);

    if (!protocolInterpreter)
        return NS_ERROR_OUT_OF_MEMORY;

    protocolInterpreter->Init(workerThread, mUrl);
    protocolInterpreter->SetAction(PUT);
    protocolInterpreter->SetUsePasv(PR_TRUE);

    rv = NS_NewThread(&workerThread, protocolInterpreter);
    NS_ASSERTION(NS_SUCCEEDED(rv), "new thread failed");
    
    // XXX this release should probably be in the destructor.
    NS_RELEASE(protocolInterpreter);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetStreamListener(nsIStreamListener *aListener) {
    mListener = aListener;
    NS_ADDREF(mListener);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsFTPChannel::OnStartRequest(nsIChannel* channel, nsISupports* context) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::OnStopRequest(nsIChannel* channel, nsISupports* context,
                            nsresult aStatus,
                            const PRUnichar* aMsg) {
    // Release the lock so the user get's the data stream
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsFTPChannel::OnDataAvailable(nsIChannel* channel, nsISupports* context,
                              nsIInputStream *aIStream, 
                              PRUint32 aSourceOffset,
                              PRUint32 aLength) {
    // Fill in the buffer w/ the new data.
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
