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
 * The Original Code is nsDiskCacheBlockFile.h, released April 12, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan  <gordon@netscape.com>
 */

#ifndef _nsDiskCacheBlockFile_h_
#define _nsDiskCacheBlockFile_h_

#include "nsILocalFile.h"
#include "nspr.h"

const unsigned short kBitMapBytes = 4096;

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
           , mBlockSize(0)
           , mEndOfFile(0)
           , mBitMap(nsnull)
           , mBitMapDirty(PR_FALSE)
            {}
    ~nsDiskCacheBlockFile() { (void) Close(PR_TRUE); }
    
    nsresult  Open( nsILocalFile *  blockFile, PRUint32  blockSize);
    nsresult  Close(PRBool flush);
    nsresult  Trim();
    PRInt32   AllocateBlocks( PRInt32  numBlocks);
    nsresult  DeallocateBlocks( PRInt32  startBlock, PRInt32  numBlocks);
    nsresult  WriteBlocks( void * buffer, PRInt32  startBlock, PRInt32  numBlocks);
    nsresult  ReadBlocks(  void * buffer, PRInt32  startBlock, PRInt32  numBlocks);
    
private:
    nsresult  FlushBitMap();
    nsresult  ValidateFile();   // called by Open()
    nsresult  VerifyAllocation( PRInt32 startBlock, PRInt32 numBLocks);
    PRInt32   LastBlock();

/**
 *  Data members
 */
    PRFileDesc *                mFD;
    PRUint32                    mBlockSize;
    PRUint32                    mEndOfFile;
    PRUint8 *                   mBitMap;      // XXX future: array of bit map blocks
    PRBool                      mBitMapDirty;
};

#endif // _nsDiskCacheBlockFile_h_
