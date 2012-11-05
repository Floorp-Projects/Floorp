/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsDiskCacheBlockFile_h_
#define _nsDiskCacheBlockFile_h_

#include "nsIFile.h"
#include "nsDiskCache.h"

/******************************************************************************
 *  nsDiskCacheBlockFile
 *
 *  The structure of a cache block file is a 4096 bytes bit map, followed by
 *  some number of blocks of mBlockSize.  The creator of a
 *  nsDiskCacheBlockFile object must provide the block size for a given file.
 *
 *****************************************************************************/
class nsDiskCacheBlockFile {
public:
    nsDiskCacheBlockFile()
           : mFD(nullptr)
           , mBitMap(nullptr)
           , mBlockSize(0)
           , mBitMapWords(0)
           , mFileSize(0)
           , mBitMapDirty(false)
            {}
    ~nsDiskCacheBlockFile() { (void) Close(true); }
    
    nsresult  Open( nsIFile *  blockFile, uint32_t  blockSize,
                    uint32_t  bitMapSize, nsDiskCache::CorruptCacheInfo *  corruptInfo);
    nsresult  Close(bool flush);

    /*
     * Trim
     * Truncates the block file to the end of the last allocated block.
     */
    nsresult  Trim() { return nsDiskCache::Truncate(mFD, CalcBlockFileSize()); }
    nsresult  DeallocateBlocks( int32_t  startBlock, int32_t  numBlocks);
    nsresult  WriteBlocks( void * buffer, uint32_t size, int32_t  numBlocks, 
                           int32_t * startBlock);
    nsresult  ReadBlocks( void * buffer, int32_t  startBlock, int32_t  numBlocks, 
                          int32_t * bytesRead);

    size_t   SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf);

private:
    nsresult  FlushBitMap();
    int32_t   AllocateBlocks( int32_t  numBlocks);
    nsresult  VerifyAllocation( int32_t startBlock, int32_t numBLocks);
    uint32_t  CalcBlockFileSize();
    bool   Write(int32_t offset, const void *buf, int32_t amount);

/**
 *  Data members
 */
    PRFileDesc *                mFD;
    uint32_t *                  mBitMap;      // XXX future: array of bit map blocks
    uint32_t                    mBlockSize;
    uint32_t                    mBitMapWords;
    int32_t                     mFileSize;
    bool                        mBitMapDirty;
};

#endif // _nsDiskCacheBlockFile_h_
