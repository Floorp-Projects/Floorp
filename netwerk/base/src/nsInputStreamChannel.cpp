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

#include "nsInputStreamChannel.h"
#include "nsCOMPtr.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIMIMEService.h"
#include "nsIFileTransportService.h"
#include "netCore.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////
// nsInputStreamChannel methods:

nsInputStreamChannel::nsInputStreamChannel()
    : mContentType(nsnull), mContentLength(-1), mLoadAttributes(LOAD_NORMAL),
      mBufferSegmentSize(0), mBufferMaxSize(0), mStatus(NS_OK)
{
    NS_INIT_REFCNT(); 
}

nsInputStreamChannel::~nsInputStreamChannel()
{
    if (mContentType) nsCRT::free(mContentType);
}

NS_METHOD
nsInputStreamChannel::Create(nsISupports *aOuter, REFNSIID aIID,
                             void **aResult)
{
    nsInputStreamChannel* channel = new nsInputStreamChannel();
    if (channel == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(channel);
    nsresult rv = channel->QueryInterface(aIID, aResult);
    NS_RELEASE(channel);
    return rv;
}

NS_IMETHODIMP
nsInputStreamChannel::Init(nsIURI* uri, 
                           nsIInputStream* in,
                           const char* contentType,
                           PRInt32 contentLength)
{
    mURI = uri;
    mContentLength = contentLength;

    if (contentType) { 
        mContentType = nsCRT::strdup(contentType);
        const char *constContentType = mContentType;
        if (!constContentType) return NS_ERROR_OUT_OF_MEMORY;
        char* semicolon = PL_strchr(constContentType, ';');
        CBufDescriptor cbd(constContentType,
                           PR_TRUE,
                           semicolon ? (semicolon-constContentType) + 1: PL_strlen(constContentType), // capacity 
                           semicolon ? (semicolon-constContentType) : PL_strlen(constContentType)); 
        nsCAutoString(cbd).ToLowerCase(); 
    } 
    mInputStream = in;
    return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS5(nsInputStreamChannel, 
                              nsIInputStreamChannel,
                              nsIChannel,
                              nsIRequest,
                              nsIStreamObserver,
                              nsIStreamListener);

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsInputStreamChannel::IsPending(PRBool *result)
{
    if (mFileTransport)
        return mFileTransport->IsPending(result);
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    mStatus = status;
    if (mFileTransport)
        return mFileTransport->Cancel(status);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::Suspend(void)
{
    if (mFileTransport)
        return mFileTransport->Suspend();
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::Resume(void)
{
    if (mFileTransport)
        return mFileTransport->Resume();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsInputStreamChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mURI;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetOriginalURI(nsIURI* aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mURI;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetURI(nsIURI* aURI)
{
    mURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::AsyncOpen(nsIStreamObserver *observer, nsISupports* ctxt)
{
    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    nsresult rv;
    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString spec;
    rv = mURI->GetSpec(getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;
    rv = fts->CreateTransportFromStream(mInputStream, spec, mContentType, mContentLength,
                                        getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) return rv;
    if (mBufferSegmentSize > 0) {
        rv = mFileTransport->SetBufferSegmentSize(mBufferSegmentSize);
        if (NS_FAILED(rv)) return rv;
    }
    if (mBufferMaxSize > 0) {
        rv = mFileTransport->SetBufferMaxSize(mBufferMaxSize);
        if (NS_FAILED(rv)) return rv;
    }

    return mFileTransport->AsyncOpen(observer, ctxt);
}

NS_IMETHODIMP
nsInputStreamChannel::OpenInputStream(nsIInputStream **result)
{
    *result = mInputStream;
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::OpenOutputStream(nsIOutputStream **_retval)
{
    // we don't do output
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsInputStreamChannel::AsyncRead(nsIStreamListener *listener, nsISupports *ctxt)
{
    nsresult rv;

    NS_ASSERTION(listener, "no listener");
    mRealListener = listener;

    if (mLoadGroup) {
        nsCOMPtr<nsILoadGroupListenerFactory> factory;
        //
        // Create a load group "proxy" listener...
        //
        rv = mLoadGroup->GetGroupListenerFactory(getter_AddRefs(factory));
        if (factory) {
            nsIStreamListener *newListener;
            rv = factory->CreateLoadGroupListener(mRealListener, &newListener);
            if (NS_SUCCEEDED(rv)) {
                mRealListener = newListener;
                NS_RELEASE(newListener);
            }
        }

        rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    if (mFileTransport == nsnull) {
        NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsXPIDLCString spec;
        rv = mURI->GetSpec(getter_Copies(spec));
        if (NS_FAILED(rv)) return rv;
        rv = fts->CreateTransportFromStream(mInputStream, spec, mContentType, mContentLength,
                                            getter_AddRefs(mFileTransport));
        if (NS_FAILED(rv)) return rv;
        if (mBufferSegmentSize > 0) {
            rv = mFileTransport->SetBufferSegmentSize(mBufferSegmentSize);
            if (NS_FAILED(rv)) return rv;
        }
        if (mBufferMaxSize > 0) {
            rv = mFileTransport->SetBufferMaxSize(mBufferMaxSize);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return mFileTransport->AsyncRead(this, ctxt);
}

NS_IMETHODIMP
nsInputStreamChannel::AsyncWrite(nsIInputStream *fromStream, 
                                 nsIStreamObserver *observer, nsISupports *ctxt)
{
    // we don't do output
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsInputStreamChannel::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetContentType(char * *aContentType)
{
    *aContentType = nsCRT::strdup(mContentType);
    if (*aContentType == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetContentType(const char *aContentType)
{
    if (mContentType) {
        nsCRT::free(mContentType);
    }
    mContentType = nsCRT::strdup(aContentType);
    if (aContentType == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInputStreamChannel::GetTransferOffset(PRUint32 *aTransferOffset)
{
    NS_NOTREACHED("GetTransferOffset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInputStreamChannel::SetTransferOffset(PRUint32 aTransferOffset)
{
    NS_NOTREACHED("SetTransferOffset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInputStreamChannel::GetTransferCount(PRInt32 *aTransferCount)
{
    NS_NOTREACHED("GetTransferCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInputStreamChannel::SetTransferCount(PRInt32 aTransferCount)
{
    NS_NOTREACHED("SetTransferCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInputStreamChannel::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
    *aBufferSegmentSize = mBufferSegmentSize;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
    mBufferSegmentSize = aBufferSegmentSize;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
    *aBufferMaxSize = mBufferMaxSize;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
    mBufferMaxSize = aBufferMaxSize;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetShouldCache(PRBool *aShouldCache)
{
    *aShouldCache = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
    *aPipeliningAllowed = PR_FALSE;
    return NS_OK;
}
 
NS_IMETHODIMP
nsInputStreamChannel::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
    NS_NOTREACHED("SetPipeliningAllowed");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInputStreamChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup.get();
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}


NS_IMETHODIMP 
nsInputStreamChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsInputStreamChannel::OnStartRequest(nsIChannel* transportChannel, nsISupports* context)
{
    NS_ASSERTION(mRealListener, "No listener...");
    return mRealListener->OnStartRequest(this, context);
}

NS_IMETHODIMP
nsInputStreamChannel::OnStopRequest(nsIChannel* transportChannel, nsISupports* context,
                                    nsresult aStatus, const PRUnichar* aMsg)
{
    nsresult rv;

    rv = mRealListener->OnStopRequest(this, context, aStatus, aMsg);

    if (mLoadGroup) {
        if (NS_SUCCEEDED(rv)) {
            mLoadGroup->RemoveChannel(this, context, aStatus, aMsg);
        }
    }

    // Release the reference to the consumer stream listener...
    mRealListener = null_nsCOMPtr();
    mFileTransport = null_nsCOMPtr();
    return rv;
}

NS_IMETHODIMP
nsInputStreamChannel::OnDataAvailable(nsIChannel* transportChannel, nsISupports* context,
                                      nsIInputStream *aIStream, PRUint32 aSourceOffset,
                                      PRUint32 aLength)
{
    return mRealListener->OnDataAvailable(this, context, aIStream,
                                          aSourceOffset, aLength);
}

////////////////////////////////////////////////////////////////////////////////
