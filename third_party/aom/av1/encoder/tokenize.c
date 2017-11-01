/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "aom_mem/aom_mem.h"

#include "av1/common/entropy.h"
#include "av1/common/pred_common.h"
#include "av1/common/scan.h"
#include "av1/common/seg_common.h"

#include "av1/encoder/cost.h"
#include "av1/encoder/encoder.h"
#if CONFIG_LV_MAP
#include "av1/encoder/encodetxb.c"
#endif
#include "av1/encoder/rdopt.h"
#include "av1/encoder/tokenize.h"

static const TOKENVALUE dct_cat_lt_10_value_tokens[] = {
  { 9, 63 }, { 9, 61 }, { 9, 59 }, { 9, 57 }, { 9, 55 }, { 9, 53 }, { 9, 51 },
  { 9, 49 }, { 9, 47 }, { 9, 45 }, { 9, 43 }, { 9, 41 }, { 9, 39 }, { 9, 37 },
  { 9, 35 }, { 9, 33 }, { 9, 31 }, { 9, 29 }, { 9, 27 }, { 9, 25 }, { 9, 23 },
  { 9, 21 }, { 9, 19 }, { 9, 17 }, { 9, 15 }, { 9, 13 }, { 9, 11 }, { 9, 9 },
  { 9, 7 },  { 9, 5 },  { 9, 3 },  { 9, 1 },  { 8, 31 }, { 8, 29 }, { 8, 27 },
  { 8, 25 }, { 8, 23 }, { 8, 21 }, { 8, 19 }, { 8, 17 }, { 8, 15 }, { 8, 13 },
  { 8, 11 }, { 8, 9 },  { 8, 7 },  { 8, 5 },  { 8, 3 },  { 8, 1 },  { 7, 15 },
  { 7, 13 }, { 7, 11 }, { 7, 9 },  { 7, 7 },  { 7, 5 },  { 7, 3 },  { 7, 1 },
  { 6, 7 },  { 6, 5 },  { 6, 3 },  { 6, 1 },  { 5, 3 },  { 5, 1 },  { 4, 1 },
  { 3, 1 },  { 2, 1 },  { 1, 1 },  { 0, 0 },  { 1, 0 },  { 2, 0 },  { 3, 0 },
  { 4, 0 },  { 5, 0 },  { 5, 2 },  { 6, 0 },  { 6, 2 },  { 6, 4 },  { 6, 6 },
  { 7, 0 },  { 7, 2 },  { 7, 4 },  { 7, 6 },  { 7, 8 },  { 7, 10 }, { 7, 12 },
  { 7, 14 }, { 8, 0 },  { 8, 2 },  { 8, 4 },  { 8, 6 },  { 8, 8 },  { 8, 10 },
  { 8, 12 }, { 8, 14 }, { 8, 16 }, { 8, 18 }, { 8, 20 }, { 8, 22 }, { 8, 24 },
  { 8, 26 }, { 8, 28 }, { 8, 30 }, { 9, 0 },  { 9, 2 },  { 9, 4 },  { 9, 6 },
  { 9, 8 },  { 9, 10 }, { 9, 12 }, { 9, 14 }, { 9, 16 }, { 9, 18 }, { 9, 20 },
  { 9, 22 }, { 9, 24 }, { 9, 26 }, { 9, 28 }, { 9, 30 }, { 9, 32 }, { 9, 34 },
  { 9, 36 }, { 9, 38 }, { 9, 40 }, { 9, 42 }, { 9, 44 }, { 9, 46 }, { 9, 48 },
  { 9, 50 }, { 9, 52 }, { 9, 54 }, { 9, 56 }, { 9, 58 }, { 9, 60 }, { 9, 62 }
};
const TOKENVALUE *av1_dct_cat_lt_10_value_tokens =
    dct_cat_lt_10_value_tokens +
    (sizeof(dct_cat_lt_10_value_tokens) / sizeof(*dct_cat_lt_10_value_tokens)) /
        2;
// The corresponding costs of the extrabits for the tokens in the above table
// are stored in the table below. The values are obtained from looking up the
// entry for the specified extrabits in the table corresponding to the token
// (as defined in cost element av1_extra_bits)
// e.g. {9, 63} maps to cat5_cost[63 >> 1], {1, 1} maps to sign_cost[1 >> 1]
static const int dct_cat_lt_10_value_cost[] = {
  3773, 3750, 3704, 3681, 3623, 3600, 3554, 3531, 3432, 3409, 3363, 3340, 3282,
  3259, 3213, 3190, 3136, 3113, 3067, 3044, 2986, 2963, 2917, 2894, 2795, 2772,
  2726, 2703, 2645, 2622, 2576, 2553, 3197, 3116, 3058, 2977, 2881, 2800, 2742,
  2661, 2615, 2534, 2476, 2395, 2299, 2218, 2160, 2079, 2566, 2427, 2334, 2195,
  2023, 1884, 1791, 1652, 1893, 1696, 1453, 1256, 1229, 864,  512,  512,  512,
  512,  0,    512,  512,  512,  512,  864,  1229, 1256, 1453, 1696, 1893, 1652,
  1791, 1884, 2023, 2195, 2334, 2427, 2566, 2079, 2160, 2218, 2299, 2395, 2476,
  2534, 2615, 2661, 2742, 2800, 2881, 2977, 3058, 3116, 3197, 2553, 2576, 2622,
  2645, 2703, 2726, 2772, 2795, 2894, 2917, 2963, 2986, 3044, 3067, 3113, 3136,
  3190, 3213, 3259, 3282, 3340, 3363, 3409, 3432, 3531, 3554, 3600, 3623, 3681,
  3704, 3750, 3773,
};
const int *av1_dct_cat_lt_10_value_cost =
    dct_cat_lt_10_value_cost +
    (sizeof(dct_cat_lt_10_value_cost) / sizeof(*dct_cat_lt_10_value_cost)) / 2;

