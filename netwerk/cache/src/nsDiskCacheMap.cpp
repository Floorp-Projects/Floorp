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
 * The Original Code is nsDiskCacheMap.cpp, released March 23, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Patrick C. Beard <beard@netscape.com>
 */

#include "nsDiskCacheMap.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"

nsDiskCacheMap::nsDiskCacheMap()
{
}

nsDiskCacheMap::~nsDiskCacheMap()
{
}

nsDiskCacheRecord* nsDiskCacheMap::GetRecord(PRUint32 hashNumber)
{
    PRUint32 index = (hashNumber & (kBucketsPerTable - 1));
    nsDiskCacheBucket& bucket = mBuckets[index];
    nsDiskCacheRecord* oldestRecord = &bucket.mRecords[0];

    for (int i = 0; i < kRecordsPerBucket; ++i) {
        nsDiskCacheRecord* record = &bucket.mRecords[i];
        if (record->HashNumber() == 0 || record->HashNumber() == hashNumber)
            return record;
        if (record->EvictionRank() > oldestRecord->EvictionRank())
            oldestRecord = record;
    }

    // record not found, so must evict a record.
    oldestRecord->SetHashNumber(0);
    oldestRecord->SetEvictionRank(0);
    return oldestRecord;
}

nsresult nsDiskCacheMap::Read(nsIInputStream* input)
{
    // XXX need a header, etc.
    PRUint32 count;
    nsresult rv = input->Read((char*)&mBuckets, sizeof(mBuckets), &count);
    if (NS_FAILED(rv)) return rv;
    
    // unswap all of the active records.
    for (int b = 0; b < kBucketsPerTable; ++b) {
        nsDiskCacheBucket& bucket = mBuckets[b];
        for (int r = 0; r < kRecordsPerBucket; ++r) {
            nsDiskCacheRecord* record = &bucket.mRecords[r];
            if (record->HashNumber() == 0)
                break;
            record->Unswap();
        }
    }
    
    return NS_OK;
}

nsresult nsDiskCacheMap::Write(nsIOutputStream* output)
{
    // swap all of the active records.
    for (int b = 0; b < kBucketsPerTable; ++b) {
        nsDiskCacheBucket& bucket = mBuckets[b];
        for (int r = 0; r < kRecordsPerBucket; ++r) {
            nsDiskCacheRecord* record = &bucket.mRecords[r];
            if (record->HashNumber() == 0)
                break;
            record->Swap();
        }
    }
    
    // XXX need a header, etc.
    PRUint32 count;
    nsresult rv = output->Write((char*)&mBuckets, sizeof(mBuckets), &count);

    // unswap all of the active records.
    for (int b = 0; b < kBucketsPerTable; ++b) {
        nsDiskCacheBucket& bucket = mBuckets[b];
        for (int r = 0; r < kRecordsPerBucket; ++r) {
            nsDiskCacheRecord* record = &bucket.mRecords[r];
            if (record->HashNumber() == 0)
                break;
            record->Unswap();
        }
    }
    
    return rv;
}
