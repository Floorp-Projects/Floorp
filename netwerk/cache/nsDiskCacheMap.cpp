/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 cin et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is nsDiskCacheMap.cpp, released
 * March 23, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick C. Beard <beard@netscape.com>
 *   Gordon Sheridan  <gordon@netscape.com>
 *   Alfred Kayser <alfredkayser@nl.ibm.com>
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDiskCacheMap.h"
#include "nsDiskCacheBinding.h"
#include "nsDiskCacheEntry.h"

#include "nsCache.h"

#include <string.h>
#include "nsPrintfCString.h"

#include "nsISerializable.h"
#include "nsSerializationHelper.h"

#include "mozilla/Telemetry.h"

/******************************************************************************
 *  nsDiskCacheMap
 *****************************************************************************/

/**
 *  File operations
 */

nsresult
nsDiskCacheMap::Open(nsILocalFile *  cacheDirectory)
{
    NS_ENSURE_ARG_POINTER(cacheDirectory);
    if (mMapFD)  return NS_ERROR_ALREADY_INITIALIZED;

    mCacheDirectory = cacheDirectory;   // save a reference for ourselves
    
    // create nsILocalFile for _CACHE_MAP_
    nsresult rv;
    nsCOMPtr<nsIFile> file;
    rv = cacheDirectory->Clone(getter_AddRefs(file));
    nsCOMPtr<nsILocalFile> localFile(do_QueryInterface(file, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    rv = localFile->AppendNative(NS_LITERAL_CSTRING("_CACHE_MAP_"));
    NS_ENSURE_SUCCESS(rv, rv);

    // open the file - restricted to user, the data could be confidential
    rv = localFile->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE, 00600, &mMapFD);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FILE_CORRUPTED);

    bool cacheFilesExist = CacheFilesExist();
    rv = NS_ERROR_FILE_CORRUPTED;  // presume the worst

    // check size of map file
    PRUint32 mapSize = PR_Available(mMapFD);    
    if (mapSize == 0) {  // creating a new _CACHE_MAP_

        // block files shouldn't exist if we're creating the _CACHE_MAP_
        if (cacheFilesExist)
            goto error_exit;

        if (NS_FAILED(CreateCacheSubDirectories()))
            goto error_exit;

        // create the file - initialize in memory
        memset(&mHeader, 0, sizeof(nsDiskCacheHeader));
        mHeader.mVersion = nsDiskCache::kCurrentVersion;
        mHeader.mRecordCount = kMinRecordCount;
        mRecordArray = (nsDiskCacheRecord *)
            PR_CALLOC(mHeader.mRecordCount * sizeof(nsDiskCacheRecord));
        if (!mRecordArray) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto error_exit;
        }
    } else if (mapSize >= sizeof(nsDiskCacheHeader)) {  // read existing _CACHE_MAP_
        
        // if _CACHE_MAP_ exists, so should the block files
        if (!cacheFilesExist)
            goto error_exit;

        // read the header
        PRUint32 bytesRead = PR_Read(mMapFD, &mHeader, sizeof(nsDiskCacheHeader));
        if (sizeof(nsDiskCacheHeader) != bytesRead)  goto error_exit;
        mHeader.Unswap();

        if (mHeader.mIsDirty || (mHeader.mVersion != nsDiskCache::kCurrentVersion))
            goto error_exit;

        PRUint32 recordArraySize =
                mHeader.mRecordCount * sizeof(nsDiskCacheRecord);
        if (mapSize < recordArraySize + sizeof(nsDiskCacheHeader))
            goto error_exit;

        // Get the space for the records
        mRecordArray = (nsDiskCacheRecord *) PR_MALLOC(recordArraySize);
        if (!mRecordArray) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto error_exit;
        }

        // Read the records
        bytesRead = PR_Read(mMapFD, mRecordArray, recordArraySize);
        if (bytesRead < recordArraySize)
            goto error_exit;

        // Unswap each record
        PRInt32 total = 0;
        for (PRInt32 i = 0; i < mHeader.mRecordCount; ++i) {
            if (mRecordArray[i].HashNumber()) {
#if defined(IS_LITTLE_ENDIAN)
                mRecordArray[i].Unswap();
#endif
                total ++;
            }
        }
        
        // verify entry count
        if (total != mHeader.mEntryCount)
            goto error_exit;

    } else {
        goto error_exit;
    }

    rv = OpenBlockFiles();
    if (NS_FAILED(rv))  goto error_exit;

    // set dirty bit and flush header
    mHeader.mIsDirty    = true;
    rv = FlushHeader();
    if (NS_FAILED(rv))  goto error_exit;
    
    {
        // extra scope so the compiler doesn't barf on the above gotos jumping
        // past this declaration down here
        PRUint32 overhead = moz_malloc_size_of(mRecordArray);
        mozilla::Telemetry::Accumulate(mozilla::Telemetry::HTTP_DISK_CACHE_OVERHEAD,
                overhead);
    }

    return NS_OK;
    
error_exit:
    (void) Close(false);
       
    return rv;
}


