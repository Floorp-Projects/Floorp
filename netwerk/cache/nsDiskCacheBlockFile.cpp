/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCache.h"
#include "nsDiskCache.h"
#include "nsDiskCacheBlockFile.h"
#include "mozilla/FileUtils.h"
#include <algorithm>

using namespace mozilla;

/******************************************************************************
 * nsDiskCacheBlockFile - 
 *****************************************************************************/

/******************************************************************************
 *  Open
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::Open(nsIFile * blockFile,
                           uint32_t  blockSize,
                           uint32_t  bitMapSize,
                           nsDiskCache::CorruptCacheInfo *  corruptInfo)
{
    NS_ENSURE_ARG_POINTER(corruptInfo);
    *corruptInfo = nsDiskCache::kUnexpectedError;

    if (bitMapSize % 32) {
        *corruptInfo = nsDiskCache::kInvalidArgPointer;
        return NS_ERROR_INVALID_ARG;
    }

    mBlockSize = blockSize;
    mBitMapWords = bitMapSize / 32;
    uint32_t bitMapBytes = mBitMapWords * 4;
    
    // open the file - restricted to user, the data could be confidential
    nsresult rv = blockFile->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE, 00600, &mFD);
    if (NS_FAILED(rv)) {
        *corruptInfo = nsDiskCache::kCouldNotCreateBlockFile;
        CACHE_LOG_DEBUG(("CACHE: nsDiskCacheBlockFile::Open "
                         "[this=%p] unable to open or create file: %d",
                         this, rv));
        return rv;  // unable to open or create file
    }
    
    // allocate bit map buffer
    mBitMap = new uint32_t[mBitMapWords];
    
    // check if we just creating the file
    mFileSize = PR_Available(mFD);
    if (mFileSize < 0) {
        // XXX an error occurred. We could call PR_GetError(), but how would that help?
        *corruptInfo = nsDiskCache::kBlockFileSizeError;
        rv = NS_ERROR_UNEXPECTED;
        goto error_exit;
    }
    if (mFileSize == 0) {
        // initialize bit map and write it
        memset(mBitMap, 0, bitMapBytes);
        if (!Write(0, mBitMap, bitMapBytes)) {
            *corruptInfo = nsDiskCache::kBlockFileBitMapWriteError;
            goto error_exit;
        }
        
    } else if ((uint32_t)mFileSize < bitMapBytes) {
        *corruptInfo = nsDiskCache::kBlockFileSizeLessThanBitMap;
        rv = NS_ERROR_UNEXPECTED;  // XXX NS_ERROR_CACHE_INVALID;
        goto error_exit;
        
    } else {
        // read the bit map
        const int32_t bytesRead = PR_Read(mFD, mBitMap, bitMapBytes);
        if ((bytesRead < 0) || ((uint32_t)bytesRead < bitMapBytes)) {
            *corruptInfo = nsDiskCache::kBlockFileBitMapReadError;
            rv = NS_ERROR_UNEXPECTED;
            goto error_exit;
        }
#if defined(IS_LITTLE_ENDIAN)
        // Swap from network format
        for (unsigned int i = 0; i < mBitMapWords; ++i)
            mBitMap[i] = ntohl(mBitMap[i]);
#endif
        // validate block file size
        // Because not whole blocks are written, the size may be a 
        // little bit smaller than used blocks times blocksize,
        // because the last block will generally not be 'whole'.
        const uint32_t  estimatedSize = CalcBlockFileSize();
        if ((uint32_t)mFileSize + blockSize < estimatedSize) {
            *corruptInfo = nsDiskCache::kBlockFileEstimatedSizeError;
            rv = NS_ERROR_UNEXPECTED;
            goto error_exit;
        }
    }
    CACHE_LOG_DEBUG(("CACHE: nsDiskCacheBlockFile::Open [this=%p] succeeded",
                      this));
    return NS_OK;

error_exit:
    CACHE_LOG_DEBUG(("CACHE: nsDiskCacheBlockFile::Open [this=%p] failed with "
                     "error %d", this, rv));
    Close(false);
    return rv;
}


/******************************************************************************
 *  Close
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::Close(bool flush)
{
    nsresult rv = NS_OK;

    if (mFD) {
        if (flush)
            rv  = FlushBitMap();
        PRStatus err = PR_Close(mFD);
        if (NS_SUCCEEDED(rv) && (err != PR_SUCCESS))
            rv = NS_ERROR_UNEXPECTED;
        mFD = nullptr;
    }

     if (mBitMap) {
         delete [] mBitMap;
         mBitMap = nullptr;
     }
        
    return rv;
}


/******************************************************************************
 *  AllocateBlocks
 *
 *  Allocates 1-4 blocks, using a first fit strategy,
 *  so that no group of blocks spans a quad block boundary.
 *
 *  Returns block number of first block allocated or -1 on failure.
 *
 *****************************************************************************/
