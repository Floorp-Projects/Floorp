/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDiskCache.h"
#include "nsDiskCacheBlockFile.h"
#include "mozilla/FileUtils.h"

using namespace mozilla;

/******************************************************************************
 * nsDiskCacheBlockFile - 
 *****************************************************************************/

/******************************************************************************
 *  Open
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::Open(nsILocalFile * blockFile,
                           PRUint32       blockSize,
                           PRUint32       bitMapSize)
{
    if (bitMapSize % 32)
        return NS_ERROR_INVALID_ARG;

    mBlockSize = blockSize;
    mBitMapWords = bitMapSize / 32;
    PRUint32 bitMapBytes = mBitMapWords * 4;
    
    // open the file - restricted to user, the data could be confidential
    nsresult rv = blockFile->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE, 00600, &mFD);
    if (NS_FAILED(rv))  return rv;  // unable to open or create file
    
    // allocate bit map buffer
    mBitMap = new PRUint32[mBitMapWords];
    if (!mBitMap) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto error_exit;
    }
    
    // check if we just creating the file
    mFileSize = PR_Available(mFD);
    if (mFileSize < 0) {
        // XXX an error occurred. We could call PR_GetError(), but how would that help?
        rv = NS_ERROR_UNEXPECTED;
        goto error_exit;
    }
    if (mFileSize == 0) {
        // initialize bit map and write it
        memset(mBitMap, 0, bitMapBytes);
        if (!Write(0, mBitMap, bitMapBytes))
            goto error_exit;
        
    } else if ((PRUint32)mFileSize < bitMapBytes) {
        rv = NS_ERROR_UNEXPECTED;  // XXX NS_ERROR_CACHE_INVALID;
        goto error_exit;
        
    } else {
        // read the bit map
        const PRInt32 bytesRead = PR_Read(mFD, mBitMap, bitMapBytes);
        if ((bytesRead < 0) || ((PRUint32)bytesRead < bitMapBytes)) {
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
        const PRUint32  estimatedSize = CalcBlockFileSize();
        if ((PRUint32)mFileSize + blockSize < estimatedSize) {
            rv = NS_ERROR_UNEXPECTED;
            goto error_exit;
        }
    }
    return NS_OK;

error_exit:
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
        mFD = nsnull;
    }

     if (mBitMap) {
         delete [] mBitMap;
         mBitMap = nsnull;
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
PRInt32
nsDiskCacheBlockFile::AllocateBlocks(PRInt32 numBlocks)
{
    const int maxPos = 32 - numBlocks;
    const PRUint32 mask = (0x01 << numBlocks) - 1;
    for (unsigned int i = 0; i < mBitMapWords; ++i) {
        PRUint32 mapWord = ~mBitMap[i]; // flip bits so free bits are 1
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
                    return (PRInt32)i * 32 + bit;
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
nsDiskCacheBlockFile::DeallocateBlocks( PRInt32  startBlock, PRInt32  numBlocks)
{
    if (!mFD)  return NS_ERROR_NOT_AVAILABLE;

    if ((startBlock < 0) || ((PRUint32)startBlock > mBitMapWords * 32 - 1) ||
        (numBlocks < 1)  || (numBlocks > 4))
       return NS_ERROR_ILLEGAL_VALUE;
           
    const PRInt32 startWord = startBlock >> 5;      // Divide by 32
    const PRUint32 startBit = startBlock & 31;      // Modulo by 32 
      
    // make sure requested deallocation doesn't span a word boundary
    if (startBit + numBlocks > 32)  return NS_ERROR_UNEXPECTED;
    PRUint32 mask = ((0x01 << numBlocks) - 1) << startBit;
    
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
                                   PRUint32 size,
                                   PRInt32  numBlocks,
                                   PRInt32 * startBlock)
{
    // presume buffer != nsnull and startBlock != nsnull
    NS_ENSURE_TRUE(mFD, NS_ERROR_NOT_AVAILABLE);

    // allocate some blocks in the cache block file
    *startBlock = AllocateBlocks(numBlocks);
    if (*startBlock < 0)
        return NS_ERROR_NOT_AVAILABLE;

    // seek to block position
    PRInt32 blockPos = mBitMapWords * 4 + *startBlock * mBlockSize;
    
    // write the blocks
    return Write(blockPos, buffer, size) ? NS_OK : NS_ERROR_FAILURE;
}


/******************************************************************************
 *  ReadBlocks
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::ReadBlocks( void *    buffer,
                                  PRInt32   startBlock,
                                  PRInt32   numBlocks,
                                  PRInt32 * bytesRead)
{
    // presume buffer != nsnull and bytesRead != bytesRead

    if (!mFD)  return NS_ERROR_NOT_AVAILABLE;
    nsresult rv = VerifyAllocation(startBlock, numBlocks);
    if (NS_FAILED(rv))  return rv;
    
    // seek to block position
    PRInt32 blockPos = mBitMapWords * 4 + startBlock * mBlockSize;
    PRInt32 filePos = PR_Seek(mFD, blockPos, PR_SEEK_SET);
    if (filePos != blockPos)  return NS_ERROR_UNEXPECTED;

    // read the blocks
    PRInt32 bytesToRead = *bytesRead;
    if ((bytesToRead <= 0) || ((PRUint32)bytesToRead > mBlockSize * numBlocks)) {
        bytesToRead = mBlockSize * numBlocks;
    }
    *bytesRead = PR_Read(mFD, buffer, bytesToRead);
    
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
    PRUint32 *bitmap = new PRUint32[mBitMapWords];
    // Copy and swap to network format
    PRUint32 *p = bitmap;
    for (unsigned int i = 0; i < mBitMapWords; ++i, ++p)
      *p = htonl(mBitMap[i]);
#else
    PRUint32 *bitmap = mBitMap;
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
nsDiskCacheBlockFile::VerifyAllocation( PRInt32  startBlock, PRInt32  numBlocks)
{
    if ((startBlock < 0) || ((PRUint32)startBlock > mBitMapWords * 32 - 1) ||
        (numBlocks < 1)  || (numBlocks > 4))
       return NS_ERROR_ILLEGAL_VALUE;
    
    const PRInt32 startWord = startBlock >> 5;      // Divide by 32
    const PRUint32 startBit = startBlock & 31;      // Modulo by 32 
      
    // make sure requested deallocation doesn't span a word boundary
    if (startBit + numBlocks > 32)  return NS_ERROR_ILLEGAL_VALUE;
    PRUint32 mask = ((0x01 << numBlocks) - 1) << startBit;
    
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
PRUint32
nsDiskCacheBlockFile::CalcBlockFileSize()
{
    // search for last byte in mBitMap with allocated bits
    PRUint32  estimatedSize = mBitMapWords * 4;
    PRInt32   i = mBitMapWords;
    while (--i >= 0) {
        if (mBitMap[i]) break;
    }

    if (i >= 0) {
        // binary search to find last allocated bit in byte
        PRUint32 mapWord = mBitMap[i];
        PRUint32 lastBit = 31;
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
nsDiskCacheBlockFile::Write(PRInt32 offset, const void *buf, PRInt32 amount)
{
    /* Grow the file to 4mb right away, then double it until the file grows to 20mb.
       20mb is a magic threshold because OSX stops autodefragging files bigger than that.
       Beyond 20mb grow in 4mb chunks.
     */
    const PRInt32 upTo = offset + amount;
    // Use a conservative definition of 20MB
    const PRInt32 minPreallocate = 4*1024*1024;
    const PRInt32 maxPreallocate = 20*1000*1000;
    if (mFileSize < upTo) {
        // maximal file size
        const PRInt32 maxFileSize = mBitMapWords * 4 * (mBlockSize * 8 + 1);
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
        mFileSize = NS_MIN(mFileSize, maxFileSize);
        //  Appears to cause bug 617123?  Disabled for now.
        //mozilla::fallocate(mFD, mFileSize);
    }
    if (PR_Seek(mFD, offset, PR_SEEK_SET) != offset)
        return false;
    return PR_Write(mFD, buf, amount) == amount;
}