nsresult
nsDiskCacheMap::Close(bool flush)
{
    nsresult  rv = NS_OK;

    // If cache map file and its block files are still open, close them
    if (mMapFD) {
        // close block files
        rv = CloseBlockFiles(flush);
        if (NS_SUCCEEDED(rv) && flush && mRecordArray) {
            // write the map records
            rv = FlushRecords(false);   // don't bother swapping buckets back
            if (NS_SUCCEEDED(rv)) {
                // clear dirty bit
                mHeader.mIsDirty = false;
                rv = FlushHeader();
            }
        }
        if ((PR_Close(mMapFD) != PR_SUCCESS) && (NS_SUCCEEDED(rv)))
            rv = NS_ERROR_UNEXPECTED;

        mMapFD = nsnull;
    }
    PR_FREEIF(mRecordArray);
    PR_FREEIF(mBuffer);
    mBufferSize = 0;
    return rv;
}


nsresult
nsDiskCacheMap::Trim()
{
    nsresult rv, rv2 = NS_OK;
    for (int i=0; i < kNumBlockFiles; ++i) {
        rv = mBlockFile[i].Trim();
        if (NS_FAILED(rv))  rv2 = rv;   // if one or more errors, report at least one
    }
    // Try to shrink the records array
    rv = ShrinkRecords();
    if (NS_FAILED(rv))  rv2 = rv;   // if one or more errors, report at least one
    return rv2;
}


nsresult
nsDiskCacheMap::FlushHeader()
{
    if (!mMapFD)  return NS_ERROR_NOT_AVAILABLE;
    
    // seek to beginning of cache map
    PRInt32 filePos = PR_Seek(mMapFD, 0, PR_SEEK_SET);
    if (filePos != 0)  return NS_ERROR_UNEXPECTED;
    
    // write the header
    mHeader.Swap();
    PRInt32 bytesWritten = PR_Write(mMapFD, &mHeader, sizeof(nsDiskCacheHeader));
    mHeader.Unswap();
    if (sizeof(nsDiskCacheHeader) != bytesWritten) {
        return NS_ERROR_UNEXPECTED;
    }

    PRStatus err = PR_Sync(mMapFD);
    if (err != PR_SUCCESS) return NS_ERROR_UNEXPECTED;

    return NS_OK;
}


nsresult
nsDiskCacheMap::FlushRecords(bool unswap)
{
    if (!mMapFD)  return NS_ERROR_NOT_AVAILABLE;
    
    // seek to beginning of buckets
    PRInt32 filePos = PR_Seek(mMapFD, sizeof(nsDiskCacheHeader), PR_SEEK_SET);
    if (filePos != sizeof(nsDiskCacheHeader))
        return NS_ERROR_UNEXPECTED;
    
#if defined(IS_LITTLE_ENDIAN)
    // Swap each record
    for (PRInt32 i = 0; i < mHeader.mRecordCount; ++i) {
        if (mRecordArray[i].HashNumber())   
            mRecordArray[i].Swap();
    }
#endif
    
    PRInt32 recordArraySize = sizeof(nsDiskCacheRecord) * mHeader.mRecordCount;

    PRInt32 bytesWritten = PR_Write(mMapFD, mRecordArray, recordArraySize);
    if (bytesWritten != recordArraySize)
        return NS_ERROR_UNEXPECTED;

#if defined(IS_LITTLE_ENDIAN)
    if (unswap) {
        // Unswap each record
        for (PRInt32 i = 0; i < mHeader.mRecordCount; ++i) {
            if (mRecordArray[i].HashNumber())   
                mRecordArray[i].Unswap();
        }
    }
#endif
    
    return NS_OK;
}


/**
 *  Record operations
 */

PRUint32
nsDiskCacheMap::GetBucketRank(PRUint32 bucketIndex, PRUint32 targetRank)
{
    nsDiskCacheRecord * records = GetFirstRecordInBucket(bucketIndex);
    PRUint32            rank = 0;

    for (int i = mHeader.mBucketUsage[bucketIndex]-1; i >= 0; i--) {          
        if ((rank < records[i].EvictionRank()) &&
            ((targetRank == 0) || (records[i].EvictionRank() < targetRank)))
                rank = records[i].EvictionRank();
    }
    return rank;
}

nsresult
nsDiskCacheMap::GrowRecords()
{
    if (mHeader.mRecordCount >= mMaxRecordCount)
        return NS_OK;
    CACHE_LOG_DEBUG(("CACHE: GrowRecords\n"));

    // Resize the record array
    PRInt32 newCount = mHeader.mRecordCount << 1;
    if (newCount > mMaxRecordCount)
        newCount = mMaxRecordCount;
    nsDiskCacheRecord *newArray = (nsDiskCacheRecord *)
            PR_REALLOC(mRecordArray, newCount * sizeof(nsDiskCacheRecord));
    if (!newArray)
        return NS_ERROR_OUT_OF_MEMORY;

    // Space out the buckets
    PRUint32 oldRecordsPerBucket = GetRecordsPerBucket();
    PRUint32 newRecordsPerBucket = newCount / kBuckets;
    // Work from back to space out each bucket to the new array
    for (int bucketIndex = kBuckets - 1; bucketIndex >= 0; --bucketIndex) {
        // Move bucket
        nsDiskCacheRecord *newRecords = newArray + bucketIndex * newRecordsPerBucket;
        const PRUint32 count = mHeader.mBucketUsage[bucketIndex];
        memmove(newRecords,
                newArray + bucketIndex * oldRecordsPerBucket,
                count * sizeof(nsDiskCacheRecord));
        // clear unused records
        memset(newRecords + count, 0,
               (newRecordsPerBucket - count) * sizeof(nsDiskCacheRecord));
    }

    // Set as the new record array
    mRecordArray = newArray;
    mHeader.mRecordCount = newCount;
    return NS_OK;
}

