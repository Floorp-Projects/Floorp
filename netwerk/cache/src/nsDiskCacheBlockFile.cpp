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

#include "nsCRT.h"
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
    
    // open the file
    nsresult rv = blockFile->OpenNSPRFileDesc(PR_RDWR | PR_CREATE_FILE, 00666, &mFD);
    if (NS_FAILED(rv))  return rv;  // unable to open or create file
    
    // allocate bit map buffer
    mBitMap = new PRUint8[kBitMapBytes];
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
    mEndOfFile = fileSize;
    if (mEndOfFile == 0) {
        // initialize bit map and write it
        memset(mBitMap, 0, kBitMapBytes);
        PRInt32 bytesWritten = PR_Write(mFD, mBitMap, kBitMapBytes);
        if (bytesWritten < kBitMapBytes) goto error_exit;
        mEndOfFile = kBitMapBytes;
        
    } else if (mEndOfFile < kBitMapBytes) {
        rv = NS_ERROR_UNEXPECTED;  // XXX NS_ERROR_CACHE_INVALID;
        goto error_exit;
        
    } else {
        // read the bit map
        PRInt32 bytesRead = PR_Read(mFD, mBitMap, kBitMapBytes);
        if (bytesRead < kBitMapBytes) {
            rv = NS_ERROR_UNEXPECTED;
            goto error_exit;
        }
        
        // validate block file
        rv = ValidateFile();
        if (NS_FAILED(rv)) goto error_exit;
    }
    
    return NS_OK;

error_exit:
    if (mFD) {
        (void) PR_Close(mFD);
        mFD = nsnull;
    }
    
    if (mBitMap) {
        delete [] mBitMap;
        mBitMap = nsnull;
    }
    return rv;
}


/******************************************************************************
 *  Close
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::Close(PRBool flush)
{
    if (!mFD)  return NS_OK;
    nsresult rv = NS_OK;

    if (flush)
        rv  = FlushBitMap();

    PRStatus err = PR_Close(mFD);
    mFD = nsnull;
    
    if (mBitMap) {
        delete [] mBitMap;
        mBitMap = nsnull;
    }
    
    if (NS_SUCCEEDED(rv) && (err != PR_SUCCESS))
        rv = NS_ERROR_UNEXPECTED;
        
    return rv;
}


/******************************************************************************
 *  Trim
 *
 *  Truncate the block file to the end of the last allocated block.
 *
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::Trim()
{
    PRInt32  estimatedSize = kBitMapBytes;
    PRInt32  lastBlock = LastBlock();
    if (lastBlock >= 0)
        estimatedSize += (lastBlock + 1) * mBlockSize;
            
    nsresult rv = nsDiskCache::Truncate(mFD, estimatedSize);
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
    // return -1 if unable to allocate blocks
    // PRUint8  mask = (0x01 << numBlocks) - 1;    
    int     i = 0;
    PRUint8 mapByte;
    PRUint8 mask;
    
    // presume allocation will succeed
    PRBool oldValue = mBitMapDirty;
    mBitMapDirty = PR_TRUE;
    
    while ((mBitMap[i] == 0xFF) && (i < kBitMapBytes)) ++i;     // find first block with a free bit
    
    if (numBlocks == 1) {
        if (i < kBitMapBytes) {
            // don't need a while loop, because we know there's at least 1 free bit in this byte
            mapByte = ~mBitMap[i]; // flip bits so free bits are 1
/*
 *          // Linear search for first free bit in byte
 *          mask = 0x01;
 *          for (int j=0; j<8; ++j, mask <<= 1)
 *              if (mask & mapByte) {mBitMap[i] |= mask; return (i * 8 + j); }
 */
            // Binary search for first free bit in byte
            PRUint8 bit = 0;
            if ((mapByte & 0x0F) == 0) { bit |= 4; mapByte >>= 4; }
            if ((mapByte & 0x03) == 0) { bit |= 2; mapByte >>= 2; }
            if ((mapByte & 0x01) == 0) { bit |= 1; mapByte >>= 1; }
            mBitMap[i] |= (PRUint8)1 << bit;
            return i * 8 + bit;
        }
    } else if (numBlocks == 2) {
        while (i < kBitMapBytes) {
            mapByte = ~mBitMap[i]; // flip bits so free bits are 1
            mask = 0x03;
            // check for fit in lower quad bits
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8); }       mask <<= 1;
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8 + 1); }   mask <<= 1;
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8 + 2); }   mask <<= 2;
            // check for fit in upper quad bits
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8 + 4); }   mask <<= 1;
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8 + 5); }   mask <<= 1;
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8 + 6); }
            ++i;
        }
    } else if (numBlocks == 3) {
        while (i < kBitMapBytes) {
            mapByte = ~mBitMap[i]; // flip bits so free bits are 1
            mask = 0x07;
            // check for fit in lower quad bits
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8); }       mask <<= 1;
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8 + 1); }   mask <<= 3;
            // check for fit in upper quad bits
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8 + 4); }   mask <<= 1;
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8 + 5); }
            ++i;
        }
    } else if (numBlocks == 4) {
        while (i < kBitMapBytes) {
            mapByte = ~mBitMap[i]; // flip bits so free bits are 1
            mask = 0x0F;
            // check for fit in lower quad bits
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8); }      mask <<= 4;
            // check for fit in upper quad bits
            if ((mask & mapByte) == mask) { mBitMap[i] |= mask; return (i * 8 + 4); }
            ++i;
        }
    }
    
    mBitMapDirty = oldValue;
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
           
    PRInt32 startByte = startBlock / 8;
    PRUint8 startBit  = startBlock % 8;
    
    // make sure requested deallocation doesn't span a byte boundary
    if ((startBlock + numBlocks - 1) / 8 != startByte)  return NS_ERROR_UNEXPECTED;
    PRUint8 mask = ((0x01 << numBlocks) - 1) << startBit;
    
    PRUint8 mapByte = ~mBitMap[startByte]; // flip so allocated bits are zero
    
    // make sure requested deallocation is currently allocated
    if (mapByte & mask)  return NS_ERROR_ABORT;
    
    mBitMap[startByte] ^= mask;    // flips the bits off;
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
    
    if (mEndOfFile < (blockPos + numBlocks * mBlockSize))
        mEndOfFile = (blockPos + numBlocks * mBlockSize);
    
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
    
    // write bitmap
    PRInt32 bytesWritten = PR_Write(mFD, mBitMap, kBitMapBytes);
    if (bytesWritten < kBitMapBytes)  return NS_ERROR_UNEXPECTED;

    PRStatus err = PR_Sync(mFD);
    if (err != PR_SUCCESS)  return NS_ERROR_UNEXPECTED;

    mBitMapDirty = PR_FALSE;
    return NS_OK;
}


