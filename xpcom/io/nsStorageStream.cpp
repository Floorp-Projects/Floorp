/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sts=4 sw=4 cin et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * The storage stream provides an internal buffer that can be filled by a
 * client using a single output stream.  One or more independent input streams
 * can be created to read the data out non-destructively.  The implementation
 * uses a segmented buffer internally to avoid realloc'ing of large buffers,
 * with the attendant performance loss and heap fragmentation.
 */

#include "nsAlgorithm.h"
#include "nsStorageStream.h"
#include "nsSegmentedBuffer.h"
#include "nsStreamUtils.h"
#include "nsCOMPtr.h"
#include "prbit.h"
#include "nsIInputStream.h"
#include "nsISeekableStream.h"
#include "prlog.h"

#if defined(PR_LOGGING)
//
// Log module for StorageStream logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=StorageStreamLog:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
static PRLogModuleInfo* sLog = PR_NewLogModule("nsStorageStream");
#endif
#define LOG(args) PR_LOG(sLog, PR_LOG_DEBUG, args)

nsStorageStream::nsStorageStream()
    : mSegmentedBuffer(0), mSegmentSize(0), mWriteInProgress(PR_FALSE),
      mLastSegmentNum(-1), mWriteCursor(0), mSegmentEnd(0), mLogicalLength(0)
{
    LOG(("Creating nsStorageStream [%p].\n", this));
}

nsStorageStream::~nsStorageStream()
{
    delete mSegmentedBuffer;
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsStorageStream,
                              nsIStorageStream,
                              nsIOutputStream)

NS_IMETHODIMP
nsStorageStream::Init(PRUint32 segmentSize, PRUint32 maxSize,
                      nsIMemory *segmentAllocator)
{
    mSegmentedBuffer = new nsSegmentedBuffer();
    if (!mSegmentedBuffer)
        return NS_ERROR_OUT_OF_MEMORY;
    
    mSegmentSize = segmentSize;
    mSegmentSizeLog2 = PR_FloorLog2(segmentSize);

    // Segment size must be a power of two
    if (mSegmentSize != ((PRUint32)1 << mSegmentSizeLog2))
        return NS_ERROR_INVALID_ARG;

    return mSegmentedBuffer->Init(segmentSize, maxSize, segmentAllocator);
}

