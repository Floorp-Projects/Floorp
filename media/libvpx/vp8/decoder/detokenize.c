/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
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
#include "detokenize.h"

#define BOOL_DATA UINT8

#define OCB_X PREV_COEF_CONTEXTS * ENTROPY_NODES
DECLARE_ALIGNED(16, UINT8, vp8_coef_bands_x[16]) = { 0, 1 * OCB_X, 2 * OCB_X, 3 * OCB_X, 6 * OCB_X, 4 * OCB_X, 5 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 6 * OCB_X, 7 * OCB_X};
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
    {  0, -1, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },  /* ZERO_TOKEN */
    {  1, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   /* ONE_TOKEN */
    {  2, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   /* TWO_TOKEN */
    {  3, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   /* THREE_TOKEN */
    {  4, 0, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },   /* FOUR_TOKEN */
    {  5, 0, { 159, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },  /* DCT_VAL_CATEGORY1 */
    {  7, 1, { 145, 165, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } }, /* DCT_VAL_CATEGORY2 */
    { 11, 2, { 140, 148, 173, 0,  0,  0,  0,  0,  0,  0,  0,  0   } }, /* DCT_VAL_CATEGORY3 */
    { 19, 3, { 135, 140, 155, 176, 0,  0,  0,  0,  0,  0,  0,  0   } }, /* DCT_VAL_CATEGORY4 */
    { 35, 4, { 130, 134, 141, 157, 180, 0,  0,  0,  0,  0,  0,  0   } }, /* DCT_VAL_CATEGORY5 */
    { 67, 10, { 129, 130, 133, 140, 153, 177, 196, 230, 243, 254, 254, 0   } }, /* DCT_VAL_CATEGORY6 */
    {  0, -1, { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0   } },  /*  EOB TOKEN */
};


void vp8_reset_mb_tokens_context(MACROBLOCKD *x)
{
    /* Clear entropy contexts for Y2 blocks */
    if (x->mode_info_context->mbmi.mode != B_PRED && x->mode_info_context->mbmi.mode != SPLITMV)
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

#if CONFIG_ARM_ASM_DETOK
/* mashup of vp8_block2left and vp8_block2above so we only need one pointer
 * for the assembly version.
 */
DECLARE_ALIGNED(16, const UINT8, vp8_block2leftabove[25*2]) =
{
    /* vp8_block2left */
    0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8,
    /* vp8_block2above */
    0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 4, 5, 6, 7, 6, 7, 8
};

void vp8_init_detokenizer(VP8D_COMP *dx)
{
    const VP8_COMMON *const oc = & dx->common;
    MACROBLOCKD *x = & dx->mb;

    dx->detoken.vp8_coef_tree_ptr = vp8_coef_tree;
    dx->detoken.ptr_block2leftabove = vp8_block2leftabove;
    dx->detoken.ptr_coef_bands_x = vp8_coef_bands_x;
    dx->detoken.scan = vp8_default_zig_zag1d;
    dx->detoken.teb_base_ptr = vp8d_token_extra_bits2;
    dx->detoken.qcoeff_start_ptr = &x->qcoeff[0];

    dx->detoken.coef_probs[0] = (oc->fc.coef_probs [0] [ 0 ] [0]);
    dx->detoken.coef_probs[1] = (oc->fc.coef_probs [1] [ 0 ] [0]);
    dx->detoken.coef_probs[2] = (oc->fc.coef_probs [2] [ 0 ] [0]);
    dx->detoken.coef_probs[3] = (oc->fc.coef_probs [3] [ 0 ] [0]);
}
#endif

DECLARE_ALIGNED(16, extern const unsigned char, vp8dx_bitreader_norm[256]);
#define FILL \
    if(count < 0) \
        VP8DX_BOOL_DECODER_FILL(count, value, bufptr, bufend);

#define NORMALIZE \
    /*if(range < 0x80)*/                            \
    { \
        shift = vp8dx_bitreader_norm[range]; \
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
            Prob += vp8_coef_bands_x[c]; \
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
        qcoeff_ptr [ scan[c] ] = (INT16) v; \
        ++c; \
        goto DO_WHILE; }\
    qcoeff_ptr [ scan[15] ] = (INT16) v; \
    goto BLOCK_FINISHED;


#define DECODE_EXTRABIT_AND_ADJUST_VAL(t,bits_count)\
    split = 1 +  (((range-1) * vp8d_token_extra_bits2[t].Probs[bits_count]) >> 8); \
    bigsplit = (VP8_BD_VALUE)split << (VP8_BD_VALUE_SIZE - 8); \
    FILL \
    if(value >= bigsplit)\
    {\
        range = range-split;\
        value = value-bigsplit;\
        val += ((UINT16)1<<bits_count);\
    }\
    else\
    {\
        range = split;\
    }\
    NORMALIZE

#if CONFIG_ARM_ASM_DETOK
int vp8_decode_mb_tokens(VP8D_COMP *dx, MACROBLOCKD *x)
{
    int eobtotal = 0;
    int i, type;

    dx->detoken.current_bc = x->current_bc;
    dx->detoken.A = x->above_context;
    dx->detoken.L = x->left_context;

    type = 3;

    if (x->mode_info_context->mbmi.mode != B_PRED && x->mode_info_context->mbmi.mode != SPLITMV)
    {
        type = 1;
        eobtotal -= 16;
    }

    vp8_decode_mb_tokens_v6(&dx->detoken, type);

    for (i = 0; i < 25; i++)
    {
        x->eobs[i] = dx->detoken.eob[i];
        eobtotal += dx->detoken.eob[i];
    }

    return eobtotal;
}
#else
int vp8_decode_mb_tokens(VP8D_COMP *dx, MACROBLOCKD *x)
{
    ENTROPY_CONTEXT *A = (ENTROPY_CONTEXT *)x->above_context;
    ENTROPY_CONTEXT *L = (ENTROPY_CONTEXT *)x->left_context;
    const VP8_COMMON *const oc = & dx->common;

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
    UINT32 split;
    VP8_BD_VALUE bigsplit;
    INT16 *qcoeff_ptr;

    const vp8_prob *coef_probs;
    int type;
    int stop;
    INT16 val, bits_count;
    INT16 c;
    INT16 v;
    const vp8_prob *Prob;

    type = 3;
    i = 0;
    stop = 16;

    scan = vp8_default_zig_zag1d;
    qcoeff_ptr = &x->qcoeff[0];

    if (x->mode_info_context->mbmi.mode != B_PRED && x->mode_info_context->mbmi.mode != SPLITMV)
    {
        i = 24;
        stop = 24;
        type = 1;
        qcoeff_ptr += 24*16;
        eobtotal -= 16;
    }

    bufend  = bc->user_buffer_end;
    bufptr  = bc->user_buffer;
    value   = bc->value;
    count   = bc->count;
    range   = bc->range;


    coef_probs = oc->fc.coef_probs [type] [ 0 ] [0];

BLOCK_LOOP:
    a = A + vp8_block2above[i];
    l = L + vp8_block2left[i];

    c = (INT16)(!type);

    /*Dest = ((A)!=0) + ((B)!=0);*/
    VP8_COMBINEENTROPYCONTEXTS(v, *a, *l);
    Prob = coef_probs;
    Prob += v * ENTROPY_NODES;

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
    *a = *l = ((eobs[i] = c) != !type);   /* any nonzero data? */
    eobtotal += c;
    qcoeff_ptr += 16;

    i++;

    if (i < stop)
        goto BLOCK_LOOP;

    if (i == 25)
    {
        type = 0;
        i = 0;
        stop = 16;
        coef_probs = oc->fc.coef_probs [type] [ 0 ] [0];
        qcoeff_ptr -= (24*16 + 16);
        goto BLOCK_LOOP;
    }

    if (i == 16)
    {
        type = 2;
        coef_probs = oc->fc.coef_probs [type] [ 0 ] [0];
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
#endif /*!CONFIG_ASM_DETOK*/
