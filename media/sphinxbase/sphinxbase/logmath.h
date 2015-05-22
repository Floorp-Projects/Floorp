/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* ====================================================================
 * Copyright (c) 2007 Carnegie Mellon University.  All rights
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
 * @file logmath.h
 * @brief Fast integer logarithmic addition operations.
 *
 * In evaluating HMM models, probability values are often kept in log
 * domain, to avoid overflow.  To enable these logprob values to be
 * held in int32 variables without significant loss of precision, a
 * logbase of (1+epsilon) (where epsilon < 0.01 or so) is used.  This
 * module maintains this logbase (B).
 * 
 * However, maintaining probabilities in log domain creates a problem
 * when adding two probability values.  This problem can be solved by
 * table lookup.  Note that:
 *
 *  - \f$ b^z = b^x + b^y \f$
 *  - \f$ b^z = b^x(1 + b^{y-x})     = b^y(1 + e^{x-y}) \f$
 *  - \f$ z   = x + log_b(1 + b^{y-x}) = y + log_b(1 + b^{x-y}) \f$
 *
 * So:
 * 
 *  - when \f$ y > x, z = y + logadd\_table[-(x-y)] \f$
 *  - when \f$ x > y, z = x + logadd\_table[-(y-x)] \f$
 *  - where \f$ logadd\_table[n] = log_b(1 + b^{-n}) \f$
 *
 * The first entry in <i>logadd_table</i> is
 * simply \f$ log_b(2.0) \f$, for
 * the case where \f$ y = x \f$ and thus
 * \f$ z = log_b(2x) = log_b(2) + x \f$.  The last entry is zero,
 * where \f$ log_b(x+y) = x = y \f$ due to loss of precision.
 *
 * Since this table can be quite large particularly for small
 * logbases, an option is provided to compress it by dropping the
 * least significant bits of the table.
 */

#ifndef __LOGMATH_H__
#define __LOGMATH_H__

#include <sphinxbase/sphinxbase_export.h>
#include <sphinxbase/prim_type.h>
#include <sphinxbase/cmd_ln.h>


#ifdef __cplusplus
extern "C" {
#endif
#if 0
/* Fool Emacs. */
}
#endif

/**
 * Integer log math computation table.
 *
 * This is exposed here to allow log-add computations to be inlined.
 */
typedef struct logadd_s logadd_t;
struct logadd_s {
    /** Table, in unsigned integers of (width) bytes. */
    void *table;
    /** Number of elements in (table).  This is never smaller than 256 (important!) */
    uint32 table_size;
    /** Width of elements of (table). */
    uint8 width;
    /** Right shift applied to elements in (table). */
    int8 shift;
};

/**
 * Integer log math computation class.
 */
typedef struct logmath_s logmath_t;

/**
 * Obtain the log-add table from a logmath_t *
 */
#define LOGMATH_TABLE(lm) ((logadd_t *)lm)

/**
 * Initialize a log math computation table.
 * @param base The base B in which computation is to be done.
 * @param shift Log values are shifted right by this many bits.
 * @param use_table Whether to use an add table or not
 * @return The newly created log math table.
 */
SPHINXBASE_EXPORT
logmath_t *logmath_init(float64 base, int shift, int use_table);

/**
 * Memory-map (or read) a log table from a file.
 */
SPHINXBASE_EXPORT
logmath_t *logmath_read(const char *filename);

/**
 * Write a log table to a file.
 */
SPHINXBASE_EXPORT
int32 logmath_write(logmath_t *lmath, const char *filename);

/**
 * Get the log table size and dimensions.
 */
SPHINXBASE_EXPORT
int32 logmath_get_table_shape(logmath_t *lmath, uint32 *out_size,
                              uint32 *out_width, uint32 *out_shift);

/**
 * Get the log base.
 */
SPHINXBASE_EXPORT
float64 logmath_get_base(logmath_t *lmath);

/**
 * Get the smallest possible value represented in this base.
 */
SPHINXBASE_EXPORT
int logmath_get_zero(logmath_t *lmath);

/**
 * Get the width of the values in a log table.
 */
SPHINXBASE_EXPORT
int logmath_get_width(logmath_t *lmath);

/**
 * Get the shift of the values in a log table.
 */
SPHINXBASE_EXPORT
int logmath_get_shift(logmath_t *lmath);

/**
 * Retain ownership of a log table.
 *
 * @return pointer to retained log table.
 */
SPHINXBASE_EXPORT
logmath_t *logmath_retain(logmath_t *lmath);

/**
 * Free a log table.
 *
 * @return new reference count (0 if freed completely)
 */
SPHINXBASE_EXPORT
int logmath_free(logmath_t *lmath);

/**
 * Add two values in log space exactly and slowly (without using add table).
 */
SPHINXBASE_EXPORT
int logmath_add_exact(logmath_t *lmath, int logb_p, int logb_q);

/**
 * Add two values in log space (i.e. return log(exp(p)+exp(q)))
 */
SPHINXBASE_EXPORT
int logmath_add(logmath_t *lmath, int logb_p, int logb_q);

/**
 * Convert linear floating point number to integer log in base B.
 */
SPHINXBASE_EXPORT
int logmath_log(logmath_t *lmath, float64 p);

/**
 * Convert integer log in base B to linear floating point.
 */
SPHINXBASE_EXPORT
float64 logmath_exp(logmath_t *lmath, int logb_p);

/**
 * Convert natural log (in floating point) to integer log in base B.
 */
SPHINXBASE_EXPORT
int logmath_ln_to_log(logmath_t *lmath, float64 log_p);

/**
 * Convert integer log in base B to natural log (in floating point).
 */
SPHINXBASE_EXPORT
float64 logmath_log_to_ln(logmath_t *lmath, int logb_p);

/**
 * Convert base 10 log (in floating point) to integer log in base B.
 */
SPHINXBASE_EXPORT
int logmath_log10_to_log(logmath_t *lmath, float64 log_p);

/**
 * Convert integer log in base B to base 10 log (in floating point).
 */
SPHINXBASE_EXPORT
float64 logmath_log_to_log10(logmath_t *lmath, int logb_p);

#ifdef __cplusplus
}
#endif


#endif /*  __LOGMATH_H__ */
