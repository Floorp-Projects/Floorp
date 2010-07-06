/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license 
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may 
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "type_aliases.h"
#include "blockd.h"
#include "onyxd_int.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/mem.h"

#define BR_COUNT 8
#define BOOL_DATA UINT8

#define OCB_X PREV_COEF_CONTEXTS * ENTROPY_NODES
DECLARE_ALIGNED(16, UINT16, vp8_coef_bands_x[16]) = { 0, 1 * OCB_X, 2 * OCB_X, 3 * OCB_X, 6 * OCB_X, 4 * OCB_X, 5 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 7 * OCB_X};
#define EOB_CONTEXT_NODE            0
#define ZERO_CONTEXT_NODE           1
#define ONE_CONTEXT_NODE            2
#define LOW_VAL_CONTEXT_NODE        3
#define TWO_CONTEXT_NODE            4
#define THREE_CONTEXT_NODE          5
#define HIGH_LOW_CONTEXT_NODE       6
#define CAT_ONE_CONTEXT_NODE        7
#define CAT_THREEFOUR_CONTEXT_NODE  8
#define CAT_THREE_CONTEXT_NODE      9
#define CAT_FIVE_CONTEXT_NODE       10

/*
//the definition is put in "onyxd_int.h"
typedef struct
{
    INT16         min_val;
    INT16         Length;
    UINT8 Probs[12];
} TOKENEXTRABITS;
*/

