/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *  
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Scott Furman, fur@netscape.com
 */

#include "nsINetDataCache.h"
#include "nsCacheManager.h"
#include "nsCachedNetData.h"
#include "nsReplacementPolicy.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsHashtable.h"
#include "nsIComponentManager.h"
#include "nsINetDataDiskCache.h"

// Limit the number of entries in the cache to conserve memory space
// in the nsReplacementPolicy code
#define MAX_MEM_CACHE_ENTRIES    800
#define MAX_DISK_CACHE_ENTRIES  3200

// Cache capacities in MB, overridable via APIs
#define DEFAULT_MEMORY_CACHE_CAPACITY  2000
#define DEFAULT_DISK_CACHE_CAPACITY   10000

#define CACHE_HIGH_WATER_MARK(capacity) ((PRUint32)(0.98 * (capacity)))
#define CACHE_LOW_WATER_MARK(capacity)  ((PRUint32)(0.97 * (capacity)))

nsCacheManager* gCacheManager = 0;

NS_IMPL_ISUPPORTS(nsCacheManager, NS_GET_IID(nsINetDataCacheManager))

nsCacheManager::nsCacheManager()
    : mActiveCacheRecords(0),
    mDiskCacheCapacity(DEFAULT_DISK_CACHE_CAPACITY),
    mMemCacheCapacity(DEFAULT_MEMORY_CACHE_CAPACITY)
{
    NS_ASSERTION(!gCacheManager, "Multiple cache managers created");
    gCacheManager = this;
    NS_INIT_REFCNT();
}

nsCacheManager::~nsCacheManager()
{
    gCacheManager = 0;
    delete mActiveCacheRecords;
    delete mMemSpaceManager;
    delete mDiskSpaceManager;
}

