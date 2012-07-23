/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 cin et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsDiskCacheMap_h_
#define _nsDiskCacheMap_h_

#include <limits.h>

#include "prtypes.h"
#include "prnetdb.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIFile.h"

#include "nsDiskCache.h"
#include "nsDiskCacheBlockFile.h"
 
 
class nsDiskCacheBinding;
struct nsDiskCacheEntry;

/******************************************************************************
 *  nsDiskCacheRecord
 *
 *   Cache Location Format
 *
 *    1000 0000 0000 0000 0000 0000 0000 0000 : initialized bit
 *
 *    0011 0000 0000 0000 0000 0000 0000 0000 : File Selector (0 = separate file)
 *    0000 0011 0000 0000 0000 0000 0000 0000 : number of extra contiguous blocks 1-4
 *    0100 1100 0000 0000 0000 0000 0000 0000 : reserved bits
 *    0000 0000 1111 1111 1111 1111 1111 1111 : block#  0-16777216 (2^24)
 *
 *    0000 0000 1111 1111 1111 1111 0000 0000 : eFileSizeMask (size of file in k: see note)
 *    0000 0000 0000 0000 0000 0000 1111 1111 : eFileGenerationMask
 *
 *  File Selector:
 *      0 = separate file on disk
 *      1 = 256 byte block file
 *      2 = 1k block file
 *      3 = 4k block file
 *
 *  eFileSizeMask note:  Files larger than 65535 KiB have this limit stored in
 *                       the location.  The file itself must be examined to
 *                       determine its actual size if necessary.
 *
 *****************************************************************************/

/*
  We have 3 block files with roughly the same max size (32MB)
    1 - block size 256B, number of blocks 131072
    2 - block size  1kB, number of blocks  32768
    3 - block size  4kB, number of blocks   8192
*/
#define kNumBlockFiles             3
#define SIZE_SHIFT(idx)            (2 * ((idx) - 1))
#define BLOCK_SIZE_FOR_INDEX(idx)  ((idx) ? (256    << SIZE_SHIFT(idx)) : 0)
#define BITMAP_SIZE_FOR_INDEX(idx) ((idx) ? (131072 >> SIZE_SHIFT(idx)) : 0)

// Min and max values for the number of records in the DiskCachemap
#define kMinRecordCount    512

#define kSeparateFile      0
#define kBuckets           (1 << 5)    // must be a power of 2!

// Maximum size in K which can be stored in the location (see eFileSizeMask).
// Both data and metadata can be larger, but only up to kMaxDataSizeK can be
// counted into total cache size. I.e. if there are entries where either data or
// metadata is larger than kMaxDataSizeK, the total cache size will be
// inaccurate (smaller) than the actual cache size. The alternative is to stat
// the files to find the real size, which was decided against for performance
// reasons. See bug #651100 comment #21.
#define kMaxDataSizeK      0xFFFF

// preallocate up to 1MB of separate cache file
#define kPreallocateLimit  1 * 1024 * 1024

class nsDiskCacheRecord {

private:
    PRUint32    mHashNumber;
    PRUint32    mEvictionRank;
    PRUint32    mDataLocation;
    PRUint32    mMetaLocation;
 
    enum {
        eLocationInitializedMask = 0x80000000,
        
        eLocationSelectorMask    = 0x30000000,
        eLocationSelectorOffset  = 28,
        
        eExtraBlocksMask         = 0x03000000,
        eExtraBlocksOffset       = 24,
        
        eReservedMask            = 0x4C000000,
        
        eBlockNumberMask         = 0x00FFFFFF,

        eFileSizeMask            = 0x00FFFF00,
        eFileSizeOffset          = 8,
        eFileGenerationMask      = 0x000000FF,
        eFileReservedMask        = 0x4F000000
        
    };

public:
    nsDiskCacheRecord()
        :   mHashNumber(0), mEvictionRank(0), mDataLocation(0), mMetaLocation(0)
    {
    }
    
