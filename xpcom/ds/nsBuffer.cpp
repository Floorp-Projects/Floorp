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
    NS_ASSERTION(sizeof(PRCList) <= SEGMENT_OVERHEAD,
                 "need to change SEGMENT_OVERHEAD size");
    NS_ASSERTION(growBySize > SEGMENT_OVERHEAD, "bad growBySize");
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
    PRCList* header;

    header = (PRCList*)mAllocator->Alloc(mGrowBySize);
    if (header == nsnull) 
        return NS_ERROR_OUT_OF_MEMORY;

    mBufferSize += mGrowBySize;

    PR_INSERT_BEFORE(header, &mSegments);       // insert at end

    // initialize the write segment
    mWriteSegment = header;
    mWriteSegmentEnd = (char*)mWriteSegment + mGrowBySize;
    mWriteCursor = (char*)mWriteSegment + sizeof(PRCList);

    return NS_OK;
}

nsresult
nsBuffer::PopReadSegment()
{
    nsresult rv;
    nsAutoMonitor mon(this);        // protect mSegments

    PRCList* header = (PRCList*)mSegments.next;
    char* segment = (char*)header;

    NS_ASSERTION(mReadSegment == header, "wrong segment");

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
        mReadSegment = mSegments.next;
        mReadSegmentEnd = (char*)mReadSegment + mGrowBySize;
        mReadCursor = (char*)mReadSegment + sizeof(PRCList);
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
        rv = GetReadBuffer(0, &readBuffer, &readBufferLen);
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
nsBuffer::GetReadBuffer(PRUint32 startPosition, 
                        char* *result,
                        PRUint32 *readBufferLength)
{
    // first set the read segment and cursor if not already set
    if (mReadSegment == nsnull) {
        if (PR_CLIST_IS_EMPTY(&mSegments)) {
            *readBufferLength = 0;
            *result = nsnull;
            return mEOF ? NS_BASE_STREAM_EOF : NS_OK;
        }
        else {
            mReadSegment = mSegments.next;
            mReadSegmentEnd = (char*)mReadSegment + mGrowBySize;
            mReadCursor = (char*)mReadSegment + sizeof(PRCList);
        }
    }

    // now search for the segment starting from startPosition and return it
    PRCList* curSeg = mReadSegment;
    char* curSegStart = mReadCursor;
    char* curSegEnd = mReadSegmentEnd;
    PRInt32 amt;
    PRInt32 offset = (PRInt32)startPosition;
    while (offset >= 0) {
        // snapshot the write cursor into a local variable -- this allows
        // a writer to freely change it while we're reading while avoiding
        // using a lock
        char* snapshotWriteCursor = mWriteCursor;   // atomic

        // next check if the write cursor is in our segment
        if (curSegStart <= snapshotWriteCursor &&
            snapshotWriteCursor < curSegEnd) {
            // same segment -- read up to the snapshotWriteCursor
            curSegEnd = snapshotWriteCursor;

            amt = curSegEnd - curSegStart;
            if (offset < amt) {
                // startPosition is in this segment, so read up to its end
                *readBufferLength = amt - offset;
                *result = curSegStart + offset;
                return NS_OK;
            }
            else {
                // don't continue past the write segment
                *readBufferLength = 0;
                *result = nsnull;
                return mEOF ? NS_BASE_STREAM_EOF : NS_OK;
            }
        }
        else {
            amt = curSegEnd - curSegStart;
            if (offset < amt) {
                // startPosition is in this segment, so read up to its end
                *readBufferLength = amt - offset;
                *result = curSegStart + offset;
                return NS_OK;
            }
            else {
                curSeg = PR_NEXT_LINK(curSeg);
                if (curSeg == mReadSegment) {
                    // been all the way around
                    *readBufferLength = 0;
                    *result = nsnull;
                    return mEOF ? NS_BASE_STREAM_EOF : NS_OK;
                }
                curSegEnd = (char*)curSeg + mGrowBySize;
                curSegStart = (char*)curSeg + sizeof(PRCList);
                offset -= amt;
            }
        }
    }
    return NS_ERROR_FAILURE;
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
        rv = GetWriteBuffer(0, &writeBuf, &writeBufLen);
        if (NS_FAILED(rv)) {
            // if we failed to allocate a new segment, we're probably out
            // of memory, but we don't care -- just report what we were
            // able to write so far
            return NS_OK;
        }

        PRUint32 count = PR_MIN(writeBufLen, bufLen);
        nsCRT::memcpy(writeBuf, fromBuf, count);
		fromBuf += count;
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
nsBuffer::WriteFrom(nsIInputStream* fromStream, PRUint32 count, PRUint32 *writeCount)
{
    nsresult rv;

    if (mEOF)
        return NS_BASE_STREAM_EOF;

    *writeCount = 0;
    while (count > 0) {
        PRUint32 writeBufLen;
        char* writeBuf;
        rv = GetWriteBuffer(0, &writeBuf, &writeBufLen);
        if (NS_FAILED(rv)) {
            // if we failed to allocate a new segment, we're probably out
            // of memory, but we don't care -- just report what we were
            // able to write so far
            return NS_OK;
        }

        PRUint32 readCount;
        rv = fromStream->Read(writeBuf, PR_MIN(writeBufLen, count), &readCount);
        if (NS_FAILED(rv)) {
            // if we failed to read just report what we were
            // able to write so far
            return NS_OK;
        }
        *writeCount += readCount;
        count -= readCount;
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
nsBuffer::GetWriteBuffer(PRUint32 startPosition,
                         char* *result,
                         PRUint32 *writeBufferLength)
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

NS_IMETHODIMP
nsBuffer::AtEOF(PRBool *result)
{
    *result = mEOF;
    return NS_OK;
}

typedef PRInt32 (*compare_t)(const char*, const char*, PRUint32);

NS_IMETHODIMP
nsBuffer::Search(const char* string, PRBool ignoreCase, 
                 PRBool *found, PRUint32 *offsetSearchedTo)
{
    nsresult rv;
    char* bufSeg1;
    PRUint32 bufSegLen1;
    PRUint32 segmentPos = 0;
    PRUint32 strLen = nsCRT::strlen(string);
    compare_t compare = 
        ignoreCase ? (compare_t)nsCRT::strncasecmp : (compare_t)nsCRT::strncmp;

    rv = GetReadBuffer(segmentPos, &bufSeg1, &bufSegLen1);
    if (NS_FAILED(rv) || bufSegLen1 == 0) {
        *found = PR_FALSE;
        *offsetSearchedTo = segmentPos;
        return NS_OK;
    }

    while (PR_TRUE) {
        PRUint32 i;
        // check if the string is in the buffer segment
        for (i = 0; i < bufSegLen1 - strLen + 1; i++) {
            if (compare(&bufSeg1[i], string, strLen) == 0) {
                *found = PR_TRUE;
                *offsetSearchedTo = segmentPos + i;
                return NS_OK;
            }
        }

        // get the next segment
        char* bufSeg2;
        PRUint32 bufSegLen2;
        segmentPos += bufSegLen1;
        rv = GetReadBuffer(segmentPos, &bufSeg2, &bufSegLen2);
        if (NS_FAILED(rv) || bufSegLen2 == 0) {
            *found = PR_FALSE;
            if (mEOF) 
                *offsetSearchedTo = segmentPos - bufSegLen1;
            else
                *offsetSearchedTo = segmentPos - bufSegLen1 - strLen + 1;
            return NS_OK;
        }

        // check if the string is straddling the next buffer segment
        PRUint32 limit = PR_MIN(strLen, bufSegLen2 + 1);
        for (i = 0; i < limit; i++) {
            PRUint32 strPart1Len = strLen - i - 1;
            PRUint32 strPart2Len = strLen - strPart1Len;
            const char* strPart2 = &string[strLen - strPart2Len];
            PRUint32 bufSeg1Offset = bufSegLen1 - strPart1Len;
            if (compare(&bufSeg1[bufSeg1Offset], string, strPart1Len) == 0 &&
                compare(bufSeg2, strPart2, strPart2Len) == 0) {
                *found = PR_TRUE;
                *offsetSearchedTo = segmentPos - strPart1Len;
                return NS_OK;
            }
        }

        // finally continue with the next buffer
        bufSeg1 = bufSeg2;
        bufSegLen1 = bufSegLen2;
    }
    NS_NOTREACHED("can't get here");
    return NS_ERROR_FAILURE;    // keep compiler happy
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
