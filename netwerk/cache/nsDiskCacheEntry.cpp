/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDiskCache.h"
#include "nsDiskCacheEntry.h"
#include "nsDiskCacheBinding.h"
#include "nsCRT.h"

#include "nsCache.h"

#include "nsISerializable.h"
#include "nsSerializationHelper.h"

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
    nsCacheEntry * entry = nullptr;
    nsresult       rv = nsCacheEntry::Create(Key(),
                                             nsICache::STREAM_BASED,
                                             nsICache::STORE_ON_DISK,
                                             device,
                                             &entry);
    if (NS_FAILED(rv) || !entry) return nullptr;
    
    entry->SetFetchCount(mFetchCount);
    entry->SetLastFetched(mLastFetched);
    entry->SetLastModified(mLastModified);
    entry->SetExpirationTime(mExpirationTime);
    entry->SetCacheDevice(device);
    // XXX why does nsCacheService have to fill out device in BindEntry()?
    entry->SetDataSize(mDataSize);
    
    rv = entry->UnflattenMetaData(MetaData(), mMetaDataSize);
    if (NS_FAILED(rv)) {
        delete entry;
        return nullptr;
    }

    // Restore security info, if present
    const char* info = entry->GetMetaDataElement("security-info");
    if (info) {
        nsCOMPtr<nsISupports> infoObj;
        rv = NS_DeserializeObject(nsDependentCString(info),
                                  getter_AddRefs(infoObj));
        if (NS_FAILED(rv)) {
            delete entry;
            return nullptr;
        }
        entry->SetSecurityInfo(infoObj);
    }

    return entry;                      
}


/******************************************************************************
 *  nsDiskCacheEntryInfo
 *****************************************************************************/

NS_IMPL_ISUPPORTS1(nsDiskCacheEntryInfo, nsICacheEntryInfo)

NS_IMETHODIMP nsDiskCacheEntryInfo::GetClientID(char ** clientID)
{
    NS_ENSURE_ARG_POINTER(clientID);
    return ClientIDFromCacheKey(nsDependentCString(mDiskEntry->Key()), clientID);
}

extern const char DISK_CACHE_DEVICE_ID[];
NS_IMETHODIMP nsDiskCacheEntryInfo::GetDeviceID(char ** deviceID)
{
    NS_ENSURE_ARG_POINTER(deviceID);
    *deviceID = NS_strdup(mDeviceID);
    return *deviceID ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP nsDiskCacheEntryInfo::GetKey(nsACString &clientKey)
{
    return ClientKeyFromCacheKey(nsDependentCString(mDiskEntry->Key()), clientKey);
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

NS_IMETHODIMP nsDiskCacheEntryInfo::IsStreamBased(bool *aStreamBased)
{
    NS_ENSURE_ARG_POINTER(aStreamBased);
    *aStreamBased = true;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetDataSize(PRUint32 *aDataSize)
{
    NS_ENSURE_ARG_POINTER(aDataSize);
    *aDataSize = mDiskEntry->mDataSize;
    return NS_OK;
}