// Array indices are identical to previously-existing CONTEXT_NODE indices
/* clang-format off */
const aom_tree_index av1_coef_tree[TREE_SIZE(ENTROPY_TOKENS)] = {
  -EOB_TOKEN, 2,                       // 0  = EOB
  -ZERO_TOKEN, 4,                      // 1  = ZERO
  -ONE_TOKEN, 6,                       // 2  = ONE
  8, 12,                               // 3  = LOW_VAL
  -TWO_TOKEN, 10,                      // 4  = TWO
  -THREE_TOKEN, -FOUR_TOKEN,           // 5  = THREE
  14, 16,                              // 6  = HIGH_LOW
  -CATEGORY1_TOKEN, -CATEGORY2_TOKEN,  // 7  = CAT_ONE
  18, 20,                              // 8  = CAT_THREEFOUR
  -CATEGORY3_TOKEN, -CATEGORY4_TOKEN,  // 9  = CAT_THREE
  -CATEGORY5_TOKEN, -CATEGORY6_TOKEN   // 10 = CAT_FIVE
};
/* clang-format on */

static const int16_t zero_cost[] = { 0 };
static const int16_t sign_cost[1] = { 512 };
static const int16_t cat1_cost[1 << 1] = { 864, 1229 };
static const int16_t cat2_cost[1 << 2] = { 1256, 1453, 1696, 1893 };
static const int16_t cat3_cost[1 << 3] = { 1652, 1791, 1884, 2023,
                                           2195, 2334, 2427, 2566 };
static const int16_t cat4_cost[1 << 4] = { 2079, 2160, 2218, 2299, 2395, 2476,
                                           2534, 2615, 2661, 2742, 2800, 2881,
                                           2977, 3058, 3116, 3197 };
