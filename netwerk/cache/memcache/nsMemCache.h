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
 */

/**
 * nsMemCache is the implementation of an in-memory network-data
 * cache, used to cache the responses to network retrieval commands.
 * Each cache entry may contain both content, e.g. GIF image data, and
 * associated metadata, e.g. HTTP headers.  Each entry is indexed by
 * two different keys: a record id number and an opaque key, which is
 * created by the cache manager by combining the URI with a "secondary
 * key", e.g. HTTP post data.
 */

#ifndef _nsMemCache_h_
#define _nsMemCache_h_

#include "nsINetDataCache.h"

// Maximum number of URIs that may be resident in the cache
#define MEM_CACHE_MAX_ENTRIES 1000

#define MEM_CACHE_SEGMENT_SIZE       (1 << 12)
#define MEM_CACHE_MAX_ENTRY_SIZE     (1 << 20)

class nsHashtable;
class nsMemCacheRecord;

class nsMemCache : public nsINetDataCache
{
public:
    nsMemCache();
    virtual ~nsMemCache();
    nsresult Init();

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsINetDataCache methods
    NS_DECL_NSINETDATACACHE

    // Factory
    static NS_METHOD nsMemCacheConstructor(nsISupports *aOuter, REFNSIID aIID,
                                           void **aResult);

protected:

    PRUint32         mNumEntries;
    PRUint32         mOccupancy;   // Memory used, in bytes
    PRBool           mEnabled;     // If false, bypass mem cache

    nsINetDataCache* mNextCache;

    // Mapping from either opaque key or record ID to nsMemCacheRecord
    nsHashtable*     mHashTable;

    // Used to assign record ID's
    static PRInt32   gRecordSerialNumber;

    NS_METHOD Delete(nsMemCacheRecord* aRecord);

    friend class nsMemCacheRecord;
    friend class nsMemCacheTransport;
};

#endif // _nsMemCache_h_
