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
      mObserver(nsnull),
      mBufferSize(0),
      mReadSegment(nsnull),
      mReadCursor(0),
      mWriteSegment(nsnull),
      mWriteCursor(0),
      mReaderClosed(PR_FALSE),
      mCondition(NS_OK)
{
    NS_INIT_REFCNT();
    PR_INIT_CLIST(&mSegments);
}

NS_IMETHODIMP
nsBuffer::Init(PRUint32 growBySize, PRUint32 maxSize,
               nsIBufferObserver* observer, nsIAllocator* allocator)
{
    NS_ASSERTION(sizeof(PRCList) <= SEGMENT_OVERHEAD,
                 "need to change SEGMENT_OVERHEAD size");
    NS_ASSERTION(growBySize > SEGMENT_OVERHEAD, "bad growBySize");
    mGrowBySize = growBySize;
    mMaxSize = maxSize;
    mObserver = observer;
    NS_IF_ADDREF(mObserver);
    mAllocator = allocator;
    NS_ADDREF(mAllocator);
    return NS_OK;
}

nsBuffer::~nsBuffer()
{
    // Free any allocated pages...
    while (!PR_CLIST_IS_EMPTY(&mSegments)) {
        PRCList* header = (PRCList*)mSegments.next;
        char* segment = (char*)header;

        PR_REMOVE_LINK(header);     // unlink from mSegments
        (void) mAllocator->Free(segment);
    }

    NS_IF_RELEASE(mObserver);
    NS_IF_RELEASE(mAllocator);
}

