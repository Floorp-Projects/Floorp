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
 * The Original Code is nsCacheEntryDescriptor.cpp, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 22-February-2001
 */

#include "nsICache.h"
#include "nsCache.h"
#include "nsCacheService.h"
#include "nsCacheEntryDescriptor.h"
#include "nsCacheEntry.h"
#include "nsReadableUtils.h"
#include "nsIOutputStream.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheEntryDescriptor, nsICacheEntryDescriptor)


nsCacheEntryDescriptor::nsCacheEntryDescriptor(nsCacheEntry * entry,
                                               nsCacheAccessMode accessGranted)
    : mCacheEntry(entry),
      mAccessGranted(accessGranted)
{
  NS_INIT_ISUPPORTS();
  PR_INIT_CLIST(this);
}


nsCacheEntryDescriptor::~nsCacheEntryDescriptor()
{
    if (mCacheEntry)
        Close();
}


nsresult
nsCacheEntryDescriptor::Create(nsCacheEntry * entry, nsCacheAccessMode  accessGranted,
                               nsICacheEntryDescriptor ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    nsresult rv = nsnull;
    
    nsCacheEntryDescriptor * descriptor =
        new nsCacheEntryDescriptor(entry, accessGranted);

    if (descriptor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(descriptor);
    rv = descriptor->QueryInterface(NS_GET_IID(nsICacheEntryDescriptor), (void**)result);
    NS_RELEASE(descriptor);
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetClientID(char ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return ClientIDFromCacheKey(*(mCacheEntry->Key()), result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetDeviceID(char ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = nsCRT::strdup(mCacheEntry->GetDeviceID());
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetKey(char ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return ClientKeyFromCacheKey(*(mCacheEntry->Key()), result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetFetchCount(PRInt32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->FetchCount();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetLastFetched(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->LastFetched();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetLastModified(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->LastModified();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetExpirationTime(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->ExpirationTime();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetExpirationTime(PRUint32 expirationTime)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    mCacheEntry->SetExpirationTime(expirationTime);
    mCacheEntry->MarkEntryDirty();
    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::IsStreamBased(PRBool *result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->IsStreamData();  // XXX which name is better?
    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::GetDataSize(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->DataSize();
    return NS_OK;
}


nsresult
nsCacheEntryDescriptor::RequestDataSizeChange(PRInt32 deltaSize)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    nsresult  rv;
    rv = nsCacheService::GlobalInstance()->OnDataSizeChange(mCacheEntry, deltaSize);
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
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    // XXX review for signed/unsigned math errors
    PRInt32  deltaSize = dataSize - mCacheEntry->DataSize();

    // this had better be NS_OK, this call instance is advisory
    nsresult  rv = RequestDataSizeChange(deltaSize);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed SetDataSize() on memory cache object!");
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetTransport(nsITransport ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)                  return NS_ERROR_NOT_AVAILABLE;
    if (!mCacheEntry->IsStreamData())  return NS_ERROR_CACHE_DATA_IS_NOT_STREAM;

    NS_ADDREF(*result = &mTransportWrapper);
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetCacheElement(nsISupports ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)                 return NS_ERROR_NOT_AVAILABLE;
    if (mCacheEntry->IsStreamData())  return NS_ERROR_CACHE_DATA_IS_STREAM;

    return mCacheEntry->GetData(result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetCacheElement(nsISupports * cacheElement)
{
    if (!mCacheEntry)                 return NS_ERROR_NOT_AVAILABLE;
    if (mCacheEntry->IsStreamData())  return NS_ERROR_CACHE_DATA_IS_STREAM;

    return nsCacheService::GlobalInstance()->SetCacheElement(mCacheEntry, cacheElement);
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
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    
    return mCacheEntry->StoragePolicy();
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetStoragePolicy(nsCacheStoragePolicy policy)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    // XXX validate policy against session?
    mCacheEntry->SetStoragePolicy(policy);
    mCacheEntry->MarkEntryDirty();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetFile(nsIFile ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    
    return nsCacheService::GlobalInstance()->GetFileForEntry(mCacheEntry, result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetSecurityInfo(nsISupports ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return mCacheEntry->GetSecurityInfo(result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetSecurityInfo(nsISupports * securityInfo)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    mCacheEntry->SetSecurityInfo(securityInfo);
    mCacheEntry->MarkEntryDirty();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::Doom()
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return nsCacheService::GlobalInstance()->DoomEntry(mCacheEntry);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::DoomAndFailPendingRequests(nsresult status)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::MarkValid()
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    nsresult  rv;
    rv = nsCacheService::GlobalInstance()->ValidateEntry(mCacheEntry);
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::Close()
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    // tell nsCacheService we're going away
    nsCacheService::GlobalInstance()->CloseDescriptor(this);
    mCacheEntry = nsnull;

    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetMetaDataElement(const char *key, char ** result)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    if (!key | !result) return NS_ERROR_NULL_POINTER;
    nsAReadableCString *value;
    *result = nsnull;

    // XXX not thread safe    
    nsresult rv = mCacheEntry->GetMetaDataElement(nsDependentCString(key), &value);
    if (NS_FAILED(rv)) return rv;

    if (!value) return NS_ERROR_NOT_AVAILABLE;

    *result = ToNewCString(*value);
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetMetaDataElement(const char *key, const char *value)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    if (!key) return NS_ERROR_NULL_POINTER;
    // XXX not thread safe
    // XXX allow null value, for clearing key?
    nsresult rv = mCacheEntry->SetMetaDataElement(nsDependentCString(key),
                                                  nsDependentCString(value));
    if (NS_SUCCEEDED(rv))
        mCacheEntry->TouchMetaData();
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::VisitMetaData(nsICacheMetaDataVisitor * visitor)
{
    NS_ENSURE_ARG_POINTER(visitor);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return mCacheEntry->VisitMetaDataElements(visitor);
}


/******************************************************************************
 * nsCacheTransportWrapper
 ******************************************************************************/

// XXX NS_IMPL_ISUPPORTS1(nsCacheEntryDescriptor::nsTransportWrapper, nsITransport);
NS_IMPL_QUERY_INTERFACE1(nsCacheEntryDescriptor::nsTransportWrapper, nsITransport)


//  special AddRef and Release, because we are part of the descriptor
#define GET_DESCRIPTOR_FROM_TRANSPORT_WRAPPER(_this) \
        ((nsCacheEntryDescriptor*)((char*)(_this) - \
                                   offsetof(nsCacheEntryDescriptor, mTransportWrapper)))

NS_IMETHODIMP_(nsrefcnt) nsCacheEntryDescriptor::
nsTransportWrapper::AddRef(void)
{
    return GET_DESCRIPTOR_FROM_TRANSPORT_WRAPPER(this)->AddRef();
}

NS_IMETHODIMP_(nsrefcnt) nsCacheEntryDescriptor::
nsTransportWrapper::Release(void)
{
    return GET_DESCRIPTOR_FROM_TRANSPORT_WRAPPER(this)->Release();
}


nsresult nsCacheEntryDescriptor::
nsTransportWrapper::EnsureTransportWithAccess(nsCacheAccessMode  mode)
{
    nsresult  rv = NS_OK;

    nsCacheEntryDescriptor * descriptor = GET_DESCRIPTOR_FROM_TRANSPORT_WRAPPER(this);
    if (!descriptor->mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    if (!descriptor->mAccessGranted & mode) {
        rv = (mode == nsICache::ACCESS_READ) ?
            NS_ERROR_CACHE_READ_ACCESS_DENIED : NS_ERROR_CACHE_WRITE_ACCESS_DENIED;
        return rv;
    }

    if (!mTransport) {
        rv = nsCacheService::GlobalInstance()->
            GetTransportForEntry(descriptor->mCacheEntry,
                                 descriptor->mAccessGranted,
                                 getter_AddRefs(mTransport));
        if (NS_FAILED(rv))  return rv;

        if (mCallbacks) {
            mTransport->SetNotificationCallbacks(mCallbacks, mCallbackFlags);
        }
    }
    return NS_OK;
}


nsresult 
nsCacheEntryDescriptor::NewOutputStreamWrapper(nsIOutputStream **       result,
                                               nsCacheEntryDescriptor * descriptor,
                                               nsIOutputStream *        output)
{
    nsOutputStreamWrapper* cacheOutput =
        new nsOutputStreamWrapper(descriptor, output);
    if (!cacheOutput) return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsISupports> ref(cacheOutput);
    nsresult              rv = cacheOutput->Init();
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = cacheOutput);
    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::
nsTransportWrapper::GetSecurityInfo(nsISupports ** securityInfo)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsCacheEntryDescriptor::
nsTransportWrapper::GetNotificationCallbacks(nsIInterfaceRequestor **result)
{
    NS_ENSURE_ARG_POINTER(result);

    *result = mCallbacks;
    NS_IF_ADDREF(*result);

    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::
nsTransportWrapper::SetNotificationCallbacks(nsIInterfaceRequestor *requestor,
                                             PRUint32 flags)
{
    if (mTransport) {
        mTransport->SetNotificationCallbacks(requestor, flags);
    } 

    mCallbacks     = requestor;
    mCallbackFlags = flags;
    
    return NS_OK;;
}


NS_IMETHODIMP nsCacheEntryDescriptor::
nsTransportWrapper::OpenInputStream(PRUint32           offset,
                                    PRUint32           count,
                                    PRUint32           flags,
                                    nsIInputStream  ** result)
{
    NS_ENSURE_ARG_POINTER(result);

    nsresult  rv = EnsureTransportWithAccess(nsICache::ACCESS_READ);
    if (NS_FAILED(rv)) return rv;
    
    return mTransport->OpenInputStream(offset, count, flags, result);
}


NS_IMETHODIMP nsCacheEntryDescriptor::
nsTransportWrapper::OpenOutputStream(PRUint32            offset,
                                     PRUint32            count,
                                     PRUint32            flags,
                                     nsIOutputStream  ** result)
{
    NS_ENSURE_ARG_POINTER(result);

    nsresult  rv = EnsureTransportWithAccess(nsICache::ACCESS_WRITE);
    if (NS_FAILED(rv))  return rv;

    // XXX allow more than one output stream at a time on a descriptor?  Why?  

    // Create the underlying output stream using the wrapped transport.
    nsCOMPtr<nsIOutputStream> output;    
    rv = mTransport->OpenOutputStream(offset, count, flags, getter_AddRefs(output));
    if (NS_FAILED(rv)) return rv;    

    // Wrap this output stream with a stream that monitors how much data gets written,
    // maintains the cache entry's size, and informs the cache device.
    // This mechanism provides a way for the cache device to enforce space limits,
    // and to drive cache entry eviction.
    nsCacheEntryDescriptor * descriptor = GET_DESCRIPTOR_FROM_TRANSPORT_WRAPPER(this);
    
    // reset datasize of entry based on offset so OnWrite calculates delta changes correctly.
    rv = descriptor->SetDataSize(offset);
    if (NS_FAILED(rv)) return rv;
    
    return NewOutputStreamWrapper(result, descriptor, output);
}


NS_IMETHODIMP nsCacheEntryDescriptor::
nsTransportWrapper::AsyncRead(nsIStreamListener * listener,
                              nsISupports *       ctxt,
                              PRUint32            offset,
                              PRUint32            count,
                              PRUint32            flags,
                              nsIRequest       ** result)
{
    NS_ENSURE_ARG_POINTER(result);

    nsresult  rv = EnsureTransportWithAccess(nsICache::ACCESS_READ);
    if (NS_FAILED(rv))  return rv;
    
    return mTransport->AsyncRead(listener, ctxt, offset, count, flags, result);
}


NS_IMETHODIMP nsCacheEntryDescriptor::
nsTransportWrapper::AsyncWrite(nsIStreamProvider * provider,
                               nsISupports *       ctxt,
                               PRUint32            offset, 
                               PRUint32            count, 
                               PRUint32            flags, 
                               nsIRequest       ** result)
{
    // we're not planning on implementing this
    return NS_ERROR_NOT_IMPLEMENTED;

#if 0
    NS_ENSURE_ARG_POINTER(result);

    nsresult rv = EnsureTransportWithAccess(nsICache::ACCESS_WRITE);
    if (NS_FAILED(rv)) return rv;
    
    return mTransport->AsyncWrite(provider, ctxt, offset, count, flags, result);
#endif
}


/******************************************************************************
 * nsCacheOutputStream - a wrapper for nsIOutputstream to track the amount of
 *                       data written to a cache entry.
 ******************************************************************************/

NS_IMPL_ISUPPORTS1(nsCacheEntryDescriptor::nsOutputStreamWrapper, nsIOutputStream);

nsresult nsCacheEntryDescriptor::
nsOutputStreamWrapper::Init()
{
    nsCacheAccessMode mode;
    nsresult rv = mDescriptor->GetAccessGranted(&mode);
    if (NS_FAILED(rv)) return rv;

    if (mode == nsICache::ACCESS_WRITE) {
        nsCacheEntry* cacheEntry = mDescriptor->CacheEntry();
        if (!cacheEntry) return NS_ERROR_NOT_AVAILABLE;

        nsCacheDevice* device = cacheEntry->CacheDevice();
        if (!device) return NS_ERROR_NOT_AVAILABLE;

        // the entry has been truncated to zero bytes, inform the device.
        PRInt32 delta = cacheEntry->DataSize();
        rv = device->OnDataSizeChange(cacheEntry, -delta);
        cacheEntry->SetDataSize(0);
    }
    return rv;
}


NS_IMETHODIMP nsCacheEntryDescriptor::
nsOutputStreamWrapper::Write(const char * buf,
                             PRUint32     count,
                             PRUint32 *   result)
{
    nsresult rv = OnWrite(count);
    if (NS_FAILED(rv)) return rv;
    return mOutput->Write(buf, count, result);
}


NS_IMETHODIMP nsCacheEntryDescriptor::
nsOutputStreamWrapper::WriteFrom(nsIInputStream * inStr,
                                 PRUint32         count,
                                 PRUint32 *       result)
{
    nsresult rv = OnWrite(count);
    if (NS_FAILED(rv)) return rv;
    return mOutput->WriteFrom(inStr, count, result);
}


NS_IMETHODIMP nsCacheEntryDescriptor::
nsOutputStreamWrapper::WriteSegments(nsReadSegmentFun  reader,
                                    void *            closure,
                                    PRUint32          count,
                                    PRUint32 *        result)
{
    nsresult rv = OnWrite(count);
    if (NS_FAILED(rv)) return rv;
    return mOutput->WriteSegments(reader, closure, count, result);
}


nsresult nsCacheEntryDescriptor::
nsOutputStreamWrapper::OnWrite(PRUint32 count)
{
    // XXX if count > 2^31 error_write_too_big
    return mDescriptor->RequestDataSizeChange((PRInt32)count);
}