static const int16_t cat5_cost[1 << 5] = {
  2553, 2576, 2622, 2645, 2703, 2726, 2772, 2795, 2894, 2917, 2963,
  2986, 3044, 3067, 3113, 3136, 3190, 3213, 3259, 3282, 3340, 3363,
  3409, 3432, 3531, 3554, 3600, 3623, 3681, 3704, 3750, 3773
};
const int16_t av1_cat6_low_cost[256] = {
  3378, 3390, 3401, 3413, 3435, 3447, 3458, 3470, 3517, 3529, 3540, 3552, 3574,
  3586, 3597, 3609, 3671, 3683, 3694, 3706, 3728, 3740, 3751, 3763, 3810, 3822,
  3833, 3845, 3867, 3879, 3890, 3902, 3973, 3985, 3996, 4008, 4030, 4042, 4053,
  4065, 4112, 4124, 4135, 4147, 4169, 4181, 4192, 4204, 4266, 4278, 4289, 4301,
  4323, 4335, 4346, 4358, 4405, 4417, 4428, 4440, 4462, 4474, 4485, 4497, 4253,
  4265, 4276, 4288, 4310, 4322, 4333, 4345, 4392, 4404, 4415, 4427, 4449, 4461,
  4472, 4484, 4546, 4558, 4569, 4581, 4603, 4615, 4626, 4638, 4685, 4697, 4708,
  4720, 4742, 4754, 4765, 4777, 4848, 4860, 4871, 4883, 4905, 4917, 4928, 4940,
  4987, 4999, 5010, 5022, 5044, 5056, 5067, 5079, 5141, 5153, 5164, 5176, 5198,
  5210, 5221, 5233, 5280, 5292, 5303, 5315, 5337, 5349, 5360, 5372, 4988, 5000,
  5011, 5023, 5045, 5057, 5068, 5080, 5127, 5139, 5150, 5162, 5184, 5196, 5207,
  5219, 5281, 5293, 5304, 5316, 5338, 5350, 5361, 5373, 5420, 5432, 5443, 5455,
  5477, 5489, 5500, 5512, 5583, 5595, 5606, 5618, 5640, 5652, 5663, 5675, 5722,
  5734, 5745, 5757, 5779, 5791, 5802, 5814, 5876, 5888, 5899, 5911, 5933, 5945,
  5956, 5968, 6015, 6027, 6038, 6050, 6072, 6084, 6095, 6107, 5863, 5875, 5886,
  5898, 5920, 5932, 5943, 5955, 6002, 6014, 6025, 6037, 6059, 6071, 6082, 6094,
  6156, 6168, 6179, 6191, 6213, 6225, 6236, 6248, 6295, 6307, 6318, 6330, 6352,
  6364, 6375, 6387, 6458, 6470, 6481, 6493, 6515, 6527, 6538, 6550, 6597, 6609,
  6620, 6632, 6654, 6666, 6677, 6689, 6751, 6763, 6774, 6786, 6808, 6820, 6831,
  6843, 6890, 6902, 6913, 6925, 6947, 6959, 6970, 6982
};
const int av1_cat6_high_cost[CAT6_HIGH_COST_ENTRIES] = {
  100,   2263,  2739,  4902,  3160,  5323,  5799,  7962,  3678,  5841,  6317,
  8480,  6738,  8901,  9377,  11540, 3678,  5841,  6317,  8480,  6738,  8901,
  9377,  11540, 7256,  9419,  9895,  12058, 10316, 12479, 12955, 15118, 3678,
  5841,  6317,  8480,  6738,  8901,  9377,  11540, 7256,  9419,  9895,  12058,
  10316, 12479, 12955, 15118, 7256,  9419,  9895,  12058, 10316, 12479, 12955,
  15118, 10834, 12997, 13473, 15636, 13894, 16057, 16533, 18696,
#if CONFIG_HIGHBITDEPTH
  4193,  6356,  6832,  8995,  7253,  9416,  9892,  12055, 7771,  9934,  10410,
  12573, 10831, 12994, 13470, 15633, 7771,  9934,  10410, 12573, 10831, 12994,
  13470, 15633, 11349, 13512, 13988, 16151, 14409, 16572, 17048, 19211, 7771,
  9934,  10410, 12573, 10831, 12994, 13470, 15633, 11349, 13512, 13988, 16151,
  14409, 16572, 17048, 19211, 11349, 13512, 13988, 16151, 14409, 16572, 17048,
  19211, 14927, 17090, 17566, 19729, 17987, 20150, 20626, 22789, 4193,  6356,
  6832,  8995,  7253,  9416,  9892,  12055, 7771,  9934,  10410, 12573, 10831,
  12994, 13470, 15633, 7771,  9934,  10410, 12573, 10831, 12994, 13470, 15633,
  11349, 13512, 13988, 16151, 14409, 16572, 17048, 19211, 7771,  9934,  10410,
  12573, 10831, 12994, 13470, 15633, 11349, 13512, 13988, 16151, 14409, 16572,
  17048, 19211, 11349, 13512, 13988, 16151, 14409, 16572, 17048, 19211, 14927,
  17090, 17566, 19729, 17987, 20150, 20626, 22789, 8286,  10449, 10925, 13088,
  11346, 13509, 13985, 16148, 11864, 14027, 14503, 16666, 14924, 17087, 17563,
  19726, 11864, 14027, 14503, 16666, 14924, 17087, 17563, 19726, 15442, 17605,
  18081, 20244, 18502, 20665, 21141, 23304, 11864, 14027, 14503, 16666, 14924,
  17087, 17563, 19726, 15442, 17605, 18081, 20244, 18502, 20665, 21141, 23304,
  15442, 17605, 18081, 20244, 18502, 20665, 21141, 23304, 19020, 21183, 21659,
  23822, 22080, 24243, 24719, 26882, 4193,  6356,  6832,  8995,  7253,  9416,
  9892,  12055, 7771,  9934,  10410, 12573, 10831, 12994, 13470, 15633, 7771,
  9934,  10410, 12573, 10831, 12994, 13470, 15633, 11349, 13512, 13988, 16151,
  14409, 16572, 17048, 19211, 7771,  9934,  10410, 12573, 10831, 12994, 13470,
  15633, 11349, 13512, 13988, 16151, 14409, 16572, 17048, 19211, 11349, 13512,
  13988, 16151, 14409, 16572, 17048, 19211, 14927, 17090, 17566, 19729, 17987,
  20150, 20626, 22789, 8286,  10449, 10925, 13088, 11346, 13509, 13985, 16148,
  11864, 14027, 14503, 16666, 14924, 17087, 17563, 19726, 11864, 14027, 14503,
  16666, 14924, 17087, 17563, 19726, 15442, 17605, 18081, 20244, 18502, 20665,
  21141, 23304, 11864, 14027, 14503, 16666, 14924, 17087, 17563, 19726, 15442,
  17605, 18081, 20244, 18502, 20665, 21141, 23304, 15442, 17605, 18081, 20244,
  18502, 20665, 21141, 23304, 19020, 21183, 21659, 23822, 22080, 24243, 24719,
  26882, 8286,  10449, 10925, 13088, 11346, 13509, 13985, 16148, 11864, 14027,
  14503, 16666, 14924, 17087, 17563, 19726, 11864, 14027, 14503, 16666, 14924,
  17087, 17563, 19726, 15442, 17605, 18081, 20244, 18502, 20665, 21141, 23304,
  11864, 14027, 14503, 16666, 14924, 17087, 17563, 19726, 15442, 17605, 18081,
  20244, 18502, 20665, 21141, 23304, 15442, 17605, 18081, 20244, 18502, 20665,
  21141, 23304, 19020, 21183, 21659, 23822, 22080, 24243, 24719, 26882, 12379,
  14542, 15018, 17181, 15439, 17602, 18078, 20241, 15957, 18120, 18596, 20759,
  19017, 21180, 21656, 23819, 15957, 18120, 18596, 20759, 19017, 21180, 21656,
  23819, 19535, 21698, 22174, 24337, 22595, 24758, 25234, 27397, 15957, 18120,
  18596, 20759, 19017, 21180, 21656, 23819, 19535, 21698, 22174, 24337, 22595,
  24758, 25234, 27397, 19535, 21698, 22174, 24337, 22595, 24758, 25234, 27397,
  23113, 25276, 25752, 27915, 26173, 28336, 28812, 30975, 4193,  6356,  6832,
  8995,  7253,  9416,  9892,  12055, 7771,  9934,  10410, 12573, 10831, 12994,
  13470, 15633, 7771,  9934,  10410, 12573, 10831, 12994, 13470, 15633, 11349,
  13512, 13988, 16151, 14409, 16572, 17048, 19211, 7771,  9934,  10410, 12573,
  10831, 12994, 13470, 15633, 11349, 13512, 13988, 16151, 14409, 16572, 17048,
  19211, 11349, 13512, 13988, 16151, 14409, 16572, 17048, 19211, 14927, 17090,
  17566, 19729, 17987, 20150, 20626, 22789, 8286,  10449, 10925, 13088, 11346,
  13509, 13985, 16148, 11864, 14027, 14503, 16666, 14924, 17087, 17563, 19726,
  11864, 14027, 14503, 16666, 14924, 17087, 17563, 19726, 15442, 17605, 18081,
  20244, 18502, 20665, 21141, 23304, 11864, 14027, 14503, 16666, 14924, 17087,
  17563, 19726, 15442, 17605, 18081, 20244, 18502, 20665, 21141, 23304, 15442,
  17605, 18081, 20244, 18502, 20665, 21141, 23304, 19020, 21183, 21659, 23822,
  22080, 24243, 24719, 26882, 8286,  10449, 10925, 13088, 11346, 13509, 13985,
  16148, 11864, 14027, 14503, 16666, 14924, 17087, 17563, 19726, 11864, 14027,
  14503, 16666, 14924, 17087, 17563, 19726, 15442, 17605, 18081, 20244, 18502,
  20665, 21141, 23304, 11864, 14027, 14503, 16666, 14924, 17087, 17563, 19726,
  15442, 17605, 18081, 20244, 18502, 20665, 21141, 23304, 15442, 17605, 18081,
  20244, 18502, 20665, 21141, 23304, 19020, 21183, 21659, 23822, 22080, 24243,
  24719, 26882, 12379, 14542, 15018, 17181, 15439, 17602, 18078, 20241, 15957,
  18120, 18596, 20759, 19017, 21180, 21656, 23819, 15957, 18120, 18596, 20759,
  19017, 21180, 21656, 23819, 19535, 21698, 22174, 24337, 22595, 24758, 25234,
  27397, 15957, 18120, 18596, 20759, 19017, 21180, 21656, 23819, 19535, 21698,
  22174, 24337, 22595, 24758, 25234, 27397, 19535, 21698, 22174, 24337, 22595,
  24758, 25234, 27397, 23113, 25276, 25752, 27915, 26173, 28336, 28812, 30975,
  8286,  10449, 10925, 13088, 11346, 13509, 13985, 16148, 11864, 14027, 14503,
  16666, 14924, 17087, 17563, 19726, 11864, 14027, 14503, 16666, 14924, 17087,
  17563, 19726, 15442, 17605, 18081, 20244, 18502, 20665, 21141, 23304, 11864,
  14027, 14503, 16666, 14924, 17087, 17563, 19726, 15442, 17605, 18081, 20244,
  18502, 20665, 21141, 23304, 15442, 17605, 18081, 20244, 18502, 20665, 21141,
  23304, 19020, 21183, 21659, 23822, 22080, 24243, 24719, 26882, 12379, 14542,
  15018, 17181, 15439, 17602, 18078, 20241, 15957, 18120, 18596, 20759, 19017,
  21180, 21656, 23819, 15957, 18120, 18596, 20759, 19017, 21180, 21656, 23819,
  19535, 21698, 22174, 24337, 22595, 24758, 25234, 27397, 15957, 18120, 18596,
  20759, 19017, 21180, 21656, 23819, 19535, 21698, 22174, 24337, 22595, 24758,
  25234, 27397, 19535, 21698, 22174, 24337, 22595, 24758, 25234, 27397, 23113,
  25276, 25752, 27915, 26173, 28336, 28812, 30975, 12379, 14542, 15018, 17181,
  15439, 17602, 18078, 20241, 15957, 18120, 18596, 20759, 19017, 21180, 21656,
  23819, 15957, 18120, 18596, 20759, 19017, 21180, 21656, 23819, 19535, 21698,
  22174, 24337, 22595, 24758, 25234, 27397, 15957, 18120, 18596, 20759, 19017,
  21180, 21656, 23819, 19535, 21698, 22174, 24337, 22595, 24758, 25234, 27397,
  19535, 21698, 22174, 24337, 22595, 24758, 25234, 27397, 23113, 25276, 25752,
  27915, 26173, 28336, 28812, 30975, 16472, 18635, 19111, 21274, 19532, 21695,
  22171, 24334, 20050, 22213, 22689, 24852, 23110, 25273, 25749, 27912, 20050,
  22213, 22689, 24852, 23110, 25273, 25749, 27912, 23628, 25791, 26267, 28430,
  26688, 28851, 29327, 31490, 20050, 22213, 22689, 24852, 23110, 25273, 25749,
  27912, 23628, 25791, 26267, 28430, 26688, 28851, 29327, 31490, 23628, 25791,
  26267, 28430, 26688, 28851, 29327, 31490, 27206, 29369, 29845, 32008, 30266,
  32429, 32905, 35068
#endif
};

