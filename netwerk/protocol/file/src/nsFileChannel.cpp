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

////////////////////////////////////////////////////////////////////////////////

nsFileChannel::nsFileChannel()
{
    NS_INIT_REFCNT();
}

nsresult
nsFileChannel::Init(nsIURI* uri, nsIEventSinkGetter* getter, nsIEventQueue* queue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsFileChannel::~nsFileChannel()
{
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
// From nsIChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::GetURI(nsIURI * *aURI)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::OpenInputStream(PRUint32 startPosition, PRInt32 count,
                               nsIInputStream **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *ctxt,
                         nsIEventQueue *eventQueue,
                         nsIStreamListener *listener)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::AsyncWrite(nsIInputStream *fromStream,
                          PRUint32 startPosition, PRInt32 writeCount,
                          nsISupports *ctxt,
                          nsIEventQueue *eventQueue,
                          nsIStreamObserver *observer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

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
    return NS_ERROR_NOT_IMPLEMENTED;
#if 0
    PRUint32 date;
    nsresult rv = GetModDate(&date);
    LL_I2L(*aModDate, date);
    return rv;
#endif
}

NS_IMETHODIMP
nsFileChannel::GetFileSize(PRUint32 *aFileSize)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::GetParent(nsIFileChannel * *aParent)
{
//    return GetParent((nsIFileSpec**)aParent);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::GetNativePath(char * *aNativePath)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::Exists(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
    nsIFileChannel* FileChannel;
    rv = src->QueryInterface(nsIFileChannel::GetIID(), (void**)&FileChannel);
    if (NS_SUCCEEDED(rv)) {
        rv = FileChannel->moveToDir(this);
        NS_RELEASE(FileChannel);
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
    nsIFileChannel* FileChannel;
    rv = src->QueryInterface(nsIFileChannel::GetIID(), (void**)&FileChannel);
    if (NS_SUCCEEDED(rv)) {
        rv = FileChannel->copyToDir(this);
        NS_RELEASE(FileChannel);
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
nsFileChannel::IsDirectory(PRBool *_retval)
{
    // XXX rename isDirectory to IsDirectory
//    return isDirectory(_retval);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::IsFile(PRBool *_retval)
{
    // XXX rename isFile to IsFile
//    return isFile(_retval);
    return NS_ERROR_NOT_IMPLEMENTED;
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
