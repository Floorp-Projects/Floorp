/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is nsStorageTransport.cpp, released February 26, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include "nsStorageTransport.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "prmem.h"
#include "netCore.h"

#define MAX_IO_CHUNK 8192 // maximum count reported per OnDataAvailable
#define MAX_COUNT ((PRUint32) -1)

static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

//----------------------------------------------------------------------------
// helper functions...
//----------------------------------------------------------------------------

static NS_METHOD
nsWriteToBuffer(nsIInputStream *aInput,
                void *aClosure,
                const char *aFromBuf,
                PRUint32 aOffset,
                PRUint32 aCount,
                PRUint32 *aWriteCount)
{
    char *toBuf = NS_REINTERPRET_CAST(char *, aClosure);
    nsCRT::memcpy(toBuf + aOffset, aFromBuf, aCount);
    *aWriteCount = aCount;
    return NS_OK;
}

static NS_METHOD
nsReadFromBuffer(nsIOutputStream *aOutput,
                 void *aClosure,
                 char *aToBuf,
                 PRUint32 aOffset,
                 PRUint32 aCount,
                 PRUint32 *aReadCount)
{
    const char *fromBuf = NS_REINTERPRET_CAST(const char *, aClosure);
    nsCRT::memcpy(aToBuf, fromBuf + aOffset, aCount);
    *aReadCount = aCount;
    return NS_OK;
}

static NS_METHOD
nsReadFromInputStream(nsIOutputStream *aOutput,
                      void *aClosure,
                      char *aToBuf,
                      PRUint32 aOffset,
                      PRUint32 aCount,
                      PRUint32 *aReadCount)
{
    nsIInputStream *fromStream = NS_REINTERPRET_CAST(nsIInputStream *, aClosure);
    return fromStream->Read(aToBuf, aCount, aReadCount);
}

//----------------------------------------------------------------------------
// nsStorageTransport
//----------------------------------------------------------------------------

nsStorageTransport::nsStorageTransport()
    : mOutputStream(nsnull)
    , mSegmentSize(DEFAULT_SEGMENT_SIZE)
    , mMaxSize(DEFAULT_BUFFER_SIZE)
    , mSegments(nsnull)
    , mWriteSegment(nsnull)
    , mWriteCursor(0)
{
    NS_INIT_ISUPPORTS();

    PR_INIT_CLIST(&mReadRequests);
    PR_INIT_CLIST(&mInputStreams);
}

nsStorageTransport::~nsStorageTransport()
{
    if (mOutputStream)
        CloseOutputStream();

    DeleteAllSegments();
}

nsresult
nsStorageTransport::Init(PRUint32 aBufSegmentSize, PRUint32 aBufMaxSize)
{
    mSegmentSize = aBufSegmentSize;
    mMaxSize = aBufMaxSize;
    return NS_OK;
}

nsresult
nsStorageTransport::GetReadSegment(PRUint32 aOffset, char **aPtr, PRUint32 *aCount)
{
    *aPtr = nsnull;
    *aCount = 0;

    if (aOffset < mWriteCursor) {
        PRUint32 index = aOffset / mSegmentSize;
        nsSegment *s = GetNthSegment(index);
        if (s) {
            PRUint32 offset = aOffset % mSegmentSize;
            *aPtr = s->Data() + offset;
            *aCount = mSegmentSize - offset;
        }
    }
    return NS_OK;
}

nsresult
nsStorageTransport::GetWriteSegment(char **aPtr, PRUint32 *aCount)
{
    NS_ENSURE_ARG_POINTER(aPtr);
    NS_ENSURE_ARG_POINTER(aCount);

    if (mWriteSegment) {
        PRUint32 offset = mWriteCursor % mSegmentSize;
        *aPtr = mWriteSegment->Data() + offset;
        *aCount = mSegmentSize - offset;
        return NS_OK;
    }
    else {
        // add a new segment for writing and redo...
        nsresult rv = AddWriteSegment();
        return NS_FAILED(rv) ? rv : GetWriteSegment(aPtr, aCount);
    }
}

nsresult
nsStorageTransport::AddToBytesWritten(PRUint32 aCount)
{
    // advance write cursor
    mWriteCursor += aCount;

    // clear write segment if we have written to the end
    if (!(mWriteCursor % mSegmentSize))
        mWriteSegment = nsnull;

    // process waiting readers
    PRCList *link = PR_LIST_HEAD(&mReadRequests);
    for (; link != &mReadRequests; link = PR_NEXT_LINK(link)) {
        nsReadRequest *req = NS_STATIC_CAST(nsReadRequest *, link);
        if (req->IsWaitingForWrite())
            req->Process();
    }

    return NS_OK;
}

