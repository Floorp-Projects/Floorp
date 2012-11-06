/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef _nsDiskCacheBinding_h_
#define _nsDiskCacheBinding_h_

#include "nspr.h"
#include "pldhash.h"

#include "nsISupports.h"
#include "nsCacheEntry.h"

#include "nsDiskCacheMap.h"
#include "nsDiskCacheStreams.h"


/******************************************************************************
 *  nsDiskCacheBinding
 *
 *  Created for disk cache specific data and stored in nsCacheEntry.mData as
 *  an nsISupports.  Also stored in nsDiskCacheHashTable, with collisions
 *  linked by the PRCList.
 *
 *****************************************************************************/

class nsDiskCacheDeviceDeactivateEntryEvent;

class nsDiskCacheBinding : public nsISupports, public PRCList {
public:
    NS_DECL_ISUPPORTS

    nsDiskCacheBinding(nsCacheEntry* entry, nsDiskCacheRecord * record);
    virtual ~nsDiskCacheBinding();

    nsresult EnsureStreamIO();
    bool     IsActive() { return mCacheEntry != nullptr;}

// XXX make friends
public:
    nsCacheEntry*           mCacheEntry;    // back pointer to parent nsCacheEntry
    nsDiskCacheRecord       mRecord;
    nsDiskCacheStreamIO*    mStreamIO;      // strong reference
    bool                    mDoomed;        // record is not stored in cache map
    uint8_t                 mGeneration;    // possibly just reservation

    // If set, points to a pending event which will deactivate |mCacheEntry|.
    // If not set then either |mCacheEntry| is not deactivated, or it has been
    // deactivated but the device returned it from FindEntry() before the event
    // fired. In both two latter cases this binding is to be considered valid.
    nsDiskCacheDeviceDeactivateEntryEvent *mDeactivateEvent;
};


/******************************************************************************
 *  Utility Functions
 *****************************************************************************/

nsDiskCacheBinding *   GetCacheEntryBinding(nsCacheEntry * entry);



/******************************************************************************
 *  nsDiskCacheBindery
 *
 *  Used to keep track of nsDiskCacheBinding associated with active/bound (and
 *  possibly doomed) entries.  Lookups on 4 byte disk hash to find collisions
 *  (which need to be doomed, instead of just evicted.  Collisions are linked
 *  using a PRCList to keep track of current generation number.
 *
 *  Used to detect hash number collisions, and find available generation numbers.
 *
 *  Not all nsDiskCacheBinding have a generation number.
 *
 *  Generation numbers may be aquired late, or lost (when data fits in block file)
 *
 *  Collisions can occur:
 *      BindEntry()       - hashnumbers collide (possibly different keys)
 *
 *  Generation number required:
 *      DeactivateEntry() - metadata written to disk, may require file
 *      GetFileForEntry() - force data to require file
 *      writing to stream - data size may require file
 *
 *  Binding can be kept in PRCList in order of generation numbers.
 *  Binding with no generation number can be Appended to PRCList (last).
 *
 *****************************************************************************/

class nsDiskCacheBindery {
public:
    nsDiskCacheBindery();
    ~nsDiskCacheBindery();

    nsresult                Init();
    void                    Reset();

    nsDiskCacheBinding *    CreateBinding(nsCacheEntry *       entry,
                                          nsDiskCacheRecord *  record);

    nsDiskCacheBinding *    FindActiveBinding(uint32_t  hashNumber);
    void                    RemoveBinding(nsDiskCacheBinding * binding);
    bool                    ActiveBindings();

    size_t                 SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf);

private:
    nsresult                AddBinding(nsDiskCacheBinding * binding);

    // member variables
    static PLDHashTableOps ops;
    PLDHashTable           table;
    bool                   initialized;
};

#endif /* _nsDiskCacheBinding_h_ */
