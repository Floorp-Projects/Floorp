/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsReplacementPolicy.h"
#include "nsCachedNetData.h"

#include "nsQuickSort.h"
#include "nsMemory.h"
#include "nsIEnumerator.h"
#include "prtime.h"
#include "prinrval.h"
#include "prbit.h"
#include "nsCOMPtr.h"
#include <math.h>

// Constant used to estimate frequency of access to a document based on size
#define CACHE_CONST_B   1.35

#define CACHE_LOW_NUM_ENTRIES(entries)  ((PRUint32)(0.75 * (entries)))

nsReplacementPolicy::nsReplacementPolicy()
    : mRankedEntries(0), mCaches(0), mRecordsRemovedSinceLastRanking(0),
      mNumEntries(0), mCapacityRankedEntriesArray(0), mLastRankTime(0), mLoadedAllDatabaseRecords( PR_FALSE ) {}

nsReplacementPolicy::~nsReplacementPolicy()
{
    if (mRankedEntries)
        nsMemory::Free(mRankedEntries);
    if (mMapRecordIdToEntry)
        nsMemory::Free(mMapRecordIdToEntry);
    while (mCaches) {
        CacheInfo* nextCacheInfo =  mCaches->mNext;
        delete mCaches;
        mCaches = nextCacheInfo;
    } 
}

nsresult
nsReplacementPolicy::Init(PRUint32 aMaxCacheEntries)
{
    nsresult rv;

    rv = NS_NewHeapArena(getter_AddRefs(mArena), sizeof(nsCachedNetData) * 32);
    if (NS_FAILED(rv)) return rv;

    mMaxEntries = aMaxCacheEntries;

    // Hash array length must be power-of-two
    mHashArrayLength = (1 << PR_CeilingLog2(aMaxCacheEntries)) >> 3;
    size_t numBytes = mHashArrayLength * sizeof(*mMapRecordIdToEntry);
    mMapRecordIdToEntry = (nsCachedNetData**)nsMemory::Alloc(numBytes);
    if (!mMapRecordIdToEntry)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCRT::zero(mMapRecordIdToEntry, numBytes);

    return NS_OK;
}

nsresult
nsReplacementPolicy::AddCache(nsINetDataCache *aCache)
{
    CacheInfo *cacheInfo = new CacheInfo(aCache);
    if (!cacheInfo)
        return NS_ERROR_OUT_OF_MEMORY;
    cacheInfo->mNext = mCaches;
    mCaches = cacheInfo;
    return NS_OK;
}

PRUint32
nsReplacementPolicy::HashRecordID(PRInt32 aRecordID)
{
    return ((aRecordID >> 16) ^ aRecordID) & (mHashArrayLength - 1);
}

nsCachedNetData*
nsReplacementPolicy::FindCacheEntryByRecordID(PRInt32 aRecordID, nsINetDataCache *aCache)
{
    nsresult rv;
    nsCachedNetData* cacheEntry;
    PRUint32 bucket = HashRecordID(aRecordID);

    cacheEntry = mMapRecordIdToEntry[bucket];
    for (;cacheEntry; cacheEntry = cacheEntry->mNext) {

        PRInt32 recordID;
        rv = cacheEntry->GetRecordID(&recordID);
        if (NS_FAILED(rv))
            continue;

        if ((recordID == aRecordID) && (cacheEntry->mCache == aCache))
            return cacheEntry;
    }

    return 0;
}

// Add a cache entry to the hash table that maps record ID to entries
void
nsReplacementPolicy::AddCacheEntry(nsCachedNetData* aCacheEntry, PRInt32 aRecordID)
{
    nsCachedNetData** cacheEntryp;
    PRUint32 bucket = HashRecordID(aRecordID);

    cacheEntryp = &mMapRecordIdToEntry[bucket];
    while (*cacheEntryp)
        cacheEntryp = &(*cacheEntryp)->mNext;
    *cacheEntryp = aCacheEntry;
    aCacheEntry->mNext = 0;
}

