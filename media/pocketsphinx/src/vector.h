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
 * vector.h -- vector routines.
 * 
 * **********************************************
 * CMU ARPA Speech Project
 *
 * Copyright (c) 1997 Carnegie Mellon University.
 * ALL RIGHTS RESERVED.
 * **********************************************
 */


#ifndef __VECTOR_H__
#define __VECTOR_H__

/* System headers. */
#include <stdio.h>

/* SphinxBase headers. */
#include <sphinxbase/prim_type.h>

typedef float32 *vector_t;

/*
 * The reason for some of the "trivial" routines below is that they could be OPTIMIZED for SPEED
 * at some point.
 */


/* Floor all elements of v[0..dim-1] to min value of f */
void vector_floor(vector_t v, int32 dim, float64 f);


/* Floor all non-0 elements of v[0..dim-1] to min value of f */
void vector_nz_floor(vector_t v, int32 dim, float64 f);


/*
 * Normalize the elements of the given vector so that they sum to 1.0.  If the sum is 0.0
 * to begin with, the vector is left untouched.  Return value: The normalization factor.
 */
float64 vector_sum_norm(vector_t v, int32 dim);


/* Print vector in one line, in %11.4e format, terminated by newline */
void vector_print(FILE *fp, vector_t v, int32 dim);


/* Return TRUE iff given vector is all 0.0 */
int32 vector_is_zero (float32 *vec,	/* In: Vector to be checked */
		      int32 len);	/* In: Length of above vector */

#endif /* VECTOR_H */ 