nsresult
nsDiskCacheMap::ShrinkRecords()
{
    if (mHeader.mRecordCount <= kMinRecordCount)
        return NS_OK;
    CACHE_LOG_DEBUG(("CACHE: ShrinkRecords\n"));

    // Verify if we can shrink the record array: all buckets must be less than
    // 1/2 filled
    PRUint32 maxUsage = 0, bucketIndex;
    for (bucketIndex = 0; bucketIndex < kBuckets; ++bucketIndex) {
        if (maxUsage < mHeader.mBucketUsage[bucketIndex])
            maxUsage = mHeader.mBucketUsage[bucketIndex];
    }
    // Determine new bucket size, halve size until maxUsage
    PRUint32 oldRecordsPerBucket = GetRecordsPerBucket();
    PRUint32 newRecordsPerBucket = oldRecordsPerBucket;
    while (maxUsage < (newRecordsPerBucket >> 1))
        newRecordsPerBucket >>= 1;
    if (newRecordsPerBucket < kMinRecordCount) 
        newRecordsPerBucket = kMinRecordCount;
    if (newRecordsPerBucket == oldRecordsPerBucket)
        return NS_OK;
    // Move the buckets close to each other
    for (bucketIndex = 0; bucketIndex < kBuckets; ++bucketIndex) {
        // Move bucket
        memmove(mRecordArray + bucketIndex * newRecordsPerBucket,
                mRecordArray + bucketIndex * oldRecordsPerBucket,
                mHeader.mBucketUsage[bucketIndex] * sizeof(nsDiskCacheRecord));
    }

    // Shrink the record array memory block itself
    PRUint32 newCount = newRecordsPerBucket * kBuckets;
    nsDiskCacheRecord* newArray = (nsDiskCacheRecord *)
            PR_REALLOC(mRecordArray, newCount * sizeof(nsDiskCacheRecord));
    if (!newArray)
        return NS_ERROR_OUT_OF_MEMORY;

    // Set as the new record array
    mRecordArray = newArray;
    mHeader.mRecordCount = newCount;
    return NS_OK;
}

nsresult
nsDiskCacheMap::AddRecord( nsDiskCacheRecord *  mapRecord,
                           nsDiskCacheRecord *  oldRecord)
{
    CACHE_LOG_DEBUG(("CACHE: AddRecord [%x]\n", mapRecord->HashNumber()));

    const PRUint32      hashNumber = mapRecord->HashNumber();
    const PRUint32      bucketIndex = GetBucketIndex(hashNumber);
    const PRUint32      count = mHeader.mBucketUsage[bucketIndex];

    oldRecord->SetHashNumber(0);  // signify no record

    if (count == GetRecordsPerBucket()) {
        // Ignore failure to grow the record space, we will then reuse old records
        GrowRecords();
    }
    
    nsDiskCacheRecord * records = GetFirstRecordInBucket(bucketIndex);
    if (count < GetRecordsPerBucket()) {
        // stick the new record at the end
        records[count] = *mapRecord;
        mHeader.mEntryCount++;
        mHeader.mBucketUsage[bucketIndex]++;           
        if (mHeader.mEvictionRank[bucketIndex] < mapRecord->EvictionRank())
            mHeader.mEvictionRank[bucketIndex] = mapRecord->EvictionRank();
    } else {
        // Find the record with the highest eviction rank
        nsDiskCacheRecord * mostEvictable = &records[0];
        for (int i = count-1; i > 0; i--) {
            if (records[i].EvictionRank() > mostEvictable->EvictionRank())
                mostEvictable = &records[i];
        }
        *oldRecord     = *mostEvictable;    // i == GetRecordsPerBucket(), so
                                            // evict the mostEvictable
        *mostEvictable = *mapRecord;        // replace it with the new record
        // check if we need to update mostEvictable entry in header
        if (mHeader.mEvictionRank[bucketIndex] < mapRecord->EvictionRank())
            mHeader.mEvictionRank[bucketIndex] = mapRecord->EvictionRank();
        if (oldRecord->EvictionRank() >= mHeader.mEvictionRank[bucketIndex]) 
            mHeader.mEvictionRank[bucketIndex] = GetBucketRank(bucketIndex, 0);
    }

    NS_ASSERTION(mHeader.mEvictionRank[bucketIndex] == GetBucketRank(bucketIndex, 0),
                 "eviction rank out of sync");
    return NS_OK;
}


