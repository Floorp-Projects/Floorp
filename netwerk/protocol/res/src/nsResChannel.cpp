/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsResChannel.h"
#include "nsCRT.h"
#include "netCore.h"
#include "nsIServiceManager.h"
#include "nsIFileTransportService.h"
#include "nsIMIMEService.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsResChannel::nsResChannel()
    : mLoadAttributes(LOAD_NORMAL),
      mLock(nsnull),
      mState(QUIESCENT)
{
    NS_INIT_REFCNT();
}

nsresult
nsResChannel::Init(nsIResProtocolHandler* handler,
                   const char* command,
                   nsIURI* uri,
                   nsILoadGroup* aLoadGroup,
                   nsIInterfaceRequestor* notificationCallbacks,
                   nsLoadFlags loadAttributes,
                   nsIURI* originalURI,
                   PRUint32 bufferSegmentSize,
                   PRUint32 bufferMaxSize)
{
    nsresult rv;

    mLock = PR_NewLock();
    if (mLock == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    mBufferSegmentSize = bufferSegmentSize;
    mBufferMaxSize = bufferMaxSize;
    mResolvedURI = uri;
    mHandler = handler;
    mOriginalURI = originalURI ? originalURI : uri;
    mResourceURI = uri;
    mCommand = nsCRT::strdup(command);
    if (mCommand == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = SetLoadAttributes(loadAttributes);
    if (NS_FAILED(rv)) return rv;

    rv = SetLoadGroup(aLoadGroup);
    if (NS_FAILED(rv)) return rv;

    rv = SetNotificationCallbacks(notificationCallbacks);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

nsResChannel::~nsResChannel()
{
    if (mCommand) nsCRT::free(mCommand);
    if (mLock) PR_DestroyLock(mLock);
}

NS_IMPL_ISUPPORTS5(nsResChannel,
                   nsIResChannel,
                   nsIChannel,
                   nsIRequest,
                   nsIStreamListener,
                   nsIStreamObserver)

NS_METHOD
nsResChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsResChannel* fc = new nsResChannel();
    if (fc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(fc);
    nsresult rv = fc->QueryInterface(aIID, aResult);
    NS_RELEASE(fc);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsResChannel::Substitutions
////////////////////////////////////////////////////////////////////////////////

nsresult
nsResChannel::Substitutions::Init()
{
    nsresult rv;
    nsResChannel* channel = GET_SUBSTITUTIONS_CHANNEL(this);
    
    NS_ASSERTION(mSubstitutions == nsnull, "failed to call destructor");

    char* root;
    rv = channel->mResourceURI->GetHost(&root);
    if (NS_SUCCEEDED(rv)) {
        rv = channel->mHandler->GetSubstitutions(root, &mSubstitutions);
        nsCRT::free(root);
    }
    return rv;
}

nsresult
nsResChannel::Substitutions::Next(nsIURI* *result)
{
    nsresult rv;
    nsResChannel* channel = GET_SUBSTITUTIONS_CHANNEL(this);

    nsCString* str = (nsCString*)mSubstitutions->CStringAt(0);
    if (str == nsnull) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIURI> resolvedURI;
    rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                            NS_GET_IID(nsIURI),
                                            getter_AddRefs(resolvedURI));
    if (NS_FAILED(rv)) return rv;

    rv = resolvedURI->SetSpec(str->GetBuffer());
    if (NS_FAILED(rv)) return rv;

    PRBool ok = mSubstitutions->RemoveCStringAt(0);
    if (!ok) return NS_ERROR_FAILURE;

    char* path;
    rv = channel->mResourceURI->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    // XXX this path[0] check is a hack -- it seems to me that GetPath 
    // shouldn't include the leading slash:
    rv = resolvedURI->SetRelativePath(path[0] == '/' ? path+1 : path);
    nsCRT::free(path);
    if (NS_FAILED(rv)) return rv;
    
    *result = resolvedURI;
    NS_IF_ADDREF(*result);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsResChannel::IsPending(PRBool *result)
{
    nsAutoLock lock(mLock);

    if (mResolvedChannel)
        return mResolvedChannel->IsPending(result);
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::Cancel()
{
    nsAutoLock lock(mLock);

    mResolvedURI = mResourceURI;        // remove the resolution
    if (mResolvedChannel)
        return mResolvedChannel->Cancel();
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::Suspend()
{
    nsAutoLock lock(mLock);

    if (mResolvedChannel)
        return mResolvedChannel->Suspend();
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::Resume()
{
    nsAutoLock lock(mLock);

    if (mResolvedChannel)
        return mResolvedChannel->Resume();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsResChannel::GetOriginalURI(nsIURI * *aURI)
{
    *aURI = mOriginalURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::GetURI(nsIURI * *aURI)
{
    if (mResolvedURI == nsnull)
        mResolvedURI = mResourceURI;
    *aURI = mResolvedURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                              nsIInputStream **result)
{
    nsresult rv;

    nsAutoLock lock(mLock);

    if (mResolvedChannel)
        return NS_ERROR_IN_PROGRESS;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = mSubstitutions.Init();
    if (NS_FAILED(rv)) return rv;

    do {
        rv = mSubstitutions.Next(getter_AddRefs(mResolvedURI));
        if (NS_FAILED(rv)) return rv;

        rv = serv->NewChannelFromURI(mCommand, mResolvedURI, mLoadGroup, mCallbacks, 
                                     mLoadAttributes, mOriginalURI,
                                     mBufferSegmentSize, mBufferMaxSize,
                                     getter_AddRefs(mResolvedChannel));
        if (NS_FAILED(rv)) continue;

        rv = mResolvedChannel->OpenInputStream(startPosition, readCount, result);
    } while (NS_FAILED(rv));

    return rv;
}

NS_IMETHODIMP
nsResChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **result)
{
    nsresult rv;

    nsAutoLock lock(mLock);

    if (mResolvedChannel)
        return NS_ERROR_IN_PROGRESS;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = mSubstitutions.Init();
    if (NS_FAILED(rv)) return rv;

    do {
        rv = mSubstitutions.Next(getter_AddRefs(mResolvedURI));
        if (NS_FAILED(rv)) return rv;

        rv = serv->NewChannelFromURI(mCommand, mResolvedURI, mLoadGroup, mCallbacks, 
                                     mLoadAttributes, mOriginalURI,
                                     mBufferSegmentSize, mBufferMaxSize,
                                     getter_AddRefs(mResolvedChannel));
        if (NS_FAILED(rv)) continue;

        rv = mResolvedChannel->OpenOutputStream(startPosition, result);
    } while (NS_FAILED(rv));

    return rv;
}

NS_IMETHODIMP
nsResChannel::AsyncOpen(nsIStreamObserver *observer, nsISupports* ctxt)
{
    nsresult rv;

    nsAutoLock lock(mLock);

    switch (mState) {
      case QUIESCENT:
        if (mResolvedChannel)
            return NS_ERROR_IN_PROGRESS;

        // first time through
        rv = mSubstitutions.Init();
        if (NS_FAILED(rv)) return rv;
        mState = ASYNC_OPEN;
        break;

      case ASYNC_OPEN:
        break;
      default:
        return NS_ERROR_IN_PROGRESS;
    }
    NS_ASSERTION(mState == ASYNC_OPEN, "wrong state");

    mUserObserver = observer;
    mUserContext = ctxt;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    do {
        rv = mSubstitutions.Next(getter_AddRefs(mResolvedURI));
        if (NS_FAILED(rv)) break;

        rv = serv->NewChannelFromURI(mCommand, mResolvedURI, mLoadGroup, mCallbacks, 
                                     mLoadAttributes, mOriginalURI,
                                     mBufferSegmentSize, mBufferMaxSize,
                                     getter_AddRefs(mResolvedChannel));
        if (NS_FAILED(rv)) continue;

        rv = mResolvedChannel->AsyncOpen(this, nsnull);
        // Later, this AsyncOpen will call back our OnStopRequest
        // method. The action resumes there...
    } while (NS_FAILED(rv));

    if (NS_FAILED(rv)) {
        (void)observer->OnStopRequest(this, ctxt, rv, nsnull);  // XXX need error message
    }

    return rv;
}

NS_IMETHODIMP
nsResChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                        nsISupports *ctxt,
                        nsIStreamListener *listener)
{
    nsresult rv;

    nsAutoLock lock(mLock);

    switch (mState) {
      case QUIESCENT:
        if (mResolvedChannel)
            return NS_ERROR_IN_PROGRESS;

        // first time through
        rv = mSubstitutions.Init();
        if (NS_FAILED(rv)) return rv;
        // fall through

      case ASYNC_OPEN:
        mState = ASYNC_READ;
        // fall through

      case ASYNC_READ:
        break;

      default:
        return NS_ERROR_IN_PROGRESS;
    }
    NS_ASSERTION(mState == ASYNC_READ, "wrong state");

    mStartPosition = startPosition;
    mCount = readCount;
    mUserContext = ctxt;
    mUserObserver = listener;   // cast back to nsIStreamListener later

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    do {
        rv = mSubstitutions.Next(getter_AddRefs(mResolvedURI));
        if (NS_FAILED(rv)) break;

        rv = serv->NewChannelFromURI(mCommand, mResolvedURI, mLoadGroup, mCallbacks, 
                                     mLoadAttributes, mOriginalURI,
                                     mBufferSegmentSize, mBufferMaxSize,
                                     getter_AddRefs(mResolvedChannel));
        if (NS_FAILED(rv)) continue;

        rv = mResolvedChannel->AsyncRead(startPosition, readCount, nsnull, this);
        // Later, this AsyncRead will call back our OnStopRequest
        // method. The action resumes there...
    } while (NS_FAILED(rv));

    if (NS_FAILED(rv)) {
        (void)mUserObserver->OnStopRequest(this, mUserContext, rv, nsnull);  // XXX need error message
    }

    return rv;
}

NS_IMETHODIMP
nsResChannel::AsyncWrite(nsIInputStream *fromStream,
                         PRUint32 startPosition, PRInt32 writeCount,
                         nsISupports *ctxt,
                         nsIStreamObserver *observer)
{
    nsresult rv;

    nsAutoLock lock(mLock);

    switch (mState) {
      case QUIESCENT:
        if (mResolvedChannel)
            return NS_ERROR_IN_PROGRESS;

        // first time through
        rv = mSubstitutions.Init();
        if (NS_FAILED(rv)) return rv;
        // fall through

      case ASYNC_OPEN:
        mState = ASYNC_READ;
        // fall through

      case ASYNC_READ:
        break;

      default:
        return NS_ERROR_IN_PROGRESS;
    }
    NS_ASSERTION(mState == ASYNC_READ, "wrong state");

    mFromStream = fromStream;
    mStartPosition = startPosition;
    mCount = writeCount;
    mUserContext = ctxt;
    mUserObserver = observer;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    do {
        rv = mSubstitutions.Next(getter_AddRefs(mResolvedURI));
        if (NS_FAILED(rv)) break;

        rv = serv->NewChannelFromURI(mCommand, mResolvedURI, mLoadGroup, mCallbacks, 
                                     mLoadAttributes, mOriginalURI,
                                     mBufferSegmentSize, mBufferMaxSize,
                                     getter_AddRefs(mResolvedChannel));
        if (NS_FAILED(rv)) continue;

        rv = mResolvedChannel->AsyncWrite(fromStream, startPosition, writeCount, 
                                          nsnull, this);
        // Later, this AsyncWrite will call back our OnStopRequest
        // method. The action resumes there...
    } while (NS_FAILED(rv));

    if (NS_FAILED(rv)) {
        (void)mUserObserver->OnStopRequest(this, mUserContext, rv, nsnull);  // XXX need error message
    }

    return rv;
}

NS_IMETHODIMP
nsResChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::GetContentType(char * *aContentType)
{
    if (mResolvedChannel)
        return mResolvedChannel->GetContentType(aContentType);
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsResChannel::SetContentType(const char *aContentType)
{
    if (mResolvedChannel)
        return mResolvedChannel->SetContentType(aContentType);
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsResChannel::GetContentLength(PRInt32 *aContentLength)
{
    if (mResolvedChannel)
        return mResolvedChannel->GetContentLength(aContentLength);
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsResChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    nsAutoLock lock(mLock);

    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsResChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsResChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP 
nsResChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsResChannel::OnStartRequest(nsIChannel* transportChannel, nsISupports* context)
{
    NS_ASSERTION(mUserObserver, "No observer...");
    return mUserObserver->OnStartRequest(this, mUserContext);
}

NS_IMETHODIMP
nsResChannel::OnStopRequest(nsIChannel* transportChannel, nsISupports* context,
                            nsresult aStatus, const PRUnichar* aMsg)
{
    nsresult rv;

    if (NS_FAILED(aStatus) && aStatus != NS_BINDING_ABORTED) {
        // if we failed to process this channel, then try the next one:
        switch (mState) {
          case ASYNC_OPEN:
            return AsyncOpen(mUserObserver, mUserContext);
          case ASYNC_READ: 
            return AsyncRead(mStartPosition, mCount, mUserContext, 
                             GetUserListener());
          case ASYNC_WRITE:
            return AsyncWrite(mFromStream, mStartPosition, mCount,
                              mUserContext, mUserObserver);
        }
    }

    rv = mUserObserver->OnStopRequest(this, mUserContext, aStatus, aMsg);
#if 0 // we don't add the resource channel to the group (although maybe we should)
    if (mLoadGroup) {
        if (NS_SUCCEEDED(rv)) {
            mLoadGroup->RemoveChannel(this, context, aStatus, aMsg);
        }
    }
#endif
    // Release the reference to the consumer stream listener...
    mUserObserver = null_nsCOMPtr();
    mUserContext = null_nsCOMPtr();
    mResolvedChannel = null_nsCOMPtr();
    mFromStream = null_nsCOMPtr();
    return rv;
}

NS_IMETHODIMP
nsResChannel::OnDataAvailable(nsIChannel* transportChannel, nsISupports* context,
                               nsIInputStream *aIStream, PRUint32 aSourceOffset,
                               PRUint32 aLength)
{
    return GetUserListener()->OnDataAvailable(this, mUserContext, aIStream,
                                              aSourceOffset, aLength);
}

////////////////////////////////////////////////////////////////////////////////
// From nsIResChannel
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
