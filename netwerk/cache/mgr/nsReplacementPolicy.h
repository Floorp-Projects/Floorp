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

/**
 * This class manages one or more caches that share a storage resource, e.g. a
 * file cache and a flat-database cache might each occupy space on the disk and
 * they would share a single instance of nsReplacementPolicy.  The replacement
 * policy heuristically chooses which cache entries to evict when storage is
 * required to accommodate incoming cache data.
 */

#ifndef _nsReplacementPolicy_h_
#define _nsReplacementPolicy_h_

#include "nscore.h"
#include "nsISupportsUtils.h"
#include "nsINetDataCache.h"
#include "nsICachedNetData.h"
#include "nsIArena.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"

class nsCachedNetData;
struct PL_HashTable;

/**
 * This private class is responsible for implementing the network data cache's
 * replacement policy, i.e. it decides which cache data should be evicted to
 * make room for new incoming data.
 */
class nsReplacementPolicy {

public:

    nsReplacementPolicy();
    ~nsReplacementPolicy();

protected:

    nsresult Init(PRUint32 aMaxCacheEntries);
    nsresult AddCache(nsINetDataCache *aCache);
    nsresult GetCachedNetData(const char* cacheKey, PRUint32 cacheKeyLength,
                              nsINetDataCache* aCache,
                              nsCachedNetData** aResult);
    nsresult GetStorageInUse(PRUint32* aNumKBytes);

    friend class nsCacheManager;

private:
    nsresult RankRecords();
    void MaybeRerankRecords();
    void CompactRankedEntriesArray();
    nsresult DeleteAtleastOneEntry(nsINetDataCache* aCache, PRUint32 targetNumEntries, PRUint32* numEntriesDeleted);
    nsresult Evict(PRUint32 aTargetOccupancy);

    nsCachedNetData* FindCacheEntryByRecordID(PRInt32 aRecordID, nsINetDataCache *aCache);
    void AddCacheEntry(nsCachedNetData* aCacheEntry, PRInt32 aRecordID);
    nsresult DeleteCacheEntry(nsCachedNetData* aCacheEntry);
    PRUint32 HashRecordID(PRInt32 aRecordID);
    nsresult AssociateCacheEntryWithRecord(nsINetDataCacheRecord *aRecord,
                                           nsINetDataCache* aCache,
                                           nsCachedNetData** aResult);

    nsresult AddAllRecordsInCache(nsINetDataCache *aCache);
    nsresult CheckForTooManyCacheEntries();
    nsresult LoadAllRecordsInAllCacheDatabases();

    // A cache whose space is managed by this replacement policy
    class CacheInfo {
    public:
        CacheInfo(nsINetDataCache* aCache):mCache(aCache),mNext(0) {}

        nsINetDataCache* mCache;
        CacheInfo*       mNext;
    };

private:

    // Growable array of pointers to individual cache entries; It is sorted by
    // profitability, such that low-numbered array indices refer to cache
    // entries that are the least profitable to retain.  New cache entries are
    // added to the end of the array.  Deleted cache entries are specially
    // marked and percolate to the end of the array for recycling whenever
    // mRankedEntries is sorted.  Evicted cache entries (those with no
    // associated content data) are retained for the purpose of improving the
    // replacement policy efficacy, and are percolated towards the end of the
    // array, just prior to the deleted cache entries.
    //
    // The array is not in sorted order 100% of the time; For efficiency
    // reasons, sorting is only done when heuristically deemed necessary.
    nsCachedNetData**   mRankedEntries;

    // Hash table buckets to map Record ID to cache entry.  We use this instead
    // of a PL_HashTable to reduce storage requirements
    nsCachedNetData**   mMapRecordIdToEntry;

    // Length of mMapRecordIdToEntry array
    PRUint32            mHashArrayLength;

    // Linked list of caches that share this replacement policy
    CacheInfo*          mCaches;

    // Allocation area for cache entry (nsCachedNetData) instances
    nsCOMPtr<nsIArena>  mArena;

    // Bookkeeping
    PRUint32            mRecordsRemovedSinceLastRanking;

    // Maximum permitted length of mRankedEntries array
    PRUint32            mMaxEntries;

    // Number of occupied slots in mRankedEntries array
    PRUint32            mNumEntries;

    // Capacity of mRankedEntries array
    PRUint32            mCapacityRankedEntriesArray;

    // Time at which cache entries were last ranked by profitability
    PRUint32            mLastRankTime;
    
    // When true, all cache database records have been loaded into the
    // mRankedEntries array.  Until this occurs, it is not possible to rank
    // cache entries against each other to determine which is the best
    // candidate for eviction from the cache.
    PRBool              mLoadedAllDatabaseRecords;
};


#endif // _nsReplacementPolicy_h_
