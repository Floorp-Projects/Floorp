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
#include "nsCRT.h"
#include "prmem.h"
#include "netCore.h"

//----------------------------------------------------------------------------
// helper functions...
//----------------------------------------------------------------------------

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
    , mInputStreams(nsnull)
    , mReadRequests(nsnull)
    , mSegmentSize(NS_MEMORY_CACHE_SEGMENT_SIZE)
    , mMaxSize(NS_MEMORY_CACHE_BUFFER_SIZE)
    , mWriteCursor(0)
    , mSegments(nsnull)
    , mWriteSegment(nsnull)
{
    NS_INIT_ISUPPORTS();
}

nsMemoryCacheTransport::~nsMemoryCacheTransport()
{
    if (mOutputStream)
        CloseOutputStream();

    if (mSegments)
        DeleteSegments();
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

    // XXX wake up blocked readers

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
nsMemoryCacheTransport::DeleteSegments()
{
    while (mSegments) {
        nsSegment *s = mSegments->next;
        PR_Free(mSegments);
        mSegments = s;
    }
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

    NS_NEWXPCOM(mOutputStream, nsMemoryCacheBOS);
    if (!mOutputStream)
        return NS_ERROR_OUT_OF_MEMORY;

    mOutputStream->SetTransport(this);

    // SetWriteOffset(aOffset)
    // SetWriteCount(aCount)
    // SetWriteFlags(aFlags)

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
    return NS_ERROR_NOT_IMPLEMENTED;
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
    , mInputStream(nsnull)
    , mOffset(0)
    , mCountRemaining(0)
    , mStatus(NS_OK)
    , mCanceled(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsMemoryCacheReadRequest::~nsMemoryCacheReadRequest()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsMemoryCacheReadRequest,
                              nsITransportRequest,
                              nsIRequest)

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

//----------------------------------------------------------------------------
// nsMemoryCacheIS
//----------------------------------------------------------------------------

nsMemoryCacheIS::nsMemoryCacheIS()
    : mRequest(nsnull)
{
    NS_INIT_ISUPPORTS();
}

nsMemoryCacheIS::~nsMemoryCacheIS()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMemoryCacheIS,
                              nsIInputStream)

NS_IMETHODIMP
nsMemoryCacheIS::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheIS::Available(PRUint32 *aCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheIS::Read(char *aBuf, PRUint32 aCount, PRUint32 *aBytesRead)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheIS::ReadSegments(nsWriteSegmentFun aWriter,
                              void *aClosure,
                              PRUint32 aCount,
                              PRUint32 *aBytesRead)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryCacheIS::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_ENSURE_ARG_POINTER(aNonBlocking);
    *aNonBlocking = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheIS::GetObserver(nsIInputStreamObserver **aObserver)
{
    NS_ENSURE_ARG_POINTER(aObserver);
    *aObserver = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryCacheIS::SetObserver(nsIInputStreamObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------
// nsMemoryCacheBS
//----------------------------------------------------------------------------

nsMemoryCacheBS::nsMemoryCacheBS()
    : mTransport(nsnull)
    , mCountRemaining(0)
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

    // XXX need to honor mCountRemaining

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
    *aNonBlocking = PR_TRUE;
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