nsresult
nsDiskCacheMap::UpdateRecord( nsDiskCacheRecord *  mapRecord)
{
    CACHE_LOG_DEBUG(("CACHE: UpdateRecord [%x]\n", mapRecord->HashNumber()));

    const PRUint32      hashNumber = mapRecord->HashNumber();
    const PRUint32      bucketIndex = GetBucketIndex(hashNumber);
    nsDiskCacheRecord * records = GetFirstRecordInBucket(bucketIndex);

    for (int i = mHeader.mBucketUsage[bucketIndex]-1; i >= 0; i--) {          
        if (records[i].HashNumber() == hashNumber) {
            const PRUint32 oldRank = records[i].EvictionRank();

            // stick the new record here            
            records[i] = *mapRecord;

            // update eviction rank in header if necessary
            if (mHeader.mEvictionRank[bucketIndex] < mapRecord->EvictionRank())
                mHeader.mEvictionRank[bucketIndex] = mapRecord->EvictionRank();
            else if (mHeader.mEvictionRank[bucketIndex] == oldRank)
                mHeader.mEvictionRank[bucketIndex] = GetBucketRank(bucketIndex, 0);

NS_ASSERTION(mHeader.mEvictionRank[bucketIndex] == GetBucketRank(bucketIndex, 0),
             "eviction rank out of sync");
            return NS_OK;
        }
    }
    NS_NOTREACHED("record not found");
    return NS_ERROR_UNEXPECTED;
}


nsresult
nsDiskCacheMap::FindRecord( PRUint32  hashNumber, nsDiskCacheRecord *  result)
{
    const PRUint32      bucketIndex = GetBucketIndex(hashNumber);
    nsDiskCacheRecord * records = GetFirstRecordInBucket(bucketIndex);

    for (int i = mHeader.mBucketUsage[bucketIndex]-1; i >= 0; i--) {          
        if (records[i].HashNumber() == hashNumber) {
            *result = records[i];    // copy the record
            NS_ASSERTION(result->ValidRecord(), "bad cache map record");
            return NS_OK;
        }
    }
    return NS_ERROR_CACHE_KEY_NOT_FOUND;
}


nsresult
nsDiskCacheMap::DeleteRecord( nsDiskCacheRecord *  mapRecord)
{
    CACHE_LOG_DEBUG(("CACHE: DeleteRecord [%x]\n", mapRecord->HashNumber()));

    const PRUint32      hashNumber = mapRecord->HashNumber();
    const PRUint32      bucketIndex = GetBucketIndex(hashNumber);
    nsDiskCacheRecord * records = GetFirstRecordInBucket(bucketIndex);
    PRUint32            last = mHeader.mBucketUsage[bucketIndex]-1;

    for (int i = last; i >= 0; i--) {          
        if (records[i].HashNumber() == hashNumber) {
            // found it, now delete it.
            PRUint32  evictionRank = records[i].EvictionRank();
            NS_ASSERTION(evictionRank == mapRecord->EvictionRank(),
                         "evictionRank out of sync");
            // if not the last record, shift last record into opening
            records[i] = records[last];
            records[last].SetHashNumber(0); // clear last record
            mHeader.mBucketUsage[bucketIndex] = last;
            mHeader.mEntryCount--;

            // update eviction rank
            PRUint32  bucketIndex = GetBucketIndex(mapRecord->HashNumber());
            if (mHeader.mEvictionRank[bucketIndex] <= evictionRank) {
                mHeader.mEvictionRank[bucketIndex] = GetBucketRank(bucketIndex, 0);
            }

            NS_ASSERTION(mHeader.mEvictionRank[bucketIndex] ==
                         GetBucketRank(bucketIndex, 0), "eviction rank out of sync");
            return NS_OK;
        }
    }
    return NS_ERROR_UNEXPECTED;
}


PRInt32
nsDiskCacheMap::VisitEachRecord(PRUint32                    bucketIndex,
                                nsDiskCacheRecordVisitor *  visitor,
                                PRUint32                    evictionRank)
{
    PRInt32             rv = kVisitNextRecord;
    PRUint32            count = mHeader.mBucketUsage[bucketIndex];
    nsDiskCacheRecord * records = GetFirstRecordInBucket(bucketIndex);

    // call visitor for each entry (matching any eviction rank)
    for (int i = count-1; i >= 0; i--) {
        if (evictionRank > records[i].EvictionRank()) continue;

        rv = visitor->VisitRecord(&records[i]);
        if (rv == kStopVisitingRecords) 
            break;    // Stop visiting records
        
        if (rv == kDeleteRecordAndContinue) {
            --count;
            records[i] = records[count];
            records[count].SetHashNumber(0);
        }
    }

    if (mHeader.mBucketUsage[bucketIndex] - count != 0) {
        mHeader.mEntryCount -= mHeader.mBucketUsage[bucketIndex] - count;
        mHeader.mBucketUsage[bucketIndex] = count;
        // recalc eviction rank
        mHeader.mEvictionRank[bucketIndex] = GetBucketRank(bucketIndex, 0);
    }
    NS_ASSERTION(mHeader.mEvictionRank[bucketIndex] ==
                 GetBucketRank(bucketIndex, 0), "eviction rank out of sync");

    return rv;
}


/**
 *  VisitRecords
 *
 *  Visit every record in cache map in the most convenient order
 */
nsresult
nsDiskCacheMap::VisitRecords( nsDiskCacheRecordVisitor *  visitor)
{
    for (int bucketIndex = 0; bucketIndex < kBuckets; ++bucketIndex) {
        if (VisitEachRecord(bucketIndex, visitor, 0) == kStopVisitingRecords)
            break;
    }   
    return NS_OK;
}


/**
 *  EvictRecords
 *
 *  Just like VisitRecords, but visits the records in order of their eviction rank
 */