NS_IMETHODIMP
nsStorageStream::GetOutputStream(PRInt32 aStartingOffset, 
                                 nsIOutputStream * *aOutputStream)
{
    NS_ENSURE_ARG(aOutputStream);
    NS_ENSURE_TRUE(mSegmentedBuffer, NS_ERROR_NOT_INITIALIZED);
    
    if (mWriteInProgress)
        return NS_ERROR_NOT_AVAILABLE;

    nsresult rv = Seek(aStartingOffset);
    if (NS_FAILED(rv)) return rv;

    // Enlarge the last segment in the buffer so that it is the same size as
    // all the other segments in the buffer.  (It may have been realloc'ed
    // smaller in the Close() method.)
    if (mLastSegmentNum >= 0)
        mSegmentedBuffer->ReallocLastSegment(mSegmentSize);

    // Need to re-Seek, since realloc might have changed segment base pointer
    rv = Seek(aStartingOffset);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(this);
    *aOutputStream = static_cast<nsIOutputStream*>(this);
    mWriteInProgress = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageStream::Close()
{
    NS_ENSURE_TRUE(mSegmentedBuffer, NS_ERROR_NOT_INITIALIZED);
    
    mWriteInProgress = PR_FALSE;
    
    PRInt32 segmentOffset = SegOffset(mLogicalLength);

    // Shrink the final segment in the segmented buffer to the minimum size
    // needed to contain the data, so as to conserve memory.
    if (segmentOffset)
        mSegmentedBuffer->ReallocLastSegment(segmentOffset);
    
    mWriteCursor = 0;
    mSegmentEnd = 0;

    LOG(("nsStorageStream [%p] Close mWriteCursor=%x mSegmentEnd=%x\n",
        this, mWriteCursor, mSegmentEnd));

    return NS_OK;
}

NS_IMETHODIMP
nsStorageStream::Flush()
{
    return NS_OK;
}

NS_IMETHODIMP
nsStorageStream::Write(const char *aBuffer, PRUint32 aCount, PRUint32 *aNumWritten)
{
    NS_ENSURE_TRUE(mSegmentedBuffer, NS_ERROR_NOT_INITIALIZED);
    
    const char* readCursor;
    PRUint32 count, availableInSegment, remaining;
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(aNumWritten);
    NS_ENSURE_ARG(aBuffer);

    LOG(("nsStorageStream [%p] Write mWriteCursor=%x mSegmentEnd=%x aCount=%d\n",
        this, mWriteCursor, mSegmentEnd, aCount));

    remaining = aCount;
    readCursor = aBuffer;
    // If no segments have been created yet, create one even if we don't have
    // to write any data; this enables creating an input stream which reads from
    // the very end of the data for any amount of data in the stream (i.e.
    // this stream contains N bytes of data and newInputStream(N) is called),
    // even for N=0 (with the caveat that we require .write("", 0) be called to
    // initialize internal buffers).
    bool firstTime = mSegmentedBuffer->GetSegmentCount() == 0;
    while (remaining || NS_UNLIKELY(firstTime)) {
        firstTime = PR_FALSE;
        availableInSegment = mSegmentEnd - mWriteCursor;
        if (!availableInSegment) {
            mWriteCursor = mSegmentedBuffer->AppendNewSegment();
            if (!mWriteCursor) {
                mSegmentEnd = 0;
                rv = NS_ERROR_OUT_OF_MEMORY;
                goto out;
            }
            mLastSegmentNum++;
            mSegmentEnd = mWriteCursor + mSegmentSize;
            availableInSegment = mSegmentEnd - mWriteCursor;
            LOG(("nsStorageStream [%p] Write (new seg) mWriteCursor=%x mSegmentEnd=%x\n",
                this, mWriteCursor, mSegmentEnd));
        }
	
        count = NS_MIN(availableInSegment, remaining);
        memcpy(mWriteCursor, readCursor, count);
        remaining -= count;
        readCursor += count;
        mWriteCursor += count;
        LOG(("nsStorageStream [%p] Writing mWriteCursor=%x mSegmentEnd=%x count=%d\n",
            this, mWriteCursor, mSegmentEnd, count));
    };

 out:
    *aNumWritten = aCount - remaining;
    mLogicalLength += *aNumWritten;

    LOG(("nsStorageStream [%p] Wrote mWriteCursor=%x mSegmentEnd=%x numWritten=%d\n",
        this, mWriteCursor, mSegmentEnd, *aNumWritten));
    return rv;
}

NS_IMETHODIMP 
nsStorageStream::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsStorageStream::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsStorageStream::IsNonBlocking(bool *aNonBlocking)
{
    *aNonBlocking = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageStream::GetLength(PRUint32 *aLength)
{
    NS_ENSURE_ARG(aLength);
    *aLength = mLogicalLength;
    return NS_OK;
}

// Truncate the buffer by deleting the end segments
NS_IMETHODIMP
nsStorageStream::SetLength(PRUint32 aLength)
{
    NS_ENSURE_TRUE(mSegmentedBuffer, NS_ERROR_NOT_INITIALIZED);
    
    if (mWriteInProgress)
        return NS_ERROR_NOT_AVAILABLE;

    if (aLength > mLogicalLength)
        return NS_ERROR_INVALID_ARG;

    PRInt32 newLastSegmentNum = SegNum(aLength);
    PRInt32 segmentOffset = SegOffset(aLength);
    if (segmentOffset == 0)
        newLastSegmentNum--;

    while (newLastSegmentNum < mLastSegmentNum) {
        mSegmentedBuffer->DeleteLastSegment();
        mLastSegmentNum--;
    }

    mLogicalLength = aLength;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageStream::GetWriteInProgress(bool *aWriteInProgress)
{
    NS_ENSURE_ARG(aWriteInProgress);

    *aWriteInProgress = mWriteInProgress;
    return NS_OK;
}

NS_METHOD
nsStorageStream::Seek(PRInt32 aPosition)
{
    NS_ENSURE_TRUE(mSegmentedBuffer, NS_ERROR_NOT_INITIALIZED);
    
    // An argument of -1 means "seek to end of stream"
    if (aPosition == -1)
        aPosition = mLogicalLength;

    // Seeking beyond the buffer end is illegal
    if ((PRUint32)aPosition > mLogicalLength)
        return NS_ERROR_INVALID_ARG;

    // Seeking backwards in the write stream results in truncation
    SetLength(aPosition);

    // Special handling for seek to start-of-buffer
    if (aPosition == 0) {
        mWriteCursor = 0;
        mSegmentEnd = 0;
        LOG(("nsStorageStream [%p] Seek mWriteCursor=%x mSegmentEnd=%x\n",
            this, mWriteCursor, mSegmentEnd));
        return NS_OK;
    }

    // Segment may have changed, so reset pointers
    mWriteCursor = mSegmentedBuffer->GetSegment(mLastSegmentNum);
    NS_ASSERTION(mWriteCursor, "null mWriteCursor");
    mSegmentEnd = mWriteCursor + mSegmentSize;

    // Adjust write cursor for current segment offset.  This test is necessary
    // because SegNum may reference the next-to-be-allocated segment, in which
    // case we need to be pointing at the end of the last segment.
    PRInt32 segmentOffset = SegOffset(aPosition);
    if (segmentOffset == 0 && (SegNum(aPosition) > (PRUint32) mLastSegmentNum))
        mWriteCursor = mSegmentEnd;
    else
        mWriteCursor += segmentOffset;
    
    LOG(("nsStorageStream [%p] Seek mWriteCursor=%x mSegmentEnd=%x\n",
        this, mWriteCursor, mSegmentEnd));
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

// There can be many nsStorageInputStreams for a single nsStorageStream
class nsStorageInputStream : public nsIInputStream
                           , public nsISeekableStream
{
public:
    nsStorageInputStream(nsStorageStream *aStorageStream, PRUint32 aSegmentSize)
        : mStorageStream(aStorageStream), mReadCursor(0),
          mSegmentEnd(0), mSegmentNum(0),
          mSegmentSize(aSegmentSize), mLogicalCursor(0),
          mStatus(NS_OK)
	{
        NS_ADDREF(mStorageStream);
	}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM

private:
    ~nsStorageInputStream()
    {
        NS_IF_RELEASE(mStorageStream);
    }

protected:
    NS_METHOD Seek(PRUint32 aPosition);

    friend class nsStorageStream;

private:
    nsStorageStream* mStorageStream;
    PRUint32         mReadCursor;    // Next memory location to read byte, or NULL
    PRUint32         mSegmentEnd;    // One byte past end of current buffer segment
    PRUint32         mSegmentNum;    // Segment number containing read cursor
    PRUint32         mSegmentSize;   // All segments, except the last, are of this size
    PRUint32         mLogicalCursor; // Logical offset into stream
    nsresult         mStatus;

    PRUint32 SegNum(PRUint32 aPosition)    {return aPosition >> mStorageStream->mSegmentSizeLog2;}
    PRUint32 SegOffset(PRUint32 aPosition) {return aPosition & (mSegmentSize - 1);}
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsStorageInputStream,
                              nsIInputStream,
                              nsISeekableStream)

NS_IMETHODIMP
nsStorageStream::NewInputStream(PRInt32 aStartingOffset, nsIInputStream* *aInputStream)
{
    NS_ENSURE_TRUE(mSegmentedBuffer, NS_ERROR_NOT_INITIALIZED);

    nsStorageInputStream *inputStream = new nsStorageInputStream(this, mSegmentSize);
    if (!inputStream)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(inputStream);

    nsresult rv = inputStream->Seek(aStartingOffset);
    if (NS_FAILED(rv)) {
        NS_RELEASE(inputStream);
        return rv;
    }

    *aInputStream = inputStream;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::Close()
{
    mStatus = NS_BASE_STREAM_CLOSED;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::Available(PRUint32 *aAvailable)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    *aAvailable = mStorageStream->mLogicalLength - mLogicalCursor;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::Read(char* aBuffer, PRUint32 aCount, PRUint32 *aNumRead)
{
    return ReadSegments(NS_CopySegmentToBuffer, aBuffer, aCount, aNumRead);
}

NS_IMETHODIMP 
nsStorageInputStream::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 aCount, PRUint32 *aNumRead)
{
    *aNumRead = 0;
    if (mStatus == NS_BASE_STREAM_CLOSED)
        return NS_OK;
    if (NS_FAILED(mStatus))
        return mStatus;

    PRUint32 count, availableInSegment, remainingCapacity, bytesConsumed;
    nsresult rv;

    remainingCapacity = aCount;
    while (remainingCapacity) {
        availableInSegment = mSegmentEnd - mReadCursor;
        if (!availableInSegment) {
            PRUint32 available = mStorageStream->mLogicalLength - mLogicalCursor;
            if (!available)
                goto out;

            mSegmentNum++;
            mReadCursor = 0;
            mSegmentEnd = NS_MIN(mSegmentSize, available);
            availableInSegment = mSegmentEnd;
        }
        const char *cur = mStorageStream->mSegmentedBuffer->GetSegment(mSegmentNum);
	
        count = NS_MIN(availableInSegment, remainingCapacity);
        rv = writer(this, closure, cur + mReadCursor, aCount - remainingCapacity,
                    count, &bytesConsumed);
        if (NS_FAILED(rv) || (bytesConsumed == 0))
          break;
        remainingCapacity -= bytesConsumed;
        mReadCursor += bytesConsumed;
        mLogicalCursor += bytesConsumed;
    };

 out:
    *aNumRead = aCount - remainingCapacity;

    bool isWriteInProgress = false;
    if (NS_FAILED(mStorageStream->GetWriteInProgress(&isWriteInProgress)))
        isWriteInProgress = PR_FALSE;

    if (*aNumRead == 0 && isWriteInProgress)
        return NS_BASE_STREAM_WOULD_BLOCK;

    return NS_OK;
}

NS_IMETHODIMP 
nsStorageInputStream::IsNonBlocking(bool *aNonBlocking)
{
    // TODO: This class should implement nsIAsyncInputStream so that callers
    // have some way of dealing with NS_BASE_STREAM_WOULD_BLOCK errors.
 
    *aNonBlocking = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    PRInt64 pos = aOffset;

    switch (aWhence) {
    case NS_SEEK_SET:
        break;
    case NS_SEEK_CUR:
        pos += mLogicalCursor;
        break;
    case NS_SEEK_END:
        pos += mStorageStream->mLogicalLength;
        break;
    default:
        NS_NOTREACHED("unexpected whence value");
        return NS_ERROR_UNEXPECTED;
    }
    if (pos == PRInt64(mLogicalCursor))
        return NS_OK;

    return Seek(pos);
}

NS_IMETHODIMP
nsStorageInputStream::Tell(PRInt64 *aResult)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    LL_UI2L(*aResult, mLogicalCursor);
    return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::SetEOF()
{
    NS_NOTREACHED("nsStorageInputStream::SetEOF");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsStorageInputStream::Seek(PRUint32 aPosition)
{
    PRUint32 length = mStorageStream->mLogicalLength;
    if (aPosition > length)
        return NS_ERROR_INVALID_ARG;

    if (length == 0)
        return NS_OK;

    mSegmentNum = SegNum(aPosition);
    mReadCursor = SegOffset(aPosition);
    PRUint32 available = length - aPosition;
    mSegmentEnd = mReadCursor + NS_MIN(mSegmentSize - mReadCursor, available);
    mLogicalCursor = aPosition;
    return NS_OK;
}

nsresult
NS_NewStorageStream(PRUint32 segmentSize, PRUint32 maxSize, nsIStorageStream **result)
{
    NS_ENSURE_ARG(result);

    nsStorageStream* storageStream = new nsStorageStream();
    if (!storageStream) return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(storageStream);
    nsresult rv = storageStream->Init(segmentSize, maxSize, nsnull);
    if (NS_FAILED(rv)) {
        NS_RELEASE(storageStream);
        return rv;
    }
    *result = storageStream;
    return NS_OK;
}
