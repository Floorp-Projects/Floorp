/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vp9/common/vp9_entropy.h"
#include "vp9/common/vp9_blockd.h"
#include "vp9/common/vp9_onyxc_int.h"
#include "vp9/common/vp9_entropymode.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx/vpx_integer.h"

#define MODEL_NODES (ENTROPY_NODES - UNCONSTRAINED_NODES)

DECLARE_ALIGNED(16, const uint8_t, vp9_norm[256]) = {
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

DECLARE_ALIGNED(16, const uint8_t,
                vp9_coefband_trans_8x8plus[1024]) = {
  0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4,
  4, 4, 4, 4, 4, 5,
  // beyond MAXBAND_INDEX+1 all values are filled as 5
                    5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
};

DECLARE_ALIGNED(16, const uint8_t,
                vp9_coefband_trans_4x4[16]) = {
  0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 5,
};

DECLARE_ALIGNED(16, const uint8_t, vp9_pt_energy_class[MAX_ENTROPY_TOKENS]) = {
  0, 1, 2, 3, 3, 4, 4, 5, 5, 5, 5, 5
};



/* Array indices are identical to previously-existing CONTEXT_NODE indices */

const vp9_tree_index vp9_coef_tree[TREE_SIZE(MAX_ENTROPY_TOKENS)] = {
  -DCT_EOB_TOKEN, 2,                          /* 0 = EOB */
  -ZERO_TOKEN, 4,                             /* 1 = ZERO */
  -ONE_TOKEN, 6,                              /* 2 = ONE */
  8, 12,                                      /* 3 = LOW_VAL */
  -TWO_TOKEN, 10,                            /* 4 = TWO */
  -THREE_TOKEN, -FOUR_TOKEN,                /* 5 = THREE */
  14, 16,                                   /* 6 = HIGH_LOW */
  -DCT_VAL_CATEGORY1, -DCT_VAL_CATEGORY2,   /* 7 = CAT_ONE */
  18, 20,                                   /* 8 = CAT_THREEFOUR */
  -DCT_VAL_CATEGORY3, -DCT_VAL_CATEGORY4,   /* 9 = CAT_THREE */
  -DCT_VAL_CATEGORY5, -DCT_VAL_CATEGORY6    /* 10 = CAT_FIVE */
};

struct vp9_token vp9_coef_encodings[MAX_ENTROPY_TOKENS];

/* Trees for extra bits.  Probabilities are constant and
   do not depend on previously encoded bits */

static const vp9_prob Pcat1[] = { 159};
static const vp9_prob Pcat2[] = { 165, 145};
static const vp9_prob Pcat3[] = { 173, 148, 140};
static const vp9_prob Pcat4[] = { 176, 155, 140, 135};
static const vp9_prob Pcat5[] = { 180, 157, 141, 134, 130};
static const vp9_prob Pcat6[] = {
  254, 254, 254, 252, 249, 243, 230, 196, 177, 153, 140, 133, 130, 129
};

const vp9_tree_index vp9_coefmodel_tree[6] = {
  -DCT_EOB_MODEL_TOKEN, 2,                      /* 0 = EOB */
  -ZERO_TOKEN, 4,                               /* 1 = ZERO */
  -ONE_TOKEN, -TWO_TOKEN,
};

// Model obtained from a 2-sided zero-centerd distribuition derived
// from a Pareto distribution. The cdf of the distribution is:
// cdf(x) = 0.5 + 0.5 * sgn(x) * [1 - {alpha/(alpha + |x|)} ^ beta]
//
// For a given beta and a given probablity of the 1-node, the alpha
// is first solved, and then the {alpha, beta} pair is used to generate
// the probabilities for the rest of the nodes.

// beta = 8
static const vp9_prob modelcoefprobs_pareto8[COEFPROB_MODELS][MODEL_NODES] = {
  {  3,  86, 128,   6,  86,  23,  88,  29},
  {  9,  86, 129,  17,  88,  61,  94,  76},
  { 15,  87, 129,  28,  89,  93, 100, 110},
  { 20,  88, 130,  38,  91, 118, 106, 136},
  { 26,  89, 131,  48,  92, 139, 111, 156},
  { 31,  90, 131,  58,  94, 156, 117, 171},
  { 37,  90, 132,  66,  95, 171, 122, 184},
  { 42,  91, 132,  75,  97, 183, 127, 194},
  { 47,  92, 133,  83,  98, 193, 132, 202},
  { 52,  93, 133,  90, 100, 201, 137, 208},
  { 57,  94, 134,  98, 101, 208, 142, 214},
  { 62,  94, 135, 105, 103, 214, 146, 218},
  { 66,  95, 135, 111, 104, 219, 151, 222},
  { 71,  96, 136, 117, 106, 224, 155, 225},
  { 76,  97, 136, 123, 107, 227, 159, 228},
  { 80,  98, 137, 129, 109, 231, 162, 231},
  { 84,  98, 138, 134, 110, 234, 166, 233},
  { 89,  99, 138, 140, 112, 236, 170, 235},
  { 93, 100, 139, 145, 113, 238, 173, 236},
  { 97, 101, 140, 149, 115, 240, 176, 238},
  {101, 102, 140, 154, 116, 242, 179, 239},
  {105, 103, 141, 158, 118, 243, 182, 240},
  {109, 104, 141, 162, 119, 244, 185, 241},
  {113, 104, 142, 166, 120, 245, 187, 242},
  {116, 105, 143, 170, 122, 246, 190, 243},
  {120, 106, 143, 173, 123, 247, 192, 244},
  {123, 107, 144, 177, 125, 248, 195, 244},
  {127, 108, 145, 180, 126, 249, 197, 245},
  {130, 109, 145, 183, 128, 249, 199, 245},
  {134, 110, 146, 186, 129, 250, 201, 246},
  {137, 111, 147, 189, 131, 251, 203, 246},
  {140, 112, 147, 192, 132, 251, 205, 247},
  {143, 113, 148, 194, 133, 251, 207, 247},
  {146, 114, 149, 197, 135, 252, 208, 248},
  {149, 115, 149, 199, 136, 252, 210, 248},
  {152, 115, 150, 201, 138, 252, 211, 248},
  {155, 116, 151, 204, 139, 253, 213, 249},
  {158, 117, 151, 206, 140, 253, 214, 249},
  {161, 118, 152, 208, 142, 253, 216, 249},
  {163, 119, 153, 210, 143, 253, 217, 249},
  {166, 120, 153, 212, 144, 254, 218, 250},
  {168, 121, 154, 213, 146, 254, 220, 250},
  {171, 122, 155, 215, 147, 254, 221, 250},
  {173, 123, 155, 217, 148, 254, 222, 250},
  {176, 124, 156, 218, 150, 254, 223, 250},
  {178, 125, 157, 220, 151, 254, 224, 251},
  {180, 126, 157, 221, 152, 254, 225, 251},
  {183, 127, 158, 222, 153, 254, 226, 251},
  {185, 128, 159, 224, 155, 255, 227, 251},
  {187, 129, 160, 225, 156, 255, 228, 251},
  {189, 131, 160, 226, 157, 255, 228, 251},
  {191, 132, 161, 227, 159, 255, 229, 251},
  {193, 133, 162, 228, 160, 255, 230, 252},
  {195, 134, 163, 230, 161, 255, 231, 252},
  {197, 135, 163, 231, 162, 255, 231, 252},
  {199, 136, 164, 232, 163, 255, 232, 252},
  {201, 137, 165, 233, 165, 255, 233, 252},
  {202, 138, 166, 233, 166, 255, 233, 252},
  {204, 139, 166, 234, 167, 255, 234, 252},
  {206, 140, 167, 235, 168, 255, 235, 252},
  {207, 141, 168, 236, 169, 255, 235, 252},
  {209, 142, 169, 237, 171, 255, 236, 252},
  {210, 144, 169, 237, 172, 255, 236, 252},
  {212, 145, 170, 238, 173, 255, 237, 252},
  {214, 146, 171, 239, 174, 255, 237, 253},
  {215, 147, 172, 240, 175, 255, 238, 253},
  {216, 148, 173, 240, 176, 255, 238, 253},
  {218, 149, 173, 241, 177, 255, 239, 253},
  {219, 150, 174, 241, 179, 255, 239, 253},
  {220, 152, 175, 242, 180, 255, 240, 253},
  {222, 153, 176, 242, 181, 255, 240, 253},
  {223, 154, 177, 243, 182, 255, 240, 253},
  {224, 155, 178, 244, 183, 255, 241, 253},
  {225, 156, 178, 244, 184, 255, 241, 253},
  {226, 158, 179, 244, 185, 255, 242, 253},
  {228, 159, 180, 245, 186, 255, 242, 253},
  {229, 160, 181, 245, 187, 255, 242, 253},
  {230, 161, 182, 246, 188, 255, 243, 253},
  {231, 163, 183, 246, 189, 255, 243, 253},
  {232, 164, 184, 247, 190, 255, 243, 253},
  {233, 165, 185, 247, 191, 255, 244, 253},
  {234, 166, 185, 247, 192, 255, 244, 253},
  {235, 168, 186, 248, 193, 255, 244, 253},
  {236, 169, 187, 248, 194, 255, 244, 253},
  {236, 170, 188, 248, 195, 255, 245, 253},
  {237, 171, 189, 249, 196, 255, 245, 254},
  {238, 173, 190, 249, 197, 255, 245, 254},
  {239, 174, 191, 249, 198, 255, 245, 254},
  {240, 175, 192, 249, 199, 255, 246, 254},
  {240, 177, 193, 250, 200, 255, 246, 254},
  {241, 178, 194, 250, 201, 255, 246, 254},
  {242, 179, 195, 250, 202, 255, 246, 254},
  {242, 181, 196, 250, 203, 255, 247, 254},
  {243, 182, 197, 251, 204, 255, 247, 254},
  {244, 184, 198, 251, 205, 255, 247, 254},
  {244, 185, 199, 251, 206, 255, 247, 254},
  {245, 186, 200, 251, 207, 255, 247, 254},
  {246, 188, 201, 252, 207, 255, 248, 254},
  {246, 189, 202, 252, 208, 255, 248, 254},
  {247, 191, 203, 252, 209, 255, 248, 254},
  {247, 192, 204, 252, 210, 255, 248, 254},
  {248, 194, 205, 252, 211, 255, 248, 254},
  {248, 195, 206, 252, 212, 255, 249, 254},
  {249, 197, 207, 253, 213, 255, 249, 254},
  {249, 198, 208, 253, 214, 255, 249, 254},
  {250, 200, 210, 253, 215, 255, 249, 254},
  {250, 201, 211, 253, 215, 255, 249, 254},
  {250, 203, 212, 253, 216, 255, 249, 254},
  {251, 204, 213, 253, 217, 255, 250, 254},
  {251, 206, 214, 254, 218, 255, 250, 254},
  {252, 207, 216, 254, 219, 255, 250, 254},
  {252, 209, 217, 254, 220, 255, 250, 254},
  {252, 211, 218, 254, 221, 255, 250, 254},
  {253, 213, 219, 254, 222, 255, 250, 254},
  {253, 214, 221, 254, 223, 255, 250, 254},
  {253, 216, 222, 254, 224, 255, 251, 254},
  {253, 218, 224, 254, 225, 255, 251, 254},
  {254, 220, 225, 254, 225, 255, 251, 254},
  {254, 222, 227, 255, 226, 255, 251, 254},
  {254, 224, 228, 255, 227, 255, 251, 254},
  {254, 226, 230, 255, 228, 255, 251, 254},
  {255, 228, 231, 255, 230, 255, 251, 254},
  {255, 230, 233, 255, 231, 255, 252, 254},
  {255, 232, 235, 255, 232, 255, 252, 254},
  {255, 235, 237, 255, 233, 255, 252, 254},
  {255, 238, 240, 255, 235, 255, 252, 255},
  {255, 241, 243, 255, 236, 255, 252, 254},
  {255, 246, 247, 255, 239, 255, 253, 255}
};

static void extend_model_to_full_distribution(vp9_prob p,
                                              vp9_prob *tree_probs) {
  const int l = (p - 1) / 2;
  const vp9_prob (*model)[MODEL_NODES] = modelcoefprobs_pareto8;
  if (p & 1) {
    vpx_memcpy(tree_probs + UNCONSTRAINED_NODES,
               model[l], MODEL_NODES * sizeof(vp9_prob));
  } else {
    // interpolate
    int i;
    for (i = UNCONSTRAINED_NODES; i < ENTROPY_NODES; ++i)
      tree_probs[i] = (model[l][i - UNCONSTRAINED_NODES] +
                       model[l + 1][i - UNCONSTRAINED_NODES]) >> 1;
  }
}

void vp9_model_to_full_probs(const vp9_prob *model, vp9_prob *full) {
  if (full != model)
    vpx_memcpy(full, model, sizeof(vp9_prob) * UNCONSTRAINED_NODES);
  extend_model_to_full_distribution(model[PIVOT_NODE], full);
}

static vp9_tree_index cat1[2], cat2[4], cat3[6], cat4[8], cat5[10], cat6[28];

static void init_bit_tree(vp9_tree_index *p, int n) {
  int i = 0;

  while (++i < n) {
    p[0] = p[1] = i << 1;
    p += 2;
  }

  p[0] = p[1] = 0;
}

static void init_bit_trees() {
  init_bit_tree(cat1, 1);
  init_bit_tree(cat2, 2);
  init_bit_tree(cat3, 3);
  init_bit_tree(cat4, 4);
  init_bit_tree(cat5, 5);
  init_bit_tree(cat6, 14);
}

const vp9_extra_bit vp9_extra_bits[MAX_ENTROPY_TOKENS] = {
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
  { cat6, Pcat6, 14, 67},
  { 0, 0, 0, 0}
};

#include "vp9/common/vp9_default_coef_probs.h"

void vp9_default_coef_probs(VP9_COMMON *cm) {
  vp9_copy(cm->fc.coef_probs[TX_4X4], default_coef_probs_4x4);
  vp9_copy(cm->fc.coef_probs[TX_8X8], default_coef_probs_8x8);
  vp9_copy(cm->fc.coef_probs[TX_16X16], default_coef_probs_16x16);
  vp9_copy(cm->fc.coef_probs[TX_32X32], default_coef_probs_32x32);
}

void vp9_coef_tree_initialize() {
  init_bit_trees();
  vp9_tokens_from_tree(vp9_coef_encodings, vp9_coef_tree);
}

// #define COEF_COUNT_TESTING

#define COEF_COUNT_SAT 24
#define COEF_MAX_UPDATE_FACTOR 112
#define COEF_COUNT_SAT_KEY 24
#define COEF_MAX_UPDATE_FACTOR_KEY 112
#define COEF_COUNT_SAT_AFTER_KEY 24
#define COEF_MAX_UPDATE_FACTOR_AFTER_KEY 128

static void adapt_coef_probs(VP9_COMMON *cm, TX_SIZE tx_size,
                             unsigned int count_sat,
                             unsigned int update_factor) {
  const FRAME_CONTEXT *pre_fc = &cm->frame_contexts[cm->frame_context_idx];

  vp9_coeff_probs_model *dst_coef_probs = cm->fc.coef_probs[tx_size];
  const vp9_coeff_probs_model *pre_coef_probs = pre_fc->coef_probs[tx_size];
  vp9_coeff_count_model *coef_counts = cm->counts.coef[tx_size];
  unsigned int (*eob_branch_count)[REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS] =
      cm->counts.eob_branch[tx_size];
  int i, j, k, l, m;
  unsigned int branch_ct[UNCONSTRAINED_NODES][2];

  for (i = 0; i < BLOCK_TYPES; ++i)
    for (j = 0; j < REF_TYPES; ++j)
      for (k = 0; k < COEF_BANDS; ++k)
        for (l = 0; l < PREV_COEF_CONTEXTS; ++l) {
          if (l >= 3 && k == 0)
            continue;
          vp9_tree_probs_from_distribution(vp9_coefmodel_tree, branch_ct,
                                           coef_counts[i][j][k][l]);
          branch_ct[0][1] = eob_branch_count[i][j][k][l] - branch_ct[0][0];
          for (m = 0; m < UNCONSTRAINED_NODES; ++m)
            dst_coef_probs[i][j][k][l][m] = merge_probs(
                                                pre_coef_probs[i][j][k][l][m],
                                                branch_ct[m],
                                                count_sat, update_factor);
        }
}

void vp9_adapt_coef_probs(VP9_COMMON *cm) {
  TX_SIZE t;
  unsigned int count_sat, update_factor;

  if (frame_is_intra_only(cm)) {
    update_factor = COEF_MAX_UPDATE_FACTOR_KEY;
    count_sat = COEF_COUNT_SAT_KEY;
  } else if (cm->last_frame_type == KEY_FRAME) {
    update_factor = COEF_MAX_UPDATE_FACTOR_AFTER_KEY;  /* adapt quickly */
    count_sat = COEF_COUNT_SAT_AFTER_KEY;
  } else {
    update_factor = COEF_MAX_UPDATE_FACTOR;
    count_sat = COEF_COUNT_SAT;
  }
  for (t = TX_4X4; t <= TX_32X32; t++)
    adapt_coef_probs(cm, t, count_sat, update_factor);
}