nsresult
nsDiskCacheMap::EvictRecords( nsDiskCacheRecordVisitor * visitor)
{
    PRUint32  tempRank[kBuckets];
    int       bucketIndex = 0;
    
    // copy eviction rank array
    for (bucketIndex = 0; bucketIndex < kBuckets; ++bucketIndex)
        tempRank[bucketIndex] = mHeader.mEvictionRank[bucketIndex];

    // Maximum number of iterations determined by number of records
    // as a safety limiter for the loop. Use a copy of mHeader.mEntryCount since
    // the value could decrease if some entry is evicted.
    PRInt32 entryCount = mHeader.mEntryCount;
    for (int n = 0; n < entryCount; ++n) {
    
        // find bucket with highest eviction rank
        PRUint32    rank  = 0;
        for (int i = 0; i < kBuckets; ++i) {
            if (rank < tempRank[i]) {
                rank = tempRank[i];
                bucketIndex = i;
            }
        }
        
        if (rank == 0) break;  // we've examined all the records

        // visit records in bucket with eviction ranks >= target eviction rank
        if (VisitEachRecord(bucketIndex, visitor, rank) == kStopVisitingRecords)
            break;

        // find greatest rank less than 'rank'
        tempRank[bucketIndex] = GetBucketRank(bucketIndex, rank);
    }
    return NS_OK;
}



nsresult
nsDiskCacheMap::OpenBlockFiles()
{
    // create nsILocalFile for block file
    nsCOMPtr<nsILocalFile> blockFile;
    nsresult rv = NS_OK;
    
    for (int i = 0; i < kNumBlockFiles; ++i) {
        rv = GetBlockFileForIndex(i, getter_AddRefs(blockFile));
        if (NS_FAILED(rv)) break;
    
        PRUint32 blockSize = GetBlockSizeForIndex(i+1); // +1 to match file selectors 1,2,3
        PRUint32 bitMapSize = GetBitMapSizeForIndex(i+1);
        rv = mBlockFile[i].Open(blockFile, blockSize, bitMapSize);
        if (NS_FAILED(rv)) break;
    }
    // close all files in case of any error
    if (NS_FAILED(rv)) 
        (void)CloseBlockFiles(false); // we already have an error to report

    return rv;
}


nsresult
nsDiskCacheMap::CloseBlockFiles(bool flush)
{
    nsresult rv, rv2 = NS_OK;
    for (int i=0; i < kNumBlockFiles; ++i) {
        rv = mBlockFile[i].Close(flush);
        if (NS_FAILED(rv))  rv2 = rv;   // if one or more errors, report at least one
    }
    return rv2;
}


bool
nsDiskCacheMap::CacheFilesExist()
{
    nsCOMPtr<nsILocalFile> blockFile;
    nsresult rv;
    
    for (int i = 0; i < kNumBlockFiles; ++i) {
        bool exists;
        rv = GetBlockFileForIndex(i, getter_AddRefs(blockFile));
        if (NS_FAILED(rv))  return false;

        rv = blockFile->Exists(&exists);
        if (NS_FAILED(rv) || !exists)  return false;
    }

    return true;
}


nsresult
nsDiskCacheMap::CreateCacheSubDirectories()
{
    if (!mCacheDirectory)
        return NS_ERROR_UNEXPECTED;

    for (PRInt32 index = 0 ; index < 16 ; index++) {
        nsCOMPtr<nsIFile> file;
        nsresult rv = mCacheDirectory->Clone(getter_AddRefs(file));
        if (NS_FAILED(rv))
            return rv;

        rv = file->AppendNative(nsPrintfCString("%X", index));
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);
        rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0700);
        if (NS_FAILED(rv))
            return rv;
    }

    return NS_OK;
}


nsDiskCacheEntry *
nsDiskCacheMap::ReadDiskCacheEntry(nsDiskCacheRecord * record)
{
    CACHE_LOG_DEBUG(("CACHE: ReadDiskCacheEntry [%x]\n", record->HashNumber()));

    nsresult            rv         = NS_ERROR_UNEXPECTED;
    nsDiskCacheEntry *  diskEntry  = nsnull;
    PRUint32            metaFile   = record->MetaFile();
    PRInt32             bytesRead  = 0;
    
    if (!record->MetaLocationInitialized())  return nsnull;
    
    if (metaFile == 0) {  // entry/metadata stored in separate file
        // open and read the file
        nsCOMPtr<nsILocalFile> file;
        rv = GetLocalFileForDiskCacheRecord(record,
                                            nsDiskCache::kMetaData,
                                            false,
                                            getter_AddRefs(file));
        NS_ENSURE_SUCCESS(rv, nsnull);

        PRFileDesc * fd = nsnull;
        // open the file - restricted to user, the data could be confidential
        rv = file->OpenNSPRFileDesc(PR_RDONLY, 00600, &fd);
        NS_ENSURE_SUCCESS(rv, nsnull);
        
        PRInt32 fileSize = PR_Available(fd);
        if (fileSize < 0) {
            // an error occurred. We could call PR_GetError(), but how would that help?
            rv = NS_ERROR_UNEXPECTED;
        } else {
            rv = EnsureBuffer(fileSize);
            if (NS_SUCCEEDED(rv)) {
                bytesRead = PR_Read(fd, mBuffer, fileSize);
                if (bytesRead < fileSize) {
                    rv = NS_ERROR_UNEXPECTED;
                }
            }
        }
        PR_Close(fd);
        NS_ENSURE_SUCCESS(rv, nsnull);

    } else if (metaFile < (kNumBlockFiles + 1)) {
        // entry/metadata stored in cache block file
        
        // allocate buffer
        PRUint32 blockCount = record->MetaBlockCount();
        bytesRead = blockCount * GetBlockSizeForIndex(metaFile);

        rv = EnsureBuffer(bytesRead);
        NS_ENSURE_SUCCESS(rv, nsnull);
        
        // read diskEntry, note when the blocks are at the end of file, 
        // bytesRead may be less than blockSize*blockCount.
        // But the bytesRead should at least agree with the real disk entry size.
        rv = mBlockFile[metaFile - 1].ReadBlocks(mBuffer,
                                                 record->MetaStartBlock(),
                                                 blockCount, 
                                                 &bytesRead);
        NS_ENSURE_SUCCESS(rv, nsnull);
    }
    diskEntry = (nsDiskCacheEntry *)mBuffer;
    diskEntry->Unswap();    // disk to memory
    // Check if calculated size agrees with bytesRead
    if (bytesRead < 0 || (PRUint32)bytesRead < diskEntry->Size())
        return nsnull;

    // Return the buffer containing the diskEntry structure
    return diskEntry;
}