// Delete a cache entry from the hash table that maps record ID to entries
nsresult
nsReplacementPolicy::DeleteCacheEntry(nsCachedNetData* aCacheEntry)
{
    nsresult rv;
    PRInt32 recordID;
    rv = aCacheEntry->GetRecordID(&recordID);
    if (NS_FAILED(rv)) return rv;

    PRUint32 bucket = HashRecordID(recordID);

    nsCachedNetData** cacheEntryp;
    cacheEntryp = &mMapRecordIdToEntry[bucket];
    while (*cacheEntryp) {
        if (*cacheEntryp == aCacheEntry) {
            *cacheEntryp = aCacheEntry->mNext;
            return NS_OK;
        }
        cacheEntryp = &(*cacheEntryp)->mNext;
    }
    
    NS_ASSERTION(0, "hash table inconsistency");
    return NS_ERROR_FAILURE;
}

nsresult
nsReplacementPolicy::AddAllRecordsInCache(nsINetDataCache *aCache)
{
    nsresult rv;
    nsCOMPtr<nsISimpleEnumerator> iterator;
    nsCOMPtr<nsISupports> iSupports;
    nsCOMPtr<nsINetDataCacheRecord> record;
    rv = aCache->NewCacheEntryIterator(getter_AddRefs(iterator));
    if (!NS_SUCCEEDED(rv)) return rv;

    while (1) {
        PRBool notDone;

        rv = iterator->HasMoreElements(&notDone);
        if (NS_FAILED(rv)) return rv;
        if (!notDone)
            break;

        rv = iterator->GetNext(getter_AddRefs(iSupports));
        if (!NS_SUCCEEDED(rv)) return rv;
        record = do_QueryInterface(iSupports);
        
        rv = AssociateCacheEntryWithRecord(record, aCache, 0);
        if (!NS_SUCCEEDED(rv)) return rv;
    }

    return NS_OK;
}

// Get current time and convert to seconds since the epoch
static PRUint32
now32()
{
    double nowFP;
    PRInt64 now64 = PR_Now();
    LL_L2D(nowFP, now64);
    PRUint32 now = (PRUint32)(nowFP * 1e-6);
    return now;
}

void
nsCachedNetData::NoteDownloadTime(PRIntervalTime start, PRIntervalTime end)
{
    double rate;
    PRUint32 duration;

    duration = PR_IntervalToMilliseconds(end - start);

    // If the data arrives so fast that it can not be timed due to insufficient
    // clock granularity, assume a data arrival duration of 5 ms
    if (!duration)
        duration = 5;

    // Compute download rate in kB/s
    rate = mLogicalLength / (duration * (1.0e-3 * 1024.0));
    
    if (mDownloadRate) {
        // Exponentially smooth download rate
        const double alpha = 0.5;
        mDownloadRate = (float)(mDownloadRate * alpha + rate * (1.0 - alpha));
    } else {
        mDownloadRate = (float)rate;
    }
}

// 1 hour
#define MIN_HALFLIFE (60 * 60)

// 1 week
#define TYPICAL_HALFLIFE (7 * 24 * 60 * 60)

/**
 * Estimate the profit that would be lost if the given cache entry was evicted
 * from the cache.  Profit is defined as the future expected download delay per
 * byte of cached content.  The profit computation is made based on projected
 * frequency of access, prior download performance and a heuristic staleness
 * criteria.  The technique used is a variation of that described in the
 * following paper:
 *
 *    "A Case for Delay-Conscious Caching of Web Documents"
 *    http://www.bell-labs.com/user/rvingral/www97.html
 *
 * Briefly, expected profit is:
 *
 *   (projected frequency of access) * (download time per byte) * (probability freshness)
 */
