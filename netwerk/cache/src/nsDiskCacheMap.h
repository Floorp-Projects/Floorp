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
 * The Original Code is nsDiskCacheMap.h, released March 23, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Patrick C. Beard <beard@netscape.com>
 */

#ifndef _nsDiskCacheMap_h_

#include "nsDiskCache.h"
#include "nsError.h"

class nsIInputStream;
class nsIOutputStream;

struct nsDiskCacheHeader {
    enum { kCurrentVersion = 0x00010002 };

    PRUint32    mVersion;                           // cache version.
    PRUint32    mDataSize;                          // size of cache in bytes.
    PRUint32    mEntryCount;                        // number of entries stored in cache.
    PRUint32    mIsDirty;                           // dirty flag.
    // XXX need a bitmap?
    
    nsDiskCacheHeader()
        :   mVersion(kCurrentVersion), mDataSize(0),
            mEntryCount(0), mIsDirty(PR_TRUE)
    {
    }

    void        Swap()
    {
        mVersion = ::PR_htonl(mVersion);
        mDataSize = ::PR_htonl(mDataSize);
        mEntryCount = ::PR_htonl(mEntryCount);
        mIsDirty = ::PR_htonl(mIsDirty);
    }
    
    void        Unswap()
    {
        mVersion = ::PR_ntohl(mVersion);
        mDataSize = ::PR_ntohl(mDataSize);
        mEntryCount = ::PR_ntohl(mEntryCount);
        mIsDirty = ::PR_ntohl(mIsDirty);
    }
};

// XXX initial capacity, enough for 8192 distinct entries.
class nsDiskCacheMap {
public:
    nsDiskCacheMap();
    ~nsDiskCacheMap();

    void Reset();
    
    PRUint32& DataSize() { return mHeader.mDataSize; }
    PRUint32& EntryCount() { return mHeader.mEntryCount; }
    PRUint32& IsDirty() { return mHeader.mIsDirty; }
    
    nsDiskCacheRecord* GetRecord(PRUint32 hashNumber);
    void DeleteRecord(nsDiskCacheRecord* record);
    
    enum {
        kRecordsPerBucket = 256,
        kBucketsPerTable = (1 << 5)                 // must be a power of 2!
    };
    
    nsDiskCacheRecord* GetBucket(PRUint32 index)
    {
        return mBuckets[index].mRecords;
    }
    
    PRUint32 GetBucketIndex(PRUint32 hashNumber)
    {
        return (hashNumber & (kBucketsPerTable - 1));
    }
    
    PRUint32 GetBucketIndex(nsDiskCacheRecord* record)
    {
        return GetBucketIndex(record->HashNumber());
    }
    
    nsresult Read(nsIInputStream* input);
    nsresult Write(nsIOutputStream* output);
    
    nsresult ReadBucket(nsIInputStream* input, PRUint32 index);
    nsresult WriteBucket(nsIOutputStream* output, PRUint32 index);
    
    nsresult ReadHeader(nsIInputStream* input);
    nsresult WriteHeader(nsIOutputStream* output);
    
private:
    struct nsDiskCacheBucket {
        nsDiskCacheRecord   mRecords[kRecordsPerBucket];
    };
    
    nsDiskCacheHeader       mHeader;
    nsDiskCacheBucket       mBuckets[kBucketsPerTable];
};

#endif // _nsDiskCacheMap_h_