/**
 *  CreateDiskCacheEntry(nsCacheEntry * entry)
 *
 *  Prepare an nsCacheEntry for writing to disk
 */
nsDiskCacheEntry *
nsDiskCacheMap::CreateDiskCacheEntry(nsDiskCacheBinding *  binding,
                                     PRUint32 * aSize)
{
    nsCacheEntry * entry = binding->mCacheEntry;
    if (!entry)  return nsnull;
    
    // Store security info, if it is serializable
    nsCOMPtr<nsISupports> infoObj = entry->SecurityInfo();
    nsCOMPtr<nsISerializable> serializable = do_QueryInterface(infoObj);
    if (infoObj && !serializable) return nsnull;
    if (serializable) {
        nsCString info;
        nsresult rv = NS_SerializeToString(serializable, info);
        if (NS_FAILED(rv)) return nsnull;
        rv = entry->SetMetaDataElement("security-info", info.get());
        if (NS_FAILED(rv)) return nsnull;
    }

    PRUint32  keySize  = entry->Key()->Length() + 1;
    PRUint32  metaSize = entry->MetaDataSize();
    PRUint32  size     = sizeof(nsDiskCacheEntry) + keySize + metaSize;
    
    if (aSize) *aSize = size;
    
    nsresult rv = EnsureBuffer(size);
    if (NS_FAILED(rv)) return nsnull;

    nsDiskCacheEntry *diskEntry = (nsDiskCacheEntry *)mBuffer;
    diskEntry->mHeaderVersion   = nsDiskCache::kCurrentVersion;
    diskEntry->mMetaLocation    = binding->mRecord.MetaLocation();
    diskEntry->mFetchCount      = entry->FetchCount();
    diskEntry->mLastFetched     = entry->LastFetched();
    diskEntry->mLastModified    = entry->LastModified();
    diskEntry->mExpirationTime  = entry->ExpirationTime();
    diskEntry->mDataSize        = entry->DataSize();
    diskEntry->mKeySize         = keySize;
    diskEntry->mMetaDataSize    = metaSize;
    
    memcpy(diskEntry->Key(), entry->Key()->get(), keySize);
    
    rv = entry->FlattenMetaData(diskEntry->MetaData(), metaSize);
    if (NS_FAILED(rv)) return nsnull;
    
    return diskEntry;
}


