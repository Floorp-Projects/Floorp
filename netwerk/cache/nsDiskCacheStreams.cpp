/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsCache.h"
#include "nsDiskCache.h"
#include "nsDiskCacheDevice.h"
#include "nsDiskCacheStreams.h"
#include "nsCacheService.h"
#include "mozilla/FileUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include <algorithm>


// we pick 16k as the max buffer size because that is the threshold above which
//      we are unable to store the data in the cache block files
//      see nsDiskCacheMap.[cpp,h]
#define kMaxBufferSize      (16 * 1024)

// Assumptions:
//      - cache descriptors live for life of streams
//      - streams will only be used by FileTransport,
//         they will not be directly accessible to clients
//      - overlapped I/O is NOT supported


/******************************************************************************
 *  nsDiskCacheInputStream
 *****************************************************************************/
class nsDiskCacheInputStream : public nsIInputStream {

public:

    nsDiskCacheInputStream( nsDiskCacheStreamIO * parent,
                            PRFileDesc *          fileDesc,
                            const char *          buffer,
                            uint32_t              endOfStream);

    virtual ~nsDiskCacheInputStream();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM

private:
    nsDiskCacheStreamIO *           mStreamIO;  // backpointer to parent
    PRFileDesc *                    mFD;
    const char *                    mBuffer;
    uint32_t                        mStreamEnd;
    uint32_t                        mPos;       // stream position
    bool                            mClosed;
};


NS_IMPL_THREADSAFE_ISUPPORTS1(nsDiskCacheInputStream, nsIInputStream)


nsDiskCacheInputStream::nsDiskCacheInputStream( nsDiskCacheStreamIO * parent,
                                                PRFileDesc *          fileDesc,
                                                const char *          buffer,
                                                uint32_t              endOfStream)
    : mStreamIO(parent)
    , mFD(fileDesc)
    , mBuffer(buffer)
    , mStreamEnd(endOfStream)
    , mPos(0)
    , mClosed(false)
{
    NS_ADDREF(mStreamIO);
    mStreamIO->IncrementInputStreamCount();
}


nsDiskCacheInputStream::~nsDiskCacheInputStream()
{
    Close();
    mStreamIO->DecrementInputStreamCount();
    NS_RELEASE(mStreamIO);
}


