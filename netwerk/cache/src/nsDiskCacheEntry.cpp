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
 * The Original Code is nsDiskCacheEntry.cpp, released
 * May 10, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan  <gordon@netscape.com>
 *   Patrick C. Beard <beard@netscape.com>
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

#include "nsDiskCache.h"
#include "nsDiskCacheEntry.h"
#include "nsDiskCacheBinding.h"
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
    nsresult       rv = nsCacheEntry::Create(Key(),
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
    
    rv = entry->UnflattenMetaData(MetaData(), mMetaDataSize);
    if (NS_FAILED(rv)) {
        delete entry;
        return nsnull;
    }
    
    return entry;                      
}

/**
 *  CreateDiskCacheEntry(nsCacheEntry * entry)
 *
 *  Prepare an nsCacheEntry for writing to disk
 */
nsDiskCacheEntry *
CreateDiskCacheEntry(nsDiskCacheBinding *  binding,
                     PRUint32 * aSize)
{
    nsCacheEntry * entry = binding->mCacheEntry;
    if (!entry)  return nsnull;
    
    PRUint32  keySize  = entry->Key()->Length() + 1;
    PRUint32  metaSize = entry->MetaDataSize();
    PRUint32  size     = sizeof(nsDiskCacheEntry) + keySize + metaSize;
    
    if (aSize) *aSize = size;
    
    nsDiskCacheEntry * diskEntry = (nsDiskCacheEntry *)new char[size];
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
    
    memcpy(diskEntry->Key(), entry->Key()->get(),keySize);
    
    nsresult rv = entry->FlattenMetaData(diskEntry->MetaData(), metaSize);
    if (NS_FAILED(rv)) {
        delete [] (char *)diskEntry;
        return nsnull;
    }
        
    return  diskEntry;
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
    *deviceID = nsCRT::strdup(mDeviceID);
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
