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
 * The Original Code is nsMemoryCacheTransport.cpp, released February 26, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include "nsMemoryCacheTransport.h"
#include "nsCacheEntry.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "prmem.h"
#include "netCore.h"

#define MAX_IO_CHUNK 8192 // maximum count reported per OnDataAvailable

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
// nsMemoryCacheTransport
//----------------------------------------------------------------------------

nsMemoryCacheTransport::nsMemoryCacheTransport()
    : mOutputStream(nsnull)
    , mSegmentSize(NS_MEMORY_CACHE_SEGMENT_SIZE)
    , mMaxSize(NS_MEMORY_CACHE_BUFFER_SIZE)
    , mSegments(nsnull)
    , mWriteSegment(nsnull)
    , mWriteCursor(0)
{
    NS_INIT_ISUPPORTS();

    PR_INIT_CLIST(&mReadRequests);
    PR_INIT_CLIST(&mInputStreams);
}

nsMemoryCacheTransport::~nsMemoryCacheTransport()
{
    if (mOutputStream)
        CloseOutputStream();

    DeleteAllSegments();
}

nsresult
nsMemoryCacheTransport::Init(PRUint32 aBufSegmentSize, PRUint32 aBufMaxSize)
{
    mSegmentSize = aBufSegmentSize;
    mMaxSize = aBufMaxSize;
    return NS_OK;
}

nsresult
nsMemoryCacheTransport::GetReadSegment(PRUint32 aOffset, char **aPtr, PRUint32 *aCount)
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
nsMemoryCacheTransport::GetWriteSegment(char **aPtr, PRUint32 *aCount)
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
nsMemoryCacheTransport::AddToBytesWritten(PRUint32 aCount)
{
    // advance write cursor
    mWriteCursor += aCount;

    // clear write segment if we have written to the end
    if (!(mWriteCursor % mSegmentSize))
        mWriteSegment = nsnull;

    // process waiting readers
    PRCList *link = PR_LIST_HEAD(&mReadRequests);
    for (; link != &mReadRequests; link = PR_NEXT_LINK(link)) {
        nsMemoryCacheReadRequest *req = 
            NS_STATIC_CAST(nsMemoryCacheReadRequest *, link);
        if (req->IsWaitingForWrite())
            req->Process();
    }

    return NS_OK;
}

nsresult
nsMemoryCacheTransport::CloseOutputStream()
{
    if (mOutputStream) {
        mOutputStream->SetTransport(nsnull);
        mOutputStream = nsnull;

        // XXX wake up blocked reads
    }
    return NS_OK;
}

nsresult
nsMemoryCacheTransport::ReadRequestCompleted(nsMemoryCacheReadRequest *aReader)
{
    // remove the reader from the list of readers
    PR_REMOVE_AND_INIT_LINK(aReader);
    aReader->SetTransport(nsnull);
    return NS_OK;
}

nsresult
nsMemoryCacheTransport::Available(PRUint32 aStartingFrom, PRUint32 *aCount)
{
    *aCount = mWriteCursor - aStartingFrom;
    return NS_OK;
}

nsresult
nsMemoryCacheTransport::AddWriteSegment()
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
nsMemoryCacheTransport::AppendSegment(nsSegment *aSegment)
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
nsMemoryCacheTransport::DeleteSegments(nsSegment *segments)
{
    while (segments) {
        nsSegment *s = segments->next;
        PR_Free(segments);
        segments = s;
    }
}

void
nsMemoryCacheTransport::TruncateTo(PRUint32 aOffset)
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

nsMemoryCacheTransport::nsSegment *
nsMemoryCacheTransport::GetNthSegment(PRUint32 index)
{
    nsSegment *s = mSegments;
    for (; s && index; s = s->next, --index);
    return s;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMemoryCacheTransport,
                              nsITransport)

