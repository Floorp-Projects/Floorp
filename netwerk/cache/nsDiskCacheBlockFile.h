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
 * The Original Code is nsDiskCacheBlockFile.h, released
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

#ifndef _nsDiskCacheBlockFile_h_
#define _nsDiskCacheBlockFile_h_

#include "nsILocalFile.h"

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
           , mFileSize(0)
           , mBitMapDirty(PR_FALSE)
            {}
    ~nsDiskCacheBlockFile() { (void) Close(PR_TRUE); }
    
    nsresult  Open( nsILocalFile *  blockFile, PRUint32  blockSize);
    nsresult  Close(PRBool flush);
    
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
    PRInt32                     mFileSize;
    PRBool                      mBitMapDirty;
};

#endif // _nsDiskCacheBlockFile_h_
