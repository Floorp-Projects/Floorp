/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vp8/common/blockd.h"
#include "onyxd_int.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/mem.h"
#include "detokenize.h"

#define BOOL_DATA unsigned char

#define OCB_X PREV_COEF_CONTEXTS * ENTROPY_NODES
DECLARE_ALIGNED(16, static const unsigned char, coef_bands_x[16]) =
{
    0 * OCB_X, 1 * OCB_X, 2 * OCB_X, 3 * OCB_X,
    6 * OCB_X, 4 * OCB_X, 5 * OCB_X, 6 * OCB_X,
    6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X,
    6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 7 * OCB_X
};
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

#define CAT1_MIN_VAL    5
#define CAT2_MIN_VAL    7
#define CAT3_MIN_VAL   11
#define CAT4_MIN_VAL   19
#define CAT5_MIN_VAL   35
#define CAT6_MIN_VAL   67

#define CAT1_PROB0    159
#define CAT2_PROB0    145
#define CAT2_PROB1    165

#define CAT3_PROB0 140
#define CAT3_PROB1 148
#define CAT3_PROB2 173

#define CAT4_PROB0 135
#define CAT4_PROB1 140
#define CAT4_PROB2 155
#define CAT4_PROB3 176

#define CAT5_PROB0 130
#define CAT5_PROB1 134
#define CAT5_PROB2 141
#define CAT5_PROB3 157
#define CAT5_PROB4 180

static const unsigned char cat6_prob[12] =
{ 129, 130, 133, 140, 153, 177, 196, 230, 243, 254, 254, 0 };


void vp8_reset_mb_tokens_context(MACROBLOCKD *x)
{
    /* Clear entropy contexts for Y2 blocks */
    if (x->mode_info_context->mbmi.mode != B_PRED &&
        x->mode_info_context->mbmi.mode != SPLITMV)
    {
        vpx_memset(x->above_context, 0, sizeof(ENTROPY_CONTEXT_PLANES));
        vpx_memset(x->left_context, 0, sizeof(ENTROPY_CONTEXT_PLANES));
    }
    else
    {
        vpx_memset(x->above_context, 0, sizeof(ENTROPY_CONTEXT_PLANES)-1);
        vpx_memset(x->left_context, 0, sizeof(ENTROPY_CONTEXT_PLANES)-1);
    }
}

DECLARE_ALIGNED(16, extern const unsigned char, vp8_norm[256]);
#define FILL \
    if(count < 0) \
        VP8DX_BOOL_DECODER_FILL(count, value, bufptr, bufend);

#define NORMALIZE \
    /*if(range < 0x80)*/                            \
    { \
        shift = vp8_norm[range]; \
        range <<= shift; \
        value <<= shift; \
        count -= shift; \
    }

#define DECODE_AND_APPLYSIGN(value_to_sign) \
    split = (range + 1) >> 1; \
    bigsplit = (VP8_BD_VALUE)split << (VP8_BD_VALUE_SIZE - 8); \
    FILL \
    if ( value < bigsplit ) \
    { \
        range = split; \
        v= value_to_sign; \
    } \
    else \
    { \
        range = range-split; \
        value = value-bigsplit; \
        v = -value_to_sign; \
    } \
    range +=range;                   \
    value +=value;                   \
    count--;

#define DECODE_AND_BRANCH_IF_ZERO(probability,branch) \
    { \
        split = 1 +  ((( probability*(range-1) ) )>> 8); \
        bigsplit = (VP8_BD_VALUE)split << (VP8_BD_VALUE_SIZE - 8); \
        FILL \
        if ( value < bigsplit ) \
        { \
            range = split; \
            NORMALIZE \
            goto branch; \
        } \
        value -= bigsplit; \
        range = range - split; \
        NORMALIZE \
    }

#define DECODE_AND_LOOP_IF_ZERO(probability,branch) \
    { \
        split = 1 + ((( probability*(range-1) ) ) >> 8); \
        bigsplit = (VP8_BD_VALUE)split << (VP8_BD_VALUE_SIZE - 8); \
        FILL \
        if ( value < bigsplit ) \
        { \
            range = split; \
            NORMALIZE \
            Prob = coef_probs; \
            if(c<15) {\
            ++c; \
            Prob += coef_bands_x[c]; \
            goto branch; \
            } goto BLOCK_FINISHED; /*for malformed input */\
        } \
        value -= bigsplit; \
        range = range - split; \
        NORMALIZE \
    }

#define DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val) \
    DECODE_AND_APPLYSIGN(val) \
    Prob = coef_probs + (ENTROPY_NODES*2); \
    if(c < 15){\
        qcoeff_ptr [ scan[c] ] = (int16_t) v; \
        ++c; \
        goto DO_WHILE; }\
    qcoeff_ptr [ 15 ] = (int16_t) v; \
    goto BLOCK_FINISHED;


#define DECODE_EXTRABIT_AND_ADJUST_VAL(prob, bits_count)\
    split = 1 +  (((range-1) * prob) >> 8); \
    bigsplit = (VP8_BD_VALUE)split << (VP8_BD_VALUE_SIZE - 8); \
    FILL \
    if(value >= bigsplit)\
    {\
        range = range-split;\
        value = value-bigsplit;\
        val += ((uint16_t)1<<bits_count);\
    }\
    else\
    {\
        range = split;\
    }\
    NORMALIZE

