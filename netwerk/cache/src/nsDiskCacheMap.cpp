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
 *    Gordon Sheridan  <gordon@netscape.com>
 */

#include "nsDiskCacheMap.h"
#include "nsDiskCacheBinding.h"
#include "nsDiskCacheEntry.h"

#include "nsCache.h"

#include "nsCRT.h"

#include <string.h>


/******************************************************************************
 *  nsDiskCacheBucket
 *****************************************************************************/

void
nsDiskCacheBucket::Swap()
{
    nsDiskCacheRecord * record = &mRecords[0];
    for (int i = 0; i < kRecordsPerBucket; ++i) {
        if (record->HashNumber() == 0)
            break;
        record->Swap();
    }
}

void
nsDiskCacheBucket::Unswap()
{
    nsDiskCacheRecord * record = &mRecords[0];
    for (int i = 0; i < kRecordsPerBucket; ++i) {
        if (record->HashNumber() == 0)
            break;
        record->Unswap();
    }
}


PRInt32
nsDiskCacheBucket::CountRecords()
{
    if (mRecords[0].HashNumber() == 0)  return 0;
    
    PRUint32 i      = kRecordsPerBucket >> 1;
    PRUint32 offset = kRecordsPerBucket >> 2;
    
    while (offset > 0) {
        if (mRecords[i].HashNumber())  i += offset;
        else                           i -= offset;
        offset >>= 1;
    }
    
    if (mRecords[i].HashNumber() != 0)
        ++i;
    
    return i;
}


PRUint32
nsDiskCacheBucket::EvictionRank(PRUint32 targetRank)
{
    PRUint32  rank = 0;
    for (int i = CountRecords() - 1; i >= 0; --i) {
        if ((rank < mRecords[i].EvictionRank()) &&
            ((targetRank == 0) || (mRecords[i].EvictionRank() < targetRank)))
                rank = mRecords[i].EvictionRank();
    }
    return rank;
}


PRInt32
nsDiskCacheBucket::VisitEachRecord(nsDiskCacheRecordVisitor *  visitor,
                                   PRUint32                    evictionRank,
                                   PRUint32 *                  result)
{
    PRUint32 recordsDeleted = 0;
    PRInt32  rv = kVisitNextRecord;
    PRInt32  last  = CountRecords() - 1;
    
    // call visitor for each entry (matching any eviction rank)
    for (int i = last; i >= 0; i--) {
        if (evictionRank > mRecords[i].EvictionRank())  continue;
    
        rv = visitor->VisitRecord(&mRecords[i]);
        if (rv == kVisitNextRecord) continue;
        
        if (rv == kDeleteRecordAndContinue) {
            mRecords[i] = mRecords[last];
            mRecords[last].SetHashNumber(0);
            --last;
            ++recordsDeleted;
            continue;
        }

        *result = recordsDeleted;
        return kStopVisitingRecords;  // rv == kStopVisitingRecords
    }
    
    *result = recordsDeleted;
    return rv;
}


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
    if (NS_FAILED(rv))  return rv;
    rv = localFile->AppendNative(NS_LITERAL_CSTRING("_CACHE_MAP_"));
    if (NS_FAILED(rv))  return rv;

    // open the file
    rv = localFile->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE, 00666, &mMapFD);
    if (NS_FAILED(rv))  return NS_ERROR_FILE_CORRUPTED;

    PRBool cacheFilesExist = CacheFilesExist();
    rv = NS_ERROR_FILE_CORRUPTED;  // presume the worst

    // check size of map file
    PRInt32  mapSize = PR_Available(mMapFD);    
    if (mapSize == 0) {  // creating a new _CACHE_MAP_

        // block files shouldn't exist if we're creating the _CACHE_MAP_
        if (cacheFilesExist)  goto error_exit;

        // create the file - initialize in memory
        mHeader.mVersion    = nsDiskCache::kCurrentVersion;
        mHeader.mDataSize   = 0;
        mHeader.mEntryCount = 0;
        mHeader.mIsDirty = PR_TRUE;
        for (int i = 0; i < kBucketsPerTable; ++i) {
            mHeader.mEvictionRank[i] = 0;
        }

        memset(mHeader.reserved, 0, nsDiskCacheHeader::kReservedBytes);
        memset(mBuckets, 0, sizeof(nsDiskCacheBucket) * kBucketsPerTable);
        
    } else if (mapSize == kCacheMapSize) {  // read existing _CACHE_MAP_
        
        // if _CACHE_MAP_ exists, so should the block files
        if (!cacheFilesExist)  goto error_exit;

        // read it in
        PRUint32 bytesRead = PR_Read(mMapFD, &mHeader, kCacheMapSize);
        if (kCacheMapSize != bytesRead)  goto error_exit;

        mHeader.Unswap();
        if (mHeader.mIsDirty ||
            mHeader.mVersion != nsDiskCache::kCurrentVersion)
            goto error_exit;
        
        // Unswap each bucket
        PRInt32 total = 0;
        for (PRUint32 i = 0; i < kBucketsPerTable; ++i) {
            mBuckets[i].Unswap();
            total += mBuckets[i].CountRecords();
        }
        
        // verify entry count
        if (total != mHeader.mEntryCount)  goto error_exit;


    } else {
        goto error_exit;
    }

    rv = OpenBlockFiles();
    if (NS_FAILED(rv))  goto error_exit;

    // set dirty bit and flush header
    mHeader.mIsDirty    = PR_TRUE;
    rv = FlushHeader();
    if (NS_FAILED(rv))  goto error_exit;
    
    return NS_OK;
    