DECLARE_ALIGNED(16, static const TOKENEXTRABITS, vp8d_token_extra_bits2[MAX_ENTROPY_TOKENS]) =
{
    {  0, -1, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },  //ZERO_TOKEN
    {  1, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   //ONE_TOKEN
    {  2, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   //TWO_TOKEN
    {  3, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   //THREE_TOKEN
    {  4, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   //FOUR_TOKEN
    {  5, 0, { 159, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },  //DCT_VAL_CATEGORY1
    {  7, 1, { 145, 165, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } }, //DCT_VAL_CATEGORY2
    { 11, 2, { 140, 148, 173, 0,  0,  0,  0,  0,  0,  0,  0,  0   } }, //DCT_VAL_CATEGORY3
    { 19, 3, { 135, 140, 155, 176, 0,  0,  0,  0,  0,  0,  0,  0   } }, //DCT_VAL_CATEGORY4
    { 35, 4, { 130, 134, 141, 157, 180, 0,  0,  0,  0,  0,  0,  0   } }, //DCT_VAL_CATEGORY5
    { 67, 10, { 129, 130, 133, 140, 153, 177, 196, 230, 243, 254, 254, 0   } }, //DCT_VAL_CATEGORY6
    {  0, -1, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },  // EOB TOKEN
};


void vp8_reset_mb_tokens_context(MACROBLOCKD *x)
{
    ENTROPY_CONTEXT **const A = x->above_context;
    ENTROPY_CONTEXT(* const L)[4] = x->left_context;

    ENTROPY_CONTEXT *a;
    ENTROPY_CONTEXT *l;

    /* Clear entropy contexts for Y blocks */
    a = A[Y1CONTEXT];
    l = L[Y1CONTEXT];
    *a = 0;
    *(a+1) = 0;
    *(a+2) = 0;
    *(a+3) = 0;
    *l = 0;
    *(l+1) = 0;
    *(l+2) = 0;
    *(l+3) = 0;

    /* Clear entropy contexts for U blocks */
    a = A[UCONTEXT];
    l = L[UCONTEXT];
    *a = 0;
    *(a+1) = 0;
    *l = 0;
    *(l+1) = 0;

    /* Clear entropy contexts for V blocks */
    a = A[VCONTEXT];
    l = L[VCONTEXT];
    *a = 0;
    *(a+1) = 0;
    *l = 0;
    *(l+1) = 0;

    /* Clear entropy contexts for Y2 blocks */
    if (x->mbmi.mode != B_PRED && x->mbmi.mode != SPLITMV)
    {
        a = A[Y2CONTEXT];
        l = L[Y2CONTEXT];
        *a = 0;
        *l = 0;
    }
}
DECLARE_ALIGNED(16, extern const unsigned int, vp8dx_bitreader_norm[256]);
#define NORMALIZE \
    /*if(range < 0x80)*/                            \
    { \
        shift = vp8dx_bitreader_norm[range]; \
        range <<= shift; \
        value <<= shift; \
        count -= shift; \
        if(count <= 0) \
        { \
            count += BR_COUNT ; \
            value |= (*bufptr) << (BR_COUNT-count); \
            bufptr = br_ptr_advance(bufptr, 1); \
        } \
    }

#define DECODE_AND_APPLYSIGN(value_to_sign) \
    split = (range + 1) >> 1; \
    if ( (value >> 8) < split ) \
    { \
        range = split; \
        v= value_to_sign; \
    } \
    else \
    { \
        range = range-split; \
        value = value-(split<<8); \
        v = -value_to_sign; \
    } \
    range +=range;                   \
    value +=value;                   \
    if (!--count) \
    { \
        count = BR_COUNT; \
        value |= *bufptr; \
        bufptr = br_ptr_advance(bufptr, 1); \
    }

#define DECODE_AND_BRANCH_IF_ZERO(probability,branch) \
    { \
        split = 1 +  ((( probability*(range-1) ) )>> 8); \
        if ( (value >> 8) < split ) \
        { \
            range = split; \
            NORMALIZE \
            goto branch; \
        } \
        value -= (split<<8); \
        range = range - split; \
        NORMALIZE \
    }

#define DECODE_AND_LOOP_IF_ZERO(probability,branch) \
    { \
        split = 1 + ((( probability*(range-1) ) ) >> 8); \
        if ( (value >> 8) < split ) \
        { \
            range = split; \
            NORMALIZE \
            Prob = coef_probs; \
            if(c<15) {\
            ++c; \
            Prob += vp8_coef_bands_x[c]; \
            goto branch; \
            } goto BLOCK_FINISHED; /*for malformed input */\
        } \
        value -= (split<<8); \
        range = range - split; \
        NORMALIZE \
    }

#define DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val) \
    DECODE_AND_APPLYSIGN(val) \
    Prob = coef_probs + (ENTROPY_NODES*2); \
    if(c < 15){\
        qcoeff_ptr [ scan[c] ] = (INT16) v; \
        ++c; \
        goto DO_WHILE; }\
    qcoeff_ptr [ scan[15] ] = (INT16) v; \
    goto BLOCK_FINISHED;


#define DECODE_EXTRABIT_AND_ADJUST_VAL(t,bits_count)\
    split = 1 +  (((range-1) * vp8d_token_extra_bits2[t].Probs[bits_count]) >> 8); \
    if(value >= (split<<8))\
    {\
        range = range-split;\
        value = value-(split<<8);\
        val += ((UINT16)1<<bits_count);\
    }\
    else\
    {\
        range = split;\
    }\
    NORMALIZE

int vp8_decode_mb_tokens(VP8D_COMP *dx, MACROBLOCKD *x)
{
    ENTROPY_CONTEXT **const A = x->above_context;
    ENTROPY_CONTEXT(* const L)[4] = x->left_context;
    const VP8_COMMON *const oc = & dx->common;

    BOOL_DECODER *bc = x->current_bc;

    ENTROPY_CONTEXT *a;
    ENTROPY_CONTEXT *l;
    int i;

    int eobtotal = 0;

    register int count;

    const BOOL_DATA *bufptr;
    register unsigned int range;
    register unsigned int value;
    const int *scan;
    register unsigned int shift;
    UINT32 split;
    INT16 *qcoeff_ptr;

    const vp8_prob *coef_probs;
    int type;
    int stop;
    INT16 val, bits_count;
    INT16 c;
    INT16 t;
    INT16 v;
    const vp8_prob *Prob;

    //int *scan;
    type = 3;
    i = 0;
    stop = 16;

    if (x->mbmi.mode != B_PRED && x->mbmi.mode != SPLITMV)
    {
        i = 24;
        stop = 24;
        type = 1;
        qcoeff_ptr = &x->qcoeff[24*16];
        scan = vp8_default_zig_zag1d;
        eobtotal -= 16;
    }
    else
    {
        scan = vp8_default_zig_zag1d;
        qcoeff_ptr = &x->qcoeff[0];
    }

    count   = bc->count;
    range   = bc->range;
    value   = bc->value;
    bufptr  = bc->read_ptr;


    coef_probs = oc->fc.coef_probs [type] [ 0 ] [0];

BLOCK_LOOP:
    a = A[ vp8_block2context[i] ] + vp8_block2above[i];
    l = L[ vp8_block2context[i] ] + vp8_block2left[i];
    c = (INT16)(!type);

    VP8_COMBINEENTROPYCONTEXTS(t, *a, *l);
    Prob = coef_probs;
    Prob += t * ENTROPY_NODES;

DO_WHILE:
    Prob += vp8_coef_bands_x[c];
    DECODE_AND_BRANCH_IF_ZERO(Prob[EOB_CONTEXT_NODE], BLOCK_FINISHED);

CHECK_0_:
    DECODE_AND_LOOP_IF_ZERO(Prob[ZERO_CONTEXT_NODE], CHECK_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[ONE_CONTEXT_NODE], ONE_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[LOW_VAL_CONTEXT_NODE], LOW_VAL_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[HIGH_LOW_CONTEXT_NODE], HIGH_LOW_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_THREEFOUR_CONTEXT_NODE], CAT_THREEFOUR_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_FIVE_CONTEXT_NODE], CAT_FIVE_CONTEXT_NODE_0_);
    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY6].min_val;
    bits_count = vp8d_token_extra_bits2[DCT_VAL_CATEGORY6].Length;

    do
    {
        DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY6, bits_count);
        bits_count -- ;
    }
    while (bits_count >= 0);

    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_FIVE_CONTEXT_NODE_0_:
    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY5].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 4);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 3);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY5, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_THREEFOUR_CONTEXT_NODE_0_:
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_THREE_CONTEXT_NODE], CAT_THREE_CONTEXT_NODE_0_);
    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY4].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 3);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY4, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_THREE_CONTEXT_NODE_0_:
    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY3].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY3, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY3, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY3, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