void
nsCachedNetData::ComputeProfit(PRUint32 aNow)
{
    PRUint32 K, now;

    if (aNow)
        now = aNow;
    else
        now = now32();
    
    K = PR_MIN(MAX_K, mNumAccesses);
    if (!K) {
        mProfit = 0;
        return;
    }

    // Compute time, in seconds, since k'th most recent access
    double timeSinceKthAccess = now - mAccessTime[K - 1];
    if (timeSinceKthAccess <= 0.0)    // Sanity check
        timeSinceKthAccess = 1.0;

    // Estimate frequency of future document access based on past
    //  access frequency
    double frequencyAccess = K / timeSinceKthAccess;

    // If we don't have much historical data on access frequency
    //   use a heuristic based on document size as an estimate
    if (mLogicalLength) {
        if (K == 1) {
            frequencyAccess /= pow(mLogicalLength, CACHE_CONST_B);
        } else if (K == 2) {
            frequencyAccess /= pow(mLogicalLength, CACHE_CONST_B / 2);
        }
    }

    // Estimate likelihood that data in cache is fresh, i.e.
    //  that it corresponds to the document on the server
    double probabilityFreshness;
    PRInt32 halfLife, age, docTime;
    PRBool potentiallyStale;

    docTime = GetFlag(LAST_MODIFIED_KNOWN) ? mLastModifiedTime : mLastUpdateTime;
    age = now - docTime;

    probabilityFreshness = 1.0; // Optimistic

    if (GetFlag(EXPIRATION_KNOWN)) {
        potentiallyStale = now > mExpirationTime;
        halfLife = mExpirationTime - mLastModifiedTime;
    } else if (GetFlag(STALE_TIME_KNOWN)) {
        potentiallyStale = PR_TRUE;
        halfLife = mStaleTime - docTime;
    } else {
        potentiallyStale = PR_TRUE;
        halfLife = TYPICAL_HALFLIFE;
    }
    
    if (potentiallyStale) {
        if (halfLife < MIN_HALFLIFE)
            halfLife = MIN_HALFLIFE;

        probabilityFreshness = pow(0.5, (double)age / (double)halfLife);
    }

    mProfit = (float)(frequencyAccess * probabilityFreshness);
    if (mDownloadRate)
        mProfit /= mDownloadRate;
}

// Number of entries to grow mRankedEntries array when it's full
#define STATS_GROWTH_INCREMENT  256


// Sorting predicate for NS_Quicksort
int
nsCachedNetData::Compare(const void *a, const void *b, void *unused)
{
    nsCachedNetData* entryA = *(nsCachedNetData**)a;
    nsCachedNetData* entryB = *(nsCachedNetData**)b;

    // Percolate deleted or empty entries to the end of the mRankedEntries
    // array, so that they can be recycled.
    if (!entryA || entryA->GetFlag(RECYCLED)) {
        if (!entryB || entryB->GetFlag(RECYCLED))
            return 0;
        else 
            return +1;
    } 
    if (!entryB || entryB->GetFlag(RECYCLED))
        return -1;

    // Evicted entries (those with no content data) and active entries (those
    // currently being updated) are collected towards the end of the sorted
    // array just prior to the deleted cache entries, since evicted entries
    // can't be re-evicted.
    if (entryA->GetFlag(UPDATE_IN_PROGRESS)) {
        if (entryB->GetFlag(UPDATE_IN_PROGRESS))
            return 0;
        else 
            return +1;
    } 
    if (entryB->GetFlag(UPDATE_IN_PROGRESS))
        return -1;

    PRUint16 Ka = PR_MIN(MAX_K, entryA->mNumAccesses);
    PRUint16 Kb = PR_MIN(MAX_K, entryB->mNumAccesses);

    // Order cache entries by the number of times they've been accessed
    if (Ka < Kb)
        return -1;
    if (Ka > Kb)
        return +1;

    /*
     * Among records that have been accessed an equal number of times, order
     * them by profit.
     */
    if (entryA->mProfit > entryB->mProfit)
        return +1;
    if (entryA->mProfit < entryB->mProfit)
        return -1;
    return 0;
}

/**
 * Rank cache entries in terms of their elegibility for eviction.
 */
nsresult
nsReplacementPolicy::RankRecords()
{
    PRUint32 i, now;

    // Get current time and convert to seconds since the epoch
    now = now32();

    // Recompute profit for every known cache record, except deleted ones
    for (i = 0; i < mNumEntries; i++) {
        nsCachedNetData* entry = mRankedEntries[i];
        if (entry && !entry->GetFlag(nsCachedNetData::RECYCLED))
            entry->ComputeProfit(now);
    }
    NS_QuickSort(mRankedEntries, mNumEntries, sizeof *mRankedEntries, 
                 nsCachedNetData::Compare, 0);

    mNumEntries -= mRecordsRemovedSinceLastRanking;
    mRecordsRemovedSinceLastRanking = 0;
    mLastRankTime = now;
    return NS_OK;
}