error_exit:
    (void) CloseBlockFiles(PR_FALSE);
    
    if (mMapFD) {
        (void) PR_Close(mMapFD);
        mMapFD = nsnull;
    }
    
    return rv;
}


nsresult
nsDiskCacheMap::Close(PRBool flush)
{
    if (!mMapFD)  return NS_OK;

    // close block files
    nsresult  rv = CloseBlockFiles(flush);
    if (NS_FAILED(rv)) goto exit;  // this is going to be a mess...
    
    if (flush) {
        // write map record buckets
        rv = FlushBuckets(PR_FALSE);   // don't bother swapping buckets back
        if (NS_FAILED(rv))  goto exit;
    
        // clear dirty bit
        mHeader.mIsDirty    = PR_FALSE;

        rv = FlushHeader();
    }

exit:
    PRStatus err = PR_Close(mMapFD);
    mMapFD = nsnull;
    
    if (NS_FAILED(rv))  return rv;
    return err == PR_SUCCESS ? NS_OK : NS_ERROR_UNEXPECTED;
}


nsresult
nsDiskCacheMap::Trim()
{
    nsresult rv, rv2 = NS_OK;
    for (int i=0; i < 3; ++i) {
        rv = mBlockFile[i].Trim();
        if (NS_FAILED(rv))  rv2 = rv;   // if one or more errors, report at least one
    }
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
    
    return NS_OK;
}


nsresult
nsDiskCacheMap::FlushBuckets(PRBool unswap)
{
    if (!mMapFD)  return NS_ERROR_NOT_AVAILABLE;
    
    // seek to beginning of buckets
    PRInt32 filePos = PR_Seek(mMapFD, sizeof(nsDiskCacheHeader), PR_SEEK_SET);
    if (filePos != sizeof(nsDiskCacheHeader))  return NS_ERROR_UNEXPECTED;
    
    // Swap each bucket
    for (PRUint32 i = 0; i < kBucketsPerTable; ++i) {
        mBuckets[i].Swap();
    }
    
    PRInt32 bytesWritten = PR_Write(mMapFD, &mBuckets, sizeof(nsDiskCacheBucket) * kBucketsPerTable);

    if (unswap) {
        // Unswap each bucket
        for (PRUint32 i = 0; i < kBucketsPerTable; ++i) {
            mBuckets[i].Unswap();
        }
    }

    if ( sizeof(nsDiskCacheBucket) * kBucketsPerTable != bytesWritten) {
        return NS_ERROR_UNEXPECTED;
    }
    
    return NS_OK;
}


/**
 *  Record operations
 */

