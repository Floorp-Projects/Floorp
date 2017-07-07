/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 cin et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsDiskCacheMap_h_
#define _nsDiskCacheMap_h_

#include "mozilla/MemoryReporting.h"
#include <limits.h>

#include "prnetdb.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIFile.h"
#include "nsITimer.h"

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

// The minimum amount of milliseconds to wait before re-attempting to
// revalidate the cache.
#define kRevalidateCacheTimeout 3000
#define kRevalidateCacheTimeoutTolerance 10
#define kRevalidateCacheErrorTimeout 1000

class nsDiskCacheRecord {

private:
    uint32_t    mHashNumber;
    uint32_t    mEvictionRank;
    uint32_t    mDataLocation;
    uint32_t    mMetaLocation;

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
    uint32_t  HashNumber() const                  { return mHashNumber; }
    void      SetHashNumber( uint32_t hashNumber) { mHashNumber = hashNumber; }

    // EvictionRank accessors
    uint32_t  EvictionRank() const              { return mEvictionRank; }
    void      SetEvictionRank( uint32_t rank)   { mEvictionRank = rank ? rank : 1; }

    // DataLocation accessors
    bool      DataLocationInitialized() const { return 0 != (mDataLocation & eLocationInitializedMask); }
    void      ClearDataLocation()       { mDataLocation = 0; }

    uint32_t  DataFile() const
    {
        return (uint32_t)(mDataLocation & eLocationSelectorMask) >> eLocationSelectorOffset;
    }

    void      SetDataBlocks( uint32_t index, uint32_t startBlock, uint32_t blockCount)
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

    uint32_t   DataBlockCount() const
    {
        return (uint32_t)((mDataLocation & eExtraBlocksMask) >> eExtraBlocksOffset) + 1;
    }

    uint32_t   DataStartBlock() const
    {
        return (mDataLocation & eBlockNumberMask);
    }

    uint32_t   DataBlockSize() const
    {
        return BLOCK_SIZE_FOR_INDEX(DataFile());
    }

    uint32_t   DataFileSize() const  { return (mDataLocation & eFileSizeMask) >> eFileSizeOffset; }
    void       SetDataFileSize(uint32_t  size)
    {
        NS_ASSERTION((mDataLocation & eFileReservedMask) == 0, "bad location");
        mDataLocation &= ~eFileSizeMask;    // clear eFileSizeMask
        mDataLocation |= (size << eFileSizeOffset) & eFileSizeMask;
    }

    uint8_t   DataFileGeneration() const
    {
        return (mDataLocation & eFileGenerationMask);
    }

    void       SetDataFileGeneration( uint8_t generation)
    {
        // clear everything, (separate file index = 0)
        mDataLocation = 0;
        mDataLocation |= generation & eFileGenerationMask;
        mDataLocation |= eLocationInitializedMask;
    }

    // MetaLocation accessors
    bool      MetaLocationInitialized() const { return 0 != (mMetaLocation & eLocationInitializedMask); }
    void      ClearMetaLocation()             { mMetaLocation = 0; }
    uint32_t  MetaLocation() const            { return mMetaLocation; }

    uint32_t  MetaFile() const
    {
        return (uint32_t)(mMetaLocation & eLocationSelectorMask) >> eLocationSelectorOffset;
    }

    void      SetMetaBlocks( uint32_t index, uint32_t startBlock, uint32_t blockCount)
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

    uint32_t   MetaBlockCount() const
    {
        return (uint32_t)((mMetaLocation & eExtraBlocksMask) >> eExtraBlocksOffset) + 1;
    }

    uint32_t   MetaStartBlock() const
    {
        return (mMetaLocation & eBlockNumberMask);
    }

    uint32_t   MetaBlockSize() const
    {
        return BLOCK_SIZE_FOR_INDEX(MetaFile());
    }

    uint32_t   MetaFileSize() const  { return (mMetaLocation & eFileSizeMask) >> eFileSizeOffset; }
    void       SetMetaFileSize(uint32_t  size)
    {
        mMetaLocation &= ~eFileSizeMask;    // clear eFileSizeMask
        mMetaLocation |= (size << eFileSizeOffset) & eFileSizeMask;
    }

    uint8_t   MetaFileGeneration() const
    {
        return (mMetaLocation & eFileGenerationMask);
    }

    void       SetMetaFileGeneration( uint8_t generation)
    {
        // clear everything, (separate file index = 0)
        mMetaLocation = 0;
        mMetaLocation |= generation & eFileGenerationMask;
        mMetaLocation |= eLocationInitializedMask;
    }

    uint8_t   Generation() const
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

    virtual int32_t  VisitRecord( nsDiskCacheRecord *  mapRecord) = 0;
};


/******************************************************************************
 *  nsDiskCacheHeader
 *****************************************************************************/

struct nsDiskCacheHeader {
    uint32_t    mVersion;                           // cache version.
    uint32_t    mDataSize;                          // size of cache in units of 1024bytes.
    int32_t     mEntryCount;                        // number of entries stored in cache.
    uint32_t    mIsDirty;                           // dirty flag.
    int32_t     mRecordCount;                       // Number of records
    uint32_t    mEvictionRank[kBuckets];            // Highest EvictionRank of the bucket
    uint32_t    mBucketUsage[kBuckets];             // Number of used entries in the bucket

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

