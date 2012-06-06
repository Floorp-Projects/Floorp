/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsDiskCacheBlockFile_h_
#define _nsDiskCacheBlockFile_h_

#include "nsIFile.h"

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
           : mFD(nsnull)
           , mBitMap(nsnull)
           , mBlockSize(0)
           , mBitMapWords(0)
           , mFileSize(0)
           , mBitMapDirty(false)
            {}
    ~nsDiskCacheBlockFile() { (void) Close(true); }
    
    nsresult  Open( nsIFile *  blockFile, PRUint32  blockSize,
                    PRUint32  bitMapSize);
    nsresult  Close(bool flush);
    
    /*
     * Trim
     * Truncates the block file to the end of the last allocated block.
     */
    nsresult  Trim() { return nsDiskCache::Truncate(mFD, CalcBlockFileSize()); }
    nsresult  DeallocateBlocks( PRInt32  startBlock, PRInt32  numBlocks);
    nsresult  WriteBlocks( void * buffer, PRUint32 size, PRInt32  numBlocks, 
                           PRInt32 * startBlock);
    nsresult  ReadBlocks( void * buffer, PRInt32  startBlock, PRInt32  numBlocks, 
                          PRInt32 * bytesRead);
    
private:
    nsresult  FlushBitMap();
    PRInt32   AllocateBlocks( PRInt32  numBlocks);
    nsresult  VerifyAllocation( PRInt32 startBlock, PRInt32 numBLocks);
    PRUint32  CalcBlockFileSize();
    bool   Write(PRInt32 offset, const void *buf, PRInt32 amount);

/**
 *  Data members
 */
    PRFileDesc *                mFD;
    PRUint32 *                  mBitMap;      // XXX future: array of bit map blocks
    PRUint32                    mBlockSize;
    PRUint32                    mBitMapWords;
    PRInt32                     mFileSize;
    bool                        mBitMapDirty;
};

#endif // _nsDiskCacheBlockFile_h_
