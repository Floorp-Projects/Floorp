/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "entropymode.h"
#include "entropy.h"
#include "vpx_mem/vpx_mem.h"

static const unsigned int kf_y_mode_cts[VP8_YMODES] = { 1607, 915, 812, 811, 5455};
static const unsigned int y_mode_cts  [VP8_YMODES] = { 8080, 1908, 1582, 1007, 5874};

static const unsigned int uv_mode_cts  [VP8_UV_MODES] = { 59483, 13605, 16492, 4230};
static const unsigned int kf_uv_mode_cts[VP8_UV_MODES] = { 5319, 1904, 1703, 674};

static const unsigned int bmode_cts[VP8_BINTRAMODES] =
{
    43891, 17694, 10036, 3920, 3363, 2546, 5119, 3221, 2471, 1723
};

typedef enum
{
    SUBMVREF_NORMAL,
    SUBMVREF_LEFT_ZED,
    SUBMVREF_ABOVE_ZED,
    SUBMVREF_LEFT_ABOVE_SAME,
    SUBMVREF_LEFT_ABOVE_ZED
} sumvfref_t;

int vp8_mv_cont(const MV *l, const MV *a)
{
    int lez = (l->row == 0 && l->col == 0);
    int aez = (a->row == 0 && a->col == 0);
    int lea = (l->row == a->row && l->col == a->col);

    if (lea && lez)
        return SUBMVREF_LEFT_ABOVE_ZED;

    if (lea)
        return SUBMVREF_LEFT_ABOVE_SAME;

    if (aez)
        return SUBMVREF_ABOVE_ZED;

    if (lez)
        return SUBMVREF_LEFT_ZED;

    return SUBMVREF_NORMAL;
}

static const vp8_prob sub_mv_ref_prob [VP8_SUBMVREFS-1] = { 180, 162, 25};

const vp8_prob vp8_sub_mv_ref_prob2 [SUBMVREF_COUNT][VP8_SUBMVREFS-1] =
{
    { 147, 136, 18 },
    { 106, 145, 1  },
    { 179, 121, 1  },
    { 223, 1  , 34 },
    { 208, 1  , 1  }
};



vp8_mbsplit vp8_mbsplits [VP8_NUMMBSPLITS] =
{
    {
        0,  0,  0,  0,
        0,  0,  0,  0,
        1,  1,  1,  1,
        1,  1,  1,  1,
    },
    {
        0,  0,  1,  1,
        0,  0,  1,  1,
        0,  0,  1,  1,
        0,  0,  1,  1,
    },
    {
        0,  0,  1,  1,
        0,  0,  1,  1,
        2,  2,  3,  3,
        2,  2,  3,  3,
    },
    {
        0,  1,  2,  3,
        4,  5,  6,  7,
        8,  9,  10, 11,
        12, 13, 14, 15,
    },
};

const int vp8_mbsplit_count [VP8_NUMMBSPLITS] = { 2, 2, 4, 16};

const vp8_prob vp8_mbsplit_probs [VP8_NUMMBSPLITS-1] = { 110, 111, 150};


/* Array indices are identical to previously-existing INTRAMODECONTEXTNODES. */

const vp8_tree_index vp8_bmode_tree[18] =     /* INTRAMODECONTEXTNODE value */
{
    -B_DC_PRED, 2,                             /* 0 = DC_NODE */
    -B_TM_PRED, 4,                            /* 1 = TM_NODE */
    -B_VE_PRED, 6,                           /* 2 = VE_NODE */
    8, 12,                                  /* 3 = COM_NODE */
    -B_HE_PRED, 10,                        /* 4 = HE_NODE */
    -B_RD_PRED, -B_VR_PRED,               /* 5 = RD_NODE */
    -B_LD_PRED, 14,                        /* 6 = LD_NODE */
    -B_VL_PRED, 16,                      /* 7 = VL_NODE */
    -B_HD_PRED, -B_HU_PRED             /* 8 = HD_NODE */
};

/* Again, these trees use the same probability indices as their
   explicitly-programmed predecessors. */

const vp8_tree_index vp8_ymode_tree[8] =
{
    -DC_PRED, 2,
    4, 6,
    -V_PRED, -H_PRED,
    -TM_PRED, -B_PRED
};

const vp8_tree_index vp8_kf_ymode_tree[8] =
{
    -B_PRED, 2,
    4, 6,
    -DC_PRED, -V_PRED,
    -H_PRED, -TM_PRED
};

const vp8_tree_index vp8_uv_mode_tree[6] =
{
    -DC_PRED, 2,
    -V_PRED, 4,
    -H_PRED, -TM_PRED
};

const vp8_tree_index vp8_mbsplit_tree[6] =
{
    -3, 2,
    -2, 4,
    -0, -1
};

