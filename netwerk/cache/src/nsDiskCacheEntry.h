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
 * The Original Code is nsMemoryCacheDevice.cpp, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Patrick C. Beard <beard@netscape.com>
 */

#ifndef _nsDiskCacheEntry_h_
#define _nsDiskCacheEntry_h_

#include "nspr.h"
#include "pldhash.h"

#include "nsISupports.h"
#include "nsCacheEntry.h"

#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
#include "nsITransport.h"
#endif

class nsDiskCacheEntry : public nsISupports, public PRCList {
public:
    NS_DECL_ISUPPORTS

    nsDiskCacheEntry(nsCacheEntry* entry)
        :   mCacheEntry(entry),
            mGeneration(0)
    {
        NS_INIT_ISUPPORTS();
        PR_INIT_CLIST(this);
        mHashNumber = ::PL_DHashStringKey(NULL, entry->Key()->get());
    }

    virtual ~nsDiskCacheEntry()
    {
        PR_REMOVE_LINK(this);
    }

#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
    /**
     * Maps a cache access mode to a cached nsITransport for that access
     * mode. We keep these cached to avoid repeated trips to the
     * file transport service.
     */
    nsCOMPtr<nsITransport>& getTransport(nsCacheAccessMode mode)
    {
        return mTransports[mode - 1];
    }
    
    nsCOMPtr<nsITransport>& getMetaTransport(nsCacheAccessMode mode)
    {
        return mMetaTransports[mode - 1];
    }
#endif
    
    nsCacheEntry* getCacheEntry()
    {
        return mCacheEntry;
    }
    
    PRUint32 getGeneration()
    {
        return mGeneration;
    }
    
    void setGeneration(PRUint32 generation)
    {
        mGeneration = generation;
    }
    
    PLDHashNumber getHashNumber()
    {
        return mHashNumber;
    }
    
private:
#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
    nsCOMPtr<nsITransport>  mTransports[3];
    nsCOMPtr<nsITransport>  mMetaTransports[3];
#endif
    nsCacheEntry*           mCacheEntry;
    PRUint32                mGeneration;
    PLDHashNumber           mHashNumber;
};

class nsDiskCacheEntryHashTable {
public:
    nsDiskCacheEntryHashTable();
    ~nsDiskCacheEntryHashTable();

    nsresult                Init();

    nsDiskCacheEntry *      GetEntry(const char *               key);
    nsDiskCacheEntry *      GetEntry(PLDHashNumber              key);
    nsresult                AddEntry(nsDiskCacheEntry *         entry);
    void                    RemoveEntry(nsDiskCacheEntry *      entry);
    
    class Visitor {
    public:
        virtual PRBool      VisitEntry(nsDiskCacheEntry *       entry) = 0;
    };
    
    void                    VisitEntries(Visitor *              visitor);
    
private:
    struct HashTableEntry : PLDHashEntryHdr {
        nsDiskCacheEntry *  mDiskCacheEntry;                    // STRONG ref?
    };

    // PLDHashTable operation callbacks
    static const void *     GetKey(PLDHashTable *               table,
                                   PLDHashEntryHdr *            entry);

    static PLDHashNumber    HashKey(PLDHashTable *              table,
                                    const void *                key);

    static PRBool           MatchEntry(PLDHashTable *           table,
                                      const PLDHashEntryHdr *   entry,
                                      const void *              key);

    static void             MoveEntry(PLDHashTable *            table,
                                     const PLDHashEntryHdr *    from,
                                     PLDHashEntryHdr       *    to);

    static void             ClearEntry(PLDHashTable *           table,
                                       PLDHashEntryHdr *        entry);

    static void             Finalize(PLDHashTable *table);

    static
    PLDHashOperator         FreeCacheEntries(PLDHashTable *     table,
                                             PLDHashEntryHdr *  hdr,
                                             PRUint32           number,
                                             void *             arg);
    static
    PLDHashOperator         VisitEntry(PLDHashTable *           table,
                                       PLDHashEntryHdr *        hdr,
                                       PRUint32                 number,
                                       void *                   arg);
                                     
    // member variables
    static PLDHashTableOps ops;
    PLDHashTable           table;
    PRBool                 initialized;
};

#endif /* _nsDiskCacheEntry_h_ */
