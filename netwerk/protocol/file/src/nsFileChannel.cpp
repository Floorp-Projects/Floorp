/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#include "nsFileChannel.h"
#include "nscore.h"
#include "nsIEventSinkGetter.h"
#include "nsIURI.h"
#include "nsIEventQueue.h"
#include "nsIStreamListener.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"

NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsFileChannel::nsFileChannel()
    : mURI(nsnull), mGetter(nsnull), mListener(nsnull), mEventQueue(nsnull)
{
    NS_INIT_REFCNT();
}

nsresult
nsFileChannel::Init(const char* verb, nsIURI* uri, nsIEventSinkGetter* getter,
                    nsIEventQueue* queue)
{
    nsresult rv;

    mGetter = getter;
    NS_ADDREF(mGetter);

    rv = getter->GetEventSink(verb, nsIStreamListener::GetIID(), (nsISupports**)&mListener);
    if (NS_FAILED(rv)) return rv;

    mURI = uri;
    NS_ADDREF(mURI);

    // XXX temporary, until we integrate more thoroughly with nsFileSpec
    char* url;
    rv = mURI->GetSpec(&url);
    if (NS_FAILED(rv)) return rv;
    nsFileURL fileURL(url);
    nsCRT::free(url);
    mSpec = fileURL;

    mEventQueue = queue;
    NS_ADDREF(mEventQueue);
    
    return NS_OK;
}

nsFileChannel::~nsFileChannel()
{
    NS_IF_RELEASE(mURI);
    NS_IF_RELEASE(mGetter);
    NS_IF_RELEASE(mListener);
    NS_IF_RELEASE(mEventQueue);
}

NS_IMETHODIMP
nsFileChannel::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(nsIFileChannel::GetIID()) ||
        aIID.Equals(nsIChannel::GetIID()) ||
        aIID.Equals(nsISupports::GetIID()) ) {
        *aInstancePtr = NS_STATIC_CAST(nsIFileChannel*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

NS_IMPL_ADDREF(nsFileChannel);
NS_IMPL_RELEASE(nsFileChannel);

NS_METHOD
nsFileChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsFileChannel* fc = new nsFileChannel();
    if (fc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(fc);
    nsresult rv = fc->QueryInterface(aIID, aResult);
    NS_RELEASE(fc);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIRequest
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::Cancel()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::Suspend()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::Resume()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::GetURI(nsIURI * *aURI)
{
    *aURI = mURI;
    NS_ADDREF(mURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::OpenInputStream(PRUint32 startPosition, PRInt32 count,
                               nsIInputStream **_retval)
{
    NS_ASSERTION(startPosition == 0 && count == -1, "fix me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    NS_ASSERTION(startPosition == 0, "fix me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *ctxt,
                         nsIEventQueue *eventQueue,
                         nsIStreamListener *listener)
{
    NS_ASSERTION(startPosition == 0 && readCount == -1, "fix me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::AsyncWrite(nsIInputStream *fromStream,
                          PRUint32 startPosition, PRInt32 writeCount,
                          nsISupports *ctxt,
                          nsIEventQueue *eventQueue,
                          nsIStreamObserver *observer)
{
    NS_ASSERTION(startPosition == 0, "fix me");
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIFileChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::GetCreationDate(PRTime *aCreationDate)
{
    // XXX no GetCreationDate in nsFileSpec yet
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::GetModDate(PRTime *aModDate)
{
    nsFileSpec::TimeStamp date;
    mSpec.GetModDate(date);
    LL_I2L(*aModDate, date);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetFileSize(PRUint32 *aFileSize)
{
    *aFileSize = mSpec.GetFileSize();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetParent(nsIFileChannel * *aParent)
{
    nsresult rv;

    nsFileSpec parentSpec;
    mSpec.GetParent(parentSpec);
    nsFileURL parentURL(parentSpec);
    const char* urlStr = parentURL.GetURLString();

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsIChannel* channel;
    rv = serv->NewChannel("load",    // XXX what should this be?
                          urlStr, nsnull,
                          mGetter, &channel);
    if (NS_FAILED(rv)) return rv;

    // this cast is safe because nsFileURL::GetURLString aways
    // returns file: strings, and consequently we'll make nsIFileChannel
    // objects from them:
    *aParent = NS_STATIC_CAST(nsIFileChannel*, channel);

    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetNativePath(char * *aNativePath)
{
    char* nativePath = nsCRT::strdup(mSpec.GetNativePathCString());
    if (nativePath == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    *aNativePath = nativePath;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Exists(PRBool *result)
{
    *result = mSpec.Exists();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Create()
{
    // XXX no Create in nsFileSpec -- creates non-existent file
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::Delete()
{
    // XXX no Delete in nsFileSpec -- deletes file or dir
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::MoveFrom(nsIURI *src)
{
#if 0
    nsresult rv;
    nsIFileChannel* fc;
    rv = src->QueryInterface(nsIFileChannel::GetIID(), (void**)&fc);
    if (NS_SUCCEEDED(rv)) {
        rv = fc->moveToDir(this);
        NS_RELEASE(fc);
        return rv;
    }
    else {
        // Do it the hard way -- fetch the URL and store the bits locally.
        // Delete the src when done.
        return NS_ERROR_NOT_IMPLEMENTED;
    }
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsFileChannel::CopyFrom(nsIURI *src)
{
#if 0
    nsresult rv;
    nsIFileChannel* fc;
    rv = src->QueryInterface(nsIFileChannel::GetIID(), (void**)&fc);
    if (NS_SUCCEEDED(rv)) {
        rv = fc->copyToDir(this);
        NS_RELEASE(fc);
        return rv;
    }
    else {
        // Do it the hard way -- fetch the URL and store the bits locally.
        return NS_ERROR_NOT_IMPLEMENTED;
    }
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsFileChannel::IsDirectory(PRBool *result)
{
    *result = mSpec.IsDirectory();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::IsFile(PRBool *result)
{
    *result = mSpec.IsFile();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::IsLink(PRBool *_retval)
{
    // XXX no IsLink in nsFileSpec (for alias/shortcut/symlink)
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::ResolveLink(nsIFileChannel **_retval)
{
    // XXX no ResolveLink in nsFileSpec yet -- returns what link points to
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::MakeUniqueFileName(const char* baseName, char **_retval)
{
    // XXX makeUnique needs to return the name or file spec to the newly create
    // file!
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