const uint8_t av1_cat6_skipped_bits_discount[8] = {
  0, 3, 6, 9, 12, 18, 24, 30
};

#if CONFIG_NEW_MULTISYMBOL
const av1_extra_bit av1_extra_bits[ENTROPY_TOKENS] = {
  { 0, 0, 0, zero_cost },                        // ZERO_TOKEN
  { 0, 0, 1, sign_cost },                        // ONE_TOKEN
  { 0, 0, 2, sign_cost },                        // TWO_TOKEN
  { 0, 0, 3, sign_cost },                        // THREE_TOKEN
  { 0, 0, 4, sign_cost },                        // FOUR_TOKEN
  { av1_cat1_cdf, 1, CAT1_MIN_VAL, cat1_cost },  // CATEGORY1_TOKEN
  { av1_cat2_cdf, 2, CAT2_MIN_VAL, cat2_cost },  // CATEGORY2_TOKEN
  { av1_cat3_cdf, 3, CAT3_MIN_VAL, cat3_cost },  // CATEGORY3_TOKEN
  { av1_cat4_cdf, 4, CAT4_MIN_VAL, cat4_cost },  // CATEGORY4_TOKEN
  { av1_cat5_cdf, 5, CAT5_MIN_VAL, cat5_cost },  // CATEGORY5_TOKEN
  { av1_cat6_cdf, 18, CAT6_MIN_VAL, 0 },         // CATEGORY6_TOKEN
  { 0, 0, 0, zero_cost }                         // EOB_TOKEN
};
#else
const av1_extra_bit av1_extra_bits[ENTROPY_TOKENS] = {
  { 0, 0, 0, zero_cost },                         // ZERO_TOKEN
  { 0, 0, 1, sign_cost },                         // ONE_TOKEN
  { 0, 0, 2, sign_cost },                         // TWO_TOKEN
  { 0, 0, 3, sign_cost },                         // THREE_TOKEN
  { 0, 0, 4, sign_cost },                         // FOUR_TOKEN
  { av1_cat1_prob, 1, CAT1_MIN_VAL, cat1_cost },  // CATEGORY1_TOKEN
  { av1_cat2_prob, 2, CAT2_MIN_VAL, cat2_cost },  // CATEGORY2_TOKEN
  { av1_cat3_prob, 3, CAT3_MIN_VAL, cat3_cost },  // CATEGORY3_TOKEN
  { av1_cat4_prob, 4, CAT4_MIN_VAL, cat4_cost },  // CATEGORY4_TOKEN
  { av1_cat5_prob, 5, CAT5_MIN_VAL, cat5_cost },  // CATEGORY5_TOKEN
  { av1_cat6_prob, 18, CAT6_MIN_VAL, 0 },         // CATEGORY6_TOKEN
  { 0, 0, 0, zero_cost }                          // EOB_TOKEN
};
#endif

