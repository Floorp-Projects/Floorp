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


#include "nsDiskCacheDevice.h"
#include "nsDiskCacheStreams.h"

#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISeekableStream.h"
#include "nsAutoLock.h"

// headers for ftruncate (or equivalent)
#if defined(XP_UNIX)
#include <unistd.h>
#elif defined(XP_MAC)
#include <Files.h>
#elif defined(XP_WIN)
#include <windows.h>
#elif defined(XP_OS2)
#define INCL_DOSERRORS
#include <os2.h>
#else
// other platforms: add necessary include file for ftruncate (or equivalent)
#endif

#if defined(XP_MAC)
#include "pprio.h"
#else
#include "private/pprio.h"
#endif




// Assumptions:
//      - cache descriptors live for life of streams
//      - streams will only be used by FileTransport,
//         they will not be directly accessible to clients
//      - overlapped I/O is NOT supported


// XXX perhaps closing descriptors should clear/sever transports
// XXX why doesn't nsCacheDescriptor::RemoveDescriptor() clear descriptor->mCacheEntry?


/******************************************************************************
 *  nsDiskCacheInputStream
 *****************************************************************************/
#ifdef XP_MAC
#pragma mark nsDiskCacheInputStream
#endif
class nsDiskCacheInputStream : public PRCList, public nsIInputStream {

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
    
