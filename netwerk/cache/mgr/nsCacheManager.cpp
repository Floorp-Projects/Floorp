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
#include "nsIPref.h"
#include "nsIServiceManager.h"
#define FILE_CACHE_IS_READY
// Limit the number of entries in the cache to conserve memory space
// in the nsReplacementPolicy code
#define MAX_MEM_CACHE_ENTRIES    800
#define MAX_DISK_CACHE_ENTRIES  3200

// Cache capacities in MB, overridable via APIs
#define DEFAULT_MEMORY_CACHE_CAPACITY  1024
#define DEFAULT_DISK_CACHE_CAPACITY   10000
const char* CACHE_MEM_CAPACITY = "browser.cache.memory_cache_size";
const char* CACHE_DISK_CAPACITY = "browser.cache.disk_cache_size";

static int PR_CALLBACK diskCacheSizeChanged(const char *pref, void *closure)
{
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
	PRInt32 capacity = DEFAULT_DISK_CACHE_CAPACITY;
	if ( NS_SUCCEEDED (rv ) )
	{
		rv = prefs->GetIntPref(CACHE_DISK_CAPACITY, &capacity   );
	}
	return ( (nsCacheManager*)closure )->SetDiskCacheCapacity( capacity );	
}

static int PR_CALLBACK memCacheSizeChanged(const char *pref, void *closure)
{
	nsresult rv;
	PRInt32 capacity = DEFAULT_MEMORY_CACHE_CAPACITY ;
	NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
	if ( NS_SUCCEEDED (rv ) )
	{
		rv = prefs->GetIntPref(CACHE_MEM_CAPACITY, &capacity   );
	}
	return ( (nsCacheManager*)closure )->SetMemCacheCapacity( capacity );	
}

#define CACHE_HIGH_WATER_MARK(capacity) ((PRUint32)(0.98 * (capacity)))
#define CACHE_LOW_WATER_MARK(capacity)  ((PRUint32)(0.97 * (capacity)))

nsCacheManager* gCacheManager = 0;
PRBool gCacheManagerNeedToEvict = PR_FALSE;
NS_IMPL_ISUPPORTS(nsCacheManager, NS_GET_IID(nsINetDataCacheManager))

nsCacheManager::nsCacheManager()
    : mActiveCacheRecords(0),
    mDiskCacheCapacity(DEFAULT_DISK_CACHE_CAPACITY),
    mMemCacheCapacity(DEFAULT_MEMORY_CACHE_CAPACITY)
{
    NS_ASSERTION(!gCacheManager, "Multiple cache managers created");
    gCacheManager = this;
    gCacheManagerNeedToEvict = PR_FALSE;
    NS_INIT_REFCNT();
}

nsCacheManager::~nsCacheManager()
{
    gCacheManager = 0;
    delete mActiveCacheRecords;
    delete mMemSpaceManager;
    delete mDiskSpaceManager;
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if ( NS_SUCCEEDED (rv ) )
    {
        prefs->UnregisterCallback( CACHE_DISK_CAPACITY, diskCacheSizeChanged, this); 
        prefs->UnregisterCallback( CACHE_MEM_CAPACITY, memCacheSizeChanged, this); 
    }
}


nsresult nsCacheManager::InitPrefs()
{
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
	if ( NS_FAILED (rv ) )
		return rv; 
	rv = prefs->RegisterCallback( CACHE_DISK_CAPACITY, diskCacheSizeChanged, this); 
	if ( NS_FAILED( rv ) )
		return rv;
	
	rv = prefs->RegisterCallback( CACHE_MEM_CAPACITY, memCacheSizeChanged, this); 
	if ( NS_FAILED( rv ) )
		return rv;
	// Init the prefs 
	diskCacheSizeChanged( CACHE_DISK_CAPACITY, this );	
    memCacheSizeChanged( CACHE_MEM_CAPACITY, this );
	return rv;
}

