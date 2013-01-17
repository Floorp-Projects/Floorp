/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sts=4 sw=4 cin et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

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
static PRLogModuleInfo*
GetStorageStreamLog()
{
    static PRLogModuleInfo *sLog;
    if (!sLog)
        sLog = PR_NewLogModule("nsStorageStream");
    return sLog;
}
#endif
#define LOG(args) PR_LOG(GetStorageStreamLog(), PR_LOG_DEBUG, args)

nsStorageStream::nsStorageStream()
    : mSegmentedBuffer(0), mSegmentSize(0), mWriteInProgress(false),
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
nsStorageStream::Init(uint32_t segmentSize, uint32_t maxSize,
                      nsIMemory *segmentAllocator)
{
    mSegmentedBuffer = new nsSegmentedBuffer();
    if (!mSegmentedBuffer)
        return NS_ERROR_OUT_OF_MEMORY;
    
    mSegmentSize = segmentSize;
    mSegmentSizeLog2 = PR_FloorLog2(segmentSize);

    // Segment size must be a power of two
    if (mSegmentSize != ((uint32_t)1 << mSegmentSizeLog2))
        return NS_ERROR_INVALID_ARG;

    return mSegmentedBuffer->Init(segmentSize, maxSize, segmentAllocator);
}

NS_IMETHODIMP
nsStorageStream::GetOutputStream(int32_t aStartingOffset, 
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
        if (mSegmentedBuffer->ReallocLastSegment(mSegmentSize)) {
            // Need to re-Seek, since realloc changed segment base pointer
            rv = Seek(aStartingOffset);
            if (NS_FAILED(rv)) return rv;
        }

    NS_ADDREF(this);
    *aOutputStream = static_cast<nsIOutputStream*>(this);
    mWriteInProgress = true;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageStream::Close()
{
    NS_ENSURE_TRUE(mSegmentedBuffer, NS_ERROR_NOT_INITIALIZED);
    
    mWriteInProgress = false;
    
    int32_t segmentOffset = SegOffset(mLogicalLength);

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
nsStorageStream::Write(const char *aBuffer, uint32_t aCount, uint32_t *aNumWritten)
{
    NS_ENSURE_TRUE(mSegmentedBuffer, NS_ERROR_NOT_INITIALIZED);
    
    const char* readCursor;
    uint32_t count, availableInSegment, remaining;
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
    while (remaining || MOZ_UNLIKELY(firstTime)) {
        firstTime = false;
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
	
        count = XPCOM_MIN(availableInSegment, remaining);
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
nsStorageStream::WriteFrom(nsIInputStream *inStr, uint32_t count, uint32_t *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsStorageStream::WriteSegments(nsReadSegmentFun reader, void * closure, uint32_t count, uint32_t *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsStorageStream::IsNonBlocking(bool *aNonBlocking)
{
    *aNonBlocking = false;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageStream::GetLength(uint32_t *aLength)
{
    NS_ENSURE_ARG(aLength);
    *aLength = mLogicalLength;
    return NS_OK;
}

// Truncate the buffer by deleting the end segments
NS_IMETHODIMP
nsStorageStream::SetLength(uint32_t aLength)
{
    NS_ENSURE_TRUE(mSegmentedBuffer, NS_ERROR_NOT_INITIALIZED);
    
    if (mWriteInProgress)
        return NS_ERROR_NOT_AVAILABLE;

    if (aLength > mLogicalLength)
        return NS_ERROR_INVALID_ARG;

    int32_t newLastSegmentNum = SegNum(aLength);
    int32_t segmentOffset = SegOffset(aLength);
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
nsStorageStream::Seek(int32_t aPosition)
{
    NS_ENSURE_TRUE(mSegmentedBuffer, NS_ERROR_NOT_INITIALIZED);
    
    // An argument of -1 means "seek to end of stream"
    if (aPosition == -1)
        aPosition = mLogicalLength;

    // Seeking beyond the buffer end is illegal
    if ((uint32_t)aPosition > mLogicalLength)
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
    int32_t segmentOffset = SegOffset(aPosition);
    if (segmentOffset == 0 && (SegNum(aPosition) > (uint32_t) mLastSegmentNum))
        mWriteCursor = mSegmentEnd;
    else
        mWriteCursor += segmentOffset;
    
    LOG(("nsStorageStream [%p] Seek mWriteCursor=%x mSegmentEnd=%x\n",
        this, mWriteCursor, mSegmentEnd));
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

// There can be many nsStorageInputStreams for a single nsStorageStream
class nsStorageInputStream MOZ_FINAL : public nsIInputStream
                                     , public nsISeekableStream
{
public:
    nsStorageInputStream(nsStorageStream *aStorageStream, uint32_t aSegmentSize)
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
    NS_METHOD Seek(uint32_t aPosition);

    friend class nsStorageStream;

private:
    nsStorageStream* mStorageStream;
    uint32_t         mReadCursor;    // Next memory location to read byte, or NULL
    uint32_t         mSegmentEnd;    // One byte past end of current buffer segment
    uint32_t         mSegmentNum;    // Segment number containing read cursor
    uint32_t         mSegmentSize;   // All segments, except the last, are of this size
    uint32_t         mLogicalCursor; // Logical offset into stream
    nsresult         mStatus;

    uint32_t SegNum(uint32_t aPosition)    {return aPosition >> mStorageStream->mSegmentSizeLog2;}
    uint32_t SegOffset(uint32_t aPosition) {return aPosition & (mSegmentSize - 1);}
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsStorageInputStream,
                              nsIInputStream,
                              nsISeekableStream)

NS_IMETHODIMP
nsStorageStream::NewInputStream(int32_t aStartingOffset, nsIInputStream* *aInputStream)
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
nsStorageInputStream::Available(uint64_t *aAvailable)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    *aAvailable = mStorageStream->mLogicalLength - mLogicalCursor;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::Read(char* aBuffer, uint32_t aCount, uint32_t *aNumRead)
{
    return ReadSegments(NS_CopySegmentToBuffer, aBuffer, aCount, aNumRead);
}

NS_IMETHODIMP 
nsStorageInputStream::ReadSegments(nsWriteSegmentFun writer, void * closure, uint32_t aCount, uint32_t *aNumRead)
{
    *aNumRead = 0;
    if (mStatus == NS_BASE_STREAM_CLOSED)
        return NS_OK;
    if (NS_FAILED(mStatus))
        return mStatus;

    uint32_t count, availableInSegment, remainingCapacity, bytesConsumed;
    nsresult rv;

    remainingCapacity = aCount;
    while (remainingCapacity) {
        availableInSegment = mSegmentEnd - mReadCursor;
        if (!availableInSegment) {
            uint32_t available = mStorageStream->mLogicalLength - mLogicalCursor;
            if (!available)
                goto out;

            mSegmentNum++;
            mReadCursor = 0;
            mSegmentEnd = XPCOM_MIN(mSegmentSize, available);
            availableInSegment = mSegmentEnd;
        }
        const char *cur = mStorageStream->mSegmentedBuffer->GetSegment(mSegmentNum);
	
        count = XPCOM_MIN(availableInSegment, remainingCapacity);
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
        isWriteInProgress = false;

    if (*aNumRead == 0 && isWriteInProgress)
        return NS_BASE_STREAM_WOULD_BLOCK;

    return NS_OK;
}

NS_IMETHODIMP 
nsStorageInputStream::IsNonBlocking(bool *aNonBlocking)
{
    // TODO: This class should implement nsIAsyncInputStream so that callers
    // have some way of dealing with NS_BASE_STREAM_WOULD_BLOCK errors.
 
    *aNonBlocking = true;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::Seek(int32_t aWhence, int64_t aOffset)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    int64_t pos = aOffset;

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
    if (pos == int64_t(mLogicalCursor))
        return NS_OK;

    return Seek(pos);
}

NS_IMETHODIMP
nsStorageInputStream::Tell(int64_t *aResult)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    *aResult = mLogicalCursor;
    return NS_OK;
}

NS_IMETHODIMP
nsStorageInputStream::SetEOF()
{
    NS_NOTREACHED("nsStorageInputStream::SetEOF");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsStorageInputStream::Seek(uint32_t aPosition)
{
    uint32_t length = mStorageStream->mLogicalLength;
    if (aPosition > length)
        return NS_ERROR_INVALID_ARG;

    if (length == 0)
        return NS_OK;

    mSegmentNum = SegNum(aPosition);
    mReadCursor = SegOffset(aPosition);
    uint32_t available = length - aPosition;
    mSegmentEnd = mReadCursor + XPCOM_MIN(mSegmentSize - mReadCursor, available);
    mLogicalCursor = aPosition;
    return NS_OK;
}

nsresult
NS_NewStorageStream(uint32_t segmentSize, uint32_t maxSize, nsIStorageStream **result)
{
    NS_ENSURE_ARG(result);

    nsStorageStream* storageStream = new nsStorageStream();
    if (!storageStream) return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(storageStream);
    nsresult rv = storageStream->Init(segmentSize, maxSize, nullptr);
    if (NS_FAILED(rv)) {
        NS_RELEASE(storageStream);
        return rv;
    }
    *result = storageStream;
    return NS_OK;
}
