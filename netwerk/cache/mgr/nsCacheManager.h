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

#ifndef _nsCacheManager_h_
#define _nsCacheManager_h_

// 2030f0b0-9567-11d3-90d3-0040056a906e
#define NS_CACHE_MANAGER_CID                              \
    {                                                     \
        0x2030f0b0,                                       \
        0x9567,                                           \
        0x11d3,                                           \
        {0x90, 0xd3, 0x00, 0x40, 0x05, 0x6a, 0x90, 0x6e}  \
    }

#include "nsINetDataCacheManager.h"
#include "nsINetDataCache.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"

class nsHashtable;
class nsReplacementPolicy;
class nsCachedNetData;

class nsCacheManager : public nsINetDataCacheManager,
                       public nsIObserver
{

public:
    nsCacheManager();
    virtual ~nsCacheManager();

    NS_METHOD Init();

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsINetDataCacheManager methods
    NS_DECL_NSINETDATACACHEMANAGER

    // nsIObserver methods
    NS_DECL_NSIOBSERVER

private:
	
    // Mapping from cache key to nsCachedNetData, but only for those cache
    // entries with external references, i.e. those referred to outside the
    // cache manager
    nsHashtable*              mActiveCacheRecords;

    // Memory cache
    nsCOMPtr<nsINetDataCache> mMemCache;

    // Flat-file database cache; All content aggregated into single disk file
    nsCOMPtr<nsINetDataCache> mFlatCache;

    // stream-as-file cache
    nsCOMPtr<nsINetDataCache> mDiskCache;

    // Unified replacement policy for flat-cache and file-cache
    nsReplacementPolicy*      mDiskSpaceManager;
    
    // Replacement policy for memory cache
    nsReplacementPolicy*      mMemSpaceManager;

    // List of caches in search order
    nsINetDataCache*          mCacheSearchChain;

    // Combined file/flat cache capacity, in KB
    PRUint32                  mDiskCacheCapacity;

    // Memory cache capacity, in KB
    PRUint32                  mMemCacheCapacity;

	// Helper routines
		nsresult InitPrefs();
		nsresult GetCacheAndReplacementPolicy( PRUint32 aFlags, nsINetDataCache*& cache, nsReplacementPolicy *&spaceManager );
protected:
    static nsresult NoteDormant(nsCachedNetData* aEntry);
    static nsresult LimitCacheSize();
    static nsresult LimitMemCacheSize();
public:
    static nsresult LimitDiskCacheSize(PRBool skipCheck=PR_FALSE);
protected:

    friend class nsCachedNetData;
    friend class CacheOutputStream;
};

#endif // _nsCacheManager_h_