#if !CONFIG_PVQ || CONFIG_VAR_TX
static void cost_coeffs_b(int plane, int block, int blk_row, int blk_col,
                          BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *const cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const PLANE_TYPE type = pd->plane_type;
  const TX_TYPE tx_type =
      av1_get_tx_type(type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const int rate = av1_cost_coeffs(
      cpi, x, plane, blk_row, blk_col, block, tx_size, scan_order,
      pd->above_context + blk_col, pd->left_context + blk_row, 0);
  args->this_rate += rate;
  (void)plane_bsize;
  av1_set_contexts(xd, pd, plane, tx_size, p->eobs[block] > 0, blk_col,
                   blk_row);
}

static void set_entropy_context_b(int plane, int block, int blk_row,
                                  int blk_col, BLOCK_SIZE plane_bsize,
                                  TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args *const args = arg;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  (void)plane_bsize;
  av1_set_contexts(xd, pd, plane, tx_size, p->eobs[block] > 0, blk_col,
                   blk_row);
}

static INLINE void add_token(TOKENEXTRA **t,
                             aom_cdf_prob (*tail_cdf)[CDF_SIZE(ENTROPY_TOKENS)],
                             aom_cdf_prob (*head_cdf)[CDF_SIZE(ENTROPY_TOKENS)],
                             int eob_val, int first_val, int32_t extra,
                             uint8_t token) {
  (*t)->token = token;
  (*t)->extra = extra;
  (*t)->tail_cdf = tail_cdf;
  (*t)->head_cdf = head_cdf;
  (*t)->eob_val = eob_val;
  (*t)->first_val = first_val;
  (*t)++;

  if (token == BLOCK_Z_TOKEN) {
    update_cdf(*head_cdf, 0, HEAD_TOKENS + 1);
  } else {
    if (eob_val != LAST_EOB) {
      const int symb = 2 * AOMMIN(token, TWO_TOKEN) - eob_val + first_val;
      update_cdf(*head_cdf, symb, HEAD_TOKENS + first_val);
    }
    if (token > ONE_TOKEN)
      update_cdf(*tail_cdf, token - TWO_TOKEN, TAIL_TOKENS);
  }
}
#endif  // !CONFIG_PVQ || CONFIG_VAR_TX

static int cost_and_tokenize_map(Av1ColorMapParam *param, TOKENEXTRA **t,
                                 int calc_rate) {
  const uint8_t *const color_map = param->color_map;
  MapCdf map_cdf = param->map_cdf;
  ColorCost color_cost = param->color_cost;
  const int plane_block_width = param->plane_width;
  const int rows = param->rows;
  const int cols = param->cols;
  const int n = param->n_colors;

  int this_rate = 0;
  uint8_t color_order[PALETTE_MAX_SIZE];
#if CONFIG_PALETTE_THROUGHPUT
  for (int k = 1; k < rows + cols - 1; ++k) {
    for (int j = AOMMIN(k, cols - 1); j >= AOMMAX(0, k - rows + 1); --j) {
      int i = k - j;
#else
  for (int i = 0; i < rows; ++i) {
    for (int j = (i == 0 ? 1 : 0); j < cols; ++j) {
#endif  // CONFIG_PALETTE_THROUGHPUT
      int color_new_idx;
      const int color_ctx = av1_get_palette_color_index_context(
          color_map, plane_block_width, i, j, n, color_order, &color_new_idx);
      assert(color_new_idx >= 0 && color_new_idx < n);
      if (calc_rate) {
        this_rate +=
            (*color_cost)[n - PALETTE_MIN_SIZE][color_ctx][color_new_idx];
      } else {
        (*t)->token = color_new_idx;
        (*t)->color_map_cdf = map_cdf[n - PALETTE_MIN_SIZE][color_ctx];
        ++(*t);
      }
    }
  }
  if (calc_rate) return this_rate;
  return 0;
}

static void get_palette_params(const MACROBLOCK *const x, int plane,
                               BLOCK_SIZE bsize, Av1ColorMapParam *params) {
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  params->color_map = xd->plane[plane].color_index_map;
  params->map_cdf = plane ? xd->tile_ctx->palette_uv_color_index_cdf
                          : xd->tile_ctx->palette_y_color_index_cdf;
  params->color_cost =
      plane ? &x->palette_uv_color_cost : &x->palette_y_color_cost;
  params->n_colors = pmi->palette_size[plane];
  av1_get_block_dimensions(bsize, plane, xd, &params->plane_width, NULL,
                           &params->rows, &params->cols);
}

#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
static void get_mrc_params(const MACROBLOCK *const x, int block,
                           TX_SIZE tx_size, Av1ColorMapParam *params) {
  memset(params, 0, sizeof(*params));
  const MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int is_inter = is_inter_block(mbmi);
  params->color_map = BLOCK_OFFSET(xd->mrc_mask, block);
  params->map_cdf = is_inter ? xd->tile_ctx->mrc_mask_inter_cdf
                             : xd->tile_ctx->mrc_mask_intra_cdf;
  params->color_cost =
      is_inter ? &x->mrc_mask_inter_cost : &x->mrc_mask_intra_cost;
  params->n_colors = 2;
  params->plane_width = tx_size_wide[tx_size];
  params->rows = tx_size_high[tx_size];
  params->cols = tx_size_wide[tx_size];
}
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK

static void get_color_map_params(const MACROBLOCK *const x, int plane,
                                 int block, BLOCK_SIZE bsize, TX_SIZE tx_size,
                                 COLOR_MAP_TYPE type,
                                 Av1ColorMapParam *params) {
  (void)block;
  (void)tx_size;
  memset(params, 0, sizeof(*params));
  switch (type) {
    case PALETTE_MAP: get_palette_params(x, plane, bsize, params); break;
#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
    case MRC_MAP: get_mrc_params(x, block, tx_size, params); break;
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
    default: assert(0 && "Invalid color map type"); return;
  }
}

int av1_cost_color_map(const MACROBLOCK *const x, int plane, int block,
                       BLOCK_SIZE bsize, TX_SIZE tx_size, COLOR_MAP_TYPE type) {
  assert(plane == 0 || plane == 1);
  Av1ColorMapParam color_map_params;
  get_color_map_params(x, plane, block, bsize, tx_size, type,
                       &color_map_params);
  return cost_and_tokenize_map(&color_map_params, NULL, 1);
}

void av1_tokenize_color_map(const MACROBLOCK *const x, int plane, int block,
                            TOKENEXTRA **t, BLOCK_SIZE bsize, TX_SIZE tx_size,
                            COLOR_MAP_TYPE type) {
  assert(plane == 0 || plane == 1);
#if CONFIG_MRC_TX
  if (type == MRC_MAP) {
    const int is_inter = is_inter_block(&x->e_mbd.mi[0]->mbmi);
    if ((is_inter && !SIGNAL_MRC_MASK_INTER) ||
        (!is_inter && !SIGNAL_MRC_MASK_INTRA))
      return;
  }
#endif  // CONFIG_MRC_TX
  Av1ColorMapParam color_map_params;
  get_color_map_params(x, plane, block, bsize, tx_size, type,
                       &color_map_params);
  // The first color index does not use context or entropy.
  (*t)->token = color_map_params.color_map[0];
  (*t)->color_map_cdf = NULL;
  ++(*t);
  cost_and_tokenize_map(&color_map_params, t, 0);
}

#if CONFIG_PVQ
static void add_pvq_block(AV1_COMMON *const cm, MACROBLOCK *const x,
                          PVQ_INFO *pvq) {
  PVQ_QUEUE *q = x->pvq_q;
  if (q->curr_pos >= q->buf_len) {
    int new_buf_len = 2 * q->buf_len + 1;
    PVQ_INFO *new_buf;
    CHECK_MEM_ERROR(cm, new_buf, aom_malloc(new_buf_len * sizeof(PVQ_INFO)));
    memcpy(new_buf, q->buf, q->buf_len * sizeof(PVQ_INFO));
    aom_free(q->buf);
    q->buf = new_buf;
    q->buf_len = new_buf_len;
  }
  OD_COPY(q->buf + q->curr_pos, pvq, 1);
  ++q->curr_pos;
}

// NOTE: This does not actually generate tokens, instead we store the encoding
// decisions made for PVQ in a queue that we will read from when
// actually writing the bitstream in write_modes_b
static void tokenize_pvq(int plane, int block, int blk_row, int blk_col,
                         BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *const cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  PVQ_INFO *pvq_info;

  (void)block;
  (void)blk_row;
  (void)blk_col;
  (void)plane_bsize;
  (void)tx_size;

  assert(block < MAX_PVQ_BLOCKS_IN_SB);
  pvq_info = &x->pvq[block][plane];
  add_pvq_block((AV1_COMMON * const) cm, x, pvq_info);
}
#endif  // CONFIG_PVQ

static void tokenize_b(int plane, int block, int blk_row, int blk_col,
                       BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg) {
#if !CONFIG_PVQ
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *const cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  TOKENEXTRA **tp = args->tp;
  uint8_t token_cache[MAX_TX_SQUARE];
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  int pt; /* near block/prev token context index */
  int c;
  TOKENEXTRA *t = *tp; /* store tokens starting here */
  const int eob = p->eobs[block];
  const PLANE_TYPE type = pd->plane_type;
  const tran_low_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
#if CONFIG_SUPERTX
  const int segment_id = AOMMIN(mbmi->segment_id, mbmi->segment_id_supertx);
#else
  const int segment_id = mbmi->segment_id;
#endif  // CONFIG_SUEPRTX
  const int16_t *scan, *nb;
  const TX_TYPE tx_type =
      av1_get_tx_type(type, xd, blk_row, blk_col, block, tx_size);
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const int ref = is_inter_block(mbmi);
  FRAME_CONTEXT *ec_ctx = xd->tile_ctx;
  aom_cdf_prob(
      *const coef_head_cdfs)[COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)] =
      ec_ctx->coef_head_cdfs[txsize_sqr_map[tx_size]][type][ref];
  aom_cdf_prob(
      *const coef_tail_cdfs)[COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)] =
      ec_ctx->coef_tail_cdfs[txsize_sqr_map[tx_size]][type][ref];
  int eob_val;
  int first_val = 1;
  const int seg_eob = av1_get_tx_eob(&cpi->common.seg, segment_id, tx_size);
  const uint8_t *const band = get_band_translate(tx_size);
  int16_t token;
  EXTRABIT extra;
  (void)plane_bsize;
  pt = get_entropy_context(tx_size, pd->above_context + blk_col,
                           pd->left_context + blk_row);
  scan = scan_order->scan;
  nb = scan_order->neighbors;
  c = 0;

#if CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK
  if (tx_type == MRC_DCT)
    av1_tokenize_color_map(x, plane, block, &t, plane_bsize, tx_size, MRC_MAP);
#endif  // CONFIG_MRC_TX && SIGNAL_ANY_MRC_MASK

  if (eob == 0)
    add_token(&t, &coef_tail_cdfs[band[c]][pt], &coef_head_cdfs[band[c]][pt], 1,
              1, 0, BLOCK_Z_TOKEN);

  while (c < eob) {
    int v = qcoeff[scan[c]];
    first_val = (c == 0);

    if (!v) {
      add_token(&t, &coef_tail_cdfs[band[c]][pt], &coef_head_cdfs[band[c]][pt],
                0, first_val, 0, ZERO_TOKEN);
      token_cache[scan[c]] = 0;
    } else {
      eob_val =
          (c + 1 == eob) ? (c + 1 == seg_eob ? LAST_EOB : EARLY_EOB) : NO_EOB;
      av1_get_token_extra(v, &token, &extra);
      add_token(&t, &coef_tail_cdfs[band[c]][pt], &coef_head_cdfs[band[c]][pt],
                eob_val, first_val, extra, (uint8_t)token);
      token_cache[scan[c]] = av1_pt_energy_class[token];
    }
    ++c;
    pt = get_coef_context(nb, token_cache, AOMMIN(c, eob - 1));
  }

#if CONFIG_COEF_INTERLEAVE
  t->token = EOSB_TOKEN;
  t++;
#endif

  *tp = t;

#if CONFIG_ADAPT_SCAN
  // Since dqcoeff is not available here, we pass qcoeff into
  // av1_update_scan_count_facade(). The update behavior should be the same
  // because av1_update_scan_count_facade() only cares if coefficients are zero
  // or not.
  av1_update_scan_count_facade((AV1_COMMON *)cm, td->counts, tx_size, tx_type,
                               qcoeff, c);
#endif

  av1_set_contexts(xd, pd, plane, tx_size, c > 0, blk_col, blk_row);
#else   // !CONFIG_PVQ
  tokenize_pvq(plane, block, blk_row, blk_col, plane_bsize, tx_size, arg);
#endif  // !CONFIG_PVQ
}

struct is_skippable_args {
  uint16_t *eobs;
  int *skippable;
};
static void is_skippable(int plane, int block, int blk_row, int blk_col,
                         BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *argv) {
  struct is_skippable_args *args = argv;
  (void)plane;
  (void)plane_bsize;
  (void)tx_size;
  (void)blk_row;
  (void)blk_col;
  args->skippable[0] &= (!args->eobs[block]);
}

// TODO(yaowu): rewrite and optimize this function to remove the usage of
//              av1_foreach_transform_block() and simplify is_skippable().
int av1_is_skippable_in_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane) {
  int result = 1;
  struct is_skippable_args args = { x->plane[plane].eobs, &result };
  av1_foreach_transformed_block_in_plane(&x->e_mbd, bsize, plane, is_skippable,
                                         &args);
  return result;
}