    bool    ValidRecord()
    {
        if ((mDataLocation & eReservedMask) || (mMetaLocation & eReservedMask))
            return false;
        return true;
    }
    
    // HashNumber accessors
    PRUint32  HashNumber() const                  { return mHashNumber; }
    void      SetHashNumber( PRUint32 hashNumber) { mHashNumber = hashNumber; }

    // EvictionRank accessors
    PRUint32  EvictionRank() const              { return mEvictionRank; }
    void      SetEvictionRank( PRUint32 rank)   { mEvictionRank = rank ? rank : 1; }

    // DataLocation accessors
    bool      DataLocationInitialized() const { return 0 != (mDataLocation & eLocationInitializedMask); }
    void      ClearDataLocation()       { mDataLocation = 0; }
    
    PRUint32  DataFile() const
    {
        return (PRUint32)(mDataLocation & eLocationSelectorMask) >> eLocationSelectorOffset;
    }

    void      SetDataBlocks( PRUint32 index, PRUint32 startBlock, PRUint32 blockCount)
    {
        // clear everything
        mDataLocation = 0;
        
        // set file index
        NS_ASSERTION( index < (kNumBlockFiles + 1), "invalid location index");
        NS_ASSERTION( index > 0,"invalid location index");
        mDataLocation |= (index << eLocationSelectorOffset) & eLocationSelectorMask;

        // set startBlock
        NS_ASSERTION(startBlock == (startBlock & eBlockNumberMask), "invalid block number");
        mDataLocation |= startBlock & eBlockNumberMask;
        
        // set blockCount
        NS_ASSERTION( (blockCount>=1) && (blockCount<=4),"invalid block count");
        --blockCount;
        mDataLocation |= (blockCount << eExtraBlocksOffset) & eExtraBlocksMask;
        
        mDataLocation |= eLocationInitializedMask;
    }

    PRUint32   DataBlockCount() const
    {
        return (PRUint32)((mDataLocation & eExtraBlocksMask) >> eExtraBlocksOffset) + 1;
    }

    PRUint32   DataStartBlock() const
    {
        return (mDataLocation & eBlockNumberMask);
    }
    
    PRUint32   DataBlockSize() const
    {
        return BLOCK_SIZE_FOR_INDEX(DataFile());
    }
    
    PRUint32   DataFileSize() const  { return (mDataLocation & eFileSizeMask) >> eFileSizeOffset; }
    void       SetDataFileSize(PRUint32  size)
    {
        NS_ASSERTION((mDataLocation & eFileReservedMask) == 0, "bad location");
        mDataLocation &= ~eFileSizeMask;    // clear eFileSizeMask
        mDataLocation |= (size << eFileSizeOffset) & eFileSizeMask;
    }

    PRUint8   DataFileGeneration() const
    {
        return (mDataLocation & eFileGenerationMask);
    }

    void       SetDataFileGeneration( PRUint8 generation)
    {
        // clear everything, (separate file index = 0)
        mDataLocation = 0;
        mDataLocation |= generation & eFileGenerationMask;
        mDataLocation |= eLocationInitializedMask;
    }

    // MetaLocation accessors
    bool      MetaLocationInitialized() const { return 0 != (mMetaLocation & eLocationInitializedMask); }
    void      ClearMetaLocation()             { mMetaLocation = 0; }   
    PRUint32  MetaLocation() const            { return mMetaLocation; }
    
    PRUint32  MetaFile() const
    {
        return (PRUint32)(mMetaLocation & eLocationSelectorMask) >> eLocationSelectorOffset;
    }

