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
 *    Gordon Sheridan  <gordon@netscape.com>
 */

#ifndef _nsDiskCacheMap_h_
#define _nsDiskCacheMap_h_

#include <limits.h>

#include "prtypes.h"
#include "prnetdb.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsILocalFile.h"

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
 *  eFileSizeMask note:  Files larger than 64 MiB have zero size stored in the
 *                       location.  The file itself must be examined to determine
 *                       its actual size.  (XXX This is broken in places -darin)
 *
 *****************************************************************************/

#define BLOCK_SIZE_FOR_INDEX(index)  ((index) ? (256 << (2 * ((index) - 1))) : 0)
#define kSeparateFile      0
#define kMaxDataFileSize   0x4000000   // 64 MiB

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
    
    PRBool  ValidRecord()
    {
        if ((mDataLocation & eReservedMask) || (mMetaLocation & eReservedMask))
            return PR_FALSE;
        return PR_TRUE;
    }
    
    // HashNumber accessors
    PRUint32  HashNumber() const                  { return mHashNumber; }
    void      SetHashNumber( PRUint32 hashNumber) { mHashNumber = hashNumber; }

    // EvictionRank accessors
    PRUint32  EvictionRank() const              { return mEvictionRank; }
    void      SetEvictionRank( PRUint32 rank)   { mEvictionRank = rank ? rank : 1; }

    // DataLocation accessors
    PRBool    DataLocationInitialized() { return mDataLocation & eLocationInitializedMask; }
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
        NS_ASSERTION( index < 4,"invalid location index");
        NS_ASSERTION( index > 0,"invalid location index");
        mDataLocation |= (index << eLocationSelectorOffset) & eLocationSelectorMask;

        // set startBlock
        NS_ASSERTION(startBlock == (startBlock & eBlockNumberMask), "invalid block number");
        mDataLocation |= startBlock & eBlockNumberMask;
        
        // set blockCount
        NS_ASSERTION( (blockCount>=1) && (blockCount<=4),"invalid block count");
        blockCount = --blockCount;
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
    PRBool    MetaLocationInitialized() { return mMetaLocation & eLocationInitializedMask; }
    void      ClearMetaLocation()       { mMetaLocation = 0; }
    
    PRUint32  MetaLocation()                     { return mMetaLocation; }
    
    PRUint32  MetaFile() const
    {
        return (PRUint32)(mMetaLocation & eLocationSelectorMask) >> eLocationSelectorOffset;
    }

    void      SetMetaBlocks( PRUint32 index, PRUint32 startBlock, PRUint32 blockCount)
    {
        // clear everything
        mMetaLocation = 0;
        
        // set file index
        NS_ASSERTION( index < 4, "invalid location index");
        NS_ASSERTION( index > 0, "invalid location index");
        mMetaLocation |= (index << eLocationSelectorOffset) & eLocationSelectorMask;

        // set startBlock
        NS_ASSERTION(startBlock == (startBlock & eBlockNumberMask), "invalid block number");
        mMetaLocation |= startBlock & eBlockNumberMask;
        
        // set blockCount
        NS_ASSERTION( (blockCount>=1) && (blockCount<=4),"invalid block count");
        blockCount = --blockCount;
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



    void        Swap()
    {
#if defined(IS_LITTLE_ENDIAN)
        mHashNumber   = ::PR_htonl(mHashNumber);
        mEvictionRank = ::PR_htonl(mEvictionRank);
        mDataLocation = ::PR_htonl(mDataLocation);
        mMetaLocation = ::PR_htonl(mMetaLocation);
#endif
    }
    
    void        Unswap()
    {
#if defined(IS_LITTLE_ENDIAN)
        mHashNumber   = ::PR_ntohl(mHashNumber);
        mEvictionRank = ::PR_ntohl(mEvictionRank);
        mDataLocation = ::PR_ntohl(mDataLocation);
        mMetaLocation = ::PR_ntohl(mMetaLocation);
#endif
    }

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
 *  nsDiskCacheBucket
 *****************************************************************************/
enum {
    kRecordsPerBucket = 256,
    kBucketsPerTable = (1 << 5)         // must be a power of 2!
};

struct nsDiskCacheBucket {
    nsDiskCacheRecord   mRecords[kRecordsPerBucket];
    
    void      Swap();
    void      Unswap();
    PRInt32   CountRecords();
    PRUint32  EvictionRank(PRUint32  targetRank);   // return largest rank in bucket < targetRank
    PRInt32   VisitEachRecord( nsDiskCacheRecordVisitor *  visitor,
                               PRUint32                    evictionRank,
                               PRUint32 *                  recordsDeleted);
};


/******************************************************************************
 *  nsDiskCacheHeader
 *****************************************************************************/

struct nsDiskCacheHeader {
    PRUint32    mVersion;                           // cache version.
    PRInt32     mDataSize;                          // size of cache in bytes.
    PRInt32     mEntryCount;                        // number of entries stored in cache.
    PRUint32    mIsDirty;                           // dirty flag.
    PRUint32    mEvictionRank[kBucketsPerTable];
    
    // pad to blocksize
    enum { kReservedBytes = sizeof(nsDiskCacheBucket)
                            - sizeof(PRUint32) * 4      // version, size, count, dirty
                            - sizeof(PRUint32) * kBucketsPerTable   // eviction array
    };
                            
    PRUint8     reserved[kReservedBytes];

    // XXX need a bitmap?
    
    nsDiskCacheHeader()
        : mVersion(nsDiskCache::kCurrentVersion)
        , mDataSize(0)
        , mEntryCount(0)
        , mIsDirty(PR_TRUE)
    {}

    void        Swap()
    {
#if defined(IS_LITTLE_ENDIAN)
        mVersion     = ::PR_htonl(mVersion);
        mDataSize    = ::PR_htonl(mDataSize);
        mEntryCount  = ::PR_htonl(mEntryCount);
        mIsDirty     = ::PR_htonl(mIsDirty);
#endif
    }
    
    void        Unswap()
    {
#if defined(IS_LITTLE_ENDIAN)
        mVersion     = ::PR_ntohl(mVersion);
        mDataSize    = ::PR_ntohl(mDataSize);
        mEntryCount  = ::PR_ntohl(mEntryCount);
        mIsDirty     = ::PR_ntohl(mIsDirty);
#endif
    }
};


/******************************************************************************
 *  nsDiskCacheMap
 *****************************************************************************/

// XXX fixed capacity for 8192 entries. Future: make dynamic

enum {
    kCacheMapSize = sizeof(nsDiskCacheHeader) +
                    kBucketsPerTable * sizeof(nsDiskCacheBucket)
};


class nsDiskCacheMap {
public:

     nsDiskCacheMap()
        : mCacheDirectory(nsnull)
        , mMapFD(nsnull)
        {
            NS_ASSERTION(sizeof(nsDiskCacheHeader) == sizeof(nsDiskCacheBucket), "structure misalignment");
        }
    ~nsDiskCacheMap()                   { (void) Close(PR_TRUE); }

/**
 *  File Operations
 *
 *  Open
 *
 *  Creates a new cache map file if one doesn't exist.
 *  Returns error if it detects change in format or cache wasn't closed.
 */
    nsresult  Open( nsILocalFile *  cacheDirectory);
    nsresult  Close(PRBool flush);
    nsresult  Trim();

//  nsresult  Flush();
    nsresult  FlushHeader();
    nsresult  FlushBuckets( PRBool unswap);

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
    nsresult    DoomRecord( nsDiskCacheRecord *  record);
    nsresult    DeleteStorage( nsDiskCacheRecord *  record);
    nsresult    DeleteRecordAndStorage( nsDiskCacheRecord *  record);

    nsresult    GetFileForDiskCacheRecord( nsDiskCacheRecord * record,
                                           PRBool              meta,
                                           nsIFile **          result);
                                          
    nsresult    GetLocalFileForDiskCacheRecord( nsDiskCacheRecord *  record,
                                                PRBool               meta,
                                                nsILocalFile **      result);

    nsresult    ReadDiskCacheEntry( nsDiskCacheRecord *  record,
                                    nsDiskCacheEntry **  result);

    nsresult    WriteDiskCacheEntry( nsDiskCacheBinding *  binding);
    
    nsresult    ReadDataCacheBlocks(nsDiskCacheBinding * binding, char * buffer, PRUint32 size);
    nsresult    WriteDataCacheBlocks(nsDiskCacheBinding * binding, char * buffer, PRUint32 size);
    nsresult    DeleteStorage( nsDiskCacheRecord * record, PRBool metaData);
    
    /**
     *  Statistical Operations
     */
    void     IncrementTotalSize( PRInt32  delta)
             {
                NS_ASSERTION(mHeader.mDataSize >= 0, "disk cache size negative?");
                mHeader.mDataSize += delta;
                mHeader.mIsDirty   = PR_TRUE;
             }
             
    void     DecrementTotalSize( PRInt32  delta)
             {
                mHeader.mDataSize -= delta;
                mHeader.mIsDirty   = PR_TRUE;
                NS_ASSERTION(mHeader.mDataSize >= 0, "disk cache size negative?");
             }
    
    PRInt32  TotalSize()   { return mHeader.mDataSize; }
    
    PRInt32  EntryCount()  { return mHeader.mEntryCount; }


private:

    /**
     *  Private methods
     */
    nsresult    OpenBlockFiles();
    nsresult    CloseBlockFiles(PRBool flush);
    PRBool      CacheFilesExist();

    PRUint32    CalculateFileIndex(PRUint32 size);

    nsresult    GetBlockFileForIndex( PRUint32 index, nsILocalFile ** result);
    PRUint32    GetBlockSizeForIndex( PRUint32 index);
    
    nsDiskCacheBucket * GetBucketForHashNumber( PRUint32  hashNumber)
    {
        return &mBuckets[GetBucketIndex(hashNumber)];
    }

    PRUint32 GetBucketIndex( PRUint32 hashNumber)
    {
        return (hashNumber & (kBucketsPerTable - 1));
    }
    

   

/**
 *  data members
 */
private:
    nsCOMPtr<nsILocalFile>  mCacheDirectory;
    PRFileDesc *            mMapFD;
    nsDiskCacheBlockFile    mBlockFile[3];
    nsDiskCacheHeader       mHeader;
    nsDiskCacheBucket       mBuckets[kBucketsPerTable];
};

#endif // _nsDiskCacheMap_h_