NS_IMPL_ISUPPORTS1(nsBuffer, nsIBuffer)

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
    nsAutoCMonitor mon(this);        // protect mSegments

    if (mBufferSize >= mMaxSize) {
        if (mObserver) {
            nsresult rv = mObserver->OnFull(this);
            if (NS_FAILED(rv)) return rv;
        }
        return NS_BASE_STREAM_WOULD_BLOCK;
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
    nsAutoCMonitor mon(this);        // protect mSegments

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
        if (mObserver) {
            rv = mObserver->OnEmpty(this);
            if (NS_FAILED(rv)) return rv;
        }
        return NS_BASE_STREAM_WOULD_BLOCK;
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
nsBuffer::ReadSegments(nsWriteSegmentFun writer, void* closure, PRUint32 count,
                       PRUint32 *readCount)
{
    NS_ASSERTION(!mReaderClosed, "state change error");

    nsAutoCMonitor mon(this);
    nsresult rv = NS_OK;
    PRUint32 readBufferLen;
    const char* readBuffer;

    *readCount = 0;
    while (count > 0) {
        rv = GetReadSegment(0, &readBuffer, &readBufferLen);
        if (NS_FAILED(rv) || readBufferLen == 0) {
            return *readCount == 0 ? rv : NS_OK;
        }

        readBufferLen = PR_MIN(readBufferLen, count);
        while (readBufferLen > 0) {
            PRUint32 writeCount;
            rv = writer(closure, readBuffer, *readCount, readBufferLen, &writeCount);
			NS_ASSERTION(rv != NS_BASE_STREAM_EOF, "Write should not return EOF");
            if (rv == NS_BASE_STREAM_WOULD_BLOCK || NS_FAILED(rv) || writeCount == 0) {
                // if we failed to write just report what we were
                // able to read so far
                return *readCount == 0 ? rv : NS_OK;
            }
            NS_ASSERTION(writeCount <= readBufferLen, "writer returned bad writeCount");
            readBuffer += writeCount;
            readBufferLen -= writeCount;
            *readCount += writeCount;
            count -= writeCount;

            if (mReadCursor + writeCount == mReadSegmentEnd) {
                rv = PopReadSegment();
                if (NS_FAILED(rv)) {
                    return *readCount == 0 ? rv : NS_OK;
                }
            }
            else {
                mReadCursor += writeCount;
            }
        }
    }
    return NS_OK;
}

static NS_METHOD
nsWriteToRawBuffer(void* closure,
                   const char* fromRawSegment,
                   PRUint32 offset,
                   PRUint32 count,
                   PRUint32 *writeCount)
{
    char* toBuf = (char*)closure;
    nsCRT::memcpy(&toBuf[offset], fromRawSegment, count);
    *writeCount = count;
    return NS_OK;
}

NS_IMETHODIMP
nsBuffer::Read(char* toBuf, PRUint32 bufLen, PRUint32 *readCount)
{
    return ReadSegments(nsWriteToRawBuffer, toBuf, bufLen, readCount);
}

NS_IMETHODIMP
nsBuffer::GetReadSegment(PRUint32 segmentLogicalOffset, 
                         const char* *resultSegment,
                         PRUint32 *resultSegmentLen)
{
    nsAutoCMonitor mon(this);

    // set the read segment and cursor if not already set
    if (mReadSegment == nsnull) {
        if (PR_CLIST_IS_EMPTY(&mSegments)) {
            *resultSegmentLen = 0;
            *resultSegment = nsnull;
            return mCondition;
        }
        else {
            mReadSegment = mSegments.next;
            mReadSegmentEnd = (char*)mReadSegment + mGrowBySize;
            mReadCursor = (char*)mReadSegment + sizeof(PRCList);
        }
    }

    // now search for the segment starting from segmentLogicalOffset and return it
    PRCList* curSeg = mReadSegment;
    char* curSegStart = mReadCursor;
    char* curSegEnd = mReadSegmentEnd;
    PRInt32 amt;
    PRInt32 offset = (PRInt32)segmentLogicalOffset;
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
                // segmentLogicalOffset is in this segment, so read up to its end
                *resultSegmentLen = amt - offset;
                *resultSegment = curSegStart + offset;
                return NS_OK;
            }
            else {
                // don't continue past the write segment
                *resultSegmentLen = 0;
                *resultSegment = nsnull;
                return mCondition;
            }
        }
        else {
            amt = curSegEnd - curSegStart;
            if (offset < amt) {
                // segmentLogicalOffset is in this segment, so read up to its end
                *resultSegmentLen = amt - offset;
                *resultSegment = curSegStart + offset;
                return NS_OK;
            }
            else {
                curSeg = PR_NEXT_LINK(curSeg);
                if (curSeg == mReadSegment) {
                    // been all the way around
                    *resultSegmentLen = 0;
                    *resultSegment = nsnull;
                    return mCondition;
                }
                curSegEnd = (char*)curSeg + mGrowBySize;
                curSegStart = (char*)curSeg + sizeof(PRCList);
                offset -= amt;
            }
        }
    }
    NS_NOTREACHED("nsBuffer::GetReadSegment failed");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsBuffer::GetReadableAmount(PRUint32 *result)
{
    NS_ASSERTION(!mReaderClosed, "state change error");

    nsAutoCMonitor mon(this);
    *result = 0; 

    // first set the read segment and cursor if not already set
    if (mReadSegment == nsnull) {
        if (PR_CLIST_IS_EMPTY(&mSegments)) {
            return NS_OK;
        }
        else {
            mReadSegment = mSegments.next;
            mReadSegmentEnd = (char*)mReadSegment + mGrowBySize;
            mReadCursor = (char*)mReadSegment + sizeof(PRCList);
        }
    }

    // now search for the segment starting from segmentLogicalOffset and return it
    PRCList* curSeg = mReadSegment;
    char* curSegStart = mReadCursor;
    char* curSegEnd = mReadSegmentEnd;
    PRInt32 amt;
    while (PR_TRUE) {
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
            *result += amt;
            return NS_OK;
        }
        else {
            amt = curSegEnd - curSegStart;
            *result += amt;
            curSeg = PR_NEXT_LINK(curSeg);
            if (curSeg == mReadSegment) {
                // been all the way around
                return NS_OK;
            }
            curSegEnd = (char*)curSeg + mGrowBySize;
            curSegStart = (char*)curSeg + sizeof(PRCList);
        }
    }
    return NS_ERROR_FAILURE;
}

#define COMPARE(s1, s2, i)	ignoreCase ? nsCRT::strncasecmp((const char *)s1, (const char *)s2, (PRUint32)i) : nsCRT::strncmp((const char *)s1, (const char *)s2, (PRUint32)i)

