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
#include "nsFileProtocolHandler.h"
#include "nsIBuffer.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsAutoLock.h"
#include "netCore.h"
#include "nsIFileStream.h"
#include "nsISimpleEnumerator.h"
#include "nsIURL.h"
#include "prio.h"

NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsFileChannel::nsFileChannel()
    : mURI(nsnull), mGetter(nsnull), mListener(nsnull), mEventQueue(nsnull),
      mContext(nsnull), mState(ENDED),
      mSuspended(PR_FALSE), mFileStream(nsnull), mBuffer(nsnull),
      mBufferStream(nsnull), mStatus(NS_OK), mHandler(nsnull), mSourceOffset(0)
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

    mLock = PR_NewLock();    
    if (mLock == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

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
    NS_IF_RELEASE(mContext);
    NS_IF_RELEASE(mHandler);
    NS_IF_RELEASE(mFileStream);
    NS_IF_RELEASE(mBuffer);
    NS_IF_RELEASE(mBufferStream);
    if (mLock) 
        PR_DestroyLock(mLock);
}

NS_IMETHODIMP
nsFileChannel::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(nsIFileChannel::GetIID()) ||
        aIID.Equals(nsIChannel::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
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
    nsAutoLock lock(mLock);

    nsresult rv = NS_OK;
    mStatus = NS_BINDING_ABORTED;
    if (mSuspended) {
        Resume();
    }
    mState = ENDING;
    return rv;
}