/******************************************************************************
 *  ValidateFile
 *
 *  Check size of file against last bit allocated for mBlockSize.
 *
 *****************************************************************************/
nsresult
nsDiskCacheBlockFile::ValidateFile()
{
    PRInt32  estimatedSize = kBitMapBytes;
    PRInt32  lastBlock = LastBlock();
    if (lastBlock >= 0)
        estimatedSize += (lastBlock + 1) * mBlockSize;
        
    // seek to beginning
    PRInt32 filePos = PR_Seek(mFD, 0, PR_SEEK_SET);
    if (filePos != 0)  return NS_ERROR_UNEXPECTED;
    
    PRInt32 fileSize = PR_Available(mFD);

    if (estimatedSize > fileSize)
        return NS_ERROR_UNEXPECTED;

    return NS_OK;
}


/******************************************************************************
 *  VerfiyAllocation
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
    
    PRInt32 startByte = startBlock / 8;
    PRUint8 startBit  = startBlock % 8;
    
    // make sure requested deallocation doesn't span a byte boundary
    if ((startBlock + numBlocks - 1) / 8 != startByte)  return NS_ERROR_ILLEGAL_VALUE;
    PRUint8 mask = ((0x01 << numBlocks) - 1) << startBit;
    
    // check if all specified blocks are currently allocated
    if ((mBitMap[startByte] & mask) != mask)    return NS_ERROR_FAILURE;
    
    return NS_OK;
}


/******************************************************************************
 *  LastBlock
 *
 *  Return last block allocated or -1 if no blocks are allocated.
 *
 *****************************************************************************/
PRInt32
nsDiskCacheBlockFile::LastBlock()
{
    // search for last byte in mBitMap with allocated bits
    PRInt32 i = kBitMapBytes;
    while (--i >= 0) {
        if (mBitMap[i]) break;
    }

    if (i >= 0) {
        // binary search to find last allocated bit in byte
        PRUint8 mapByte = mBitMap[i];
        PRUint8 lastBit = 7;
        if ((mapByte & 0xF0) == 0) { lastBit ^= 4; mapByte <<= 4; }
        if ((mapByte & 0xC0) == 0) { lastBit ^= 2; mapByte <<= 2; }
        if ((mapByte & 0x80) == 0) { lastBit ^= 1; mapByte <<= 1; }
        return i * 8 + lastBit;
    }
    
    return -1;

}
