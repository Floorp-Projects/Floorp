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

#include "nsInputStreamChannel.h"
#include "nsCOMPtr.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIFileTransportService.h"
#include "netCore.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsNetCID.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////
// nsInputStreamIO methods:

NS_IMPL_THREADSAFE_ISUPPORTS2(nsInputStreamIO,
                              nsIInputStreamIO,
                              nsIStreamIO)

nsInputStreamIO::nsInputStreamIO()
    : mName(nsnull), mContentType(nsnull), mContentLength(-1), mStatus(NS_OK)
{
    NS_INIT_REFCNT();
}

nsInputStreamIO::~nsInputStreamIO()
{
    (void)Close(NS_OK);
    if (mName) nsCRT::free(mName);
    if (mContentType) nsCRT::free(mContentType);
}

NS_METHOD
nsInputStreamIO::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsInputStreamIO* io = new nsInputStreamIO();
    if (io == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(io);
    nsresult rv = io->QueryInterface(aIID, aResult);
    NS_RELEASE(io);
    return rv;
}

NS_IMETHODIMP
nsInputStreamIO::Init(const char* name, nsIInputStream* input,
                      const char* contentType, PRInt32 contentLength)
{
    mName = nsCRT::strdup(name);
    if (mName == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    mInputStream = input;
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
    mContentLength = contentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamIO::Open(char **contentType, PRInt32 *contentLength)
{
    *contentType = nsCRT::strdup(mContentType);
    if (*contentType == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    *contentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamIO::Close(nsresult status)
{
    mStatus = status;
    if (mInputStream)
        return mInputStream->Close();
    else
        return NS_OK;
}

NS_IMETHODIMP
nsInputStreamIO::GetInputStream(nsIInputStream * *aInputStream)
{
    *aInputStream = mInputStream;
    NS_ADDREF(*aInputStream);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamIO::GetOutputStream(nsIOutputStream * *aOutputStream)
{
    // this method should never be called
    NS_NOTREACHED("nsInputStreamIO::GetOutputStream");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsInputStreamIO::GetName(char* *aName)
{
    *aName = nsCRT::strdup(mName);
    return *aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

////////////////////////////////////////////////////////////////////////////////
// nsStreamIOChannel methods:

nsStreamIOChannel::nsStreamIOChannel()
    : mContentType(nsnull), mContentLength(-1),
      mBufferSegmentSize(0), mBufferMaxSize(0),
      mLoadFlags(LOAD_NORMAL), mStatus(NS_OK)
{
    NS_INIT_REFCNT(); 
}

nsStreamIOChannel::~nsStreamIOChannel()
{
    if (mContentType) nsCRT::free(mContentType);
}

NS_METHOD
nsStreamIOChannel::Create(nsISupports *aOuter, REFNSIID aIID,
                             void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsStreamIOChannel* channel = new nsStreamIOChannel();
    if (channel == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(channel);
    nsresult rv = channel->QueryInterface(aIID, aResult);
    NS_RELEASE(channel);
    return rv;
}

NS_IMETHODIMP
nsStreamIOChannel::Init(nsIURI* uri, nsIStreamIO* io)
{
    mURI = uri;
    mStreamIO = io;
    return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsStreamIOChannel)
  NS_INTERFACE_MAP_ENTRY(nsIStreamIOChannel)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIStreamProvider)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIRequestObserver, nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamListener)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsStreamIOChannel)
NS_IMPL_THREADSAFE_RELEASE(nsStreamIOChannel)

NS_IMETHODIMP
nsStreamIOChannel::GetName(PRUnichar* *result)
{
    nsresult rv;
    nsXPIDLCString urlStr;
    rv = mURI->GetSpec(getter_Copies(urlStr));
    if (NS_FAILED(rv)) return rv;
    nsString name;
    name.AppendWithConversion(urlStr);
    *result = ToNewUnicode(name);
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsStreamIOChannel::IsPending(PRBool *result)
{
    if (mRequest)
        return mRequest->IsPending(result);
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    // if we don't have a status error of our own to report
    // then we should propogate the status error of the underlying
    // file transport (if we have one)
    if (NS_SUCCEEDED(mStatus) && mRequest)
      mRequest->GetStatus(status);

    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    mStatus = status;
    if (mRequest)
        return mRequest->Cancel(status);
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::Suspend(void)
{
    if (mRequest)
        return mRequest->Suspend();
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::Resume(void)
{
    if (mRequest)
        return mRequest->Resume();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel implementation:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsStreamIOChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mURI;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::SetOriginalURI(nsIURI* aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mURI;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::Open(nsIInputStream **result)
{
    return mStreamIO->GetInputStream(result);
}

NS_IMETHODIMP
nsStreamIOChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
    nsresult rv;

    NS_ASSERTION(listener, "no listener");
    SetListener(listener);

    if (mLoadGroup) {
        rv = mLoadGroup->AddRequest(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    if (mFileTransport == nsnull) {
        nsCOMPtr<nsIFileTransportService> fts = 
                 do_GetService(kFileTransportServiceCID, &rv);
        if (NS_FAILED(rv)) goto done;

        rv = fts->CreateTransportFromStreamIO(mStreamIO, getter_AddRefs(mFileTransport));
        if (NS_FAILED(rv)) goto done;
    }

    // Hook up the notification callbacks InterfaceRequestor...
    {
        nsCOMPtr<nsIInterfaceRequestor> requestor =
            do_QueryInterface(NS_STATIC_CAST(nsIRequest*, this));
        rv = mFileTransport->SetNotificationCallbacks
                (requestor, (mLoadFlags & nsIRequest::LOAD_BACKGROUND));
        if (NS_FAILED(rv)) goto done;
    }

#if 0
    if (mContentType == nsnull) {
        rv = mStreamIO->Open(&mContentType, &mContentLength);
        if (NS_FAILED(rv)) goto done;
    }
#endif
    rv = mFileTransport->AsyncRead(this, ctxt, 0, PRUint32(-1), 0, getter_AddRefs(mRequest));

  done:
    if (NS_FAILED(rv)) {
        nsresult rv2;
        rv2 = mLoadGroup->RemoveRequest(this, ctxt, rv);
        NS_ASSERTION(NS_SUCCEEDED(rv2), "RemoveRequest failed");
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsStreamIOChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup.get();
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  nsresult rv = NS_OK;
  mCallbacks = aNotificationCallbacks;
  mProgressSink = do_GetInterface(mCallbacks);
  return rv;
}

NS_IMETHODIMP 
nsStreamIOChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::GetContentType(char * *aContentType)
{
    nsresult rv;
    if (mContentType == nsnull) {
        rv = mStreamIO->Open(&mContentType, &mContentLength);
        if (NS_FAILED(rv)) return rv;
    }
    *aContentType = nsCRT::strdup(mContentType);
    if (*aContentType == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::SetContentType(const char *aContentType)
{
    mContentType = nsCRT::strdup(aContentType);
    if (*aContentType == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::GetContentLength(PRInt32 *aContentLength)
{
    nsresult rv;
    if (mContentType == nsnull) {
        rv = mStreamIO->Open(&mContentType, &mContentLength);
        if (NS_FAILED(rv)) return rv;
    }
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::SetContentLength(PRInt32 aContentLength)
{
    mContentLength = aContentLength;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequestObserver implementation:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsStreamIOChannel::OnStartRequest(nsIRequest *request, nsISupports* context)
{
    NS_ASSERTION(mUserObserver, "No listener...");
    return mUserObserver->OnStartRequest(this, context);
}

NS_IMETHODIMP
nsStreamIOChannel::OnStopRequest(nsIRequest *request, nsISupports* context, nsresult aStatus)
{
    if (mUserObserver)
        mUserObserver->OnStopRequest(this, context, aStatus);

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, context, aStatus);

    // Release the reference to the consumer stream listener...
    mRequest = 0;
    mFileTransport = 0;
    mUserObserver = 0;

    // Make sure the stream io is closed
    mStreamIO->Close(aStatus);

    // There is no point in returning anything other than NS_OK
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener implementation:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsStreamIOChannel::OnDataAvailable(nsIRequest *request, nsISupports* context,
                                   nsIInputStream *aIStream, PRUint32 aSourceOffset,
                                   PRUint32 aLength)
{
    return GetListener()->OnDataAvailable(this, context, aIStream,
                                          aSourceOffset, aLength);
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamProvider implementation:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsStreamIOChannel::OnDataWritable(nsIRequest *request, nsISupports *context,
                                  nsIOutputStream *aOStream,
                                  PRUint32 aOffset, PRUint32 aLength)
{
    return GetProvider()->OnDataWritable(this, context, aOStream,
                                         aOffset, aLength);
}

////////////////////////////////////////////////////////////////////////////////
// nsIProgressEventSink implementation:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsStreamIOChannel::OnProgress(nsIRequest *request, nsISupports *context,
                              PRUint32 progress, PRUint32 progressMax)
{
    if (mProgressSink)
        mProgressSink->OnProgress(this, context, progress, progressMax);
    return NS_OK;
}

NS_IMETHODIMP
nsStreamIOChannel::OnStatus(nsIRequest *request, nsISupports *context,
                            nsresult status, const PRUnichar *statusText)
{
    if (mProgressSink)
        mProgressSink->OnStatus(this, context, status, statusText);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIInterfaceRequestor implementation:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsStreamIOChannel::GetInterface(const nsIID &iid, void **result)
{
    if (iid.Equals(NS_GET_IID(nsIProgressEventSink)))
        return QueryInterface(iid, result);

    return NS_ERROR_NO_INTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
