/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsICache.h"
#include "nsCache.h"
#include "nsCacheService.h"
#include "nsCacheEntryDescriptor.h"
#include "nsCacheEntry.h"
#include "nsReadableUtils.h"
#include "nsIOutputStream.h"
#include "nsCRT.h"

#define kMinDecompressReadBufLen 1024
#define kMinCompressWriteBufLen  1024

NS_IMPL_THREADSAFE_ISUPPORTS2(nsCacheEntryDescriptor,
                              nsICacheEntryDescriptor,
                              nsICacheEntryInfo)

nsCacheEntryDescriptor::nsCacheEntryDescriptor(nsCacheEntry * entry,
                                               nsCacheAccessMode accessGranted)
    : mCacheEntry(entry),
      mAccessGranted(accessGranted),
      mOutput(nsnull)
{
    PR_INIT_CLIST(this);
    NS_ADDREF(nsCacheService::GlobalInstance());  // ensure it lives for the lifetime of the descriptor
}


nsCacheEntryDescriptor::~nsCacheEntryDescriptor()
{
    // No need to close if the cache entry has already been severed.  This
    // helps avoid a shutdown assertion (bug 285519) that is caused when
    // consumers end up holding onto these objects past xpcom-shutdown.  It's
    // okay for them to do that because the cache service calls our Close
    // method during xpcom-shutdown, so we don't need to complain about it.
    if (mCacheEntry)
        Close();

    nsCacheService * service = nsCacheService::GlobalInstance();
    NS_RELEASE(service);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetClientID(char ** result)
{
    NS_ENSURE_ARG_POINTER(result);

    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return ClientIDFromCacheKey(*(mCacheEntry->Key()), result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetDeviceID(char ** aDeviceID)
{
    NS_ENSURE_ARG_POINTER(aDeviceID);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    const char* deviceID = mCacheEntry->GetDeviceID();
    if (!deviceID) {
        *aDeviceID = nsnull;
        return NS_OK;
    }

    *aDeviceID = NS_strdup(deviceID);
    return *aDeviceID ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetKey(nsACString &result)
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return ClientKeyFromCacheKey(*(mCacheEntry->Key()), result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetFetchCount(PRInt32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->FetchCount();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetLastFetched(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->LastFetched();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetLastModified(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->LastModified();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetExpirationTime(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->ExpirationTime();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetExpirationTime(PRUint32 expirationTime)
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    mCacheEntry->SetExpirationTime(expirationTime);
    mCacheEntry->MarkEntryDirty();
    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::IsStreamBased(bool *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->IsStreamData();
    return NS_OK;
}

NS_IMETHODIMP nsCacheEntryDescriptor::GetPredictedDataSize(PRInt64 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry) return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->PredictedDataSize();
    return NS_OK;
}

NS_IMETHODIMP nsCacheEntryDescriptor::SetPredictedDataSize(PRInt64
                                                           predictedSize)
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    mCacheEntry->SetPredictedDataSize(predictedSize);
    return NS_OK;
}

NS_IMETHODIMP nsCacheEntryDescriptor::GetDataSize(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    const char* val = mCacheEntry->GetMetaDataElement("uncompressed-len");
    if (!val) {
        *result = mCacheEntry->DataSize();
    } else {
        *result = atol(val);
    }

    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::GetStorageDataSize(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->DataSize();

    return NS_OK;
}


nsresult
nsCacheEntryDescriptor::RequestDataSizeChange(PRInt32 deltaSize)
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    nsresult  rv;
    rv = nsCacheService::OnDataSizeChange(mCacheEntry, deltaSize);
    if (NS_SUCCEEDED(rv)) {
        // XXX review for signed/unsigned math errors
        PRUint32  newDataSize = mCacheEntry->DataSize() + deltaSize;
        mCacheEntry->SetDataSize(newDataSize);
        mCacheEntry->TouchData();
    }
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetDataSize(PRUint32 dataSize)
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    // XXX review for signed/unsigned math errors
    PRInt32  deltaSize = dataSize - mCacheEntry->DataSize();

    nsresult  rv;
    rv = nsCacheService::OnDataSizeChange(mCacheEntry, deltaSize);
    // this had better be NS_OK, this call instance is advisory for memory cache objects
    if (NS_SUCCEEDED(rv)) {
        // XXX review for signed/unsigned math errors
        PRUint32  newDataSize = mCacheEntry->DataSize() + deltaSize;
        mCacheEntry->SetDataSize(newDataSize);
        mCacheEntry->TouchData();
    } else {
        NS_WARNING("failed SetDataSize() on memory cache object!");
    }
    
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::OpenInputStream(PRUint32 offset, nsIInputStream ** result)
{
    NS_ENSURE_ARG_POINTER(result);

    {
        nsCacheServiceAutoLock lock;
        if (!mCacheEntry)                  return NS_ERROR_NOT_AVAILABLE;
        if (!mCacheEntry->IsStreamData())  return NS_ERROR_CACHE_DATA_IS_NOT_STREAM;

        // ensure valid permissions
        if (!(mAccessGranted & nsICache::ACCESS_READ))
            return NS_ERROR_CACHE_READ_ACCESS_DENIED;
    }

    nsInputStreamWrapper* cacheInput = nsnull;
    const char *val;
    val = mCacheEntry->GetMetaDataElement("uncompressed-len");
    if (val) {
        cacheInput = new nsDecompressInputStreamWrapper(this, offset);
    } else {
        cacheInput = new nsInputStreamWrapper(this, offset);
    }
    if (!cacheInput) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result = cacheInput);
    return NS_OK;
}

NS_IMETHODIMP
nsCacheEntryDescriptor::OpenOutputStream(PRUint32 offset, nsIOutputStream ** result)
{
    NS_ENSURE_ARG_POINTER(result);

    {
        nsCacheServiceAutoLock lock;
        if (!mCacheEntry)                  return NS_ERROR_NOT_AVAILABLE;
        if (!mCacheEntry->IsStreamData())  return NS_ERROR_CACHE_DATA_IS_NOT_STREAM;

        // ensure valid permissions
        if (!(mAccessGranted & nsICache::ACCESS_WRITE))
            return NS_ERROR_CACHE_WRITE_ACCESS_DENIED;
    }

    nsOutputStreamWrapper* cacheOutput = nsnull;
    PRInt32 compressionLevel = nsCacheService::CacheCompressionLevel();
    const char *val;
    val = mCacheEntry->GetMetaDataElement("uncompressed-len");
    if ((compressionLevel > 0) && val) {
        cacheOutput = new nsCompressOutputStreamWrapper(this, offset);
    } else {
        // clear compression flag when compression disabled - see bug #715198
        if (val) {
            mCacheEntry->SetMetaDataElement("uncompressed-len", nsnull);
        }
        cacheOutput = new nsOutputStreamWrapper(this, offset);
    }
    if (!cacheOutput) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result = cacheOutput);
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetCacheElement(nsISupports ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)                 return NS_ERROR_NOT_AVAILABLE;
    if (mCacheEntry->IsStreamData())  return NS_ERROR_CACHE_DATA_IS_STREAM;

    NS_IF_ADDREF(*result = mCacheEntry->Data());
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetCacheElement(nsISupports * cacheElement)
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)                 return NS_ERROR_NOT_AVAILABLE;
    if (mCacheEntry->IsStreamData())  return NS_ERROR_CACHE_DATA_IS_STREAM;

    return nsCacheService::SetCacheElement(mCacheEntry, cacheElement);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetAccessGranted(nsCacheAccessMode *result)
{
    NS_ENSURE_ARG_POINTER(result);
    *result = mAccessGranted;
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetStoragePolicy(nsCacheStoragePolicy *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    
    *result = mCacheEntry->StoragePolicy();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetStoragePolicy(nsCacheStoragePolicy policy)
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    // XXX validate policy against session?
    
    bool        storageEnabled = false;
    storageEnabled = nsCacheService::IsStorageEnabledForPolicy_Locked(policy);
    if (!storageEnabled)    return NS_ERROR_FAILURE;

    // Don't change the storage policy of entries we can't write
    if (!(mAccessGranted & nsICache::ACCESS_WRITE))
        return NS_ERROR_NOT_AVAILABLE;
    
    // Don't allow a cache entry to move from memory-only to anything else
    if (mCacheEntry->StoragePolicy() == nsICache::STORE_IN_MEMORY &&
        policy != nsICache::STORE_IN_MEMORY)
        return NS_ERROR_NOT_AVAILABLE;
        
    mCacheEntry->SetStoragePolicy(policy);
    mCacheEntry->MarkEntryDirty();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetFile(nsIFile ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return nsCacheService::GetFileForEntry(mCacheEntry, result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetSecurityInfo(nsISupports ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->SecurityInfo();
    NS_IF_ADDREF(*result);
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetSecurityInfo(nsISupports * securityInfo)
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    mCacheEntry->SetSecurityInfo(securityInfo);
    mCacheEntry->MarkEntryDirty();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::Doom()
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return nsCacheService::DoomEntry(mCacheEntry);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::DoomAndFailPendingRequests(nsresult status)
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::MarkValid()
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    nsresult  rv = nsCacheService::ValidateEntry(mCacheEntry);
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::Close()
{
    nsCacheServiceAutoLock lock;
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    // XXX perhaps closing descriptors should clear/sever transports

    // tell nsCacheService we're going away
    nsCacheService::CloseDescriptor(this);
    NS_ASSERTION(mCacheEntry == nsnull, "mCacheEntry not null");

    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetMetaDataElement(const char *key, char **result)
{
    NS_ENSURE_ARG_POINTER(key);
    *result = nsnull;

    nsCacheServiceAutoLock lock;
    NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_NOT_AVAILABLE);

    const char *value;

    value = mCacheEntry->GetMetaDataElement(key);
    if (!value) return NS_ERROR_NOT_AVAILABLE;

    *result = NS_strdup(value);
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetMetaDataElement(const char *key, const char *value)
{
    NS_ENSURE_ARG_POINTER(key);

    nsCacheServiceAutoLock lock;
    NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_NOT_AVAILABLE);

    // XXX allow null value, for clearing key?

    nsresult rv = mCacheEntry->SetMetaDataElement(key, value);
    if (NS_SUCCEEDED(rv))
        mCacheEntry->TouchMetaData();
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::VisitMetaData(nsICacheMetaDataVisitor * visitor)
{
    nsCacheServiceAutoLock lock;  // XXX check callers, we're calling out of module
    NS_ENSURE_ARG_POINTER(visitor);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return mCacheEntry->VisitMetaDataElements(visitor);
}


/******************************************************************************
 * nsCacheInputStream - a wrapper for nsIInputStream keeps the cache entry
 *                      open while referenced.
 ******************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheEntryDescriptor::nsInputStreamWrapper,
                              nsIInputStream)

nsresult nsCacheEntryDescriptor::
nsInputStreamWrapper::LazyInit()
{
    nsCacheServiceAutoLock lock;

    nsCacheAccessMode mode;
    nsresult rv = mDescriptor->GetAccessGranted(&mode);
    if (NS_FAILED(rv)) return rv;

    NS_ENSURE_TRUE(mode & nsICache::ACCESS_READ, NS_ERROR_UNEXPECTED);

    nsCacheEntry* cacheEntry = mDescriptor->CacheEntry();
    if (!cacheEntry) return NS_ERROR_NOT_AVAILABLE;

    rv = nsCacheService::OpenInputStreamForEntry(cacheEntry, mode,
                                                 mStartOffset,
                                                 getter_AddRefs(mInput));
    if (NS_FAILED(rv)) return rv;

    mInitialized = true;
    return NS_OK;
}

nsresult nsCacheEntryDescriptor::
nsInputStreamWrapper::Close()
{
    nsresult rv = EnsureInit();
    if (NS_FAILED(rv)) return rv;

    return mInput->Close();
}

nsresult nsCacheEntryDescriptor::
nsInputStreamWrapper::Available(PRUint32 *avail)
{
    nsresult rv = EnsureInit();
    if (NS_FAILED(rv)) return rv;

    return mInput->Available(avail);
}

nsresult nsCacheEntryDescriptor::
nsInputStreamWrapper::Read(char *buf, PRUint32 count, PRUint32 *countRead)
{
    nsresult rv = EnsureInit();
    if (NS_FAILED(rv)) return rv;

    return mInput->Read(buf, count, countRead);
}

nsresult nsCacheEntryDescriptor::
nsInputStreamWrapper::ReadSegments(nsWriteSegmentFun writer, void *closure,
                                   PRUint32 count, PRUint32 *countRead)
{
    // cache stream not buffered
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsCacheEntryDescriptor::
nsInputStreamWrapper::IsNonBlocking(bool *result)
{
    // cache streams will never return NS_BASE_STREAM_WOULD_BLOCK
    *result = false;
    return NS_OK;
}


/******************************************************************************
 * nsDecompressInputStreamWrapper - an input stream wrapper that decompresses
 ******************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheEntryDescriptor::nsDecompressInputStreamWrapper,
                              nsIInputStream)

NS_IMETHODIMP nsCacheEntryDescriptor::
nsDecompressInputStreamWrapper::Read(char *    buf, 
                                     PRUint32  count, 
                                     PRUint32 *countRead)
{
    int zerr = Z_OK;
    nsresult rv = NS_OK;

    if (!mStreamInitialized) {
        rv = InitZstream();
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    mZstream.next_out = (Bytef*)buf;
    mZstream.avail_out = count;

    if (mReadBufferLen < count) {
        // Allocate a buffer for reading from the input stream. This will
        // determine the max number of compressed bytes read from the
        // input stream at one time. Making the buffer size proportional
        // to the request size is not necessary, but helps minimize the
        // number of read requests to the input stream.
        PRUint32 newBufLen = NS_MAX(count, (PRUint32)kMinDecompressReadBufLen);
        unsigned char* newBuf;
        newBuf = (unsigned char*)nsMemory::Realloc(mReadBuffer, 
            newBufLen);
        if (newBuf) {
            mReadBuffer = newBuf;
            mReadBufferLen = newBufLen;
        }
        if (!mReadBuffer) {
            mReadBufferLen = 0;
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    // read and inflate data until the output buffer is full, or
    // there is no more data to read
    while (NS_SUCCEEDED(rv) &&
           zerr == Z_OK && 
           mZstream.avail_out > 0 &&
           count > 0) {
        if (mZstream.avail_in == 0) {
            rv = nsInputStreamWrapper::Read((char*)mReadBuffer, 
                                            mReadBufferLen, 
                                            &mZstream.avail_in);
            if (NS_FAILED(rv) || !mZstream.avail_in) {
                break;
            }
            mZstream.next_in = mReadBuffer;
        }
        zerr = inflate(&mZstream, Z_NO_FLUSH);
        if (zerr == Z_STREAM_END) {
            // The compressed data may have been stored in multiple
            // chunks/streams. To allow for this case, re-initialize 
            // the inflate stream and continue decompressing from 
            // the next byte.
            Bytef * saveNextIn = mZstream.next_in;
            unsigned int saveAvailIn = mZstream.avail_in;
            Bytef * saveNextOut = mZstream.next_out;
            unsigned int saveAvailOut = mZstream.avail_out;
            inflateReset(&mZstream);
            mZstream.next_in = saveNextIn;
            mZstream.avail_in = saveAvailIn;
            mZstream.next_out = saveNextOut;
            mZstream.avail_out = saveAvailOut;
            zerr = Z_OK;
        } else if (zerr != Z_OK) {
            rv = NS_ERROR_INVALID_CONTENT_ENCODING;
        }
    }
    if (NS_SUCCEEDED(rv)) {
        *countRead = count - mZstream.avail_out;
    }
    return rv;
}

nsresult nsCacheEntryDescriptor::
nsDecompressInputStreamWrapper::Close()
{
    EndZstream();
    if (mReadBuffer) {
        nsMemory::Free(mReadBuffer);
        mReadBuffer = 0;
        mReadBufferLen = 0;
    }
    return nsInputStreamWrapper::Close();
}

nsresult nsCacheEntryDescriptor::
nsDecompressInputStreamWrapper::InitZstream()
{
    // Initialize zlib inflate stream
    mZstream.zalloc = Z_NULL;
    mZstream.zfree = Z_NULL;
    mZstream.opaque = Z_NULL;
    mZstream.next_out = Z_NULL;
    mZstream.avail_out = 0;
    mZstream.next_in = Z_NULL;
    mZstream.avail_in = 0;
    if (inflateInit(&mZstream) != Z_OK) {
        return NS_ERROR_FAILURE;
    }
    mStreamInitialized = true;
    return NS_OK;
}

nsresult nsCacheEntryDescriptor::
nsDecompressInputStreamWrapper::EndZstream()
{
    if (mStreamInitialized && !mStreamEnded) {
        inflateEnd(&mZstream);
        mStreamEnded = true;
    }
    return NS_OK;
}


/******************************************************************************
 * nsOutputStreamWrapper - a wrapper for nsIOutputstream to track the amount of
 *                         data written to a cache entry.
 *                       - also keeps the cache entry open while referenced.
 ******************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheEntryDescriptor::nsOutputStreamWrapper,
                              nsIOutputStream)

nsresult nsCacheEntryDescriptor::
nsOutputStreamWrapper::LazyInit()
{
    nsCacheServiceAutoLock lock;

    nsCacheAccessMode mode;
    nsresult rv = mDescriptor->GetAccessGranted(&mode);
    if (NS_FAILED(rv)) return rv;

    NS_ENSURE_TRUE(mode & nsICache::ACCESS_WRITE, NS_ERROR_UNEXPECTED);

    nsCacheEntry* cacheEntry = mDescriptor->CacheEntry();
    if (!cacheEntry) return NS_ERROR_NOT_AVAILABLE;

    NS_ASSERTION(mOutput == nsnull, "mOutput set in LazyInit");

    nsCOMPtr<nsIOutputStream> stream;
    rv = nsCacheService::OpenOutputStreamForEntry(cacheEntry, mode, mStartOffset,
                                                  getter_AddRefs(stream));
    if (NS_FAILED(rv))
        return rv;

    nsCacheDevice* device = cacheEntry->CacheDevice();
    if (device) {
        // the entry has been truncated to mStartOffset bytes, inform device
        PRInt32 size = cacheEntry->DataSize();
        rv = device->OnDataSizeChange(cacheEntry, mStartOffset - size);
        if (NS_SUCCEEDED(rv))
            cacheEntry->SetDataSize(mStartOffset);
    } else {
        rv = NS_ERROR_NOT_AVAILABLE;
    }

    // If anything above failed, clean up internal state and get out of here
    // (see bug #654926)...
    if (NS_FAILED(rv)) {
        mDescriptor->InternalCleanup(stream);
        return rv;
    }

    // ... otherwise, set members and mark initialized
    mDescriptor->mOutput = mOutput = stream;
    mInitialized = true;
    return NS_OK;
}

nsresult nsCacheEntryDescriptor::
nsOutputStreamWrapper::OnWrite(PRUint32 count)
{
    if (count > PR_INT32_MAX)  return NS_ERROR_UNEXPECTED;
    return mDescriptor->RequestDataSizeChange((PRInt32)count);
}

NS_IMETHODIMP nsCacheEntryDescriptor::
nsOutputStreamWrapper::Close()
{
    nsresult rv = EnsureInit();
    if (NS_FAILED(rv)) return rv;

    return mOutput->Close();
}

NS_IMETHODIMP nsCacheEntryDescriptor::
nsOutputStreamWrapper::Flush()
{
    nsresult rv = EnsureInit();
    if (NS_FAILED(rv)) return rv;

    return mOutput->Flush();
}

NS_IMETHODIMP nsCacheEntryDescriptor::
nsOutputStreamWrapper::Write(const char * buf,
                             PRUint32     count,
                             PRUint32 *   result)
{
    nsresult rv = EnsureInit();
    if (NS_FAILED(rv)) return rv;

    rv = OnWrite(count);
    if (NS_FAILED(rv)) return rv;

    return mOutput->Write(buf, count, result);
}

NS_IMETHODIMP nsCacheEntryDescriptor::
nsOutputStreamWrapper::WriteFrom(nsIInputStream * inStr,
                                 PRUint32         count,
                                 PRUint32 *       result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCacheEntryDescriptor::
nsOutputStreamWrapper::WriteSegments(nsReadSegmentFun  reader,
                                     void *            closure,
                                     PRUint32          count,
                                     PRUint32 *        result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCacheEntryDescriptor::
nsOutputStreamWrapper::IsNonBlocking(bool *result)
{
    // cache streams will never return NS_BASE_STREAM_WOULD_BLOCK
    *result = false;
    return NS_OK;
}


/******************************************************************************
 * nsCompressOutputStreamWrapper - an output stream wrapper that compresses
 *   data before it is written
 ******************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheEntryDescriptor::nsCompressOutputStreamWrapper,
                              nsIOutputStream)

NS_IMETHODIMP nsCacheEntryDescriptor::
nsCompressOutputStreamWrapper::Write(const char * buf,
                                     PRUint32     count,
                                     PRUint32 *   result)
{
    int zerr = Z_OK;
    nsresult rv = NS_OK;

    if (!mStreamInitialized) {
        rv = InitZstream();
        if (NS_FAILED(rv)) {
            return rv;
        }
    }

    if (!mWriteBuffer) {
        // Once allocated, this buffer is referenced by the zlib stream and
        // cannot be grown. We use 2x(initial write request) to approximate
        // a stream buffer size proportional to request buffers.
        mWriteBufferLen = NS_MAX(count*2, (PRUint32)kMinCompressWriteBufLen);
        mWriteBuffer = (unsigned char*)nsMemory::Alloc(mWriteBufferLen);
        if (!mWriteBuffer) {
            mWriteBufferLen = 0;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        mZstream.next_out = mWriteBuffer;
        mZstream.avail_out = mWriteBufferLen;
    }

    // Compress (deflate) the requested buffer. Keep going
    // until the entire buffer has been deflated.
    mZstream.avail_in = count;
    mZstream.next_in = (Bytef*)buf;
    while (mZstream.avail_in > 0) {
        zerr = deflate(&mZstream, Z_NO_FLUSH);
        if (zerr == Z_STREAM_ERROR) {
            deflateEnd(&mZstream);
            mStreamInitialized = false;
            return NS_ERROR_FAILURE;
        }
        // Note: Z_BUF_ERROR is non-fatal and sometimes expected here.

        // If the compression stream output buffer is filled, write
        // it out to the underlying stream wrapper.
        if (mZstream.avail_out == 0) {
            rv = WriteBuffer();
            if (NS_FAILED(rv)) {
                deflateEnd(&mZstream);
                mStreamInitialized = false;
                return rv;
            }
        }
    }
    *result = count;
    mUncompressedCount += *result;
    return NS_OK;
}

NS_IMETHODIMP nsCacheEntryDescriptor::
nsCompressOutputStreamWrapper::Close()
{
    nsresult rv = NS_OK;
    int zerr = 0;

    if (mStreamInitialized) {
        // complete compression of any data remaining in the zlib stream
        do {
            zerr = deflate(&mZstream, Z_FINISH);
            rv = WriteBuffer();
        } while (zerr == Z_OK && rv == NS_OK);
        deflateEnd(&mZstream);
    }

    if (mDescriptor->CacheEntry()) {
        nsCAutoString uncompressedLenStr;
        rv = mDescriptor->GetMetaDataElement("uncompressed-len",
                                             getter_Copies(uncompressedLenStr));
        if (NS_SUCCEEDED(rv)) {
            PRInt32 oldCount = uncompressedLenStr.ToInteger(&rv);
            if (NS_SUCCEEDED(rv)) {
                mUncompressedCount += oldCount;
            }
        }
        uncompressedLenStr.Adopt(0);
        uncompressedLenStr.AppendInt(mUncompressedCount);
        rv = mDescriptor->SetMetaDataElement("uncompressed-len",
            uncompressedLenStr.get());
    }

    if (mWriteBuffer) {
        nsMemory::Free(mWriteBuffer);
        mWriteBuffer = 0;
        mWriteBufferLen = 0;
    }

    return nsOutputStreamWrapper::Close();
}

nsresult nsCacheEntryDescriptor::
nsCompressOutputStreamWrapper::InitZstream()
{
    // Determine compression level: Aggressive compression
    // may impact performance on mobile devices, while a
    // lower compression level still provides substantial
    // space savings for many text streams.
    PRInt32 compressionLevel = nsCacheService::CacheCompressionLevel();

    // Initialize zlib deflate stream
    mZstream.zalloc = Z_NULL;
    mZstream.zfree = Z_NULL;
    mZstream.opaque = Z_NULL;
    if (deflateInit2(&mZstream, compressionLevel, Z_DEFLATED,
                     MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return NS_ERROR_FAILURE;
    }
    mZstream.next_in = Z_NULL;
    mZstream.avail_in = 0;

    mStreamInitialized = true;

    return NS_OK;
}

nsresult nsCacheEntryDescriptor::
nsCompressOutputStreamWrapper::WriteBuffer()
{
    PRUint32 bytesToWrite = mWriteBufferLen - mZstream.avail_out;
    PRUint32 result = 0;
    nsresult rv = nsCacheEntryDescriptor::nsOutputStreamWrapper::Write(
        (const char *)mWriteBuffer, bytesToWrite, &result);
    mZstream.next_out = mWriteBuffer;
    mZstream.avail_out = mWriteBufferLen;
    return rv;
}