const vp8_tree_index vp8_mv_ref_tree[8] =
{
    -ZEROMV, 2,
    -NEARESTMV, 4,
    -NEARMV, 6,
    -NEWMV, -SPLITMV
};

const vp8_tree_index vp8_sub_mv_ref_tree[6] =
{
    -LEFT4X4, 2,
    -ABOVE4X4, 4,
    -ZERO4X4, -NEW4X4
};


struct vp8_token_struct vp8_bmode_encodings   [VP8_BINTRAMODES];
struct vp8_token_struct vp8_ymode_encodings   [VP8_YMODES];
struct vp8_token_struct vp8_kf_ymode_encodings [VP8_YMODES];
struct vp8_token_struct vp8_uv_mode_encodings  [VP8_UV_MODES];
struct vp8_token_struct vp8_mbsplit_encodings [VP8_NUMMBSPLITS];

struct vp8_token_struct vp8_mv_ref_encoding_array    [VP8_MVREFS];
struct vp8_token_struct vp8_sub_mv_ref_encoding_array [VP8_SUBMVREFS];


const vp8_tree_index vp8_small_mvtree [14] =
{
    2, 8,
    4, 6,
    -0, -1,
    -2, -3,
    10, 12,
    -4, -5,
    -6, -7
};

struct vp8_token_struct vp8_small_mvencodings [8];

void vp8_init_mbmode_probs(VP8_COMMON *x)
{
    unsigned int bct [VP8_YMODES] [2];      /* num Ymodes > num UV modes */

    vp8_tree_probs_from_distribution(
        VP8_YMODES, vp8_ymode_encodings, vp8_ymode_tree,
        x->fc.ymode_prob, bct, y_mode_cts,
        256, 1
    );
    vp8_tree_probs_from_distribution(
        VP8_YMODES, vp8_kf_ymode_encodings, vp8_kf_ymode_tree,
        x->kf_ymode_prob, bct, kf_y_mode_cts,
        256, 1
    );
    vp8_tree_probs_from_distribution(
        VP8_UV_MODES, vp8_uv_mode_encodings, vp8_uv_mode_tree,
        x->fc.uv_mode_prob, bct, uv_mode_cts,
        256, 1
    );
    vp8_tree_probs_from_distribution(
        VP8_UV_MODES, vp8_uv_mode_encodings, vp8_uv_mode_tree,
        x->kf_uv_mode_prob, bct, kf_uv_mode_cts,
        256, 1
    );
    vpx_memcpy(x->fc.sub_mv_ref_prob, sub_mv_ref_prob, sizeof(sub_mv_ref_prob));
}


static void intra_bmode_probs_from_distribution(
    vp8_prob p [VP8_BINTRAMODES-1],
    unsigned int branch_ct [VP8_BINTRAMODES-1] [2],
    const unsigned int events [VP8_BINTRAMODES]
)
{
    vp8_tree_probs_from_distribution(
        VP8_BINTRAMODES, vp8_bmode_encodings, vp8_bmode_tree,
        p, branch_ct, events,
        256, 1
    );
}

void vp8_default_bmode_probs(vp8_prob p [VP8_BINTRAMODES-1])
{
    unsigned int branch_ct [VP8_BINTRAMODES-1] [2];
    intra_bmode_probs_from_distribution(p, branch_ct, bmode_cts);
}

void vp8_kf_default_bmode_probs(vp8_prob p [VP8_BINTRAMODES] [VP8_BINTRAMODES] [VP8_BINTRAMODES-1])
{
    unsigned int branch_ct [VP8_BINTRAMODES-1] [2];

    int i = 0;

    do
    {
        int j = 0;

        do
        {
            intra_bmode_probs_from_distribution(
                p[i][j], branch_ct, vp8_kf_default_bmode_counts[i][j]);

        }
        while (++j < VP8_BINTRAMODES);
    }
    while (++i < VP8_BINTRAMODES);
}


void vp8_entropy_mode_init()
{
    vp8_tokens_from_tree(vp8_bmode_encodings,   vp8_bmode_tree);
    vp8_tokens_from_tree(vp8_ymode_encodings,   vp8_ymode_tree);
    vp8_tokens_from_tree(vp8_kf_ymode_encodings, vp8_kf_ymode_tree);
    vp8_tokens_from_tree(vp8_uv_mode_encodings,  vp8_uv_mode_tree);
    vp8_tokens_from_tree(vp8_mbsplit_encodings, vp8_mbsplit_tree);

    vp8_tokens_from_tree_offset(vp8_mv_ref_encoding_array,
                                vp8_mv_ref_tree, NEARESTMV);
    vp8_tokens_from_tree_offset(vp8_sub_mv_ref_encoding_array,
                                vp8_sub_mv_ref_tree, LEFT4X4);

    vp8_tokens_from_tree(vp8_small_mvencodings, vp8_small_mvtree);
}