// A heuristic policy to avoid the cost of re-ranking cache records by
// profitability every single time space must be made available in the cache.
void
nsReplacementPolicy::MaybeRerankRecords()
{
    // Rank at most once per minute
    PRUint32 now = now32();
    if ((now - mLastRankTime) >= 60)
        RankRecords();
}

void
nsReplacementPolicy::CompactRankedEntriesArray()
{
    if (mRecordsRemovedSinceLastRanking || !mLastRankTime)
        RankRecords();
}

nsresult
nsReplacementPolicy::CheckForTooManyCacheEntries()
{
    PRUint32 undeletedEntries;
    PRUint32 maxEntries = 0;
    PRUint32 numEntriesDeleted = 0;
	PRBool deleteEntries = PR_FALSE;
    nsresult rv;
    CacheInfo *cacheInfo;

    cacheInfo = mCaches;

    undeletedEntries = mNumEntries - mRecordsRemovedSinceLastRanking;

    while (cacheInfo) {

        rv = cacheInfo->mCache->GetMaxEntries(&maxEntries);
        if (NS_FAILED(rv)) return rv;

        if (undeletedEntries >= (mMaxEntries-1)) {
			deleteEntries = PR_TRUE;    
        } else {
        
            PRUint32 numEntries = 0;

            rv = cacheInfo->mCache->GetNumEntries(&numEntries);
            if (NS_FAILED(rv)) return rv;
            if (numEntries == maxEntries) {
                deleteEntries = PR_TRUE;
            }
      
        }

		if (deleteEntries) {
			return DeleteAtleastOneEntry(cacheInfo->mCache,
						CACHE_LOW_NUM_ENTRIES(maxEntries), &numEntriesDeleted);
		}
		deleteEntries = PR_FALSE;
        cacheInfo = cacheInfo->mNext;
    }

    return NS_OK;
}


/** 
 * Create a new association between a low-level cache database record and a
 * cache entry.  Add the entry to the set of entries eligible for eviction from
 * the cache.  This would typically be done when the cache entry is created.
 */
nsresult
nsReplacementPolicy::AssociateCacheEntryWithRecord(nsINetDataCacheRecord *aRecord,
                                                   nsINetDataCache* aCache,
                                                   nsCachedNetData** aResult)
{
    nsCachedNetData* cacheEntry;
    nsresult rv;

    // First, see if the record is already known to the replacement policy
    PRInt32 recordID;
    rv = aRecord->GetRecordID(&recordID);
    if (NS_FAILED(rv)) return rv;
    cacheEntry = FindCacheEntryByRecordID(recordID, aCache);
    if (cacheEntry) {
        if (aResult) {
            if (cacheEntry->GetFlag(nsCachedNetData::DORMANT))
                cacheEntry->Resurrect(aRecord);
            NS_ADDREF(cacheEntry);
            *aResult = cacheEntry;
        }
        return NS_OK;
    }

 
    // Compact the array of cache entry statistics, so that free entries appear
    // at the end, for possible reuse.
    if (mNumEntries && (mNumEntries == mCapacityRankedEntriesArray))
        CompactRankedEntriesArray();

    // If compaction doesn't yield available entries in the
    // mRankedEntries array, then extend the array.
    if (mNumEntries == mCapacityRankedEntriesArray) {
        PRUint32 newCapacity;

        newCapacity = mCapacityRankedEntriesArray + STATS_GROWTH_INCREMENT;
        if (newCapacity > mMaxEntries)
            newCapacity = mMaxEntries;

        nsCachedNetData** newRankedEntriesArray; 
        PRUint32 numBytes = sizeof(nsCachedNetData*) * newCapacity;
        newRankedEntriesArray = 
            (nsCachedNetData**)nsMemory::Realloc(mRankedEntries, numBytes);
        if (!newRankedEntriesArray)
            return NS_ERROR_OUT_OF_MEMORY;
        
        mRankedEntries = newRankedEntriesArray;
        mCapacityRankedEntriesArray = newCapacity;

        PRUint32 i;
        for (i = mNumEntries; i < newCapacity; i++)
            mRankedEntries[i] = 0;
    }

    // We should never hit this condition, should return otherwise it will cause an ABR
    if (mNumEntries >= mMaxEntries)
        return NS_ERROR_FAILURE;

    // Recycle the record after the last in-use record in the array
    nsCachedNetData *entry = mRankedEntries[mNumEntries];
    NS_ASSERTION(!entry || entry->GetFlag(nsCachedNetData::RECYCLED),
                 "Only deleted cache entries should appear at end of array");

    if (!entry) {
        entry = new(mArena) nsCachedNetData;
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;
        mRankedEntries[mNumEntries] = entry;
    } else {
        // Clear out recycled data structure
        entry = new(entry) nsCachedNetData;
    }

    entry->Init(aRecord, aCache);
    AddCacheEntry(entry, recordID);
    
    // Add one reference to the cache entry from the cache manager
    NS_ADDREF(entry);

    if (aResult) {
        // And one reference from our caller
        NS_ADDREF(entry);
        *aResult = entry;
    }

    mNumEntries++;
    return NS_OK;
}

