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
 * The Original Code is nsDiskCacheEntry.cpp, released May 10, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan  <gordon@netscape.com>
 *    Patrick C. Beard <beard@netscape.com>
 */

#include "nsDiskCache.h"
#include "nsDiskCacheEntry.h"
#include "nsDiskCacheBinding.h"
#include "nsDiskCacheMap.h"
#include "nsCRT.h"

#include "nsCache.h"


/******************************************************************************
 *  nsDiskCacheEntry
 *****************************************************************************/

/**
 *  CreateCacheEntry()
 *
 *  Creates an nsCacheEntry and sets all fields except for the binding.
 */
nsCacheEntry *
nsDiskCacheEntry::CreateCacheEntry(nsCacheDevice *  device)
{
    nsCacheEntry * entry = nsnull;
    nsresult       rv = nsCacheEntry::Create(mKeyStart,
                                             nsICache::STREAM_BASED,
                                             nsICache::STORE_ON_DISK,
                                             device,
                                             &entry);
    if (NS_FAILED(rv) || !entry) return nsnull;
    
    entry->SetFetchCount(mFetchCount);
    entry->SetLastFetched(mLastFetched);
    entry->SetLastModified(mLastModified);
    entry->SetExpirationTime(mExpirationTime);
    entry->SetCacheDevice(device);
    // XXX why does nsCacheService have to fill out device in BindEntry()?
    entry->SetDataSize(mDataSize);
    
    rv = entry->UnflattenMetaData(&mKeyStart[mKeySize], mMetaDataSize);
    if (NS_FAILED(rv)) {
        delete entry;
        return nsnull;
    }
    
    return entry;                      
}


/**
 *  CheckConsistency()
 *
 *  Perform a few simple checks to verify the data looks reasonable.
 */
PRBool
nsDiskCacheEntry::CheckConsistency(PRUint32  size)
{
    if ((mHeaderVersion != nsDiskCache::kCurrentVersion) ||
        (Size() > size) ||
        (mKeySize == 0) ||
        (mKeyStart[mKeySize - 1] != 0)) // key is null terminated
        return PR_FALSE;
    
    return PR_TRUE;
}


/**
 *  CreateDiskCacheEntry(nsCacheEntry * entry)
 *
 *  Prepare an nsCacheEntry for writing to disk
 */
nsDiskCacheEntry *
CreateDiskCacheEntry(nsDiskCacheBinding *  binding)
{
    nsCacheEntry * entry = binding->mCacheEntry;
    if (!entry)  return nsnull;
    
    PRUint32  keySize  = entry->Key()->Length() + 1;
    PRUint32  metaSize = entry->MetaDataSize();
    PRUint32  size     = sizeof(nsDiskCacheEntry) + keySize + metaSize;
    
    // pad size so we can write to block files without overrunning buffer
    PRInt32 pad;
    if      (size <=  1024) pad = (((size-1)/ 256) + 1) *  256;
    else if (size <=  4096) pad = (((size-1)/1024) + 1) * 1024;
    else if (size <= 16384) pad = (((size-1)/4096) + 1) * 4096;
    else return nsnull; // unexpected size!
    
    nsDiskCacheEntry * diskEntry = (nsDiskCacheEntry *)new char[pad];
    if (!diskEntry)  return nsnull;
    
    diskEntry->mHeaderVersion   = nsDiskCache::kCurrentVersion;
    diskEntry->mMetaLocation    = binding->mRecord.MetaLocation();
    diskEntry->mFetchCount      = entry->FetchCount();
    diskEntry->mLastFetched     = entry->LastFetched();
    diskEntry->mLastModified    = entry->LastModified();
    diskEntry->mExpirationTime  = entry->ExpirationTime();
    diskEntry->mDataSize        = entry->DataSize();
    diskEntry->mKeySize         = keySize;
    diskEntry->mMetaDataSize    = metaSize;
    
    memcpy(diskEntry->mKeyStart, entry->Key()->get(),keySize);
    
    nsresult rv = entry->FlattenMetaData(&diskEntry->mKeyStart[keySize], metaSize);
    if (NS_FAILED(rv)) {
        delete [] (char *)diskEntry;
        return nsnull;
    }
        
    pad -= diskEntry->Size();
    NS_ASSERTION(pad >= 0, "under allocated buffer for diskEntry.");
    if (pad > 0)
        memset(&diskEntry->mKeyStart[keySize+metaSize], 0, pad);
    
    return  diskEntry;
}


/******************************************************************************
 *  nsDiskCacheEntryInfo
 *****************************************************************************/

NS_IMPL_ISUPPORTS1(nsDiskCacheEntryInfo, nsICacheEntryInfo)

NS_IMETHODIMP nsDiskCacheEntryInfo::GetClientID(char ** clientID)
{
    NS_ENSURE_ARG_POINTER(clientID);
    return ClientIDFromCacheKey(nsDependentCString(mDiskEntry->mKeyStart), clientID);
}

extern const char DISK_CACHE_DEVICE_ID[];
NS_IMETHODIMP nsDiskCacheEntryInfo::GetDeviceID(char ** deviceID)
{
    NS_ENSURE_ARG_POINTER(deviceID);
    *deviceID = nsCRT::strdup(mDeviceID);
    return *deviceID ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP nsDiskCacheEntryInfo::GetKey(char ** clientKey)
{
    NS_ENSURE_ARG_POINTER(clientKey);
    return ClientKeyFromCacheKey(nsDependentCString(mDiskEntry->mKeyStart), clientKey);
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetFetchCount(PRInt32 *aFetchCount)
{
    NS_ENSURE_ARG_POINTER(aFetchCount);
    *aFetchCount = mDiskEntry->mFetchCount;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetLastFetched(PRUint32 *aLastFetched)
{
    NS_ENSURE_ARG_POINTER(aLastFetched);
    *aLastFetched = mDiskEntry->mLastFetched;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetLastModified(PRUint32 *aLastModified)
{
    NS_ENSURE_ARG_POINTER(aLastModified);
    *aLastModified = mDiskEntry->mLastModified;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetExpirationTime(PRUint32 *aExpirationTime)
{
    NS_ENSURE_ARG_POINTER(aExpirationTime);
    *aExpirationTime = mDiskEntry->mExpirationTime;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::IsStreamBased(PRBool *aStreamBased)
{
    NS_ENSURE_ARG_POINTER(aStreamBased);
    *aStreamBased = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetDataSize(PRUint32 *aDataSize)
{
    NS_ENSURE_ARG_POINTER(aDataSize);
    *aDataSize = mDiskEntry->mDataSize;
    return NS_OK;
}
