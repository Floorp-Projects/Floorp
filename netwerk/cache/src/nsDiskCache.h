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
 * The Original Code is nsCacheDevice.h, released March 9, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Patrick Beard   <beard@netscape.com>
 *    Gordon Sheridan <gordon@netscape.com>
 */


#ifndef _nsDiskCache_h_
#define _nsDiskCache_h_

enum {
    eEvictionRankMask       = 0xFF000000;
    eLocationSelectorMask   = 0x00C00000;

    eExtraBlocksMask        = 0x00300000;
    eBlockNumberMask        = 0x000FFFFF;

    eFileReservedMask       = 0x003F0000;
    eFileGenerationMask     = 0x0000FFFF;
};

class DiskCacheRecord {
public:
    PRUint32   HashNumber(void)       { return mHashNumber; }
    void       SetHashNumber(PRUint32 hashNumber) { mHashNumber = hashNumber; }

    PRUint8    EvictionRank(void)
    {
        return (PRUint8)(mInfo & eEvictionRankMask) >> 24;
    }

    void       SetEvictionRank(PRUint8 rank)
    {
        mInfo &= ~eEvictionRankMask; // clear eviction rank bits
        mInfo |=  rank << 24;
    }

    PRUint32   LocationSelector(void)
    {
        return (PRUint32)(mInfo & eLocationSelectorMask) >> 22;
    }

    void       SetLocationSelector(PRUint32  selector)
    {
        mInfo &= ~eLocationSelectorMask; // clear location selector bits
        mInfo |= (selector & eLocationSelectorMask) << 22;
    }

    PRUint32   BlockCount(void)
    {
        return (PRUint32)((mInfo & eExtraBlocksMask) >> 20) + 1;
    }

    void       SetBlockCount(PRUint32  count)
    {
        NS_ASSERTION( (count>=1) && (count<=4),"invalid block count");
        count = --count;
        mInfo &= ~eExtraBlocksMask; // clear extra blocks bits
        mInfo |= (count & eExtraBlocksMask) << 20;
    }

    PRUint32   BlockNumber(void)
    {
        return mInfo & eBlockNumberMask;
    }

    void       SetBlockNumber(PRUint32  blockNumber)
    {
        mInfo &= ~eBlockNumberMask;  // clear block number bits
        mInfo |= blockNumber & eBlockNumberMask;
    }

    PRUint16   FileGeneration(void)
    {
        return mInfo & eFileGenerationMask;
    }

    void       SetFileGeneration(PRUint16 generation)
    {
        mInfo &= ~eFileGenerationMask;  // clear file generation bits
        mInfo |= generation & eFileGenerationMask;
    }


private:
    PRUint32   mHashNumber;
    PRUint32   mInfo;
};


/*

Cache Extent Table


1111 1111 0000 0000 0000 0000 0000 0000 : eEvictionRankMask
0000 0000 1100 0000 0000 0000 0000 0000 : eLocationSelectorMask   (0-3)
0000 0000 0011 0000 0000 0000 0000 0000 : number of extra contiguous blocks 1-4
0000 0000 0000 1111 1111 1111 1111 1111 : block#  0-1,048,575

0000 0000 0011 1111 0000 0000 0000 0000 : eFileReservedMask
0000 0000 0000 0000 1111 1111 1111 1111 : eFileGenerationMask

0  file
1   256 bytes
2  1024
3  4096


	log2 of seconds = 5 bits

log2	minutes	hours	days   years
00000	   1
00001	   2
00010	   4
00011	   8
00100	  16
00101	  32
00110	  64	  1
00111	 128	  2
01000	 256	  4
01001	 512	  8.5
01010	1024	 17
01011	2048	 34
01100		 68
01101		136
01110		273	 11
01111			 22
10000			 45.5
10001			 91
10010			182
10011			364	1
10100				2
10101				3
10110				4
10111
11000
11001
11010
11011
11100
11101
11110
11111

*/


#endif // _nsDiskCache_h_