int32_t
nsDiskCacheBlockFile::AllocateBlocks(int32_t numBlocks)
{
    const int maxPos = 32 - numBlocks;
    const uint32_t mask = (0x01 << numBlocks) - 1;
    for (unsigned int i = 0; i < mBitMapWords; ++i) {
        uint32_t mapWord = ~mBitMap[i]; // flip bits so free bits are 1
        if (mapWord) {                  // At least one free bit
            // Binary search for first free bit in word
            int bit = 0;
            if ((mapWord & 0x0FFFF) == 0) { bit |= 16; mapWord >>= 16; }
            if ((mapWord & 0x000FF) == 0) { bit |= 8;  mapWord >>= 8;  }
            if ((mapWord & 0x0000F) == 0) { bit |= 4;  mapWord >>= 4;  }
            if ((mapWord & 0x00003) == 0) { bit |= 2;  mapWord >>= 2;  }
            if ((mapWord & 0x00001) == 0) { bit |= 1;  mapWord >>= 1;  }
            // Find first fit for mask
            for (; bit <= maxPos; ++bit) {
                // all bits selected by mask are 1, so free
                if ((mask & mapWord) == mask) {
                    mBitMap[i] |= mask << bit; 
                    mBitMapDirty = true;
                    return (int32_t)i * 32 + bit;
                }
            }
        }
    }
    
    return -1;
}


/******************************************************************************
 *  DeallocateBlocks
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::DeallocateBlocks( int32_t  startBlock, int32_t  numBlocks)
{
    if (!mFD)  return NS_ERROR_NOT_AVAILABLE;

    if ((startBlock < 0) || ((uint32_t)startBlock > mBitMapWords * 32 - 1) ||
        (numBlocks < 1)  || (numBlocks > 4))
       return NS_ERROR_ILLEGAL_VALUE;
           
    const int32_t startWord = startBlock >> 5;      // Divide by 32
    const uint32_t startBit = startBlock & 31;      // Modulo by 32 
      
    // make sure requested deallocation doesn't span a word boundary
    if (startBit + numBlocks > 32)  return NS_ERROR_UNEXPECTED;
    uint32_t mask = ((0x01 << numBlocks) - 1) << startBit;
    
    // make sure requested deallocation is currently allocated
    if ((mBitMap[startWord] & mask) != mask)    return NS_ERROR_ABORT;

    mBitMap[startWord] ^= mask;    // flips the bits off;
    mBitMapDirty = true;
    // XXX rv = FlushBitMap();  // coherency vs. performance
    return NS_OK;
}


/******************************************************************************
 *  WriteBlocks
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::WriteBlocks( void *   buffer,
                                   uint32_t size,
                                   int32_t  numBlocks,
                                   int32_t * startBlock)
{
    // presume buffer != nullptr and startBlock != nullptr
    NS_ENSURE_TRUE(mFD, NS_ERROR_NOT_AVAILABLE);

    // allocate some blocks in the cache block file
    *startBlock = AllocateBlocks(numBlocks);
    if (*startBlock < 0)
        return NS_ERROR_NOT_AVAILABLE;

    // seek to block position
    int32_t blockPos = mBitMapWords * 4 + *startBlock * mBlockSize;
    
    // write the blocks
    return Write(blockPos, buffer, size) ? NS_OK : NS_ERROR_FAILURE;
}


/******************************************************************************
 *  ReadBlocks
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::ReadBlocks( void *    buffer,
                                  int32_t   startBlock,
                                  int32_t   numBlocks,
                                  int32_t * bytesRead)
{
    // presume buffer != nullptr and bytesRead != bytesRead

    if (!mFD)  return NS_ERROR_NOT_AVAILABLE;
    nsresult rv = VerifyAllocation(startBlock, numBlocks);
    if (NS_FAILED(rv))  return rv;
    
    // seek to block position
    int32_t blockPos = mBitMapWords * 4 + startBlock * mBlockSize;
    int32_t filePos = PR_Seek(mFD, blockPos, PR_SEEK_SET);
    if (filePos != blockPos)  return NS_ERROR_UNEXPECTED;

    // read the blocks
    int32_t bytesToRead = *bytesRead;
    if ((bytesToRead <= 0) || ((uint32_t)bytesToRead > mBlockSize * numBlocks)) {
        bytesToRead = mBlockSize * numBlocks;
    }
    *bytesRead = PR_Read(mFD, buffer, bytesToRead);
    
    CACHE_LOG_DEBUG(("CACHE: nsDiskCacheBlockFile::Read [this=%p] "
                     "returned %d / %d bytes", this, *bytesRead, bytesToRead));

    return NS_OK;
}


/******************************************************************************
 *  FlushBitMap
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::FlushBitMap()
{
    if (!mBitMapDirty)  return NS_OK;
    
#if defined(IS_LITTLE_ENDIAN)
    uint32_t *bitmap = new uint32_t[mBitMapWords];
    // Copy and swap to network format
    uint32_t *p = bitmap;
    for (unsigned int i = 0; i < mBitMapWords; ++i, ++p)
      *p = htonl(mBitMap[i]);
#else
    uint32_t *bitmap = mBitMap;
#endif

    // write bitmap
    bool written = Write(0, bitmap, mBitMapWords * 4);
#if defined(IS_LITTLE_ENDIAN)
    delete [] bitmap;
#endif
    if (!written)
        return NS_ERROR_UNEXPECTED;

    PRStatus err = PR_Sync(mFD);
    if (err != PR_SUCCESS)  return NS_ERROR_UNEXPECTED;

    mBitMapDirty = false;
    return NS_OK;
}


/******************************************************************************
 *  VerifyAllocation
 *
 *  Return values:
 *      NS_OK if all bits are marked allocated
 *      NS_ERROR_ILLEGAL_VALUE if parameters don't obey constraints
 *      NS_ERROR_FAILURE if some or all the bits are marked unallocated
 *
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::VerifyAllocation( int32_t  startBlock, int32_t  numBlocks)
{
    if ((startBlock < 0) || ((uint32_t)startBlock > mBitMapWords * 32 - 1) ||
        (numBlocks < 1)  || (numBlocks > 4))
       return NS_ERROR_ILLEGAL_VALUE;
    
    const int32_t startWord = startBlock >> 5;      // Divide by 32
    const uint32_t startBit = startBlock & 31;      // Modulo by 32 
      
    // make sure requested deallocation doesn't span a word boundary
    if (startBit + numBlocks > 32)  return NS_ERROR_ILLEGAL_VALUE;
    uint32_t mask = ((0x01 << numBlocks) - 1) << startBit;
    
    // check if all specified blocks are currently allocated
    if ((mBitMap[startWord] & mask) != mask)    return NS_ERROR_FAILURE;
    
    return NS_OK;
}


/******************************************************************************
 *  CalcBlockFileSize
 *
 *  Return size of the block file according to the bits set in mBitmap
 *
 *****************************************************************************/