#if CONFIG_VAR_TX
void tokenize_vartx(ThreadData *td, TOKENEXTRA **t, RUN_TYPE dry_run,
                    TX_SIZE tx_size, BLOCK_SIZE plane_bsize, int blk_row,
                    int blk_col, int block, int plane, void *arg) {
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const BLOCK_SIZE bsize = txsize_to_bsize[tx_size];
  const int tx_row = blk_row >> (1 - pd->subsampling_y);
  const int tx_col = blk_col >> (1 - pd->subsampling_x);
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  TX_SIZE plane_tx_size;

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  plane_tx_size =
      plane ? uv_txsize_lookup[bsize][mbmi->inter_tx_size[tx_row][tx_col]][0][0]
            : mbmi->inter_tx_size[tx_row][tx_col];

  if (tx_size == plane_tx_size) {
    plane_bsize = get_plane_block_size(mbmi->sb_type, pd);
#if CONFIG_LV_MAP
    if (!dry_run) {
      av1_update_and_record_txb_context(plane, block, blk_row, blk_col,
                                        plane_bsize, tx_size, arg);
    } else if (dry_run == DRY_RUN_NORMAL) {
      av1_update_txb_context_b(plane, block, blk_row, blk_col, plane_bsize,
                               tx_size, arg);
    } else {
      printf("DRY_RUN_COSTCOEFFS is not supported yet\n");
      assert(0);
    }
#else
    if (!dry_run)
      tokenize_b(plane, block, blk_row, blk_col, plane_bsize, tx_size, arg);
    else if (dry_run == DRY_RUN_NORMAL)
      set_entropy_context_b(plane, block, blk_row, blk_col, plane_bsize,
                            tx_size, arg);
    else if (dry_run == DRY_RUN_COSTCOEFFS)
      cost_coeffs_b(plane, block, blk_row, blk_col, plane_bsize, tx_size, arg);
#endif
  } else {
#if CONFIG_RECT_TX_EXT
    int is_qttx = plane_tx_size == quarter_txsize_lookup[plane_bsize];
    const TX_SIZE sub_txs = is_qttx ? plane_tx_size : sub_tx_size_map[tx_size];
#else
    // Half the block size in transform block unit.
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
#endif
    const int bsl = tx_size_wide_unit[sub_txs];
    int i;

    assert(bsl > 0);

    for (i = 0; i < 4; ++i) {
#if CONFIG_RECT_TX_EXT
      int is_wide_tx = tx_size_wide_unit[sub_txs] > tx_size_high_unit[sub_txs];
      const int offsetr =
          is_qttx ? (is_wide_tx ? i * tx_size_high_unit[sub_txs] : 0)
                  : blk_row + ((i >> 1) * bsl);
      const int offsetc =
          is_qttx ? (is_wide_tx ? 0 : i * tx_size_wide_unit[sub_txs])
                  : blk_col + ((i & 0x01) * bsl);
#else
      const int offsetr = blk_row + ((i >> 1) * bsl);
      const int offsetc = blk_col + ((i & 0x01) * bsl);
#endif

      int step = tx_size_wide_unit[sub_txs] * tx_size_high_unit[sub_txs];

      if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;

      tokenize_vartx(td, t, dry_run, sub_txs, plane_bsize, offsetr, offsetc,
                     block, plane, arg);
      block += step;
    }
  }
}