nsresult
nsDiskCacheMap::AddRecord( nsDiskCacheRecord *  mapRecord,
                           nsDiskCacheRecord *  oldRecord)
{
    PRUint32            hashNumber = mapRecord->HashNumber();
    nsDiskCacheBucket * bucket= GetBucketForHashNumber(hashNumber);
    PRUint32            bucketIndex = GetBucketIndex(hashNumber);
    int                 i;

    oldRecord->SetHashNumber(0);  // signify no record
    
    nsDiskCacheRecord * mostEvictable = &bucket->mRecords[0];
    for (i = 0; i < kRecordsPerBucket; ++i) {
        if (bucket->mRecords[i].HashNumber() == 0) {
            // stick the new record here
            bucket->mRecords[i] = *mapRecord;
            ++mHeader.mEntryCount;
            
            // update eviction rank in header if necessary
            if (mHeader.mEvictionRank[bucketIndex] < mapRecord->EvictionRank())
                mHeader.mEvictionRank[bucketIndex] = mapRecord->EvictionRank();

            NS_ASSERTION(mHeader.mEvictionRank[bucketIndex] == bucket->EvictionRank(0),
                         "eviction rank out of sync");
            return NS_OK;
        }
        
        if (bucket->mRecords[i].EvictionRank() > mostEvictable->EvictionRank())
            mostEvictable = &bucket->mRecords[i];
    }
    
    *oldRecord     = *mostEvictable;    // i == kRecordsPerBucket, so evict the mostEvictable
    *mostEvictable = *mapRecord;        // replace it with the new record
    
    // check if we need to update mostEvictable entry in header
    if ((oldRecord->HashNumber() != 0) ||
         (mapRecord->EvictionRank() > mHeader.mEvictionRank[bucketIndex])) {
         
        mHeader.mEvictionRank[bucketIndex] = bucket->EvictionRank(0);
    }

NS_ASSERTION(mHeader.mEvictionRank[bucketIndex] == bucket->EvictionRank(0),
             "eviction rank out of sync");    
    return NS_OK;
}


nsresult
nsDiskCacheMap::UpdateRecord( nsDiskCacheRecord *  mapRecord)
{
    PRUint32            hashNumber = mapRecord->HashNumber();
    nsDiskCacheBucket * bucket = GetBucketForHashNumber(hashNumber);

    for (int i = 0; i < kRecordsPerBucket; ++i) {
        if (bucket->mRecords[i].HashNumber() == mapRecord->HashNumber()) {
            PRUint32 oldRank = bucket->mRecords[i].EvictionRank();

            // stick the new record here            
            bucket->mRecords[i] = *mapRecord;

            // update eviction rank in header if necessary
            PRUint32  bucketIndex = GetBucketIndex(mapRecord->HashNumber());
            if (mHeader.mEvictionRank[bucketIndex] < mapRecord->EvictionRank())
                mHeader.mEvictionRank[bucketIndex] = mapRecord->EvictionRank();
            else if (mHeader.mEvictionRank[bucketIndex] == oldRank)
                mHeader.mEvictionRank[bucketIndex] = bucket->EvictionRank(0);

NS_ASSERTION(mHeader.mEvictionRank[bucketIndex] == bucket->EvictionRank(0),
             "eviction rank out of sync");
            return NS_OK;
        }
    }
    return NS_ERROR_UNEXPECTED;
}


nsresult
nsDiskCacheMap::FindRecord( PRUint32  hashNumber, nsDiskCacheRecord *  result)
{
    nsDiskCacheBucket * bucket = GetBucketForHashNumber(hashNumber);
    
    for (int i = 0; i < kRecordsPerBucket; ++i) {
        if (bucket->mRecords[i].HashNumber() == 0)  break;
            
        if (bucket->mRecords[i].HashNumber() == hashNumber) {
            *result = bucket->mRecords[i];    // copy the record
            NS_ASSERTION(result->ValidRecord(), "bad cache map record");
            return NS_OK;
        }
    }
    return NS_ERROR_CACHE_KEY_NOT_FOUND;
}


