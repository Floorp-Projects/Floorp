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
 * The Original Code is nsDiskCacheStreams.cpp, released June 13, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan <gordon@netscape.com>
 */


#include "nsDiskCache.h"
#include "nsDiskCacheDevice.h"
#include "nsDiskCacheStreams.h"
#include "nsCacheService.h"

#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISeekableStream.h"
#include "nsAutoLock.h"



// Assumptions:
//      - cache descriptors live for life of streams
//      - streams will only be used by FileTransport,
//         they will not be directly accessible to clients
//      - overlapped I/O is NOT supported


/******************************************************************************
 *  nsDiskCacheInputStream
 *****************************************************************************/
#ifdef XP_MAC
#pragma mark nsDiskCacheInputStream
#endif
class nsDiskCacheInputStream : public nsIInputStream {

public:

    nsDiskCacheInputStream( nsDiskCacheStreamIO * parent,
                            PRFileDesc *          fileDesc,
                            const char *          buffer,
                            PRUint32              endOfStream);

    virtual ~nsDiskCacheInputStream();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM

private:
    friend class nsDiskCacheStreamIO;
    
    nsCOMPtr<nsDiskCacheStreamIO>   mStreamIO;  // backpointer to parent
    PRFileDesc *                    mFD;
    const char *                    mBuffer;
    PRUint32                        mStreamEnd;
    PRUint32                        mPos;       // stream position
    PRBool                          mClosed;
};


NS_IMPL_THREADSAFE_ISUPPORTS1(nsDiskCacheInputStream, nsIInputStream);


nsDiskCacheInputStream::nsDiskCacheInputStream( nsDiskCacheStreamIO * parent,
                                                PRFileDesc *          fileDesc,
                                                const char *          buffer,
                                                PRUint32              endOfStream)
    : mStreamIO(parent)
    , mFD(fileDesc)
    , mBuffer(buffer)
    , mStreamEnd(endOfStream)
    , mPos(0)
    , mClosed(PR_FALSE)
{
    mStreamIO->IncrementInputStreamCount();
}


nsDiskCacheInputStream::~nsDiskCacheInputStream()
{
    Close();
    mStreamIO->DecrementInputStreamCount();
}


