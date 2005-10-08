/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is nsDiskCacheBlockFile.cpp, released
 * April 12, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan  <gordon@netscape.com>
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

#include "nsDiskCache.h"
#include "nsDiskCacheBlockFile.h"

/******************************************************************************
 * nsDiskCacheBlockFile - 
 *****************************************************************************/


/******************************************************************************
 *  Open
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::Open( nsILocalFile *  blockFile, PRUint32  blockSize)
{
    PRInt32   fileSize;

    mBlockSize = blockSize;
    
    // open the file - restricted to user, the data could be confidential
    nsresult rv = blockFile->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE, 00600, &mFD);
    if (NS_FAILED(rv))  return rv;  // unable to open or create file
    
    // allocate bit map buffer
    mBitMap = new PRUint32[kBitMapWords];
    if (!mBitMap) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto error_exit;
    }
    
    // check if we just creating the file
    fileSize = PR_Available(mFD);
    if (fileSize < 0) {
        // XXX an error occurred. We could call PR_GetError(), but how would that help?
        rv = NS_ERROR_UNEXPECTED;
        goto error_exit;
    }
    if (fileSize == 0) {
        // initialize bit map and write it
        memset(mBitMap, 0, kBitMapBytes);
        PRInt32 bytesWritten = PR_Write(mFD, mBitMap, kBitMapBytes);
        if (bytesWritten < kBitMapBytes) 
            goto error_exit;
        
    } else if (fileSize < kBitMapBytes) {
        rv = NS_ERROR_UNEXPECTED;  // XXX NS_ERROR_CACHE_INVALID;
        goto error_exit;
        
    } else {
        // read the bit map
        const PRInt32 bytesRead = PR_Read(mFD, mBitMap, kBitMapBytes);
        if (bytesRead < kBitMapBytes) {
            rv = NS_ERROR_UNEXPECTED;
            goto error_exit;
        }
        
        // validate block file size
        const PRInt32  estimatedSize = CalcBlockFileSize();
        if (estimatedSize > fileSize) {
            rv = NS_ERROR_UNEXPECTED;
            goto error_exit;
        }
    }
    return NS_OK;

error_exit:
    Close(PR_FALSE);
    return rv;
}


/******************************************************************************
 *  Close
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::Close(PRBool flush)
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
    if (!mFD)  return -1;  // NS_ERROR_NOT_AVAILABLE;
    
    const int maxPos = 32 - numBlocks;
    const PRUint32 mask = (0x01 << numBlocks) - 1;
    for (int i = 0; i < kBitMapWords; ++i) {
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
                    mBitMapDirty = PR_TRUE;
                    return i * 32 + bit;
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

    if ((startBlock < 0) || (startBlock > kBitMapBytes * 8 - 1) ||
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
    mBitMapDirty = PR_TRUE;
    // XXX rv = FlushBitMap();  // coherency vs. performance
    return NS_OK;
}


/******************************************************************************
 *  WriteBlocks
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::WriteBlocks( void *   buffer,
                                   PRInt32  startBlock,
                                   PRInt32  numBlocks)
{
    // presume buffer != nsnull
    if (!mFD)  return NS_ERROR_NOT_AVAILABLE;
    nsresult rv = VerifyAllocation(startBlock, numBlocks);
    if (NS_FAILED(rv))  return rv;
    
    // seek to block position
    PRInt32 blockPos = kBitMapBytes + startBlock * mBlockSize;
    PRInt32 filePos = PR_Seek(mFD, blockPos, PR_SEEK_SET);
    if (filePos != blockPos)  return NS_ERROR_UNEXPECTED;
    
    // write the blocks
    PRInt32 bytesToWrite = numBlocks * mBlockSize;
    PRInt32 bytesWritten = PR_Write(mFD, buffer, bytesToWrite);
    if (bytesWritten < bytesToWrite)  return NS_ERROR_UNEXPECTED;
    
    // write the bit map and flush the file
    // XXX except we would take a severe performance hit
    // XXX rv = FlushBitMap();
    return rv;
}


/******************************************************************************
 *  ReadBlocks
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::ReadBlocks(  void *    buffer,
                                   PRInt32   startBlock,
                                   PRInt32   numBlocks)
{
    // presume buffer != nsnull
    if (!mFD)  return NS_ERROR_NOT_AVAILABLE;
    nsresult rv = VerifyAllocation(startBlock, numBlocks);
    if (NS_FAILED(rv))  return rv;
    
    // seek to block position
    PRInt32 blockPos = kBitMapBytes + startBlock * mBlockSize;
    PRInt32 filePos = PR_Seek(mFD, blockPos, PR_SEEK_SET);
    if (filePos != blockPos)  return NS_ERROR_UNEXPECTED;

    // read the blocks
    PRInt32 bytesToRead = numBlocks * mBlockSize;
    PRInt32 bytesRead = PR_Read(mFD, buffer, bytesToRead);
    if (bytesRead < bytesToRead)  return NS_ERROR_UNEXPECTED;
    
    return rv;
}


/******************************************************************************
 *  FlushBitMap
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::FlushBitMap()
{
    if (!mBitMapDirty)  return NS_OK;

    // seek to bitmap
    PRInt32 filePos = PR_Seek(mFD, 0, PR_SEEK_SET);
    if (filePos != 0)  return NS_ERROR_UNEXPECTED;
    
#if defined(IS_LITTLE_ENDIAN)
    PRUint32 bitmap[kBitMapWords];
    // Copy and swap to network format
    PRUint32 *p = bitmap;
    for (int i = 0; i < kBitMapWords; ++i, ++p)
      *p = htonl(mBitMap[i]);
#else
    PRUint32 *bitmap = mBitMap;
#endif

    // write bitmap
    PRInt32 bytesWritten = PR_Write(mFD, bitmap, kBitMapBytes);
    if (bytesWritten < kBitMapBytes)  return NS_ERROR_UNEXPECTED;

    PRStatus err = PR_Sync(mFD);
    if (err != PR_SUCCESS)  return NS_ERROR_UNEXPECTED;

    mBitMapDirty = PR_FALSE;
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
    if ((startBlock < 0) || (startBlock > kBitMapBytes * 8 - 1) ||
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
PRInt32
nsDiskCacheBlockFile::CalcBlockFileSize()
{
    // search for last byte in mBitMap with allocated bits
    PRInt32  estimatedSize = kBitMapBytes;      
    PRInt32  i = kBitMapWords;
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