    void      SetMetaBlocks( PRUint32 index, PRUint32 startBlock, PRUint32 blockCount)
    {
        // clear everything
        mMetaLocation = 0;
        
        // set file index
        NS_ASSERTION( index < (kNumBlockFiles + 1), "invalid location index");
        NS_ASSERTION( index > 0, "invalid location index");
        mMetaLocation |= (index << eLocationSelectorOffset) & eLocationSelectorMask;

        // set startBlock
        NS_ASSERTION(startBlock == (startBlock & eBlockNumberMask), "invalid block number");
        mMetaLocation |= startBlock & eBlockNumberMask;
        
        // set blockCount
        NS_ASSERTION( (blockCount>=1) && (blockCount<=4),"invalid block count");
        --blockCount;
        mMetaLocation |= (blockCount << eExtraBlocksOffset) & eExtraBlocksMask;
        
        mMetaLocation |= eLocationInitializedMask;
    }

    PRUint32   MetaBlockCount() const
    {
        return (PRUint32)((mMetaLocation & eExtraBlocksMask) >> eExtraBlocksOffset) + 1;
    }

    PRUint32   MetaStartBlock() const
    {
        return (mMetaLocation & eBlockNumberMask);
    }

    PRUint32   MetaBlockSize() const
    {
        return BLOCK_SIZE_FOR_INDEX(MetaFile());
    }
    
    PRUint32   MetaFileSize() const  { return (mMetaLocation & eFileSizeMask) >> eFileSizeOffset; }
    void       SetMetaFileSize(PRUint32  size)
    {
        mMetaLocation &= ~eFileSizeMask;    // clear eFileSizeMask
        mMetaLocation |= (size << eFileSizeOffset) & eFileSizeMask;
    }

    PRUint8   MetaFileGeneration() const
    {
        return (mMetaLocation & eFileGenerationMask);
    }

    void       SetMetaFileGeneration( PRUint8 generation)
    {
        // clear everything, (separate file index = 0)
        mMetaLocation = 0;
        mMetaLocation |= generation & eFileGenerationMask;
        mMetaLocation |= eLocationInitializedMask;
    }

    PRUint8   Generation() const
    {
        if ((mDataLocation & eLocationInitializedMask)  &&
            (DataFile() == 0))
            return DataFileGeneration();
            
        if ((mMetaLocation & eLocationInitializedMask)  &&
            (MetaFile() == 0))
            return MetaFileGeneration();
        
        return 0;  // no generation
    }

#if defined(IS_LITTLE_ENDIAN)
    void        Swap()
    {
        mHashNumber   = htonl(mHashNumber);
        mEvictionRank = htonl(mEvictionRank);
        mDataLocation = htonl(mDataLocation);
        mMetaLocation = htonl(mMetaLocation);
    }
#endif
    
#if defined(IS_LITTLE_ENDIAN)
    void        Unswap()
    {
        mHashNumber   = ntohl(mHashNumber);
        mEvictionRank = ntohl(mEvictionRank);
        mDataLocation = ntohl(mDataLocation);
        mMetaLocation = ntohl(mMetaLocation);
    }
#endif

};


/******************************************************************************
 *  nsDiskCacheRecordVisitor
 *****************************************************************************/

enum {  kDeleteRecordAndContinue = -1,
        kStopVisitingRecords     =  0,
        kVisitNextRecord         =  1
};

class nsDiskCacheRecordVisitor {
    public:

    virtual PRInt32  VisitRecord( nsDiskCacheRecord *  mapRecord) = 0;
};


/******************************************************************************
 *  nsDiskCacheHeader
 *****************************************************************************/

struct nsDiskCacheHeader {
    PRUint32    mVersion;                           // cache version.
    PRUint32    mDataSize;                          // size of cache in units of 1024bytes.
    PRInt32     mEntryCount;                        // number of entries stored in cache.
    PRUint32    mIsDirty;                           // dirty flag.
    PRInt32     mRecordCount;                       // Number of records
    PRUint32    mEvictionRank[kBuckets];            // Highest EvictionRank of the bucket
    PRUint32    mBucketUsage[kBuckets];             // Number of used entries in the bucket
  