void av1_tokenize_sb_vartx(const AV1_COMP *cpi, ThreadData *td, TOKENEXTRA **t,
                           RUN_TYPE dry_run, int mi_row, int mi_col,
                           BLOCK_SIZE bsize, int *rate) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
#if CONFIG_LV_MAP
  (void)t;
#else
  TOKENEXTRA *t_backup = *t;
#endif
  const int ctx = av1_get_skip_context(xd);
  const int skip_inc =
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP);
  struct tokenize_b_args arg = { cpi, td, t, 0 };
  int plane;
  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  if (mbmi->skip) {
    if (!dry_run) td->counts->skip[ctx][1] += skip_inc;
    av1_reset_skip_context(xd, mi_row, mi_col, bsize);
#if !CONFIG_LV_MAP
    if (dry_run) *t = t_backup;
#endif
    return;
  }

  if (!dry_run) td->counts->skip[ctx][0] += skip_inc;
#if !CONFIG_LV_MAP
  else
    *t = t_backup;
#endif

  for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
#if CONFIG_CB4X4
    if (!is_chroma_reference(mi_row, mi_col, bsize,
                             xd->plane[plane].subsampling_x,
                             xd->plane[plane].subsampling_y)) {
#if !CONFIG_PVQ && !CONFIG_LV_MAP
      if (!dry_run) {
        (*t)->token = EOSB_TOKEN;
        (*t)++;
      }
#endif
      continue;
    }
#endif
    const struct macroblockd_plane *const pd = &xd->plane[plane];
#if CONFIG_CHROMA_SUB8X8
    const BLOCK_SIZE plane_bsize =
        AOMMAX(BLOCK_4X4, get_plane_block_size(bsize, pd));
