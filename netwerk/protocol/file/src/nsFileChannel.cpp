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
      mContext(nsnull), mState(QUIESCENT),
      mSuspended(PR_FALSE), mFileStream(nsnull),
      mBufferInputStream(nsnull), mBufferOutputStream(nsnull),
      mStatus(NS_OK), mHandler(nsnull), mSourceOffset(0)
{
    NS_INIT_REFCNT();
}

nsresult
nsFileChannel::Init(nsFileProtocolHandler* handler,
                    const char* verb, nsIURI* uri, nsIEventSinkGetter* getter,
                    nsIEventQueue* queue)
{
    nsresult rv;

    mHandler = handler;
    NS_ADDREF(mHandler);

    mGetter = getter;
    NS_IF_ADDREF(mGetter);

    mLock = PR_NewLock();    
    if (mLock == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    if (getter) {
        rv = getter->GetEventSink(verb, nsIStreamListener::GetIID(), (nsISupports**)&mListener);
        // ignore the failure -- we can live without having an event sink
    }

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
    NS_IF_ADDREF(mEventQueue);
    
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
    NS_ASSERTION(mFileStream == nsnull, "channel not closed");
    NS_ASSERTION(mBufferInputStream == nsnull, "channel not closed");
    NS_ASSERTION(mBufferOutputStream == nsnull, "channel not closed");
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

class nsAsyncOutputStream : public nsIBufferOutputStream {
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:

    NS_IMETHOD Close() {
        return mOutputStream->Close();
    }

    // nsIOutputStream methods:

    NS_IMETHOD Write(const char *buf, PRUint32 count, PRUint32 *writeCount) {
        nsresult rv;
        rv = mOutputStream->Write(buf, count, writeCount);
        if (NS_FAILED(rv)) return rv;
        rv = mListener->OnDataAvailable(mContext, mInputStream, mOffset, *writeCount);
        mOffset += *writeCount;
        return rv;
    }

    NS_IMETHOD Flush() {
        return mOutputStream->Flush();
    }

    // nsIBufferOutputStream methods:

    NS_IMETHOD GetBuffer(nsIBuffer * *aBuffer) {
        return mOutputStream->GetBuffer(aBuffer);
    }

    NS_IMETHOD WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *writeCount) {
        nsresult rv;
        rv = mOutputStream->WriteFrom(inStr, count, writeCount);
        if (NS_FAILED(rv)) return rv;
        rv = mListener->OnDataAvailable(mContext, mInputStream, mOffset, *writeCount);
        mOffset += *writeCount;
        return rv;
    }

    nsAsyncOutputStream()
        : mContext(nsnull), mListener(nsnull), mInputStream(nsnull),
          mOutputStream(nsnull), mOffset(0)
    {
        NS_INIT_REFCNT();
    }

    nsresult Init(nsISupports* context, nsIStreamListener* listener,
                  PRUint32 growBySize, PRUint32 maxSize) {
        nsresult rv;
        rv = NS_NewPipe(&mInputStream, &mOutputStream,
                        growBySize, maxSize, PR_TRUE, nsnull);
        if (NS_FAILED(rv)) return rv;

        mContext = context;
        NS_IF_ADDREF(mContext);
        mListener = listener;
        NS_ADDREF(mListener);
        return rv;
    }

    virtual ~nsAsyncOutputStream() {
        NS_IF_RELEASE(mContext);
        NS_IF_RELEASE(mListener);
        NS_IF_RELEASE(mInputStream);
        NS_IF_RELEASE(mOutputStream);
    }

    static NS_METHOD Create(nsIBufferInputStream* *inStr,
                            nsIBufferOutputStream* *outStr,
                            nsISupports* context, nsIStreamListener* listener,
                            PRUint32 growBySize, PRUint32 maxSize) {
        nsAsyncOutputStream* str = new nsAsyncOutputStream();
        if (str == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(str);
        nsresult rv = str->Init(context, listener, growBySize, maxSize);
        if (NS_FAILED(rv)) {
            NS_RELEASE(str);
            return rv;
        }
        *inStr = str->mInputStream;
        *outStr = str;
        return NS_OK;
    }

protected:
    nsISupports*                mContext;
    nsIStreamListener*          mListener;
    nsIBufferInputStream*       mInputStream;
    nsIBufferOutputStream*      mOutputStream;
    PRUint32                    mOffset;
};

NS_IMPL_ISUPPORTS(nsAsyncOutputStream, nsIBufferOutputStream::GetIID());

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

    if (mState != QUIESCENT)
        return NS_ERROR_IN_PROGRESS;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewPipe(&mBufferInputStream, &mBufferOutputStream,
                    NS_FILE_TRANSPORT_SEGMENT_SIZE,
                    NS_FILE_TRANSPORT_BUFFER_SIZE, PR_TRUE, nsnull);
//    rv = serv->NewSyncStreamListener(&mBufferInputStream, &mBufferOutputStream, &mListener);
    if (NS_FAILED(rv)) return rv;

    mState = START_READ;
    mSourceOffset = startPosition;
    mAmount = readCount;
    mListener = nsnull;

    rv = mHandler->DispatchRequest(this);
    if (NS_FAILED(rv)) return rv;

    *result = mBufferInputStream;
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **result)
{
    nsAutoLock lock(mLock);

    nsresult rv;

    if (mState != QUIESCENT)
        return NS_ERROR_IN_PROGRESS;

#if 0
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIStreamListener* syncListener;
    nsIBufferInputStream* inStr;
    nsIBufferOutputStream* outStr;
    rv = serv->NewSyncStreamListener(&inStr, &outStr, &syncListener);
    if (NS_FAILED(rv)) return rv;

    mListener = syncListener;
    mOutputStream = outStr;
    mState = START_READ;
    mSourceOffset = startPosition;
    mAmount = readCount;

    rv = mHandler->DispatchRequest(this);
    if (NS_FAILED(rv)) {
        NS_RELEASE(inStr);
        return rv;
    }

    *result = inStr;
#else
    NS_ASSERTION(startPosition == 0, "implement startPosition");
    nsISupports* str;
    rv = NS_NewTypicalOutputFileStream(&str, mSpec);
    if (NS_FAILED(rv)) return rv;
    rv = str->QueryInterface(nsIOutputStream::GetIID(), (void**)result);
    NS_RELEASE(str);
    return rv;

#endif
    return NS_OK;
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

    rv = serv->NewAsyncStreamListener(listener, eventQueue, &mListener);
    if (NS_FAILED(rv)) return rv;

    rv = nsAsyncOutputStream::Create(&mBufferInputStream,
                                     &mBufferOutputStream,
                                     ctxt, mListener, 
                                     NS_FILE_TRANSPORT_SEGMENT_SIZE,
                                     NS_FILE_TRANSPORT_BUFFER_SIZE);
    if (NS_FAILED(rv)) return rv;

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
// nsIRunnable methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::Run(void)
{
    while (mState != QUIESCENT && !mSuspended) {
        Process();
    }
    return NS_OK;
}

static NS_METHOD
nsWriteToFile(void* closure,
              const char* fromRawSegment, 
              PRUint32 toOffset,
              PRUint32 count,
              PRUint32 *writeCount)
{
    nsIOutputStream* outStr = (nsIOutputStream*)closure;
    nsresult rv = outStr->Write(fromRawSegment, count, writeCount);
    return rv;
}

void
nsFileChannel::Process(void)
{
    nsAutoLock lock(mLock);

    switch (mState) {
      case START_READ: {
          nsISupports* fs;
          NS_ASSERTION(mSourceOffset == 0, "implement seek");

          if (mListener) {
              mStatus = mListener->OnStartBinding(mContext);  // always send the start notification
              if (NS_FAILED(mStatus)) goto error;
          }

          mStatus = NS_NewTypicalInputFileStream(&fs, mSpec);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = fs->QueryInterface(nsIInputStream::GetIID(), (void**)&mFileStream);
          NS_RELEASE(fs);
          if (NS_FAILED(mStatus)) goto error;

          mState = READING;
          break;
      }
      case READING: {
          if (NS_FAILED(mStatus)) goto error;

          nsIInputStream* fileStr = NS_STATIC_CAST(nsIInputStream*, mFileStream);

          PRUint32 inLen;
          mStatus = fileStr->GetLength(&inLen);
          if (NS_FAILED(mStatus)) goto error;

          PRUint32 amt;
          mStatus = mBufferOutputStream->WriteFrom(fileStr, inLen, &amt);
          if (NS_FAILED(mStatus)) goto error;

          // and feed the buffer to the application via the buffer stream:
          if (mListener) {
              mStatus = mListener->OnDataAvailable(mContext, mBufferInputStream, mSourceOffset, amt);
              if (NS_FAILED(mStatus)) goto error;
          }
          
          mSourceOffset += amt;

          // stay in the READING state
          break;
      }
      case START_WRITE: {
          nsISupports* fs;

          if (mListener) {
              mStatus = mListener->OnStartBinding(mContext);  // always send the start notification
              if (NS_FAILED(mStatus)) goto error;
          }

          mStatus = NS_NewTypicalOutputFileStream(&fs, mSpec);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = fs->QueryInterface(nsIOutputStream::GetIID(), (void**)&mFileStream);
          NS_RELEASE(fs);
          if (NS_FAILED(mStatus)) goto error;

          mState = WRITING;
          break;
      }
      case WRITING: {
          if (NS_FAILED(mStatus)) goto error;
#if 0
          PRUint32 amt;
          mStatus = mBuffer->ReadSegments(nsWriteToFile, mFileStream, (PRUint32)-1, &amt);
          if (mStatus == NS_BASE_STREAM_EOF) goto error; 
          if (NS_FAILED(mStatus)) goto error;

          nsAutoCMonitor mon(mBuffer);
          mon.Notify();

          mSourceOffset += amt;
#endif
          // stay in the WRITING state
          break;
      }
      case ENDING: {
          mBufferOutputStream->Flush();
          NS_IF_RELEASE(mBufferOutputStream);
          mBufferOutputStream = nsnull;
          NS_IF_RELEASE(mBufferInputStream);
          mBufferInputStream = nsnull;
          NS_IF_RELEASE(mFileStream);
          mFileStream = nsnull;
          NS_IF_RELEASE(mContext);
          mContext = nsnull;

          if (mListener) {
              // XXX where do we get the error message?
              (void)mListener->OnStopBinding(mContext, mStatus, nsnull);
          }

          mState = QUIESCENT;
          break;
      }
      case QUIESCENT: {
          NS_NOTREACHED("trying to continue a quiescent file transfer");
          break;
      }
    }
    return;

  error:
    mState = ENDING;
    return;
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
