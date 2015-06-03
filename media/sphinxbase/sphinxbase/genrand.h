/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* 
   A C-program for MT19937, with initialization improved 2002/1/26.
   Coded by Takuji Nishimura and Makoto Matsumoto.

   Before using, initialize the state by using init_genrand(seed)  
   or init_by_array(init_key, key_length).

   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.                          

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
`        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote 
        products derived from this software without specific prior written 
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


   Any feedback is very welcome.
   http://www.math.keio.ac.jp/matumoto/emt.html
   email: matumoto@math.keio.ac.jp
*/

/* ====================================================================
 * Copyright (c) 1999-2004 Carnegie Mellon University.  All rights
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
 * randgen.c   : a portable random generator 
 * 
 *
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1999 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 * 
 * HISTORY
 * $Log: genrand.h,v $
 * Revision 1.3  2005/06/22 03:01:50  arthchan2003
 * Added  keyword
 *
 * Revision 1.3  2005/03/30 01:22:48  archan
 * Fixed mistakes in last updates. Add
 *
 * 
 * 18-Nov-04 ARCHAN (archan@cs.cmu.edu) at Carnegie Mellon University
 *              First incorporated from the Mersenne Twister Random
 *              Number Generator package. It was chosen because it is
 *              in BSD-license and its performance is quite
 *              reasonable. Of course if you look at the inventors's
 *              page.  This random generator can actually gives
 *              19937-bits period.  This is already far from we need. 
 *              This will possibly good enough for the next 10 years. 
 *
 *              I also downgrade the code a little bit to avoid Sphinx's
 *              developers misused it. 
 */

#ifndef _LIBUTIL_GENRAND_H_
#define _LIBUTIL_GENRAND_H_

#define S3_RAND_MAX_INT32 0x7fffffff
#include <stdio.h>

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>

/** \file genrand.h
 *\brief High performance prortable random generator created by Takuji
 *Nishimura and Makoto Matsumoto.  
 * 
 * A high performance which applied Mersene twister primes to generate
 * random number. If probably seeded, the random generator can achieve 
 * 19937-bits period.  For technical detail.  Please take a look at 
 * (FIXME! Need to search for the web site.) http://www.
 */
#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

/**
 * Macros to simplify calling of random generator function.
 *
 */
#define s3_rand_seed(s) genrand_seed(s);
#define s3_rand_int31()  genrand_int31()
#define s3_rand_real() genrand_real3()
#define s3_rand_res53()  genrand_res53()

/**
 *Initialize the seed of the random generator. 
 */
SPHINXBASE_EXPORT
void genrand_seed(unsigned long s);

/**
 *generates a random number on [0,0x7fffffff]-interval 
 */
SPHINXBASE_EXPORT
long genrand_int31(void);

/**
 *generates a random number on (0,1)-real-interval 
 */
SPHINXBASE_EXPORT
double genrand_real3(void);

/**
 *generates a random number on [0,1) with 53-bit resolution
 */
SPHINXBASE_EXPORT
double genrand_res53(void);

#ifdef __cplusplus
}
#endif

#endif /*_LIBUTIL_GENRAND_H_*/