nsresult
nsStorageTransport::CloseOutputStream()
{
    if (mOutputStream) {
        mOutputStream->SetTransport(nsnull);
        mOutputStream = nsnull;

        // XXX wake up blocked reads
    }
    return NS_OK;
}

nsresult
nsStorageTransport::ReadRequestCompleted(nsReadRequest *aReader)
{
    // remove the reader from the list of readers
    PR_REMOVE_AND_INIT_LINK(aReader);
    aReader->SetTransport(nsnull);
    return NS_OK;
}

nsresult
nsStorageTransport::Available(PRUint32 aStartingFrom, PRUint32 *aCount)
{
    *aCount = mWriteCursor - aStartingFrom;
    return NS_OK;
}

void
nsStorageTransport::FireOnProgress(nsIRequest *aRequest,
                                   nsISupports *aContext,
                                   PRUint32 aByteOffset)
{
    if (mProgressSink)
      mProgressSink->OnProgress(aRequest, aContext, aByteOffset, mWriteCursor);
}

nsresult
nsStorageTransport::AddWriteSegment()
{
    NS_ASSERTION(mWriteSegment == nsnull, "write segment is non-null");

    mWriteSegment = (nsSegment *) PR_Malloc(sizeof(nsSegment) + mSegmentSize);
    if (!mWriteSegment)
        return NS_ERROR_OUT_OF_MEMORY;

    mWriteSegment->next = nsnull;

    AppendSegment(mWriteSegment);
    return NS_OK;
}

void
nsStorageTransport::AppendSegment(nsSegment *aSegment)
{
    if (!mSegments)
        mSegments = aSegment;
    else {
        nsSegment *s = mSegments;
        for (; s && s->next; s = s->next);
        s->next = aSegment;
    }
}

void
nsStorageTransport::DeleteSegments(nsSegment *segments)
{
    while (segments) {
        nsSegment *s = segments->next;
        PR_Free(segments);
        segments = s;
    }
}

void
nsStorageTransport::TruncateTo(PRUint32 aOffset)
{
    if (aOffset < mWriteCursor) {
        if (aOffset == 0) {
            DeleteSegments(mSegments);
            mSegments = nsnull;
            mWriteSegment = nsnull;
        }
        else {
            PRUint32 offset = 0;
            nsSegment *s = mSegments;
            for (; s; s = s->next) {
                if ((offset + mSegmentSize) > aOffset)
                    break;
                offset += mSegmentSize;
            }
            // "s" now points to the last segment that we should keep
            if (s->next) {
                DeleteSegments(s->next);
                s->next = nsnull;
            }
            mWriteSegment = s;
        }
    }
    mWriteCursor = aOffset;
}

nsStorageTransport::nsSegment *
nsStorageTransport::GetNthSegment(PRUint32 index)
{
    nsSegment *s = mSegments;
    for (; s && index; s = s->next, --index);
    return s;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStorageTransport,
                              nsITransport)