nsresult
nsReplacementPolicy::GetCachedNetData(const char* cacheKey, PRUint32 cacheKeyLength,
                                      nsINetDataCache* aCache,
                                      nsCachedNetData** aResult)
{
    nsresult rv;
    nsCOMPtr<nsINetDataCacheRecord> record;

    // If number of tracked cache entries exceeds limits, delete one
    rv = CheckForTooManyCacheEntries();
    if (NS_FAILED(rv)) return rv;


    rv = aCache->GetCachedNetData(cacheKey, cacheKeyLength,
                                  getter_AddRefs(record));
    if (NS_FAILED(rv)) return rv;
    
    return AssociateCacheEntryWithRecord(record, aCache, aResult);
}

/**
 * Delete the atleast one desirable record from the cache database.  This is used
 * when the addition of another record would exceed either the cache manager or
 * the cache's maximum permitted number of records. This method tries to reduce the
 * number of records in the cache database to targetNumEntries. targetNumEntriesReached
 * is set to PR_TRUE, if the nember of records has been reduced to targetNumEntries.
 * It returns NS_OK, if atleast one record has been succesfully deleted.
 */
nsresult
nsReplacementPolicy::DeleteAtleastOneEntry(nsINetDataCache *aCache, 
                                    PRUint32 targetNumEntries,
                                    PRUint32* numEntriesDeleted)
{
    PRUint32 i;
    nsresult rv;
    nsCachedNetData *entry;
    PRBool atleastOneEntryDeleted = PR_FALSE;
    PRUint32 numRecordEntries = 0;

	*numEntriesDeleted = 0;

	if (!aCache)
		return NS_ERROR_FAILURE;

	rv = aCache->GetNumEntries(&numRecordEntries);
    if (NS_FAILED(rv)) 
		return rv;

	if (targetNumEntries >= numRecordEntries)
		return NS_ERROR_FAILURE;


    // It's not possible to rank cache entries by their profitability
    // until all of them are known to the replacement policy.
    rv = LoadAllRecordsInAllCacheDatabases();
    if(NS_FAILED(rv)) {
        return rv ;
    }

    i = 0;
    MaybeRerankRecords();
  
    for (i = 0; i < mNumEntries; i++) {
        entry = mRankedEntries[i];
        if (!entry || entry->GetFlag(nsCachedNetData::RECYCLED) || (entry->mRefCnt > 1))
            continue;
        if (entry->mCache == aCache) {
			rv = entry->Delete();
			if (NS_SUCCEEDED(rv)) {
				rv = DeleteCacheEntry(entry);
				mRecordsRemovedSinceLastRanking++; 
				atleastOneEntryDeleted = PR_TRUE;
				numRecordEntries--;
				*numEntriesDeleted = mRecordsRemovedSinceLastRanking;
				if (numRecordEntries <= targetNumEntries) {
				   return rv;
				}
			}
		}
    }

    // Report error if no record found to delete
    if (i == mNumEntries) {
        if (atleastOneEntryDeleted)
            return NS_OK;
        else
            return NS_ERROR_FAILURE;
    }

	return NS_ERROR_FAILURE;
}

