/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdio.h>

#include "entropy.h"
#include "string.h"
#include "blockd.h"
#include "onyxc_int.h"
#include "vpx_mem/vpx_mem.h"

#define uchar unsigned char     /* typedefs can clash */
#define uint  unsigned int

typedef const uchar cuchar;
typedef const uint cuint;

typedef vp8_prob Prob;

#include "coefupdateprobs.h"

DECLARE_ALIGNED(16, const unsigned char, vp8_norm[256]) =
{
    0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

DECLARE_ALIGNED(16, cuchar, vp8_coef_bands[16]) =
{ 0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7};

DECLARE_ALIGNED(16, cuchar, vp8_prev_token_class[MAX_ENTROPY_TOKENS]) =
{ 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0};

DECLARE_ALIGNED(16, const int, vp8_default_zig_zag1d[16]) =
{
    0,  1,  4,  8,
    5,  2,  3,  6,
    9, 12, 13, 10,
    7, 11, 14, 15,
};

DECLARE_ALIGNED(16, const short, vp8_default_inv_zig_zag[16]) =
{
    1,  2,  6,  7,
    3,  5,  8, 13,
    4,  9, 12, 14,
   10, 11, 15, 16
};

DECLARE_ALIGNED(16, short, vp8_default_zig_zag_mask[16]);

const int vp8_mb_feature_data_bits[MB_LVL_MAX] = {7, 6};

/* Array indices are identical to previously-existing CONTEXT_NODE indices */

const vp8_tree_index vp8_coef_tree[ 22] =     /* corresponding _CONTEXT_NODEs */
{
    -DCT_EOB_TOKEN, 2,                             /* 0 = EOB */
    -ZERO_TOKEN, 4,                               /* 1 = ZERO */
    -ONE_TOKEN, 6,                               /* 2 = ONE */
    8, 12,                                      /* 3 = LOW_VAL */
    -TWO_TOKEN, 10,                            /* 4 = TWO */
    -THREE_TOKEN, -FOUR_TOKEN,                /* 5 = THREE */
    14, 16,                                    /* 6 = HIGH_LOW */
    -DCT_VAL_CATEGORY1, -DCT_VAL_CATEGORY2,   /* 7 = CAT_ONE */
    18, 20,                                   /* 8 = CAT_THREEFOUR */
    -DCT_VAL_CATEGORY3, -DCT_VAL_CATEGORY4,  /* 9 = CAT_THREE */
    -DCT_VAL_CATEGORY5, -DCT_VAL_CATEGORY6   /* 10 = CAT_FIVE */
};

struct vp8_token_struct vp8_coef_encodings[MAX_ENTROPY_TOKENS];

/* Trees for extra bits.  Probabilities are constant and
   do not depend on previously encoded bits */

static const Prob Pcat1[] = { 159};
static const Prob Pcat2[] = { 165, 145};
static const Prob Pcat3[] = { 173, 148, 140};
static const Prob Pcat4[] = { 176, 155, 140, 135};
static const Prob Pcat5[] = { 180, 157, 141, 134, 130};
static const Prob Pcat6[] =
{ 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129};

static vp8_tree_index cat1[2], cat2[4], cat3[6], cat4[8], cat5[10], cat6[22];

void vp8_init_scan_order_mask()
{
    int i;

    for (i = 0; i < 16; i++)
    {
        vp8_default_zig_zag_mask[vp8_default_zig_zag1d[i]] = 1 << i;
    }

}

static void init_bit_tree(vp8_tree_index *p, int n)
{
    int i = 0;

    while (++i < n)
    {
        p[0] = p[1] = i << 1;
        p += 2;
    }

    p[0] = p[1] = 0;
}

static void init_bit_trees()
{
    init_bit_tree(cat1, 1);
    init_bit_tree(cat2, 2);
    init_bit_tree(cat3, 3);
    init_bit_tree(cat4, 4);
    init_bit_tree(cat5, 5);
    init_bit_tree(cat6, 11);
}

vp8_extra_bit_struct vp8_extra_bits[12] =
{
    { 0, 0, 0, 0},
    { 0, 0, 0, 1},
    { 0, 0, 0, 2},
    { 0, 0, 0, 3},
    { 0, 0, 0, 4},
    { cat1, Pcat1, 1, 5},
    { cat2, Pcat2, 2, 7},
    { cat3, Pcat3, 3, 11},
    { cat4, Pcat4, 4, 19},
    { cat5, Pcat5, 5, 35},
    { cat6, Pcat6, 11, 67},
    { 0, 0, 0, 0}
};

#include "default_coef_probs.h"

void vp8_default_coef_probs(VP8_COMMON *pc)
{
    vpx_memcpy(pc->fc.coef_probs, default_coef_probs,
                   sizeof(default_coef_probs));
}

void vp8_coef_tree_initialize()
{
    init_bit_trees();
    vp8_tokens_from_tree(vp8_coef_encodings, vp8_coef_tree);
}