NS_IMETHODIMP
nsStorageTransport::GetSecurityInfo(nsISupports **aSecurityInfo)
{
    NS_ENSURE_ARG_POINTER(aSecurityInfo);
    *aSecurityInfo = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::GetNotificationCallbacks(nsIInterfaceRequestor** aCallbacks)
{
    NS_ENSURE_ARG_POINTER(aCallbacks);
    *aCallbacks = mCallbacks;

    NS_IF_ADDREF(*aCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks,
                                             PRUint32 flags)
{
    mCallbacks = aCallbacks;

    if (mCallbacks)
        mProgressSink = do_QueryInterface(mCallbacks);
    else
        mProgressSink = 0;

    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::OpenInputStream(PRUint32 aOffset,
                                    PRUint32 aCount,
                                    PRUint32 aFlags,
                                    nsIInputStream **aInput)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageTransport::OpenOutputStream(PRUint32 aOffset,
                                     PRUint32 aCount,
                                     PRUint32 aFlags,
                                     nsIOutputStream **aOutput)
{
    NS_ENSURE_TRUE(!mOutputStream, NS_ERROR_IN_PROGRESS);
    
    if (!PR_CLIST_IS_EMPTY(&mInputStreams) || !PR_CLIST_IS_EMPTY(&mReadRequests)) {
        NS_NOTREACHED("Attempt to open a memory cache output stream while "
                      "read is in progress!");
        return NS_ERROR_FAILURE;
    }

    NS_NEWXPCOM(mOutputStream, nsOutputStream);
    if (!mOutputStream)
        return NS_ERROR_OUT_OF_MEMORY;

    mOutputStream->SetTransport(this);
    mOutputStream->SetTransferCount(aCount);

    TruncateTo(aOffset);

    NS_ADDREF(*aOutput = mOutputStream);
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::AsyncRead(nsIStreamListener *aListener,
                              nsISupports *aContext,
                              PRUint32 aOffset,
                              PRUint32 aCount,
                              PRUint32 aFlags,
                              nsIRequest **aRequest)
{
    nsresult rv = NS_OK;

    nsReadRequest *reader;
    NS_NEWXPCOM(reader, nsReadRequest);
    if (!reader)
        return NS_ERROR_OUT_OF_MEMORY;

    reader->SetTransport(this);
    reader->SetTransferOffset(aOffset);
    reader->SetTransferCount(aCount);

    // append the read request to the list of existing read requests.
    // it is important to do this before the possibility of failure.
    PR_APPEND_LINK(reader, &mReadRequests);

    rv = reader->SetListener(aListener, aContext);
    if (NS_FAILED(rv)) goto error;

    rv = reader->Process();
    if (NS_FAILED(rv)) goto error;

    NS_ADDREF(*aRequest = reader);
    return NS_OK;

error:
    NS_DELETEXPCOM(reader);
    return rv;
}

NS_IMETHODIMP
nsStorageTransport::AsyncWrite(nsIStreamProvider *aProvider,
                               nsISupports *aContext,
                               PRUint32 aOffset,
                               PRUint32 aCount,
                               PRUint32 aFlags,
                               nsIRequest **aRequest)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------
// nsStorageTransport::nsReadRequest
//----------------------------------------------------------------------------

nsStorageTransport::nsReadRequest::nsReadRequest()
    : mTransport(nsnull)
    , mTransferOffset(0)
    , mTransferCount(MAX_COUNT)
    , mStatus(NS_OK)
    , mCanceled(PR_FALSE)
    , mOnStartFired(PR_FALSE)
    , mWaitingForWrite(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
    PR_INIT_CLIST(this);
}

nsStorageTransport::nsReadRequest::~nsReadRequest()
{
    if (mTransport)
        mTransport->ReadRequestCompleted(this);
}

nsresult
nsStorageTransport::nsReadRequest::SetListener(nsIStreamListener *aListener,
                                               nsISupports *aListenerContext)
{
    nsresult rv = NS_OK;

    mListener = aListener;
    mListenerContext = aListenerContext;

    // We proxy listener events to ourself and then forward them onto
    // the real listener (on the listener's thread).
 
    nsCOMPtr<nsIProxyObjectManager> proxyMgr =
        do_GetService(kProxyObjectManagerCID, &rv);

    if (NS_SUCCEEDED(rv))
        rv = proxyMgr->GetProxyForObject(NS_CURRENT_EVENTQ,
                                         NS_GET_IID(nsIStreamListener),
                                         NS_STATIC_CAST(nsIStreamListener *, this),
                                         PROXY_ASYNC | PROXY_ALWAYS,
                                         getter_AddRefs(mListenerProxy));
    return rv;
}

nsresult
nsStorageTransport::nsReadRequest::Process()
{
    nsresult rv = NS_OK;

    // this method must always be called on the client's thread

    NS_ENSURE_TRUE(mTransport, NS_ERROR_NOT_INITIALIZED);

    // always clear this flag initially
    mWaitingForWrite = PR_FALSE;

    if (!mOnStartFired) {
        // no need to proxy this callback
        (void) mListener->OnStartRequest(this, mListenerContext);
        mOnStartFired = PR_TRUE;
    }

    PRUint32 count = 0;

    if (mCanceled) {
        // forcing the transfer count to zero indicates that we are done.
        mTransferCount = 0;
    }
    else {
        rv = mTransport->Available(mTransferOffset, &count);
        if (NS_FAILED(rv)) return rv; 

        count = PR_MIN(count, mTransferCount);
    }

    if (count) {
        count = PR_MIN(count, MAX_IO_CHUNK);

        // proxy this callback
        (void) mListenerProxy->OnDataAvailable(this, mListenerContext,
                                               this,
                                               mTransferOffset,
                                               count);
        // Fire the progress notification...
        mTransport->FireOnProgress(this, mListenerContext, mTransferOffset);
    }
    else if ((mTransferCount == 0) || !mTransport->HasWriter()) {

        // there is no more data to read and there is no writer, so we
        // must stop this read request.

        // first let the transport know that we are done
        mTransport->ReadRequestCompleted(this);
        
        // no need to proxy this callback
        (void) mListener->OnStopRequest(this, mListenerContext, mStatus);

        //OnStopRequest completed and listeners no longer needed. 
        mListener=nsnull;
        mListenerContext=nsnull;
        mListenerProxy=nsnull;
    }
    else
        mWaitingForWrite = PR_TRUE;

    return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS5(nsStorageTransport::nsReadRequest,
                              nsITransportRequest,
                              nsIRequest,
                              nsIStreamListener,
                              nsIRequestObserver,
                              nsIInputStream)

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::GetTransport(nsITransport **aTransport)
{
    NS_ENSURE_ARG_POINTER(aTransport);
    NS_IF_ADDREF(*aTransport = mTransport);
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::GetName(PRUnichar **aName)
{
    NS_ENSURE_ARG_POINTER(aName);
    *aName = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::IsPending(PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = (mTransport ? PR_TRUE : PR_FALSE);
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::GetStatus(nsresult *aStatus)
{
    NS_ENSURE_ARG_POINTER(aStatus);
    *aStatus = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::Cancel(nsresult aStatus)
{
    mCanceled = PR_TRUE;
    mStatus = aStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::Suspend()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::Resume()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::GetLoadGroup(nsILoadGroup **loadGroup)
{
    *loadGroup = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::SetLoadGroup(nsILoadGroup *loadGroup)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::GetLoadFlags(nsLoadFlags *loadFlags)
{
    *loadFlags = nsIRequest::LOAD_NORMAL;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::SetLoadFlags(nsLoadFlags loadFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::OnStartRequest(nsIRequest *aRequest,
                                                  nsISupports *aContext)
{
    NS_NOTREACHED("nsStorageTransport::nsReadRequest::OnStartRequest");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::OnStopRequest(nsIRequest *aRequest,
                                                 nsISupports *aContext,
                                                 nsresult aStatus)
{
    NS_NOTREACHED("nsStorageTransport::nsReadRequest::OnStopRequest");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::OnDataAvailable(nsIRequest *aRequest,
                                                   nsISupports *aContext,
                                                   nsIInputStream *aInput,
                                                   PRUint32 aOffset,
                                                   PRUint32 aCount)
{
    nsresult rv = NS_OK;
    PRUint32 priorOffset = mTransferOffset;

    rv = mListener->OnDataAvailable(aRequest, aContext, aInput, aOffset, aCount);
    NS_ASSERTION(rv != NS_BASE_STREAM_WOULD_BLOCK, "not implemented");

    if (NS_FAILED(rv)) return rv;

    // avoid an infinite loop
    if (priorOffset == mTransferOffset) {
        NS_WARNING("nsIStreamListener::OnDataAvailable implementation did not "
                   "consume any data!");
        // end the read
        (void) Cancel(NS_ERROR_UNEXPECTED);
    }

    // post the next message...
    return Process();
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::Available(PRUint32 *aCount)
{
    NS_ENSURE_TRUE(mTransport, NS_BASE_STREAM_CLOSED);
    return mTransport->Available(mTransferOffset, aCount);
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::Read(char *aBuf,
                                        PRUint32 aCount,
                                        PRUint32 *aBytesRead)
{
    return ReadSegments(nsWriteToBuffer, aBuf, aCount, aBytesRead);
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::ReadSegments(nsWriteSegmentFun aWriter,
                                                void *aClosure,
                                                PRUint32 aCount,
                                                PRUint32 *aBytesRead)
{
    NS_ENSURE_TRUE(mTransport, NS_BASE_STREAM_CLOSED);

    nsresult rv = NS_OK;

    *aBytesRead = 0;

    // limit the number of bytes that can be read
    aCount = PR_MIN(aCount, mTransferCount);

    while (aCount) {
        char *ptr = nsnull;
        PRUint32 count = 0;

        rv = mTransport->GetReadSegment(mTransferOffset, &ptr, &count);
        if (NS_FAILED(rv)) return rv;

        if (count == 0)
            break;

        count = PR_MIN(count, aCount);

        while (count) {
            PRUint32 writeCount = 0;

            rv = aWriter(this, aClosure, ptr, *aBytesRead, count, &writeCount);

            if (rv == NS_BASE_STREAM_WOULD_BLOCK)
                return NS_OK; // mask this error
            else if (NS_FAILED(rv))
                return rv;

            ptr += writeCount;
            count -= writeCount;
            aCount -= writeCount;
            *aBytesRead += writeCount;

            // decrement the total number of bytes remaining to be read
            mTransferCount -= writeCount;
            mTransferOffset += writeCount;
        }
    }
    return rv;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_ENSURE_ARG_POINTER(aNonBlocking);
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::GetObserver(nsIInputStreamObserver **aObserver)
{
    NS_ENSURE_ARG_POINTER(aObserver);
    *aObserver = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsReadRequest::SetObserver(nsIInputStreamObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------
// nsStorageTransport::nsBlockingStream
//----------------------------------------------------------------------------

nsStorageTransport::nsBlockingStream::nsBlockingStream()
    : mTransport(nsnull)
    , mTransferCount(MAX_COUNT)
{
}

nsStorageTransport::nsBlockingStream::~nsBlockingStream()
{
}

//----------------------------------------------------------------------------
// nsStorageTransport::nsInputStream
//----------------------------------------------------------------------------

nsStorageTransport::nsInputStream::nsInputStream()
    : mOffset(0)
{
    NS_INIT_ISUPPORTS();
}

nsStorageTransport::nsInputStream::~nsInputStream()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStorageTransport::nsInputStream,
                              nsIInputStream)

NS_IMETHODIMP
nsStorageTransport::nsInputStream::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageTransport::nsInputStream::Available(PRUint32 *aCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageTransport::nsInputStream::Read(char *aBuf, PRUint32 aCount, PRUint32 *aBytesRead)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageTransport::nsInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                           void *aClosure,
                           PRUint32 aCount,
                           PRUint32 *aBytesRead)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageTransport::nsInputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_ENSURE_ARG_POINTER(aNonBlocking);
    *aNonBlocking = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsInputStream::GetObserver(nsIInputStreamObserver **aObserver)
{
    NS_ENSURE_ARG_POINTER(aObserver);
    *aObserver = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsInputStream::SetObserver(nsIInputStreamObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------
// nsStorageTransport::nsOutputStream
//----------------------------------------------------------------------------

nsStorageTransport::nsOutputStream::nsOutputStream()
{
    NS_INIT_ISUPPORTS();
}

nsStorageTransport::nsOutputStream::~nsOutputStream()
{
    if (mTransport)
        mTransport->CloseOutputStream();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStorageTransport::nsOutputStream,
                              nsIOutputStream)

NS_IMETHODIMP
nsStorageTransport::nsOutputStream::Close()
{
    NS_ENSURE_TRUE(mTransport, NS_BASE_STREAM_CLOSED);
    return mTransport->CloseOutputStream();
}

NS_IMETHODIMP
nsStorageTransport::nsOutputStream::Flush()
{
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsOutputStream::Write(const char *aBuf,
                                          PRUint32 aCount,
                                          PRUint32 *aBytesWritten)
{
    return WriteSegments(nsReadFromBuffer, (void *) aBuf, aCount, aBytesWritten);
}

NS_IMETHODIMP
nsStorageTransport::nsOutputStream::WriteFrom(nsIInputStream *aInput,
                                              PRUint32 aCount,
                                              PRUint32 *aBytesWritten)
{
    return WriteSegments(nsReadFromInputStream, aInput, aCount, aBytesWritten);
}

NS_IMETHODIMP
nsStorageTransport::nsOutputStream::WriteSegments(nsReadSegmentFun aReader,
                                                  void *aClosure,
                                                  PRUint32 aCount,
                                                  PRUint32 *aBytesWritten)
{
    NS_ENSURE_TRUE(mTransport, NS_BASE_STREAM_CLOSED);

    nsresult rv = NS_OK;

    *aBytesWritten = 0;

    // XXX need to honor mTransferCount

    while (aCount) {
        char *ptr;
        PRUint32 count;

        rv = mTransport->GetWriteSegment(&ptr, &count);
        if (NS_FAILED(rv)) return rv;

        count = PR_MIN(count, aCount);

        while (count) {
            PRUint32 readCount;

            rv = aReader(this, aClosure, ptr, *aBytesWritten, count, &readCount);

            if (NS_FAILED(rv))
                break;
 
            count -= readCount;
            aCount -= readCount;
            *aBytesWritten += readCount;

            rv = mTransport->AddToBytesWritten(readCount);
        }
    }
    return rv;
}

NS_IMETHODIMP
nsStorageTransport::nsOutputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_ENSURE_ARG_POINTER(aNonBlocking);
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsOutputStream::SetNonBlocking(PRBool aNonBlocking)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStorageTransport::nsOutputStream::GetObserver(nsIOutputStreamObserver **aObserver)
{
    NS_ENSURE_ARG_POINTER(aObserver);
    *aObserver = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageTransport::nsOutputStream::SetObserver(nsIOutputStreamObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
