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
#include "nsCacheService.h"
#include "nsCacheEntryDescriptor.h"
#include "nsCacheEntry.h"
#include "nsReadableUtils.h"

NS_IMPL_ISUPPORTS1(nsCacheEntryDescriptor, nsICacheEntryDescriptor)


nsCacheEntryDescriptor::nsCacheEntryDescriptor(nsCacheEntry * entry,
                                               nsCacheAccessMode accessGranted)
    : mCacheEntry(entry), mAccessGranted(accessGranted)
{
  NS_INIT_ISUPPORTS();
  PR_INIT_CLIST(&mListLink);
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
nsCacheEntryDescriptor::GetKey(char ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    nsCString * key;
    nsresult    rv = NS_OK;

    *result = nsnull;
    key = mCacheEntry->Key();

    nsReadingIterator<char> start;
    key->BeginReading(start);
        
    nsReadingIterator<char> end;
    key->EndReading(end);
        
    if (FindCharInReadable(':', start, end)) {
        ++start;  // advance past clientID ':' delimiter
        *result = ToNewCString( Substring(start, end));
        if (!*result) rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        NS_ASSERTION(PR_FALSE, "FindCharInRead failed to find ':'");
        rv = NS_ERROR_UNEXPECTED;
    }
    return rv;
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
nsCacheEntryDescriptor::GetLastFetched(PRTime *result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->LastFetched();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetLastValidated(PRTime *result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->LastValidated();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetExpirationTime(PRTime *result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->ExpirationTime();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetExpirationTime(PRTime expirationTime)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    mCacheEntry->SetExpirationTime(expirationTime);
    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::IsStreamBased(PRBool *result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->IsStreamData();  //** which name is better?
    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::GetDataSize(PRUint32 *result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *result = mCacheEntry->DataSize();
    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::SetDataSize(PRUint32 dataSize)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    mCacheEntry->SetDataSize(dataSize);
    return NS_OK;
}


NS_IMETHODIMP nsCacheEntryDescriptor::GetTransport(nsITransport ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)                  return NS_ERROR_NOT_AVAILABLE;
    if (!mCacheEntry->IsStreamData())  return NS_ERROR_CACHE_DATA_IS_NOT_STREAM;

    NS_ADDREF(*result = this);
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

    return mCacheEntry->SetData(cacheElement);
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
    
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetStoragePolicy(nsCacheStoragePolicy policy)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return NS_ERROR_NOT_IMPLEMENTED;
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
    if (mCacheEntry) {
        mCacheEntry->MarkValid();
        return NS_OK;
    }
    return NS_ERROR_NOT_AVAILABLE;
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
    
    nsresult rv = mCacheEntry->GetMetaDataElement(nsLiteralCString(key), &value);
    if (NS_SUCCEEDED(rv) && (value)) {
        *result = ToNewCString(*value);
        if (!*result) rv = NS_ERROR_OUT_OF_MEMORY;
        else {
            NS_ASSERTION(PR_FALSE, "FindCharInRead failed to find ':'");
            rv = NS_ERROR_UNEXPECTED;
        }
    }
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetMetadataElement(const char *key, const char *value)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    if (!key) return NS_ERROR_NULL_POINTER;

    //** allow null value, for clearing key?
    nsresult rv = mCacheEntry->SetMetaDataElement(nsLiteralCString(key),
                                                  nsLiteralCString(value));
    return rv;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetMetaDataEnumerator(nsISimpleEnumerator ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetSecurityInfo(nsISupports ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::GetProgressEventSink(nsIProgressEventSink ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::SetProgressEventSink(nsIProgressEventSink * progressEventSink)
{
    if (!mCacheEntry)       return NS_ERROR_NOT_AVAILABLE;
    if (!progressEventSink) return NS_ERROR_NULL_POINTER;

    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCacheEntryDescriptor::OpenInputStream(PRUint32           offset,
                                        PRUint32           count,
                                        PRUint32           flags,
                                        nsIInputStream  ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    if (!(mAccessGranted & nsICache::ACCESS_READ))
        return NS_ERROR_CACHE_READ_ACCESS_DENIED;
    if (!mTransport) {
        nsresult rv;
        rv = nsCacheService::GlobalInstance()->
            GetTransportForEntry(mCacheEntry,
                                 mAccessGranted,
                                 getter_AddRefs(mTransport));
        if (NS_FAILED(rv))  return rv;
    }
    
    return mTransport->OpenInputStream(offset, count, flags, result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::OpenOutputStream(PRUint32            offset,
                                         PRUint32            count,
                                         PRUint32            flags,
                                         nsIOutputStream  ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    if (!(mAccessGranted & nsICache::ACCESS_WRITE))
        return NS_ERROR_CACHE_WRITE_ACCESS_DENIED;
    if (!mTransport) {
        nsresult rv;
        rv = nsCacheService::GlobalInstance()->
            GetTransportForEntry(mCacheEntry,
                                 mAccessGranted,
                                 getter_AddRefs(mTransport));
        if (NS_FAILED(rv))  return rv;
    }
    
    return mTransport->OpenOutputStream(offset, count, flags, result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::AsyncRead(nsIStreamListener * listener,
                                  nsISupports *       ctxt,
                                  PRUint32            offset,
                                  PRUint32            count,
                                  PRUint32            flags,
                                  nsIRequest       ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    if (!(mAccessGranted & nsICache::ACCESS_READ))
        return NS_ERROR_CACHE_READ_ACCESS_DENIED;
    if (!mTransport) {
        nsresult rv;
        rv = nsCacheService::GlobalInstance()->
            GetTransportForEntry(mCacheEntry,
                                 mAccessGranted,
                                 getter_AddRefs(mTransport));
        if (NS_FAILED(rv))  return rv;
    }
    
    return mTransport->AsyncRead(listener, ctxt, offset, count, flags, result);
}


NS_IMETHODIMP
nsCacheEntryDescriptor::AsyncWrite(nsIStreamProvider * provider,
                                   nsISupports *       ctxt,
                                   PRUint32            offset, 
                                   PRUint32            count, 
                                   PRUint32            flags, 
                                   nsIRequest       ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    if (!(mAccessGranted & nsICache::ACCESS_WRITE))
        return NS_ERROR_CACHE_WRITE_ACCESS_DENIED;
    if (!mTransport) {
        nsresult rv;
        rv = nsCacheService::GlobalInstance()->
            GetTransportForEntry(mCacheEntry,
                                 mAccessGranted,
                                 getter_AddRefs(mTransport));
        if (NS_FAILED(rv))  return rv;
    }
    
    return mTransport->AsyncWrite(provider, ctxt, offset, count, flags, result);
}