    nsDiskCacheHeader()
        : mVersion(nsDiskCache::kCurrentVersion)
        , mDataSize(0)
        , mEntryCount(0)
        , mIsDirty(true)
        , mRecordCount(0)
    {}

    void        Swap()
    {
#if defined(IS_LITTLE_ENDIAN)
        mVersion     = htonl(mVersion);
        mDataSize    = htonl(mDataSize);
        mEntryCount  = htonl(mEntryCount);
        mIsDirty     = htonl(mIsDirty);
        mRecordCount = htonl(mRecordCount);

        for (PRUint32 i = 0; i < kBuckets ; i++) {
            mEvictionRank[i] = htonl(mEvictionRank[i]);
            mBucketUsage[i]  = htonl(mBucketUsage[i]);
        }
#endif
    }
    
    void        Unswap()
    {
#if defined(IS_LITTLE_ENDIAN)
        mVersion     = ntohl(mVersion);
        mDataSize    = ntohl(mDataSize);
        mEntryCount  = ntohl(mEntryCount);
        mIsDirty     = ntohl(mIsDirty);
        mRecordCount = ntohl(mRecordCount);

        for (PRUint32 i = 0; i < kBuckets ; i++) {
            mEvictionRank[i] = ntohl(mEvictionRank[i]);
            mBucketUsage[i]  = ntohl(mBucketUsage[i]);
        }
#endif
    }
};


/******************************************************************************
 *  nsDiskCacheMap
 *****************************************************************************/

class nsDiskCacheMap {
public:

     nsDiskCacheMap() : 
        mCacheDirectory(nsnull),
        mMapFD(nsnull),
        mRecordArray(nsnull),
        mBufferSize(0),
        mBuffer(nsnull),
        mMaxRecordCount(16384) // this default value won't matter
    { }

    ~nsDiskCacheMap() {
        (void) Close(true);
    }

/**
 *  File Operations
 *
 *  Open
 *
 *  Creates a new cache map file if one doesn't exist.
 *  Returns error if it detects change in format or cache wasn't closed.
 */
    nsresult  Open( nsIFile *  cacheDirectory,
                    nsDiskCache::CorruptCacheInfo *  corruptInfo);
    nsresult  Close(bool flush);
    nsresult  Trim();

    nsresult  FlushHeader();
    nsresult  FlushRecords( bool unswap);

    void      NotifyCapacityChange(PRUint32 capacity);

/**
 *  Record operations
 */
    nsresult AddRecord( nsDiskCacheRecord *  mapRecord, nsDiskCacheRecord * oldRecord);
    nsresult UpdateRecord( nsDiskCacheRecord *  mapRecord);
    nsresult FindRecord( PRUint32  hashNumber, nsDiskCacheRecord *  mapRecord);
    nsresult DeleteRecord( nsDiskCacheRecord *  mapRecord);
    nsresult VisitRecords( nsDiskCacheRecordVisitor * visitor);
    nsresult EvictRecords( nsDiskCacheRecordVisitor * visitor);

/**
 *  Disk Entry operations
 */
    nsresult    DeleteStorage( nsDiskCacheRecord *  record);

    nsresult    GetFileForDiskCacheRecord( nsDiskCacheRecord * record,
                                           bool                meta,
                                           bool                createPath,
                                           nsIFile **          result);
                                          
    nsresult    GetLocalFileForDiskCacheRecord( nsDiskCacheRecord *  record,
                                                bool                 meta,
                                                bool                 createPath,
                                                nsIFile **           result);

    // On success, this returns the buffer owned by nsDiskCacheMap,
    // so it must not be deleted by the caller.
    nsDiskCacheEntry * ReadDiskCacheEntry( nsDiskCacheRecord *  record);

    nsresult    WriteDiskCacheEntry( nsDiskCacheBinding *  binding);
    