nsresult
nsCacheManager::Init()
{
    nsresult rv;

    mActiveCacheRecords = new nsHashtable(64);
    if (!mActiveCacheRecords)
        return NS_ERROR_OUT_OF_MEMORY;

    // Instantiate the memory cache component
    rv = nsComponentManager::CreateInstance(NS_NETWORK_MEMORY_CACHE_CONTRACTID,
                                            nsnull,
                                            NS_GET_IID(nsINetDataCache),
                                            getter_AddRefs(mMemCache));
    if (NS_FAILED(rv))
        return rv;

    rv = nsComponentManager::CreateInstance(NS_NETWORK_FLAT_CACHE_CONTRACTID,
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
    rv = nsComponentManager::CreateInstance(NS_NETWORK_FILE_CACHE_CONTRACTID,
                                            nsnull,
                                            NS_GET_IID(nsINetDataCache),
                                            getter_AddRefs(mDiskCache));
    if (NS_FAILED(rv)) {
        NS_WARNING("No disk cache present");
    }
#endif

    // Set up linked list of caches in search order
    mCacheSearchChain = mMemCache;
    if (mFlatCache) {
        mMemCache->SetNextCache(mFlatCache);
        mFlatCache->SetNextCache(mDiskCache);
    } else {
        mMemCache->SetNextCache(mDiskCache);
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
    if (mDiskCache) {
        rv = mDiskSpaceManager->AddCache(mDiskCache);
        if (NS_FAILED(rv)) return rv;
    }
    if (mFlatCache) {
        rv = mDiskSpaceManager->AddCache(mFlatCache);
        if (NS_FAILED(rv)) return rv;
    }

    InitPrefs();
    return NS_OK;
}

nsresult nsCacheManager::GetCacheAndReplacementPolicy( PRUint32 aFlags, nsINetDataCache*& cache, nsReplacementPolicy *&spaceManager )
{
	PRBool diskCacheEnabled = PR_FALSE;
	PRBool memCacheEnabled  = PR_FALSE;

    // Initialize the out parameters...
    cache = nsnull;
    spaceManager = nsnull;

    // Determine if the Disk cache is available...
	if ( mDiskCache.get() ) {
		mDiskCache->GetEnabled( &diskCacheEnabled );
        // Ensure that the cache is initialized
        if ((mDiskCacheCapacity == (PRUint32)-1) || (mDiskCacheCapacity == 0))
            diskCacheEnabled = PR_FALSE;
    }

    // Determine if the Memory cache is available...
    if (mMemCache) {
        mMemCache->GetEnabled(&memCacheEnabled);
        // Ensure that the cache is initialized
        if ((mMemCacheCapacity == (PRUint32)-1) || (mMemCacheCapacity == 0))
            memCacheEnabled = PR_FALSE;
    }

    // The CACHE_AS_FILE flag requires the Disk cache...
    if (aFlags & CACHE_AS_FILE) {
        if (!diskCacheEnabled) 
            return NS_ERROR_NOT_AVAILABLE;

        cache = mDiskCache;
        spaceManager = mDiskSpaceManager;
    } 
    // The BYPASS_PERSISTANT_CACHE flag requires the Memory cache
    else if (aFlags & BYPASS_PERSISTENT_CACHE) {
        if (!memCacheEnabled)
            return NS_ERROR_NOT_AVAILABLE;

        cache = mMemCache;
        spaceManager = mMemSpaceManager;
    } 
    // Use the Disk cache if it is available...
    else if (diskCacheEnabled) {
        cache = mDiskCache;
        spaceManager = mDiskSpaceManager;
    }
    // Otherwise use the memory cache if it is available...
    else if (memCacheEnabled) {
        cache = mMemCache;
        spaceManager = mMemSpaceManager;
    }
    // No caches are available, so fail !!!
    else {
        return NS_ERROR_NOT_AVAILABLE;
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
	
    rv = GetCacheAndReplacementPolicy( aFlags, cache, spaceManager );
    if (NS_FAILED(rv))
        return rv;
    // Construct the cache key by appending the secondary key to the URI spec
    nsCAutoString cacheKey(aUriSpec);

    // Insert a newline between URI spec and secondary key
    if (aSecondaryKey) {
        cacheKey += '\n';
        cacheKey.Append(aSecondaryKey, aSecondaryKeyLength);
    }

    nsCStringKey key(cacheKey);
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
    
    nsCStringKey hashTableKey(key, keyLength);
    deletedEntry = (nsCachedNetData*)gCacheManager->mActiveCacheRecords->Remove(&hashTableKey);
//  NS_ASSERTION(deletedEntry == aEntry, "Hash table inconsistency");
    nsMemory::Free( key );
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

 		nsresult rv = GetCacheAndReplacementPolicy( aFlags, cache, spaceManager );
		if ( NS_FAILED ( rv ) )
			return rv;
    // Construct the cache key by appending the secondary key to the URI spec
    nsCAutoString cacheKey(aUriSpec);

    // Insert a newline between URI spec and secondary key
    if (aSecondaryKey) {
        cacheKey += '\n';
        cacheKey.Append(aSecondaryKey, aSecondaryKeyLength);
    }

    // Locate the record using (URI + secondary key)
    nsCStringKey key(cacheKey);
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

NS_IMETHODIMP
nsCacheManager::Clear( PRUint32 aCacheToClear )
{
	nsresult rv = NS_OK;
	if ( aCacheToClear & ALL_CACHES )
	{
		return RemoveAll();
	}
	
	if ( ( aCacheToClear & MEM_CACHE) && mMemCache.get() )
	{
		rv = mMemCache->RemoveAll();
		if( NS_FAILED ( rv ) )
			return rv;	
	}
	
		if ( (aCacheToClear & FILE_CACHE) && mDiskCache.get() )
	{
		rv = mDiskCache->RemoveAll();
		if( NS_FAILED ( rv ) )
			return rv;	
	}
	
	if ( ( aCacheToClear & FLAT_CACHE) && mFlatCache.get() )
	{
		rv = mFlatCache->RemoveAll();
		if( NS_FAILED ( rv ) )
			return rv;	
	}
	return rv;
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
nsCacheManager::LimitDiskCacheSize(PRBool skipCheck)
{
    nsresult rv;
    nsReplacementPolicy* spaceManager;

    NS_ASSERTION(gCacheManager, "No cache manager");

    spaceManager = gCacheManager->mDiskSpaceManager;

    PRUint32 occupancy;
    rv = spaceManager->GetStorageInUse(&occupancy);
    if (NS_FAILED(rv)) return rv;

    PRUint32 diskCacheCapacity = gCacheManager->mDiskCacheCapacity;
    if (skipCheck)
    {
        if (occupancy > CACHE_HIGH_WATER_MARK(diskCacheCapacity))
        {
            gCacheManagerNeedToEvict = PR_FALSE;
            return spaceManager->Evict(CACHE_LOW_WATER_MARK(diskCacheCapacity));
        }
    }
    else
        gCacheManagerNeedToEvict = PR_TRUE;
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
