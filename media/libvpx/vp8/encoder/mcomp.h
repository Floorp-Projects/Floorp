/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef __INC_MCOMP_H
#define __INC_MCOMP_H

#include "block.h"
#include "variance.h"

#ifdef ENTROPY_STATS
extern void init_mv_ref_counts();
extern void accum_mv_refs(MB_PREDICTION_MODE, const int near_mv_ref_cts[4]);
#endif


#define MAX_MVSEARCH_STEPS 8                                    // The maximum number of steps in a step search given the largest allowed initial step
#define MAX_FULL_PEL_VAL ((1 << (MAX_MVSEARCH_STEPS)) - 1)      // Max full pel mv specified in 1 pel units
#define MAX_FIRST_STEP (1 << (MAX_MVSEARCH_STEPS-1))            // Maximum size of the first step in full pel units

extern void print_mode_context(void);
extern int vp8_mv_bit_cost(int_mv *mv, int_mv *ref, int *mvcost[2], int Weight);
extern void vp8_init_dsmotion_compensation(MACROBLOCK *x, int stride);
extern void vp8_init3smotion_compensation(MACROBLOCK *x,  int stride);


extern int vp8_hex_search
(
    MACROBLOCK *x,
    BLOCK *b,
    BLOCKD *d,
    int_mv *ref_mv,
    int_mv *best_mv,
    int search_param,
    int error_per_bit,
    const vp8_variance_fn_ptr_t *vf,
    int *mvsadcost[2],
    int *mvcost[2],
    int_mv *center_mv
);

typedef int (fractional_mv_step_fp)
    (MACROBLOCK *x, BLOCK *b, BLOCKD *d, int_mv *bestmv, int_mv *ref_mv,
     int error_per_bit, const vp8_variance_fn_ptr_t *vfp, int *mvcost[2],
     int *distortion, unsigned int *sse);
extern fractional_mv_step_fp vp8_find_best_sub_pixel_step_iteratively;
extern fractional_mv_step_fp vp8_find_best_sub_pixel_step;
extern fractional_mv_step_fp vp8_find_best_half_pixel_step;
extern fractional_mv_step_fp vp8_skip_fractional_mv_step;

#define prototype_full_search_sad(sym)\
    int (sym)\
    (\
     MACROBLOCK *x, \
     BLOCK *b, \
     BLOCKD *d, \
     int_mv *ref_mv, \
     int sad_per_bit, \
     int distance, \
     vp8_variance_fn_ptr_t *fn_ptr, \
     int *mvcost[2], \
     int_mv *center_mv \
    )

#define prototype_refining_search_sad(sym)\
    int (sym)\
    (\
     MACROBLOCK *x, \
     BLOCK *b, \
     BLOCKD *d, \
     int_mv *ref_mv, \
     int sad_per_bit, \
     int distance, \
     vp8_variance_fn_ptr_t *fn_ptr, \
     int *mvcost[2], \
     int_mv *center_mv \
    )

#define prototype_diamond_search_sad(sym)\
    int (sym)\
    (\
     MACROBLOCK *x, \
     BLOCK *b, \
     BLOCKD *d, \
     int_mv *ref_mv, \
     int_mv *best_mv, \
     int search_param, \
     int sad_per_bit, \
     int *num00, \
     vp8_variance_fn_ptr_t *fn_ptr, \
     int *mvcost[2], \
     int_mv *center_mv \
    )

#if ARCH_X86 || ARCH_X86_64
#include "x86/mcomp_x86.h"
#endif

typedef prototype_full_search_sad(*vp8_full_search_fn_t);
extern prototype_full_search_sad(vp8_full_search_sad);
extern prototype_full_search_sad(vp8_full_search_sadx3);
extern prototype_full_search_sad(vp8_full_search_sadx8);

typedef prototype_refining_search_sad(*vp8_refining_search_fn_t);
extern prototype_refining_search_sad(vp8_refining_search_sad);
extern prototype_refining_search_sad(vp8_refining_search_sadx4);

typedef prototype_diamond_search_sad(*vp8_diamond_search_fn_t);
extern prototype_diamond_search_sad(vp8_diamond_search_sad);
extern prototype_diamond_search_sad(vp8_diamond_search_sadx4);

#ifndef vp8_search_full_search
#define vp8_search_full_search vp8_full_search_sad
#endif
extern prototype_full_search_sad(vp8_search_full_search);

#ifndef vp8_search_refining_search
#define vp8_search_refining_search vp8_refining_search_sad
#endif
extern prototype_refining_search_sad(vp8_search_refining_search);

#ifndef vp8_search_diamond_search
#define vp8_search_diamond_search vp8_diamond_search_sad
#endif
extern prototype_diamond_search_sad(vp8_search_diamond_search);

typedef struct
{
    prototype_full_search_sad(*full_search);
    prototype_refining_search_sad(*refining_search);
    prototype_diamond_search_sad(*diamond_search);
} vp8_search_rtcd_vtable_t;

#if CONFIG_RUNTIME_CPU_DETECT
#define SEARCH_INVOKE(ctx,fn) (ctx)->fn
#else
#define SEARCH_INVOKE(ctx,fn) vp8_search_##fn
#endif

#endif