nsresult
nsDiskCacheMap::WriteDiskCacheEntry(nsDiskCacheBinding *  binding)
{
    CACHE_LOG_DEBUG(("CACHE: WriteDiskCacheEntry [%x]\n",
        binding->mRecord.HashNumber()));

    nsresult            rv        = NS_OK;
    PRUint32            size;
    nsDiskCacheEntry *  diskEntry =  CreateDiskCacheEntry(binding, &size);
    if (!diskEntry)  return NS_ERROR_UNEXPECTED;
    
    PRUint32  fileIndex = CalculateFileIndex(size);

    // Deallocate old storage if necessary    
    if (binding->mRecord.MetaLocationInitialized()) {
        // we have existing storage

        if ((binding->mRecord.MetaFile() == 0) &&
            (fileIndex == 0)) {  // keeping the separate file
            // just decrement total
            DecrementTotalSize(binding->mRecord.MetaFileSize());
            NS_ASSERTION(binding->mRecord.MetaFileGeneration() == binding->mGeneration,
                         "generations out of sync");
        } else {
            rv = DeleteStorage(&binding->mRecord, nsDiskCache::kMetaData);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    binding->mRecord.SetEvictionRank(ULONG_MAX - SecondsFromPRTime(PR_Now()));
    // write entry data to disk cache block file
    diskEntry->Swap();

    if (fileIndex != 0) {
        while (1) {
            PRUint32  blockSize = GetBlockSizeForIndex(fileIndex);
            PRUint32  blocks    = ((size - 1) / blockSize) + 1;

            PRInt32 startBlock;
            rv = mBlockFile[fileIndex - 1].WriteBlocks(diskEntry, size, blocks,
                                                       &startBlock);
            if (NS_SUCCEEDED(rv)) {
                // update binding and cache map record
                binding->mRecord.SetMetaBlocks(fileIndex, startBlock, blocks);

                rv = UpdateRecord(&binding->mRecord);
                NS_ENSURE_SUCCESS(rv, rv);

                // XXX we should probably write out bucket ourselves

                IncrementTotalSize(blocks, blockSize);
                break;
            }

            if (fileIndex == kNumBlockFiles) {
                fileIndex = 0; // write data to separate file
                break;
            }

            // try next block file
            fileIndex++;
        }
    }

    if (fileIndex == 0) {
        // Write entry data to separate file
        PRUint32 metaFileSizeK = ((size + 0x03FF) >> 10); // round up to nearest 1k
        if (metaFileSizeK > kMaxDataSizeK)
            metaFileSizeK = kMaxDataSizeK;

        binding->mRecord.SetMetaFileGeneration(binding->mGeneration);
        binding->mRecord.SetMetaFileSize(metaFileSizeK);
        rv = UpdateRecord(&binding->mRecord);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsILocalFile> localFile;
        rv = GetLocalFileForDiskCacheRecord(&binding->mRecord,
                                            nsDiskCache::kMetaData,
                                            true,
                                            getter_AddRefs(localFile));
        NS_ENSURE_SUCCESS(rv, rv);
        
        // open the file
        PRFileDesc * fd;
        // open the file - restricted to user, the data could be confidential
        rv = localFile->OpenNSPRFileDesc(PR_RDWR | PR_TRUNCATE | PR_CREATE_FILE, 00600, &fd);
        NS_ENSURE_SUCCESS(rv, rv);

        // write the file
        PRInt32 bytesWritten = PR_Write(fd, diskEntry, size);
        
        PRStatus err = PR_Close(fd);
        if ((bytesWritten != (PRInt32)size) || (err != PR_SUCCESS)) {
            return NS_ERROR_UNEXPECTED;
        }

        IncrementTotalSize(metaFileSizeK);
    }

    return rv;
}


nsresult
nsDiskCacheMap::ReadDataCacheBlocks(nsDiskCacheBinding * binding, char * buffer, PRUint32 size)
{
    CACHE_LOG_DEBUG(("CACHE: ReadDataCacheBlocks [%x size=%u]\n",
        binding->mRecord.HashNumber(), size));

    PRUint32  fileIndex = binding->mRecord.DataFile();
    PRInt32   readSize = size;
    
    nsresult rv = mBlockFile[fileIndex - 1].ReadBlocks(buffer,
                                                       binding->mRecord.DataStartBlock(),
                                                       binding->mRecord.DataBlockCount(),
                                                       &readSize);
    NS_ENSURE_SUCCESS(rv, rv);
    if (readSize < (PRInt32)size) {
        rv = NS_ERROR_UNEXPECTED;
    } 
    return rv;
}


nsresult
nsDiskCacheMap::WriteDataCacheBlocks(nsDiskCacheBinding * binding, char * buffer, PRUint32 size)
{
    CACHE_LOG_DEBUG(("CACHE: WriteDataCacheBlocks [%x size=%u]\n",
        binding->mRecord.HashNumber(), size));

    nsresult  rv = NS_OK;
    
    // determine block file & number of blocks
    PRUint32  fileIndex  = CalculateFileIndex(size);
    PRUint32  blockCount = 0;
    PRInt32   startBlock = 0;

    if (size > 0) {
        while (1) {
            PRUint32  blockSize  = GetBlockSizeForIndex(fileIndex);
            blockCount = ((size - 1) / blockSize) + 1;

            rv = mBlockFile[fileIndex - 1].WriteBlocks(buffer, size, blockCount,
                                                       &startBlock);
            if (NS_SUCCEEDED(rv)) {
                IncrementTotalSize(blockCount, blockSize);
                break;
            }

            if (fileIndex == kNumBlockFiles)
                return rv;

            fileIndex++;
        }
    }

    // update binding and cache map record
    binding->mRecord.SetDataBlocks(fileIndex, startBlock, blockCount);
    if (!binding->mDoomed) {
        rv = UpdateRecord(&binding->mRecord);
    }
    return rv;
}


nsresult
nsDiskCacheMap::DeleteStorage(nsDiskCacheRecord * record)
{
    nsresult  rv1 = DeleteStorage(record, nsDiskCache::kData);
    nsresult  rv2 = DeleteStorage(record, nsDiskCache::kMetaData);
    return NS_FAILED(rv1) ? rv1 : rv2;
}


nsresult
nsDiskCacheMap::DeleteStorage(nsDiskCacheRecord * record, bool metaData)
{
    CACHE_LOG_DEBUG(("CACHE: DeleteStorage [%x %u]\n", record->HashNumber(),
        metaData));

    nsresult    rv = NS_ERROR_UNEXPECTED;
    PRUint32    fileIndex = metaData ? record->MetaFile() : record->DataFile();
    nsCOMPtr<nsIFile> file;
    
    if (fileIndex == 0) {
        // delete the file
        PRUint32  sizeK = metaData ? record->MetaFileSize() : record->DataFileSize();
        // XXX if sizeK == USHRT_MAX, stat file for actual size

        rv = GetFileForDiskCacheRecord(record, metaData, false, getter_AddRefs(file));
        if (NS_SUCCEEDED(rv)) {
            rv = file->Remove(false);    // false == non-recursive
        }
        DecrementTotalSize(sizeK);
        
    } else if (fileIndex < (kNumBlockFiles + 1)) {
        // deallocate blocks
        PRUint32  startBlock = metaData ? record->MetaStartBlock() : record->DataStartBlock();
        PRUint32  blockCount = metaData ? record->MetaBlockCount() : record->DataBlockCount();
        
        rv = mBlockFile[fileIndex - 1].DeallocateBlocks(startBlock, blockCount);
        DecrementTotalSize(blockCount, GetBlockSizeForIndex(fileIndex));
    }
    if (metaData)  record->ClearMetaLocation();
    else           record->ClearDataLocation();
    
    return rv;
}


nsresult
nsDiskCacheMap::GetFileForDiskCacheRecord(nsDiskCacheRecord * record,
                                          bool                meta,
                                          bool                createPath,
                                          nsIFile **          result)
{
    if (!mCacheDirectory)  return NS_ERROR_NOT_AVAILABLE;
    
    nsCOMPtr<nsIFile> file;
    nsresult rv = mCacheDirectory->Clone(getter_AddRefs(file));
    if (NS_FAILED(rv))  return rv;

    PRUint32 hash = record->HashNumber();

    // The file is stored under subdirectories according to the hash number:
    // 0x01234567 -> 0/12/
    rv = file->AppendNative(nsPrintfCString("%X", hash >> 28));
    if (NS_FAILED(rv))  return rv;
    rv = file->AppendNative(nsPrintfCString("%02X", (hash >> 20) & 0xFF));
    if (NS_FAILED(rv))  return rv;

    bool exists;
    if (createPath && (NS_FAILED(file->Exists(&exists)) || !exists)) {
        nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);
        rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0700);
        if (NS_FAILED(rv))  return rv;
    }

    PRInt16 generation = record->Generation();
    char name[32];
    // Cut the beginning of the hash that was used in the path
    ::sprintf(name, "%05X%c%02X", hash & 0xFFFFF, (meta ? 'm' : 'd'),
              generation);
    rv = file->AppendNative(nsDependentCString(name));
    if (NS_FAILED(rv))  return rv;
    
    NS_IF_ADDREF(*result = file);
    return rv;
}


