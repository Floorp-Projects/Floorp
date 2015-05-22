/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 1999-2010 Carnegie Mellon University.  All rights
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

/**
 * @file tied_mgau_common.h
 * @brief Common code shared between SC and PTM (tied-state) models.
 */

#ifndef __TIED_MGAU_COMMON_H__
#define __TIED_MGAU_COMMON_H__

#include <sphinxbase/logmath.h>
#include <sphinxbase/fixpoint.h>

#define MGAU_MIXW_VERSION	"1.0"   /* Sphinx-3 file format version for mixw */
#define MGAU_PARAM_VERSION	"1.0"   /* Sphinx-3 file format version for mean/var */
#define NONE		-1
#define WORST_DIST	(int32)(0x80000000)

/** Subtract GMM component b (assumed to be positive) and saturate */
#ifdef FIXED_POINT
#define GMMSUB(a,b) \
	(((a)-(b) > a) ? (INT_MIN) : ((a)-(b)))
/** Add GMM component b (assumed to be positive) and saturate */
#define GMMADD(a,b) \
	(((a)+(b) < a) ? (INT_MAX) : ((a)+(b)))
#else
#define GMMSUB(a,b) ((a)-(b))
#define GMMADD(a,b) ((a)+(b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif


#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#define LOGMATH_INLINE static inline
#elif defined(_MSC_VER)
#define LOGMATH_INLINE __inline
#else
#define LOGMATH_INLINE static
#endif

/* Allocate 0..159 for negated quantized mixture weights and 0..96 for
 * negated normalized acoustic scores, so that the combination of the
 * two (for a single mixture) can never exceed 255. */
#define MAX_NEG_MIXW 159 /**< Maximum negated mixture weight value. */
#define MAX_NEG_ASCR 96  /**< Maximum negated acoustic score value. */

/**
 * Quickly log-add two negated log probabilities.
 *
 * @param lmath The log-math object
 * @param mlx A negative log probability (0 < mlx < 255)
 * @param mly A negative log probability (0 < mly < 255)
 * @return -log(exp(-mlx)+exp(-mly))
 *
 * We can do some extra-fast log addition since we know that
 * mixw+ascr is always less than 256 and hence x-y is also always less
 * than 256.  This relies on some cooperation from logmath_t which
 * will never produce a logmath table smaller than 256 entries.
 *
 * Note that the parameters are *negated* log probabilities (and
 * hence, are positive numbers), as is the return value.  This is the
 * key to the "fastness" of this function.
 */
LOGMATH_INLINE int
fast_logmath_add(logmath_t *lmath, int mlx, int mly)
{
    logadd_t *t = LOGMATH_TABLE(lmath);
    int d, r;

    /* d must be positive, obviously. */
    if (mlx > mly) {
        d = (mlx - mly);
        r = mly;
    }
    else {
        d = (mly - mlx);
        r = mlx;
    }

    return r - (((uint8 *)t->table)[d]);
}

#endif /* __TIED_MGAU_COMMON_H__ */