int vp8_decode_mb_tokens(VP8D_COMP *dx, MACROBLOCKD *x)
{
    ENTROPY_CONTEXT *A = (ENTROPY_CONTEXT *)x->above_context;
    ENTROPY_CONTEXT *L = (ENTROPY_CONTEXT *)x->left_context;
    const FRAME_CONTEXT * const fc = &dx->common.fc;

    BOOL_DECODER *bc = x->current_bc;

    char *eobs = x->eobs;

    ENTROPY_CONTEXT *a;
    ENTROPY_CONTEXT *l;
    int i;

    int eobtotal = 0;

    register int count;

    const BOOL_DATA *bufptr;
    const BOOL_DATA *bufend;
    register unsigned int range;
    VP8_BD_VALUE value;
    const int *scan;
    register unsigned int shift;
    unsigned int split;
    VP8_BD_VALUE bigsplit;
    short *qcoeff_ptr;

    const vp8_prob *coef_probs;
    int stop;
    int val, bits_count;
    int c;
    int v;
    const vp8_prob *Prob;
    int start_coeff;


    i = 0;
    stop = 16;

    scan = vp8_default_zig_zag1d;
    qcoeff_ptr = &x->qcoeff[0];
    coef_probs = fc->coef_probs [3] [ 0 ] [0];

    if (x->mode_info_context->mbmi.mode != B_PRED &&
        x->mode_info_context->mbmi.mode != SPLITMV)
    {
        i = 24;
        stop = 24;
        qcoeff_ptr += 24*16;
        eobtotal -= 16;
        coef_probs = fc->coef_probs [1] [ 0 ] [0];
    }

    bufend  = bc->user_buffer_end;
    bufptr  = bc->user_buffer;
    value   = bc->value;
    count   = bc->count;
    range   = bc->range;

    start_coeff = 0;

BLOCK_LOOP:
    a = A + vp8_block2above[i];
    l = L + vp8_block2left[i];

    c = start_coeff;

    VP8_COMBINEENTROPYCONTEXTS(v, *a, *l);

    Prob = coef_probs;
    Prob += v * ENTROPY_NODES;
    *a = *l = 0;

DO_WHILE:
    Prob += coef_bands_x[c];
    DECODE_AND_BRANCH_IF_ZERO(Prob[EOB_CONTEXT_NODE], BLOCK_FINISHED);
    *a = *l = 1;

CHECK_0_:
    DECODE_AND_LOOP_IF_ZERO(Prob[ZERO_CONTEXT_NODE], CHECK_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[ONE_CONTEXT_NODE], ONE_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[LOW_VAL_CONTEXT_NODE],
                              LOW_VAL_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[HIGH_LOW_CONTEXT_NODE],
                              HIGH_LOW_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_THREEFOUR_CONTEXT_NODE],
                              CAT_THREEFOUR_CONTEXT_NODE_0_);
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_FIVE_CONTEXT_NODE],
                              CAT_FIVE_CONTEXT_NODE_0_);

    val = CAT6_MIN_VAL;
    bits_count = 10;

    do
    {
        DECODE_EXTRABIT_AND_ADJUST_VAL(cat6_prob[bits_count], bits_count);
        bits_count -- ;
    }
    while (bits_count >= 0);

    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_FIVE_CONTEXT_NODE_0_:
    val = CAT5_MIN_VAL;
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT5_PROB4, 4);
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT5_PROB3, 3);
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT5_PROB2, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT5_PROB1, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT5_PROB0, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_THREEFOUR_CONTEXT_NODE_0_:
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_THREE_CONTEXT_NODE],
                              CAT_THREE_CONTEXT_NODE_0_);
    val = CAT4_MIN_VAL;
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT4_PROB3, 3);
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT4_PROB2, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT4_PROB1, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT4_PROB0, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_THREE_CONTEXT_NODE_0_:
    val = CAT3_MIN_VAL;
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT3_PROB2, 2);
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT3_PROB1, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT3_PROB0, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

HIGH_LOW_CONTEXT_NODE_0_:
    DECODE_AND_BRANCH_IF_ZERO(Prob[CAT_ONE_CONTEXT_NODE],
                              CAT_ONE_CONTEXT_NODE_0_);

    val = CAT2_MIN_VAL;
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT2_PROB1, 1);
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT2_PROB0, 0);
    DECODE_SIGN_WRITE_COEFF_AND_CHECK_EXIT(val);

CAT_ONE_CONTEXT_NODE_0_:
    val = CAT1_MIN_VAL;
    DECODE_EXTRABIT_AND_ADJUST_VAL(CAT1_PROB0, 0);
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
        qcoeff_ptr [ scan[c] ] = (int16_t) v;
        ++c;
        goto DO_WHILE;
    }

    qcoeff_ptr [ 15 ] = (int16_t) v;
BLOCK_FINISHED:
    eobs[i] = c;
    eobtotal += c;
    qcoeff_ptr += 16;

    i++;

    if (i < stop)
        goto BLOCK_LOOP;

    if (i == 25)
    {
        start_coeff = 1;
        i = 0;
        stop = 16;
        coef_probs = fc->coef_probs [0] [ 0 ] [0];
        qcoeff_ptr -= (24*16 + 16);
        goto BLOCK_LOOP;
    }

    if (i == 16)
    {
        start_coeff = 0;
        coef_probs = fc->coef_probs [2] [ 0 ] [0];
        stop = 24;
        goto BLOCK_LOOP;
    }

    FILL
    bc->user_buffer = bufptr;
    bc->value = value;
    bc->count = count;
    bc->range = range;
    return eobtotal;

}