nsresult
nsDiskCacheMap::DeleteRecord( nsDiskCacheRecord *  mapRecord)
{
    nsDiskCacheBucket * bucket = GetBucketForHashNumber(mapRecord->HashNumber());
    
    PRInt32 count = bucket->CountRecords();
    for (PRInt32 i = 0; i < count; ++i) {        
        if (bucket->mRecords[i].HashNumber() == mapRecord->HashNumber()) {
            // found it, now delete it.
            PRUint32  evictionRank = bucket->mRecords[i].EvictionRank();
            NS_ASSERTION(evictionRank == mapRecord->EvictionRank(),
                         "evictionRank out of sync");
            if (i != (count - 1)) {
                // if not the last record, shift last record into opening
                bucket->mRecords[i] = bucket->mRecords[count - 1];
            }
            bucket->mRecords[count - 1].SetHashNumber(0); // clear last record
            mHeader.mEntryCount--;
            // update eviction rank
            PRUint32  bucketIndex = GetBucketIndex(mapRecord->HashNumber());
            if (mHeader.mEvictionRank[bucketIndex] <= evictionRank) {
                mHeader.mEvictionRank[bucketIndex] = bucket->EvictionRank(0);
            }

            NS_ASSERTION(mHeader.mEvictionRank[bucketIndex] ==
                         bucket->EvictionRank(0), "eviction rank out of sync");
            return NS_OK;
        }
    }
    return NS_ERROR_UNEXPECTED;
}


/**
 *  VisitRecords
 *
 *  Visit every record in cache map in the most convenient order
 */