NS_IMETHODIMP
nsMemoryCacheTransport::GetSecurityInfo(nsISupports **aSecurityInfo)
{
    NS_ENSURE_ARG_POINTER(aSecurityInfo);
    *aSecurityInfo = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheTransport::GetProgressEventSink(nsIProgressEventSink **aSink)
{
    NS_ENSURE_ARG_POINTER(aSink);
    *aSink = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheTransport::SetProgressEventSink(nsIProgressEventSink *aSink)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheTransport::OpenInputStream(PRUint32 aOffset,
                                        PRUint32 aCount,
                                        PRUint32 aFlags,
                                        nsIInputStream **aInput)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheTransport::OpenOutputStream(PRUint32 aOffset,
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

    NS_NEWXPCOM(mOutputStream, nsMemoryCacheBOS);
    if (!mOutputStream)
        return NS_ERROR_OUT_OF_MEMORY;

    mOutputStream->SetTransport(this);
    mOutputStream->SetTransferCount(aCount);

    TruncateTo(aOffset);

    NS_ADDREF(*aOutput = mOutputStream);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheTransport::AsyncRead(nsIStreamListener *aListener,
                                  nsISupports *aContext,
                                  PRUint32 aOffset,
                                  PRUint32 aCount,
                                  PRUint32 aFlags,
                                  nsIRequest **aRequest)
{
    nsresult rv = NS_OK;

    nsMemoryCacheReadRequest *reader;
    NS_NEWXPCOM(reader, nsMemoryCacheReadRequest);
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
nsMemoryCacheTransport::AsyncWrite(nsIStreamProvider *aProvider,
                                   nsISupports *aContext,
                                   PRUint32 aOffset,
                                   PRUint32 aCount,
                                   PRUint32 aFlags,
                                   nsIRequest **aRequest)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------
// nsMemoryCacheReadRequest
//----------------------------------------------------------------------------

nsMemoryCacheReadRequest::nsMemoryCacheReadRequest()
    : mTransport(nsnull)
    , mTransferOffset(0)
    , mTransferCount(NS_TRANSFER_COUNT_UNKNOWN)
    , mStatus(NS_OK)
    , mCanceled(PR_FALSE)
    , mOnStartFired(PR_FALSE)
    , mWaitingForWrite(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
    PR_INIT_CLIST(this);
}

nsMemoryCacheReadRequest::~nsMemoryCacheReadRequest()
{
    if (mTransport)
        mTransport->ReadRequestCompleted(this);
}

nsresult
nsMemoryCacheReadRequest::SetListener(nsIStreamListener *aListener,
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
nsMemoryCacheReadRequest::Process()
{
    nsresult rv = NS_OK;

    // this method must always be called on the client's thread

    NS_ENSURE_TRUE(mTransport, NS_ERROR_NOT_INITIALIZED);

    // always clear this flag initially
    mWaitingForWrite = PR_FALSE;

    PRUint32 count = 0;

    rv = mTransport->Available(mTransferOffset, &count);
    if (NS_FAILED(rv)) return rv; 

    if (!mOnStartFired) {
        // no need to proxy this callback
        (void) mListener->OnStartRequest(this, mListenerContext);
        mOnStartFired = PR_TRUE;
    }

    count = PR_MIN(count, mTransferCount);

    if (count) {
        count = PR_MIN(count, MAX_IO_CHUNK);

        // proxy this callback
        (void) mListenerProxy->OnDataAvailable(this, mListenerContext,
                                               this,
                                               mTransferOffset,
                                               count);
    }
    else if ((mTransferCount == 0) || !mTransport->HasWriter()) {

        // there is no more data to read and there is no writer, so we
        // must stop this read request.

        // first let the transport know that we are done
        mTransport->ReadRequestCompleted(this);

        // no need to proxy this callback
        (void) mListener->OnStopRequest(this, mListenerContext, mStatus, nsnull);
    }
    else
        mWaitingForWrite = PR_TRUE;

    return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS5(nsMemoryCacheReadRequest,
                              nsITransportRequest,
                              nsIRequest,
                              nsIStreamListener,
                              nsIStreamObserver,
                              nsIInputStream)

NS_IMETHODIMP
nsMemoryCacheReadRequest::GetTransport(nsITransport **aTransport)
{
    NS_ENSURE_ARG_POINTER(aTransport);
    NS_IF_ADDREF(*aTransport = mTransport);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::GetName(PRUnichar **aName)
{
    NS_ENSURE_ARG_POINTER(aName);
    *aName = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::IsPending(PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = (mTransport ? PR_TRUE : PR_FALSE);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::GetStatus(nsresult *aStatus)
{
    NS_ENSURE_ARG_POINTER(aStatus);
    *aStatus = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::Cancel(nsresult aStatus)
{
    mCanceled = PR_TRUE;
    mStatus = aStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::Suspend()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::Resume()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::OnStartRequest(nsIRequest *aRequest,
                                         nsISupports *aContext)
{
    NS_NOTREACHED("nsMemoryCacheReadRequest::OnStartRequest");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::OnStopRequest(nsIRequest *aRequest,
                                        nsISupports *aContext,
                                        nsresult aStatus,
                                        const PRUnichar *aStatusText)
{
    NS_NOTREACHED("nsMemoryCacheReadRequest::OnStopRequest");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::OnDataAvailable(nsIRequest *aRequest,
                                          nsISupports *aContext,
                                          nsIInputStream *aInput,
                                          PRUint32 aOffset,
                                          PRUint32 aCount)
{
    nsresult rv = NS_OK;

    rv = mListener->OnDataAvailable(aRequest, aContext, aInput, aOffset, aCount);

    NS_ASSERTION(rv != NS_BASE_STREAM_WOULD_BLOCK, "not implemented");

    if (NS_FAILED(rv)) return rv;

    // post the next message...
    return Process();
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::Available(PRUint32 *aCount)
{
    NS_ENSURE_TRUE(mTransport, NS_BASE_STREAM_CLOSED);
    return mTransport->Available(mTransferOffset, aCount);
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::Read(char *aBuf, PRUint32 aCount, PRUint32 *aBytesRead)
{
    return ReadSegments(nsWriteToBuffer, aBuf, aCount, aBytesRead);
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::ReadSegments(nsWriteSegmentFun aWriter,
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
nsMemoryCacheReadRequest::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_ENSURE_ARG_POINTER(aNonBlocking);
    *aNonBlocking = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::GetObserver(nsIInputStreamObserver **aObserver)
{
    NS_ENSURE_ARG_POINTER(aObserver);
    *aObserver = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheReadRequest::SetObserver(nsIInputStreamObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------
// nsMemoryCacheBS
//----------------------------------------------------------------------------

nsMemoryCacheBS::nsMemoryCacheBS()
    : mTransport(nsnull)
    , mTransferCount(NS_TRANSFER_COUNT_UNKNOWN)
{
}

nsMemoryCacheBS::~nsMemoryCacheBS()
{
}

//----------------------------------------------------------------------------
// nsMemoryCacheBIS
//----------------------------------------------------------------------------

nsMemoryCacheBIS::nsMemoryCacheBIS()
    : mOffset(0)
{
    NS_INIT_ISUPPORTS();
}

nsMemoryCacheBIS::~nsMemoryCacheBIS()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMemoryCacheBIS,
                              nsIInputStream)

NS_IMETHODIMP
nsMemoryCacheBIS::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheBIS::Available(PRUint32 *aCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheBIS::Read(char *aBuf, PRUint32 aCount, PRUint32 *aBytesRead)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheBIS::ReadSegments(nsWriteSegmentFun aWriter,
                              void *aClosure,
                              PRUint32 aCount,
                              PRUint32 *aBytesRead)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheBIS::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_ENSURE_ARG_POINTER(aNonBlocking);
    *aNonBlocking = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheBIS::GetObserver(nsIInputStreamObserver **aObserver)
{
    NS_ENSURE_ARG_POINTER(aObserver);
    *aObserver = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheBIS::SetObserver(nsIInputStreamObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------
// nsMemoryCacheBOS
//----------------------------------------------------------------------------

nsMemoryCacheBOS::nsMemoryCacheBOS()
{
    NS_INIT_ISUPPORTS();
}

nsMemoryCacheBOS::~nsMemoryCacheBOS()
{
    if (mTransport)
        mTransport->CloseOutputStream();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMemoryCacheBOS,
                              nsIOutputStream)

NS_IMETHODIMP
nsMemoryCacheBOS::Close()
{
    NS_ENSURE_TRUE(mTransport, NS_BASE_STREAM_CLOSED);
    return mTransport->CloseOutputStream();
}

NS_IMETHODIMP
nsMemoryCacheBOS::Flush()
{
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheBOS::Write(const char *aBuf,
                        PRUint32 aCount,
                        PRUint32 *aBytesWritten)
{
    return WriteSegments(nsReadFromBuffer, (void *) aBuf, aCount, aBytesWritten);
}

NS_IMETHODIMP
nsMemoryCacheBOS::WriteFrom(nsIInputStream *aInput,
                            PRUint32 aCount,
                            PRUint32 *aBytesWritten)
{
    return WriteSegments(nsReadFromInputStream, aInput, aCount, aBytesWritten);
}

NS_IMETHODIMP
nsMemoryCacheBOS::WriteSegments(nsReadSegmentFun aReader,
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
nsMemoryCacheBOS::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_ENSURE_ARG_POINTER(aNonBlocking);
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheBOS::SetNonBlocking(PRBool aNonBlocking)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheBOS::GetObserver(nsIOutputStreamObserver **aObserver)
{
    NS_ENSURE_ARG_POINTER(aObserver);
    *aObserver = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheBOS::SetObserver(nsIOutputStreamObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