uint32_t
nsDiskCacheBlockFile::CalcBlockFileSize()
{
    // search for last byte in mBitMap with allocated bits
    uint32_t  estimatedSize = mBitMapWords * 4;
    int32_t   i = mBitMapWords;
    while (--i >= 0) {
        if (mBitMap[i]) break;
    }

    if (i >= 0) {
        // binary search to find last allocated bit in byte
        uint32_t mapWord = mBitMap[i];
        uint32_t lastBit = 31;
        if ((mapWord & 0xFFFF0000) == 0) { lastBit ^= 16; mapWord <<= 16; }
        if ((mapWord & 0xFF000000) == 0) { lastBit ^= 8; mapWord <<= 8; }
        if ((mapWord & 0xF0000000) == 0) { lastBit ^= 4; mapWord <<= 4; }
        if ((mapWord & 0xC0000000) == 0) { lastBit ^= 2; mapWord <<= 2; }
        if ((mapWord & 0x80000000) == 0) { lastBit ^= 1; mapWord <<= 1; }
        estimatedSize +=  (i * 32 + lastBit + 1) * mBlockSize;
    }

    return estimatedSize;
}

/******************************************************************************
 *  Write
 *
 *  Wrapper around PR_Write that grows file in larger chunks to combat fragmentation
 *
 *****************************************************************************/
bool
nsDiskCacheBlockFile::Write(int32_t offset, const void *buf, int32_t amount)
{
    /* Grow the file to 4mb right away, then double it until the file grows to 20mb.
       20mb is a magic threshold because OSX stops autodefragging files bigger than that.
       Beyond 20mb grow in 4mb chunks.
     */
    const int32_t upTo = offset + amount;
    // Use a conservative definition of 20MB
    const int32_t minPreallocate = 4*1024*1024;
    const int32_t maxPreallocate = 20*1000*1000;
    if (mFileSize < upTo) {
        // maximal file size
        const int32_t maxFileSize = mBitMapWords * 4 * (mBlockSize * 8 + 1);
        if (upTo > maxPreallocate) {
            // grow the file as a multiple of minPreallocate
            mFileSize = ((upTo + minPreallocate - 1) / minPreallocate) * minPreallocate;
        } else {
            // Grow quickly between 1MB to 20MB
            if (mFileSize)
                while(mFileSize < upTo)
                    mFileSize *= 2;
            mFileSize = clamped(mFileSize, minPreallocate, maxPreallocate);
        }
        mFileSize = std::min(mFileSize, maxFileSize);
#if !defined(XP_MACOSX)
        mozilla::fallocate(mFD, mFileSize);
#endif
    }
    if (PR_Seek(mFD, offset, PR_SEEK_SET) != offset)
        return false;
    return PR_Write(mFD, buf, amount) == amount;
}

size_t
nsDiskCacheBlockFile::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf)
{
    return aMallocSizeOf(mBitMap) + aMallocSizeOf(mFD);
}