nsresult
nsReplacementPolicy::GetStorageInUse(PRUint32* aStorageInUse)
{
    nsresult rv;
    CacheInfo *cacheInfo;

    *aStorageInUse = 0;
    cacheInfo = mCaches;
    while (cacheInfo) {
        PRUint32 cacheStorage;
        rv = cacheInfo->mCache->GetStorageInUse(&cacheStorage);
        if (NS_FAILED(rv)) return rv;

        *aStorageInUse += cacheStorage;
        cacheInfo = cacheInfo->mNext;
    }
    return NS_OK;
}

// When true, all cache database records have been loaded into the
// mRankedEntries array.  Until this occurs, it is not possible to rank
// cache entries against each other to determine which is the best
// candidate for eviction from the cache.
nsresult
nsReplacementPolicy::LoadAllRecordsInAllCacheDatabases()
{
    // We been here before ?
    if (mLoadedAllDatabaseRecords)
        return NS_OK;

    nsresult rv;
    CacheInfo *cacheInfo;

    cacheInfo = mCaches;
    while (cacheInfo) {
        rv = AddAllRecordsInCache(cacheInfo->mCache);
        if (NS_FAILED(rv)) {
            mLoadedAllDatabaseRecords = PR_FALSE;
            return rv;
        }
        cacheInfo = cacheInfo->mNext;
    }
    mLoadedAllDatabaseRecords = PR_TRUE;
    return RankRecords();
}


/**
 * Delete the least desirable records from the cache until the occupancy of the
 * cache has been reduced by the given number of KB.  This is used when the
 * addition of more cache data would exceed the cache's capacity. 
 */
nsresult
nsReplacementPolicy::Evict(PRUint32 aTargetOccupancy)
{
    PRUint32 i;
    nsCachedNetData *entry;
    nsresult rv;
    PRUint32 occupancy;
    PRInt32 truncatedLength;
    nsCOMPtr<nsINetDataCacheRecord> record;

    // It's not possible to rank cache entries by their profitability
    // until all of them are known to the replacement policy.
    rv = LoadAllRecordsInAllCacheDatabases();
    if(NS_FAILED(rv)) {
        return rv ;
    }

    MaybeRerankRecords();

    for (i = 0; i < mNumEntries; i++) {
        rv = GetStorageInUse(&occupancy);
        if (!NS_SUCCEEDED(rv)) return rv;
        
        if (occupancy <= aTargetOccupancy)
            return NS_OK;

        entry = mRankedEntries[i];

        // Skip deleted/empty cache entries and ones that have already been evicted
        if (!entry || entry->GetFlag(nsCachedNetData::UNEVICTABLE))
            continue;

        // If the entry is not DORMANT then it cannot be evicted, so skip it..
        if (!(entry->GetFlag(nsCachedNetData::DORMANT)))
            continue;

        if (entry->GetFlag(nsCachedNetData::ALLOW_PARTIAL)) {
            rv = entry->GetRecord(getter_AddRefs(record));
            if (NS_FAILED(rv))
                continue;

            PRUint32 contentLength;
            rv = record->GetStoredContentLength(&contentLength);
            if (NS_FAILED(rv))
                continue;
            
            // Additional cache storage required, in KB
            PRUint32 storageToReclaim = (occupancy - aTargetOccupancy) << 10;
            
            truncatedLength = (PRInt32)(contentLength - storageToReclaim);
            if (truncatedLength < 0)
                truncatedLength = 0;
        } else {
            truncatedLength = 0;
        }
        rv = entry->Evict(truncatedLength);
    }
    if (occupancy <= aTargetOccupancy)
        return NS_OK;
    else
        return NS_ERROR_FAILURE;
}