nsresult
nsDiskCacheMap::VisitRecords( nsDiskCacheRecordVisitor *  visitor)
{
    for (PRUint32 i = 0; i < kBucketsPerTable; ++i) {
        // get bucket
        PRUint32 recordsDeleted;
        PRBool continueFlag = mBuckets[i].VisitEachRecord(visitor, 0, &recordsDeleted);
        if (recordsDeleted) {
            // recalc eviction rank
            mHeader.mEvictionRank[i] = mBuckets[i].EvictionRank(0);
            mHeader.mEntryCount -= recordsDeleted;
            // XXX write bucket
        }
        NS_ASSERTION(mHeader.mEvictionRank[i] == mBuckets[i].EvictionRank(0),
                     "eviction rank out of sync");
        if (!continueFlag)  break;
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
    PRUint32  tempRank[kBucketsPerTable];
    int       i;
    
    // copy eviction rank array
    for (i = 0; i < kBucketsPerTable; ++i)
        tempRank[i] = mHeader.mEvictionRank[i];
    
    while (1) {
    
        // find bucket with highest eviction rank
        PRUint32    rank  = 0;
        PRUint32    index = 0;
        for (i = 0; i < kBucketsPerTable; ++i) {
            if (rank < tempRank[i]) {
                rank = tempRank[i];
                index = i;
            }
        }
        
        if (rank == 0)  break;  // we've examined all the records
NS_ASSERTION(mHeader.mEvictionRank[index] == mBuckets[index].EvictionRank(0),
             "header eviction rank out of sync");
            
        // visit records in bucket with eviction ranks >= target eviction rank
        PRUint32 recordsDeleted;
        PRInt32  continueResult = mBuckets[index].VisitEachRecord(visitor, rank, &recordsDeleted);
        if (recordsDeleted) {
            // recalc eviction rank
            mHeader.mEvictionRank[index] = mBuckets[index].EvictionRank(0);
            mHeader.mEntryCount -= recordsDeleted;
            // XXX write bucket
        }
        if (continueResult == kStopVisitingRecords)  break;
        
        // find greatest rank less than 'rank'
        tempRank[index] = mBuckets[index].EvictionRank(rank);
    }
    return NS_OK;
}



nsresult
nsDiskCacheMap::OpenBlockFiles()
{
    // create nsILocalFile for block file
    nsCOMPtr<nsILocalFile> blockFile;
    nsresult rv;
    
    for (int i = 0; i < 3; ++i) {
        rv = GetBlockFileForIndex(i, getter_AddRefs(blockFile));
        if (NS_FAILED(rv))  goto error_exit;
    
        PRUint32 blockSize = GetBlockSizeForIndex(i+1); // +1 to match file selectors 1,2,3
        rv = mBlockFile[i].Open(blockFile, blockSize);
        if (NS_FAILED(rv)) goto error_exit;
    }
    return NS_OK;

error_exit:
    (void)CloseBlockFiles(PR_FALSE); // we already have an error to report
    return rv;
}


nsresult
nsDiskCacheMap::CloseBlockFiles(PRBool flush)
{
    nsresult rv, rv2 = NS_OK;
    for (int i=0; i < 3; ++i) {
        rv = mBlockFile[i].Close(flush);
        if (NS_FAILED(rv))  rv2 = rv;   // if one or more errors, report at least one
    }
    return rv2;
}


PRBool
nsDiskCacheMap::CacheFilesExist()
{
    nsCOMPtr<nsILocalFile> blockFile;
    nsresult rv;
    
    for (int i = 0; i < 3; ++i) {
        PRBool exists;
        rv = GetBlockFileForIndex(i, getter_AddRefs(blockFile));
        if (NS_FAILED(rv))  return PR_FALSE;

        rv = blockFile->Exists(&exists);
        if (NS_FAILED(rv) || !exists)  return PR_FALSE;
    }

    return PR_TRUE;
}


nsresult
nsDiskCacheMap::ReadDiskCacheEntry(nsDiskCacheRecord * record, nsDiskCacheEntry ** result)
{
    nsresult            rv         = NS_ERROR_UNEXPECTED;
    nsDiskCacheEntry *  diskEntry  = nsnull;
    PRUint32            metaFile   = record->MetaFile();
    PRFileDesc *        fd         = nsnull;
    *result = nsnull;
    
    if (!record->MetaLocationInitialized())  return NS_ERROR_NOT_AVAILABLE;
    
    if (metaFile == 0) {  // entry/metadata stored in separate file
        // open and read the file
        nsCOMPtr<nsILocalFile> file;
        rv = GetLocalFileForDiskCacheRecord(record, nsDiskCache::kMetaData, getter_AddRefs(file));
        if (NS_FAILED(rv))  return rv;

        PRFileDesc * fd = nsnull;
        rv = file->OpenNSPRFileDesc(PR_RDONLY, 00666, &fd);
        if (NS_FAILED(rv))  return rv;
        
        PRInt32 fileSize = PR_Available(fd);
        if (fileSize < 0) {
            // an error occurred. We could call PR_GetError(), but how would that help?
            rv = NS_ERROR_UNEXPECTED;
            goto exit;
        }

        diskEntry = (nsDiskCacheEntry *) new char[fileSize];
        if (!diskEntry) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto exit;
        }
        
        PRInt32 bytesRead = PR_Read(fd, diskEntry, fileSize);
        if (bytesRead < fileSize) {
            rv = NS_ERROR_UNEXPECTED;
            goto exit;
        }

    } else if (metaFile < 4) {  // XXX magic number: use constant
        // entry/metadata stored in cache block file
        
        // allocate buffer
        PRUint32 blockSize  = GetBlockSizeForIndex(metaFile);
        PRUint32 blockCount = record->MetaBlockCount();
        diskEntry = (nsDiskCacheEntry *) new char[blockSize * blockCount];
        
        // read diskEntry
        rv = mBlockFile[metaFile - 1].ReadBlocks((char *)diskEntry,
                                                 record->MetaStartBlock(),
                                                 blockCount);
        if (NS_FAILED(rv))  goto exit;
    }
    
    diskEntry->Unswap();    // disk to memory
    // pass ownership to caller
    *result = diskEntry;
    diskEntry = nsnull;

exit:
    // XXX auto ptr would be nice
    if (fd) (void) PR_Close(fd);
    delete [] (char *)diskEntry;
    return rv;
}