    nsDiskCacheStreamIO *   mStreamIO;  // backpointer to parent
    PRFileDesc *            mFD;
    const char *            mBuffer;
    PRUint32                mStreamEnd;
    PRUint32                mPos;       // stream position
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
{
    NS_INIT_ISUPPORTS();
    PR_INIT_CLIST(this);
}


nsDiskCacheInputStream::~nsDiskCacheInputStream()
{
    
    Close();    // tell our parent we're going away
    NS_ASSERTION(PR_CLIST_IS_EMPTY(this), "nsDiskCacheInputStream destructed while still on list");
}


NS_IMETHODIMP
nsDiskCacheInputStream::Close()
{
    if (mStreamIO) {
        mStreamIO->CloseInputStream(this);
        NS_ASSERTION(!mStreamIO, "mStreamIO hasn't been nulled out");
    }
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheInputStream::Available(PRUint32 * bytesAvailable)
{
    if (!mStreamIO)  return NS_ERROR_NOT_AVAILABLE;
    if (mStreamEnd < mPos)  return NS_ERROR_UNEXPECTED;
    
    *bytesAvailable = mStreamEnd - mPos;
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheInputStream::Read(char * buffer, PRUint32 count, PRUint32 * bytesRead)
{
    if (!mStreamIO)  return NS_ERROR_NOT_AVAILABLE;
    
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
    
    nsDiskCacheStreamIO *   mStreamIO;  // backpointer to parent
};


NS_IMPL_THREADSAFE_ISUPPORTS2(nsDiskCacheOutputStream, nsIOutputStream, nsISeekableStream);


nsDiskCacheOutputStream::nsDiskCacheOutputStream( nsDiskCacheStreamIO * parent)
    : mStreamIO(parent)
{
    NS_INIT_ISUPPORTS();
}


nsDiskCacheOutputStream::~nsDiskCacheOutputStream()
{
    Close();
}


NS_IMETHODIMP
nsDiskCacheOutputStream::Close()
{
    // tell parent streamIO we are closing
    if (!mStreamIO)  return NS_OK;
    mStreamIO->CloseOutputStream(this);
    NS_ASSERTION(!mStreamIO, "mStreamIO hasn't been nulled out");
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheOutputStream::Flush()
{
    // yeah, yeah, well get to it...eventually...
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheOutputStream::Write(const char *buf, PRUint32 count, PRUint32 *bytesWritten)
{
    if (!mStreamIO)  return NS_ERROR_NOT_AVAILABLE;
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
    if (!mStreamIO)  return NS_ERROR_NOT_AVAILABLE;
    return mStreamIO->Seek(whence, offset);
}


NS_IMETHODIMP
nsDiskCacheOutputStream::Tell(PRUint32 * result)
{
    if (!mStreamIO)  return NS_ERROR_NOT_AVAILABLE;
    return mStreamIO->Tell(result);
}


NS_IMETHODIMP
nsDiskCacheOutputStream::SetEOF()
{
    if (!mStreamIO)  return NS_ERROR_NOT_AVAILABLE;
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
    , mFD(nsnull)
    , mStreamPos(0)
    , mStreamEnd(0)
    , mBufPos(0)
    , mBufEnd(0)
    , mBufSize(0)
    , mBufOffset(0)
    , mBufDirty(PR_FALSE)
    , mBuffer(nsnull)
{
    NS_INIT_ISUPPORTS();
    mDevice = (nsDiskCacheDevice *)mBinding->mCacheEntry->CacheDevice();
    PR_INIT_CLIST(&mInStreamQ);
}


nsDiskCacheStreamIO::~nsDiskCacheStreamIO()
{
    (void) Close(NS_OK);
}


NS_IMETHODIMP
nsDiskCacheStreamIO::Open()
{
    return NS_OK;
}


NS_IMETHODIMP nsDiskCacheStreamIO::Close(nsresult status)
{
    nsAutoLock lock(mDevice->DeviceLock()); // grab device lock

    // assert streams closed
    if (mOutStream) {
        NS_WARNING("output stream still open");
        (void) mOutStream->Close();
    }
    
    NS_ASSERTION(PR_CLIST_IS_EMPTY(&mInStreamQ), "input stream still open");
    
    // close remaining input streams (if any)
    nsDiskCacheInputStream * stream =
        (nsDiskCacheInputStream *)PR_LIST_HEAD(&mInStreamQ);
    while (stream != &mInStreamQ) {
        nsDiskCacheInputStream * nextStream =
            (nsDiskCacheInputStream *)PR_NEXT_LINK(stream);
            
        // can't call (void) stream->Close(), because it will try to reaquire device lock
        PR_REMOVE_AND_INIT_LINK(stream);
        stream->mStreamIO = nsnull;

        stream = nextStream;
    }
    
    NS_ASSERTION(!mFD, "file descriptor not closed");
    delete [] mBuffer;
    
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheStreamIO::GetInputStream(nsIInputStream ** inputStream)
{
    NS_ENSURE_ARG_POINTER(inputStream);
    *inputStream = nsnull;
    
    nsAutoLock lock(mDevice->DeviceLock()); // grab device lock

    NS_ASSERTION(!mOutStream, "already have an output stream open");
    if (mOutStream)  return NS_ERROR_NOT_AVAILABLE;

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

    NS_ASSERTION(!(fd && mBuffer), "ambiguous data sources for input stream");

    // create a new input stream
    nsDiskCacheInputStream * inStream = new nsDiskCacheInputStream(this, fd, mBuffer, mStreamEnd);
    if (!inStream)  return NS_ERROR_OUT_OF_MEMORY;
    
    PR_APPEND_LINK(inStream, &mInStreamQ);
    NS_ADDREF(*inputStream = inStream);
    return NS_OK;
}


NS_IMETHODIMP
nsDiskCacheStreamIO::GetOutputStream(nsIOutputStream ** outputStream)
{
    NS_ENSURE_ARG_POINTER(outputStream);
    *outputStream = nsnull;

    nsAutoLock lock(mDevice->DeviceLock()); // grab device lock
        
    NS_ASSERTION(!mOutStream, "already have an output stream open");
    NS_ASSERTION(PR_CLIST_IS_EMPTY(&mInStreamQ), "we already have input streams open");
    if ((mOutStream) || (!PR_CLIST_IS_EMPTY(&mInStreamQ)))
        return NS_ERROR_NOT_AVAILABLE;
    
    // mBuffer lazily allocated, but might exist if a previous stream already created one
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
nsDiskCacheStreamIO::CloseInputStream(nsDiskCacheInputStream *  inputStream)
{
    nsAutoLock lock(mDevice->DeviceLock()); // grab device lock
    PR_REMOVE_AND_INIT_LINK(inputStream);
    inputStream->mStreamIO = nsnull;
}


void
nsDiskCacheStreamIO::CloseOutputStream(nsDiskCacheOutputStream *  outputStream)
{
    nsAutoLock lock(mDevice->DeviceLock()); // grab device lock
    nsresult   rv;

    if (outputStream != mOutStream) {
        NS_WARNING("mismatched output streams");
        return;
    }
    
    // output stream is closing

    // write data to cache blocks, or flush mBuffer to file
    nsDiskCacheMap *    cacheMap = mDevice->CacheMap();  // get map reference
    
    if ((mStreamEnd > kMaxBufferSize) ||
        (mBinding->mCacheEntry->StoragePolicy() == nsICache::STORE_ON_DISK_AS_FILE)) {
        // make sure we save as separate file
        rv = FlushBufferToFile(PR_TRUE);       // will initialize DataFileLocation() if necessary
        NS_ASSERTION(NS_SUCCEEDED(rv), "FlushBufferToFile() failed");
        
        // close file descriptor
        NS_ASSERTION(mFD, "no file descriptor");
        (void) PR_Close(mFD);

    } else {
        // store data (if any) in cache block files
        
        // delete existing storage
        nsDiskCacheRecord * record = &mBinding->mRecord;
        if (record->DataLocationInitialized()) {
        
        
            rv = cacheMap->DeleteStorage(record, nsDiskCache::kData);
            if (NS_FAILED(rv)) {
            // XXX but perhaps DecrementTotalSize() should not be called ?
            // XXX mark DataLocation Uninitialized
                NS_WARNING("cacheMap->DeleteStorage() failed.");
                // XXX doom cache entry
                return;
            }
        }
    
        // XXX calculate which disk block file to put it in and put it there
        // XXX update data location for mBinding

        // flush buffer to block files
        if (mStreamEnd > 0) {
            nsresult  rv = cacheMap->WriteDataCacheBlocks(mBinding, mBuffer, mBufEnd);
            if (NS_FAILED(rv)) {
                NS_WARNING("WriteDatacacheBlocks() failed.");
                // XXX doom cache entry
                // XXX return ?
            }
        }
    }
    
    // dealloc mBuffer if it doesn't contain beginning of stream
    if (mBufOffset != 0) {
        free(mBuffer);
    }
    
    // update cache map if entry isn't doomed
    if (!mBinding->mDoomed) {
        rv = cacheMap->UpdateRecord(&mBinding->mRecord);
        if (NS_FAILED(rv)) {
            NS_WARNING("cacheMap->UpdateRecord() failed.");
            // XXX doom cache entry
        }
    }
    
    mOutStream = nsnull;
    outputStream->mStreamIO = nsnull;
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
    nsAutoLock lock(mDevice->DeviceLock()); // grab device lock

    if (!PR_CLIST_IS_EMPTY(&mInStreamQ)) {
        // we have open input streams already
        // this is an error until we support overlapped I/O
        NS_WARNING("Attempting to write to cache entry with open input streams.\n");
        return NS_ERROR_NOT_AVAILABLE;
    }

    *bytesWritten = WriteToBuffer(buffer, count);
    if (*bytesWritten != count)  return NS_ERROR_FAILURE;

    // update mStreamPos, mStreamEnd
    mStreamPos += count;
    if (mStreamEnd < mStreamPos)
        mStreamEnd = mStreamPos;

    return NS_OK;
}


// XXX collapse function if only used once
PRBool
nsDiskCacheStreamIO::EnsureLocalFile()
{
    if (mLocalFile)  return PR_TRUE;
    
    nsresult         rv;
    nsDiskCacheMap * cacheMap = mDevice->CacheMap();
    
    rv = cacheMap->GetLocalFileForDiskCacheRecord(&mBinding->mRecord,
                                                  nsDiskCache::kData,
                                                  getter_AddRefs(mLocalFile));

    return NS_SUCCEEDED(rv) ? PR_TRUE : PR_FALSE;
}


nsresult
nsDiskCacheStreamIO::OpenCacheFile(PRIntn flags, PRFileDesc ** fd)
{
    NS_ENSURE_ARG_POINTER(fd);
    
    if (!EnsureLocalFile())  return NS_ERROR_UNEXPECTED;
    
    // create PRFileDesc for input stream
    nsresult rv = mLocalFile->OpenNSPRFileDesc(flags, 00666, fd);
    if (NS_FAILED(rv))  return rv;  // unable to open file

    return NS_OK;
}


nsresult
nsDiskCacheStreamIO::ReadCacheBlocks()
{
    if (mStreamEnd != mBinding->mCacheEntry->DataSize()) {
        NS_WARNING("bad stream");
        return NS_ERROR_UNEXPECTED;
    }

    if (mStreamEnd > kMaxBufferSize) {
        NS_WARNING("data too large for buffer");
        return NS_ERROR_UNEXPECTED;
    }

    nsDiskCacheRecord * record = &mBinding->mRecord;
    if (!record->DataLocationInitialized())  return NS_OK;
    
    if (record->DataFile() == kSeparateFile) {
        NS_WARNING("attempted to read cache blocks on separate file");
        return NS_ERROR_UNEXPECTED;
    }

    PRUint32    bufSize = record->DataBlockCount() * record->DataBlockSize();
    if (mBuffer && (mBufSize < bufSize)) {
            // XXX how does this happen?  Is mBuffer dirty?
            NS_WARNING("allocated buffer is too small");
            return NS_ERROR_UNEXPECTED;
    }
    
    if (!mBuffer) {
        // allocate buffer
        mBufSize  = bufSize;
        mBuffer   = (char *) malloc(mBufSize);
        if (!mBuffer) {
            mBufSize = 0;
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    
    // read data stored in cache block files            
    nsDiskCacheMap *    map = mDevice->CacheMap();  // get map reference
    nsresult  rv = map->ReadDataCacheBlocks(mBinding, mBuffer, mBufSize);
    if (NS_FAILED(rv))  return rv;

    // update streamIO variables
    mBufOffset  = 0;
    mBufPos     = 0;
    mBufEnd     = mStreamEnd;
    
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
            // XXX update size totals?
        }
        record->SetDataFileGeneration(mBinding->mGeneration);
        
        // allocate file
        rv = OpenCacheFile(PR_RDWR | PR_CREATE_FILE, &mFD);
        if (NS_FAILED(rv))  return rv;
    }
    
    // write buffer
    PRInt32 bytesWritten = PR_Write(mFD, mBuffer, mBufEnd);
    if (bytesWritten != mBufEnd) {
        NS_WARNING("failed to flush all data");
        return NS_ERROR_UNEXPECTED;     // NS_ErrorAccordingToNSPR()
    }
    mBufDirty = PR_FALSE;

    // XXX entry->DataSize() may be larger than amount of data flushed at this point
    nsCacheEntry * entry = mBinding->mCacheEntry;
    PRUint32       sizeK = ((entry->DataSize() + 0x03FF) >> 10); // round up to next 1k
    record->SetDataFileSize(sizeK);                             // 1k minimum
    
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


nsresult
nsDiskCacheStreamIO::Seek(PRInt32 whence, PRInt32 offset)
{
    PRInt32  newPos;

    if (offset > mStreamEnd)  return NS_ERROR_FAILURE;

    
    if (mFD) {
        
        // do we have data in the buffer that needs to be flushed?
        if (mBufDirty) {
            // XXX optimization: are we just moving within the current buffer?
            
            nsresult rv = FlushBufferToFile(PR_TRUE);
            if (NS_FAILED(rv))  return rv;
        }
    
        newPos = PR_Seek(mFD, offset, (PRSeekWhence)whence);
        if (newPos == -1) {
            return NS_ErrorAccordingToNSPR();
        }
        
        mStreamPos = (PRUint32) newPos;
        mBufPos = 0;
        mBufEnd = 0;
        return NS_OK;
    }
    
    // seek in mBuffer

    // read data into mBuffer if not read yet.  mStreamEnd != mBufEnd   XXX
    if (mStreamEnd != mBufEnd) {
        nsresult rv = ReadCacheBlocks();
        if (NS_FAILED(rv))  return rv;
    }

    // stream buffer sanity checks
    if (mBufEnd > (16 * 1024)) {
        NS_WARNING("bad stream: mBufEnd > 16k");
        return NS_ERROR_UNEXPECTED;
    }
    
    if (mBufPos > mBufEnd) {
        NS_WARNING("bad stream: mBufPos > mBufEnd");
        return NS_ERROR_UNEXPECTED;
    }
    
    if (mStreamPos != mBufPos) {
        NS_WARNING("bad stream: mStreamPos != mBufPos");
        return NS_ERROR_UNEXPECTED;
    }
    
    if (mStreamEnd != mBufEnd) {
        NS_WARNING("bad stream: mStreamEnd != mBufEnd");
        return NS_ERROR_UNEXPECTED;
    }
    
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
    
    if ((newPos < 0) || (newPos > mBufEnd)) {
        NS_WARNING("seek offset out of range");
        return NS_ERROR_INVALID_ARG;
    }

    mStreamPos = newPos;
    mBufPos    = newPos;
    return NS_OK;
}


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
nsresult
nsDiskCacheStreamIO::SetEOF()
{
    nsresult    rv;
    NS_ASSERTION(mStreamPos <= mStreamEnd, "bad stream");
    
    if (mBinding->mRecord.DataLocationInitialized()) {
        
        if (mBinding->mRecord.DataFile() == 0) {
            if (!mFD) {
                // we need an mFD, we better open it now
                rv = OpenCacheFile(PR_RDWR | PR_CREATE_FILE, &mFD);
                if (NS_FAILED(rv))  return rv;
            }
        } else {
            // data in cache block files
            if (mStreamPos != 0) {
                // only read data if there will be some left after truncation
                rv = ReadCacheBlocks();
                if (NS_FAILED(rv))  return rv;
            }
        }
    }
    
    if (mFD) {
        // use modified SetEOF from nsFileStreams::SetEOF()
#if defined(XP_UNIX)
        if (ftruncate(PR_FileDesc2NativeHandle(mFD), mStreamPos) != 0) {
            NS_ERROR("ftruncate failed");
            return NS_ERROR_FAILURE;
        }
#elif defined(XP_MAC)
        if (::SetEOF(PR_FileDesc2NativeHandle(mFD), mStreamPos) != 0) {
            NS_ERROR("SetEOF failed");
            return NS_ERROR_FAILURE;
        }
#elif defined(XP_WIN)
        if (!SetEndOfFile((HANDLE) PR_FileDesc2NativeHandle(mFD))) {
            NS_ERROR("SetEndOfFile failed");
            return NS_ERROR_FAILURE;
        }
#elif defined(XP_OS2)
        if (DosSetFileSize((HFILE) PR_FileDesc2NativeHandle(mFD), mStreamPos) != NO_ERROR) {
            NS_ERROR("DosSetFileSize failed");
            return NS_ERROR_FAILURE;
        }
#else
        // add implementations for other platforms here
#endif

    } else {
        // data stored in buffer.
        NS_ASSERTION(mStreamEnd < (16 * 1024), "buffer truncation inadequate");
        NS_ASSERTION(mBufOffset == 0, "buffer truncation inadequate");
        NS_ASSERTION(mBufPos == mStreamPos, "bad stream");
        NS_ASSERTION(mBuffer ? mBufEnd == mStreamEnd : PR_TRUE, "bad stream");
    }

    NS_ASSERTION(mStreamEnd == mBinding->mCacheEntry->DataSize(), "cache entry not updated");
    // XXX we expect nsCacheEntryDescriptor::TransportWrapper::OpenOutputStream() to eventually
    // XXX update the cache entry

    mStreamEnd  = mStreamPos;
    mBufEnd     = mBufPos;    

    return  NS_OK;
}