#else
    const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#endif
    const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
    const int mi_height = block_size_high[plane_bsize] >> tx_size_wide_log2[0];
    const TX_SIZE max_tx_size = get_vartx_max_txsize(
        mbmi, plane_bsize, pd->subsampling_x || pd->subsampling_y);
    const BLOCK_SIZE txb_size = txsize_to_bsize[max_tx_size];
    int bw = block_size_wide[txb_size] >> tx_size_wide_log2[0];
    int bh = block_size_high[txb_size] >> tx_size_wide_log2[0];
    int idx, idy;
    int block = 0;
    int step = tx_size_wide_unit[max_tx_size] * tx_size_high_unit[max_tx_size];

    const BLOCK_SIZE max_unit_bsize = get_plane_block_size(BLOCK_64X64, pd);
    int mu_blocks_wide =
        block_size_wide[max_unit_bsize] >> tx_size_wide_log2[0];
    int mu_blocks_high =
        block_size_high[max_unit_bsize] >> tx_size_high_log2[0];

    mu_blocks_wide = AOMMIN(mi_width, mu_blocks_wide);
    mu_blocks_high = AOMMIN(mi_height, mu_blocks_high);

    for (idy = 0; idy < mi_height; idy += mu_blocks_high) {
      for (idx = 0; idx < mi_width; idx += mu_blocks_wide) {
        int blk_row, blk_col;
        const int unit_height = AOMMIN(mu_blocks_high + idy, mi_height);
        const int unit_width = AOMMIN(mu_blocks_wide + idx, mi_width);
        for (blk_row = idy; blk_row < unit_height; blk_row += bh) {
          for (blk_col = idx; blk_col < unit_width; blk_col += bw) {
            tokenize_vartx(td, t, dry_run, max_tx_size, plane_bsize, blk_row,
                           blk_col, block, plane, &arg);
            block += step;
          }
        }
      }
    }
#if !CONFIG_LV_MAP
    if (!dry_run) {
      (*t)->token = EOSB_TOKEN;
      (*t)++;
    }
#endif
  }
  if (rate) *rate += arg.this_rate;
}
#endif  // CONFIG_VAR_TX

void av1_tokenize_sb(const AV1_COMP *cpi, ThreadData *td, TOKENEXTRA **t,
                     RUN_TYPE dry_run, BLOCK_SIZE bsize, int *rate,
                     const int mi_row, const int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int ctx = av1_get_skip_context(xd);
  const int skip_inc =
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP);
  struct tokenize_b_args arg = { cpi, td, t, 0 };
  if (mbmi->skip) {
    if (!dry_run) td->counts->skip[ctx][1] += skip_inc;
    av1_reset_skip_context(xd, mi_row, mi_col, bsize);
    return;
  }

  if (!dry_run) {
#if CONFIG_COEF_INTERLEAVE
    td->counts->skip[ctx][0] += skip_inc;
    av1_foreach_transformed_block_interleave(xd, bsize, tokenize_b, &arg);
#else
    int plane;

    td->counts->skip[ctx][0] += skip_inc;
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
#if CONFIG_CB4X4
      if (!is_chroma_reference(mi_row, mi_col, bsize,
                               xd->plane[plane].subsampling_x,
                               xd->plane[plane].subsampling_y)) {
#if !CONFIG_PVQ
        (*t)->token = EOSB_TOKEN;
        (*t)++;
#endif
        continue;
      }
#else
      (void)mi_row;
      (void)mi_col;
#endif
      av1_foreach_transformed_block_in_plane(xd, bsize, plane, tokenize_b,
                                             &arg);
#if !CONFIG_PVQ
      (*t)->token = EOSB_TOKEN;
      (*t)++;
#endif  // !CONFIG_PVQ
    }
#endif
  }
#if !CONFIG_PVQ
  else if (dry_run == DRY_RUN_NORMAL) {
    int plane;
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
#if CONFIG_CB4X4
      if (!is_chroma_reference(mi_row, mi_col, bsize,
                               xd->plane[plane].subsampling_x,
                               xd->plane[plane].subsampling_y))
        continue;
#else
      (void)mi_row;
      (void)mi_col;
#endif
      av1_foreach_transformed_block_in_plane(xd, bsize, plane,
                                             set_entropy_context_b, &arg);
    }
  } else if (dry_run == DRY_RUN_COSTCOEFFS) {
    int plane;
    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
#if CONFIG_CB4X4
      if (!is_chroma_reference(mi_row, mi_col, bsize,
                               xd->plane[plane].subsampling_x,
                               xd->plane[plane].subsampling_y))
        continue;
#else
      (void)mi_row;
      (void)mi_col;
#endif
      av1_foreach_transformed_block_in_plane(xd, bsize, plane, cost_coeffs_b,
                                             &arg);
    }
  }
#endif  // !CONFIG_PVQ

  if (rate) *rate += arg.this_rate;
}

#if CONFIG_SUPERTX
void av1_tokenize_sb_supertx(const AV1_COMP *cpi, ThreadData *td,
                             TOKENEXTRA **t, RUN_TYPE dry_run, int mi_row,
                             int mi_col, BLOCK_SIZE bsize, int *rate) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &td->mb.e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  TOKENEXTRA *t_backup = *t;
  const int ctx = av1_get_skip_context(xd);
  const int skip_inc =
      !segfeature_active(&cm->seg, mbmi->segment_id_supertx, SEG_LVL_SKIP);
  struct tokenize_b_args arg = { cpi, td, t, 0 };
  if (mbmi->skip) {
    if (!dry_run) td->counts->skip[ctx][1] += skip_inc;
    av1_reset_skip_context(xd, mi_row, mi_col, bsize);
    if (dry_run) *t = t_backup;
    return;
  }

  if (!dry_run) {
    int plane;
    td->counts->skip[ctx][0] += skip_inc;

    for (plane = 0; plane < MAX_MB_PLANE; ++plane) {
      av1_foreach_transformed_block_in_plane(xd, bsize, plane, tokenize_b,
                                             &arg);
      (*t)->token = EOSB_TOKEN;
      (*t)++;
    }
  } else if (dry_run == DRY_RUN_NORMAL) {
    int plane;
    for (plane = 0; plane < MAX_MB_PLANE; ++plane)
      av1_foreach_transformed_block_in_plane(xd, bsize, plane,
                                             set_entropy_context_b, &arg);
    *t = t_backup;
  } else if (dry_run == DRY_RUN_COSTCOEFFS) {
    int plane;
    for (plane = 0; plane < MAX_MB_PLANE; ++plane)
      av1_foreach_transformed_block_in_plane(xd, bsize, plane, cost_coeffs_b,
                                             &arg);
  }
  if (rate) *rate += arg.this_rate;
}
#endif  // CONFIG_SUPERTX