nsresult
nsDiskCacheMap::WriteDiskCacheEntry(nsDiskCacheBinding *  binding)
{
    nsresult            rv        = NS_OK;
    nsDiskCacheEntry *  diskEntry =  CreateDiskCacheEntry(binding);
    if (!diskEntry)  return NS_ERROR_UNEXPECTED;
    
    PRUint32  size      = diskEntry->Size();
    PRUint32  fileIndex = CalculateFileIndex(size);
    PRUint32  blockSize = BLOCK_SIZE_FOR_INDEX(fileIndex);
    PRUint32  blocks    = blockSize ? ((size - 1) / blockSize) + 1 : 0;

    // Deallocate old storage if necessary    
    if (binding->mRecord.MetaLocationInitialized()) {
        // we have existing storage

        if ((binding->mRecord.MetaFile() == 0) &&
            (fileIndex == 0)) {  // keeping the separate file
            // just decrement total
            // XXX if bindRecord.MetaFileSize == USHRT_MAX, stat the file to see how big it is
            DecrementTotalSize(binding->mRecord.MetaFileSize() * 1024);
            NS_ASSERTION(binding->mRecord.MetaFileGeneration() == binding->mGeneration,
                         "generations out of sync");
        } else {
            rv = DeleteStorage(&binding->mRecord, nsDiskCache::kMetaData);
            if (NS_FAILED(rv))  goto exit;
        }
    }

    binding->mRecord.SetEvictionRank(ULONG_MAX - SecondsFromPRTime(PR_Now()));
        
    if (fileIndex == 0) {
        // Write entry data to separate file
        PRUint32 metaFileSizeK = ((size + 0x03FF) >> 10); // round up to nearest 1k
        nsCOMPtr<nsILocalFile> localFile;
        
        // XXX handle metaFileSizeK > USHRT_MAX
        binding->mRecord.SetMetaFileGeneration(binding->mGeneration);
        binding->mRecord.SetMetaFileSize(metaFileSizeK);
        rv = UpdateRecord(&binding->mRecord);
        if (NS_FAILED(rv))  goto exit;

        rv = GetLocalFileForDiskCacheRecord(&binding->mRecord,
                                            nsDiskCache::kMetaData,
                                            getter_AddRefs(localFile));
        if (NS_FAILED(rv))  goto exit;
        
        // open the file
        PRFileDesc * fd;
        rv = localFile->OpenNSPRFileDesc(PR_RDWR | PR_TRUNCATE | PR_CREATE_FILE, 00666, &fd);
        if (NS_FAILED(rv))  goto exit;  // unable to open or create file

        // write the file
        diskEntry->Swap();
        PRInt32 bytesWritten = PR_Write(fd, diskEntry, size);
        
        PRStatus err = PR_Close(mMapFD);
        if ((bytesWritten != (PRInt32)size) || (err != PR_SUCCESS)) {
            rv = NS_ERROR_UNEXPECTED;
            goto exit;
        }
        // XXX handle metaFileSizeK == USHRT_MAX
        IncrementTotalSize(metaFileSizeK * 1024);
        
    } else {
        // write entry data to disk cache block file
        PRInt32 startBlock = mBlockFile[fileIndex - 1].AllocateBlocks(blocks);
        if (startBlock < 0) {
            rv = NS_ERROR_UNEXPECTED;
            goto exit;
        }
        
        // update binding and cache map record
        binding->mRecord.SetMetaBlocks(fileIndex, startBlock, blocks);
        rv = UpdateRecord(&binding->mRecord);
        if (NS_FAILED(rv))  goto exit;
        // XXX we should probably write out bucket ourselves

        // write data
        diskEntry->Swap();
        rv = mBlockFile[fileIndex - 1].WriteBlocks(diskEntry, startBlock, blocks);
        if (NS_FAILED(rv))  goto exit;
        
        IncrementTotalSize(blocks * GetBlockSizeForIndex(fileIndex));
    }

exit:
    delete [] (char *)diskEntry;
    return rv;
}


nsresult
nsDiskCacheMap::ReadDataCacheBlocks(nsDiskCacheBinding * binding, char * buffer, PRUint32 size)
{
    nsresult  rv;
    PRUint32  fileIndex = binding->mRecord.DataFile();
    PRUint32  blockSize = GetBlockSizeForIndex(fileIndex);
    PRUint32  blockCount = binding->mRecord.DataBlockCount();
    PRUint32  minSize = blockSize * blockCount;
    
    if (size < minSize) {
        NS_WARNING("buffer too small");
        return NS_ERROR_UNEXPECTED;
    }
    
    rv = mBlockFile[fileIndex - 1].ReadBlocks(buffer,
                                              binding->mRecord.DataStartBlock(),
                                              blockCount);
    return rv;
}