    nsresult    ReadDataCacheBlocks(nsDiskCacheBinding * binding, char * buffer, PRUint32 size);
    nsresult    WriteDataCacheBlocks(nsDiskCacheBinding * binding, char * buffer, PRUint32 size);
    nsresult    DeleteStorage( nsDiskCacheRecord * record, bool metaData);
    
    /**
     *  Statistical Operations
     */
    void     IncrementTotalSize( PRUint32  delta)
             {
                mHeader.mDataSize += delta;
                mHeader.mIsDirty   = true;
             }
             
    void     DecrementTotalSize( PRUint32  delta)
             {
                NS_ASSERTION(mHeader.mDataSize >= delta, "disk cache size negative?");
                mHeader.mDataSize  = mHeader.mDataSize > delta ? mHeader.mDataSize - delta : 0;               
                mHeader.mIsDirty   = true;
             }
    
    inline void IncrementTotalSize( PRUint32  blocks, PRUint32 blockSize)
             {
                // Round up to nearest K
                IncrementTotalSize(((blocks*blockSize) + 0x03FF) >> 10);
             }

    inline void DecrementTotalSize( PRUint32  blocks, PRUint32 blockSize)
             {
                // Round up to nearest K
                DecrementTotalSize(((blocks*blockSize) + 0x03FF) >> 10);
             }
                 
    PRUint32 TotalSize()   { return mHeader.mDataSize; }
    
    PRInt32  EntryCount()  { return mHeader.mEntryCount; }


private:

    /**
     *  Private methods
     */
    nsresult    OpenBlockFiles(nsDiskCache::CorruptCacheInfo *  corruptInfo);
    nsresult    CloseBlockFiles(bool flush);
    bool        CacheFilesExist();

    nsresult    CreateCacheSubDirectories();

    PRUint32    CalculateFileIndex(PRUint32 size);

    nsresult    GetBlockFileForIndex( PRUint32 index, nsIFile ** result);
    PRUint32    GetBlockSizeForIndex( PRUint32 index) const {
        return BLOCK_SIZE_FOR_INDEX(index);
    }
    PRUint32    GetBitMapSizeForIndex( PRUint32 index) const {
        return BITMAP_SIZE_FOR_INDEX(index);
    }
    
    // returns the bucket number    
    PRUint32 GetBucketIndex( PRUint32 hashNumber) const {
        return (hashNumber & (kBuckets - 1));
    }
    
    // Gets the size of the bucket (in number of records)
    PRUint32 GetRecordsPerBucket() const {
        return mHeader.mRecordCount / kBuckets;
    }

    // Gets the first record in the bucket
    nsDiskCacheRecord *GetFirstRecordInBucket(PRUint32 bucket) const {
        return mRecordArray + bucket * GetRecordsPerBucket();
    }

    PRUint32 GetBucketRank(PRUint32 bucketIndex, PRUint32 targetRank);

    PRInt32  VisitEachRecord(PRUint32                    bucketIndex,
                             nsDiskCacheRecordVisitor *  visitor,
                             PRUint32                    evictionRank);

    nsresult GrowRecords();
    nsresult ShrinkRecords();

    nsresult EnsureBuffer(PRUint32 bufSize);

    // The returned structure will point to the buffer owned by nsDiskCacheMap, 
    // so it must not be deleted by the caller.
    nsDiskCacheEntry *  CreateDiskCacheEntry(nsDiskCacheBinding *  binding,
                                             PRUint32 * size);

/**
 *  data members
 */
private:
    nsCOMPtr<nsIFile>       mCacheDirectory;
    PRFileDesc *            mMapFD;
    nsDiskCacheRecord *     mRecordArray;
    nsDiskCacheBlockFile    mBlockFile[kNumBlockFiles];
    PRUint32                mBufferSize;
    char *                  mBuffer;
    nsDiskCacheHeader       mHeader;
    PRInt32                 mMaxRecordCount;
};

#endif // _nsDiskCacheMap_h_