NS_IMETHODIMP
nsDiskCacheInputStream::Close()
{
    if (!mClosed) {
        if (mFD) {
            (void) PR_Close(mFD);
            mFD = nullptr;
        }
        mClosed = true;
    }
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheInputStream::Available(uint64_t * bytesAvailable)
{
    if (mClosed)  return NS_BASE_STREAM_CLOSED;
    if (mStreamEnd < mPos)  return NS_ERROR_UNEXPECTED;
    
    *bytesAvailable = mStreamEnd - mPos;
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheInputStream::Read(char * buffer, uint32_t count, uint32_t * bytesRead)
{
    *bytesRead = 0;

    if (mClosed) {
        CACHE_LOG_DEBUG(("CACHE: nsDiskCacheInputStream::Read "
                         "[stream=%p] stream was closed",
                         this, buffer, count));
        return NS_OK;
    }
    
    if (mPos == mStreamEnd) {
        CACHE_LOG_DEBUG(("CACHE: nsDiskCacheInputStream::Read "
                         "[stream=%p] stream at end of file",
                         this, buffer, count));
        return NS_OK;
    }
    if (mPos > mStreamEnd) {
        CACHE_LOG_DEBUG(("CACHE: nsDiskCacheInputStream::Read "
                         "[stream=%p] stream past end of file (!)",
                         this, buffer, count));
        return NS_ERROR_UNEXPECTED;
    }
    
    if (count > mStreamEnd - mPos)
        count = mStreamEnd - mPos;

    if (mFD) {
        // just read from file
        int32_t  result = PR_Read(mFD, buffer, count);
        if (result < 0) {
            nsresult rv = NS_ErrorAccordingToNSPR();
            CACHE_LOG_DEBUG(("CACHE: nsDiskCacheInputStream::Read PR_Read failed"
                             "[stream=%p, rv=%d, NSPR error %s",
                             this, int(rv), PR_ErrorToName(PR_GetError())));
            return rv;
        }
        
        mPos += (uint32_t)result;
        *bytesRead = (uint32_t)result;
        
    } else if (mBuffer) {
        // read data from mBuffer
        memcpy(buffer, mBuffer + mPos, count);
        mPos += count;
        *bytesRead = count;
    } else {
        // no data source for input stream
    }

    CACHE_LOG_DEBUG(("CACHE: nsDiskCacheInputStream::Read "
                     "[stream=%p, count=%ud, byteRead=%ud] ",
                     this, unsigned(count), unsigned(*bytesRead)));
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheInputStream::ReadSegments(nsWriteSegmentFun writer,
                                     void *            closure,
                                     uint32_t          count,
                                     uint32_t *        bytesRead)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDiskCacheInputStream::IsNonBlocking(bool * nonBlocking)
{
    *nonBlocking = false;
    return NS_OK;
}


/******************************************************************************
 *  nsDiskCacheOutputStream
 *****************************************************************************/
class nsDiskCacheOutputStream : public nsIOutputStream
{
public:
    nsDiskCacheOutputStream( nsDiskCacheStreamIO * parent);
    virtual ~nsDiskCacheOutputStream();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOUTPUTSTREAM

    void ReleaseStreamIO() { NS_IF_RELEASE(mStreamIO); }

private:
    nsDiskCacheStreamIO *           mStreamIO;  // backpointer to parent
    bool                            mClosed;
};


NS_IMPL_THREADSAFE_ISUPPORTS1(nsDiskCacheOutputStream,
                              nsIOutputStream)

nsDiskCacheOutputStream::nsDiskCacheOutputStream( nsDiskCacheStreamIO * parent)
    : mStreamIO(parent)
    , mClosed(false)
{
    NS_ADDREF(mStreamIO);
}


nsDiskCacheOutputStream::~nsDiskCacheOutputStream()
{
    Close();
    ReleaseStreamIO();
}


NS_IMETHODIMP
nsDiskCacheOutputStream::Close()
{
    nsresult rv = NS_OK;
    mozilla::TimeStamp start = mozilla::TimeStamp::Now();

    if (!mClosed) {
        mClosed = true;
        // tell parent streamIO we are closing
        rv = mStreamIO->CloseOutputStream(this);
    }

    mozilla::Telemetry::ID id;
    if (NS_IsMainThread())
        id = mozilla::Telemetry::NETWORK_DISK_CACHE_OUTPUT_STREAM_CLOSE_MAIN_THREAD;
    else
        id = mozilla::Telemetry::NETWORK_DISK_CACHE_OUTPUT_STREAM_CLOSE;

    mozilla::Telemetry::AccumulateTimeDelta(id, start);

    return rv;
}

NS_IMETHODIMP
nsDiskCacheOutputStream::Flush()
{
    if (mClosed)  return NS_BASE_STREAM_CLOSED;
    // yeah, yeah, well get to it...eventually...
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheOutputStream::Write(const char *buf, uint32_t count, uint32_t *bytesWritten)
{
    if (mClosed)  return NS_BASE_STREAM_CLOSED;
    return mStreamIO->Write(buf, count, bytesWritten);
}


NS_IMETHODIMP
nsDiskCacheOutputStream::WriteFrom(nsIInputStream *inStream, uint32_t count, uint32_t *bytesWritten)
{
    NS_NOTREACHED("WriteFrom");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDiskCacheOutputStream::WriteSegments( nsReadSegmentFun reader,
                                        void *           closure,
                                        uint32_t         count,
                                        uint32_t *       bytesWritten)
{
    NS_NOTREACHED("WriteSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDiskCacheOutputStream::IsNonBlocking(bool * nonBlocking)
{
    *nonBlocking = false;
    return NS_OK;
}



/******************************************************************************
 *  nsDiskCacheStreamIO
 *****************************************************************************/
NS_IMPL_THREADSAFE_ISUPPORTS0(nsDiskCacheStreamIO)

nsDiskCacheStreamIO::nsDiskCacheStreamIO(nsDiskCacheBinding *   binding)
    : mBinding(binding)
    , mOutStream(nullptr)
    , mInStreamCount(0)
    , mFD(nullptr)
    , mStreamEnd(0)
    , mBufSize(0)
    , mBuffer(nullptr)
{
    mDevice = (nsDiskCacheDevice *)mBinding->mCacheEntry->CacheDevice();

    // acquire "death grip" on cache service
    nsCacheService *service = nsCacheService::GlobalInstance();
    NS_ADDREF(service);
}


nsDiskCacheStreamIO::~nsDiskCacheStreamIO()
{
    Close();

    // release "death grip" on cache service
    nsCacheService *service = nsCacheService::GlobalInstance();
    NS_RELEASE(service);
}


void
nsDiskCacheStreamIO::Close()
{
    // this should only be called from our destructor
    // no one is interested in us anymore, so we don't need to grab any locks
    
    // assert streams closed
    NS_ASSERTION(!mOutStream, "output stream still open");
    NS_ASSERTION(mInStreamCount == 0, "input stream still open");
    NS_ASSERTION(!mFD, "file descriptor not closed");

    DeleteBuffer();
}


// NOTE: called with service lock held
nsresult
nsDiskCacheStreamIO::GetInputStream(uint32_t offset, nsIInputStream ** inputStream)
{
    NS_ENSURE_ARG_POINTER(inputStream);
    NS_ENSURE_TRUE(offset == 0, NS_ERROR_NOT_IMPLEMENTED);

    *inputStream = nullptr;
    
    if (!mBinding)  return NS_ERROR_NOT_AVAILABLE;

    if (mOutStream) {
        NS_WARNING("already have an output stream open");
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsresult            rv;
    PRFileDesc *        fd = nullptr;

    mStreamEnd = mBinding->mCacheEntry->DataSize();
    if (mStreamEnd == 0) {
        // there's no data to read
        NS_ASSERTION(!mBinding->mRecord.DataLocationInitialized(), "storage allocated for zero data size");
    } else if (mBinding->mRecord.DataFile() == 0) {
        // open file desc for data
        rv = OpenCacheFile(PR_RDONLY, &fd);
        if (NS_FAILED(rv))  return rv;  // unable to open file        
        NS_ASSERTION(fd, "cache stream lacking open file.");
            
    } else if (!mBuffer) {
        // read block file for data
        rv = ReadCacheBlocks(mStreamEnd);
        if (NS_FAILED(rv))  return rv;
    }
    // else, mBuffer already contains all of the data (left over from a
    // previous block-file read or write).

    NS_ASSERTION(!(fd && mBuffer), "ambiguous data sources for input stream");

    // create a new input stream
    nsDiskCacheInputStream * inStream = new nsDiskCacheInputStream(this, fd, mBuffer, mStreamEnd);
    if (!inStream)  return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(*inputStream = inStream);
    return NS_OK;
}


// NOTE: called with service lock held
nsresult
nsDiskCacheStreamIO::GetOutputStream(uint32_t offset, nsIOutputStream ** outputStream)
{
    NS_ENSURE_ARG_POINTER(outputStream);
    *outputStream = nullptr;

    if (!mBinding)  return NS_ERROR_NOT_AVAILABLE;
        
    NS_ASSERTION(!mOutStream, "already have an output stream open");
    NS_ASSERTION(mInStreamCount == 0, "we already have input streams open");
    if (mOutStream || mInStreamCount)  return NS_ERROR_NOT_AVAILABLE;
    
    mStreamEnd = mBinding->mCacheEntry->DataSize();

    // Inits file or buffer and truncate at the desired offset
    nsresult rv = SeekAndTruncate(offset);
    if (NS_FAILED(rv)) return rv;

    // create a new output stream
    mOutStream = new nsDiskCacheOutputStream(this);
    if (!mOutStream)  return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(*outputStream = mOutStream);
    return NS_OK;
}

nsresult
nsDiskCacheStreamIO::ClearBinding()
{
    nsresult rv = NS_OK;
    if (mBinding && mOutStream)
        rv = Flush();
    mBinding = nullptr;
    return rv;
}

nsresult
nsDiskCacheStreamIO::CloseOutputStream(nsDiskCacheOutputStream *  outputStream)
{
    nsCacheServiceAutoLock lock(LOCK_TELEM(NSDISKCACHESTREAMIO_CLOSEOUTPUTSTREAM)); // grab service lock

    if (outputStream != mOutStream) {
        NS_WARNING("mismatched output streams");
        return NS_ERROR_UNEXPECTED;
    }

    // output stream is closing
    if (!mBinding) {    // if we're severed, just clear member variables
        mOutStream = nullptr;
        outputStream->ReleaseStreamIO();
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsresult rv = Flush();
    if (NS_FAILED(rv))
        NS_WARNING("Flush() failed");

    mOutStream = nullptr;
    return rv;
}

nsresult
nsDiskCacheStreamIO::Flush()
{
    NS_ASSERTION(mBinding, "oops");

    CACHE_LOG_DEBUG(("CACHE: Flush [%x doomed=%u]\n",
        mBinding->mRecord.HashNumber(), mBinding->mDoomed));

    // When writing to a file, just close the file
    if (mFD) {
        (void) PR_Close(mFD);
        mFD = nullptr;
        return NS_OK;
    }

    // write data to cache blocks, or flush mBuffer to file
    nsDiskCacheMap *cacheMap = mDevice->CacheMap();  // get map reference
    nsresult rv;

    bool written = false;

    if (mStreamEnd <= kMaxBufferSize) {
        // store data (if any) in cache block files

        // delete existing storage
        nsDiskCacheRecord * record = &mBinding->mRecord;
        if (record->DataLocationInitialized()) {
            rv = cacheMap->DeleteStorage(record, nsDiskCache::kData);
            if (NS_FAILED(rv)) {
                NS_WARNING("cacheMap->DeleteStorage() failed.");
                return rv;
            }
        }

        // flush buffer to block files
        written = true;
        if (mStreamEnd > 0) {
            rv = cacheMap->WriteDataCacheBlocks(mBinding, mBuffer, mStreamEnd);
            if (NS_FAILED(rv)) {
                NS_WARNING("WriteDataCacheBlocks() failed.");
                written = false;
            }
        }
    }

    if (!written) {
        // failed to store in cacheblocks, save as separate file
        rv = FlushBufferToFile(); // initializes DataFileLocation() if necessary

        if (mFD) {
          // Update the file size of the disk file in the cache
          UpdateFileSize();

          // close file descriptor
          (void) PR_Close(mFD);
          mFD = nullptr;
        }
        else
          NS_WARNING("no file descriptor");

        // close mFD first if possible before returning if FlushBufferToFile
        // failed
        NS_ENSURE_SUCCESS(rv, rv);
    }
    
    // XXX do we need this here?  WriteDataCacheBlocks() calls UpdateRecord()
    // update cache map if entry isn't doomed
    if (!mBinding->mDoomed) {
        rv = cacheMap->UpdateRecord(&mBinding->mRecord);
        if (NS_FAILED(rv)) {
            NS_WARNING("cacheMap->UpdateRecord() failed.");
            return rv;   // XXX doom cache entry
        }
    }
    
    return NS_OK;
}


// assumptions:
//      only one thread writing at a time
//      never have both output and input streams open
//      OnDataSizeChanged() will have already been called to update entry->DataSize()

nsresult
nsDiskCacheStreamIO::Write( const char * buffer,
                            uint32_t     count,
                            uint32_t *   bytesWritten)
{
    // grab service lock
    nsCacheServiceAutoLock lock(LOCK_TELEM(NSDISKCACHESTREAMIO_WRITE));
    if (!mBinding)  return NS_ERROR_NOT_AVAILABLE;

    if (mInStreamCount) {
        // we have open input streams already
        // this is an error until we support overlapped I/O
        NS_WARNING("Attempting to write to cache entry with open input streams.\n");
        return NS_ERROR_NOT_AVAILABLE;
    }

    NS_ASSERTION(count, "Write called with count of zero");

    // Not writing to file, and it will fit in the cachedatablocks?
    if (!mFD && (mStreamEnd + count <= kMaxBufferSize)) {

        // We have more data than the current buffer size?
        if ((mStreamEnd + count > mBufSize) && (mBufSize < kMaxBufferSize)) {
            // Try to increase buffer to the max size, no problem if not 
            // succesful, we just use what is available.
            char *newbuf = (char *) realloc(mBuffer, kMaxBufferSize);
            if (newbuf) {
                // Use the new larger buffer
                mBuffer = newbuf;
                mBufSize = kMaxBufferSize;
            }
        }

        // Store in the buffer but only if it fits
        if ((count > 0) && (mStreamEnd + count <= mBufSize)) {
            memcpy(mBuffer + mStreamEnd, buffer, count);
            mStreamEnd += count;
            *bytesWritten = count;
            return NS_OK;
        }
    }

    // There are more bytes than fit in the buffer/cacheblocks, switch to file
    if (!mFD) {
        // Opens a cache file and write the buffer to it
        nsresult rv = FlushBufferToFile();
        if (NS_FAILED(rv)) {
            *bytesWritten = 0;
            return rv;
        }
    }
    // Write directly to the file
    if (PR_Write(mFD, buffer, count) != (int32_t)count) {
        NS_WARNING("failed to write all data");
        *bytesWritten = 0;
        return NS_ERROR_UNEXPECTED;     // NS_ErrorAccordingToNSPR()
    }
    mStreamEnd += count;
    *bytesWritten = count;

    UpdateFileSize();
    NS_ASSERTION(mBinding->mCacheEntry->DataSize() == mStreamEnd, "bad stream");

    return NS_OK;
}


void
nsDiskCacheStreamIO::UpdateFileSize()
{
    NS_ASSERTION(mFD, "nsDiskCacheStreamIO::UpdateFileSize should not have been called");
    
    nsDiskCacheRecord * record = &mBinding->mRecord;
    const uint32_t      oldSizeK  = record->DataFileSize();
    uint32_t            newSizeK  = (mStreamEnd + 0x03FF) >> 10;

    // make sure the size won't overflow (bug #651100)
    if (newSizeK > kMaxDataSizeK)
        newSizeK = kMaxDataSizeK;

    if (newSizeK == oldSizeK)  return;
    
    record->SetDataFileSize(newSizeK);

    // update cache size totals
    nsDiskCacheMap * cacheMap = mDevice->CacheMap();
    cacheMap->DecrementTotalSize(oldSizeK);       // decrement old size
    cacheMap->IncrementTotalSize(newSizeK);       // increment new size
    
    if (!mBinding->mDoomed) {
        nsresult rv = cacheMap->UpdateRecord(record);
        if (NS_FAILED(rv)) {
            NS_WARNING("cacheMap->UpdateRecord() failed.");
            // XXX doom cache entry?
        }
    }
}


nsresult
nsDiskCacheStreamIO::OpenCacheFile(int flags, PRFileDesc ** fd)
{
    NS_ENSURE_ARG_POINTER(fd);
    
    CACHE_LOG_DEBUG(("nsDiskCacheStreamIO::OpenCacheFile"));

    nsresult         rv;
    nsDiskCacheMap * cacheMap = mDevice->CacheMap();
    nsCOMPtr<nsIFile>           localFile;
    
    rv = cacheMap->GetLocalFileForDiskCacheRecord(&mBinding->mRecord,
                                                  nsDiskCache::kData,
                                                  !!(flags & PR_CREATE_FILE),
                                                  getter_AddRefs(localFile));
    if (NS_FAILED(rv))  return rv;
    
    // create PRFileDesc for input stream - the 00600 is just for consistency
    return localFile->OpenNSPRFileDesc(flags, 00600, fd);
}


nsresult
nsDiskCacheStreamIO::ReadCacheBlocks(uint32_t bufferSize)
{
    NS_ASSERTION(mStreamEnd == mBinding->mCacheEntry->DataSize(), "bad stream");
    NS_ASSERTION(bufferSize <= kMaxBufferSize, "bufferSize too large for buffer");
    NS_ASSERTION(mStreamEnd <= bufferSize, "data too large for buffer");

    nsDiskCacheRecord * record = &mBinding->mRecord;
    if (!record->DataLocationInitialized()) return NS_OK;

    NS_ASSERTION(record->DataFile() != kSeparateFile, "attempt to read cache blocks on separate file");

    if (!mBuffer) {
        // allocate buffer
        mBuffer = (char *) malloc(bufferSize);
        if (!mBuffer) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        mBufSize = bufferSize;
    }
    
    // read data stored in cache block files
    nsDiskCacheMap *map = mDevice->CacheMap();  // get map reference
    return map->ReadDataCacheBlocks(mBinding, mBuffer, mStreamEnd);
}


nsresult
nsDiskCacheStreamIO::FlushBufferToFile()
{
    nsresult  rv;
    nsDiskCacheRecord * record = &mBinding->mRecord;
    
    if (!mFD) {
        if (record->DataLocationInitialized() && (record->DataFile() > 0)) {
            // remove cache block storage
            nsDiskCacheMap * cacheMap = mDevice->CacheMap();
            rv = cacheMap->DeleteStorage(record, nsDiskCache::kData);
            if (NS_FAILED(rv))  return rv;
        }
        record->SetDataFileGeneration(mBinding->mGeneration);
        
        // allocate file
        rv = OpenCacheFile(PR_RDWR | PR_CREATE_FILE, &mFD);
        if (NS_FAILED(rv))  return rv;

        int64_t dataSize = mBinding->mCacheEntry->PredictedDataSize();
        if (dataSize != -1)
            mozilla::fallocate(mFD, std::min<int64_t>(dataSize, kPreallocateLimit));
    }
    
    // write buffer to the file
    if (PR_Write(mFD, mBuffer, mStreamEnd) != (int32_t)mStreamEnd) {
        NS_WARNING("failed to flush all data");
        return NS_ERROR_UNEXPECTED;     // NS_ErrorAccordingToNSPR()
    }

    // buffer is no longer valid
    DeleteBuffer();
   
    return NS_OK;
}


void
nsDiskCacheStreamIO::DeleteBuffer()
{
    if (mBuffer) {
        free(mBuffer);
        mBuffer = nullptr;
        mBufSize = 0;
    }
}

size_t
nsDiskCacheStreamIO::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf)
{
    size_t usage = aMallocSizeOf(this);

    usage += aMallocSizeOf(mFD);
    usage += aMallocSizeOf(mBuffer);

    return usage;
}

nsresult
nsDiskCacheStreamIO::SeekAndTruncate(uint32_t offset)
{
    if (!mBinding)  return NS_ERROR_NOT_AVAILABLE;
    
    if (uint32_t(offset) > mStreamEnd)  return NS_ERROR_FAILURE;
    
    // Set the current end to the desired offset
    mStreamEnd = offset;

    // Currently stored in file?
    if (mBinding->mRecord.DataLocationInitialized() && 
        (mBinding->mRecord.DataFile() == 0)) {
        if (!mFD) {
            // we need an mFD, we better open it now
            nsresult rv = OpenCacheFile(PR_RDWR | PR_CREATE_FILE, &mFD);
            if (NS_FAILED(rv))  return rv;
        }
        if (offset) {
            if (PR_Seek(mFD, offset, PR_SEEK_SET) == -1)
                return NS_ErrorAccordingToNSPR();
        }
        nsDiskCache::Truncate(mFD, offset);
        UpdateFileSize();

        // When we starting at zero again, close file and start with buffer.
        // If offset is non-zero (and within buffer) an option would be
        // to read the file into the buffer, but chance is high that it is 
        // rewritten to the file anyway.
        if (offset == 0) {
            // close file descriptor
            (void) PR_Close(mFD);
            mFD = nullptr;
        }
        return NS_OK;
    }
    
    // read data into mBuffer if not read yet.
    if (offset && !mBuffer) {
        nsresult rv = ReadCacheBlocks(kMaxBufferSize);
        if (NS_FAILED(rv))  return rv;
    }

    // stream buffer sanity check
    NS_ASSERTION(mStreamEnd <= kMaxBufferSize, "bad stream");
    return NS_OK;
}