NS_IMETHODIMP
nsDiskCacheInputStream::Close()
{
    if (!mClosed) {
        if (mFD) {
            (void) PR_Close(mFD);
            mFD = nsnull;
        }
        mClosed = PR_TRUE;
    }
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheInputStream::Available(PRUint32 * bytesAvailable)
{
    if (mClosed)  return NS_ERROR_NOT_AVAILABLE;
    if (mStreamEnd < mPos)  return NS_ERROR_UNEXPECTED;
    
    *bytesAvailable = mStreamEnd - mPos;
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheInputStream::Read(char * buffer, PRUint32 count, PRUint32 * bytesRead)
{
    if (mClosed)  return NS_ERROR_NOT_AVAILABLE;
    
    *bytesRead = 0;
    if (mPos == mStreamEnd)  return NS_OK;
    if (mPos > mStreamEnd)   return NS_ERROR_UNEXPECTED;
    
    if (mFD) {
        // just read from file
        PRInt32  result = PR_Read(mFD, buffer, count);
        if (result < 0)  return  NS_ErrorAccordingToNSPR();
        
        mPos += (PRUint32)result;
        *bytesRead = (PRUint32)result;
        
    } else if (mBuffer) {
        // read data from mBuffer
        if (count > mStreamEnd - mPos)
            count = mStreamEnd - mPos;
    
        memcpy(buffer, mBuffer + mPos, count);
        mPos += count;
        *bytesRead = count;
    } else {
        // no data source for input stream
    }

    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheInputStream::ReadSegments(nsWriteSegmentFun writer,
                                     void *            closure,
                                     PRUint32          count,
                                     PRUint32 *        bytesRead)
{
    NS_NOTREACHED("ReadSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDiskCacheInputStream::IsNonBlocking(PRBool * nonBlocking)
{
    *nonBlocking = PR_FALSE;
    return NS_OK;
}


/******************************************************************************
 *  nsDiskCacheOutputStream
 *****************************************************************************/
#ifdef XP_MAC
#pragma mark -
#pragma mark nsDiskCacheOutputStream
#endif
class nsDiskCacheOutputStream : public nsIOutputStream, nsISeekableStream {

public:
  nsDiskCacheOutputStream( nsDiskCacheStreamIO * parent);
  virtual ~nsDiskCacheOutputStream();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM

private:
    friend class nsDiskCacheStreamIO;
    
    nsCOMPtr<nsDiskCacheStreamIO>   mStreamIO;  // backpointer to parent
    PRBool                          mClosed;
};


NS_IMPL_THREADSAFE_ISUPPORTS2(nsDiskCacheOutputStream, nsIOutputStream, nsISeekableStream);


nsDiskCacheOutputStream::nsDiskCacheOutputStream( nsDiskCacheStreamIO * parent)
    : mStreamIO(parent)
    , mClosed(PR_FALSE)
{
}


nsDiskCacheOutputStream::~nsDiskCacheOutputStream()
{
    Close();
}


NS_IMETHODIMP
nsDiskCacheOutputStream::Close()
{
    if (!mClosed) {
        // tell parent streamIO we are closing
        mStreamIO->CloseOutputStream(this);
        mClosed = PR_TRUE;
    }
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheOutputStream::Flush()
{
    if (mClosed)  return NS_ERROR_NOT_AVAILABLE;
    // yeah, yeah, well get to it...eventually...
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheOutputStream::Write(const char *buf, PRUint32 count, PRUint32 *bytesWritten)
{
    if (mClosed)  return NS_ERROR_NOT_AVAILABLE;
    return mStreamIO->Write(buf, count, bytesWritten);
}


NS_IMETHODIMP
nsDiskCacheOutputStream::WriteFrom(nsIInputStream *inStream, PRUint32 count, PRUint32 *bytesWritten)
{
    NS_NOTREACHED("WriteFrom");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDiskCacheOutputStream::WriteSegments( nsReadSegmentFun reader,
                                        void *           closure,
                                        PRUint32         count,
                                        PRUint32 *       bytesWritten)
{
    NS_NOTREACHED("WriteSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDiskCacheOutputStream::Seek(PRInt32 whence, PRInt32 offset)
{
    if (mClosed)  return NS_ERROR_NOT_AVAILABLE;
    return mStreamIO->Seek(whence, offset);
}


NS_IMETHODIMP
nsDiskCacheOutputStream::Tell(PRUint32 * result)
{
    if (mClosed)  return NS_ERROR_NOT_AVAILABLE;
    return mStreamIO->Tell(result);
}


NS_IMETHODIMP
nsDiskCacheOutputStream::SetEOF()
{
    if (mClosed)  return NS_ERROR_NOT_AVAILABLE;
    return mStreamIO->SetEOF();
}


NS_IMETHODIMP
nsDiskCacheOutputStream::IsNonBlocking(PRBool * nonBlocking)
{
    *nonBlocking = PR_FALSE;
    return NS_OK;
}



/******************************************************************************
 *  nsDiskCacheStreamIO
 *****************************************************************************/
#ifdef XP_MAC
#pragma mark -
#pragma mark nsDiskCacheStreamIO
#endif

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDiskCacheStreamIO, nsIStreamIO);

// we pick 16k as the max buffer size because that is the threshold above which
//      we are unable to store the data in the cache block files
//      see nsDiskCacheMap.[cpp,h]
#define kMaxBufferSize      (16 * 1024)

nsDiskCacheStreamIO::nsDiskCacheStreamIO(nsDiskCacheBinding *   binding)
    : mBinding(binding)
    , mOutStream(nsnull)
    , mInStreamCount(0)
    , mFD(nsnull)
    , mStreamPos(0)
    , mStreamEnd(0)
    , mBufPos(0)
    , mBufEnd(0)
    , mBufSize(0)
    , mBufDirty(PR_FALSE)
    , mBuffer(nsnull)
{
    mDevice = (nsDiskCacheDevice *)mBinding->mCacheEntry->CacheDevice();

    // acquire "death grip" on cache service
    nsCacheService *service = nsCacheService::GlobalInstance();
    NS_ADDREF(service);
}


nsDiskCacheStreamIO::~nsDiskCacheStreamIO()
{
    (void) Close(NS_OK);

    // release "death grip" on cache service
    nsCacheService *service = nsCacheService::GlobalInstance();
    NS_RELEASE(service);
}


NS_IMETHODIMP
nsDiskCacheStreamIO::Open()
{
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheStreamIO::Close(nsresult status)
{
    // this should only be called from our destructor
    // no one is interested in us anymore, so we don't need to grab any locks
    
    // assert streams closed
    NS_ASSERTION(!mOutStream, "output stream still open");
    NS_ASSERTION(mInStreamCount == 0, "input stream still open");
    NS_ASSERTION(!mFD, "file descriptor not closed");

    DeleteBuffer();
    
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheStreamIO::GetInputStream(nsIInputStream ** inputStream)
{
    NS_ENSURE_ARG_POINTER(inputStream);
    *inputStream = nsnull;
    
    nsAutoLock lock(nsCacheService::ServiceLock()); // grab service lock
    if (!mBinding)  return NS_ERROR_NOT_AVAILABLE;

    if (mOutStream) {
        NS_WARNING("already have an output stream open");
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsresult            rv;
    PRFileDesc *        fd = nsnull;

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
        rv = ReadCacheBlocks();
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


NS_IMETHODIMP
nsDiskCacheStreamIO::GetOutputStream(nsIOutputStream ** outputStream)
{
    NS_ENSURE_ARG_POINTER(outputStream);
    *outputStream = nsnull;

    nsAutoLock lock(nsCacheService::ServiceLock()); // grab service lock
    if (!mBinding)  return NS_ERROR_NOT_AVAILABLE;
        
    NS_ASSERTION(!mOutStream, "already have an output stream open");
    NS_ASSERTION(mInStreamCount == 0, "we already have input streams open");
    if (mOutStream || mInStreamCount)  return NS_ERROR_NOT_AVAILABLE;
    
    // mBuffer lazily allocated, but might exist if a previous stream already
    // created one.
    mBufPos    = 0;
    mStreamPos = 0;
    mStreamEnd = mBinding->mCacheEntry->DataSize();

    // create a new output stream
    mOutStream = new nsDiskCacheOutputStream(this);
    if (!mOutStream)  return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(*outputStream = mOutStream);
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheStreamIO::GetName(nsACString & name)
{
    name = NS_LITERAL_CSTRING("nsDiskCacheStreamIO");
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheStreamIO::GetContentType(nsACString & contentType)
{
    contentType.Truncate();
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheStreamIO::GetContentCharset(nsACString & contentCharset)
{
    contentCharset.Truncate();
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheStreamIO::GetContentLength(PRInt32 *contentLength)
{
    NS_ENSURE_ARG_POINTER(contentLength);
    *contentLength = -1;
    return NS_OK;
}


void
nsDiskCacheStreamIO::ClearBinding()
{
    if (mBinding && mOutStream)
        Flush();
    mBinding = nsnull;
}

nsresult
nsDiskCacheStreamIO::CloseOutputStream(nsDiskCacheOutputStream *  outputStream)
{
    nsAutoLock lock(nsCacheService::ServiceLock()); // grab service lock
    nsresult   rv;

    if (outputStream != mOutStream) {
        NS_WARNING("mismatched output streams");
        return NS_ERROR_UNEXPECTED;
    }
    
    // output stream is closing
    if (!mBinding) {    // if we're severed, just clear member variables
        NS_ASSERTION(!mBufDirty, "oops");
        mOutStream = nsnull;
        outputStream->mStreamIO = nsnull;
        return NS_ERROR_NOT_AVAILABLE;
    }

    rv = Flush();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Flush() failed");

    mOutStream = nsnull;
    return rv;
}

nsresult
nsDiskCacheStreamIO::Flush()
{
    NS_ASSERTION(mBinding, "oops");

    if (!mBufDirty)
        return NS_OK;

    // write data to cache blocks, or flush mBuffer to file
    nsDiskCacheMap *cacheMap = mDevice->CacheMap();  // get map reference
    nsresult rv;
    
    if ((mStreamEnd > kMaxBufferSize) ||
        (mBinding->mCacheEntry->StoragePolicy() == nsICache::STORE_ON_DISK_AS_FILE)) {
        // make sure we save as separate file
        rv = FlushBufferToFile(PR_TRUE);       // will initialize DataFileLocation() if necessary
        NS_ASSERTION(NS_SUCCEEDED(rv), "FlushBufferToFile() failed");
        
        // close file descriptor
        NS_ASSERTION(mFD, "no file descriptor");
        (void) PR_Close(mFD);
        mFD = nsnull;

        // since the data location is on disk as a single file, the only value
        // in keeping mBuffer around is to avoid an extra malloc the next time
        // we need to write to this file.  reading will use a file descriptor.
        // therefore, it's probably not worth optimizing for the subsequent
        // write, so we unconditionally delete mBuffer here.
        DeleteBuffer();

    } else {
        // store data (if any) in cache block files
        
        // delete existing storage
        nsDiskCacheRecord * record = &mBinding->mRecord;
        if (record->DataLocationInitialized()) {
            rv = cacheMap->DeleteStorage(record, nsDiskCache::kData);
            if (NS_FAILED(rv)) {
                NS_WARNING("cacheMap->DeleteStorage() failed.");
                return  rv;    // XXX doom cache entry
            }
        }
    
        // flush buffer to block files
        if (mStreamEnd > 0) {
            rv = cacheMap->WriteDataCacheBlocks(mBinding, mBuffer, mBufEnd);
            if (NS_FAILED(rv)) {
                NS_WARNING("WriteDataCacheBlocks() failed.");
                return rv;   // XXX doom cache entry?
                
            }
        }

        mBufDirty = PR_FALSE;
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
                            PRUint32     count,
                            PRUint32 *   bytesWritten)
{
    nsresult    rv = NS_OK;
    nsAutoLock lock(nsCacheService::ServiceLock()); // grab service lock
    if (!mBinding)  return NS_ERROR_NOT_AVAILABLE;

    if (mInStreamCount) {
        // we have open input streams already
        // this is an error until we support overlapped I/O
        NS_WARNING("Attempting to write to cache entry with open input streams.\n");
        return NS_ERROR_NOT_AVAILABLE;
    }

    *bytesWritten = WriteToBuffer(buffer, count);
    if (*bytesWritten != count)  return NS_ERROR_FAILURE;

    // update mStreamPos, mStreamEnd
    mStreamPos += count;
    if (mStreamEnd < mStreamPos) {
        mStreamEnd = mStreamPos;
        NS_ASSERTION(mBinding->mCacheEntry->DataSize() == mStreamEnd, "bad stream");

        // if we have a separate file, we need to adjust the disk cache size totals here
        if (mFD) {
            rv = UpdateFileSize();
        }
    }
    
    return rv;
}


nsresult
nsDiskCacheStreamIO::UpdateFileSize()
{
    NS_ASSERTION(mFD, "nsDiskCacheStreamIO::UpdateFileSize should not have been called");
    if (!mFD)  return NS_ERROR_UNEXPECTED;
    
    nsDiskCacheRecord * record = &mBinding->mRecord;
    PRUint32            oldSizeK  = record->DataFileSize();
    PRUint32            newSizeK  = (mStreamEnd + 0x03FF) >> 10;
    
    if (newSizeK == oldSizeK)  return NS_OK;
    
    record->SetDataFileSize(newSizeK);

    // update cache size totals
    nsDiskCacheMap * cacheMap = mDevice->CacheMap();
    cacheMap->DecrementTotalSize(oldSizeK * 1024);       // decrement old size
    cacheMap->IncrementTotalSize(newSizeK * 1024);       // increment new size
    
    if (!mBinding->mDoomed) {
        nsresult rv = cacheMap->UpdateRecord(&mBinding->mRecord);
        if (NS_FAILED(rv)) {
            NS_WARNING("cacheMap->UpdateRecord() failed.");
            // XXX doom cache entry?
            return rv;
        }
    }
    return NS_OK;
}


nsresult
nsDiskCacheStreamIO::OpenCacheFile(PRIntn flags, PRFileDesc ** fd)
{
    NS_ENSURE_ARG_POINTER(fd);
    
    nsresult         rv;
    nsDiskCacheMap * cacheMap = mDevice->CacheMap();
    
    rv = cacheMap->GetLocalFileForDiskCacheRecord(&mBinding->mRecord,
                                                  nsDiskCache::kData,
                                                  getter_AddRefs(mLocalFile));
    if (NS_FAILED(rv))  return rv;
    
    // create PRFileDesc for input stream
    rv = mLocalFile->OpenNSPRFileDesc(flags, 00666, fd);
    if (NS_FAILED(rv))  return rv;  // unable to open file

    return NS_OK;
}


nsresult
nsDiskCacheStreamIO::ReadCacheBlocks()
{
    NS_ASSERTION(mStreamEnd == mBinding->mCacheEntry->DataSize(), "bad stream");
    NS_ASSERTION(mStreamEnd <= kMaxBufferSize, "data too large for buffer");

    nsDiskCacheRecord * record = &mBinding->mRecord;
    if (!record->DataLocationInitialized()) return NS_OK;

    NS_ASSERTION(record->DataFile() != kSeparateFile, "attempt to read cache blocks on separate file");

    PRUint32 bufSize = record->DataBlockCount() * record->DataBlockSize();
    
    if (!mBuffer) {
        // allocate buffer
        mBufSize  = bufSize;
        mBuffer   = (char *) malloc(mBufSize);
        if (!mBuffer) {
            mBufSize = 0;
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    NS_ASSERTION(bufSize <= mBufSize, "allocated buffer is too small");
    
    // read data stored in cache block files            
    nsDiskCacheMap *map = mDevice->CacheMap();  // get map reference
    nsresult rv = map->ReadDataCacheBlocks(mBinding, mBuffer, mBufSize);
    if (NS_FAILED(rv)) return rv;

    // update streamIO variables
    mBufPos = 0;
    mBufEnd = mStreamEnd;
    
    return NS_OK;
}


nsresult
nsDiskCacheStreamIO::FlushBufferToFile(PRBool  clearBuffer)
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
    }
    
    // write buffer
    PRInt32 bytesWritten = PR_Write(mFD, mBuffer, mBufEnd);
    if (PRUint32(bytesWritten) != mBufEnd) {
        NS_WARNING("failed to flush all data");
        return NS_ERROR_UNEXPECTED;     // NS_ErrorAccordingToNSPR()
    }
    mBufDirty = PR_FALSE;
    
    if (clearBuffer) {
        // reset buffer
        mBufPos = 0;
        mBufEnd = 0;
    }
    
    return NS_OK;
}


PRUint32
nsDiskCacheStreamIO::WriteToBuffer(const char * buffer, PRUint32 count)
{
    NS_ASSERTION(count, "WriteToBuffer called with count of zero");
    NS_ASSERTION(mBufPos <= mBufEnd, "streamIO buffer corrupted");

    PRUint32 bytesLeft = count;
    
    while (bytesLeft) {
        if (mBufPos == mBufSize) {
            if (mBufSize < kMaxBufferSize) {
                mBufSize = kMaxBufferSize;
                mBuffer  = (char *) realloc(mBuffer, mBufSize);
                if (!mBuffer)  {
                    mBufSize = 0;
                    return 0;
                }
            } else {
                nsresult rv = FlushBufferToFile(PR_TRUE);
                if (NS_FAILED(rv))  return 0;
            }
        }
        
        PRUint32 chunkSize = bytesLeft;
        if (chunkSize > (mBufSize - mBufPos))
            chunkSize =  mBufSize - mBufPos;
        
        memcpy(mBuffer + mBufPos, buffer, chunkSize);
        mBufDirty = PR_TRUE;
        mBufPos += chunkSize;
        bytesLeft -= chunkSize;
        buffer += chunkSize;
        
        if (mBufEnd < mBufPos)
            mBufEnd = mBufPos;
    }
    
    return count;
}

void
nsDiskCacheStreamIO::DeleteBuffer()
{
    if (mBuffer) {
        NS_ASSERTION(mBufDirty == PR_FALSE, "deleting dirty buffer");
        free(mBuffer);
        mBuffer = nsnull;
        mBufPos = 0;
        mBufEnd = 0;
        mBufSize = 0;
    }
}


// called only from nsDiskCacheOutputStream::Seek
nsresult
nsDiskCacheStreamIO::Seek(PRInt32 whence, PRInt32 offset)
{
    PRInt32  newPos;
    nsAutoLock lock(nsCacheService::ServiceLock()); // grab service lock
    if (!mBinding)  return NS_ERROR_NOT_AVAILABLE;

    if (PRUint32(offset) > mStreamEnd)  return NS_ERROR_FAILURE;
 
    if (mBinding->mRecord.DataLocationInitialized()) {
        if (mBinding->mRecord.DataFile() == 0) {
            if (!mFD) {
                // we need an mFD, we better open it now
                nsresult rv = OpenCacheFile(PR_RDWR | PR_CREATE_FILE, &mFD);
                if (NS_FAILED(rv))  return rv;
            }
        }
    }

    if (mFD) {
        // do we have data in the buffer that needs to be flushed?
        if (mBufDirty) {
            // XXX optimization: are we just moving within the current buffer?
            nsresult rv = FlushBufferToFile(PR_TRUE);
            if (NS_FAILED(rv))  return rv;
        }
    
        newPos = PR_Seek(mFD, offset, (PRSeekWhence)whence);
        if (newPos == -1)
            return NS_ErrorAccordingToNSPR();
        
        mStreamPos = (PRUint32) newPos;
        mBufPos = 0;
        mBufEnd = 0;
        return NS_OK;
    }
    
    // else, seek in mBuffer
    
    switch(whence) {
        case PR_SEEK_SET:
            newPos = offset;
            break;
        
        case PR_SEEK_CUR:   // relative from current posistion
            newPos = offset + (PRUint32)mStreamPos;
            break;
            
        case PR_SEEK_END:   // relative from end
            newPos = offset + (PRUint32)mBufEnd;
            break;
        
        default:
            return NS_ERROR_INVALID_ARG;
    }

    // read data into mBuffer if not read yet.
    if (mStreamEnd && !mBufEnd) {
        if (newPos > 0) {
            nsresult rv = ReadCacheBlocks();
            if (NS_FAILED(rv))  return rv;
        }
    }

    // stream buffer sanity checks
    NS_ASSERTION(mBufEnd <= (16 * 1024), "bad stream");
    NS_ASSERTION(mBufPos <= mBufEnd,     "bad stream");
    NS_ASSERTION(mStreamPos == mBufPos,  "bad stream");
    NS_ASSERTION(mStreamEnd == mBufEnd,  "bad stream");
    
    if ((newPos < 0) || (PRUint32(newPos) > mBufEnd)) {
        NS_WARNING("seek offset out of range");
        return NS_ERROR_INVALID_ARG;
    }

    mStreamPos = newPos;
    mBufPos    = newPos;
    return NS_OK;
}


// called only from nsDiskCacheOutputStream::Tell
nsresult
nsDiskCacheStreamIO::Tell(PRUint32 * result)
{
    NS_ENSURE_ARG_POINTER(result);
    *result = mStreamPos;
    return NS_OK;
}



// SetEOF() will only be called by FileTransport.
//      nsCacheEntryDescriptor::TransportWrapper::OpenOutputStream() will eventually update
//          the cache entry, so we need only update the underlying data structure here.
//
// called only from nsDiskCacheOutputStream::SetEOF
nsresult
nsDiskCacheStreamIO::SetEOF()
{
    nsresult    rv;
    nsAutoLock lock(nsCacheService::ServiceLock()); // grab service lock
    NS_ASSERTION(mStreamPos <= mStreamEnd, "bad stream");
    if (!mBinding)  return NS_ERROR_NOT_AVAILABLE;
    
    if (mBinding->mRecord.DataLocationInitialized()) {
        if (mBinding->mRecord.DataFile() == 0) {
            if (!mFD) {
                // we need an mFD, we better open it now
                rv = OpenCacheFile(PR_RDWR | PR_CREATE_FILE, &mFD);
                if (NS_FAILED(rv))  return rv;
            }
        } else {
            // data in cache block files
            if ((mStreamPos != 0) && (mStreamPos != mBufPos)) {
                // only read data if there will be some left after truncation
                rv = ReadCacheBlocks();
                if (NS_FAILED(rv))  return rv;
            }
        }
    }
    
    if (mFD) {
        rv = nsDiskCache::Truncate(mFD, mStreamPos);
#ifdef DEBUG
        PRUint32 oldSizeK = (mStreamEnd + 0x03FF) >> 10;
        NS_ASSERTION(mBinding->mRecord.DataFileSize() == oldSizeK, "bad disk cache entry size");
    } else {
        // data stored in buffer.
        NS_ASSERTION(mStreamEnd < (16 * 1024), "buffer truncation inadequate");
        NS_ASSERTION(mBufPos == mStreamPos, "bad stream");
        NS_ASSERTION(mBuffer ? mBufEnd == mStreamEnd : PR_TRUE, "bad stream");
#endif
    }

    NS_ASSERTION(mStreamEnd == mBinding->mCacheEntry->DataSize(), "cache entry not updated");
    // we expect nsCacheEntryDescriptor::TransportWrapper::OpenOutputStream()
    // to eventually update the cache entry    

    mStreamEnd  = mStreamPos;
    mBufEnd     = mBufPos;
    
    if (mFD) {
        UpdateFileSize();
    }

    return  NS_OK;
}