nsresult
nsCacheManager::Init()
{
    nsresult rv;

    mActiveCacheRecords = new nsHashtable(64);
    if (!mActiveCacheRecords)
        return NS_ERROR_OUT_OF_MEMORY;

    // Instantiate the memory cache component
    rv = nsComponentManager::CreateInstance(NS_NETWORK_MEMORY_CACHE_PROGID,
                                            nsnull,
                                            NS_GET_IID(nsINetDataCache),
                                            getter_AddRefs(mMemCache));
    if (NS_FAILED(rv))
        return rv;

    rv = nsComponentManager::CreateInstance(NS_NETWORK_FLAT_CACHE_PROGID,
                                            nsnull,
                                            NS_GET_IID(nsINetDataCache),

                                            getter_AddRefs(mFlatCache));

    if (NS_FAILED(rv)) {
        // For now, we don't require a flat cache module to be present
        if (rv != NS_ERROR_FACTORY_NOT_REGISTERED)
            return rv;
    }

#ifdef FILE_CACHE_IS_READY
    // Instantiate the file cache component
    rv = nsComponentManager::CreateInstance(NS_NETWORK_FILE_CACHE_PROGID,
                                            nsnull,
                                            NS_GET_IID(nsINetDataCache),
                                            getter_AddRefs(mFileCache));
    if (NS_FAILED(rv)) {
        NS_WARNING("No disk cache present");
    }
#endif

    // Set up linked list of caches in search order
    mCacheSearchChain = mMemCache;
    if (mFlatCache) {
        mMemCache->SetNextCache(mFlatCache);
        mFlatCache->SetNextCache(mFileCache);
    } else {
        mMemCache->SetNextCache(mFileCache);
    }

    // TODO - Load any extension caches here

    // Initialize replacement policy for memory cache module
    mMemSpaceManager = new nsReplacementPolicy;
    if (!mMemSpaceManager)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = mMemSpaceManager->Init(MAX_MEM_CACHE_ENTRIES);
    if (NS_FAILED(rv)) return rv;
    rv = mMemSpaceManager->AddCache(mMemCache);

    // Initialize replacement policy for disk cache modules (file
    // cache and flat cache)
    mDiskSpaceManager = new nsReplacementPolicy;
    if (!mDiskSpaceManager)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = mDiskSpaceManager->Init(MAX_DISK_CACHE_ENTRIES);
    if (NS_FAILED(rv)) return rv;
    if (mFileCache) {
        rv = mDiskSpaceManager->AddCache(mFileCache);
        if (NS_FAILED(rv)) return rv;
    }
    if (mFlatCache) {
        rv = mDiskSpaceManager->AddCache(mFlatCache);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCacheManager::GetCachedNetData(const char *aUriSpec, const char *aSecondaryKey,
                                 PRUint32 aSecondaryKeyLength,
                                 PRUint32 aFlags, nsICachedNetData* *aResult)
{
    nsCachedNetData *cachedData;
    nsresult rv;
    nsINetDataCache *cache;
    nsReplacementPolicy *spaceManager;

    if (aFlags & CACHE_AS_FILE) {
        cache = mFileCache;
        spaceManager = mDiskSpaceManager;

        // Ensure that cache is initialized
        if (mDiskCacheCapacity == (PRUint32)-1)
            return NS_ERROR_NOT_AVAILABLE;

    } else if ((aFlags & BYPASS_PERSISTENT_CACHE) || !mDiskCacheCapacity) {
        cache = mMemCache;
        spaceManager = mMemSpaceManager;
    } else {
        cache = mFlatCache ? mFlatCache : mFileCache;
        spaceManager = mDiskSpaceManager;
    }

    // Construct the cache key by appending the secondary key to the URI spec
    nsCAutoString cacheKey(aUriSpec);

    // Insert NUL at end of URI spec
    cacheKey += '\0';
    if (aSecondaryKey)
        cacheKey.Append(aSecondaryKey, aSecondaryKeyLength);

    nsStringKey key(cacheKey);
    cachedData = (nsCachedNetData*)mActiveCacheRecords->Get(&key);

    // There is no existing instance of nsCachedNetData for this URL.
    // Make one from the corresponding record in the cache module.
    if (cachedData) {
        NS_ASSERTION(cache == cachedData->mCache,
                     "Cannot yet handle simultaneously active requests for the "
                     "same URL using different caches");
        NS_ADDREF(cachedData);
    } else {
        rv = spaceManager->GetCachedNetData(cacheKey.GetBuffer(), cacheKey.Length(),
                                            cache, &cachedData);
        if (NS_FAILED(rv)) return rv;

        mActiveCacheRecords->Put(&key, cachedData);
    }
    
    *aResult = cachedData;
    return NS_OK;
}

// Remove this cache entry from the list of active ones
nsresult
nsCacheManager::NoteDormant(nsCachedNetData* aEntry)
{
    nsresult rv;
    PRUint32 keyLength;
    char* key;
    nsCOMPtr<nsINetDataCacheRecord> record;
    nsCachedNetData* deletedEntry;

    rv = aEntry->GetRecord(getter_AddRefs(record));
    if (NS_FAILED(rv)) return rv;
    
    rv = record->GetKey(&keyLength, &key);
    if (NS_FAILED(rv)) return rv;
    
    nsStringKey hashTableKey(nsCString(key, keyLength));
    deletedEntry = (nsCachedNetData*)gCacheManager->mActiveCacheRecords->Remove(&hashTableKey);
    NS_ASSERTION(deletedEntry == aEntry, "Hash table inconsistency");
    return NS_OK;
}

NS_IMETHODIMP
nsCacheManager::Contains(const char *aUriSpec, const char *aSecondaryKey,
                         PRUint32 aSecondaryKeyLength,
                         PRUint32 aFlags, PRBool *aResult)
{
    nsINetDataCache *cache;
    nsReplacementPolicy *spaceManager;
    nsCachedNetData *cachedData;

    if (aFlags & CACHE_AS_FILE) {
        cache = mFileCache;
        spaceManager = mDiskSpaceManager;
    } else if ((aFlags & BYPASS_PERSISTENT_CACHE) ||
               (!mFileCache && !mFlatCache) || !mDiskCacheCapacity) {
        cache = mMemCache;
        spaceManager = mMemSpaceManager;
    } else {
        cache = mFlatCache ? mFlatCache : mFileCache;
        spaceManager = mDiskSpaceManager;
    }

    // Construct the cache key by appending the secondary key to the URI spec
    nsCAutoString cacheKey(aUriSpec);

    // Insert NUL between URI spec and secondary key
    cacheKey += '\0';
    cacheKey.Append(aSecondaryKey, aSecondaryKeyLength);

    // Locate the record using (URI + secondary key)
    nsStringKey key(cacheKey);
    cachedData = (nsCachedNetData*)mActiveCacheRecords->Get(&key);

    if (cachedData && (cache == cachedData->mCache)) {
        *aResult = PR_TRUE;
        return NS_OK;
    } else {
        // No active cache entry, see if there is a dormant one
        return cache->Contains(cacheKey.GetBuffer(), cacheKey.Length(), aResult);
    }
}

NS_IMETHODIMP
nsCacheManager::GetNumEntries(PRUint32 *aNumEntries)
{
    nsresult rv;
    nsCOMPtr<nsISimpleEnumerator> iterator;
    nsCOMPtr<nsISupports> cacheSupports;
    nsCOMPtr<nsINetDataCache> cache;

    PRUint32 totalEntries = 0;
    
    rv = NewCacheModuleIterator(getter_AddRefs(iterator));
    if (NS_FAILED(rv)) return rv;
    while (1) {
        PRBool notDone;
        rv = iterator->HasMoreElements(&notDone);
        if (NS_FAILED(rv)) return rv;
        if (!notDone)
            break;
        
        iterator->GetNext(getter_AddRefs(cacheSupports));
        cache = do_QueryInterface(cacheSupports);
        
        PRUint32 numEntries;
        rv = cache->GetNumEntries(&numEntries);
        if (NS_FAILED(rv)) return rv;
        totalEntries += numEntries;
    }
    
    *aNumEntries = totalEntries;
    return NS_OK;
}

NS_IMETHODIMP
nsCacheManager::NewCacheEntryIterator(nsISimpleEnumerator* *aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

class CacheEnumerator : public nsISimpleEnumerator
{
public:
    CacheEnumerator(nsINetDataCache* aFirstCache):mCache(aFirstCache)
        { NS_INIT_REFCNT(); }

    virtual ~CacheEnumerator() {};
    
    NS_DECL_ISUPPORTS

    NS_IMETHODIMP
    HasMoreElements(PRBool* aMoreElements) {
        *aMoreElements = (mCache != 0);
        return NS_OK;
    }

    NS_IMETHODIMP
    GetNext(nsISupports* *aSupports) {
        *aSupports = mCache;
        if (!mCache)
            return NS_ERROR_FAILURE;
        NS_ADDREF(*aSupports);

        nsCOMPtr<nsINetDataCache> nextCache;
        nsresult rv = mCache->GetNextCache(getter_AddRefs(nextCache));
        mCache = nextCache;
        return rv;
    }

private:
    nsCOMPtr<nsINetDataCache> mCache;
};

NS_IMPL_ISUPPORTS(CacheEnumerator, NS_GET_IID(nsISimpleEnumerator))

NS_IMETHODIMP
nsCacheManager::NewCacheModuleIterator(nsISimpleEnumerator* *aResult)
{
    *aResult = new CacheEnumerator(mCacheSearchChain);
    if (!*aResult)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsCacheManager::RemoveAll(void)
{
    nsresult rv, result;
    nsCOMPtr<nsISimpleEnumerator> iterator;
    nsCOMPtr<nsINetDataCache> cache;
    nsCOMPtr<nsISupports> iSupports;

    result = NS_OK;
    rv = NewCacheModuleIterator(getter_AddRefs(iterator));
    if (NS_FAILED(rv)) return rv;
    while (1) {
        PRBool notDone;
        rv = iterator->HasMoreElements(&notDone);
        if (NS_FAILED(rv)) return rv;
        if (!notDone)
            break;
        
        iterator->GetNext(getter_AddRefs(iSupports));
        cache = do_QueryInterface(iSupports);

        PRUint32 cacheFlags;
        rv = cache->GetFlags(&cacheFlags);
        if (NS_FAILED(rv)) return rv;
        
        if ((cacheFlags & nsINetDataCache::READ_ONLY) == 0) {
            rv = cache->RemoveAll();
            if (NS_FAILED(rv))
                result = rv;
        }
    }

    return result;
}

nsresult
nsCacheManager::LimitMemCacheSize()
{
    nsresult rv;
    nsReplacementPolicy* spaceManager;

    NS_ASSERTION(gCacheManager, "No cache manager");

    spaceManager = gCacheManager->mMemSpaceManager;

    PRUint32 occupancy;
    rv = spaceManager->GetStorageInUse(&occupancy);
    if (NS_FAILED(rv)) return rv;

    PRUint32 memCacheCapacity = gCacheManager->mMemCacheCapacity;
    if (occupancy > CACHE_HIGH_WATER_MARK(memCacheCapacity))
        return spaceManager->Evict(CACHE_LOW_WATER_MARK(memCacheCapacity));

    return NS_OK;
}

nsresult
nsCacheManager::LimitDiskCacheSize()
{
    nsresult rv;
    nsReplacementPolicy* spaceManager;

    NS_ASSERTION(gCacheManager, "No cache manager");

    spaceManager = gCacheManager->mDiskSpaceManager;

    PRUint32 occupancy;
    rv = spaceManager->GetStorageInUse(&occupancy);
    if (NS_FAILED(rv)) return rv;

    PRUint32 diskCacheCapacity = gCacheManager->mDiskCacheCapacity;
    if (occupancy > CACHE_HIGH_WATER_MARK(diskCacheCapacity))
        return spaceManager->Evict(CACHE_LOW_WATER_MARK(diskCacheCapacity));

    return NS_OK;
}

nsresult
nsCacheManager::LimitCacheSize()
{
    nsresult rv;

    rv = LimitDiskCacheSize();
    if (NS_FAILED(rv)) return rv;

    rv = LimitMemCacheSize();
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsCacheManager::SetMemCacheCapacity(PRUint32 aCapacity)
{
    mMemCacheCapacity = aCapacity;
    LimitCacheSize();
    return NS_OK;
}

NS_IMETHODIMP
nsCacheManager::GetMemCacheCapacity(PRUint32* aCapacity)
{
    NS_ENSURE_ARG_POINTER(aCapacity);
    *aCapacity = mMemCacheCapacity;
    return NS_OK;
}

NS_IMETHODIMP
nsCacheManager::SetDiskCacheCapacity(PRUint32 aCapacity)
{
    mDiskCacheCapacity = aCapacity;
    LimitCacheSize();
    return NS_OK;
}

NS_IMETHODIMP
nsCacheManager::GetDiskCacheCapacity(PRUint32* aCapacity)
{
    NS_ENSURE_ARG_POINTER(aCapacity);
    *aCapacity = mDiskCacheCapacity;
    return NS_OK;
}

NS_IMETHODIMP
nsCacheManager::SetDiskCacheFolder(nsIFileSpec* aFolder)
{
    NS_ENSURE_ARG(aFolder);

    if (!mFileCache)
        return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<nsINetDataDiskCache> fileCache;
    fileCache = do_QueryInterface(mFileCache);
    return fileCache->SetDiskCacheFolder(aFolder);
}

NS_IMETHODIMP
nsCacheManager::GetDiskCacheFolder(nsIFileSpec* *aFolder)
{
    NS_ENSURE_ARG(aFolder);

    if (!mFileCache)
        return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<nsINetDataDiskCache> fileCache;
    fileCache = do_QueryInterface(mFileCache);
    return fileCache->GetDiskCacheFolder(aFolder);
}
