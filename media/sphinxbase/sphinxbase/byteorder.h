/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2001 Carnegie Mellon University.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * This work was supported in part by funding from the Defense Advanced 
 * Research Projects Agency and the National Science Foundation of the 
 * United States of America, and the CMU Sphinx Speech Consortium.
 *
 * THIS SOFTWARE IS PROVIDED BY CARNEGIE MELLON UNIVERSITY ``AS IS'' AND 
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
 * NOR ITS EMPLOYEES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ====================================================================
 *
 */

/*
 * byteorder.h -- Byte swapping ordering macros.
 * 
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1996 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * 
 * $Log: byteorder.h,v $
 * Revision 1.8  2005/09/01 21:09:54  dhdfu
 * Really, actually, truly consolidate byteswapping operations into
 * byteorder.h.  Where unconditional byteswapping is needed, SWAP_INT32()
 * and SWAP_INT16() are to be used.  The WORDS_BIGENDIAN macro from
 * autoconf controls the functioning of the conditional swap macros
 * (SWAP_?[LW]) whose names and semantics have been regularized.
 * Private, adhoc macros have been removed.
 *
 */

#ifndef __S2_BYTEORDER_H__
#define __S2_BYTEORDER_H__	1

/* Macro to byteswap an int16 variable.  x = ptr to variable */
#define SWAP_INT16(x)	*(x) = ((0x00ff & (*(x))>>8) | (0xff00 & (*(x))<<8))

/* Macro to byteswap an int32 variable.  x = ptr to variable */
#define SWAP_INT32(x)	*(x) = ((0x000000ff & (*(x))>>24) | \
				(0x0000ff00 & (*(x))>>8) | \
				(0x00ff0000 & (*(x))<<8) | \
				(0xff000000 & (*(x))<<24))

/* Macro to byteswap a float32 variable.  x = ptr to variable */
#define SWAP_FLOAT32(x)	SWAP_INT32((int32 *) x)

/* Macro to byteswap a float64 variable.  x = ptr to variable */
#define SWAP_FLOAT64(x)	{ int *low = (int *) (x), *high = (int *) (x) + 1,\
			      temp;\
			  SWAP_INT32(low);  SWAP_INT32(high);\
			  temp = *low; *low = *high; *high = temp;}

#ifdef WORDS_BIGENDIAN
#define SWAP_BE_64(x)
#define SWAP_BE_32(x)
#define SWAP_BE_16(x)
#define SWAP_LE_64(x) SWAP_FLOAT64(x)
#define SWAP_LE_32(x) SWAP_INT32(x)
#define SWAP_LE_16(x) SWAP_INT16(x)
#else
#define SWAP_LE_64(x)
#define SWAP_LE_32(x)
#define SWAP_LE_16(x)
#define SWAP_BE_64(x) SWAP_FLOAT64(x)
#define SWAP_BE_32(x) SWAP_INT32(x)
#define SWAP_BE_16(x) SWAP_INT16(x)
#endif

#endif
