/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsBuffer.h"
#include "nsAutoLock.h"
#include "nsCRT.h"
#include "nsIInputStream.h"
#include "nsIServiceManager.h"
#include "nsIPageManager.h"

////////////////////////////////////////////////////////////////////////////////

nsBuffer::nsBuffer()
    : mGrowBySize(0),
      mMaxSize(0),
      mAllocator(nsnull),
      mBufferSize(0),
      mReadSegment(nsnull),
      mReadCursor(0),
      mWriteSegment(nsnull),
      mWriteCursor(0),
      mEOF(PR_FALSE)
{
    NS_INIT_REFCNT();
    PR_INIT_CLIST(&mSegments);
}

NS_IMETHODIMP
nsBuffer::Init(PRUint32 growBySize, PRUint32 maxSize,
               nsIAllocator* allocator)
{
    mGrowBySize = growBySize;
    mMaxSize = maxSize;
    mAllocator = allocator;
    NS_ADDREF(mAllocator);
    return NS_OK;
}

nsBuffer::~nsBuffer()
{
    NS_IF_RELEASE(mAllocator);
}

NS_IMPL_ISUPPORTS(nsBuffer, nsIBuffer::GetIID());

NS_METHOD
nsBuffer::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsBuffer* buf = new nsBuffer();
    if (buf == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(buf);
    nsresult rv = buf->QueryInterface(aIID, aResult);
    NS_RELEASE(buf);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsBuffer::PushWriteSegment()
{
    nsAutoMonitor mon(this);        // protect mSegments

    if (mBufferSize >= mMaxSize) {
        return NS_ERROR_FAILURE;
    }

    // allocate a new segment to write into
    char* seg;
    PRCList* header;

    seg = (char*)mAllocator->Alloc(mGrowBySize);
    if (seg == nsnull) 
        return NS_ERROR_OUT_OF_MEMORY;

    mBufferSize += mGrowBySize;

    header = (PRCList*)seg;
    PR_INSERT_BEFORE(header, &mSegments);       // insert at end

    // initialize the write segment
    mWriteSegment = seg;
    mWriteSegmentEnd = mWriteSegment + mGrowBySize;
    mWriteCursor = mWriteSegment + sizeof(PRCList);

    return NS_OK;
}

nsresult
nsBuffer::PopReadSegment()
{
    nsresult rv;
    nsAutoMonitor mon(this);        // protect mSegments

    PRCList* header = (PRCList*)mSegments.next;
    char* segment = (char*)header;

    NS_ASSERTION(mReadSegment == segment, "wrong segment");

    // make sure that the writer isn't still in this segment (that the
    // reader is removing)
    NS_ASSERTION(!(segment <= mWriteCursor && mWriteCursor < segment + mGrowBySize),
                 "removing writer's segment");

    PR_REMOVE_LINK(header);     // unlink from mSegments

    mBufferSize -= mGrowBySize;

    rv = mAllocator->Free(segment);
    if (NS_FAILED(rv)) return rv;

    // initialize the read segment
    if (PR_CLIST_IS_EMPTY(&mSegments)) {
        mReadSegment = nsnull;
        mReadSegmentEnd = nsnull;
        mReadCursor = nsnull;
    }
    else {
        mReadSegment = (char*)mSegments.next;
        mReadSegmentEnd = mReadSegment + mGrowBySize;
        mReadCursor = mReadSegment + sizeof(PRCList);
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIBuffer methods:

NS_IMETHODIMP
nsBuffer::Read(char* toBuf, PRUint32 bufLen, PRUint32 *readCount)
{
    nsresult rv;
    PRUint32 readBufferLen;
    char* readBuffer;

    *readCount = 0;
    while (bufLen > 0) {
        rv = GetReadBuffer(&readBufferLen, &readBuffer);
        if (rv == NS_BASE_STREAM_EOF)      // all we're going to get
            return *readCount > 0 ? NS_OK : NS_BASE_STREAM_EOF;
        if (NS_FAILED(rv)) return rv;

        if (readBufferLen == 0)
            return mEOF && *readCount == 0 ? NS_BASE_STREAM_EOF : NS_OK;

        PRUint32 count = PR_MIN(bufLen, readBufferLen);
        nsCRT::memcpy(toBuf, readBuffer, count);
        *readCount += count;
        toBuf += count;
        bufLen -= count;

        if (mReadCursor + count == mReadSegmentEnd) {
            rv = PopReadSegment();
            if (NS_FAILED(rv)) return rv;
        }
        else {
            mReadCursor += count;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsBuffer::GetReadBuffer(PRUint32 *readBufferLength, char* *result)
{
    if (mReadSegment == nsnull) {
        if (PR_CLIST_IS_EMPTY(&mSegments)) {
            *readBufferLength = 0;
            *result = nsnull;
            return mEOF ? NS_BASE_STREAM_EOF : NS_OK;
        }
        else {
            mReadSegment = (char*)mSegments.next;
            mReadSegmentEnd = mReadSegment + mGrowBySize;
            mReadCursor = mReadSegment + sizeof(PRCList);
        }
    }

    // snapshot the write cursor into a local variable -- this allows
    // a writer to freely change it while we're reading while avoiding
    // using a lock
    char* snapshotWriteCursor = mWriteCursor;   // atomic

    // next check if the write cursor is in our segment
    if (mReadCursor <= snapshotWriteCursor &&
        snapshotWriteCursor < mReadSegmentEnd) {
        // same segment -- read up to the snapshotWriteCursor
        *readBufferLength = snapshotWriteCursor - mReadCursor;
    }
    else {
        // otherwise, read up to the end of this segment
        *readBufferLength = mReadSegmentEnd - mReadCursor;
    }
    *result = mReadCursor;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsBuffer::Write(const char* fromBuf, PRUint32 bufLen, PRUint32 *writeCount)
{
    nsresult rv;

    if (mEOF)
        return NS_BASE_STREAM_EOF;

    *writeCount = 0;
    while (bufLen > 0) {
        PRUint32 writeBufLen;
        char* writeBuf;
        rv = GetWriteBuffer(&writeBufLen, &writeBuf);
        if (NS_FAILED(rv)) {
            // if we failed to allocate a new segment, we're probably out
            // of memory, but we don't care -- just report what we were
            // able to write so far
            return NS_OK;
        }

        PRUint32 count = PR_MIN(writeBufLen, bufLen);
        nsCRT::memcpy(writeBuf, fromBuf, count);
        bufLen -= count;
        *writeCount += count;
        // set the write cursor after the data is valid
        if (mWriteCursor + count == mWriteSegmentEnd) {
            mWriteSegment = nsnull;      // allocate a new segment next time around
            mWriteSegmentEnd = nsnull;
            mWriteCursor = nsnull;
        }
        else
            mWriteCursor += count;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsBuffer::Write(nsIInputStream* fromStream, PRUint32 *writeCount)
{
    nsresult rv;

    if (mEOF)
        return NS_BASE_STREAM_EOF;

    *writeCount = 0;
    while (PR_TRUE) {
        PRUint32 writeBufLen;
        char* writeBuf;
        rv = GetWriteBuffer(&writeBufLen, &writeBuf);
        if (NS_FAILED(rv)) {
            // if we failed to allocate a new segment, we're probably out
            // of memory, but we don't care -- just report what we were
            // able to write so far
            return NS_OK;
        }

        PRUint32 readCount;
        rv = fromStream->Read(writeBuf, writeBufLen, &readCount);
        if (NS_FAILED(rv)) {
            // if we failed to read just report what we were
            // able to write so far
            return NS_OK;
        }
        *writeCount += readCount;
        // set the write cursor after the data is valid
        if (mWriteCursor + readCount == mWriteSegmentEnd) {
            mWriteSegment = nsnull;      // allocate a new segment next time around
            mWriteSegmentEnd = nsnull;
            mWriteCursor = nsnull;
        }
        else
            mWriteCursor += readCount;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsBuffer::GetWriteBuffer(PRUint32 *writeBufferLength, char* *result)
{
    if (mEOF)
        return NS_BASE_STREAM_EOF;

    nsresult rv;
    if (mWriteSegment == nsnull) {
        if (mBufferSize >= mMaxSize) 
            return NS_ERROR_FAILURE;

        rv = PushWriteSegment();
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(mWriteSegment != nsnull, "failed to allocate segment");
    }

    *writeBufferLength = mWriteSegmentEnd - mWriteCursor;
    *result = mWriteCursor;
    return NS_OK;
}

NS_IMETHODIMP
nsBuffer::SetEOF()
{
    if (mEOF)
        return NS_BASE_STREAM_EOF;

    mEOF = PR_TRUE;
    mWriteSegment = nsnull;     // allows reader to free last segment w/o asserting
    mWriteSegmentEnd = nsnull;
    // don't reset mWriteCursor here -- we need it for the EOF point in the buffer
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kAllocatorCID, NS_ALLOCATOR_CID);

NS_COM nsresult
NS_NewBuffer(nsIBuffer* *result,
             PRUint32 growBySize, PRUint32 maxSize)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIAllocator, alloc, kAllocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsBuffer* buf;
    rv = nsBuffer::Create(NULL, nsIBuffer::GetIID(), (void**)&buf);
    if (NS_FAILED(rv)) return rv;

    rv = buf->Init(growBySize, maxSize, alloc);
    if (NS_FAILED(rv)) {
        NS_RELEASE(buf);
        return rv;
    }

    *result = buf;
    return NS_OK;
}

static NS_DEFINE_CID(kPageManagerCID, NS_PAGEMANAGER_CID);

NS_COM nsresult
NS_NewPageBuffer(nsIBuffer* *result,
                 PRUint32 growBySize, PRUint32 maxSize)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIAllocator, alloc, kPageManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsBuffer* buf;
    rv = nsBuffer::Create(NULL, nsIBuffer::GetIID(), (void**)&buf);
    if (NS_FAILED(rv)) return rv;

    rv = buf->Init(growBySize, maxSize, alloc);
    if (NS_FAILED(rv)) {
        NS_RELEASE(buf);
        return rv;
    }

    *result = buf;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
