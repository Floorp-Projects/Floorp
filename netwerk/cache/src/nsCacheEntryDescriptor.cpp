/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is nsCacheEntryDescriptor.cpp, released
 * February 22, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan, 22-February-2001
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsICache.h"
#include "nsCache.h"
#include "nsCacheService.h"
#include "nsCacheEntryDescriptor.h"
#include "nsCacheEntry.h"
#include "nsReadableUtils.h"
#include "nsIOutputStream.h"
#include "nsCRT.h"
#include "nsAutoLock.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsCacheEntryDescriptor,
                              nsICacheEntryDescriptor,
                              nsICacheEntryInfo)

nsCacheEntryDescriptor::nsCacheEntryDescriptor(nsCacheEntry * entry,
                                               nsCacheAccessMode accessGranted)
    : mCacheEntry(entry),
      mAccessGranted(accessGranted)
{
    PR_INIT_CLIST(this);
    NS_ADDREF(nsCacheService::GlobalInstance());  // ensure it lives for the lifetime of the descriptor
}


nsCacheEntryDescriptor::~nsCacheEntryDescriptor()
{
    Close();
    nsCacheService * service = nsCacheService::GlobalInstance();
    NS_RELEASE(service);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetClientID(char ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return ClientIDFromCacheKey(*(mCacheEntry->Key()), result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetDeviceID(char ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = nsCRT::strdup(mCacheEntry->GetDeviceID());
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetKey(char ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return ClientKeyFromCacheKey(*(mCacheEntry->Key()), result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetFetchCount(PRInt32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->FetchCount();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetLastFetched(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->LastFetched();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetLastModified(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->LastModified();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetExpirationTime(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->ExpirationTime();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetExpirationTime(PRUint32 expirationTime)
{
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    mCacheEntry->SetExpirationTime(expirationTime);
    mCacheEntry->MarkEntryDirty();
    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::IsStreamBased(PRBool *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->IsStreamData();
    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::GetDataSize(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->DataSize();
    return NS_OK;
}


nsresult
nsCacheEntryDescriptor::RequestDataSizeChange(PRInt32 deltaSize)
{
    nsAutoLock  lock(nsCacheService::ServiceLock());
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
    nsAutoLock  lock(nsCacheService::ServiceLock());
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
        nsAutoLock  lock(nsCacheService::ServiceLock());
        if (!mCacheEntry)                  return NS_ERROR_NOT_AVAILABLE;
        if (!mCacheEntry->IsStreamData())  return NS_ERROR_CACHE_DATA_IS_NOT_STREAM;

        // ensure valid permissions
        if (!(mAccessGranted & nsICache::ACCESS_READ))
            return NS_ERROR_CACHE_READ_ACCESS_DENIED;
    }

    nsInputStreamWrapper* cacheInput =
        new nsInputStreamWrapper(this, offset);
    if (!cacheInput) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result = cacheInput);
    return NS_OK;
}

NS_IMETHODIMP
nsCacheEntryDescriptor::OpenOutputStream(PRUint32 offset, nsIOutputStream ** result)
{
    NS_ENSURE_ARG_POINTER(result);

    {
        nsAutoLock  lock(nsCacheService::ServiceLock());
        if (!mCacheEntry)                  return NS_ERROR_NOT_AVAILABLE;
        if (!mCacheEntry->IsStreamData())  return NS_ERROR_CACHE_DATA_IS_NOT_STREAM;

        // ensure valid permissions
        if (!(mAccessGranted & nsICache::ACCESS_WRITE))
            return NS_ERROR_CACHE_WRITE_ACCESS_DENIED;
    }

    nsOutputStreamWrapper* cacheOutput =
        new nsOutputStreamWrapper(this, offset);
    if (!cacheOutput) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result = cacheOutput);
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetCacheElement(nsISupports ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)                 return NS_ERROR_NOT_AVAILABLE;
    if (mCacheEntry->IsStreamData())  return NS_ERROR_CACHE_DATA_IS_STREAM;

    NS_IF_ADDREF(*result = mCacheEntry->Data());
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetCacheElement(nsISupports * cacheElement)
{
    nsAutoLock  lock(nsCacheService::ServiceLock());
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
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    
    return mCacheEntry->StoragePolicy();
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetStoragePolicy(nsCacheStoragePolicy policy)
{
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    // XXX validate policy against session?
    
    PRBool      storageEnabled = PR_FALSE;
    storageEnabled = nsCacheService::IsStorageEnabledForPolicy_Locked(policy);
    if (!storageEnabled)    return NS_ERROR_FAILURE;
    
    mCacheEntry->SetStoragePolicy(policy);
    mCacheEntry->MarkEntryDirty();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetFile(nsIFile ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return nsCacheService::GetFileForEntry(mCacheEntry, result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetSecurityInfo(nsISupports ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return mCacheEntry->GetSecurityInfo(result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetSecurityInfo(nsISupports * securityInfo)
{
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    mCacheEntry->SetSecurityInfo(securityInfo);
    mCacheEntry->MarkEntryDirty();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::Doom()
{
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return nsCacheService::DoomEntry(mCacheEntry);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::DoomAndFailPendingRequests(nsresult status)
{
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::MarkValid()
{
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    nsresult  rv = nsCacheService::ValidateEntry(mCacheEntry);
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::Close()
{
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

// XXX perhaps closing descriptors should clear/sever transports

    // tell nsCacheService we're going away
    nsCacheService::CloseDescriptor(this);
    NS_ASSERTION(mCacheEntry == nsnull, "mCacheEntry not null");

    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetMetaDataElement(const char *key, char ** result)
{
    *result = nsnull;

    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    if (!key | !result) return NS_ERROR_NULL_POINTER;
    const char *value;

    value = mCacheEntry->GetMetaDataElement(key);
    if (!value) return NS_ERROR_NOT_AVAILABLE;

    *result = PL_strdup(value);
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetMetaDataElement(const char *key, const char *value)
{
    nsAutoLock  lock(nsCacheService::ServiceLock());
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    if (!key) return NS_ERROR_NULL_POINTER;

    // XXX allow null value, for clearing key?

    nsresult rv = mCacheEntry->SetMetaDataElement(key, value);
    if (NS_SUCCEEDED(rv))
        mCacheEntry->TouchMetaData();
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::VisitMetaData(nsICacheMetaDataVisitor * visitor)
{
    nsAutoLock  lock(nsCacheService::ServiceLock());      // XXX check callers, we're calling out of module
    NS_ENSURE_ARG_POINTER(visitor);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return mCacheEntry->VisitMetaDataElements(visitor);
}


/******************************************************************************
 * nsCacheInputStream - a wrapper for nsIInputstream keeps the cache entry
 *                      open while referenced.
 ******************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheEntryDescriptor::nsInputStreamWrapper,
                              nsIInputStream)

nsresult nsCacheEntryDescriptor::
nsInputStreamWrapper::LazyInit()
{
    nsAutoLock lock(nsCacheService::ServiceLock());

    nsCacheAccessMode mode;
    nsresult rv = mDescriptor->GetAccessGranted(&mode);
    if (NS_FAILED(rv)) return rv;

    NS_ENSURE_TRUE(mode & nsICache::ACCESS_READ, NS_ERROR_UNEXPECTED);

    nsCacheEntry* cacheEntry = mDescriptor->CacheEntry();
    if (!cacheEntry) return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<nsIInputStream> input;
    rv = nsCacheService::OpenInputStreamForEntry(cacheEntry, mode,
                                                 mStartOffset,
                                                 getter_AddRefs(mInput));
    if (NS_FAILED(rv)) return rv;

    mInitialized = PR_TRUE;
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
    NS_NOTREACHED("cache stream not buffered");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsCacheEntryDescriptor::
nsInputStreamWrapper::IsNonBlocking(PRBool *result)
{
    // cache streams will never return NS_BASE_STREAM_WOULD_BLOCK
    *result = PR_FALSE;
    return NS_OK;
}


/******************************************************************************
 * nsCacheOutputStream - a wrapper for nsIOutputstream to track the amount of
 *                       data written to a cache entry.
 *                     - also keeps the cache entry open while referenced.
 ******************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheEntryDescriptor::nsOutputStreamWrapper,
                              nsIOutputStream)

nsresult nsCacheEntryDescriptor::
nsOutputStreamWrapper::LazyInit()
{
    nsAutoLock lock(nsCacheService::ServiceLock());

    nsCacheAccessMode mode;
    nsresult rv = mDescriptor->GetAccessGranted(&mode);
    if (NS_FAILED(rv)) return rv;

    NS_ENSURE_TRUE(mode & nsICache::ACCESS_WRITE, NS_ERROR_UNEXPECTED);

    nsCacheEntry* cacheEntry = mDescriptor->CacheEntry();
    if (!cacheEntry) return NS_ERROR_NOT_AVAILABLE;

    rv = nsCacheService::OpenOutputStreamForEntry(cacheEntry, mode, mStartOffset,
                                                  getter_AddRefs(mOutput));
    if (NS_FAILED(rv)) return rv;

    nsCacheDevice* device = cacheEntry->CacheDevice();
    if (!device) return NS_ERROR_NOT_AVAILABLE;

    // the entry has been truncated to mStartOffset bytes, inform the device.
    PRInt32 size = cacheEntry->DataSize();
    rv = device->OnDataSizeChange(cacheEntry, mStartOffset - size);
    if (NS_FAILED(rv)) return rv;

    cacheEntry->SetDataSize(mStartOffset);

    mInitialized = PR_TRUE;
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
    NS_NOTREACHED("cache stream not buffered");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCacheEntryDescriptor::
nsOutputStreamWrapper::WriteSegments(nsReadSegmentFun  reader,
                                     void *            closure,
                                     PRUint32          count,
                                     PRUint32 *        result)
{
    NS_NOTREACHED("cache stream not buffered");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCacheEntryDescriptor::
nsOutputStreamWrapper::IsNonBlocking(PRBool *result)
{
    // cache streams will never return NS_BASE_STREAM_WOULD_BLOCK
    *result = PR_FALSE;
    return NS_OK;
}