        for (uint32_t i = 0; i < kBuckets ; i++) {
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

        for (uint32_t i = 0; i < kBuckets ; i++) {
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
        mCacheDirectory(nullptr),
        mMapFD(nullptr),
        mCleanFD(nullptr),
        mRecordArray(nullptr),
        mBufferSize(0),
        mBuffer(nullptr),
        mMaxRecordCount(16384), // this default value won't matter
        mIsDirtyCacheFlushed(false),
        mLastInvalidateTime(0)
    { }

    ~nsDiskCacheMap()
    {
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

    void      NotifyCapacityChange(uint32_t capacity);

/**
 *  Record operations
 */
    nsresult AddRecord( nsDiskCacheRecord *  mapRecord, nsDiskCacheRecord * oldRecord);
    nsresult UpdateRecord( nsDiskCacheRecord *  mapRecord);
    nsresult FindRecord( uint32_t  hashNumber, nsDiskCacheRecord *  mapRecord);
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

    nsresult    ReadDataCacheBlocks(nsDiskCacheBinding * binding, char * buffer, uint32_t size);
    nsresult    WriteDataCacheBlocks(nsDiskCacheBinding * binding, char * buffer, uint32_t size);
    nsresult    DeleteStorage( nsDiskCacheRecord * record, bool metaData);

    /**
     *  Statistical Operations
     */
    void     IncrementTotalSize( uint32_t  delta)
             {
                mHeader.mDataSize += delta;
                mHeader.mIsDirty   = true;
             }

    void     DecrementTotalSize( uint32_t  delta)
             {
                NS_ASSERTION(mHeader.mDataSize >= delta, "disk cache size negative?");
                mHeader.mDataSize  = mHeader.mDataSize > delta ? mHeader.mDataSize - delta : 0;
                mHeader.mIsDirty   = true;
             }

    inline void IncrementTotalSize( uint32_t  blocks, uint32_t blockSize)
             {
                // Round up to nearest K
                IncrementTotalSize(((blocks*blockSize) + 0x03FF) >> 10);
             }

    inline void DecrementTotalSize( uint32_t  blocks, uint32_t blockSize)
             {
                // Round up to nearest K
                DecrementTotalSize(((blocks*blockSize) + 0x03FF) >> 10);
             }

    uint32_t TotalSize()   { return mHeader.mDataSize; }

    int32_t  EntryCount()  { return mHeader.mEntryCount; }

    size_t  SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf);


private:

    /**
     *  Private methods
     */
    nsresult    OpenBlockFiles(nsDiskCache::CorruptCacheInfo *  corruptInfo);
    nsresult    CloseBlockFiles(bool flush);
    bool        CacheFilesExist();

    nsresult    CreateCacheSubDirectories();

    uint32_t    CalculateFileIndex(uint32_t size);

    nsresult    GetBlockFileForIndex( uint32_t index, nsIFile ** result);
    uint32_t    GetBlockSizeForIndex( uint32_t index) const {
        return BLOCK_SIZE_FOR_INDEX(index);
    }
    uint32_t    GetBitMapSizeForIndex( uint32_t index) const {
        return BITMAP_SIZE_FOR_INDEX(index);
    }

    // returns the bucket number
    uint32_t GetBucketIndex( uint32_t hashNumber) const {
        return (hashNumber & (kBuckets - 1));
    }

    // Gets the size of the bucket (in number of records)
    uint32_t GetRecordsPerBucket() const {
        return mHeader.mRecordCount / kBuckets;
    }

    // Gets the first record in the bucket
    nsDiskCacheRecord *GetFirstRecordInBucket(uint32_t bucket) const {
        return mRecordArray + bucket * GetRecordsPerBucket();
    }

    uint32_t GetBucketRank(uint32_t bucketIndex, uint32_t targetRank);

    int32_t  VisitEachRecord(uint32_t                    bucketIndex,
                             nsDiskCacheRecordVisitor *  visitor,
                             uint32_t                    evictionRank);

    nsresult GrowRecords();
    nsresult ShrinkRecords();

    nsresult EnsureBuffer(uint32_t bufSize);

    // The returned structure will point to the buffer owned by nsDiskCacheMap,
    // so it must not be deleted by the caller.
    nsDiskCacheEntry *  CreateDiskCacheEntry(nsDiskCacheBinding *  binding,
                                             uint32_t * size);

    // Initializes the _CACHE_CLEAN_ related functionality
    nsresult InitCacheClean(nsIFile *  cacheDirectory,
                            nsDiskCache::CorruptCacheInfo *  corruptInfo);
    // Writes out a value of '0' or '1' in the _CACHE_CLEAN_ file
    nsresult WriteCacheClean(bool clean);
    // Resets the timout for revalidating the cache
    nsresult ResetCacheTimer(int32_t timeout = kRevalidateCacheTimeout);
    // Invalidates the cache, calls WriteCacheClean and ResetCacheTimer
    nsresult InvalidateCache();
    // Determines if the cache is in a safe state
    bool IsCacheInSafeState();
    // Revalidates the cache by writting out the header, records, and finally
    // by calling WriteCacheClean(true).
    nsresult RevalidateCache();
    // Timer which revalidates the cache
    static void RevalidateTimerCallback(nsITimer *aTimer, void *arg);

/**
 *  data members
 */
private:
    nsCOMPtr<nsITimer>      mCleanCacheTimer;
    nsCOMPtr<nsIFile>       mCacheDirectory;
    PRFileDesc *            mMapFD;
    PRFileDesc *            mCleanFD;
    nsDiskCacheRecord *     mRecordArray;
    nsDiskCacheBlockFile    mBlockFile[kNumBlockFiles];
    uint32_t                mBufferSize;
    char *                  mBuffer;
    nsDiskCacheHeader       mHeader;
    int32_t                 mMaxRecordCount;
    bool                    mIsDirtyCacheFlushed;
    PRIntervalTime          mLastInvalidateTime;
};

#endif // _nsDiskCacheMap_h_
