/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
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

#ifndef _LIBUTIL_BITVEC_H_
#define _LIBUTIL_BITVEC_H_

#include <string.h>

/* Win32/WinCE DLL gunk */
#include <sphinxbase/sphinxbase_export.h>

#include <sphinxbase/prim_type.h>
#include <sphinxbase/ckd_alloc.h>

/** 
 * @file bitvec.h
 * @brief An implementation of bit vectors.
 * 
 * Implementation of basic operations of bit vectors.  
 */

#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

#define BITVEC_BITS 32
typedef uint32 bitvec_t;

/**
 * Number of bitvec_t in a bit vector
 */
#define bitvec_size(n)	        (((n)+BITVEC_BITS-1)/BITVEC_BITS)

/**
 * Allocate a bit vector, all bits are clear
 */
#define bitvec_alloc(n)		ckd_calloc(bitvec_size(n), sizeof(bitvec_t))

/**
 * Resize a bit vector, clear the remaining bits
 */
SPHINXBASE_EXPORT
bitvec_t *bitvec_realloc(bitvec_t *vec,	/* In: Bit vector to search */
			 size_t old_len, /* In: Old length */
                         size_t new_len); /* In: New lenght of above bit vector */
/**
 * Free a bit vector.
 */
#define bitvec_free(v)		ckd_free(v)

/**
 * Set the b-th bit of bit vector v
 * @param v is a vector
 * @param b is the bit which will be set
 */

#define bitvec_set(v,b)		(v[(b)/BITVEC_BITS] |= (1UL << ((b) & (BITVEC_BITS-1))))

/**
 * Set all n bits in bit vector v
 * @param v is a vector
 * @param n is the number of bits
 */

#define bitvec_set_all(v,n)	memset(v, (bitvec_t)-1, \
                                       (((n)+BITVEC_BITS-1)/BITVEC_BITS) * \
                                       sizeof(bitvec_t))
/**
 * Clear the b-th bit of bit vector v
 * @param v is a vector
 * @param b is the bit which will be set
 */

#define bitvec_clear(v,b)	(v[(b)/BITVEC_BITS] &= ~(1UL << ((b) & (BITVEC_BITS-1))))

/**
 * Clear all n bits in bit vector v
 * @param v is a vector
 * @param n is the number of bits
 */

#define bitvec_clear_all(v,n)	memset(v, 0, (((n)+BITVEC_BITS-1)/BITVEC_BITS) * \
                                       sizeof(bitvec_t))

/**
 * Check whether the b-th bit is set in vector v
 * @param v is a vector
 * @param b is the bit which will be checked
 */

#define bitvec_is_set(v,b)	(v[(b)/BITVEC_BITS] & (1UL << ((b) & (BITVEC_BITS-1))))

/**
 * Check whether the b-th bit is cleared in vector v
 * @param v is a vector
 * @param b is the bit which will be checked
 */

#define bitvec_is_clear(v,b)	(! (bitvec_is_set(v,b)))


/**
 * Return the number of bits set in the given bitvector.
 *
 * @param vec is the bit vector
 * @param len is the length of bit vector <code>vec</code>
 * @return the number of bits being set in vector <code>vec</code>
 */
SPHINXBASE_EXPORT
size_t bitvec_count_set(bitvec_t *vec,	/* In: Bit vector to search */
                        size_t len);	/* In: Lenght of above bit vector */

#ifdef __cplusplus
}
#endif

#endif