nsresult
nsDiskCacheMap::WriteDataCacheBlocks(nsDiskCacheBinding * binding, char * buffer, PRUint32 size)
{
    nsresult  rv;
    
    // determine block file & number of blocks
    PRUint32  fileIndex  = CalculateFileIndex(size);
    PRUint32  blockSize  = BLOCK_SIZE_FOR_INDEX(fileIndex);
    PRUint32  blockCount = 0;
    PRInt32   startBlock = 0;
    
    if (size > 0) {
        blockCount = ((size - 1) / blockSize) + 1;
        startBlock = mBlockFile[fileIndex - 1].AllocateBlocks(blockCount);

        rv = mBlockFile[fileIndex - 1].WriteBlocks(buffer, startBlock, blockCount);
        if (NS_FAILED(rv))  return rv;
        
        IncrementTotalSize(blockCount * blockSize);
    }
    
    
    // update binding and cache map record
    binding->mRecord.SetDataBlocks(fileIndex, startBlock, blockCount);
    rv = UpdateRecord(&binding->mRecord);

    return rv;
}


nsresult
nsDiskCacheMap::DoomRecord(nsDiskCacheRecord * record)
{
    nsresult  rv = DeleteRecord(record);
    // XXX future: add record to doomed record journal

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
nsDiskCacheMap::DeleteStorage(nsDiskCacheRecord * record, PRBool metaData)
{
    nsresult    rv = NS_ERROR_UNEXPECTED;
    PRUint32    fileIndex = metaData ? record->MetaFile() : record->DataFile();
    nsCOMPtr<nsIFile> file;
    
    if (fileIndex == 0) {
        // delete the file
        PRUint32  sizeK = metaData ? record->MetaFileSize() : record->DataFileSize();
        // XXX if sizeK == USHRT_MAX, stat file for actual size

        rv = GetFileForDiskCacheRecord(record, metaData, getter_AddRefs(file));
        if (NS_SUCCEEDED(rv)) {
            rv = file->Remove(PR_FALSE);    // false == non-recursive
        }
        DecrementTotalSize(sizeK * 1024);
        
    } else if (fileIndex < 4) {
        // deallocate blocks
        PRInt32  startBlock = metaData ? record->MetaStartBlock() : record->DataStartBlock();
        PRInt32  blockCount = metaData ? record->MetaBlockCount() : record->DataBlockCount();
        
        rv = mBlockFile[fileIndex - 1].DeallocateBlocks(startBlock, blockCount);
        DecrementTotalSize(blockCount * GetBlockSizeForIndex(fileIndex));
    }
    if (metaData)  record->ClearMetaLocation();
    else           record->ClearDataLocation();
    
    return rv;
}


nsresult
nsDiskCacheMap::DeleteRecordAndStorage(nsDiskCacheRecord * record)
{
    nsresult  rv1 = DeleteStorage(record);
    nsresult  rv2 = DeleteRecord(record);
    return NS_FAILED(rv1) ? rv1 : rv2;
}


nsresult
nsDiskCacheMap::GetFileForDiskCacheRecord(nsDiskCacheRecord * record,
                                          PRBool              meta,
                                          nsIFile **          result)
{
    if (!mCacheDirectory)  return NS_ERROR_NOT_AVAILABLE;
    
    nsCOMPtr<nsIFile> file;
    nsresult rv = mCacheDirectory->Clone(getter_AddRefs(file));
    if (NS_FAILED(rv))  return rv;
    
    PRInt16 generation = record->Generation();
    char name[32];
    ::sprintf(name, "%08X%c%02X", record->HashNumber(),  (meta ? 'm' : 'd'), generation);
    rv = file->AppendNative(nsDependentCString(name));
    if (NS_FAILED(rv))  return rv;
    
    NS_IF_ADDREF(*result = file);
    return rv;
}


nsresult
nsDiskCacheMap::GetLocalFileForDiskCacheRecord(nsDiskCacheRecord * record,
                                               PRBool              meta,
                                               nsILocalFile **     result)
{
    nsCOMPtr<nsIFile> file;
    nsresult rv = GetFileForDiskCacheRecord(record, meta, getter_AddRefs(file));
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
    if      (size <=  1024)  return 1;
    else if (size <=  4096)  return 2;
    else if (size <= 16384)  return 3;
    else                     return 0;  
}


PRUint32
nsDiskCacheMap::GetBlockSizeForIndex(PRUint32 index)
{
    return BLOCK_SIZE_FOR_INDEX(index);

}
