/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef prbit_h___
#define prbit_h___

#include "prtypes.h"
PR_BEGIN_EXTERN_C

/*
** A prbitmap_t is a long integer that can be used for bitmaps
*/
typedef unsigned long prbitmap_t;

#define PR_TEST_BIT(_map,_bit) \
    ((_map)[(_bit)>>PR_BITS_PER_LONG_LOG2] & (1L << ((_bit) & (PR_BITS_PER_LONG-1))))
#define PR_SET_BIT(_map,_bit) \
    ((_map)[(_bit)>>PR_BITS_PER_LONG_LOG2] |= (1L << ((_bit) & (PR_BITS_PER_LONG-1))))
#define PR_CLEAR_BIT(_map,_bit) \
    ((_map)[(_bit)>>PR_BITS_PER_LONG_LOG2] &= ~(1L << ((_bit) & (PR_BITS_PER_LONG-1))))

/*
** Compute the log of the least power of 2 greater than or equal to n
*/
PR_EXTERN(PRIntn) PR_CeilingLog2(PRUint32 i); 

/*
** Compute the log of the greatest power of 2 less than or equal to n
*/
PR_EXTERN(PRIntn) PR_FloorLog2(PRUint32 i); 

/*
** Macro version of PR_CeilingLog2: Compute the log of the least power of
** 2 greater than or equal to _n. The result is returned in _log2.
*/
#define PR_CEILING_LOG2(_log2,_n)   \
  PR_BEGIN_MACRO                    \
    PRUint32 j_ = (PRUint32)(_n); 	\
    (_log2) = 0;                    \
    if ((j_) & ((j_)-1))            \
	(_log2) += 1;               \
    if ((j_) >> 16)                 \
	(_log2) += 16, (j_) >>= 16; \
    if ((j_) >> 8)                  \
	(_log2) += 8, (j_) >>= 8;   \
    if ((j_) >> 4)                  \
	(_log2) += 4, (j_) >>= 4;   \
    if ((j_) >> 2)                  \
	(_log2) += 2, (j_) >>= 2;   \
    if ((j_) >> 1)                  \
	(_log2) += 1;               \
  PR_END_MACRO

/*
** Macro version of PR_FloorLog2: Compute the log of the greatest power of
** 2 less than or equal to _n. The result is returned in _log2.
**
** This is equivalent to finding the highest set bit in the word.
*/
#define PR_FLOOR_LOG2(_log2,_n)   \
  PR_BEGIN_MACRO                    \
    PRUint32 j_ = (PRUint32)(_n); 	\
    (_log2) = 0;                    \
    if ((j_) >> 16)                 \
	(_log2) += 16, (j_) >>= 16; \
    if ((j_) >> 8)                  \
	(_log2) += 8, (j_) >>= 8;   \
    if ((j_) >> 4)                  \
	(_log2) += 4, (j_) >>= 4;   \
    if ((j_) >> 2)                  \
	(_log2) += 2, (j_) >>= 2;   \
    if ((j_) >> 1)                  \
	(_log2) += 1;               \
  PR_END_MACRO

PR_END_EXTERN_C
#endif /* prbit_h___ */