HIGH_LOW_CONTEXT_NODE_0_:
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_ONE_CONTEXT_NODE], CAT_ONE_CONTEXT_NODE_0_);

    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY2].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY2, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY2, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_ONE_CONTEXT_NODE_0_:
    val = vp8d_token_extra_bits2[DCT_VAL_CATEGORY1].min_val;
    DECODE_EXTRABIT_AND_ADJUST_VAL(DCT_VAL_CATEGORY1, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

LOW_VAL_CONTEXT_NODE_0_:
    DECODE_AND_BRANCH_IF_ZERO(Prob[TWO_CONTEXT_NODE], TWO_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[THREE_CONTEXT_NODE], THREE_CONTEXT_NODE_0_);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(4);

THREE_CONTEXT_NODE_0_:
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(3);

TWO_CONTEXT_NODE_0_:
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(2);

ONE_CONTEXT_NODE_0_:
    DECODE_AND_APPLYSIGN(1);
    Prob = coef_probs + ENTROPY_NODES;

    if (c < 15)
    {
        qcoeff_ptr [ scan[c] ] = (INT16) v;
        ++c;
        goto DO_WHILE;
    }

    qcoeff_ptr [ scan[15] ] = (INT16) v;
BLOCK_FINISHED:
    t = ((x->block[i].eob = c) != !type);   // any nonzero data?
    eobtotal += x->block[i].eob;
    *a = *l = t;
    qcoeff_ptr += 16;

    i++;

    if (i < stop)
        goto BLOCK_LOOP;

    if (i == 25)
    {
        scan = vp8_default_zig_zag1d;//x->scan_order1d;
        type = 0;
        i = 0;
        stop = 16;
        coef_probs = oc->fc.coef_probs [type] [ 0 ] [0];
        qcoeff_ptr = &x->qcoeff[0];
        goto BLOCK_LOOP;
    }

    if (i == 16)
    {
        type = 2;
        coef_probs = oc->fc.coef_probs [type] [ 0 ] [0];
        stop = 24;
        goto BLOCK_LOOP;
    }

    bc->count = count;
    bc->value = value;
    bc->range = range;
    bc->read_ptr = bufptr;
    return eobtotal;

}