NS_IMETHODIMP
nsFileChannel::Suspend()
{
    nsAutoLock lock(mLock);

    nsresult rv = NS_OK;
    if (!mSuspended) {
        // XXX close the stream here?
        mStatus = mHandler->Suspend(this);
        mSuspended = PR_TRUE;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::Resume()
{
    nsAutoLock lock(mLock);

    nsresult rv = NS_OK;
    if (!mSuspended) {
        // XXX re-open the stream and seek here?
        mStatus = mHandler->Resume(this);
        mSuspended = PR_FALSE;
    }
    return rv;
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
nsFileChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                               nsIInputStream **result)
{
    nsAutoLock lock(mLock);

    nsresult rv;

    if (mState != ENDED)
        return NS_ERROR_IN_PROGRESS;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIStreamListener* syncListener;
    nsIBufferInputStream* inStr;
    rv = serv->NewSyncStreamListener(&inStr, &syncListener);
    if (NS_FAILED(rv)) return rv;

    mListener = syncListener;
    mState = START_READ;
    mSourceOffset = startPosition;
    mAmount = readCount;

    rv = mHandler->DispatchRequest(this);
    if (NS_FAILED(rv)) {
        NS_RELEASE(inStr);
        return rv;
    }

    *result = inStr;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    nsAutoLock lock(mLock);

    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *ctxt,
                         nsIEventQueue *eventQueue,
                         nsIStreamListener *listener)
{
    nsAutoLock lock(mLock);

    nsresult rv;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIStreamListener* asyncListener;
    rv = serv->NewAsyncStreamListener(listener, eventQueue, &asyncListener);
    if (NS_FAILED(rv)) return rv;

    mListener = asyncListener;

    mContext = ctxt;
    NS_IF_ADDREF(mContext);

    mState = START_READ;
    mSourceOffset = startPosition;
    mAmount = readCount;

    rv = mHandler->DispatchRequest(this);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::AsyncWrite(nsIInputStream *fromStream,
                          PRUint32 startPosition, PRInt32 writeCount,
                          nsISupports *ctxt,
                          nsIEventQueue *eventQueue,
                          nsIStreamObserver *observer)
{
    nsAutoLock lock(mLock);

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

class nsDirEnumerator : public nsISimpleEnumerator
{
public: 
    NS_DECL_ISUPPORTS
 
    nsDirEnumerator() : mHandler(nsnull), mDir(nsnull), mNext(nsnull) {
        NS_INIT_REFCNT();
    }

    nsresult Init(nsFileProtocolHandler* handler, nsFileSpec& spec) {
        const char* path = spec.GetNativePathCString();
        mDir = PR_OpenDir(path);
        if (mDir == nsnull)    // not a directory?
            return NS_ERROR_FAILURE;

        mHandler = handler;
        NS_ADDREF(mHandler);
        return NS_OK;
    }

    NS_IMETHOD HasMoreElements(PRBool *result) {
        nsresult rv;
        if (mNext == nsnull && mDir) {
            PRDirEntry* entry = PR_ReadDir(mDir, PR_SKIP_BOTH);
            if (entry == nsnull) {
                // end of dir entries

                PRStatus status = PR_CloseDir(mDir);
                if (status != PR_SUCCESS) 
                    return NS_ERROR_FAILURE;
                mDir = nsnull;

                *result = PR_FALSE;
                return NS_OK;
            }

            const char* path = entry->name;
            rv = mHandler->NewChannelFromNativePath(path, &mNext);
            if (NS_FAILED(rv)) return rv;

            NS_ASSERTION(mNext, "NewChannel failed");
        }            
        *result = mNext != nsnull;
        return NS_OK;
    }

    NS_IMETHOD GetNext(nsISupports **result) {
        nsresult rv;
        PRBool hasMore;
        rv = HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;

        *result = mNext;        // might return nsnull
        mNext = nsnull;
        return NS_OK;
    }

    virtual ~nsDirEnumerator() {
        if (mDir) {
            PRStatus status = PR_CloseDir(mDir);
            NS_ASSERTION(status == PR_SUCCESS, "close failed");
        }
        NS_IF_RELEASE(mHandler);
        NS_IF_RELEASE(mNext);
    }

protected:
    nsFileProtocolHandler*      mHandler;
    PRDir*                      mDir;
    nsIFileChannel*             mNext;
};

NS_IMPL_ISUPPORTS(nsDirEnumerator, nsISimpleEnumerator::GetIID());

NS_IMETHODIMP
nsFileChannel::GetChildren(nsISimpleEnumerator * *aChildren)
{
    nsresult rv;

    PRBool isDir;    
    rv = IsDirectory(&isDir);
    if (NS_FAILED(rv)) return rv;
    if (!isDir)
        return NS_ERROR_FAILURE;

    nsDirEnumerator* dirEnum = new nsDirEnumerator();
    if (dirEnum == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(dirEnum);
    rv = dirEnum->Init(mHandler, mSpec);
    if (NS_FAILED(rv)) {
        NS_RELEASE(dirEnum);
        return rv;
    }
    *aChildren = dirEnum;
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

NS_IMETHODIMP
nsFileChannel::Execute(const char *args)
{
    nsresult rv;
    char* queryArgs = nsnull;
    
    if (args == nsnull) {
        nsIURL* url;
        rv = mURI->QueryInterface(nsIURL::GetIID(), (void**)&url);
        if (NS_SUCCEEDED(rv)) {
            rv = url->GetQuery(&queryArgs);
            NS_RELEASE(url);
            if (NS_FAILED(rv)) return rv;
            args = queryArgs;
        }
    }
    
    rv = mSpec.Execute(args);
    if (queryArgs)
        nsCRT::free(queryArgs);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRunnable methods:

NS_IMETHODIMP
nsFileChannel::Run(void)
{
    while (mState != ENDED && !mSuspended) {
        Process();
    }
    return NS_OK;
}

void
nsFileChannel::Process(void)
{
    nsAutoLock lock(mLock);

    switch (mState) {
      case START_READ: {
          nsISupports* fs;

          mStatus = mListener->OnStartBinding(mContext);  // always send the start notification
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewTypicalInputFileStream(&fs, mSpec);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = fs->QueryInterface(nsIInputStream::GetIID(), (void**)&mFileStream);
          NS_RELEASE(fs);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewBuffer(&mBuffer, NS_FILE_TRANSPORT_BUFFER_SIZE,
                                 NS_FILE_TRANSPORT_BUFFER_SIZE);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewBufferInputStream(&mBufferStream, mBuffer, PR_FALSE);
          if (NS_FAILED(mStatus)) goto error;

          mState = READING;
          break;
      }
      case READING: {
          if (NS_FAILED(mStatus)) goto error;

          PRUint32 amt;
          nsIInputStream* inStr = NS_STATIC_CAST(nsIInputStream*, mFileStream);
          PRUint32 inLen;
          mStatus = inStr->GetLength(&inLen);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = mBuffer->WriteFrom(inStr, inLen, &amt);
          if (mStatus == NS_BASE_STREAM_EOF) goto error; 
          if (NS_FAILED(mStatus)) goto error;

          // and feed the buffer to the application via the byte buffer stream:
          // XXX maybe amt should be mBufferStream->GetLength():
          mStatus = mListener->OnDataAvailable(mContext, mBufferStream, mSourceOffset, amt);
          if (NS_FAILED(mStatus)) goto error;
          
          mSourceOffset += amt;

          // stay in the READING state
          break;
      }
      case START_WRITE: {
          nsISupports* fs;

          mStatus = mListener->OnStartBinding(mContext);  // always send the start notification
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewTypicalOutputFileStream(&fs, mSpec);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = fs->QueryInterface(nsIOutputStream::GetIID(), (void**)&mFileStream);
          NS_RELEASE(fs);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewBuffer(&mBuffer, NS_FILE_TRANSPORT_BUFFER_SIZE,
                                 NS_FILE_TRANSPORT_BUFFER_SIZE);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewBufferInputStream(&mBufferStream, mBuffer, PR_FALSE);
          if (NS_FAILED(mStatus)) goto error;

          mState = WRITING;
          break;
      }
      case WRITING: {
          break;
      }
      case ENDING: {
          NS_IF_RELEASE(mBufferStream);
          mBufferStream = nsnull;
          NS_IF_RELEASE(mFileStream);
          mFileStream = nsnull;
          NS_IF_RELEASE(mContext);
          mContext = nsnull;

          // XXX where do we get the error message?
          (void)mListener->OnStopBinding(mContext, mStatus, nsnull);

          mState = ENDED;
          break;
      }
      case ENDED: {
          NS_NOTREACHED("trying to continue an ended file transfer");
          break;
      }
    }
    return;

  error:
    mState = ENDING;
    return;
}

////////////////////////////////////////////////////////////////////////////////