NS_IMETHODIMP
nsBuffer::Search(const char* string, PRBool ignoreCase, 
                 PRBool *found, PRUint32 *offsetSearchedTo)
{
    NS_ASSERTION(!mReaderClosed, "state change error");

    nsresult rv;
    const char* bufSeg1;
    PRUint32 bufSegLen1;
    PRUint32 segmentPos = 0;
    PRUint32 strLen = nsCRT::strlen(string);

    rv = GetReadSegment(segmentPos, &bufSeg1, &bufSegLen1);
    if (NS_FAILED(rv) || bufSegLen1 == 0) {
        *found = PR_FALSE;
        *offsetSearchedTo = segmentPos;
        return NS_OK;
    }

    while (PR_TRUE) {
        PRUint32 i;
        // check if the string is in the buffer segment
        for (i = 0; i < bufSegLen1 - strLen + 1; i++) {
            if (COMPARE(&bufSeg1[i], string, strLen) == 0) {
                *found = PR_TRUE;
                *offsetSearchedTo = segmentPos + i;
                return NS_OK;
            }
        }

        // get the next segment
        const char* bufSeg2;
        PRUint32 bufSegLen2;
        segmentPos += bufSegLen1;
        rv = GetReadSegment(segmentPos, &bufSeg2, &bufSegLen2);
        if (NS_FAILED(rv) || bufSegLen2 == 0) {
            *found = PR_FALSE;
            if (mCondition != NS_OK)    // XXX NS_FAILED?
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
            if (COMPARE(&bufSeg1[bufSeg1Offset], string, strPart1Len) == 0 &&
                COMPARE(bufSeg2, strPart2, strPart2Len) == 0) {
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

NS_IMETHODIMP
nsBuffer::ReaderClosed()
{
    nsresult rv = NS_OK;
    nsAutoCMonitor mon(this);        // protect mSegments

    // first prevent any more writing
    mReaderClosed = PR_TRUE;

    // then free any unread segments...
    
    // first set the read segment and cursor if not already set
    if (mReadSegment == nsnull) {
        if (!PR_CLIST_IS_EMPTY(&mSegments)) {
            mReadSegment = mSegments.next;
            mReadSegmentEnd = (char*)mReadSegment + mGrowBySize;
            mReadCursor = (char*)mReadSegment + sizeof(PRCList);
        }
    }

    while (mReadSegment) {
        // snapshot the write cursor into a local variable -- this allows
        // a writer to freely change it while we're reading while avoiding
        // using a lock
        char* snapshotWriteCursor = mWriteCursor;   // atomic

        // next check if the write cursor is in our segment
        if (mReadCursor <= snapshotWriteCursor &&
            snapshotWriteCursor < mReadSegmentEnd) {
            // same segment -- we've discarded all the unread segments we
            // can, so just updatethe read cursor
            mReadCursor = mWriteCursor;
            break;
        }
        // else advance to the next segment, freeing this one
        rv = PopReadSegment();
        if (NS_FAILED(rv)) break;
    }

#ifdef DEBUG
    PRUint32 amt;
    const char* buf;
    rv = GetReadSegment(0, &buf, &amt);
    NS_ASSERTION(rv == NS_BASE_STREAM_EOF ||
                 (NS_SUCCEEDED(rv) && amt == 0), "ReaderClosed failed");
#endif

    return rv;
}

NS_IMETHODIMP
nsBuffer::GetCondition(nsresult *result)
{
    *result = mCondition;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsBuffer::WriteSegments(nsReadSegmentFun reader, void* closure, PRUint32 count,
                        PRUint32 *writeCount)
{
    nsresult rv = NS_OK;

    nsAutoCMonitor mon(this);
    *writeCount = 0;

    if (mReaderClosed) {
        rv = NS_BASE_STREAM_CLOSED;
        goto done;
    }

    if (NS_FAILED(mCondition)) {
        rv = mCondition;
        goto done;
    }

    while (count > 0) {
        PRUint32 writeBufLen;
        char* writeBuf;
        rv = GetWriteSegment(&writeBuf, &writeBufLen);
        if (NS_FAILED(rv) || writeBufLen == 0) {
            // if we failed to allocate a new segment, we're probably out
            // of memory, but we don't care -- just report what we were
            // able to write so far
            rv = (*writeCount == 0) ? rv : NS_OK;
            goto done;
        }

        writeBufLen = PR_MIN(writeBufLen, count);
        while (writeBufLen > 0) {
            PRUint32 readCount = 0;
            rv = reader(closure, writeBuf, *writeCount, writeBufLen, &readCount);
            if (rv == NS_BASE_STREAM_WOULD_BLOCK || readCount == 0) {
                // if the place we're putting the data would block (probably ran
                // out of room) just return what we were able to write so far
                rv = (*writeCount == 0) ? rv : NS_OK;
                goto done;
            }
            if (NS_FAILED(rv)) {
                // save the failure condition so that we can get it again later
                nsresult rv2 = SetCondition(rv);
                NS_ASSERTION(NS_SUCCEEDED(rv2), "SetCondition failed");
                // if we failed to read just report what we were
                // able to write so far
                rv = (*writeCount == 0) ? rv : NS_OK;
                goto done;
            }
            NS_ASSERTION(readCount <= writeBufLen, "reader returned bad readCount");
            writeBuf += readCount;
            writeBufLen -= readCount;
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
    }
done:
    if (mObserver && *writeCount) {
        mObserver->OnWrite(this, *writeCount);
    }
    return rv;
}

static NS_METHOD
nsReadFromRawBuffer(void* closure,
                    char* toRawSegment,
                    PRUint32 offset,
                    PRUint32 count,
                    PRUint32 *readCount)
{
    const char* fromBuf = (const char*)closure;
    nsCRT::memcpy(toRawSegment, &fromBuf[offset], count);
    *readCount = count;
    return NS_OK;
}

NS_IMETHODIMP
nsBuffer::Write(const char* fromBuf, PRUint32 bufLen, PRUint32 *writeCount)
{
    return WriteSegments(nsReadFromRawBuffer, (void*)fromBuf, bufLen, writeCount);
}

static NS_METHOD
nsReadFromInputStream(void* closure,
                      char* toRawSegment, 
                      PRUint32 offset,
                      PRUint32 count,
                      PRUint32 *readCount)
{
    nsIInputStream* fromStream = (nsIInputStream*)closure;
    return fromStream->Read(toRawSegment, count, readCount);
}

NS_IMETHODIMP
nsBuffer::WriteFrom(nsIInputStream* fromStream, PRUint32 count, PRUint32 *writeCount)
{
    return WriteSegments(nsReadFromInputStream, fromStream, count, writeCount);
}

NS_IMETHODIMP
nsBuffer::GetWriteSegment(char* *resultSegment,
                          PRUint32 *resultSegmentLen)
{
    nsAutoCMonitor mon(this);
    if (mReaderClosed)
        return NS_BASE_STREAM_CLOSED;

    nsresult rv;
    *resultSegmentLen = 0;
    *resultSegment = nsnull;
    if (mWriteSegment == nsnull) {
        rv = PushWriteSegment();
        if (NS_FAILED(rv) || rv == NS_BASE_STREAM_WOULD_BLOCK) return rv;

        NS_ASSERTION(mWriteSegment != nsnull, "failed to allocate segment");
    }

    *resultSegmentLen = mWriteSegmentEnd - mWriteCursor;
    *resultSegment = mWriteCursor;
    NS_ASSERTION(*resultSegmentLen > 0, "Failed to get write segment.");
    return NS_OK;
}

NS_IMETHODIMP
nsBuffer::GetWritableAmount(PRUint32 *amount)
{
    if (mReaderClosed)
        return NS_BASE_STREAM_CLOSED;

    nsresult rv;
    PRUint32 readableAmount;
    rv = GetReadableAmount(&readableAmount);
    if (NS_FAILED(rv)) return rv;
    *amount = mMaxSize - readableAmount;
    return NS_OK;
}

NS_IMETHODIMP
nsBuffer::GetReaderClosed(PRBool *result)
{
    *result = mReaderClosed;
    return NS_OK;
}

NS_IMETHODIMP
nsBuffer::SetCondition(nsresult condition)
{
    nsAutoCMonitor mon(this);
    if (mReaderClosed)
        return NS_BASE_STREAM_CLOSED;

    mCondition = condition;
    mWriteSegment = nsnull;     // allows reader to free last segment w/o asserting
    mWriteSegmentEnd = nsnull;
    // don't reset mWriteCursor here -- we need it for the EOF point in the buffer
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kAllocatorCID, NS_ALLOCATOR_CID);

NS_COM nsresult
NS_NewBuffer(nsIBuffer* *result,
             PRUint32 growBySize, PRUint32 maxSize,
             nsIBufferObserver* observer)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIAllocator, alloc, kAllocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsBuffer* buf;
    rv = nsBuffer::Create(NULL, nsIBuffer::GetIID(), (void**)&buf);
    if (NS_FAILED(rv)) return rv;

    rv = buf->Init(growBySize, maxSize, observer, alloc);
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
                 PRUint32 growBySize, PRUint32 maxSize,
                 nsIBufferObserver* observer)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIAllocator, alloc, kPageManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsBuffer* buf;
    rv = nsBuffer::Create(NULL, nsIBuffer::GetIID(), (void**)&buf);
    if (NS_FAILED(rv)) return rv;

    rv = buf->Init(growBySize, maxSize, observer, alloc);
    if (NS_FAILED(rv)) {
        NS_RELEASE(buf);
        return rv;
    }

    *result = buf;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