nsresult
nsDiskCacheMap::GetLocalFileForDiskCacheRecord(nsDiskCacheRecord * record,
                                               bool                meta,
                                               bool                createPath,
                                               nsILocalFile **     result)
{
    nsCOMPtr<nsIFile> file;
    nsresult rv = GetFileForDiskCacheRecord(record,
                                            meta,
                                            createPath,
                                            getter_AddRefs(file));
    if (NS_FAILED(rv))  return rv;
    
    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);
    if (NS_FAILED(rv))  return rv;
    
    NS_IF_ADDREF(*result = localFile);
    return rv;
}


nsresult
nsDiskCacheMap::GetBlockFileForIndex(PRUint32 index, nsILocalFile ** result)
{
    if (!mCacheDirectory)  return NS_ERROR_NOT_AVAILABLE;
    
    nsCOMPtr<nsIFile> file;
    nsresult rv = mCacheDirectory->Clone(getter_AddRefs(file));
    if (NS_FAILED(rv))  return rv;
    
    char name[32];
    ::sprintf(name, "_CACHE_%03d_", index + 1);
    rv = file->AppendNative(nsDependentCString(name));
    if (NS_FAILED(rv))  return rv;
    
    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file, &rv);
    NS_IF_ADDREF(*result = localFile);

    return rv;
}


PRUint32
nsDiskCacheMap::CalculateFileIndex(PRUint32 size)
{
    // We prefer to use block file with larger block if the wasted space would
    // be the same. E.g. store entry with size of 3073 bytes in 1 4K-block
    // instead of in 4 1K-blocks.

    if (size <= 3 * BLOCK_SIZE_FOR_INDEX(1))  return 1;
    if (size <= 3 * BLOCK_SIZE_FOR_INDEX(2))  return 2;
    if (size <= 4 * BLOCK_SIZE_FOR_INDEX(3))  return 3;
    return 0;
}

nsresult
nsDiskCacheMap::EnsureBuffer(PRUint32 bufSize)
{
    if (mBufferSize < bufSize) {
        char * buf = (char *)PR_REALLOC(mBuffer, bufSize);
        if (!buf) {
            mBufferSize = 0;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        mBuffer = buf;
        mBufferSize = bufSize;
    }
    return NS_OK;
}        

void
nsDiskCacheMap::NotifyCapacityChange(PRUint32 capacity)
{
  // Heuristic 1. average cache entry size is probably around 1KB
  // Heuristic 2. we don't want more than 32MB reserved to store the record
  //              map in memory.
  const PRInt32 RECORD_COUNT_LIMIT = 32 * 1024 * 1024 / sizeof(nsDiskCacheRecord);
  PRInt32 maxRecordCount = NS_MIN(PRInt32(capacity), RECORD_COUNT_LIMIT);
  if (mMaxRecordCount < maxRecordCount) {
    // We can only grow
    mMaxRecordCount = maxRecordCount;
  }
}
