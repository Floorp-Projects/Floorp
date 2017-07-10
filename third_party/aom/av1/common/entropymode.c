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

#include "aom_mem/aom_mem.h"

#include "av1/common/reconinter.h"
#include "av1/common/scan.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/seg_common.h"

#if CONFIG_LV_MAP
const aom_prob default_txb_skip[TX_SIZES][TXB_SKIP_CONTEXTS] = {
#if CONFIG_CHROMA_2X2
  { 252, 71, 126, 184, 178, 218, 251, 49, 133, 221, 27, 92, 197 },
#endif
  { 252, 71, 126, 184, 178, 218, 251, 49, 133, 221, 27, 92, 197 },
  { 252, 71, 126, 184, 178, 218, 251, 49, 133, 221, 27, 92, 197 },
  { 252, 71, 126, 184, 178, 218, 251, 49, 133, 221, 27, 92, 197 },
  { 252, 71, 126, 184, 178, 218, 251, 49, 133, 221, 27, 92, 197 },
};
const aom_prob default_dc_sign[PLANE_TYPES][DC_SIGN_CONTEXTS] = {
  { 125, 102, 147 }, { 119, 101, 135 },
};

const aom_prob default_coeff_base
    [TX_SIZES][PLANE_TYPES][NUM_BASE_LEVELS][COEFF_BASE_CONTEXTS] = {
#if CONFIG_CHROMA_2X2
      { // TX_2X2
        {
            { 73,  128, 131, 204, 165, 226, 169, 236, 18,  128, 51,
              153, 97,  179, 123, 201, 145, 226, 20,  128, 59,  153,
              107, 181, 129, 201, 142, 226, 3,   128, 19,  99,  46,
              135, 92,  166, 129, 190, 157, 217, 128, 128 },

            { 128, 128, 178, 218, 192, 236, 186, 243, 55,  128, 110,
              183, 151, 205, 168, 221, 180, 238, 65,  128, 116, 178,
              157, 206, 172, 222, 183, 238, 24,  128, 65,  127, 104,
              164, 154, 195, 187, 216, 205, 230, 128, 128 },
        },
        {
            { 73,  128, 131, 204, 165, 226, 169, 236, 18,  128, 51,
              153, 97,  179, 123, 201, 145, 226, 20,  128, 59,  153,
              107, 181, 129, 201, 142, 226, 3,   128, 19,  99,  46,
              135, 92,  166, 129, 190, 157, 217, 128, 128 },

            { 128, 128, 178, 218, 192, 236, 186, 243, 55,  128, 110,
              183, 151, 205, 168, 221, 180, 238, 65,  128, 116, 178,
              157, 206, 172, 222, 183, 238, 24,  128, 65,  127, 104,
              164, 154, 195, 187, 216, 205, 230, 128, 128 },
        } },
#endif
      { // TX_4X4
        {
            // PLANE_Y
            { 73,  128, 131, 204, 165, 226, 169, 236, 18,  128, 51,
              153, 97,  179, 123, 201, 145, 226, 20,  128, 59,  153,
              107, 181, 129, 201, 142, 226, 3,   128, 19,  99,  46,
              135, 92,  166, 129, 190, 157, 217, 128, 128 },

            { 128, 128, 178, 218, 192, 236, 186, 243, 55,  128, 110,
              183, 151, 205, 168, 221, 180, 238, 65,  128, 116, 178,
              157, 206, 172, 222, 183, 238, 24,  128, 65,  127, 104,
              164, 154, 195, 187, 216, 205, 230, 128, 128 },
        },
        {
            // PLANE_UV
            { 47,  128, 100, 176, 140, 207, 150, 223, 11,  128, 35,
              133, 79,  165, 115, 186, 129, 210, 8,   128, 30,  114,
              80,  159, 116, 187, 146, 214, 2,   128, 9,   59,  28,
              86,  71,  131, 117, 165, 149, 188, 128, 128 },

            { 83,  128, 152, 205, 168, 227, 192, 238, 42,  128, 92,
              169, 138, 193, 165, 209, 128, 206, 36,  128, 86,  159,
              141, 198, 181, 213, 102, 223, 18,  128, 50,  132, 90,
              144, 141, 169, 180, 191, 128, 217, 128, 128 },
        } },
      {
          // TX_8X8
          {
              // PLANE_Y
              { 82,  128, 143, 203, 177, 225, 186, 237, 7,   128, 37,
                109, 78,  151, 110, 182, 139, 213, 25,  128, 51,  115,
                86,  146, 111, 175, 125, 205, 3,   128, 12,  55,  32,
                78,  63,  111, 96,  148, 123, 185, 146, 206 },

              { 136, 128, 182, 220, 201, 236, 205, 243, 46,  128, 101,
                164, 147, 194, 170, 218, 177, 234, 62,  128, 104, 146,
                143, 183, 165, 207, 183, 228, 30,  128, 60,  95,  95,
                128, 135, 163, 166, 196, 175, 219, 192, 231 },
          },
          {
              // PLANE_UV
              { 47,  128, 112, 189, 164, 202, 163, 218, 8,   128, 32,
                110, 68,  151, 102, 179, 134, 195, 5,   128, 22,  76,
                54,  103, 80,  146, 101, 182, 1,   128, 5,   39,  17,
                53,  46,  93,  79,  127, 112, 161, 64,  195 },

              { 90,  128, 156, 210, 183, 225, 128, 236, 39,  128, 98,
                164, 146, 201, 209, 219, 171, 208, 32,  128, 68,  123,
                119, 169, 154, 184, 128, 213, 15,  128, 38,  111, 83,
                112, 120, 163, 180, 170, 154, 213, 128, 205 },
          },
      },

      {
          // TX_16X16
          {
              // PLANE_Y
              { 96,  128, 169, 218, 208, 233, 187, 244, 10,  128, 34,
                101, 82,  153, 113, 184, 137, 212, 6,   128, 34,  104,
                81,  145, 109, 176, 147, 202, 1,   128, 3,   43,  15,
                53,  43,  89,  79,  129, 108, 168, 110, 194 },

              { 156, 128, 206, 232, 218, 240, 128, 251, 39,  128, 108,
                161, 156, 202, 187, 216, 179, 234, 40,  128, 103, 152,
                144, 185, 159, 208, 205, 227, 14,  128, 39,  84,  76,
                110, 121, 151, 157, 187, 201, 206, 64,  216 },
          },
          {
              // PLANE_UV
              { 42, 128, 139, 211, 180, 230, 199, 238, 3,   128, 32,
                96, 69,  145, 102, 186, 117, 212, 4,   128, 25,  72,
                55, 111, 81,  159, 116, 198, 1,   128, 4,   22,  16,
                34, 35,  68,  63,  116, 89,  165, 102, 199 },

              { 135, 128, 193, 227, 182, 239, 128, 246, 42,  128, 115,
                156, 146, 203, 188, 216, 128, 229, 32,  128, 82,  127,
                120, 178, 165, 203, 213, 229, 11,  128, 32,  73,  79,
                111, 129, 158, 162, 187, 156, 209, 85,  222 },
          },
      },

      {
          // TX_32X32
          {
              // PLANE_Y
              { 97,  128, 163, 232, 191, 246, 219, 252, 3,   128, 41,
                108, 91,  147, 104, 183, 118, 225, 6,   128, 45,  91,
                83,  125, 92,  160, 99,  215, 1,   128, 11,  36,  28,
                46,  43,  59,  57,  86,  73,  145, 91,  210 },

              { 127, 128, 201, 239, 247, 248, 128, 254, 40,  128, 103,
                152, 158, 199, 186, 225, 181, 242, 38,  128, 92,  112,
                146, 189, 162, 217, 112, 239, 17,  128, 30,  47,  63,
                89,  113, 146, 147, 187, 168, 217, 150, 233 },
          },
          {
              // PLANE_UV
              { 65,  128, 155, 223, 166, 235, 154, 244, 15,  128, 57,
                154, 110, 199, 159, 224, 149, 239, 9,   128, 57,  140,
                97,  185, 148, 218, 176, 236, 1,   128, 3,   43,  19,
                42,  64,  98,  117, 167, 154, 199, 128, 158 },

              { 130, 128, 189, 231, 171, 247, 128, 246, 63,  128, 132,
                222, 186, 224, 199, 244, 128, 247, 55,  128, 113, 211,
                164, 230, 225, 243, 128, 239, 7,   128, 31,  102, 106,
                138, 147, 183, 171, 223, 171, 224, 128, 128 },
          },
      },
    };

const aom_prob default_nz_map[TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS] = {
#if CONFIG_CHROMA_2X2
  {
      { 34, 103, 61, 106, 62,  160, 112, 54, 173, 121,
        75, 157, 92, 75,  157, 129, 94,  65, 52,  37 },
      { 52,  124, 84,  136, 107, 197, 161, 82, 183, 151,
        109, 153, 140, 103, 152, 134, 109, 81, 69,  50 },
  },
#endif
  {
      { 34, 103, 61, 106, 62,  160, 112, 54, 173, 121,
        75, 157, 92, 75,  157, 129, 94,  65, 52,  37 },
      { 52,  124, 84,  136, 107, 197, 161, 82, 183, 151,
        109, 153, 140, 103, 152, 134, 109, 81, 69,  50 },
  },
  {
      { 34, 127, 74,  124, 74,  204, 153, 76,  226, 162,
        92, 207, 126, 91,  227, 192, 149, 108, 85,  55 },
      { 43,  136, 115, 158, 130, 212, 187, 112, 231, 180,
        130, 202, 164, 130, 236, 204, 168, 139, 112, 114 },
  },
  {
      { 25,  117, 70,  120, 77,  215, 171, 102, 234, 156,
        105, 235, 155, 109, 247, 220, 176, 127, 92,  72 },
      { 24,  88,  49,  100, 62,  202, 148, 62,  237, 178,
        102, 233, 168, 105, 244, 198, 162, 127, 103, 71 },
  },
  {
      { 11, 54,  17,  69, 26,  128, 125, 56,  232, 130,
        60, 237, 121, 66, 250, 168, 134, 114, 93,  53 },
      { 21, 52,  32,  95,  64,  171, 152, 70,  247, 159,
        81, 252, 177, 100, 252, 221, 192, 143, 195, 146 },
  },
};

const aom_prob default_eob_flag[TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS] = {
#if CONFIG_CHROMA_2X2
  {
      { 229, 236, 231, 222, 239, 236, 214, 201, 236, 226, 195, 134, 228,
        210, 150, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 182, 186, 172, 176, 207, 213, 152, 122, 187, 171, 131, 65, 170,
        134, 101, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
  },
#endif
  {
      { 229, 236, 231, 222, 239, 236, 214, 201, 236, 226, 195, 134, 228,
        210, 150, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 182, 186, 172, 176, 207, 213, 152, 122, 187, 171, 131, 65, 170,
        134, 101, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
  },
  {
      { 225, 234, 244, 236, 205, 242, 246, 247, 246, 234, 191, 242, 237,
        215, 142, 224, 206, 142, 73,  128, 128, 128, 128, 128, 128 },
      { 154, 171, 187, 175, 62,  199, 202, 206, 215, 200, 111, 197, 199,
        174, 100, 135, 105, 104, 45,  128, 128, 128, 128, 128, 128 },
  },
  {
      { 180, 213, 216, 229, 233, 232, 240, 235, 220, 178, 239, 238, 225,
        187, 229, 214, 226, 200, 183, 141, 158, 179, 128, 128, 128 },
      { 190, 225, 234, 248, 249, 248, 253, 251, 232, 110, 254, 252, 236,
        57,  253, 248, 232, 85,  244, 189, 112, 64,  128, 128, 128 },
  },
  {
      { 248, 224, 246, 244, 239, 245, 251, 246, 251, 255, 255, 255, 249,
        255, 255, 255, 229, 255, 255, 255, 228, 255, 255, 247, 137 },
      { 204, 207, 233, 215, 193, 228, 239, 221, 227, 250, 236, 207, 135,
        236, 186, 182, 57,  209, 140, 128, 85,  184, 110, 128, 128 },
  },
};

const aom_prob default_coeff_lps[TX_SIZES][PLANE_TYPES][LEVEL_CONTEXTS] = {
#if CONFIG_CHROMA_2X2
  {
      { 164, 128, 134, 165, 128, 137, 168, 128, 97,  136, 167, 128,
        182, 205, 143, 172, 200, 145, 173, 193, 103, 137, 170, 191,
        198, 214, 162, 187, 209, 162, 187, 207, 128, 156, 183, 201,
        219, 230, 204, 210, 225, 201, 209, 225, 187, 190, 203, 214 },
      { 106, 128, 98,  126, 128, 87,  122, 128, 54,  89,  131, 128,
        142, 180, 123, 154, 189, 115, 149, 175, 79,  115, 157, 182,
        175, 197, 147, 174, 199, 145, 174, 201, 89,  135, 173, 194,
        212, 222, 206, 203, 223, 188, 201, 220, 128, 144, 202, 206 },
  },
#endif
  {
      { 164, 128, 134, 165, 128, 137, 168, 128, 97,  136, 167, 128,
        182, 205, 143, 172, 200, 145, 173, 193, 103, 137, 170, 191,
        198, 214, 162, 187, 209, 162, 187, 207, 128, 156, 183, 201,
        219, 230, 204, 210, 225, 201, 209, 225, 187, 190, 203, 214 },
      { 106, 128, 98,  126, 128, 87,  122, 128, 54,  89,  131, 128,
        142, 180, 123, 154, 189, 115, 149, 175, 79,  115, 157, 182,
        175, 197, 147, 174, 199, 145, 174, 201, 89,  135, 173, 194,
        212, 222, 206, 203, 223, 188, 201, 220, 128, 144, 202, 206 },
  },
  {
      { 171, 128, 123, 169, 128, 121, 165, 128, 82,  125, 168, 128,
        191, 213, 143, 177, 199, 136, 170, 194, 95,  135, 171, 195,
        206, 222, 166, 191, 212, 154, 184, 207, 115, 149, 180, 204,
        223, 237, 196, 215, 231, 186, 209, 228, 158, 178, 201, 222 },
      { 115, 128, 115, 146, 128, 91,  147, 128, 55,  93,  139, 128,
        147, 190, 141, 176, 201, 123, 156, 173, 68,  114, 156, 195,
        186, 205, 153, 191, 214, 141, 179, 205, 107, 132, 166, 184,
        215, 225, 200, 212, 230, 102, 207, 222, 128, 119, 200, 212 },
  },
  {
      { 185, 128, 134, 198, 128, 128, 195, 128, 58,  110, 162, 128,
        208, 227, 154, 196, 206, 144, 188, 209, 83,  130, 168, 198,
        219, 232, 167, 205, 222, 158, 196, 216, 107, 143, 178, 204,
        233, 244, 202, 226, 238, 191, 217, 234, 153, 178, 200, 223 },
      { 160, 128, 154, 197, 128, 129, 178, 128, 53,  112, 157, 128,
        185, 214, 169, 196, 221, 134, 179, 186, 82,  131, 168, 194,
        204, 220, 176, 209, 221, 173, 194, 209, 107, 154, 181, 203,
        230, 241, 202, 226, 237, 185, 223, 234, 162, 187, 203, 222 },
  },
  {
      { 177, 128, 165, 226, 128, 152, 219, 128, 45,  129, 188, 128,
        198, 218, 179, 220, 228, 163, 214, 220, 72,  134, 181, 206,
        216, 225, 177, 218, 231, 158, 213, 223, 112, 150, 185, 210,
        245, 251, 204, 234, 247, 195, 231, 243, 163, 186, 213, 235 },
      { 161, 128, 174, 205, 128, 146, 182, 128, 59,  125, 179, 128,
        183, 208, 199, 220, 239, 184, 213, 217, 71,  141, 196, 217,
        213, 219, 215, 230, 237, 171, 224, 238, 112, 173, 193, 221,
        239, 246, 168, 243, 249, 93,  241, 247, 128, 195, 216, 233 },
  },
};
#endif  // CONFIG_LV_MAP

#if CONFIG_EXT_PARTITION_TYPES
static const aom_prob
    default_partition_probs[PARTITION_CONTEXTS][EXT_PARTITION_TYPES - 1] = {
      // 8x8 -> 4x4
      { 199, 122, 141, 128, 128, 128, 255, 128, 255 },  // a/l both not split
      { 147, 63, 159, 128, 128, 128, 255, 128, 255 },   // a split, l not split
      { 148, 133, 118, 128, 128, 128, 255, 128, 255 },  // l split, a not split
      { 121, 104, 114, 128, 128, 128, 255, 128, 255 },  // a/l both split
      // 16x16 -> 8x8
      { 174, 73, 87, 128, 128, 128, 255, 128, 255 },  // a/l both not split
      { 92, 41, 83, 128, 128, 128, 255, 128, 255 },   // a split, l not split
      { 82, 99, 50, 128, 128, 128, 255, 128, 255 },   // l split, a not split
      { 53, 39, 39, 128, 128, 128, 255, 128, 255 },   // a/l both split
      // 32x32 -> 16x16
      { 177, 58, 59, 128, 128, 85, 128, 85, 128 },  // a/l both not split
      { 68, 26, 63, 128, 128, 85, 128, 85, 128 },   // a split, l not split
      { 52, 79, 25, 128, 128, 85, 128, 85, 128 },   // l split, a not split
      { 17, 14, 12, 128, 128, 85, 128, 85, 128 },   // a/l both split
      // 64x64 -> 32x32
      { 222, 34, 30, 128, 128, 128, 255, 128, 255 },  // a/l both not split
      { 72, 16, 44, 128, 128, 128, 255, 128, 255 },   // a split, l not split
      { 58, 32, 12, 128, 128, 128, 255, 128, 255 },   // l split, a not split
      { 10, 7, 6, 128, 128, 128, 255, 128, 255 },     // a/l both split
#if CONFIG_EXT_PARTITION
      // 128x128 -> 64x64
      { 222, 34, 30, 128, 128, 128, 255, 128, 255 },  // a/l both not split
      { 72, 16, 44, 128, 128, 128, 255, 128, 255 },   // a split, l not split
      { 58, 32, 12, 128, 128, 128, 255, 128, 255 },   // l split, a not split
      { 10, 7, 6, 128, 128, 128, 255, 128, 255 },     // a/l both split
#endif                                                // CONFIG_EXT_PARTITION
#if CONFIG_UNPOISON_PARTITION_CTX
      { 0, 0, 141, 0, 0, 0, 0, 0, 0 },  // 8x8 -> 4x4
      { 0, 0, 87, 0, 0, 0, 0, 0, 0 },   // 16x16 -> 8x8
      { 0, 0, 59, 0, 0, 0, 0, 0, 0 },   // 32x32 -> 16x16
      { 0, 0, 30, 0, 0, 0, 0, 0, 0 },   // 64x64 -> 32x32
#if CONFIG_EXT_PARTITION
      { 0, 0, 30, 0, 0, 0, 0, 0, 0 },   // 128x128 -> 64x64
#endif                                  // CONFIG_EXT_PARTITION
      { 0, 122, 0, 0, 0, 0, 0, 0, 0 },  // 8x8 -> 4x4
      { 0, 73, 0, 0, 0, 0, 0, 0, 0 },   // 16x16 -> 8x8
      { 0, 58, 0, 0, 0, 0, 0, 0, 0 },   // 32x32 -> 16x16
      { 0, 34, 0, 0, 0, 0, 0, 0, 0 },   // 64x64 -> 32x32
#if CONFIG_EXT_PARTITION
      { 0, 34, 0, 0, 0, 0, 0, 0, 0 },  // 128x128 -> 64x64
#endif                                 // CONFIG_EXT_PARTITION
#endif                                 // CONFIG_UNPOISON_PARTITION_CTX
    };
#else
static const aom_prob
    default_partition_probs[PARTITION_CONTEXTS][PARTITION_TYPES - 1] = {
      // 8x8 -> 4x4
      { 199, 122, 141 },  // a/l both not split
      { 147, 63, 159 },   // a split, l not split
      { 148, 133, 118 },  // l split, a not split
      { 121, 104, 114 },  // a/l both split
      // 16x16 -> 8x8
      { 174, 73, 87 },  // a/l both not split
      { 92, 41, 83 },   // a split, l not split
      { 82, 99, 50 },   // l split, a not split
      { 53, 39, 39 },   // a/l both split
      // 32x32 -> 16x16
      { 177, 58, 59 },  // a/l both not split
      { 68, 26, 63 },   // a split, l not split
      { 52, 79, 25 },   // l split, a not split
      { 17, 14, 12 },   // a/l both split
      // 64x64 -> 32x32
      { 222, 34, 30 },  // a/l both not split
      { 72, 16, 44 },   // a split, l not split
      { 58, 32, 12 },   // l split, a not split
      { 10, 7, 6 },     // a/l both split
#if CONFIG_EXT_PARTITION
      // 128x128 -> 64x64
      { 222, 34, 30 },  // a/l both not split
      { 72, 16, 44 },   // a split, l not split
      { 58, 32, 12 },   // l split, a not split
      { 10, 7, 6 },     // a/l both split
#endif  // CONFIG_EXT_PARTITION
#if CONFIG_UNPOISON_PARTITION_CTX
      { 0, 0, 141 },    // 8x8 -> 4x4
      { 0, 0, 87 },     // 16x16 -> 8x8
      { 0, 0, 59 },     // 32x32 -> 16x16
      { 0, 0, 30 },     // 64x64 -> 32x32
#if CONFIG_EXT_PARTITION
      { 0, 0, 30 },     // 128x128 -> 64x64
#endif  // CONFIG_EXT_PARTITION
      { 0, 122, 0 },    // 8x8 -> 4x4
      { 0, 73, 0 },     // 16x16 -> 8x8
      { 0, 58, 0 },     // 32x32 -> 16x16
      { 0, 34, 0 },     // 64x64 -> 32x32
#if CONFIG_EXT_PARTITION
      { 0, 34, 0 },     // 128x128 -> 64x64
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_UNPOISON_PARTITION_CTX
    };
#endif  // CONFIG_EXT_PARTITION_TYPES

static const aom_prob default_newmv_prob[NEWMV_MODE_CONTEXTS] = {
  155, 116, 94, 32, 96, 56, 30,
};

static const aom_prob default_zeromv_prob[ZEROMV_MODE_CONTEXTS] = {
  45, 13,
};

static const aom_prob default_refmv_prob[REFMV_MODE_CONTEXTS] = {
  178, 212, 135, 244, 203, 122, 128, 128, 128,
};

static const aom_prob default_drl_prob[DRL_MODE_CONTEXTS] = {
  119, 128, 189, 134, 128,
};
#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob default_newmv_cdf[NEWMV_MODE_CONTEXTS][CDF_SIZE(2)] =
    { { AOM_ICDF(128 * 155), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 116), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 94), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 32), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 96), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 56), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 30), AOM_ICDF(32768), 0 } };
static const aom_cdf_prob default_zeromv_cdf[ZEROMV_MODE_CONTEXTS][CDF_SIZE(
    2)] = { { AOM_ICDF(128 * 45), AOM_ICDF(32768), 0 },
            { AOM_ICDF(128 * 13), AOM_ICDF(32768), 0 } };
static const aom_cdf_prob default_refmv_cdf[REFMV_MODE_CONTEXTS][CDF_SIZE(2)] =
    { { AOM_ICDF(128 * 178), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 212), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 135), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 244), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 203), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 122), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 } };
static const aom_cdf_prob default_drl_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)] = {
  { AOM_ICDF(128 * 119), AOM_ICDF(32768), 0 },
  { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(128 * 189), AOM_ICDF(32768), 0 },
  { AOM_ICDF(128 * 134), AOM_ICDF(32768), 0 },
  { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 }
};
#endif

#if CONFIG_EXT_INTER
static const aom_prob default_inter_compound_mode_probs
    [INTER_MODE_CONTEXTS][INTER_COMPOUND_MODES - 1] = {
      { 154, 167, 233, 165, 143, 170, 167 },  // 0 = both zero mv
      { 75, 168, 237, 155, 135, 176, 172 },   // 1 = 1 zero + 1 predicted
      { 7, 173, 227, 128, 153, 188, 189 },    // 2 = two predicted mvs
      { 8, 120, 214, 113, 154, 178, 174 },    // 3 = 1 pred/zero, 1 new
      { 4, 85, 194, 94, 155, 173, 167 },      // 4 = two new mvs
      { 23, 89, 180, 73, 157, 151, 155 },     // 5 = one intra neighbour
      { 27, 49, 152, 91, 134, 153, 142 },     // 6 = two intra neighbours
    };

static const aom_cdf_prob
    default_inter_compound_mode_cdf[INTER_MODE_CONTEXTS][CDF_SIZE(
        INTER_COMPOUND_MODES)] = {
      { AOM_ICDF(19712), AOM_ICDF(28229), AOM_ICDF(30892), AOM_ICDF(31437),
        AOM_ICDF(31712), AOM_ICDF(32135), AOM_ICDF(32360), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9600), AOM_ICDF(24804), AOM_ICDF(29268), AOM_ICDF(30323),
        AOM_ICDF(30802), AOM_ICDF(31726), AOM_ICDF(32177), AOM_ICDF(32768), 0 },
      { AOM_ICDF(896), AOM_ICDF(22434), AOM_ICDF(27015), AOM_ICDF(29026),
        AOM_ICDF(29753), AOM_ICDF(31114), AOM_ICDF(31597), AOM_ICDF(32768), 0 },
      { AOM_ICDF(1024), AOM_ICDF(15904), AOM_ICDF(22127), AOM_ICDF(25421),
        AOM_ICDF(26864), AOM_ICDF(28996), AOM_ICDF(30001), AOM_ICDF(32768), 0 },
      { AOM_ICDF(512), AOM_ICDF(11222), AOM_ICDF(17217), AOM_ICDF(21445),
        AOM_ICDF(23473), AOM_ICDF(26133), AOM_ICDF(27550), AOM_ICDF(32768), 0 },
      { AOM_ICDF(2944), AOM_ICDF(13313), AOM_ICDF(17214), AOM_ICDF(20751),
        AOM_ICDF(23211), AOM_ICDF(25500), AOM_ICDF(26992), AOM_ICDF(32768), 0 },
      { AOM_ICDF(3456), AOM_ICDF(9067), AOM_ICDF(14069), AOM_ICDF(16907),
        AOM_ICDF(18817), AOM_ICDF(21214), AOM_ICDF(23139), AOM_ICDF(32768), 0 }
    };

#if CONFIG_COMPOUND_SINGLEREF
// TODO(zoeliu): Default values to be further adjusted based on the collected
//               stats.
/*
static const aom_prob default_inter_singleref_comp_mode_probs
    [INTER_MODE_CONTEXTS][INTER_SINGLEREF_COMP_MODES - 1] = {
      { 2, 173, 68, 180 },   // 0 = both zero mv
      { 7, 145, 160, 180 },  // 1 = 1 zero + 1 predicted
      { 7, 166, 126, 180 },  // 2 = two predicted mvs
      { 7, 94, 132, 180 },   // 3 = 1 pred/zero, 1 new
      { 8, 64, 64, 180 },    // 4 = two new mvs
      { 17, 81, 52, 180 },   // 5 = one intra neighbour
      { 25, 29, 50, 180 },   // 6 = two intra neighbours
    };*/
static const aom_prob default_inter_singleref_comp_mode_probs
    [INTER_MODE_CONTEXTS][INTER_SINGLEREF_COMP_MODES - 1] = {
      { 2, 173, 68 },   // 0 = both zero mv
      { 7, 145, 160 },  // 1 = 1 zero + 1 predicted
      { 7, 166, 126 },  // 2 = two predicted mvs
      { 7, 94, 132 },   // 3 = 1 pred/zero, 1 new
      { 8, 64, 64 },    // 4 = two new mvs
      { 17, 81, 52 },   // 5 = one intra neighbour
      { 25, 29, 50 },   // 6 = two intra neighbours
    };

static const aom_cdf_prob
    default_inter_singleref_comp_mode_cdf[INTER_MODE_CONTEXTS][CDF_SIZE(
        INTER_SINGLEREF_COMP_MODES)] = {
      { AOM_ICDF(21971), AOM_ICDF(24771), AOM_ICDF(25027), AOM_ICDF(32768), 0 },
      { AOM_ICDF(18053), AOM_ICDF(26690), AOM_ICDF(27586), AOM_ICDF(32768), 0 },
      { AOM_ICDF(20667), AOM_ICDF(26182), AOM_ICDF(27078), AOM_ICDF(32768), 0 },
      { AOM_ICDF(11703), AOM_ICDF(22103), AOM_ICDF(22999), AOM_ICDF(32768), 0 },
      { AOM_ICDF(7936), AOM_ICDF(13888), AOM_ICDF(14912), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9679), AOM_ICDF(13927), AOM_ICDF(16103), AOM_ICDF(32768), 0 },
      { AOM_ICDF(3349), AOM_ICDF(8470), AOM_ICDF(11670), AOM_ICDF(32768), 0 }
    };
#endif  // CONFIG_COMPOUND_SINGLEREF

#if CONFIG_COMPOUND_SEGMENT && CONFIG_WEDGE
static const aom_prob
    default_compound_type_probs[BLOCK_SIZES_ALL][COMPOUND_TYPES - 1] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { 128, 128 }, { 128, 128 }, { 128, 128 },
#endif
      { 128, 128 }, { 255, 128 }, { 255, 128 }, { 66, 51 },   { 72, 35 },
      { 79, 29 },   { 71, 18 },   { 81, 29 },   { 81, 26 },   { 69, 19 },
      { 104, 1 },   { 99, 1 },    { 75, 1 },
#if CONFIG_EXT_PARTITION
      { 255, 1 },   { 255, 1 },   { 255, 1 },
#endif  // CONFIG_EXT_PARTITION
      { 208, 128 }, { 208, 128 }, { 208, 128 }, { 208, 128 },
    };
#elif !CONFIG_COMPOUND_SEGMENT && CONFIG_WEDGE
static const aom_prob
    default_compound_type_probs[BLOCK_SIZES_ALL][COMPOUND_TYPES - 1] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { 255 }, { 255 }, { 255 },
#endif
      { 208 }, { 208 }, { 208 }, { 208 }, { 208 }, { 208 }, { 216 },
      { 216 }, { 216 }, { 224 }, { 224 }, { 240 }, { 240 },
#if CONFIG_EXT_PARTITION
      { 255 }, { 255 }, { 255 },
#endif  // CONFIG_EXT_PARTITION
      { 208 }, { 208 }, { 208 }, { 208 },
    };
#elif CONFIG_COMPOUND_SEGMENT && !CONFIG_WEDGE
static const aom_prob
    default_compound_type_probs[BLOCK_SIZES_ALL][COMPOUND_TYPES - 1] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { 255 }, { 255 }, { 255 },
#endif
      { 208 }, { 208 }, { 208 }, { 208 }, { 208 }, { 208 }, { 216 },
      { 216 }, { 216 }, { 224 }, { 224 }, { 240 }, { 240 },
#if CONFIG_EXT_PARTITION
      { 255 }, { 255 }, { 255 },
#endif  // CONFIG_EXT_PARTITION
      { 208 }, { 208 }, { 208 }, { 208 },
    };
#else
static const aom_prob default_compound_type_probs[BLOCK_SIZES_ALL]
                                                 [COMPOUND_TYPES - 1];
#endif  // CONFIG_COMPOUND_SEGMENT && CONFIG_WEDGE

#if CONFIG_COMPOUND_SEGMENT && CONFIG_WEDGE
static const aom_cdf_prob
    default_compound_type_cdf[BLOCK_SIZES_ALL][CDF_SIZE(COMPOUND_TYPES)] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32704), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32704), AOM_ICDF(32768), 0 },
      { AOM_ICDF(8448), AOM_ICDF(13293), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9216), AOM_ICDF(12436), AOM_ICDF(32768), 0 },
      { AOM_ICDF(10112), AOM_ICDF(12679), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9088), AOM_ICDF(10753), AOM_ICDF(32768), 0 },
      { AOM_ICDF(10368), AOM_ICDF(12906), AOM_ICDF(32768), 0 },
      { AOM_ICDF(10368), AOM_ICDF(12643), AOM_ICDF(32768), 0 },
      { AOM_ICDF(8832), AOM_ICDF(10609), AOM_ICDF(32768), 0 },
      { AOM_ICDF(13312), AOM_ICDF(13388), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12672), AOM_ICDF(12751), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9600), AOM_ICDF(9691), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
      { AOM_ICDF(32640), AOM_ICDF(32641), AOM_ICDF(32768), 0 },  // 255, 1
      { AOM_ICDF(32640), AOM_ICDF(32641), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32641), AOM_ICDF(32768), 0 },
#endif  // CONFIG_EXT_PARTITION
      { 16384, 8192, 0, 0 },
      { 16384, 8192, 0, 0 },
      { 16384, 8192, 0, 0 },
      { 16384, 8192, 0, 0 },
    };
#elif !CONFIG_COMPOUND_SEGMENT && CONFIG_WEDGE
static const aom_cdf_prob
    default_compound_type_cdf[BLOCK_SIZES_ALL][CDF_SIZE(COMPOUND_TYPES)] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },  // 255
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },  // 208
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },
      { AOM_ICDF(27648), AOM_ICDF(32768), 0 },  // 216
      { AOM_ICDF(27648), AOM_ICDF(32768), 0 },
      { AOM_ICDF(27648), AOM_ICDF(32768), 0 },
      { AOM_ICDF(28672), AOM_ICDF(32768), 0 },  // 224
      { AOM_ICDF(28672), AOM_ICDF(32768), 0 },
      { AOM_ICDF(30720), AOM_ICDF(32768), 0 },  // 240
      { AOM_ICDF(30720), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },  // 255
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
#endif  // CONFIG_EXT_PARTITION
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
    };
#elif CONFIG_COMPOUND_SEGMENT && !CONFIG_WEDGE
static const aom_cdf_prob
    default_compound_type_cdf[BLOCK_SIZES_ALL][CDF_SIZE(COMPOUND_TYPES)] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },  // 255
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },  // 208
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },
      { AOM_ICDF(26624), AOM_ICDF(32768), 0 },
      { AOM_ICDF(27648), AOM_ICDF(32768), 0 },  // 216
      { AOM_ICDF(27648), AOM_ICDF(32768), 0 },
      { AOM_ICDF(27648), AOM_ICDF(32768), 0 },
      { AOM_ICDF(28672), AOM_ICDF(32768), 0 },  // 224
      { AOM_ICDF(28672), AOM_ICDF(32768), 0 },
      { AOM_ICDF(30720), AOM_ICDF(32768), 0 },  // 240
      { AOM_ICDF(30720), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },  // 255
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
#endif  // CONFIG_EXT_PARTITION
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32768), 0 },
    };
#else
static const aom_cdf_prob default_compound_type_cdf[BLOCK_SIZES_ALL]
                                                   [CDF_SIZE(COMPOUND_TYPES)];
#endif  // CONFIG_COMPOUND_SEGMENT && CONFIG_WEDGE

#if CONFIG_INTERINTRA
static const aom_prob default_interintra_prob[BLOCK_SIZE_GROUPS] = {
  128, 226, 244, 254,
};
#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob default_interintra_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(
    2)] = { { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
            { AOM_ICDF(226 * 128), AOM_ICDF(32768), 0 },
            { AOM_ICDF(244 * 128), AOM_ICDF(32768), 0 },
            { AOM_ICDF(254 * 128), AOM_ICDF(32768), 0 } };
#endif

static const aom_prob
    default_interintra_mode_prob[BLOCK_SIZE_GROUPS][INTERINTRA_MODES - 1] = {
      { 128, 128, 128 },  // block_size < 8x8
      { 24, 34, 119 },    // block_size < 16x16
      { 38, 33, 95 },     // block_size < 32x32
      { 51, 21, 110 },    // block_size >= 32x32
    };
static const aom_cdf_prob
    default_interintra_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(
        INTERINTRA_MODES)] = {
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(28672), AOM_ICDF(32768), 0 },
      { AOM_ICDF(3072), AOM_ICDF(7016), AOM_ICDF(18987), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4864), AOM_ICDF(8461), AOM_ICDF(17481), AOM_ICDF(32768), 0 },
      { AOM_ICDF(6528), AOM_ICDF(8681), AOM_ICDF(19031), AOM_ICDF(32768), 0 }
    };

static const aom_prob default_wedge_interintra_prob[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  128, 128, 128,
#endif
  128, 128, 128, 194, 213, 217, 222, 224, 226, 220, 128, 128, 128,
#if CONFIG_EXT_PARTITION
  208, 208, 208,
#endif  // CONFIG_EXT_PARTITION
  208, 208, 208, 208,
};

#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob
    default_wedge_interintra_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(194 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(213 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(217 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(222 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(224 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(226 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(220 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
      { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
#endif  // CONFIG_EXT_PARTITION
      { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
    };
#endif  // CONFIG_NEW_MULTISYMBOL

#endif  // CONFIG_INTERINTRA
#endif  // CONFIG_EXT_INTER

#if CONFIG_NCOBMC_ADAPT_WEIGHT
const aom_tree_index av1_ncobmc_mode_tree[TREE_SIZE(MAX_NCOBMC_MODES)] = {
  -NO_OVERLAP,    2,  -NCOBMC_MODE_1, 4,
  -NCOBMC_MODE_2, 6,  -NCOBMC_MODE_3, 8,
  -NCOBMC_MODE_4, 10, -NCOBMC_MODE_5, 12,
  -NCOBMC_MODE_6, 14, -NCOBMC_MODE_7, -NCOBMC_MODE_8
};

// TODO(weitinglin): find default prob
static const aom_prob
    default_ncobmc_mode_prob[ADAPT_OVERLAP_BLOCKS][MAX_NCOBMC_MODES - 1] = {
      { 23, 37, 37, 38, 65, 71, 81, 86 },   // 8x8
      { 28, 32, 37, 43, 51, 64, 85, 128 },  // 16X16 equal prob
      { 86, 22, 32, 25, 10, 40, 97, 65 },   // 32X32
      { 28, 32, 37, 43, 51, 64, 85, 128 }   // 64X64 equal prob
    };
static const aom_cdf_prob
    default_ncobmc_mode_cdf[ADAPT_OVERLAP_BLOCKS][CDF_SIZE(MAX_NCOBMC_MODES)] =
        { { AOM_ICDF(127), AOM_ICDF(4207), AOM_ICDF(8287), AOM_ICDF(12367),
            AOM_ICDF(16447), AOM_ICDF(20527), AOM_ICDF(24607), AOM_ICDF(28687),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(127), AOM_ICDF(4207), AOM_ICDF(8287), AOM_ICDF(12367),
            AOM_ICDF(16447), AOM_ICDF(20527), AOM_ICDF(24607), AOM_ICDF(28687),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(127), AOM_ICDF(4207), AOM_ICDF(8287), AOM_ICDF(12367),
            AOM_ICDF(16447), AOM_ICDF(20527), AOM_ICDF(24607), AOM_ICDF(28687),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(127), AOM_ICDF(4207), AOM_ICDF(8287), AOM_ICDF(12367),
            AOM_ICDF(16447), AOM_ICDF(20527), AOM_ICDF(24607), AOM_ICDF(28687),
            AOM_ICDF(32768), 0 } };
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT

// Change this section appropriately once warped motion is supported
#if CONFIG_MOTION_VAR && !CONFIG_WARPED_MOTION
#if !CONFIG_NCOBMC_ADAPT_WEIGHT
const aom_tree_index av1_motion_mode_tree[TREE_SIZE(MOTION_MODES)] = {
  -SIMPLE_TRANSLATION, -OBMC_CAUSAL
};

static const aom_prob
    default_motion_mode_prob[BLOCK_SIZES_ALL][MOTION_MODES - 1] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { 255 }, { 255 }, { 255 },
#endif
      { 255 }, { 255 }, { 255 }, { 151 }, { 153 }, { 144 }, { 178 },
      { 165 }, { 160 }, { 207 }, { 195 }, { 168 }, { 244 },
#if CONFIG_EXT_PARTITION
      { 252 }, { 252 }, { 252 },
#endif  // CONFIG_EXT_PARTITION
      { 208 }, { 208 }, { 208 }, { 208 },
    };

static const aom_cdf_prob
    default_motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(151 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(153 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(144 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(178 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(165 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(160 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(207 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(195 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(168 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(244 * 128), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
      { AOM_ICDF(252 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(252 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(252 * 128), AOM_ICDF(32768), 0 },
#endif  // CONFIG_EXT_PARTITION
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
    };

#else
// TODO(weitinglin): The default probability is copied from warped motion right
//                   now as a place holder. It needs to be fined tuned after
//                   NCOBMC_ADAPT_WEIGHT is actually implemented. Also needs to
//                   change this section appropriately once warped motion is
//                   supported.
const aom_tree_index av1_motion_mode_tree[TREE_SIZE(MOTION_MODES)] = {
  -SIMPLE_TRANSLATION, 2, -OBMC_CAUSAL, -NCOBMC_ADAPT_WEIGHT,
};
static const aom_prob
    default_motion_mode_prob[BLOCK_SIZES_ALL][MOTION_MODES - 1] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { 255, 200 }, { 255, 200 }, { 255, 200 },
#endif
      { 255, 200 }, { 255, 200 }, { 255, 200 }, { 151, 200 }, { 153, 200 },
      { 144, 200 }, { 178, 200 }, { 165, 200 }, { 160, 200 }, { 207, 200 },
      { 195, 200 }, { 168, 200 }, { 244, 200 },
#if CONFIG_EXT_PARTITION
      { 252, 200 }, { 252, 200 }, { 252, 200 },
#endif  // CONFIG_EXT_PARTITION
      { 255, 200 }, { 255, 200 }, { 255, 200 }, { 255, 200 },
    };
static const aom_cdf_prob
    default_motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(7936), AOM_ICDF(19091), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4991), AOM_ICDF(19205), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4992), AOM_ICDF(19314), AOM_ICDF(32768), 0 },
      { AOM_ICDF(15104), AOM_ICDF(21590), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9855), AOM_ICDF(21043), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12800), AOM_ICDF(22238), AOM_ICDF(32768), 0 },
      { AOM_ICDF(24320), AOM_ICDF(26498), AOM_ICDF(32768), 0 },
      { AOM_ICDF(26496), AOM_ICDF(28995), AOM_ICDF(32768), 0 },
      { AOM_ICDF(25216), AOM_ICDF(28166), AOM_ICDF(32768), 0 },
      { AOM_ICDF(30592), AOM_ICDF(31238), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
      { AOM_ICDF(32256), AOM_ICDF(32656), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32256), AOM_ICDF(32656), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32256), AOM_ICDF(32656), AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(32640), AOM_ICDF(32740), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32740), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32740), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32740), AOM_ICDF(32768), 0 },
    };
#endif  // CONFIG_NCOBMC_ADAPT_WEIGHT

#elif !CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION

const aom_tree_index av1_motion_mode_tree[TREE_SIZE(MOTION_MODES)] = {
  -SIMPLE_TRANSLATION, -WARPED_CAUSAL
};

static const aom_prob
    default_motion_mode_prob[BLOCK_SIZES_ALL][MOTION_MODES - 1] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { 255 }, { 255 }, { 255 },
#endif
      { 255 }, { 255 }, { 255 }, { 151 }, { 153 }, { 144 }, { 178 },
      { 165 }, { 160 }, { 207 }, { 195 }, { 168 }, { 244 },
#if CONFIG_EXT_PARTITION
      { 252 }, { 252 }, { 252 },
#endif  // CONFIG_EXT_PARTITION
      { 208 }, { 208 }, { 208 }, { 208 },
    };

static const aom_cdf_prob
    default_motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(151 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(153 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(144 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(178 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(165 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(160 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(207 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(195 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(168 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(244 * 128), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
      { AOM_ICDF(252 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(252 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(252 * 128), AOM_ICDF(32768), 0 },
#endif  // CONFIG_EXT_PARTITION
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(255 * 128), AOM_ICDF(32768), 0 },
    };

#elif CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION

const aom_tree_index av1_motion_mode_tree[TREE_SIZE(MOTION_MODES)] = {
  -SIMPLE_TRANSLATION, 2, -OBMC_CAUSAL, -WARPED_CAUSAL,
};

static const aom_prob
    default_motion_mode_prob[BLOCK_SIZES_ALL][MOTION_MODES - 1] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { 128, 128 }, { 128, 128 }, { 128, 128 },
#endif
      { 128, 128 }, { 128, 128 }, { 128, 128 }, { 62, 115 },  { 39, 131 },
      { 39, 132 },  { 118, 94 },  { 77, 125 },  { 100, 121 }, { 190, 66 },
      { 207, 102 }, { 197, 100 }, { 239, 76 },
#if CONFIG_EXT_PARTITION
      { 252, 200 }, { 252, 200 }, { 252, 200 },
#endif  // CONFIG_EXT_PARTITION
      { 208, 200 }, { 208, 200 }, { 208, 200 }, { 208, 200 },
    };
static const aom_cdf_prob
    default_motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(16384), AOM_ICDF(24576), AOM_ICDF(32768), 0 },
      { AOM_ICDF(7936), AOM_ICDF(19091), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4991), AOM_ICDF(19205), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4992), AOM_ICDF(19314), AOM_ICDF(32768), 0 },
      { AOM_ICDF(15104), AOM_ICDF(21590), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9855), AOM_ICDF(21043), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12800), AOM_ICDF(22238), AOM_ICDF(32768), 0 },
      { AOM_ICDF(24320), AOM_ICDF(26498), AOM_ICDF(32768), 0 },
      { AOM_ICDF(26496), AOM_ICDF(28995), AOM_ICDF(32768), 0 },
      { AOM_ICDF(25216), AOM_ICDF(28166), AOM_ICDF(32768), 0 },
      { AOM_ICDF(30592), AOM_ICDF(31238), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
      { AOM_ICDF(32256), AOM_ICDF(32656), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32256), AOM_ICDF(32656), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32256), AOM_ICDF(32656), AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(32640), AOM_ICDF(32740), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32740), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32740), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32640), AOM_ICDF(32740), AOM_ICDF(32768), 0 },
    };

// Probability for the case that only 1 additional motion mode is allowed
static const aom_prob default_obmc_prob[BLOCK_SIZES_ALL] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  128, 128, 128,
#endif
  128, 128, 128, 45,  79, 75, 130, 141, 144, 208, 201, 186, 231,
#if CONFIG_EXT_PARTITION
  252, 252, 252,
#endif  // CONFIG_EXT_PARTITION
  208, 208, 208, 208,
};

#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob default_obmc_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
#endif
  { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(45 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(79 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(75 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(130 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(141 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(144 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(201 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(186 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(231 * 128), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
  { AOM_ICDF(252 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(252 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(252 * 128), AOM_ICDF(32768), 0 },
#endif  // CONFIG_EXT_PARTITION
  { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
  { AOM_ICDF(208 * 128), AOM_ICDF(32768), 0 },
};
#endif  // CONFIG_NEW_MULTISYMBOL
#endif

#if CONFIG_DELTA_Q
static const aom_prob default_delta_q_probs[DELTA_Q_PROBS] = { 220, 220, 220 };
static const aom_cdf_prob default_delta_q_cdf[CDF_SIZE(DELTA_Q_PROBS + 1)] = {
  AOM_ICDF(28160), AOM_ICDF(32120), AOM_ICDF(32677), AOM_ICDF(32768), 0
};
#if CONFIG_EXT_DELTA_Q
static const aom_prob default_delta_lf_probs[DELTA_LF_PROBS] = { 220, 220,
                                                                 220 };
static const aom_cdf_prob default_delta_lf_cdf[CDF_SIZE(DELTA_LF_PROBS + 1)] = {
  AOM_ICDF(28160), AOM_ICDF(32120), AOM_ICDF(32677), AOM_ICDF(32768), 0
};
#endif
#endif
#if CONFIG_EXT_TX
int av1_ext_tx_intra_ind[EXT_TX_SETS_INTRA][TX_TYPES];
int av1_ext_tx_intra_inv[EXT_TX_SETS_INTRA][TX_TYPES];
int av1_ext_tx_inter_ind[EXT_TX_SETS_INTER][TX_TYPES];
int av1_ext_tx_inter_inv[EXT_TX_SETS_INTER][TX_TYPES];
#endif

#if CONFIG_ALT_INTRA
#if CONFIG_SMOOTH_HV
const int av1_intra_mode_ind[INTRA_MODES] = { 0, 2, 3,  6,  4,  5, 8,
                                              9, 7, 10, 11, 12, 1 };
const int av1_intra_mode_inv[INTRA_MODES] = { 0, 12, 1, 2, 4,  5, 3,
                                              8, 6,  7, 9, 10, 11 };
#else
const int av1_intra_mode_ind[INTRA_MODES] = {
  0, 2, 3, 6, 4, 5, 8, 9, 7, 10, 1
};
const int av1_intra_mode_inv[INTRA_MODES] = {
  0, 10, 1, 2, 4, 5, 3, 8, 6, 7, 9
};
#endif  // CONFIG_SMOOTH_HV
#else
const int av1_intra_mode_ind[INTRA_MODES] = { 0, 2, 3, 6, 4, 5, 8, 9, 7, 1 };
const int av1_intra_mode_inv[INTRA_MODES] = { 0, 9, 1, 2, 4, 5, 3, 8, 6, 7 };
#endif  // CONFIG_ALT_INTRA

#if CONFIG_EXT_INTER
/* clang-format off */
#if CONFIG_INTERINTRA
const aom_tree_index av1_interintra_mode_tree[TREE_SIZE(INTERINTRA_MODES)] = {
  -II_DC_PRED, 2,        /* 0 = II_DC_NODE     */
#if CONFIG_ALT_INTRA
  -II_SMOOTH_PRED, 4,    /* 1 = II_SMOOTH_PRED */
#else
  -II_TM_PRED, 4,        /* 1 = II_TM_NODE     */
#endif
  -II_V_PRED, -II_H_PRED /* 2 = II_V_NODE      */
};
#endif  // CONFIG_INTERINTRA

const aom_tree_index av1_inter_compound_mode_tree
    [TREE_SIZE(INTER_COMPOUND_MODES)] = {
  -INTER_COMPOUND_OFFSET(ZERO_ZEROMV), 2,
  -INTER_COMPOUND_OFFSET(NEAREST_NEARESTMV), 4,
  6, -INTER_COMPOUND_OFFSET(NEW_NEWMV),
  -INTER_COMPOUND_OFFSET(NEAR_NEARMV), 8,
  10, 12,
  -INTER_COMPOUND_OFFSET(NEAREST_NEWMV), -INTER_COMPOUND_OFFSET(NEW_NEARESTMV),
  -INTER_COMPOUND_OFFSET(NEAR_NEWMV), -INTER_COMPOUND_OFFSET(NEW_NEARMV)
};

#if CONFIG_COMPOUND_SINGLEREF
// TODO(zoeliu): To redesign the tree structure once the number of mode changes.
/*
const aom_tree_index av1_inter_singleref_comp_mode_tree
    [TREE_SIZE(INTER_SINGLEREF_COMP_MODES)] = {
  -INTER_SINGLEREF_COMP_OFFSET(SR_ZERO_NEWMV), 2,
  -INTER_SINGLEREF_COMP_OFFSET(SR_NEAREST_NEARMV), 4,
  6, -INTER_SINGLEREF_COMP_OFFSET(SR_NEW_NEWMV),
  -INTER_SINGLEREF_COMP_OFFSET(SR_NEAREST_NEWMV),
  -INTER_SINGLEREF_COMP_OFFSET(SR_NEAR_NEWMV)
};*/

const aom_tree_index av1_inter_singleref_comp_mode_tree
    [TREE_SIZE(INTER_SINGLEREF_COMP_MODES)] = {
  -INTER_SINGLEREF_COMP_OFFSET(SR_ZERO_NEWMV), 2,
  -INTER_SINGLEREF_COMP_OFFSET(SR_NEAREST_NEARMV), 4,
  -INTER_SINGLEREF_COMP_OFFSET(SR_NEAR_NEWMV),
      -INTER_SINGLEREF_COMP_OFFSET(SR_NEW_NEWMV)
};
#endif  // CONFIG_COMPOUND_SINGLEREF

#if CONFIG_COMPOUND_SEGMENT && CONFIG_WEDGE
const aom_tree_index av1_compound_type_tree[TREE_SIZE(COMPOUND_TYPES)] = {
  -COMPOUND_AVERAGE, 2, -COMPOUND_WEDGE, -COMPOUND_SEG
};
#elif !CONFIG_COMPOUND_SEGMENT && CONFIG_WEDGE
const aom_tree_index av1_compound_type_tree[TREE_SIZE(COMPOUND_TYPES)] = {
  -COMPOUND_AVERAGE, -COMPOUND_WEDGE
};
#elif CONFIG_COMPOUND_SEGMENT && !CONFIG_WEDGE
const aom_tree_index av1_compound_type_tree[TREE_SIZE(COMPOUND_TYPES)] = {
  -COMPOUND_AVERAGE, -COMPOUND_SEG
};
#else
const aom_tree_index av1_compound_type_tree[TREE_SIZE(COMPOUND_TYPES)] = {};
#endif  // CONFIG_COMPOUND_SEGMENT && CONFIG_WEDGE
/* clang-format on */
#endif  // CONFIG_EXT_INTER

const aom_tree_index av1_partition_tree[TREE_SIZE(PARTITION_TYPES)] = {
  -PARTITION_NONE, 2, -PARTITION_HORZ, 4, -PARTITION_VERT, -PARTITION_SPLIT
};

#if CONFIG_EXT_PARTITION_TYPES
/* clang-format off */
const aom_tree_index av1_ext_partition_tree[TREE_SIZE(EXT_PARTITION_TYPES)] = {
  -PARTITION_NONE, 2,
  6, 4,
  8, -PARTITION_SPLIT,
  -PARTITION_HORZ, 10,
  -PARTITION_VERT, 14,

  -PARTITION_HORZ_A, 12,
  -PARTITION_HORZ_B, -PARTITION_HORZ_4,

  -PARTITION_VERT_A, 16,
  -PARTITION_VERT_B, -PARTITION_VERT_4
};
/* clang-format on */
#endif  // CONFIG_EXT_PARTITION_TYPES

static const aom_prob default_intra_inter_p[INTRA_INTER_CONTEXTS] = {
  6, 97, 151, 205,
};

#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob
    default_intra_inter_cdf[INTRA_INTER_CONTEXTS][CDF_SIZE(2)] = {
      { AOM_ICDF(768), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12416), AOM_ICDF(32768), 0 },
      { AOM_ICDF(19328), AOM_ICDF(32768), 0 },
      { AOM_ICDF(26240), AOM_ICDF(32768), 0 }
    };
#endif

static const aom_prob default_comp_inter_p[COMP_INTER_CONTEXTS] = {
#if !CONFIG_EXT_COMP_REFS
  216, 170, 131, 92, 42
#else   // CONFIG_EXT_COMP_REFS
  206, 182, 117, 104, 32
#endif  // !CONFIG_EXT_COMP_REFS
};

#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob
    default_comp_inter_cdf[COMP_INTER_CONTEXTS][CDF_SIZE(2)] = {
#if !CONFIG_EXT_COMP_REFS
      { AOM_ICDF(216 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(170 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(131 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(92 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(42 * 128), AOM_ICDF(32768), 0 }
#else   // CONFIG_EXT_COMP_REFS
      { AOM_ICDF(206 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(182 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(117 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(104 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32 * 128), AOM_ICDF(32768), 0 }
#endif  // !CONFIG_EXT_COMP_REFS
    };
#endif  // CONFIG_NEW_MULTISYMBOL

#if CONFIG_EXT_COMP_REFS
static const aom_prob default_comp_ref_type_p[COMP_REF_TYPE_CONTEXTS] = {
  8, 20, 78, 91, 194
};
static const aom_prob
    default_uni_comp_ref_p[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1] = {
      { 88, 30, 28 }, { 218, 97, 105 }, { 254, 180, 196 }
    };

#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob
    default_comp_ref_type_cdf[COMP_REF_TYPE_CONTEXTS][CDF_SIZE(2)] = {
      { AOM_ICDF(8 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(20 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(78 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(91 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(194 * 128), AOM_ICDF(32768), 0 }
    };
static const aom_cdf_prob
    default_uni_comp_ref_cdf[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS - 1]
                            [CDF_SIZE(2)] = {
                              { { AOM_ICDF(88 * 128), AOM_ICDF(32768), 0 },
                                { AOM_ICDF(30 * 128), AOM_ICDF(32768), 0 },
                                { AOM_ICDF(28 * 128), AOM_ICDF(32768), 0 } },
                              { { AOM_ICDF(218 * 128), AOM_ICDF(32768), 0 },
                                { AOM_ICDF(97 * 128), AOM_ICDF(32768), 0 },
                                { AOM_ICDF(105 * 128), AOM_ICDF(32768), 0 } },
                              { { AOM_ICDF(254 * 128), AOM_ICDF(32768), 0 },
                                { AOM_ICDF(180 * 128), AOM_ICDF(32768), 0 },
                                { AOM_ICDF(196 * 128), AOM_ICDF(32768), 0 } }
                            };
#endif  // CONFIG_NEW_MULTISYMBOL
#endif  // CONFIG_EXT_COMP_REFS

#if CONFIG_EXT_REFS
static const aom_prob default_comp_ref_p[REF_CONTEXTS][FWD_REFS - 1] = {
#if !CONFIG_EXT_COMP_REFS
  { 33, 16, 16 },
  { 77, 74, 74 },
  { 142, 142, 142 },
  { 172, 170, 170 },
  { 238, 247, 247 }
#else   // CONFIG_EXT_COMP_REFS
  { 21, 7, 5 },
  { 68, 20, 16 },
  { 128, 56, 36 },
  { 197, 111, 139 },
  { 238, 131, 136 }
#endif  // !CONFIG_EXT_COMP_REFS
};

static const aom_prob default_comp_bwdref_p[REF_CONTEXTS][BWD_REFS - 1] = {
#if CONFIG_ALTREF2
  // TODO(zoeliu): ALTREF2 to work with EXT_COMP_REFS and NEW_MULTISYMBOL.
  { 50, 50 },
  { 130, 130 },
  { 210, 210 },
  { 128, 128 },
  { 128, 128 }
#else  // !CONFIG_ALTREF2
#if !CONFIG_EXT_COMP_REFS
  { 16 }, { 74 }, { 142 }, { 170 }, { 247 }
#else   // CONFIG_EXT_COMP_REFS
  { 7 }, { 56 }, { 29 }, { 230 }, { 220 }
#endif  // CONFIG_EXT_COMP_REFS
#endif  // CONFIG_ALTREF2
};

#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob
    default_comp_ref_cdf[REF_CONTEXTS][FWD_REFS - 1][CDF_SIZE(2)] = {
#if !CONFIG_EXT_COMP_REFS
      { { AOM_ICDF(33 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(16 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(16 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(77 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(74 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(74 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(142 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(142 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(142 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(172 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(170 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(170 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(238 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(247 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(247 * 128), AOM_ICDF(32768), 0 } }
#else   // CONFIG_EXT_COMP_REFS
      { { AOM_ICDF(21 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(68 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(20 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(16 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(56 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(36 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(197 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(111 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(139 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(238 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(131 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(136 * 128), AOM_ICDF(32768), 0 } }
#endif  // !CONFIG_EXT_COMP_REFS
    };

static const aom_cdf_prob
    default_comp_bwdref_cdf[REF_CONTEXTS][BWD_REFS - 1][CDF_SIZE(2)] = {
#if !CONFIG_EXT_COMP_REFS
      { { AOM_ICDF(16 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(74 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(142 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(170 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(247 * 128), AOM_ICDF(32768), 0 } }
#else   // CONFIG_EXT_COMP_REFS
      { { AOM_ICDF(7 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(56 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(29 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(230 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(220 * 128), AOM_ICDF(32768), 0 } }
#endif  // !CONFIG_EXT_COMP_REFS
    };
#endif  // CONFIG_NEW_MULTISYMBOL

#else  // !CONFIG_EXT_REFS

static const aom_prob default_comp_ref_p[REF_CONTEXTS][COMP_REFS - 1] = {
  { 43 }, { 100 }, { 137 }, { 212 }, { 229 },
};
#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob
    default_comp_ref_cdf[REF_CONTEXTS][COMP_REFS - 1][CDF_SIZE(2)] = {
      { { AOM_ICDF(43 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(100 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(137 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(212 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(229 * 128), AOM_ICDF(32768), 0 } }
    };
#endif  // CONFIG_NEW_MULTISYMBOL
#endif  // CONFIG_EXT_REFS

static const aom_prob default_single_ref_p[REF_CONTEXTS][SINGLE_REFS - 1] = {
#if CONFIG_EXT_REFS
#if CONFIG_ALTREF2
  // TODO(zoeliu): ALTREF2 to work with EXT_COMP_REFS and NEW_MULTISYMBOL.
  { 33, 50, 16, 16, 16, 50 },
  { 77, 130, 74, 74, 74, 130 },
  { 142, 210, 142, 142, 142, 210 },
  { 172, 128, 170, 170, 170, 128 },
  { 238, 128, 247, 247, 247, 128 }
#else  // !CONFIG_ALTREF2
#if !CONFIG_EXT_COMP_REFS
  { 33, 16, 16, 16, 16 },
  { 77, 74, 74, 74, 74 },
  { 142, 142, 142, 142, 142 },
  { 172, 170, 170, 170, 170 },
  { 238, 247, 247, 247, 247 }
#else   // CONFIG_EXT_COMP_REFS
  { 36, 2, 28, 58, 9 },
  { 64, 22, 60, 122, 40 },
  { 153, 69, 126, 179, 71 },
  { 128, 174, 189, 216, 101 },
  { 233, 252, 228, 246, 200 }
#endif  // !CONFIG_EXT_COMP_REFS
#endif  // CONFIG_ALTREF2
#else   // !CONFIG_EXT_REFS
  { 31, 25 }, { 72, 80 }, { 147, 148 }, { 197, 191 }, { 235, 247 },
#endif  // CONFIG_EXT_REFS
};

#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob
    default_single_ref_cdf[REF_CONTEXTS][SINGLE_REFS - 1][CDF_SIZE(2)] = {
#if CONFIG_EXT_REFS
#if !CONFIG_EXT_COMP_REFS
      { { AOM_ICDF(33 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(16 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(16 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(16 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(16 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(77 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(74 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(74 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(74 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(74 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(142 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(142 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(142 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(142 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(142 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(172 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(170 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(170 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(170 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(170 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(238 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(247 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(247 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(247 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(247 * 128), AOM_ICDF(32768), 0 } }
#else   // CONFIG_EXT_COMP_REFS
      { { AOM_ICDF(36 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(2 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(28 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(58 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(9 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(64 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(22 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(60 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(122 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(40 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(153 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(69 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(126 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(179 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(71 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(174 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(189 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(216 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(101 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(233 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(252 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(228 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(246 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(200 * 128), AOM_ICDF(32768), 0 } }
#endif  // !CONFIG_EXT_COMP_REFS
#else   // CONFIG_EXT_REFS
      { { AOM_ICDF(31 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(25 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(72 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(80 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(147 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(148 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(197 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(191 * 128), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(235 * 128), AOM_ICDF(32768), 0 },
        { AOM_ICDF(247 * 128), AOM_ICDF(32768), 0 } }
#endif  // CONFIG_EXT_REFS
    };
#endif  // CONFIG_NEW_MULTISYMBOL

#if CONFIG_EXT_INTER && CONFIG_COMPOUND_SINGLEREF
// TODO(zoeliu): Default values to be further adjusted based on the collected
//               stats.
static const aom_prob default_comp_inter_mode_p[COMP_INTER_MODE_CONTEXTS] = {
  40, 110, 160, 220
};
#endif  // CONFIG_EXT_INTER && CONFIG_COMPOUND_SINGLEREF

#if CONFIG_PALETTE
// TODO(huisu): tune these cdfs
const aom_cdf_prob
    default_palette_y_size_cdf[PALETTE_BLOCK_SIZES][CDF_SIZE(PALETTE_SIZES)] = {
      { AOM_ICDF(12288), AOM_ICDF(19408), AOM_ICDF(24627), AOM_ICDF(26662),
        AOM_ICDF(28499), AOM_ICDF(30667), AOM_ICDF(32768), 0 },
      { AOM_ICDF(2815), AOM_ICDF(4570), AOM_ICDF(9416), AOM_ICDF(10875),
        AOM_ICDF(13782), AOM_ICDF(19863), AOM_ICDF(32768), 0 },
      { AOM_ICDF(3839), AOM_ICDF(5986), AOM_ICDF(11949), AOM_ICDF(13413),
        AOM_ICDF(16286), AOM_ICDF(21823), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12032), AOM_ICDF(14948), AOM_ICDF(22187), AOM_ICDF(23138),
        AOM_ICDF(24756), AOM_ICDF(27635), AOM_ICDF(32768), 0 },
      { AOM_ICDF(14847), AOM_ICDF(20167), AOM_ICDF(25433), AOM_ICDF(26751),
        AOM_ICDF(28278), AOM_ICDF(30119), AOM_ICDF(32768), 0 },
      { AOM_ICDF(14336), AOM_ICDF(20240), AOM_ICDF(24840), AOM_ICDF(26079),
        AOM_ICDF(27908), AOM_ICDF(30034), AOM_ICDF(32768), 0 },
      { AOM_ICDF(18816), AOM_ICDF(25574), AOM_ICDF(29030), AOM_ICDF(29877),
        AOM_ICDF(30656), AOM_ICDF(31506), AOM_ICDF(32768), 0 },
      { AOM_ICDF(23039), AOM_ICDF(27333), AOM_ICDF(30220), AOM_ICDF(30708),
        AOM_ICDF(31070), AOM_ICDF(31826), AOM_ICDF(32768), 0 },
      { AOM_ICDF(13696), AOM_ICDF(18911), AOM_ICDF(23620), AOM_ICDF(25371),
        AOM_ICDF(29821), AOM_ICDF(31617), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12543), AOM_ICDF(20838), AOM_ICDF(27455), AOM_ICDF(28762),
        AOM_ICDF(29763), AOM_ICDF(31546), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
      { AOM_ICDF(12543), AOM_ICDF(20838), AOM_ICDF(27455), AOM_ICDF(28762),
        AOM_ICDF(29763), AOM_ICDF(31546), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12543), AOM_ICDF(20838), AOM_ICDF(27455), AOM_ICDF(28762),
        AOM_ICDF(29763), AOM_ICDF(31546), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12543), AOM_ICDF(20838), AOM_ICDF(27455), AOM_ICDF(28762),
        AOM_ICDF(29763), AOM_ICDF(31546), AOM_ICDF(32768), 0 },
#endif
    };

const aom_cdf_prob default_palette_uv_size_cdf[PALETTE_BLOCK_SIZES][CDF_SIZE(
    PALETTE_SIZES)] = {
  { AOM_ICDF(20480), AOM_ICDF(29888), AOM_ICDF(32453), AOM_ICDF(32715),
    AOM_ICDF(32751), AOM_ICDF(32766), AOM_ICDF(32768), 0 },
  { AOM_ICDF(11135), AOM_ICDF(23641), AOM_ICDF(31056), AOM_ICDF(31998),
    AOM_ICDF(32496), AOM_ICDF(32668), AOM_ICDF(32768), 0 },
  { AOM_ICDF(9216), AOM_ICDF(23108), AOM_ICDF(30806), AOM_ICDF(31871),
    AOM_ICDF(32414), AOM_ICDF(32637), AOM_ICDF(32768), 0 },
  { AOM_ICDF(9984), AOM_ICDF(21999), AOM_ICDF(29192), AOM_ICDF(30645),
    AOM_ICDF(31640), AOM_ICDF(32402), AOM_ICDF(32768), 0 },
  { AOM_ICDF(7552), AOM_ICDF(16614), AOM_ICDF(24880), AOM_ICDF(27283),
    AOM_ICDF(29254), AOM_ICDF(31203), AOM_ICDF(32768), 0 },
  { AOM_ICDF(9600), AOM_ICDF(20279), AOM_ICDF(27548), AOM_ICDF(29261),
    AOM_ICDF(30494), AOM_ICDF(31631), AOM_ICDF(32768), 0 },
  { AOM_ICDF(11391), AOM_ICDF(18656), AOM_ICDF(23727), AOM_ICDF(26058),
    AOM_ICDF(27788), AOM_ICDF(30278), AOM_ICDF(32768), 0 },
  { AOM_ICDF(8576), AOM_ICDF(13585), AOM_ICDF(17632), AOM_ICDF(20884),
    AOM_ICDF(23948), AOM_ICDF(27152), AOM_ICDF(32768), 0 },
  { AOM_ICDF(15360), AOM_ICDF(24200), AOM_ICDF(26978), AOM_ICDF(30846),
    AOM_ICDF(31409), AOM_ICDF(32545), AOM_ICDF(32768), 0 },
  { AOM_ICDF(9216), AOM_ICDF(14276), AOM_ICDF(19043), AOM_ICDF(22689),
    AOM_ICDF(25799), AOM_ICDF(28712), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
  { AOM_ICDF(9216), AOM_ICDF(14276), AOM_ICDF(19043), AOM_ICDF(22689),
    AOM_ICDF(25799), AOM_ICDF(28712), AOM_ICDF(32768), 0 },
  { AOM_ICDF(9216), AOM_ICDF(14276), AOM_ICDF(19043), AOM_ICDF(22689),
    AOM_ICDF(25799), AOM_ICDF(28712), AOM_ICDF(32768), 0 },
  { AOM_ICDF(9216), AOM_ICDF(14276), AOM_ICDF(19043), AOM_ICDF(22689),
    AOM_ICDF(25799), AOM_ICDF(28712), AOM_ICDF(32768), 0 },
#endif
};

// When palette mode is enabled, following probability tables indicate the
// probabilities to code the "is_palette" bit (i.e. the bit that indicates
// if this block uses palette mode or DC_PRED mode).
const aom_prob av1_default_palette_y_mode_prob
    [PALETTE_BLOCK_SIZES][PALETTE_Y_MODE_CONTEXTS] = {
      { 240, 180, 100 }, { 240, 180, 100 }, { 240, 180, 100 },
      { 240, 180, 100 }, { 240, 180, 100 }, { 240, 180, 100 },
      { 240, 180, 100 }, { 240, 180, 100 }, { 240, 180, 100 },
      { 240, 180, 100 },
#if CONFIG_EXT_PARTITION
      { 240, 180, 100 }, { 240, 180, 100 }, { 240, 180, 100 },
#endif  // CONFIG_EXT_PARTITION
    };

const aom_prob av1_default_palette_uv_mode_prob[PALETTE_UV_MODE_CONTEXTS] = {
  253, 229
};

const aom_cdf_prob default_palette_y_color_index_cdf
    [PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS][CDF_SIZE(PALETTE_COLORS)] = {
      {
          { AOM_ICDF(29568), AOM_ICDF(32768), 0, 0, 0, 0, 0, 0, 0 },
          { AOM_ICDF(16384), AOM_ICDF(32768), 0, 0, 0, 0, 0, 0, 0 },
          { AOM_ICDF(8832), AOM_ICDF(32768), 0, 0, 0, 0, 0, 0, 0 },
          { AOM_ICDF(28672), AOM_ICDF(32768), 0, 0, 0, 0, 0, 0, 0 },
          { AOM_ICDF(31872), AOM_ICDF(32768), 0, 0, 0, 0, 0, 0, 0 },
      },
      {
          { AOM_ICDF(28032), AOM_ICDF(30326), AOM_ICDF(32768), 0, 0, 0, 0, 0,
            0 },
          { AOM_ICDF(11647), AOM_ICDF(27405), AOM_ICDF(32768), 0, 0, 0, 0, 0,
            0 },
          { AOM_ICDF(4352), AOM_ICDF(30659), AOM_ICDF(32768), 0, 0, 0, 0, 0,
            0 },
          { AOM_ICDF(23552), AOM_ICDF(27800), AOM_ICDF(32768), 0, 0, 0, 0, 0,
            0 },
          { AOM_ICDF(32256), AOM_ICDF(32504), AOM_ICDF(32768), 0, 0, 0, 0, 0,
            0 },
      },
      {
          { AOM_ICDF(26112), AOM_ICDF(28374), AOM_ICDF(30039), AOM_ICDF(32768),
            0, 0, 0, 0, 0 },
          { AOM_ICDF(9472), AOM_ICDF(22576), AOM_ICDF(27712), AOM_ICDF(32768),
            0, 0, 0, 0, 0 },
          { AOM_ICDF(6656), AOM_ICDF(26138), AOM_ICDF(29608), AOM_ICDF(32768),
            0, 0, 0, 0, 0 },
          { AOM_ICDF(19328), AOM_ICDF(23791), AOM_ICDF(28946), AOM_ICDF(32768),
            0, 0, 0, 0, 0 },
          { AOM_ICDF(31744), AOM_ICDF(31984), AOM_ICDF(32336), AOM_ICDF(32768),
            0, 0, 0, 0, 0 },
      },
      {
          { AOM_ICDF(27904), AOM_ICDF(29215), AOM_ICDF(30075), AOM_ICDF(31190),
            AOM_ICDF(32768), 0, 0, 0, 0 },
          { AOM_ICDF(9728), AOM_ICDF(22598), AOM_ICDF(26134), AOM_ICDF(29425),
            AOM_ICDF(32768), 0, 0, 0, 0 },
          { AOM_ICDF(2688), AOM_ICDF(30066), AOM_ICDF(31058), AOM_ICDF(31933),
            AOM_ICDF(32768), 0, 0, 0, 0 },
          { AOM_ICDF(22015), AOM_ICDF(25039), AOM_ICDF(27726), AOM_ICDF(29932),
            AOM_ICDF(32768), 0, 0, 0, 0 },
          { AOM_ICDF(32383), AOM_ICDF(32482), AOM_ICDF(32554), AOM_ICDF(32660),
            AOM_ICDF(32768), 0, 0, 0, 0 },
      },
      {
          { AOM_ICDF(24319), AOM_ICDF(26299), AOM_ICDF(27486), AOM_ICDF(28600),
            AOM_ICDF(29804), AOM_ICDF(32768), 0, 0, 0 },
          { AOM_ICDF(7935), AOM_ICDF(18217), AOM_ICDF(21116), AOM_ICDF(25440),
            AOM_ICDF(28589), AOM_ICDF(32768), 0, 0, 0 },
          { AOM_ICDF(6656), AOM_ICDF(25016), AOM_ICDF(27105), AOM_ICDF(28698),
            AOM_ICDF(30399), AOM_ICDF(32768), 0, 0, 0 },
          { AOM_ICDF(19967), AOM_ICDF(24117), AOM_ICDF(26550), AOM_ICDF(28566),
            AOM_ICDF(30224), AOM_ICDF(32768), 0, 0, 0 },
          { AOM_ICDF(31359), AOM_ICDF(31607), AOM_ICDF(31775), AOM_ICDF(31977),
            AOM_ICDF(32258), AOM_ICDF(32768), 0, 0, 0 },
      },
      {
          { AOM_ICDF(26368), AOM_ICDF(27768), AOM_ICDF(28588), AOM_ICDF(29274),
            AOM_ICDF(29997), AOM_ICDF(30917), AOM_ICDF(32768), 0, 0 },
          { AOM_ICDF(8960), AOM_ICDF(18260), AOM_ICDF(20810), AOM_ICDF(23986),
            AOM_ICDF(26627), AOM_ICDF(28882), AOM_ICDF(32768), 0, 0 },
          { AOM_ICDF(7295), AOM_ICDF(24111), AOM_ICDF(25836), AOM_ICDF(27515),
            AOM_ICDF(29033), AOM_ICDF(30769), AOM_ICDF(32768), 0, 0 },
          { AOM_ICDF(22016), AOM_ICDF(25208), AOM_ICDF(27305), AOM_ICDF(28159),
            AOM_ICDF(29221), AOM_ICDF(30274), AOM_ICDF(32768), 0, 0 },
          { AOM_ICDF(31744), AOM_ICDF(31932), AOM_ICDF(32050), AOM_ICDF(32199),
            AOM_ICDF(32335), AOM_ICDF(32521), AOM_ICDF(32768), 0, 0 },
      },
      {
          { AOM_ICDF(26624), AOM_ICDF(27872), AOM_ICDF(28599), AOM_ICDF(29153),
            AOM_ICDF(29633), AOM_ICDF(30172), AOM_ICDF(30841), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(6655), AOM_ICDF(17569), AOM_ICDF(19587), AOM_ICDF(23345),
            AOM_ICDF(25884), AOM_ICDF(28088), AOM_ICDF(29678), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(3584), AOM_ICDF(27296), AOM_ICDF(28429), AOM_ICDF(29158),
            AOM_ICDF(30032), AOM_ICDF(30780), AOM_ICDF(31572), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(23551), AOM_ICDF(25855), AOM_ICDF(27070), AOM_ICDF(27893),
            AOM_ICDF(28597), AOM_ICDF(29721), AOM_ICDF(30970), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(32128), AOM_ICDF(32173), AOM_ICDF(32245), AOM_ICDF(32337),
            AOM_ICDF(32416), AOM_ICDF(32500), AOM_ICDF(32609), AOM_ICDF(32768),
            0 },
      },
    };

const aom_cdf_prob default_palette_uv_color_index_cdf
    [PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS][CDF_SIZE(PALETTE_COLORS)] = {
      {
          { AOM_ICDF(29824), AOM_ICDF(32768), 0, 0, 0, 0, 0, 0, 0 },
          { AOM_ICDF(16384), AOM_ICDF(32768), 0, 0, 0, 0, 0, 0, 0 },
          { AOM_ICDF(8832), AOM_ICDF(32768), 0, 0, 0, 0, 0, 0, 0 },
          { AOM_ICDF(30720), AOM_ICDF(32768), 0, 0, 0, 0, 0, 0, 0 },
          { AOM_ICDF(31744), AOM_ICDF(32768), 0, 0, 0, 0, 0, 0, 0 },
      },
      {
          { AOM_ICDF(27648), AOM_ICDF(30208), AOM_ICDF(32768), 0, 0, 0, 0, 0,
            0 },
          { AOM_ICDF(14080), AOM_ICDF(26563), AOM_ICDF(32768), 0, 0, 0, 0, 0,
            0 },
          { AOM_ICDF(5120), AOM_ICDF(30932), AOM_ICDF(32768), 0, 0, 0, 0, 0,
            0 },
          { AOM_ICDF(24448), AOM_ICDF(27828), AOM_ICDF(32768), 0, 0, 0, 0, 0,
            0 },
          { AOM_ICDF(31616), AOM_ICDF(32219), AOM_ICDF(32768), 0, 0, 0, 0, 0,
            0 },
      },
      {
          { AOM_ICDF(25856), AOM_ICDF(28259), AOM_ICDF(30584), AOM_ICDF(32768),
            0, 0, 0, 0, 0 },
          { AOM_ICDF(11520), AOM_ICDF(22476), AOM_ICDF(27944), AOM_ICDF(32768),
            0, 0, 0, 0, 0 },
          { AOM_ICDF(8064), AOM_ICDF(26882), AOM_ICDF(30308), AOM_ICDF(32768),
            0, 0, 0, 0, 0 },
          { AOM_ICDF(19455), AOM_ICDF(23823), AOM_ICDF(29134), AOM_ICDF(32768),
            0, 0, 0, 0, 0 },
          { AOM_ICDF(30848), AOM_ICDF(31501), AOM_ICDF(32174), AOM_ICDF(32768),
            0, 0, 0, 0, 0 },
      },
      {
          { AOM_ICDF(26751), AOM_ICDF(28020), AOM_ICDF(29541), AOM_ICDF(31230),
            AOM_ICDF(32768), 0, 0, 0, 0 },
          { AOM_ICDF(12032), AOM_ICDF(26045), AOM_ICDF(30772), AOM_ICDF(31497),
            AOM_ICDF(32768), 0, 0, 0, 0 },
          { AOM_ICDF(1280), AOM_ICDF(32153), AOM_ICDF(32458), AOM_ICDF(32560),
            AOM_ICDF(32768), 0, 0, 0, 0 },
          { AOM_ICDF(23424), AOM_ICDF(24154), AOM_ICDF(29201), AOM_ICDF(29856),
            AOM_ICDF(32768), 0, 0, 0, 0 },
          { AOM_ICDF(32256), AOM_ICDF(32402), AOM_ICDF(32561), AOM_ICDF(32682),
            AOM_ICDF(32768), 0, 0, 0, 0 },
      },
      {
          { AOM_ICDF(24576), AOM_ICDF(26720), AOM_ICDF(28114), AOM_ICDF(28950),
            AOM_ICDF(31694), AOM_ICDF(32768), 0, 0, 0 },
          { AOM_ICDF(7551), AOM_ICDF(16613), AOM_ICDF(20462), AOM_ICDF(25269),
            AOM_ICDF(29077), AOM_ICDF(32768), 0, 0, 0 },
          { AOM_ICDF(6272), AOM_ICDF(23039), AOM_ICDF(25623), AOM_ICDF(28163),
            AOM_ICDF(30861), AOM_ICDF(32768), 0, 0, 0 },
          { AOM_ICDF(17024), AOM_ICDF(18808), AOM_ICDF(20771), AOM_ICDF(27941),
            AOM_ICDF(29845), AOM_ICDF(32768), 0, 0, 0 },
          { AOM_ICDF(31616), AOM_ICDF(31936), AOM_ICDF(32079), AOM_ICDF(32321),
            AOM_ICDF(32546), AOM_ICDF(32768), 0, 0, 0 },
      },
      {
          { AOM_ICDF(23296), AOM_ICDF(25590), AOM_ICDF(27833), AOM_ICDF(29337),
            AOM_ICDF(29954), AOM_ICDF(31229), AOM_ICDF(32768), 0, 0 },
          { AOM_ICDF(7552), AOM_ICDF(13659), AOM_ICDF(16570), AOM_ICDF(21695),
            AOM_ICDF(24506), AOM_ICDF(27701), AOM_ICDF(32768), 0, 0 },
          { AOM_ICDF(6911), AOM_ICDF(24788), AOM_ICDF(26284), AOM_ICDF(27753),
            AOM_ICDF(29575), AOM_ICDF(30872), AOM_ICDF(32768), 0, 0 },
          { AOM_ICDF(17535), AOM_ICDF(22236), AOM_ICDF(24457), AOM_ICDF(26242),
            AOM_ICDF(27363), AOM_ICDF(30191), AOM_ICDF(32768), 0, 0 },
          { AOM_ICDF(30592), AOM_ICDF(31289), AOM_ICDF(31745), AOM_ICDF(31921),
            AOM_ICDF(32149), AOM_ICDF(32321), AOM_ICDF(32768), 0, 0 },
      },
      {
          { AOM_ICDF(22016), AOM_ICDF(24242), AOM_ICDF(25141), AOM_ICDF(27137),
            AOM_ICDF(27797), AOM_ICDF(29331), AOM_ICDF(30848), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(8063), AOM_ICDF(13564), AOM_ICDF(16940), AOM_ICDF(21948),
            AOM_ICDF(24568), AOM_ICDF(25689), AOM_ICDF(26989), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(6528), AOM_ICDF(27028), AOM_ICDF(27835), AOM_ICDF(28741),
            AOM_ICDF(30031), AOM_ICDF(31795), AOM_ICDF(32285), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(18047), AOM_ICDF(23797), AOM_ICDF(25444), AOM_ICDF(26274),
            AOM_ICDF(27111), AOM_ICDF(27929), AOM_ICDF(30367), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(30208), AOM_ICDF(30628), AOM_ICDF(31046), AOM_ICDF(31658),
            AOM_ICDF(31762), AOM_ICDF(32367), AOM_ICDF(32469), AOM_ICDF(32768),
            0 },
      }
    };

#define MAX_COLOR_CONTEXT_HASH 8
// Negative values are invalid
static const int palette_color_index_context_lookup[MAX_COLOR_CONTEXT_HASH +
                                                    1] = { -1, -1, 0, -1, -1,
                                                           4,  3,  2, 1 };

#endif  // CONFIG_PALETTE

// The transform size is coded as an offset to the smallest transform
// block size.
const aom_tree_index av1_tx_size_tree[MAX_TX_DEPTH][TREE_SIZE(TX_SIZES)] = {
  {
      // Max tx_size is 8X8
      -0, -1,
  },
  {
      // Max tx_size is 16X16
      -0, 2, -1, -2,
  },
  {
      // Max tx_size is 32X32
      -0, 2, -1, 4, -2, -3,
  },
#if CONFIG_TX64X64
  {
      // Max tx_size is 64X64
      -0, 2, -1, 4, -2, 6, -3, -4,
  },
#endif  // CONFIG_TX64X64
};

static const aom_prob default_tx_size_prob[MAX_TX_DEPTH][TX_SIZE_CONTEXTS]
                                          [MAX_TX_DEPTH] = {
                                            {
                                                // Max tx_size is 8X8
                                                { 100 },
                                                { 66 },
                                            },
                                            {
                                                // Max tx_size is 16X16
                                                { 20, 152 },
                                                { 15, 101 },
                                            },
                                            {
                                                // Max tx_size is 32X32
                                                { 3, 136, 37 },
                                                { 5, 52, 13 },
                                            },
#if CONFIG_TX64X64
                                            {
                                                // Max tx_size is 64X64
                                                { 1, 64, 136, 127 },
                                                { 1, 32, 52, 67 },
                                            },
#endif  // CONFIG_TX64X64
                                          };

#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
static const aom_prob default_quarter_tx_size_prob = 192;
#endif

#if CONFIG_LOOP_RESTORATION
const aom_tree_index
    av1_switchable_restore_tree[TREE_SIZE(RESTORE_SWITCHABLE_TYPES)] = {
      -RESTORE_NONE, 2, -RESTORE_WIENER, -RESTORE_SGRPROJ,
    };

static const aom_prob
    default_switchable_restore_prob[RESTORE_SWITCHABLE_TYPES - 1] = {
      32, 128,
    };
#endif  // CONFIG_LOOP_RESTORATION

#if CONFIG_PALETTE
#define NUM_PALETTE_NEIGHBORS 3  // left, top-left and top.
int av1_get_palette_color_index_context(const uint8_t *color_map, int stride,
                                        int r, int c, int palette_size,
                                        uint8_t *color_order, int *color_idx) {
  int i;
  // The +10 below should not be needed. But we get a warning "array subscript
  // is above array bounds [-Werror=array-bounds]" without it, possibly due to
  // this (or similar) bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59124
  int scores[PALETTE_MAX_SIZE + 10];
  const int weights[NUM_PALETTE_NEIGHBORS] = { 2, 1, 2 };
  const int hash_multipliers[NUM_PALETTE_NEIGHBORS] = { 1, 2, 2 };
  int color_index_ctx_hash;
  int color_index_ctx;
  int color_neighbors[NUM_PALETTE_NEIGHBORS];
  int inverse_color_order[PALETTE_MAX_SIZE];
  assert(palette_size <= PALETTE_MAX_SIZE);
  assert(r > 0 || c > 0);

  // Get color indices of neighbors.
  color_neighbors[0] = (c - 1 >= 0) ? color_map[r * stride + c - 1] : -1;
  color_neighbors[1] =
      (c - 1 >= 0 && r - 1 >= 0) ? color_map[(r - 1) * stride + c - 1] : -1;
  color_neighbors[2] = (r - 1 >= 0) ? color_map[(r - 1) * stride + c] : -1;

  for (i = 0; i < PALETTE_MAX_SIZE; ++i) {
    color_order[i] = i;
    inverse_color_order[i] = i;
  }
  memset(scores, 0, PALETTE_MAX_SIZE * sizeof(scores[0]));
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    if (color_neighbors[i] >= 0) {
      scores[color_neighbors[i]] += weights[i];
    }
  }

  // Get the top NUM_PALETTE_NEIGHBORS scores (sorted from large to small).
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    int max = scores[i];
    int max_idx = i;
    int j;
    for (j = i + 1; j < palette_size; ++j) {
      if (scores[j] > max) {
        max = scores[j];
        max_idx = j;
      }
    }
    if (max_idx != i) {
      // Move the score at index 'max_idx' to index 'i', and shift the scores
      // from 'i' to 'max_idx - 1' by 1.
      const int max_score = scores[max_idx];
      const uint8_t max_color_order = color_order[max_idx];
      int k;
      for (k = max_idx; k > i; --k) {
        scores[k] = scores[k - 1];
        color_order[k] = color_order[k - 1];
        inverse_color_order[color_order[k]] = k;
      }
      scores[i] = max_score;
      color_order[i] = max_color_order;
      inverse_color_order[color_order[i]] = i;
    }
  }

  // Get hash value of context.
  color_index_ctx_hash = 0;
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    color_index_ctx_hash += scores[i] * hash_multipliers[i];
  }
  assert(color_index_ctx_hash > 0);
  assert(color_index_ctx_hash <= MAX_COLOR_CONTEXT_HASH);

  // Lookup context from hash.
  color_index_ctx = palette_color_index_context_lookup[color_index_ctx_hash];
  assert(color_index_ctx >= 0);
  assert(color_index_ctx < PALETTE_COLOR_INDEX_CONTEXTS);

  if (color_idx != NULL) {
    *color_idx = inverse_color_order[color_map[r * stride + c]];
  }
  return color_index_ctx;
}
#undef NUM_PALETTE_NEIGHBORS
#undef MAX_COLOR_CONTEXT_HASH

#endif  // CONFIG_PALETTE

#if CONFIG_VAR_TX
static const aom_prob default_txfm_partition_probs[TXFM_PARTITION_CONTEXTS] = {
  250, 231, 212, 241, 166, 66, 241, 230, 135, 243, 154, 64, 248, 161, 63, 128,
};
#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob
    default_txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(2)] = {
      { AOM_ICDF(250 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(231 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(212 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(241 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(166 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(66 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(241 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(230 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(135 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(243 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(154 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(64 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(248 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(161 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(63 * 128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0 }
    };
#endif  // CONFIG_NEW_MULTISYMBOL
#endif

static const aom_prob default_skip_probs[SKIP_CONTEXTS] = { 192, 128, 64 };
#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob default_skip_cdfs[SKIP_CONTEXTS][CDF_SIZE(2)] = {
  { AOM_ICDF(24576), AOM_ICDF(32768), 0 },
  { AOM_ICDF(16384), AOM_ICDF(32768), 0 },
  { AOM_ICDF(8192), AOM_ICDF(32768), 0 }
};
#endif

#if CONFIG_DUAL_FILTER
#if USE_EXTRA_FILTER
static const aom_prob default_switchable_interp_prob
    [SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS - 1] = {
      { 235, 192, 128 }, { 36, 243, 48 },   { 34, 16, 128 },
      { 34, 16, 128 },   { 149, 160, 128 }, { 235, 192, 128 },
      { 36, 243, 48 },   { 34, 16, 128 },   { 34, 16, 128 },
      { 149, 160, 128 }, { 235, 192, 128 }, { 36, 243, 48 },
      { 34, 16, 128 },   { 34, 16, 128 },   { 149, 160, 128 },
      { 235, 192, 128 }, { 36, 243, 48 },   { 34, 16, 128 },
      { 34, 16, 128 },   { 149, 160, 128 },
    };
#else   // USE_EXTRA_FILTER
static const aom_prob default_switchable_interp_prob
    [SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS - 1] = {
      { 252, 199 }, { 22, 255 }, { 4, 2 }, { 238, 146 },
      { 253, 66 },  { 24, 255 }, { 2, 1 }, { 198, 41 },
      { 250, 177 }, { 16, 255 }, { 3, 4 }, { 226, 162 },
      { 247, 38 },  { 33, 253 }, { 1, 1 }, { 136, 14 },
    };
#endif  // USE_EXTRA_FILTER
#else   // CONFIG_DUAL_FILTER
static const aom_prob default_switchable_interp_prob[SWITCHABLE_FILTER_CONTEXTS]
                                                    [SWITCHABLE_FILTERS - 1] = {
                                                      { 235, 162 },
                                                      { 36, 255 },
                                                      { 34, 3 },
                                                      { 149, 144 },
                                                    };
#endif  // CONFIG_DUAL_FILTER

#if CONFIG_EXT_TX
/* clang-format off */
const aom_tree_index av1_ext_tx_inter_tree[EXT_TX_SETS_INTER]
                                           [TREE_SIZE(TX_TYPES)] = {
  { // ToDo(yaowu): remove used entry 0.
    0
  }, {
    -IDTX, 2,
    4, 14,
    6, 8,
    -V_DCT, -H_DCT,
    10, 12,
    -V_ADST, -H_ADST,
    -V_FLIPADST, -H_FLIPADST,
    -DCT_DCT, 16,
    18, 24,
    20, 22,
    -ADST_DCT, -DCT_ADST,
    -FLIPADST_DCT, -DCT_FLIPADST,
    26, 28,
    -ADST_ADST, -FLIPADST_FLIPADST,
    -ADST_FLIPADST, -FLIPADST_ADST
  }, {
    -IDTX, 2,
    4, 6,
    -V_DCT, -H_DCT,
    -DCT_DCT, 8,
    10, 16,
    12, 14,
    -ADST_DCT, -DCT_ADST,
    -FLIPADST_DCT, -DCT_FLIPADST,
    18, 20,
    -ADST_ADST, -FLIPADST_FLIPADST,
    -ADST_FLIPADST, -FLIPADST_ADST
  }, {
    -IDTX, -DCT_DCT,
  },
#if CONFIG_MRC_TX
  {
    -IDTX, 2, -DCT_DCT, -MRC_DCT,
  }
#endif  // CONFIG_MRC_TX
};

const aom_tree_index av1_ext_tx_intra_tree[EXT_TX_SETS_INTRA]
                                           [TREE_SIZE(TX_TYPES)] = {
  {  // ToDo(yaowu): remove unused entry 0.
    0
  }, {
    -IDTX, 2,
    -DCT_DCT, 4,
    6, 8,
    -V_DCT, -H_DCT,
    -ADST_ADST, 10,
    -ADST_DCT, -DCT_ADST,
  }, {
    -IDTX, 2,
    -DCT_DCT, 4,
    -ADST_ADST, 6,
    -ADST_DCT, -DCT_ADST,
  },
#if CONFIG_MRC_TX
  {
    -DCT_DCT, -MRC_DCT,
  }
#endif  // CONFIG_MRC_TX
};
/* clang-format on */

static const aom_prob
    default_inter_ext_tx_prob[EXT_TX_SETS_INTER][EXT_TX_SIZES][TX_TYPES - 1] = {
      {
// ToDo(yaowu): remove unused entry 0.
#if CONFIG_CHROMA_2X2
          { 0 },
#endif
          { 0 },
          { 0 },
          { 0 },
          { 0 },
      },
      {
#if CONFIG_CHROMA_2X2
          { 0 },
#endif
          { 10, 24, 30, 128, 128, 128, 128, 112, 160, 128, 128, 128, 128, 128,
            128 },
          { 10, 24, 30, 128, 128, 128, 128, 112, 160, 128, 128, 128, 128, 128,
            128 },
          { 10, 24, 30, 128, 128, 128, 128, 112, 160, 128, 128, 128, 128, 128,
            128 },
          { 10, 24, 30, 128, 128, 128, 128, 112, 160, 128, 128, 128, 128, 128,
            128 },
      },
      {
#if CONFIG_CHROMA_2X2
          { 0 },
#endif
          { 10, 30, 128, 112, 160, 128, 128, 128, 128, 128, 128 },
          { 10, 30, 128, 112, 160, 128, 128, 128, 128, 128, 128 },
          { 10, 30, 128, 112, 160, 128, 128, 128, 128, 128, 128 },
          { 10, 30, 128, 112, 160, 128, 128, 128, 128, 128, 128 },
      },
      {
#if CONFIG_CHROMA_2X2
          { 0 },
#endif
          { 12 },
          { 12 },
          { 12 },
          { 12 },
      },
#if CONFIG_MRC_TX
      {
#if CONFIG_CHROMA_2X2
          { 0 },
#endif
          { 12, 128 },
          { 12, 128 },
          { 12, 128 },
          { 12, 128 },
      }
#endif  // CONFIG_MRC_TX
    };

// TODO(urvang): 3rd context should be tx_type instead of intra mode just like
// the baseline.
static const aom_prob
    default_intra_ext_tx_prob[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
                             [TX_TYPES - 1] = {
                               {
// ToDo(yaowu): remove unused entry 0.
#if CONFIG_CHROMA_2X2
                                   {
                                       { 0 },
                                   },
#endif
                                   {
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
#if CONFIG_ALT_INTRA
                                       { 0 },
#if CONFIG_SMOOTH_HV
                                       { 0 },
                                       { 0 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 0 },
                                   },
                                   {
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
#if CONFIG_ALT_INTRA
                                       { 0 },
#if CONFIG_SMOOTH_HV
                                       { 0 },
                                       { 0 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 0 },
                                   },
                                   {
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
#if CONFIG_ALT_INTRA
                                       { 0 },
#if CONFIG_SMOOTH_HV
                                       { 0 },
                                       { 0 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 0 },
                                   },
                                   {
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
#if CONFIG_ALT_INTRA
                                       { 0 },
#if CONFIG_SMOOTH_HV
                                       { 0 },
                                       { 0 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 0 },
                                   },
                               },
                               {
#if CONFIG_CHROMA_2X2
                                   {
                                       { 0 },
                                   },
#endif
                                   {
                                       { 8, 224, 32, 128, 64, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 9, 200, 32, 128, 64, 128 },
                                       { 8, 8, 32, 128, 224, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 23, 32, 128, 80, 176 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
#if CONFIG_SMOOTH_HV
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 32, 32, 128, 16, 64 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
                                   },
                                   {
                                       { 8, 224, 32, 128, 64, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 9, 200, 32, 128, 64, 128 },
                                       { 8, 8, 32, 128, 224, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 23, 32, 128, 80, 176 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
#if CONFIG_SMOOTH_HV
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 32, 32, 128, 16, 64 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
                                   },
                                   {
                                       { 8, 224, 32, 128, 64, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 9, 200, 32, 128, 64, 128 },
                                       { 8, 8, 32, 128, 224, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 23, 32, 128, 80, 176 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
#if CONFIG_SMOOTH_HV
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 32, 32, 128, 16, 64 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
                                   },
                                   {
                                       { 8, 224, 32, 128, 64, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 9, 200, 32, 128, 64, 128 },
                                       { 8, 8, 32, 128, 224, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 23, 32, 128, 80, 176 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
#if CONFIG_SMOOTH_HV
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 32, 32, 128, 16, 64 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
                                   },
                               },
                               {
#if CONFIG_CHROMA_2X2
                                   {
                                       { 0 },
                                   },
#endif
                                   {
                                       { 8, 224, 64, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 9, 200, 64, 128 },
                                       { 8, 8, 224, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 10, 23, 80, 176 },
                                       { 10, 23, 80, 176 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
#if CONFIG_SMOOTH_HV
                                       { 10, 32, 16, 64 },
                                       { 10, 32, 16, 64 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
                                   },
                                   {
                                       { 8, 224, 64, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 9, 200, 64, 128 },
                                       { 8, 8, 224, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 10, 23, 80, 176 },
                                       { 10, 23, 80, 176 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
#if CONFIG_SMOOTH_HV
                                       { 10, 32, 16, 64 },
                                       { 10, 32, 16, 64 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
                                   },
                                   {
                                       { 8, 224, 64, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 9, 200, 64, 128 },
                                       { 8, 8, 224, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 10, 23, 80, 176 },
                                       { 10, 23, 80, 176 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
#if CONFIG_SMOOTH_HV
                                       { 10, 32, 16, 64 },
                                       { 10, 32, 16, 64 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
                                   },
                                   {
                                       { 8, 224, 64, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 9, 200, 64, 128 },
                                       { 8, 8, 224, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 10, 23, 80, 176 },
                                       { 10, 23, 80, 176 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
#if CONFIG_SMOOTH_HV
                                       { 10, 32, 16, 64 },
                                       { 10, 32, 16, 64 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
                                   },
                               },
#if CONFIG_MRC_TX
                               {
// ToDo(yaowu): remove unused entry 0.
#if CONFIG_CHROMA_2X2
                                   {
                                       { 0 },
                                   },
#endif
                                   {
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
#if CONFIG_ALT_INTRA
                                       { 128 },
#if CONFIG_SMOOTH_HV
                                       { 128 },
                                       { 128 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 128 },
                                   },
                                   {
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
#if CONFIG_ALT_INTRA
                                       { 128 },
#if CONFIG_SMOOTH_HV
                                       { 128 },
                                       { 128 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 128 },
                                   },
                                   {
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
#if CONFIG_ALT_INTRA
                                       { 128 },
#if CONFIG_SMOOTH_HV
                                       { 128 },
                                       { 128 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 128 },
                                   },
                                   {
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
                                       { 128 },
#if CONFIG_ALT_INTRA
                                       { 128 },
#if CONFIG_SMOOTH_HV
                                       { 128 },
                                       { 128 },
#endif  //  CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
                                       { 128 },
                                   },
                               },

#endif  // CONFIG_MRC_TX
                             };
#else  // !CONFIG_EXT_TX

/* clang-format off */
#if CONFIG_MRC_TX
const aom_tree_index av1_ext_tx_tree[TREE_SIZE(TX_TYPES)] = {
  -DCT_DCT, 2,
  -MRC_DCT, 4,
  -ADST_ADST, 6,
  -ADST_DCT, -DCT_ADST
};
#else
const aom_tree_index av1_ext_tx_tree[TREE_SIZE(TX_TYPES)] = {
  -DCT_DCT, 2,
  -ADST_ADST, 4,
  -ADST_DCT, -DCT_ADST
};
#endif  // CONFIG_MRC_TX
/* clang-format on */

int av1_ext_tx_ind[TX_TYPES];
int av1_ext_tx_inv[TX_TYPES];

#if CONFIG_MRC_TX
static const aom_prob default_intra_ext_tx_prob[EXT_TX_SIZES][TX_TYPES]
                                               [TX_TYPES - 1] = {
#if CONFIG_CHROMA_2X2
                                                 { { 240, 1, 85, 128 },
                                                   { 4, 1, 1, 248 },
                                                   { 4, 1, 1, 8 },
                                                   { 4, 1, 248, 128 },
                                                   { 4, 1, 248, 128 } },
#endif
                                                 { { 240, 1, 85, 128 },
                                                   { 4, 1, 1, 248 },
                                                   { 4, 1, 1, 8 },
                                                   { 4, 1, 248, 128 },
                                                   { 4, 1, 248, 128 } },
                                                 { { 244, 1, 85, 128 },
                                                   { 8, 1, 2, 248 },
                                                   { 8, 1, 2, 8 },
                                                   { 8, 1, 248, 128 },
                                                   { 4, 1, 248, 128 } },
                                                 { { 248, 128, 85, 128 },
                                                   { 16, 128, 4, 248 },
                                                   { 16, 128, 4, 8 },
                                                   { 16, 128, 248, 128 },
                                                   { 4, 1, 248, 128 } },
                                               };

static const aom_prob default_inter_ext_tx_prob[EXT_TX_SIZES][TX_TYPES - 1] = {
#if CONFIG_CHROMA_2X2
  { 160, 1, 85, 128 },
#endif
  { 160, 1, 85, 128 },
  { 176, 1, 85, 128 },
  { 192, 128, 85, 128 },
};
#else
static const aom_prob
    default_intra_ext_tx_prob[EXT_TX_SIZES][TX_TYPES][TX_TYPES - 1] = {
#if CONFIG_CHROMA_2X2
      { { 240, 85, 128 }, { 4, 1, 248 }, { 4, 1, 8 }, { 4, 248, 128 } },
#endif
      { { 240, 85, 128 }, { 4, 1, 248 }, { 4, 1, 8 }, { 4, 248, 128 } },
      { { 244, 85, 128 }, { 8, 2, 248 }, { 8, 2, 8 }, { 8, 248, 128 } },
      { { 248, 85, 128 }, { 16, 4, 248 }, { 16, 4, 8 }, { 16, 248, 128 } },
    };

static const aom_prob default_inter_ext_tx_prob[EXT_TX_SIZES][TX_TYPES - 1] = {
#if CONFIG_CHROMA_2X2
  { 160, 85, 128 },
#endif
  { 160, 85, 128 },
  { 176, 85, 128 },
  { 192, 85, 128 },
};
#endif  // CONFIG_MRC_TX
#endif  // CONFIG_EXT_TX

#if CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP
static const aom_prob
    default_intra_filter_probs[INTRA_FILTERS + 1][INTRA_FILTERS - 1] = {
      { 98, 63, 60 }, { 98, 82, 80 }, { 94, 65, 103 },
      { 49, 25, 24 }, { 72, 38, 50 },
    };
const aom_tree_index av1_intra_filter_tree[TREE_SIZE(INTRA_FILTERS)] = {
  -INTRA_FILTER_LINEAR,      2, -INTRA_FILTER_8TAP, 4, -INTRA_FILTER_8TAP_SHARP,
  -INTRA_FILTER_8TAP_SMOOTH,
};
int av1_intra_filter_ind[INTRA_FILTERS];
int av1_intra_filter_inv[INTRA_FILTERS];
#endif  // CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP

#if CONFIG_FILTER_INTRA
static const aom_prob default_filter_intra_probs[2] = { 230, 230 };
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_SUPERTX
static const aom_prob
    default_supertx_prob[PARTITION_SUPERTX_CONTEXTS][TX_SIZES] = {
#if CONFIG_CHROMA_2X2
#if CONFIG_TX64X64
      { 1, 1, 160, 160, 170, 180 }, { 1, 1, 200, 200, 210, 220 },
#else
      { 1, 1, 160, 160, 170 }, { 1, 1, 200, 200, 210 },
#endif  // CONFIG_TX64X64
#else
#if CONFIG_TX64X64
      { 1, 160, 160, 170, 180 }, { 1, 200, 200, 210, 220 },
#else
      { 1, 160, 160, 170 }, { 1, 200, 200, 210 },
#endif  // CONFIG_TX64X64
#endif  // CONFIG_CHROMA_2X2
    };
#endif  // CONFIG_SUPERTX

// FIXME(someone) need real defaults here
static const aom_prob default_segment_tree_probs[SEG_TREE_PROBS] = {
  128, 128, 128, 128, 128, 128, 128
};
// clang-format off
static const aom_prob default_segment_pred_probs[PREDICTION_PROBS] = {
  128, 128, 128
};
#if CONFIG_NEW_MULTISYMBOL
static const aom_cdf_prob
    default_segment_pred_cdf[PREDICTION_PROBS][CDF_SIZE(2)] = {
  { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0},
  { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0},
  { AOM_ICDF(128 * 128), AOM_ICDF(32768), 0}
};
#endif
// clang-format on

#if CONFIG_DUAL_FILTER
#if USE_EXTRA_FILTER
static const aom_cdf_prob
    default_switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS][CDF_SIZE(
        SWITCHABLE_FILTERS)] = {
      { AOM_ICDF(30080), AOM_ICDF(31088), AOM_ICDF(32096), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4608), AOM_ICDF(9620), AOM_ICDF(31338), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4352), AOM_ICDF(5240), AOM_ICDF(6128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4352), AOM_ICDF(5240), AOM_ICDF(6128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(19072), AOM_ICDF(23352), AOM_ICDF(27632), AOM_ICDF(32768), 0 },
      { AOM_ICDF(30080), AOM_ICDF(31088), AOM_ICDF(32096), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4608), AOM_ICDF(9620), AOM_ICDF(31338), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4352), AOM_ICDF(5240), AOM_ICDF(6128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4352), AOM_ICDF(5240), AOM_ICDF(6128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(19072), AOM_ICDF(23352), AOM_ICDF(27632), AOM_ICDF(32768), 0 },
      { AOM_ICDF(30080), AOM_ICDF(31088), AOM_ICDF(32096), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4608), AOM_ICDF(9620), AOM_ICDF(31338), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4352), AOM_ICDF(5240), AOM_ICDF(6128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4352), AOM_ICDF(5240), AOM_ICDF(6128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(19072), AOM_ICDF(23352), AOM_ICDF(27632), AOM_ICDF(32768), 0 },
      { AOM_ICDF(30080), AOM_ICDF(31088), AOM_ICDF(32096), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4608), AOM_ICDF(9620), AOM_ICDF(31338), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4352), AOM_ICDF(5240), AOM_ICDF(6128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4352), AOM_ICDF(5240), AOM_ICDF(6128), AOM_ICDF(32768), 0 },
      { AOM_ICDF(19072), AOM_ICDF(23352), AOM_ICDF(27632), AOM_ICDF(32768), 0 }
    };
#else   // USE_EXTRA_FILTER
static const aom_cdf_prob
    default_switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS][CDF_SIZE(
        SWITCHABLE_FILTERS)] = {
      { AOM_ICDF(32256), AOM_ICDF(32654), AOM_ICDF(32768), 0 },
      { AOM_ICDF(2816), AOM_ICDF(32651), AOM_ICDF(32768), 0 },
      { AOM_ICDF(512), AOM_ICDF(764), AOM_ICDF(32768), 0 },
      { AOM_ICDF(30464), AOM_ICDF(31778), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32384), AOM_ICDF(32483), AOM_ICDF(32768), 0 },
      { AOM_ICDF(3072), AOM_ICDF(32652), AOM_ICDF(32768), 0 },
      { AOM_ICDF(256), AOM_ICDF(383), AOM_ICDF(32768), 0 },
      { AOM_ICDF(25344), AOM_ICDF(26533), AOM_ICDF(32768), 0 },
      { AOM_ICDF(32000), AOM_ICDF(32531), AOM_ICDF(32768), 0 },
      { AOM_ICDF(2048), AOM_ICDF(32648), AOM_ICDF(32768), 0 },
      { AOM_ICDF(384), AOM_ICDF(890), AOM_ICDF(32768), 0 },
      { AOM_ICDF(28928), AOM_ICDF(31358), AOM_ICDF(32768), 0 },
      { AOM_ICDF(31616), AOM_ICDF(31787), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4224), AOM_ICDF(32433), AOM_ICDF(32768), 0 },
      { AOM_ICDF(128), AOM_ICDF(256), AOM_ICDF(32768), 0 },
      { AOM_ICDF(17408), AOM_ICDF(18248), AOM_ICDF(32768), 0 }
    };
#endif  // USE_EXTRA_FILTER
#else   // CONFIG_DUAL_FILTER
static const aom_cdf_prob
    default_switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS][CDF_SIZE(
        SWITCHABLE_FILTERS)] = {
      { AOM_ICDF(30080), AOM_ICDF(31781), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4608), AOM_ICDF(32658), AOM_ICDF(32768), 0 },
      { AOM_ICDF(4352), AOM_ICDF(4685), AOM_ICDF(32768), 0 },
      { AOM_ICDF(19072), AOM_ICDF(26776), AOM_ICDF(32768), 0 },
    };
#endif  // CONFIG_DUAL_FILTER

static const aom_cdf_prob default_seg_tree_cdf[CDF_SIZE(MAX_SEGMENTS)] = {
  AOM_ICDF(4096),  AOM_ICDF(8192),  AOM_ICDF(12288),
  AOM_ICDF(16384), AOM_ICDF(20480), AOM_ICDF(24576),
  AOM_ICDF(28672), AOM_ICDF(32768), 0
};

static const aom_cdf_prob
    default_tx_size_cdf[MAX_TX_DEPTH][TX_SIZE_CONTEXTS][CDF_SIZE(MAX_TX_DEPTH +
                                                                 1)] = {
      { { AOM_ICDF(12800), AOM_ICDF(32768), 0 },
        { AOM_ICDF(8448), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(2560), AOM_ICDF(20496), AOM_ICDF(32768), 0 },
        { AOM_ICDF(1920), AOM_ICDF(14091), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(384), AOM_ICDF(17588), AOM_ICDF(19782), AOM_ICDF(32768), 0 },
        { AOM_ICDF(640), AOM_ICDF(7166), AOM_ICDF(8466), AOM_ICDF(32768), 0 } },
#if CONFIG_TX64X64
      { { AOM_ICDF(128), AOM_ICDF(8288), AOM_ICDF(21293), AOM_ICDF(26986),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(128), AOM_ICDF(4208), AOM_ICDF(10009), AOM_ICDF(15965),
          AOM_ICDF(32768), 0 } },
#endif
    };

#if CONFIG_ALT_INTRA
#if CONFIG_SMOOTH_HV
static const aom_cdf_prob
    default_if_y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(INTRA_MODES)] = {
      { AOM_ICDF(7168), AOM_ICDF(8468), AOM_ICDF(11980), AOM_ICDF(15213),
        AOM_ICDF(18579), AOM_ICDF(21075), AOM_ICDF(24090), AOM_ICDF(25954),
        AOM_ICDF(27870), AOM_ICDF(29439), AOM_ICDF(31051), AOM_ICDF(31863),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(11776), AOM_ICDF(21616), AOM_ICDF(23663), AOM_ICDF(25147),
        AOM_ICDF(26060), AOM_ICDF(26828), AOM_ICDF(27246), AOM_ICDF(28066),
        AOM_ICDF(28654), AOM_ICDF(29474), AOM_ICDF(31353), AOM_ICDF(32038),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(14720), AOM_ICDF(21911), AOM_ICDF(23650), AOM_ICDF(25282),
        AOM_ICDF(25740), AOM_ICDF(26108), AOM_ICDF(26316), AOM_ICDF(26896),
        AOM_ICDF(27194), AOM_ICDF(27695), AOM_ICDF(30113), AOM_ICDF(31254),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(18944), AOM_ICDF(27422), AOM_ICDF(28403), AOM_ICDF(29386),
        AOM_ICDF(29405), AOM_ICDF(29460), AOM_ICDF(29550), AOM_ICDF(29588),
        AOM_ICDF(29600), AOM_ICDF(29637), AOM_ICDF(30542), AOM_ICDF(31298),
        AOM_ICDF(32768), 0 },
    };

static const aom_cdf_prob
    default_uv_mode_cdf[INTRA_MODES][CDF_SIZE(UV_INTRA_MODES)] = {
      { AOM_ICDF(23552), AOM_ICDF(23660), AOM_ICDF(26044), AOM_ICDF(28731),
        AOM_ICDF(29093), AOM_ICDF(29590), AOM_ICDF(30000), AOM_ICDF(30465),
        AOM_ICDF(30825), AOM_ICDF(31478), AOM_ICDF(32088), AOM_ICDF(32401),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(2944), AOM_ICDF(3294), AOM_ICDF(26781), AOM_ICDF(27903),
        AOM_ICDF(28179), AOM_ICDF(29237), AOM_ICDF(29430), AOM_ICDF(30317),
        AOM_ICDF(30441), AOM_ICDF(30614), AOM_ICDF(31556), AOM_ICDF(31963),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(4352), AOM_ICDF(4685), AOM_ICDF(5453), AOM_ICDF(28285),
        AOM_ICDF(28641), AOM_ICDF(28927), AOM_ICDF(29092), AOM_ICDF(29279),
        AOM_ICDF(30083), AOM_ICDF(31384), AOM_ICDF(32027), AOM_ICDF(32406),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(17664), AOM_ICDF(17841), AOM_ICDF(20465), AOM_ICDF(22016),
        AOM_ICDF(22364), AOM_ICDF(22916), AOM_ICDF(27149), AOM_ICDF(29498),
        AOM_ICDF(29766), AOM_ICDF(31091), AOM_ICDF(31871), AOM_ICDF(32260),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(16640), AOM_ICDF(16766), AOM_ICDF(18516), AOM_ICDF(20359),
        AOM_ICDF(24964), AOM_ICDF(27591), AOM_ICDF(27915), AOM_ICDF(28389),
        AOM_ICDF(29997), AOM_ICDF(30495), AOM_ICDF(31623), AOM_ICDF(32151),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(13952), AOM_ICDF(14173), AOM_ICDF(18168), AOM_ICDF(19139),
        AOM_ICDF(21064), AOM_ICDF(30601), AOM_ICDF(30889), AOM_ICDF(31410),
        AOM_ICDF(31803), AOM_ICDF(32059), AOM_ICDF(32358), AOM_ICDF(32563),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(15872), AOM_ICDF(15938), AOM_ICDF(17056), AOM_ICDF(21545),
        AOM_ICDF(23947), AOM_ICDF(24667), AOM_ICDF(24920), AOM_ICDF(25196),
        AOM_ICDF(30638), AOM_ICDF(31229), AOM_ICDF(31968), AOM_ICDF(32284),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(16256), AOM_ICDF(16385), AOM_ICDF(17409), AOM_ICDF(23210),
        AOM_ICDF(23628), AOM_ICDF(24009), AOM_ICDF(24967), AOM_ICDF(25546),
        AOM_ICDF(26054), AOM_ICDF(31037), AOM_ICDF(31875), AOM_ICDF(32335),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(14720), AOM_ICDF(14932), AOM_ICDF(19461), AOM_ICDF(20713),
        AOM_ICDF(21073), AOM_ICDF(21852), AOM_ICDF(23430), AOM_ICDF(29631),
        AOM_ICDF(29876), AOM_ICDF(30520), AOM_ICDF(31591), AOM_ICDF(32078),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(16768), AOM_ICDF(17018), AOM_ICDF(20217), AOM_ICDF(22624),
        AOM_ICDF(23484), AOM_ICDF(23698), AOM_ICDF(24300), AOM_ICDF(25193),
        AOM_ICDF(25785), AOM_ICDF(26903), AOM_ICDF(29835), AOM_ICDF(31187),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(16768), AOM_ICDF(17081), AOM_ICDF(21064), AOM_ICDF(23339),
        AOM_ICDF(24047), AOM_ICDF(24264), AOM_ICDF(24829), AOM_ICDF(25759),
        AOM_ICDF(26224), AOM_ICDF(27119), AOM_ICDF(29833), AOM_ICDF(31599),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(17536), AOM_ICDF(17774), AOM_ICDF(20293), AOM_ICDF(23203),
        AOM_ICDF(23906), AOM_ICDF(24094), AOM_ICDF(24636), AOM_ICDF(25303),
        AOM_ICDF(26003), AOM_ICDF(27271), AOM_ICDF(29912), AOM_ICDF(30927),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(17536), AOM_ICDF(18250), AOM_ICDF(23467), AOM_ICDF(27840),
        AOM_ICDF(28058), AOM_ICDF(28626), AOM_ICDF(28853), AOM_ICDF(29541),
        AOM_ICDF(29907), AOM_ICDF(30600), AOM_ICDF(31515), AOM_ICDF(32049),
        AOM_ICDF(32768), 0 },
    };
#else   // !CONFIG_SMOOTH_HV
static const aom_cdf_prob
    default_if_y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(INTRA_MODES)] = {
      { AOM_ICDF(11264), AOM_ICDF(12608), AOM_ICDF(16309), AOM_ICDF(21086),
        AOM_ICDF(23297), AOM_ICDF(24860), AOM_ICDF(27022), AOM_ICDF(28099),
        AOM_ICDF(29631), AOM_ICDF(31126), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9600), AOM_ICDF(11953), AOM_ICDF(16100), AOM_ICDF(20922),
        AOM_ICDF(22756), AOM_ICDF(23913), AOM_ICDF(25435), AOM_ICDF(26724),
        AOM_ICDF(28046), AOM_ICDF(29927), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9344), AOM_ICDF(11540), AOM_ICDF(16515), AOM_ICDF(21763),
        AOM_ICDF(23078), AOM_ICDF(23816), AOM_ICDF(24725), AOM_ICDF(25856),
        AOM_ICDF(26720), AOM_ICDF(28208), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12288), AOM_ICDF(14448), AOM_ICDF(18026), AOM_ICDF(23346),
        AOM_ICDF(23833), AOM_ICDF(24188), AOM_ICDF(24724), AOM_ICDF(25415),
        AOM_ICDF(25817), AOM_ICDF(26876), AOM_ICDF(32768), 0 },
    };

static const aom_cdf_prob
    default_uv_mode_cdf[INTRA_MODES][CDF_SIZE(UV_INTRA_MODES)] = {
      { AOM_ICDF(25472), AOM_ICDF(25558), AOM_ICDF(27783), AOM_ICDF(30779),
        AOM_ICDF(30988), AOM_ICDF(31269), AOM_ICDF(31492), AOM_ICDF(31741),
        AOM_ICDF(32014), AOM_ICDF(32420), AOM_ICDF(32768), 0 },
      { AOM_ICDF(2176), AOM_ICDF(2415), AOM_ICDF(28381), AOM_ICDF(29574),
        AOM_ICDF(29832), AOM_ICDF(30712), AOM_ICDF(30881), AOM_ICDF(31662),
        AOM_ICDF(31761), AOM_ICDF(31922), AOM_ICDF(32768), 0 },
      { AOM_ICDF(3328), AOM_ICDF(3443), AOM_ICDF(4016), AOM_ICDF(31099),
        AOM_ICDF(31272), AOM_ICDF(31420), AOM_ICDF(31504), AOM_ICDF(31608),
        AOM_ICDF(31916), AOM_ICDF(32598), AOM_ICDF(32768), 0 },
      { AOM_ICDF(23424), AOM_ICDF(23534), AOM_ICDF(25915), AOM_ICDF(27831),
        AOM_ICDF(28058), AOM_ICDF(28431), AOM_ICDF(30142), AOM_ICDF(31209),
        AOM_ICDF(31459), AOM_ICDF(32369), AOM_ICDF(32768), 0 },
      { AOM_ICDF(22784), AOM_ICDF(22862), AOM_ICDF(24255), AOM_ICDF(26287),
        AOM_ICDF(28490), AOM_ICDF(29509), AOM_ICDF(29776), AOM_ICDF(30115),
        AOM_ICDF(31203), AOM_ICDF(31674), AOM_ICDF(32768), 0 },
      { AOM_ICDF(19712), AOM_ICDF(19865), AOM_ICDF(23141), AOM_ICDF(24428),
        AOM_ICDF(25731), AOM_ICDF(31377), AOM_ICDF(31622), AOM_ICDF(32047),
        AOM_ICDF(32458), AOM_ICDF(32767), AOM_ICDF(32768), 0 },
      { AOM_ICDF(21376), AOM_ICDF(21421), AOM_ICDF(22130), AOM_ICDF(27688),
        AOM_ICDF(28485), AOM_ICDF(28779), AOM_ICDF(28935), AOM_ICDF(29085),
        AOM_ICDF(31962), AOM_ICDF(32450), AOM_ICDF(32768), 0 },
      { AOM_ICDF(19712), AOM_ICDF(19814), AOM_ICDF(20725), AOM_ICDF(28510),
        AOM_ICDF(28814), AOM_ICDF(29099), AOM_ICDF(29457), AOM_ICDF(29729),
        AOM_ICDF(30133), AOM_ICDF(32408), AOM_ICDF(32768), 0 },
      { AOM_ICDF(19584), AOM_ICDF(19790), AOM_ICDF(23643), AOM_ICDF(25501),
        AOM_ICDF(25913), AOM_ICDF(26673), AOM_ICDF(27578), AOM_ICDF(30923),
        AOM_ICDF(31255), AOM_ICDF(31870), AOM_ICDF(32768), 0 },
      { AOM_ICDF(20864), AOM_ICDF(21004), AOM_ICDF(24129), AOM_ICDF(26308),
        AOM_ICDF(27062), AOM_ICDF(27065), AOM_ICDF(27488), AOM_ICDF(28045),
        AOM_ICDF(28506), AOM_ICDF(29272), AOM_ICDF(32768), 0 },
      { AOM_ICDF(23680), AOM_ICDF(23929), AOM_ICDF(27831), AOM_ICDF(30446),
        AOM_ICDF(30598), AOM_ICDF(31129), AOM_ICDF(31244), AOM_ICDF(31655),
        AOM_ICDF(31868), AOM_ICDF(32234), AOM_ICDF(32768), 0 },
    };
#endif  // CONFIG_SMOOTH_HV
#else   // !CONFIG_ALT_INTRA
static const aom_cdf_prob
    default_if_y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(INTRA_MODES)] = {
      { AOM_ICDF(8320), AOM_ICDF(11376), AOM_ICDF(12880), AOM_ICDF(19959),
        AOM_ICDF(23072), AOM_ICDF(24067), AOM_ICDF(25461), AOM_ICDF(26917),
        AOM_ICDF(29157), AOM_ICDF(32768), 0 },
      { AOM_ICDF(16896), AOM_ICDF(21112), AOM_ICDF(21932), AOM_ICDF(27852),
        AOM_ICDF(28667), AOM_ICDF(28916), AOM_ICDF(29593), AOM_ICDF(30089),
        AOM_ICDF(30905), AOM_ICDF(32768), 0 },
      { AOM_ICDF(22144), AOM_ICDF(25464), AOM_ICDF(26006), AOM_ICDF(30364),
        AOM_ICDF(30583), AOM_ICDF(30655), AOM_ICDF(31183), AOM_ICDF(31400),
        AOM_ICDF(31646), AOM_ICDF(32768), 0 },
      { AOM_ICDF(28288), AOM_ICDF(30650), AOM_ICDF(30964), AOM_ICDF(32288),
        AOM_ICDF(32308), AOM_ICDF(32331), AOM_ICDF(32495), AOM_ICDF(32586),
        AOM_ICDF(32607), AOM_ICDF(32768), 0 },
    };

static const aom_cdf_prob
    default_uv_mode_cdf[INTRA_MODES][CDF_SIZE(UV_INTRA_MODES)] = {
      { AOM_ICDF(15360), AOM_ICDF(15836), AOM_ICDF(20863), AOM_ICDF(27513),
        AOM_ICDF(28269), AOM_ICDF(29048), AOM_ICDF(29455), AOM_ICDF(30154),
        AOM_ICDF(31206), AOM_ICDF(32768), 0 },
      { AOM_ICDF(6144), AOM_ICDF(7392), AOM_ICDF(22657), AOM_ICDF(25981),
        AOM_ICDF(26965), AOM_ICDF(28779), AOM_ICDF(29309), AOM_ICDF(30890),
        AOM_ICDF(31763), AOM_ICDF(32768), 0 },
      { AOM_ICDF(8576), AOM_ICDF(9143), AOM_ICDF(11450), AOM_ICDF(27575),
        AOM_ICDF(28108), AOM_ICDF(28438), AOM_ICDF(28658), AOM_ICDF(28995),
        AOM_ICDF(30410), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12416), AOM_ICDF(12814), AOM_ICDF(16244), AOM_ICDF(22057),
        AOM_ICDF(23492), AOM_ICDF(24700), AOM_ICDF(26213), AOM_ICDF(27954),
        AOM_ICDF(29778), AOM_ICDF(32768), 0 },
      { AOM_ICDF(10624), AOM_ICDF(11057), AOM_ICDF(14619), AOM_ICDF(19415),
        AOM_ICDF(23134), AOM_ICDF(25679), AOM_ICDF(26399), AOM_ICDF(27618),
        AOM_ICDF(30676), AOM_ICDF(32768), 0 },
      { AOM_ICDF(10240), AOM_ICDF(10680), AOM_ICDF(15684), AOM_ICDF(19118),
        AOM_ICDF(21856), AOM_ICDF(27563), AOM_ICDF(28234), AOM_ICDF(29332),
        AOM_ICDF(31278), AOM_ICDF(32768), 0 },
      { AOM_ICDF(11008), AOM_ICDF(11433), AOM_ICDF(14100), AOM_ICDF(22522),
        AOM_ICDF(24365), AOM_ICDF(25330), AOM_ICDF(25737), AOM_ICDF(26341),
        AOM_ICDF(30433), AOM_ICDF(32768), 0 },
      { AOM_ICDF(10880), AOM_ICDF(11308), AOM_ICDF(13991), AOM_ICDF(23645),
        AOM_ICDF(24679), AOM_ICDF(25433), AOM_ICDF(25977), AOM_ICDF(26746),
        AOM_ICDF(28463), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9856), AOM_ICDF(10483), AOM_ICDF(16054), AOM_ICDF(19959),
        AOM_ICDF(21708), AOM_ICDF(23628), AOM_ICDF(24949), AOM_ICDF(28797),
        AOM_ICDF(30658), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12928), AOM_ICDF(14556), AOM_ICDF(22168), AOM_ICDF(27789),
        AOM_ICDF(28543), AOM_ICDF(29663), AOM_ICDF(29893), AOM_ICDF(30645),
        AOM_ICDF(31682), AOM_ICDF(32768), 0 },
    };
#endif  // CONFIG_ALT_INTRA

#if CONFIG_EXT_PARTITION_TYPES
static const aom_cdf_prob
    default_partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(EXT_PARTITION_TYPES)] = {
      // 8x8 -> 4x4 only supports the four legacy partition types
      { AOM_ICDF(25472), AOM_ICDF(28949), AOM_ICDF(31052), AOM_ICDF(32768), 0,
        0, 0, 0, 0, 0, 0 },
      { AOM_ICDF(18816), AOM_ICDF(22250), AOM_ICDF(28783), AOM_ICDF(32768), 0,
        0, 0, 0, 0, 0, 0 },
      { AOM_ICDF(18944), AOM_ICDF(26126), AOM_ICDF(29188), AOM_ICDF(32768), 0,
        0, 0, 0, 0, 0, 0 },
      { AOM_ICDF(15488), AOM_ICDF(22508), AOM_ICDF(27077), AOM_ICDF(32768), 0,
        0, 0, 0, 0, 0, 0 },
      // 16x16 -> 8x8
      { AOM_ICDF(22272), AOM_ICDF(23768), AOM_ICDF(25043), AOM_ICDF(29996),
        AOM_ICDF(30744), AOM_ICDF(31493), AOM_ICDF(32130), AOM_ICDF(32768), 0,
        0, 0 },
      { AOM_ICDF(11776), AOM_ICDF(13457), AOM_ICDF(16315), AOM_ICDF(28229),
        AOM_ICDF(29069), AOM_ICDF(29910), AOM_ICDF(31339), AOM_ICDF(32768), 0,
        0, 0 },
      { AOM_ICDF(10496), AOM_ICDF(14802), AOM_ICDF(16136), AOM_ICDF(27127),
        AOM_ICDF(29280), AOM_ICDF(31434), AOM_ICDF(32101), AOM_ICDF(32768), 0,
        0, 0 },
      { AOM_ICDF(6784), AOM_ICDF(8763), AOM_ICDF(10440), AOM_ICDF(29110),
        AOM_ICDF(30100), AOM_ICDF(31090), AOM_ICDF(31929), AOM_ICDF(32768), 0,
        0, 0 },
      // 32x32 -> 16x16
      { AOM_ICDF(22656), AOM_ICDF(23801), AOM_ICDF(24702), AOM_ICDF(30721),
        AOM_ICDF(31103), AOM_ICDF(31485), AOM_ICDF(31785), AOM_ICDF(32085),
        AOM_ICDF(32467), AOM_ICDF(32768), 0 },
      { AOM_ICDF(8704), AOM_ICDF(9926), AOM_ICDF(12586), AOM_ICDF(28885),
        AOM_ICDF(29292), AOM_ICDF(29699), AOM_ICDF(30586), AOM_ICDF(31473),
        AOM_ICDF(31881), AOM_ICDF(32768), 0 },
      { AOM_ICDF(6656), AOM_ICDF(10685), AOM_ICDF(11566), AOM_ICDF(27857),
        AOM_ICDF(29200), AOM_ICDF(30543), AOM_ICDF(30837), AOM_ICDF(31131),
        AOM_ICDF(32474), AOM_ICDF(32768), 0 },
      { AOM_ICDF(2176), AOM_ICDF(3012), AOM_ICDF(3690), AOM_ICDF(31253),
        AOM_ICDF(31532), AOM_ICDF(31811), AOM_ICDF(32037), AOM_ICDF(32263),
        AOM_ICDF(32542), AOM_ICDF(32768), 0 },
      // 64x64 -> 32x32
      { AOM_ICDF(28416), AOM_ICDF(28705), AOM_ICDF(28926), AOM_ICDF(32258),
        AOM_ICDF(32402), AOM_ICDF(32547), AOM_ICDF(32657), AOM_ICDF(32768), 0,
        0, 0 },
      { AOM_ICDF(9216), AOM_ICDF(9952), AOM_ICDF(11849), AOM_ICDF(30134),
        AOM_ICDF(30502), AOM_ICDF(30870), AOM_ICDF(31819), AOM_ICDF(32768), 0,
        0, 0 },
      { AOM_ICDF(7424), AOM_ICDF(9008), AOM_ICDF(9528), AOM_ICDF(30664),
        AOM_ICDF(31456), AOM_ICDF(32248), AOM_ICDF(32508), AOM_ICDF(32768), 0,
        0, 0 },
      { AOM_ICDF(1280), AOM_ICDF(1710), AOM_ICDF(2069), AOM_ICDF(31978),
        AOM_ICDF(32193), AOM_ICDF(32409), AOM_ICDF(32588), AOM_ICDF(32768), 0,
        0, 0 },
#if CONFIG_EXT_PARTITION
      // 128x128 -> 64x64
      { AOM_ICDF(28416), AOM_ICDF(28705), AOM_ICDF(28926), AOM_ICDF(32258),
        AOM_ICDF(32402), AOM_ICDF(32547), AOM_ICDF(32548), AOM_ICDF(32768), 0,
        0, 0 },
      { AOM_ICDF(9216), AOM_ICDF(9952), AOM_ICDF(11849), AOM_ICDF(30134),
        AOM_ICDF(30502), AOM_ICDF(30870), AOM_ICDF(30871), AOM_ICDF(32768), 0,
        0, 0 },
      { AOM_ICDF(7424), AOM_ICDF(9008), AOM_ICDF(9528), AOM_ICDF(30664),
        AOM_ICDF(31456), AOM_ICDF(32248), AOM_ICDF(32249), AOM_ICDF(32768), 0,
        0, 0 },
      { AOM_ICDF(1280), AOM_ICDF(1710), AOM_ICDF(2069), AOM_ICDF(31978),
        AOM_ICDF(32193), AOM_ICDF(32409), AOM_ICDF(32410), AOM_ICDF(32768), 0,
        0, 0 },
#endif
    };
#else
static const aom_cdf_prob
    default_partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(PARTITION_TYPES)] = {
      { AOM_ICDF(25472), AOM_ICDF(28949), AOM_ICDF(31052), AOM_ICDF(32768), 0 },
      { AOM_ICDF(18816), AOM_ICDF(22250), AOM_ICDF(28783), AOM_ICDF(32768), 0 },
      { AOM_ICDF(18944), AOM_ICDF(26126), AOM_ICDF(29188), AOM_ICDF(32768), 0 },
      { AOM_ICDF(15488), AOM_ICDF(22508), AOM_ICDF(27077), AOM_ICDF(32768), 0 },
      { AOM_ICDF(22272), AOM_ICDF(25265), AOM_ICDF(27815), AOM_ICDF(32768), 0 },
      { AOM_ICDF(11776), AOM_ICDF(15138), AOM_ICDF(20854), AOM_ICDF(32768), 0 },
      { AOM_ICDF(10496), AOM_ICDF(19109), AOM_ICDF(21777), AOM_ICDF(32768), 0 },
      { AOM_ICDF(6784), AOM_ICDF(10743), AOM_ICDF(14098), AOM_ICDF(32768), 0 },
      { AOM_ICDF(22656), AOM_ICDF(24947), AOM_ICDF(26749), AOM_ICDF(32768), 0 },
      { AOM_ICDF(8704), AOM_ICDF(11148), AOM_ICDF(16469), AOM_ICDF(32768), 0 },
      { AOM_ICDF(6656), AOM_ICDF(14714), AOM_ICDF(16477), AOM_ICDF(32768), 0 },
      { AOM_ICDF(2176), AOM_ICDF(3849), AOM_ICDF(5205), AOM_ICDF(32768), 0 },
      { AOM_ICDF(28416), AOM_ICDF(28994), AOM_ICDF(29436), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9216), AOM_ICDF(10688), AOM_ICDF(14483), AOM_ICDF(32768), 0 },
      { AOM_ICDF(7424), AOM_ICDF(10592), AOM_ICDF(11632), AOM_ICDF(32768), 0 },
      { AOM_ICDF(1280), AOM_ICDF(2141), AOM_ICDF(2859), AOM_ICDF(32768), 0 },
#if CONFIG_EXT_PARTITION
      { AOM_ICDF(28416), AOM_ICDF(28994), AOM_ICDF(29436), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9216), AOM_ICDF(10688), AOM_ICDF(14483), AOM_ICDF(32768), 0 },
      { AOM_ICDF(7424), AOM_ICDF(10592), AOM_ICDF(11632), AOM_ICDF(32768), 0 },
      { AOM_ICDF(1280), AOM_ICDF(2141), AOM_ICDF(2859), AOM_ICDF(32768), 0 },
#endif
    };
#endif

#if CONFIG_EXT_TX
static const aom_cdf_prob default_intra_ext_tx_cdf
    [EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES][CDF_SIZE(TX_TYPES)] = {
      {
// FIXME: unused zero positions, from uncoded trivial transform set
#if CONFIG_CHROMA_2X2
          {
              { 0 },
          },
#endif
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
#if CONFIG_ALT_INTRA
              { 0 },
#if CONFIG_SMOOTH_HV
              { 0 },
              { 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { 0 },
          },
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
#if CONFIG_ALT_INTRA
              { 0 },
#if CONFIG_SMOOTH_HV
              { 0 },
              { 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { 0 },
          },
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
#if CONFIG_ALT_INTRA
              { 0 },
#if CONFIG_SMOOTH_HV
              { 0 },
              { 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { 0 },
          },
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
#if CONFIG_ALT_INTRA
              { 0 },
#if CONFIG_SMOOTH_HV
              { 0 },
              { 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { 0 },
          },
      },
      {
          {
              { AOM_ICDF(1024), AOM_ICDF(28800), AOM_ICDF(29048),
                AOM_ICDF(29296), AOM_ICDF(30164), AOM_ICDF(31466),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(27118), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(25852), AOM_ICDF(26284),
                AOM_ICDF(26717), AOM_ICDF(28230), AOM_ICDF(30499),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(2016), AOM_ICDF(3938), AOM_ICDF(5860),
                AOM_ICDF(29404), AOM_ICDF(31086), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(27118), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(5900), AOM_ICDF(7691),
                AOM_ICDF(15528), AOM_ICDF(27380), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(5900), AOM_ICDF(7691),
                AOM_ICDF(15528), AOM_ICDF(27380), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
          },
          {
              { AOM_ICDF(1024), AOM_ICDF(28800), AOM_ICDF(29048),
                AOM_ICDF(29296), AOM_ICDF(30164), AOM_ICDF(31466),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(27118), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(25852), AOM_ICDF(26284),
                AOM_ICDF(26717), AOM_ICDF(28230), AOM_ICDF(30499),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(2016), AOM_ICDF(3938), AOM_ICDF(5860),
                AOM_ICDF(29404), AOM_ICDF(31086), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(27118), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(5900), AOM_ICDF(7691),
                AOM_ICDF(15528), AOM_ICDF(27380), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(5900), AOM_ICDF(7691),
                AOM_ICDF(15528), AOM_ICDF(27380), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
          },
          {
              { AOM_ICDF(1024), AOM_ICDF(28800), AOM_ICDF(29048),
                AOM_ICDF(29296), AOM_ICDF(30164), AOM_ICDF(31466),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(27118), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(25852), AOM_ICDF(26284),
                AOM_ICDF(26717), AOM_ICDF(28230), AOM_ICDF(30499),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(2016), AOM_ICDF(3938), AOM_ICDF(5860),
                AOM_ICDF(29404), AOM_ICDF(31086), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(27118), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(5900), AOM_ICDF(7691),
                AOM_ICDF(15528), AOM_ICDF(27380), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(5900), AOM_ICDF(7691),
                AOM_ICDF(15528), AOM_ICDF(27380), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
          },
          {
              { AOM_ICDF(1024), AOM_ICDF(28800), AOM_ICDF(29048),
                AOM_ICDF(29296), AOM_ICDF(30164), AOM_ICDF(31466),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(27118), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(25852), AOM_ICDF(26284),
                AOM_ICDF(26717), AOM_ICDF(28230), AOM_ICDF(30499),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(2016), AOM_ICDF(3938), AOM_ICDF(5860),
                AOM_ICDF(29404), AOM_ICDF(31086), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(27118), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(5900), AOM_ICDF(7691),
                AOM_ICDF(15528), AOM_ICDF(27380), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(5900), AOM_ICDF(7691),
                AOM_ICDF(15528), AOM_ICDF(27380), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(8660),
                AOM_ICDF(10167), AOM_ICDF(15817), AOM_ICDF(32768), 0 },
          },
      },
      {
          {
              { AOM_ICDF(1024), AOM_ICDF(28800), AOM_ICDF(29792),
                AOM_ICDF(31280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(26310),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(25852), AOM_ICDF(27581),
                AOM_ICDF(30174), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(2016), AOM_ICDF(28924),
                AOM_ICDF(30846), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(26310),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(13065),
                AOM_ICDF(26611), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(13065),
                AOM_ICDF(26611), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
          },
          {
              { AOM_ICDF(1024), AOM_ICDF(28800), AOM_ICDF(29792),
                AOM_ICDF(31280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(26310),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(25852), AOM_ICDF(27581),
                AOM_ICDF(30174), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(2016), AOM_ICDF(28924),
                AOM_ICDF(30846), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(26310),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(13065),
                AOM_ICDF(26611), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(13065),
                AOM_ICDF(26611), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
          },
          {
              { AOM_ICDF(1024), AOM_ICDF(28800), AOM_ICDF(29792),
                AOM_ICDF(31280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(26310),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(25852), AOM_ICDF(27581),
                AOM_ICDF(30174), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(2016), AOM_ICDF(28924),
                AOM_ICDF(30846), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(26310),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(13065),
                AOM_ICDF(26611), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(13065),
                AOM_ICDF(26611), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
          },
          {
              { AOM_ICDF(1024), AOM_ICDF(28800), AOM_ICDF(29792),
                AOM_ICDF(31280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(26310),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(25852), AOM_ICDF(27581),
                AOM_ICDF(30174), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(2016), AOM_ICDF(28924),
                AOM_ICDF(30846), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(26310),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(13065),
                AOM_ICDF(26611), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(4109), AOM_ICDF(13065),
                AOM_ICDF(26611), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(5216), AOM_ICDF(6938), AOM_ICDF(13396),
                AOM_ICDF(32768), 0 },
          },
      },
#if CONFIG_MRC_TX
      {
          {
              { AOM_ICDF(1024), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
          },
          {
              { AOM_ICDF(1024), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
          },
          {
              { AOM_ICDF(1024), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
          },
          {
              { AOM_ICDF(1024), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1152), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1024), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#if CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#if CONFIG_SMOOTH_HV
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
#endif  // CONFIG_SMOOTH_HV
#endif  // CONFIG_ALT_INTRA
              { AOM_ICDF(1280), AOM_ICDF(32768), 0 },
          },
      }
#endif  // CONFIG_MRC_TX
    };
static const aom_cdf_prob
    default_inter_ext_tx_cdf[EXT_TX_SETS_INTER][EXT_TX_SIZES][CDF_SIZE(
        TX_TYPES)] = {
      {
#if CONFIG_CHROMA_2X2
          { 0 },
#endif
          { 0 },
          { 0 },
          { 0 },
          { 0 } },
      {
#if CONFIG_CHROMA_2X2
          { 0 },
#endif
          { AOM_ICDF(1280), AOM_ICDF(1453), AOM_ICDF(1626), AOM_ICDF(2277),
            AOM_ICDF(2929), AOM_ICDF(3580), AOM_ICDF(4232), AOM_ICDF(16717),
            AOM_ICDF(19225), AOM_ICDF(21733), AOM_ICDF(24241), AOM_ICDF(26749),
            AOM_ICDF(28253), AOM_ICDF(29758), AOM_ICDF(31263), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(1280), AOM_ICDF(1453), AOM_ICDF(1626), AOM_ICDF(2277),
            AOM_ICDF(2929), AOM_ICDF(3580), AOM_ICDF(4232), AOM_ICDF(16717),
            AOM_ICDF(19225), AOM_ICDF(21733), AOM_ICDF(24241), AOM_ICDF(26749),
            AOM_ICDF(28253), AOM_ICDF(29758), AOM_ICDF(31263), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(1280), AOM_ICDF(1453), AOM_ICDF(1626), AOM_ICDF(2277),
            AOM_ICDF(2929), AOM_ICDF(3580), AOM_ICDF(4232), AOM_ICDF(16717),
            AOM_ICDF(19225), AOM_ICDF(21733), AOM_ICDF(24241), AOM_ICDF(26749),
            AOM_ICDF(28253), AOM_ICDF(29758), AOM_ICDF(31263), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(1280), AOM_ICDF(1453), AOM_ICDF(1626), AOM_ICDF(2277),
            AOM_ICDF(2929), AOM_ICDF(3580), AOM_ICDF(4232), AOM_ICDF(16717),
            AOM_ICDF(19225), AOM_ICDF(21733), AOM_ICDF(24241), AOM_ICDF(26749),
            AOM_ICDF(28253), AOM_ICDF(29758), AOM_ICDF(31263), AOM_ICDF(32768),
            0 } },
      {
#if CONFIG_CHROMA_2X2
          { 0 },
#endif
          { AOM_ICDF(1280), AOM_ICDF(3125), AOM_ICDF(4970), AOM_ICDF(17132),
            AOM_ICDF(19575), AOM_ICDF(22018), AOM_ICDF(24461), AOM_ICDF(26904),
            AOM_ICDF(28370), AOM_ICDF(29836), AOM_ICDF(31302), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(1280), AOM_ICDF(3125), AOM_ICDF(4970), AOM_ICDF(17132),
            AOM_ICDF(19575), AOM_ICDF(22018), AOM_ICDF(24461), AOM_ICDF(26904),
            AOM_ICDF(28370), AOM_ICDF(29836), AOM_ICDF(31302), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(1280), AOM_ICDF(3125), AOM_ICDF(4970), AOM_ICDF(17132),
            AOM_ICDF(19575), AOM_ICDF(22018), AOM_ICDF(24461), AOM_ICDF(26904),
            AOM_ICDF(28370), AOM_ICDF(29836), AOM_ICDF(31302), AOM_ICDF(32768),
            0 },
          { AOM_ICDF(1280), AOM_ICDF(3125), AOM_ICDF(4970), AOM_ICDF(17132),
            AOM_ICDF(19575), AOM_ICDF(22018), AOM_ICDF(24461), AOM_ICDF(26904),
            AOM_ICDF(28370), AOM_ICDF(29836), AOM_ICDF(31302), AOM_ICDF(32768),
            0 } },
      {
#if CONFIG_CHROMA_2X2
          { 0 },
#endif
          { AOM_ICDF(1536), AOM_ICDF(32768), 0 },
          { AOM_ICDF(1536), AOM_ICDF(32768), 0 },
          { AOM_ICDF(1536), AOM_ICDF(32768), 0 },
          { AOM_ICDF(1536), AOM_ICDF(32768), 0 } },
#if CONFIG_MRC_TX
      {
#if CONFIG_CHROMA_2X2
          { 0 },
#endif
          { AOM_ICDF(30080), AOM_ICDF(31781), AOM_ICDF(32768), 0 },
          { AOM_ICDF(4608), AOM_ICDF(32658), AOM_ICDF(32768), 0 },
          { AOM_ICDF(4352), AOM_ICDF(4685), AOM_ICDF(32768), 0 },
          { AOM_ICDF(19072), AOM_ICDF(26776), AOM_ICDF(32768), 0 } },
#endif  // CONFIG_MRC_TX
    };
#else
#if CONFIG_MRC_TX
static const aom_cdf_prob
    default_intra_ext_tx_cdf[EXT_TX_SIZES][TX_TYPES][CDF_SIZE(TX_TYPES)] = {
#if CONFIG_CHROMA_2X2
      { { AOM_ICDF(30720), AOM_ICDF(31104), AOM_ICDF(31400), AOM_ICDF(32084),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(582), AOM_ICDF(638), AOM_ICDF(31764),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(582), AOM_ICDF(638), AOM_ICDF(1642),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(582), AOM_ICDF(638), AOM_ICDF(1642),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(1280), AOM_ICDF(31760), AOM_ICDF(32264),
          AOM_ICDF(32768), 0 } },
#endif
      { { AOM_ICDF(30720), AOM_ICDF(31104), AOM_ICDF(31400), AOM_ICDF(32084),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(582), AOM_ICDF(638), AOM_ICDF(31764),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(582), AOM_ICDF(638), AOM_ICDF(1642),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(582), AOM_ICDF(638), AOM_ICDF(1642),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(1280), AOM_ICDF(31760), AOM_ICDF(32264),
          AOM_ICDF(32768), 0 } },

      { { AOM_ICDF(31232), AOM_ICDF(31488), AOM_ICDF(31742), AOM_ICDF(32255),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(1024), AOM_ICDF(1152), AOM_ICDF(1272), AOM_ICDF(31784),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(1024), AOM_ICDF(1152), AOM_ICDF(1272), AOM_ICDF(2256),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(1024), AOM_ICDF(1052), AOM_ICDF(1272), AOM_ICDF(2256),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(1024), AOM_ICDF(1792), AOM_ICDF(31776), AOM_ICDF(32272),
          AOM_ICDF(32768), 0 } },

      { { AOM_ICDF(31744), AOM_ICDF(29440), AOM_ICDF(32084), AOM_ICDF(32426),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(2048), AOM_ICDF(2176), AOM_ICDF(2528), AOM_ICDF(31823),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(2048), AOM_ICDF(2176), AOM_ICDF(2528), AOM_ICDF(3473),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(2048), AOM_ICDF(2176), AOM_ICDF(2528), AOM_ICDF(3473),
          AOM_ICDF(32768), 0 },
        { AOM_ICDF(2048), AOM_ICDF(28160), AOM_ICDF(31808), AOM_ICDF(32288),
          AOM_ICDF(32768), 0 } },
    };

static const aom_cdf_prob
    default_inter_ext_tx_cdf[EXT_TX_SIZES][CDF_SIZE(TX_TYPES)] = {
#if CONFIG_CHROMA_2X2
      { AOM_ICDF(20480), AOM_ICDF(23040), AOM_ICDF(24560), AOM_ICDF(28664),
        AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(20480), AOM_ICDF(23040), AOM_ICDF(24560), AOM_ICDF(28664),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(22528), AOM_ICDF(24320), AOM_ICDF(25928), AOM_ICDF(29348),
        AOM_ICDF(32768), 0 },
      { AOM_ICDF(24576), AOM_ICDF(25600), AOM_ICDF(27296), AOM_ICDF(30032),
        AOM_ICDF(32768), 0 },
    };
#else  // CONFIG_MRC_TX
static const aom_cdf_prob
    default_intra_ext_tx_cdf[EXT_TX_SIZES][TX_TYPES][CDF_SIZE(TX_TYPES)] = {
#if CONFIG_CHROMA_2X2
      { { AOM_ICDF(30720), AOM_ICDF(31400), AOM_ICDF(32084), AOM_ICDF(32768),
          0 },
        { AOM_ICDF(512), AOM_ICDF(638), AOM_ICDF(31764), AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(638), AOM_ICDF(1642), AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(31760), AOM_ICDF(32264), AOM_ICDF(32768),
          0 } },
#endif
      { { AOM_ICDF(30720), AOM_ICDF(31400), AOM_ICDF(32084), AOM_ICDF(32768),
          0 },
        { AOM_ICDF(512), AOM_ICDF(638), AOM_ICDF(31764), AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(638), AOM_ICDF(1642), AOM_ICDF(32768), 0 },
        { AOM_ICDF(512), AOM_ICDF(31760), AOM_ICDF(32264), AOM_ICDF(32768),
          0 } },

      { { AOM_ICDF(31232), AOM_ICDF(31742), AOM_ICDF(32255), AOM_ICDF(32768),
          0 },
        { AOM_ICDF(1024), AOM_ICDF(1272), AOM_ICDF(31784), AOM_ICDF(32768), 0 },
        { AOM_ICDF(1024), AOM_ICDF(1272), AOM_ICDF(2256), AOM_ICDF(32768), 0 },
        { AOM_ICDF(1024), AOM_ICDF(31776), AOM_ICDF(32272), AOM_ICDF(32768),
          0 } },
      { { AOM_ICDF(31744), AOM_ICDF(32084), AOM_ICDF(32426), AOM_ICDF(32768),
          0 },
        { AOM_ICDF(2048), AOM_ICDF(2528), AOM_ICDF(31823), AOM_ICDF(32768), 0 },
        { AOM_ICDF(2048), AOM_ICDF(2528), AOM_ICDF(3473), AOM_ICDF(32768), 0 },
        { AOM_ICDF(2048), AOM_ICDF(31808), AOM_ICDF(32288), AOM_ICDF(32768),
          0 } },
    };

static const aom_cdf_prob
    default_inter_ext_tx_cdf[EXT_TX_SIZES][CDF_SIZE(TX_TYPES)] = {
#if CONFIG_CHROMA_2X2
      { AOM_ICDF(20480), AOM_ICDF(24560), AOM_ICDF(28664), AOM_ICDF(32768), 0 },
#endif
      { AOM_ICDF(20480), AOM_ICDF(24560), AOM_ICDF(28664), AOM_ICDF(32768), 0 },
      { AOM_ICDF(22528), AOM_ICDF(25928), AOM_ICDF(29348), AOM_ICDF(32768), 0 },
      { AOM_ICDF(24576), AOM_ICDF(27296), AOM_ICDF(30032), AOM_ICDF(32768), 0 },
    };
#endif  // CONFIG_MRC_TX
#endif  // !CONFIG_EXT_TX

#if CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP
static const aom_cdf_prob
    default_intra_filter_cdf[INTRA_FILTERS + 1][CDF_SIZE(INTRA_FILTERS)] = {
      { AOM_ICDF(12544), AOM_ICDF(17521), AOM_ICDF(21095), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12544), AOM_ICDF(19022), AOM_ICDF(23318), AOM_ICDF(32768), 0 },
      { AOM_ICDF(12032), AOM_ICDF(17297), AOM_ICDF(23522), AOM_ICDF(32768), 0 },
      { AOM_ICDF(6272), AOM_ICDF(8860), AOM_ICDF(11101), AOM_ICDF(32768), 0 },
      { AOM_ICDF(9216), AOM_ICDF(12712), AOM_ICDF(16629), AOM_ICDF(32768), 0 },
    };
#endif  // CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP

#if CONFIG_CFL
static const aom_cdf_prob default_cfl_alpha_cdf[CDF_SIZE(CFL_ALPHABET_SIZE)] = {
  AOM_ICDF(20492), AOM_ICDF(24094), AOM_ICDF(25679), AOM_ICDF(27242),
  AOM_ICDF(28286), AOM_ICDF(29153), AOM_ICDF(29807), AOM_ICDF(30352),
  AOM_ICDF(30866), AOM_ICDF(31295), AOM_ICDF(31703), AOM_ICDF(32046),
  AOM_ICDF(32317), AOM_ICDF(32534), AOM_ICDF(32663), AOM_ICDF(32768)
};
#endif

// CDF version of 'av1_kf_y_mode_prob'.
const aom_cdf_prob
    av1_kf_y_mode_cdf[INTRA_MODES][INTRA_MODES][CDF_SIZE(INTRA_MODES)] = {
#if CONFIG_ALT_INTRA
#if CONFIG_SMOOTH_HV
      {
          { AOM_ICDF(14208), AOM_ICDF(16238), AOM_ICDF(19079), AOM_ICDF(22512),
            AOM_ICDF(23632), AOM_ICDF(24373), AOM_ICDF(25291), AOM_ICDF(26109),
            AOM_ICDF(26811), AOM_ICDF(27858), AOM_ICDF(30428), AOM_ICDF(31424),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(10496), AOM_ICDF(13193), AOM_ICDF(20992), AOM_ICDF(22569),
            AOM_ICDF(23557), AOM_ICDF(24442), AOM_ICDF(25515), AOM_ICDF(26478),
            AOM_ICDF(26994), AOM_ICDF(27693), AOM_ICDF(30349), AOM_ICDF(31757),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5120), AOM_ICDF(8252), AOM_ICDF(9593), AOM_ICDF(22972),
            AOM_ICDF(23813), AOM_ICDF(24168), AOM_ICDF(24638), AOM_ICDF(25019),
            AOM_ICDF(26048), AOM_ICDF(27413), AOM_ICDF(30090), AOM_ICDF(30812),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(12544), AOM_ICDF(14045), AOM_ICDF(16678), AOM_ICDF(19167),
            AOM_ICDF(20459), AOM_ICDF(21329), AOM_ICDF(23518), AOM_ICDF(24783),
            AOM_ICDF(25563), AOM_ICDF(27280), AOM_ICDF(30217), AOM_ICDF(31273),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7552), AOM_ICDF(8636), AOM_ICDF(10993), AOM_ICDF(12992),
            AOM_ICDF(18616), AOM_ICDF(21880), AOM_ICDF(23113), AOM_ICDF(23867),
            AOM_ICDF(25710), AOM_ICDF(26758), AOM_ICDF(30115), AOM_ICDF(31328),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(11008), AOM_ICDF(12708), AOM_ICDF(16704), AOM_ICDF(18234),
            AOM_ICDF(21591), AOM_ICDF(26744), AOM_ICDF(28368), AOM_ICDF(30104),
            AOM_ICDF(31270), AOM_ICDF(32171), AOM_ICDF(32539), AOM_ICDF(32669),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6272), AOM_ICDF(7307), AOM_ICDF(8998), AOM_ICDF(12979),
            AOM_ICDF(18143), AOM_ICDF(19397), AOM_ICDF(20233), AOM_ICDF(20772),
            AOM_ICDF(25645), AOM_ICDF(26869), AOM_ICDF(30049), AOM_ICDF(30984),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(8192), AOM_ICDF(9536), AOM_ICDF(11533), AOM_ICDF(15940),
            AOM_ICDF(17403), AOM_ICDF(18169), AOM_ICDF(19253), AOM_ICDF(20045),
            AOM_ICDF(21337), AOM_ICDF(25847), AOM_ICDF(29551), AOM_ICDF(30682),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(10752), AOM_ICDF(12558), AOM_ICDF(15005), AOM_ICDF(16854),
            AOM_ICDF(18148), AOM_ICDF(19307), AOM_ICDF(21410), AOM_ICDF(23939),
            AOM_ICDF(24698), AOM_ICDF(26117), AOM_ICDF(29832), AOM_ICDF(31323),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7424), AOM_ICDF(9008), AOM_ICDF(11885), AOM_ICDF(14829),
            AOM_ICDF(16543), AOM_ICDF(16779), AOM_ICDF(17841), AOM_ICDF(19182),
            AOM_ICDF(20190), AOM_ICDF(21664), AOM_ICDF(27650), AOM_ICDF(29909),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6528), AOM_ICDF(8476), AOM_ICDF(12841), AOM_ICDF(15721),
            AOM_ICDF(17437), AOM_ICDF(17823), AOM_ICDF(18874), AOM_ICDF(20394),
            AOM_ICDF(21216), AOM_ICDF(22344), AOM_ICDF(27922), AOM_ICDF(30743),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(8704), AOM_ICDF(10114), AOM_ICDF(12415), AOM_ICDF(15730),
            AOM_ICDF(17127), AOM_ICDF(17265), AOM_ICDF(18294), AOM_ICDF(19255),
            AOM_ICDF(20258), AOM_ICDF(21675), AOM_ICDF(27525), AOM_ICDF(29082),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6272), AOM_ICDF(12586), AOM_ICDF(15818), AOM_ICDF(21751),
            AOM_ICDF(22707), AOM_ICDF(23300), AOM_ICDF(24262), AOM_ICDF(25126),
            AOM_ICDF(25992), AOM_ICDF(27448), AOM_ICDF(30004), AOM_ICDF(31073),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(3968), AOM_ICDF(6893), AOM_ICDF(20538), AOM_ICDF(22050),
            AOM_ICDF(22805), AOM_ICDF(24408), AOM_ICDF(24833), AOM_ICDF(26073),
            AOM_ICDF(26439), AOM_ICDF(26884), AOM_ICDF(29895), AOM_ICDF(31938),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3072), AOM_ICDF(6204), AOM_ICDF(24363), AOM_ICDF(24995),
            AOM_ICDF(25363), AOM_ICDF(26103), AOM_ICDF(26546), AOM_ICDF(27518),
            AOM_ICDF(27621), AOM_ICDF(27902), AOM_ICDF(30164), AOM_ICDF(32148),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(2560), AOM_ICDF(6572), AOM_ICDF(13837), AOM_ICDF(19693),
            AOM_ICDF(20377), AOM_ICDF(21010), AOM_ICDF(21699), AOM_ICDF(22737),
            AOM_ICDF(23286), AOM_ICDF(24323), AOM_ICDF(28875), AOM_ICDF(30837),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6912), AOM_ICDF(8831), AOM_ICDF(17059), AOM_ICDF(18404),
            AOM_ICDF(19221), AOM_ICDF(20434), AOM_ICDF(22313), AOM_ICDF(24151),
            AOM_ICDF(24420), AOM_ICDF(25855), AOM_ICDF(29474), AOM_ICDF(31623),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(2944), AOM_ICDF(4808), AOM_ICDF(14965), AOM_ICDF(15870),
            AOM_ICDF(18714), AOM_ICDF(21989), AOM_ICDF(22957), AOM_ICDF(24528),
            AOM_ICDF(25365), AOM_ICDF(26001), AOM_ICDF(29596), AOM_ICDF(31678),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4352), AOM_ICDF(6239), AOM_ICDF(19711), AOM_ICDF(20602),
            AOM_ICDF(22489), AOM_ICDF(27311), AOM_ICDF(28228), AOM_ICDF(30516),
            AOM_ICDF(31097), AOM_ICDF(31750), AOM_ICDF(32319), AOM_ICDF(32656),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(2944), AOM_ICDF(4925), AOM_ICDF(13952), AOM_ICDF(15490),
            AOM_ICDF(18397), AOM_ICDF(20200), AOM_ICDF(20986), AOM_ICDF(22367),
            AOM_ICDF(24967), AOM_ICDF(25820), AOM_ICDF(29755), AOM_ICDF(31473),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4224), AOM_ICDF(6120), AOM_ICDF(14968), AOM_ICDF(17184),
            AOM_ICDF(18063), AOM_ICDF(19140), AOM_ICDF(20258), AOM_ICDF(21822),
            AOM_ICDF(22463), AOM_ICDF(24838), AOM_ICDF(28989), AOM_ICDF(31277),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5120), AOM_ICDF(7280), AOM_ICDF(17535), AOM_ICDF(18348),
            AOM_ICDF(19116), AOM_ICDF(20689), AOM_ICDF(21916), AOM_ICDF(24968),
            AOM_ICDF(25242), AOM_ICDF(26095), AOM_ICDF(29588), AOM_ICDF(31787),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(2816), AOM_ICDF(4922), AOM_ICDF(17105), AOM_ICDF(18458),
            AOM_ICDF(19325), AOM_ICDF(19614), AOM_ICDF(20231), AOM_ICDF(21700),
            AOM_ICDF(22089), AOM_ICDF(22756), AOM_ICDF(27879), AOM_ICDF(31278),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(2560), AOM_ICDF(4920), AOM_ICDF(18518), AOM_ICDF(19680),
            AOM_ICDF(20386), AOM_ICDF(20689), AOM_ICDF(21208), AOM_ICDF(22472),
            AOM_ICDF(22754), AOM_ICDF(23223), AOM_ICDF(27809), AOM_ICDF(31664),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3328), AOM_ICDF(5513), AOM_ICDF(17331), AOM_ICDF(19065),
            AOM_ICDF(19882), AOM_ICDF(20105), AOM_ICDF(20748), AOM_ICDF(22110),
            AOM_ICDF(22443), AOM_ICDF(23129), AOM_ICDF(28099), AOM_ICDF(30944),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(2560), AOM_ICDF(6690), AOM_ICDF(20748), AOM_ICDF(22590),
            AOM_ICDF(23037), AOM_ICDF(23659), AOM_ICDF(24406), AOM_ICDF(25582),
            AOM_ICDF(25835), AOM_ICDF(26485), AOM_ICDF(29553), AOM_ICDF(31826),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(11392), AOM_ICDF(13647), AOM_ICDF(15216), AOM_ICDF(23156),
            AOM_ICDF(24102), AOM_ICDF(24540), AOM_ICDF(25183), AOM_ICDF(25746),
            AOM_ICDF(26706), AOM_ICDF(28032), AOM_ICDF(30511), AOM_ICDF(31357),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(8064), AOM_ICDF(11635), AOM_ICDF(17166), AOM_ICDF(22459),
            AOM_ICDF(23608), AOM_ICDF(24297), AOM_ICDF(25025), AOM_ICDF(25902),
            AOM_ICDF(26438), AOM_ICDF(27551), AOM_ICDF(30343), AOM_ICDF(31641),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4352), AOM_ICDF(6905), AOM_ICDF(7612), AOM_ICDF(24258),
            AOM_ICDF(24862), AOM_ICDF(25005), AOM_ICDF(25399), AOM_ICDF(25658),
            AOM_ICDF(26491), AOM_ICDF(28281), AOM_ICDF(30472), AOM_ICDF(31037),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(10752), AOM_ICDF(13246), AOM_ICDF(14771), AOM_ICDF(18965),
            AOM_ICDF(20132), AOM_ICDF(20606), AOM_ICDF(22411), AOM_ICDF(23422),
            AOM_ICDF(24663), AOM_ICDF(27386), AOM_ICDF(30203), AOM_ICDF(31265),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(8320), AOM_ICDF(10135), AOM_ICDF(11815), AOM_ICDF(15962),
            AOM_ICDF(19829), AOM_ICDF(21555), AOM_ICDF(22738), AOM_ICDF(23482),
            AOM_ICDF(25513), AOM_ICDF(27100), AOM_ICDF(30222), AOM_ICDF(31246),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(11264), AOM_ICDF(13364), AOM_ICDF(16851), AOM_ICDF(20617),
            AOM_ICDF(23504), AOM_ICDF(26302), AOM_ICDF(28070), AOM_ICDF(29189),
            AOM_ICDF(30531), AOM_ICDF(31903), AOM_ICDF(32342), AOM_ICDF(32512),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6528), AOM_ICDF(7656), AOM_ICDF(8637), AOM_ICDF(15318),
            AOM_ICDF(18270), AOM_ICDF(18817), AOM_ICDF(19580), AOM_ICDF(20044),
            AOM_ICDF(24666), AOM_ICDF(26502), AOM_ICDF(29733), AOM_ICDF(30670),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6784), AOM_ICDF(8307), AOM_ICDF(9167), AOM_ICDF(17476),
            AOM_ICDF(18366), AOM_ICDF(18663), AOM_ICDF(19765), AOM_ICDF(20425),
            AOM_ICDF(21534), AOM_ICDF(26888), AOM_ICDF(29989), AOM_ICDF(30857),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(8192), AOM_ICDF(11072), AOM_ICDF(12682), AOM_ICDF(17399),
            AOM_ICDF(19010), AOM_ICDF(19743), AOM_ICDF(20964), AOM_ICDF(22993),
            AOM_ICDF(23871), AOM_ICDF(25817), AOM_ICDF(29727), AOM_ICDF(31164),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5888), AOM_ICDF(7988), AOM_ICDF(9634), AOM_ICDF(16735),
            AOM_ICDF(18009), AOM_ICDF(18129), AOM_ICDF(18930), AOM_ICDF(19741),
            AOM_ICDF(20911), AOM_ICDF(22671), AOM_ICDF(27877), AOM_ICDF(29749),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5248), AOM_ICDF(8151), AOM_ICDF(10267), AOM_ICDF(17761),
            AOM_ICDF(19077), AOM_ICDF(19232), AOM_ICDF(19919), AOM_ICDF(20772),
            AOM_ICDF(21615), AOM_ICDF(23140), AOM_ICDF(28142), AOM_ICDF(30618),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6656), AOM_ICDF(8390), AOM_ICDF(9723), AOM_ICDF(17206),
            AOM_ICDF(18212), AOM_ICDF(18275), AOM_ICDF(19068), AOM_ICDF(19657),
            AOM_ICDF(20886), AOM_ICDF(22650), AOM_ICDF(27907), AOM_ICDF(29084),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4480), AOM_ICDF(9232), AOM_ICDF(11163), AOM_ICDF(22580),
            AOM_ICDF(23368), AOM_ICDF(23653), AOM_ICDF(24436), AOM_ICDF(24989),
            AOM_ICDF(25809), AOM_ICDF(27087), AOM_ICDF(30038), AOM_ICDF(31104),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(10240), AOM_ICDF(11472), AOM_ICDF(14051), AOM_ICDF(16777),
            AOM_ICDF(18308), AOM_ICDF(19461), AOM_ICDF(22164), AOM_ICDF(24235),
            AOM_ICDF(25202), AOM_ICDF(26680), AOM_ICDF(29962), AOM_ICDF(31168),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7552), AOM_ICDF(9128), AOM_ICDF(16885), AOM_ICDF(18221),
            AOM_ICDF(19114), AOM_ICDF(20111), AOM_ICDF(23226), AOM_ICDF(25462),
            AOM_ICDF(26033), AOM_ICDF(27085), AOM_ICDF(30259), AOM_ICDF(31729),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5248), AOM_ICDF(7291), AOM_ICDF(8883), AOM_ICDF(18172),
            AOM_ICDF(19301), AOM_ICDF(19892), AOM_ICDF(21703), AOM_ICDF(22870),
            AOM_ICDF(23798), AOM_ICDF(25970), AOM_ICDF(29581), AOM_ICDF(30440),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(11008), AOM_ICDF(11943), AOM_ICDF(12838), AOM_ICDF(14729),
            AOM_ICDF(15340), AOM_ICDF(15719), AOM_ICDF(23245), AOM_ICDF(25217),
            AOM_ICDF(25453), AOM_ICDF(28282), AOM_ICDF(30735), AOM_ICDF(31696),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6272), AOM_ICDF(7100), AOM_ICDF(9506), AOM_ICDF(11141),
            AOM_ICDF(14891), AOM_ICDF(18048), AOM_ICDF(20808), AOM_ICDF(22910),
            AOM_ICDF(24450), AOM_ICDF(26172), AOM_ICDF(29625), AOM_ICDF(31233),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7424), AOM_ICDF(8513), AOM_ICDF(11924), AOM_ICDF(13742),
            AOM_ICDF(16971), AOM_ICDF(22265), AOM_ICDF(25957), AOM_ICDF(29762),
            AOM_ICDF(30831), AOM_ICDF(32193), AOM_ICDF(32537), AOM_ICDF(32669),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4992), AOM_ICDF(5969), AOM_ICDF(7435), AOM_ICDF(10922),
            AOM_ICDF(15097), AOM_ICDF(16638), AOM_ICDF(18654), AOM_ICDF(20087),
            AOM_ICDF(23356), AOM_ICDF(25452), AOM_ICDF(29281), AOM_ICDF(30725),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(9600), AOM_ICDF(10324), AOM_ICDF(12603), AOM_ICDF(15427),
            AOM_ICDF(16950), AOM_ICDF(17959), AOM_ICDF(20909), AOM_ICDF(22299),
            AOM_ICDF(22994), AOM_ICDF(27308), AOM_ICDF(30379), AOM_ICDF(31154),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(9856), AOM_ICDF(11020), AOM_ICDF(12549), AOM_ICDF(14621),
            AOM_ICDF(15493), AOM_ICDF(16182), AOM_ICDF(21430), AOM_ICDF(25947),
            AOM_ICDF(26427), AOM_ICDF(27888), AOM_ICDF(30595), AOM_ICDF(31809),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6272), AOM_ICDF(7618), AOM_ICDF(10664), AOM_ICDF(12915),
            AOM_ICDF(14454), AOM_ICDF(14722), AOM_ICDF(17965), AOM_ICDF(20394),
            AOM_ICDF(21312), AOM_ICDF(23371), AOM_ICDF(28730), AOM_ICDF(30623),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5888), AOM_ICDF(7463), AOM_ICDF(10923), AOM_ICDF(12991),
            AOM_ICDF(14555), AOM_ICDF(14934), AOM_ICDF(18208), AOM_ICDF(21052),
            AOM_ICDF(21876), AOM_ICDF(23450), AOM_ICDF(28655), AOM_ICDF(31017),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6912), AOM_ICDF(8023), AOM_ICDF(10440), AOM_ICDF(13329),
            AOM_ICDF(14958), AOM_ICDF(15150), AOM_ICDF(18109), AOM_ICDF(20056),
            AOM_ICDF(21049), AOM_ICDF(23063), AOM_ICDF(28219), AOM_ICDF(29978),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5120), AOM_ICDF(7928), AOM_ICDF(11906), AOM_ICDF(15940),
            AOM_ICDF(16978), AOM_ICDF(17773), AOM_ICDF(22342), AOM_ICDF(24419),
            AOM_ICDF(25300), AOM_ICDF(27021), AOM_ICDF(30007), AOM_ICDF(31312),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(7296), AOM_ICDF(8291), AOM_ICDF(10299), AOM_ICDF(12767),
            AOM_ICDF(18252), AOM_ICDF(20656), AOM_ICDF(21413), AOM_ICDF(22300),
            AOM_ICDF(24958), AOM_ICDF(26544), AOM_ICDF(30069), AOM_ICDF(31387),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7168), AOM_ICDF(8668), AOM_ICDF(13187), AOM_ICDF(15041),
            AOM_ICDF(18824), AOM_ICDF(21371), AOM_ICDF(22261), AOM_ICDF(23574),
            AOM_ICDF(25082), AOM_ICDF(26133), AOM_ICDF(29839), AOM_ICDF(31693),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3584), AOM_ICDF(5750), AOM_ICDF(6594), AOM_ICDF(15662),
            AOM_ICDF(18845), AOM_ICDF(20090), AOM_ICDF(20783), AOM_ICDF(21438),
            AOM_ICDF(23430), AOM_ICDF(25436), AOM_ICDF(29446), AOM_ICDF(30471),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7424), AOM_ICDF(8711), AOM_ICDF(10121), AOM_ICDF(11786),
            AOM_ICDF(15100), AOM_ICDF(16579), AOM_ICDF(20437), AOM_ICDF(21593),
            AOM_ICDF(22903), AOM_ICDF(25678), AOM_ICDF(29638), AOM_ICDF(31130),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4480), AOM_ICDF(5033), AOM_ICDF(6441), AOM_ICDF(7646),
            AOM_ICDF(18034), AOM_ICDF(21867), AOM_ICDF(22676), AOM_ICDF(23504),
            AOM_ICDF(25892), AOM_ICDF(26913), AOM_ICDF(30206), AOM_ICDF(31507),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7296), AOM_ICDF(8192), AOM_ICDF(11072), AOM_ICDF(12676),
            AOM_ICDF(19460), AOM_ICDF(25734), AOM_ICDF(26778), AOM_ICDF(28439),
            AOM_ICDF(31077), AOM_ICDF(32002), AOM_ICDF(32469), AOM_ICDF(32671),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3840), AOM_ICDF(4518), AOM_ICDF(5511), AOM_ICDF(8229),
            AOM_ICDF(16448), AOM_ICDF(18394), AOM_ICDF(19292), AOM_ICDF(20345),
            AOM_ICDF(25683), AOM_ICDF(27399), AOM_ICDF(30566), AOM_ICDF(31375),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6528), AOM_ICDF(7451), AOM_ICDF(8934), AOM_ICDF(12006),
            AOM_ICDF(15767), AOM_ICDF(17127), AOM_ICDF(18471), AOM_ICDF(19476),
            AOM_ICDF(21553), AOM_ICDF(25715), AOM_ICDF(29572), AOM_ICDF(30795),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7168), AOM_ICDF(8368), AOM_ICDF(10370), AOM_ICDF(11855),
            AOM_ICDF(14966), AOM_ICDF(17544), AOM_ICDF(19328), AOM_ICDF(21271),
            AOM_ICDF(22708), AOM_ICDF(24555), AOM_ICDF(29207), AOM_ICDF(31280),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5504), AOM_ICDF(6782), AOM_ICDF(8914), AOM_ICDF(11452),
            AOM_ICDF(15958), AOM_ICDF(16648), AOM_ICDF(17530), AOM_ICDF(18899),
            AOM_ICDF(20578), AOM_ICDF(22245), AOM_ICDF(28123), AOM_ICDF(30427),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5248), AOM_ICDF(6538), AOM_ICDF(9100), AOM_ICDF(11294),
            AOM_ICDF(15638), AOM_ICDF(16589), AOM_ICDF(17600), AOM_ICDF(19318),
            AOM_ICDF(20842), AOM_ICDF(22193), AOM_ICDF(28018), AOM_ICDF(30875),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5376), AOM_ICDF(6553), AOM_ICDF(8499), AOM_ICDF(11769),
            AOM_ICDF(15661), AOM_ICDF(16178), AOM_ICDF(17280), AOM_ICDF(18490),
            AOM_ICDF(20386), AOM_ICDF(22127), AOM_ICDF(28071), AOM_ICDF(30089),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4608), AOM_ICDF(7468), AOM_ICDF(10136), AOM_ICDF(15013),
            AOM_ICDF(17716), AOM_ICDF(19595), AOM_ICDF(20830), AOM_ICDF(22136),
            AOM_ICDF(23714), AOM_ICDF(25341), AOM_ICDF(29403), AOM_ICDF(31072),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(6656), AOM_ICDF(8186), AOM_ICDF(13755), AOM_ICDF(15971),
            AOM_ICDF(20413), AOM_ICDF(27940), AOM_ICDF(28657), AOM_ICDF(29910),
            AOM_ICDF(31004), AOM_ICDF(31969), AOM_ICDF(32443), AOM_ICDF(32665),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6400), AOM_ICDF(8048), AOM_ICDF(16256), AOM_ICDF(17568),
            AOM_ICDF(21074), AOM_ICDF(28253), AOM_ICDF(28976), AOM_ICDF(30531),
            AOM_ICDF(31099), AOM_ICDF(31875), AOM_ICDF(32426), AOM_ICDF(32701),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3840), AOM_ICDF(6439), AOM_ICDF(9524), AOM_ICDF(17270),
            AOM_ICDF(21391), AOM_ICDF(25777), AOM_ICDF(26815), AOM_ICDF(27908),
            AOM_ICDF(29199), AOM_ICDF(31151), AOM_ICDF(32168), AOM_ICDF(32407),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(9600), AOM_ICDF(10958), AOM_ICDF(14962), AOM_ICDF(16560),
            AOM_ICDF(19908), AOM_ICDF(23309), AOM_ICDF(25637), AOM_ICDF(28033),
            AOM_ICDF(29032), AOM_ICDF(32009), AOM_ICDF(32528), AOM_ICDF(32701),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4608), AOM_ICDF(5598), AOM_ICDF(9525), AOM_ICDF(10578),
            AOM_ICDF(18511), AOM_ICDF(27502), AOM_ICDF(28654), AOM_ICDF(29907),
            AOM_ICDF(31069), AOM_ICDF(32071), AOM_ICDF(32493), AOM_ICDF(32670),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4864), AOM_ICDF(5845), AOM_ICDF(11524), AOM_ICDF(12294),
            AOM_ICDF(16882), AOM_ICDF(27955), AOM_ICDF(28839), AOM_ICDF(30251),
            AOM_ICDF(30949), AOM_ICDF(31873), AOM_ICDF(32467), AOM_ICDF(32703),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3968), AOM_ICDF(5431), AOM_ICDF(8955), AOM_ICDF(11746),
            AOM_ICDF(18914), AOM_ICDF(24489), AOM_ICDF(25524), AOM_ICDF(27194),
            AOM_ICDF(29894), AOM_ICDF(31589), AOM_ICDF(32335), AOM_ICDF(32551),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6016), AOM_ICDF(7584), AOM_ICDF(11814), AOM_ICDF(14567),
            AOM_ICDF(18253), AOM_ICDF(21882), AOM_ICDF(23966), AOM_ICDF(26442),
            AOM_ICDF(27628), AOM_ICDF(31142), AOM_ICDF(32177), AOM_ICDF(32466),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7808), AOM_ICDF(9661), AOM_ICDF(15257), AOM_ICDF(16576),
            AOM_ICDF(20349), AOM_ICDF(24902), AOM_ICDF(26592), AOM_ICDF(29415),
            AOM_ICDF(30083), AOM_ICDF(31782), AOM_ICDF(32360), AOM_ICDF(32680),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(2816), AOM_ICDF(4805), AOM_ICDF(8519), AOM_ICDF(10112),
            AOM_ICDF(13408), AOM_ICDF(18465), AOM_ICDF(19582), AOM_ICDF(21333),
            AOM_ICDF(22494), AOM_ICDF(24059), AOM_ICDF(29026), AOM_ICDF(31321),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(2432), AOM_ICDF(4091), AOM_ICDF(8236), AOM_ICDF(9669),
            AOM_ICDF(13111), AOM_ICDF(19352), AOM_ICDF(20557), AOM_ICDF(22370),
            AOM_ICDF(23060), AOM_ICDF(24425), AOM_ICDF(28890), AOM_ICDF(31586),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3200), AOM_ICDF(5164), AOM_ICDF(8938), AOM_ICDF(11126),
            AOM_ICDF(14494), AOM_ICDF(18433), AOM_ICDF(19721), AOM_ICDF(21148),
            AOM_ICDF(22510), AOM_ICDF(24233), AOM_ICDF(29134), AOM_ICDF(31235),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4480), AOM_ICDF(7132), AOM_ICDF(13341), AOM_ICDF(17959),
            AOM_ICDF(21108), AOM_ICDF(25786), AOM_ICDF(27068), AOM_ICDF(29161),
            AOM_ICDF(30077), AOM_ICDF(31286), AOM_ICDF(32363), AOM_ICDF(32565),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(9600), AOM_ICDF(10686), AOM_ICDF(12152), AOM_ICDF(16918),
            AOM_ICDF(19247), AOM_ICDF(20286), AOM_ICDF(20969), AOM_ICDF(21568),
            AOM_ICDF(25987), AOM_ICDF(27444), AOM_ICDF(30376), AOM_ICDF(31348),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(8832), AOM_ICDF(10609), AOM_ICDF(14591), AOM_ICDF(17948),
            AOM_ICDF(19973), AOM_ICDF(21052), AOM_ICDF(21922), AOM_ICDF(22854),
            AOM_ICDF(25642), AOM_ICDF(26783), AOM_ICDF(29892), AOM_ICDF(31499),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3840), AOM_ICDF(5196), AOM_ICDF(5842), AOM_ICDF(17177),
            AOM_ICDF(19308), AOM_ICDF(19726), AOM_ICDF(20235), AOM_ICDF(20627),
            AOM_ICDF(24184), AOM_ICDF(26799), AOM_ICDF(29993), AOM_ICDF(30752),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(9856), AOM_ICDF(11288), AOM_ICDF(12295), AOM_ICDF(15482),
            AOM_ICDF(18345), AOM_ICDF(19093), AOM_ICDF(20963), AOM_ICDF(21747),
            AOM_ICDF(24718), AOM_ICDF(26793), AOM_ICDF(29991), AOM_ICDF(31032),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6528), AOM_ICDF(7348), AOM_ICDF(8440), AOM_ICDF(11002),
            AOM_ICDF(17084), AOM_ICDF(19749), AOM_ICDF(20766), AOM_ICDF(21563),
            AOM_ICDF(25502), AOM_ICDF(26950), AOM_ICDF(30245), AOM_ICDF(31152),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(9728), AOM_ICDF(10448), AOM_ICDF(12541), AOM_ICDF(14674),
            AOM_ICDF(19296), AOM_ICDF(23919), AOM_ICDF(25198), AOM_ICDF(26558),
            AOM_ICDF(30755), AOM_ICDF(31958), AOM_ICDF(32461), AOM_ICDF(32594),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5120), AOM_ICDF(5768), AOM_ICDF(6401), AOM_ICDF(10321),
            AOM_ICDF(14515), AOM_ICDF(15362), AOM_ICDF(15838), AOM_ICDF(16301),
            AOM_ICDF(26078), AOM_ICDF(27489), AOM_ICDF(30397), AOM_ICDF(31175),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5888), AOM_ICDF(6518), AOM_ICDF(7236), AOM_ICDF(12128),
            AOM_ICDF(14327), AOM_ICDF(15015), AOM_ICDF(16055), AOM_ICDF(16773),
            AOM_ICDF(20897), AOM_ICDF(25395), AOM_ICDF(29341), AOM_ICDF(30452),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(10368), AOM_ICDF(11856), AOM_ICDF(13245), AOM_ICDF(15614),
            AOM_ICDF(18451), AOM_ICDF(19498), AOM_ICDF(20846), AOM_ICDF(22429),
            AOM_ICDF(24610), AOM_ICDF(26522), AOM_ICDF(30279), AOM_ICDF(31523),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6016), AOM_ICDF(7061), AOM_ICDF(8668), AOM_ICDF(12423),
            AOM_ICDF(15346), AOM_ICDF(15634), AOM_ICDF(16504), AOM_ICDF(17584),
            AOM_ICDF(21083), AOM_ICDF(23000), AOM_ICDF(28456), AOM_ICDF(30241),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5760), AOM_ICDF(7026), AOM_ICDF(8735), AOM_ICDF(12665),
            AOM_ICDF(15507), AOM_ICDF(15870), AOM_ICDF(16794), AOM_ICDF(17792),
            AOM_ICDF(21068), AOM_ICDF(23033), AOM_ICDF(28395), AOM_ICDF(30701),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7040), AOM_ICDF(8045), AOM_ICDF(9300), AOM_ICDF(13303),
            AOM_ICDF(15462), AOM_ICDF(15625), AOM_ICDF(16362), AOM_ICDF(17067),
            AOM_ICDF(20686), AOM_ICDF(22810), AOM_ICDF(27983), AOM_ICDF(29347),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5632), AOM_ICDF(8070), AOM_ICDF(9903), AOM_ICDF(16658),
            AOM_ICDF(18637), AOM_ICDF(19728), AOM_ICDF(20543), AOM_ICDF(21450),
            AOM_ICDF(24456), AOM_ICDF(26372), AOM_ICDF(29645), AOM_ICDF(30731),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(11008), AOM_ICDF(12283), AOM_ICDF(14364), AOM_ICDF(18419),
            AOM_ICDF(19948), AOM_ICDF(20618), AOM_ICDF(21899), AOM_ICDF(22960),
            AOM_ICDF(23994), AOM_ICDF(26565), AOM_ICDF(30078), AOM_ICDF(31213),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(9088), AOM_ICDF(11586), AOM_ICDF(16716), AOM_ICDF(18876),
            AOM_ICDF(20112), AOM_ICDF(21105), AOM_ICDF(22426), AOM_ICDF(23800),
            AOM_ICDF(24396), AOM_ICDF(26653), AOM_ICDF(30021), AOM_ICDF(31566),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6144), AOM_ICDF(8328), AOM_ICDF(9378), AOM_ICDF(20096),
            AOM_ICDF(20984), AOM_ICDF(21256), AOM_ICDF(22335), AOM_ICDF(23109),
            AOM_ICDF(24128), AOM_ICDF(26896), AOM_ICDF(29947), AOM_ICDF(30740),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(10496), AOM_ICDF(12323), AOM_ICDF(13441), AOM_ICDF(15479),
            AOM_ICDF(16976), AOM_ICDF(17518), AOM_ICDF(20794), AOM_ICDF(22571),
            AOM_ICDF(23328), AOM_ICDF(27421), AOM_ICDF(30512), AOM_ICDF(31561),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7296), AOM_ICDF(8391), AOM_ICDF(10010), AOM_ICDF(12258),
            AOM_ICDF(15388), AOM_ICDF(16944), AOM_ICDF(19602), AOM_ICDF(21196),
            AOM_ICDF(22869), AOM_ICDF(25112), AOM_ICDF(29389), AOM_ICDF(30709),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(9728), AOM_ICDF(11168), AOM_ICDF(14881), AOM_ICDF(17298),
            AOM_ICDF(20151), AOM_ICDF(22916), AOM_ICDF(25918), AOM_ICDF(28032),
            AOM_ICDF(29549), AOM_ICDF(31787), AOM_ICDF(32293), AOM_ICDF(32521),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6400), AOM_ICDF(7636), AOM_ICDF(8716), AOM_ICDF(12718),
            AOM_ICDF(15711), AOM_ICDF(16420), AOM_ICDF(18144), AOM_ICDF(19287),
            AOM_ICDF(22815), AOM_ICDF(25886), AOM_ICDF(29596), AOM_ICDF(30674),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(9856), AOM_ICDF(10662), AOM_ICDF(11785), AOM_ICDF(14236),
            AOM_ICDF(14998), AOM_ICDF(15391), AOM_ICDF(17156), AOM_ICDF(17949),
            AOM_ICDF(18470), AOM_ICDF(27797), AOM_ICDF(30418), AOM_ICDF(31244),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(8960), AOM_ICDF(10448), AOM_ICDF(12017), AOM_ICDF(14128),
            AOM_ICDF(15765), AOM_ICDF(16637), AOM_ICDF(19347), AOM_ICDF(21759),
            AOM_ICDF(22490), AOM_ICDF(25300), AOM_ICDF(29676), AOM_ICDF(31077),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7168), AOM_ICDF(8468), AOM_ICDF(10177), AOM_ICDF(13693),
            AOM_ICDF(15333), AOM_ICDF(15472), AOM_ICDF(17094), AOM_ICDF(18257),
            AOM_ICDF(19277), AOM_ICDF(22386), AOM_ICDF(28023), AOM_ICDF(29969),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6400), AOM_ICDF(8151), AOM_ICDF(10651), AOM_ICDF(13992),
            AOM_ICDF(15677), AOM_ICDF(15835), AOM_ICDF(17422), AOM_ICDF(18621),
            AOM_ICDF(19450), AOM_ICDF(22207), AOM_ICDF(27735), AOM_ICDF(30409),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7296), AOM_ICDF(8391), AOM_ICDF(9915), AOM_ICDF(13980),
            AOM_ICDF(15352), AOM_ICDF(15450), AOM_ICDF(17006), AOM_ICDF(17930),
            AOM_ICDF(18973), AOM_ICDF(22045), AOM_ICDF(27658), AOM_ICDF(29235),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6272), AOM_ICDF(9481), AOM_ICDF(11664), AOM_ICDF(16537),
            AOM_ICDF(17656), AOM_ICDF(18094), AOM_ICDF(20673), AOM_ICDF(21949),
            AOM_ICDF(22752), AOM_ICDF(25921), AOM_ICDF(29612), AOM_ICDF(30869),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(6784), AOM_ICDF(8104), AOM_ICDF(12536), AOM_ICDF(14589),
            AOM_ICDF(15843), AOM_ICDF(17357), AOM_ICDF(19765), AOM_ICDF(23981),
            AOM_ICDF(24633), AOM_ICDF(25618), AOM_ICDF(29556), AOM_ICDF(31438),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5760), AOM_ICDF(7237), AOM_ICDF(14717), AOM_ICDF(15587),
            AOM_ICDF(16364), AOM_ICDF(17537), AOM_ICDF(20393), AOM_ICDF(26097),
            AOM_ICDF(26462), AOM_ICDF(27029), AOM_ICDF(30123), AOM_ICDF(31921),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4352), AOM_ICDF(5906), AOM_ICDF(8424), AOM_ICDF(16214),
            AOM_ICDF(16978), AOM_ICDF(17743), AOM_ICDF(19680), AOM_ICDF(22441),
            AOM_ICDF(23167), AOM_ICDF(25080), AOM_ICDF(29224), AOM_ICDF(30650),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(9472), AOM_ICDF(10473), AOM_ICDF(12737), AOM_ICDF(14173),
            AOM_ICDF(15051), AOM_ICDF(15632), AOM_ICDF(20652), AOM_ICDF(24864),
            AOM_ICDF(25204), AOM_ICDF(27006), AOM_ICDF(30292), AOM_ICDF(31501),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4480), AOM_ICDF(5475), AOM_ICDF(8247), AOM_ICDF(9646),
            AOM_ICDF(12203), AOM_ICDF(14760), AOM_ICDF(18488), AOM_ICDF(22616),
            AOM_ICDF(23449), AOM_ICDF(24650), AOM_ICDF(29026), AOM_ICDF(30955),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6016), AOM_ICDF(6957), AOM_ICDF(12502), AOM_ICDF(13805),
            AOM_ICDF(16777), AOM_ICDF(21052), AOM_ICDF(23981), AOM_ICDF(30606),
            AOM_ICDF(31206), AOM_ICDF(31981), AOM_ICDF(32414), AOM_ICDF(32681),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4480), AOM_ICDF(5475), AOM_ICDF(7820), AOM_ICDF(9805),
            AOM_ICDF(12793), AOM_ICDF(14252), AOM_ICDF(16711), AOM_ICDF(20725),
            AOM_ICDF(23406), AOM_ICDF(25015), AOM_ICDF(29225), AOM_ICDF(30775),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6912), AOM_ICDF(7619), AOM_ICDF(10173), AOM_ICDF(12424),
            AOM_ICDF(13502), AOM_ICDF(14410), AOM_ICDF(17852), AOM_ICDF(21348),
            AOM_ICDF(22017), AOM_ICDF(25461), AOM_ICDF(29571), AOM_ICDF(31020),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7680), AOM_ICDF(8562), AOM_ICDF(11399), AOM_ICDF(12263),
            AOM_ICDF(12870), AOM_ICDF(13486), AOM_ICDF(18307), AOM_ICDF(26385),
            AOM_ICDF(26734), AOM_ICDF(27724), AOM_ICDF(30482), AOM_ICDF(31955),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4992), AOM_ICDF(6186), AOM_ICDF(9820), AOM_ICDF(11725),
            AOM_ICDF(13117), AOM_ICDF(13406), AOM_ICDF(15978), AOM_ICDF(20372),
            AOM_ICDF(20953), AOM_ICDF(22245), AOM_ICDF(28205), AOM_ICDF(30879),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4736), AOM_ICDF(6050), AOM_ICDF(10747), AOM_ICDF(12295),
            AOM_ICDF(13445), AOM_ICDF(13844), AOM_ICDF(16357), AOM_ICDF(21485),
            AOM_ICDF(21838), AOM_ICDF(22820), AOM_ICDF(28183), AOM_ICDF(31138),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5760), AOM_ICDF(6710), AOM_ICDF(10476), AOM_ICDF(12855),
            AOM_ICDF(14101), AOM_ICDF(14482), AOM_ICDF(17053), AOM_ICDF(20613),
            AOM_ICDF(21278), AOM_ICDF(22580), AOM_ICDF(28351), AOM_ICDF(30542),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4480), AOM_ICDF(6359), AOM_ICDF(11826), AOM_ICDF(14265),
            AOM_ICDF(14852), AOM_ICDF(15753), AOM_ICDF(19276), AOM_ICDF(24757),
            AOM_ICDF(25226), AOM_ICDF(26287), AOM_ICDF(29629), AOM_ICDF(31493),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(7424), AOM_ICDF(8612), AOM_ICDF(11726), AOM_ICDF(15286),
            AOM_ICDF(16881), AOM_ICDF(17151), AOM_ICDF(17944), AOM_ICDF(19160),
            AOM_ICDF(20011), AOM_ICDF(21356), AOM_ICDF(27463), AOM_ICDF(29805),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5376), AOM_ICDF(7516), AOM_ICDF(15210), AOM_ICDF(17109),
            AOM_ICDF(18458), AOM_ICDF(18708), AOM_ICDF(19587), AOM_ICDF(20977),
            AOM_ICDF(21484), AOM_ICDF(22277), AOM_ICDF(27768), AOM_ICDF(30893),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3584), AOM_ICDF(5522), AOM_ICDF(7225), AOM_ICDF(18079),
            AOM_ICDF(18909), AOM_ICDF(18999), AOM_ICDF(19698), AOM_ICDF(20413),
            AOM_ICDF(21185), AOM_ICDF(23040), AOM_ICDF(28056), AOM_ICDF(29473),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7424), AOM_ICDF(8612), AOM_ICDF(10782), AOM_ICDF(12958),
            AOM_ICDF(14687), AOM_ICDF(14818), AOM_ICDF(17553), AOM_ICDF(19395),
            AOM_ICDF(20231), AOM_ICDF(23316), AOM_ICDF(28559), AOM_ICDF(30614),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5632), AOM_ICDF(6586), AOM_ICDF(9347), AOM_ICDF(11520),
            AOM_ICDF(15628), AOM_ICDF(16300), AOM_ICDF(17651), AOM_ICDF(19245),
            AOM_ICDF(20671), AOM_ICDF(22089), AOM_ICDF(28013), AOM_ICDF(30279),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3840), AOM_ICDF(5309), AOM_ICDF(9385), AOM_ICDF(10995),
            AOM_ICDF(14099), AOM_ICDF(18154), AOM_ICDF(19638), AOM_ICDF(21690),
            AOM_ICDF(23031), AOM_ICDF(24552), AOM_ICDF(29238), AOM_ICDF(31251),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5376), AOM_ICDF(6339), AOM_ICDF(8301), AOM_ICDF(11620),
            AOM_ICDF(14701), AOM_ICDF(14991), AOM_ICDF(16033), AOM_ICDF(17210),
            AOM_ICDF(20431), AOM_ICDF(22310), AOM_ICDF(27948), AOM_ICDF(29774),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5632), AOM_ICDF(6692), AOM_ICDF(8729), AOM_ICDF(12618),
            AOM_ICDF(13927), AOM_ICDF(14081), AOM_ICDF(15176), AOM_ICDF(16413),
            AOM_ICDF(17371), AOM_ICDF(22183), AOM_ICDF(28013), AOM_ICDF(29815),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6528), AOM_ICDF(7861), AOM_ICDF(11072), AOM_ICDF(12945),
            AOM_ICDF(14726), AOM_ICDF(14971), AOM_ICDF(16570), AOM_ICDF(19481),
            AOM_ICDF(20260), AOM_ICDF(21921), AOM_ICDF(27980), AOM_ICDF(30449),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5376), AOM_ICDF(6553), AOM_ICDF(9523), AOM_ICDF(12199),
            AOM_ICDF(13764), AOM_ICDF(13972), AOM_ICDF(14926), AOM_ICDF(16320),
            AOM_ICDF(17091), AOM_ICDF(18744), AOM_ICDF(26359), AOM_ICDF(29288),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4736), AOM_ICDF(6160), AOM_ICDF(10318), AOM_ICDF(12718),
            AOM_ICDF(14251), AOM_ICDF(14527), AOM_ICDF(15453), AOM_ICDF(17009),
            AOM_ICDF(17625), AOM_ICDF(19045), AOM_ICDF(26335), AOM_ICDF(30079),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5760), AOM_ICDF(6815), AOM_ICDF(9248), AOM_ICDF(12722),
            AOM_ICDF(14141), AOM_ICDF(14301), AOM_ICDF(15095), AOM_ICDF(16200),
            AOM_ICDF(17106), AOM_ICDF(18697), AOM_ICDF(26172), AOM_ICDF(28388),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4096), AOM_ICDF(6672), AOM_ICDF(11055), AOM_ICDF(16327),
            AOM_ICDF(17508), AOM_ICDF(17671), AOM_ICDF(18733), AOM_ICDF(19994),
            AOM_ICDF(20742), AOM_ICDF(22151), AOM_ICDF(27708), AOM_ICDF(30021),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(7936), AOM_ICDF(9197), AOM_ICDF(13524), AOM_ICDF(16819),
            AOM_ICDF(18267), AOM_ICDF(18636), AOM_ICDF(19409), AOM_ICDF(20661),
            AOM_ICDF(21323), AOM_ICDF(22307), AOM_ICDF(27905), AOM_ICDF(30678),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5376), AOM_ICDF(7302), AOM_ICDF(16951), AOM_ICDF(18383),
            AOM_ICDF(19388), AOM_ICDF(19608), AOM_ICDF(20225), AOM_ICDF(21597),
            AOM_ICDF(21946), AOM_ICDF(22538), AOM_ICDF(27613), AOM_ICDF(31318),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3840), AOM_ICDF(5987), AOM_ICDF(8184), AOM_ICDF(19612),
            AOM_ICDF(20392), AOM_ICDF(20476), AOM_ICDF(21100), AOM_ICDF(21693),
            AOM_ICDF(22428), AOM_ICDF(23963), AOM_ICDF(28709), AOM_ICDF(30342),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(8448), AOM_ICDF(9588), AOM_ICDF(12395), AOM_ICDF(14447),
            AOM_ICDF(16163), AOM_ICDF(16374), AOM_ICDF(18743), AOM_ICDF(20606),
            AOM_ICDF(21271), AOM_ICDF(23786), AOM_ICDF(28768), AOM_ICDF(30877),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5760), AOM_ICDF(6710), AOM_ICDF(10069), AOM_ICDF(11965),
            AOM_ICDF(15976), AOM_ICDF(16719), AOM_ICDF(17973), AOM_ICDF(19880),
            AOM_ICDF(21139), AOM_ICDF(22275), AOM_ICDF(28259), AOM_ICDF(30954),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3968), AOM_ICDF(5431), AOM_ICDF(10557), AOM_ICDF(12069),
            AOM_ICDF(14280), AOM_ICDF(18973), AOM_ICDF(20374), AOM_ICDF(23037),
            AOM_ICDF(24215), AOM_ICDF(25050), AOM_ICDF(29271), AOM_ICDF(31716),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6016), AOM_ICDF(7061), AOM_ICDF(9672), AOM_ICDF(12246),
            AOM_ICDF(15351), AOM_ICDF(15717), AOM_ICDF(16716), AOM_ICDF(18158),
            AOM_ICDF(21126), AOM_ICDF(22672), AOM_ICDF(28035), AOM_ICDF(30494),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6272), AOM_ICDF(7204), AOM_ICDF(9700), AOM_ICDF(13252),
            AOM_ICDF(14599), AOM_ICDF(14926), AOM_ICDF(15902), AOM_ICDF(17220),
            AOM_ICDF(18010), AOM_ICDF(22795), AOM_ICDF(28405), AOM_ICDF(30467),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6912), AOM_ICDF(8427), AOM_ICDF(12420), AOM_ICDF(14171),
            AOM_ICDF(15792), AOM_ICDF(16156), AOM_ICDF(17584), AOM_ICDF(20846),
            AOM_ICDF(21451), AOM_ICDF(22556), AOM_ICDF(28101), AOM_ICDF(31054),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5248), AOM_ICDF(6431), AOM_ICDF(10855), AOM_ICDF(13296),
            AOM_ICDF(14848), AOM_ICDF(15135), AOM_ICDF(15893), AOM_ICDF(17277),
            AOM_ICDF(17943), AOM_ICDF(19275), AOM_ICDF(26443), AOM_ICDF(30174),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4736), AOM_ICDF(6050), AOM_ICDF(12103), AOM_ICDF(14343),
            AOM_ICDF(15633), AOM_ICDF(15978), AOM_ICDF(16699), AOM_ICDF(18205),
            AOM_ICDF(18660), AOM_ICDF(19707), AOM_ICDF(26544), AOM_ICDF(30872),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6016), AOM_ICDF(7166), AOM_ICDF(11066), AOM_ICDF(14088),
            AOM_ICDF(15377), AOM_ICDF(15644), AOM_ICDF(16447), AOM_ICDF(17786),
            AOM_ICDF(18605), AOM_ICDF(19822), AOM_ICDF(27104), AOM_ICDF(29648),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4608), AOM_ICDF(7358), AOM_ICDF(13016), AOM_ICDF(18200),
            AOM_ICDF(19015), AOM_ICDF(19189), AOM_ICDF(20038), AOM_ICDF(21430),
            AOM_ICDF(21917), AOM_ICDF(22977), AOM_ICDF(27949), AOM_ICDF(30848),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(7296), AOM_ICDF(8490), AOM_ICDF(11145), AOM_ICDF(15318),
            AOM_ICDF(16693), AOM_ICDF(16889), AOM_ICDF(17571), AOM_ICDF(18580),
            AOM_ICDF(19688), AOM_ICDF(21272), AOM_ICDF(27245), AOM_ICDF(28971),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5376), AOM_ICDF(7623), AOM_ICDF(16070), AOM_ICDF(18136),
            AOM_ICDF(19225), AOM_ICDF(19397), AOM_ICDF(20128), AOM_ICDF(21362),
            AOM_ICDF(21808), AOM_ICDF(22621), AOM_ICDF(27932), AOM_ICDF(30407),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3200), AOM_ICDF(5164), AOM_ICDF(6566), AOM_ICDF(18368),
            AOM_ICDF(19106), AOM_ICDF(19155), AOM_ICDF(19793), AOM_ICDF(20300),
            AOM_ICDF(21177), AOM_ICDF(23079), AOM_ICDF(27848), AOM_ICDF(28924),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7040), AOM_ICDF(8146), AOM_ICDF(10550), AOM_ICDF(12876),
            AOM_ICDF(14506), AOM_ICDF(14629), AOM_ICDF(17180), AOM_ICDF(19129),
            AOM_ICDF(20088), AOM_ICDF(23407), AOM_ICDF(28673), AOM_ICDF(30257),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6016), AOM_ICDF(7166), AOM_ICDF(9466), AOM_ICDF(11999),
            AOM_ICDF(15723), AOM_ICDF(16293), AOM_ICDF(17580), AOM_ICDF(19004),
            AOM_ICDF(20509), AOM_ICDF(22233), AOM_ICDF(28118), AOM_ICDF(29989),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3840), AOM_ICDF(5422), AOM_ICDF(9054), AOM_ICDF(11018),
            AOM_ICDF(13605), AOM_ICDF(17576), AOM_ICDF(19178), AOM_ICDF(21514),
            AOM_ICDF(22877), AOM_ICDF(24461), AOM_ICDF(29069), AOM_ICDF(30933),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5376), AOM_ICDF(6553), AOM_ICDF(8294), AOM_ICDF(12601),
            AOM_ICDF(15043), AOM_ICDF(15273), AOM_ICDF(16230), AOM_ICDF(17134),
            AOM_ICDF(20737), AOM_ICDF(22899), AOM_ICDF(28219), AOM_ICDF(29410),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5760), AOM_ICDF(6815), AOM_ICDF(8336), AOM_ICDF(12965),
            AOM_ICDF(14282), AOM_ICDF(14444), AOM_ICDF(15446), AOM_ICDF(16461),
            AOM_ICDF(17544), AOM_ICDF(22183), AOM_ICDF(27682), AOM_ICDF(29132),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(6656), AOM_ICDF(8084), AOM_ICDF(10880), AOM_ICDF(12954),
            AOM_ICDF(14527), AOM_ICDF(14728), AOM_ICDF(16490), AOM_ICDF(19224),
            AOM_ICDF(20071), AOM_ICDF(21857), AOM_ICDF(27653), AOM_ICDF(30031),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5376), AOM_ICDF(6660), AOM_ICDF(9006), AOM_ICDF(12205),
            AOM_ICDF(13614), AOM_ICDF(13740), AOM_ICDF(14632), AOM_ICDF(15766),
            AOM_ICDF(16629), AOM_ICDF(18394), AOM_ICDF(25918), AOM_ICDF(28460),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4736), AOM_ICDF(6488), AOM_ICDF(9978), AOM_ICDF(12889),
            AOM_ICDF(14419), AOM_ICDF(14607), AOM_ICDF(15458), AOM_ICDF(16743),
            AOM_ICDF(17369), AOM_ICDF(19053), AOM_ICDF(26393), AOM_ICDF(29456),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5760), AOM_ICDF(6710), AOM_ICDF(8542), AOM_ICDF(12830),
            AOM_ICDF(13956), AOM_ICDF(14031), AOM_ICDF(14763), AOM_ICDF(15677),
            AOM_ICDF(16545), AOM_ICDF(18256), AOM_ICDF(25569), AOM_ICDF(27284),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4096), AOM_ICDF(7008), AOM_ICDF(11436), AOM_ICDF(17228),
            AOM_ICDF(18131), AOM_ICDF(18269), AOM_ICDF(19345), AOM_ICDF(20551),
            AOM_ICDF(21315), AOM_ICDF(22836), AOM_ICDF(28035), AOM_ICDF(29865),
            AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(6528), AOM_ICDF(10833), AOM_ICDF(17688), AOM_ICDF(21947),
            AOM_ICDF(22829), AOM_ICDF(23814), AOM_ICDF(24514), AOM_ICDF(25707),
            AOM_ICDF(26397), AOM_ICDF(27442), AOM_ICDF(30271), AOM_ICDF(31734),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4480), AOM_ICDF(8679), AOM_ICDF(21100), AOM_ICDF(23075),
            AOM_ICDF(23772), AOM_ICDF(24427), AOM_ICDF(25111), AOM_ICDF(26188),
            AOM_ICDF(26445), AOM_ICDF(27235), AOM_ICDF(29980), AOM_ICDF(31875),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(2688), AOM_ICDF(6683), AOM_ICDF(9332), AOM_ICDF(22173),
            AOM_ICDF(22688), AOM_ICDF(22972), AOM_ICDF(23623), AOM_ICDF(24159),
            AOM_ICDF(24798), AOM_ICDF(26666), AOM_ICDF(29812), AOM_ICDF(30909),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(8192), AOM_ICDF(10112), AOM_ICDF(13298), AOM_ICDF(16662),
            AOM_ICDF(17623), AOM_ICDF(18394), AOM_ICDF(20921), AOM_ICDF(22309),
            AOM_ICDF(22963), AOM_ICDF(26257), AOM_ICDF(29945), AOM_ICDF(31423),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5120), AOM_ICDF(7604), AOM_ICDF(12617), AOM_ICDF(15628),
            AOM_ICDF(18274), AOM_ICDF(20174), AOM_ICDF(21404), AOM_ICDF(22869),
            AOM_ICDF(24184), AOM_ICDF(25626), AOM_ICDF(29615), AOM_ICDF(31155),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(7424), AOM_ICDF(10295), AOM_ICDF(18459), AOM_ICDF(21302),
            AOM_ICDF(23034), AOM_ICDF(26284), AOM_ICDF(27576), AOM_ICDF(29746),
            AOM_ICDF(30502), AOM_ICDF(31794), AOM_ICDF(32346), AOM_ICDF(32600),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4224), AOM_ICDF(6789), AOM_ICDF(11254), AOM_ICDF(15589),
            AOM_ICDF(18568), AOM_ICDF(19238), AOM_ICDF(19872), AOM_ICDF(20880),
            AOM_ICDF(24409), AOM_ICDF(26238), AOM_ICDF(29580), AOM_ICDF(30875),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5120), AOM_ICDF(7388), AOM_ICDF(10164), AOM_ICDF(15886),
            AOM_ICDF(16694), AOM_ICDF(17139), AOM_ICDF(18421), AOM_ICDF(19262),
            AOM_ICDF(20106), AOM_ICDF(26734), AOM_ICDF(29987), AOM_ICDF(31160),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(5760), AOM_ICDF(8292), AOM_ICDF(13837), AOM_ICDF(16201),
            AOM_ICDF(17303), AOM_ICDF(18422), AOM_ICDF(20215), AOM_ICDF(23059),
            AOM_ICDF(23628), AOM_ICDF(25449), AOM_ICDF(29537), AOM_ICDF(31455),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4224), AOM_ICDF(7235), AOM_ICDF(12521), AOM_ICDF(16798),
            AOM_ICDF(17964), AOM_ICDF(18136), AOM_ICDF(18936), AOM_ICDF(20233),
            AOM_ICDF(20821), AOM_ICDF(22501), AOM_ICDF(27955), AOM_ICDF(30493),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3840), AOM_ICDF(7117), AOM_ICDF(13329), AOM_ICDF(17383),
            AOM_ICDF(18323), AOM_ICDF(18492), AOM_ICDF(19273), AOM_ICDF(20538),
            AOM_ICDF(21064), AOM_ICDF(22481), AOM_ICDF(27785), AOM_ICDF(30938),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(4736), AOM_ICDF(7474), AOM_ICDF(12414), AOM_ICDF(17230),
            AOM_ICDF(18246), AOM_ICDF(18457), AOM_ICDF(19128), AOM_ICDF(20087),
            AOM_ICDF(20830), AOM_ICDF(22602), AOM_ICDF(27923), AOM_ICDF(29929),
            AOM_ICDF(32768), 0 },
          { AOM_ICDF(3584), AOM_ICDF(9626), AOM_ICDF(15412), AOM_ICDF(20788),
            AOM_ICDF(21676), AOM_ICDF(22192), AOM_ICDF(23266), AOM_ICDF(24342),
            AOM_ICDF(24836), AOM_ICDF(26447), AOM_ICDF(29583), AOM_ICDF(31300),
            AOM_ICDF(32768), 0 },
      },
#else
      {
          { AOM_ICDF(15488), AOM_ICDF(17513), AOM_ICDF(20731), AOM_ICDF(24586),
            AOM_ICDF(25921), AOM_ICDF(26749), AOM_ICDF(27807), AOM_ICDF(28602),
            AOM_ICDF(29530), AOM_ICDF(30681), AOM_ICDF(32768), 0 },
          { AOM_ICDF(11648), AOM_ICDF(14783), AOM_ICDF(21879), AOM_ICDF(23981),
            AOM_ICDF(25213), AOM_ICDF(26218), AOM_ICDF(27472), AOM_ICDF(28465),
            AOM_ICDF(29221), AOM_ICDF(30232), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8448), AOM_ICDF(11108), AOM_ICDF(13392), AOM_ICDF(25167),
            AOM_ICDF(26295), AOM_ICDF(26789), AOM_ICDF(27536), AOM_ICDF(28088),
            AOM_ICDF(29039), AOM_ICDF(30700), AOM_ICDF(32768), 0 },
          { AOM_ICDF(13568), AOM_ICDF(15293), AOM_ICDF(18706), AOM_ICDF(21610),
            AOM_ICDF(23139), AOM_ICDF(24254), AOM_ICDF(26383), AOM_ICDF(27630),
            AOM_ICDF(28613), AOM_ICDF(30350), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9600), AOM_ICDF(11772), AOM_ICDF(14397), AOM_ICDF(16580),
            AOM_ICDF(20091), AOM_ICDF(22865), AOM_ICDF(24490), AOM_ICDF(25395),
            AOM_ICDF(27037), AOM_ICDF(28694), AOM_ICDF(32768), 0 },
          { AOM_ICDF(12160), AOM_ICDF(14092), AOM_ICDF(17010), AOM_ICDF(18922),
            AOM_ICDF(22683), AOM_ICDF(25751), AOM_ICDF(27725), AOM_ICDF(30109),
            AOM_ICDF(31449), AOM_ICDF(32763), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9088), AOM_ICDF(10383), AOM_ICDF(12569), AOM_ICDF(17113),
            AOM_ICDF(21351), AOM_ICDF(22511), AOM_ICDF(23633), AOM_ICDF(24382),
            AOM_ICDF(28215), AOM_ICDF(29798), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10880), AOM_ICDF(12248), AOM_ICDF(15214), AOM_ICDF(20017),
            AOM_ICDF(21922), AOM_ICDF(22757), AOM_ICDF(24360), AOM_ICDF(25280),
            AOM_ICDF(26684), AOM_ICDF(29869), AOM_ICDF(32768), 0 },
          { AOM_ICDF(11008), AOM_ICDF(13133), AOM_ICDF(15587), AOM_ICDF(17872),
            AOM_ICDF(19579), AOM_ICDF(21157), AOM_ICDF(23788), AOM_ICDF(26629),
            AOM_ICDF(27732), AOM_ICDF(29601), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10112), AOM_ICDF(12325), AOM_ICDF(15360), AOM_ICDF(18348),
            AOM_ICDF(20452), AOM_ICDF(20460), AOM_ICDF(21902), AOM_ICDF(23982),
            AOM_ICDF(25149), AOM_ICDF(26667), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8704), AOM_ICDF(14250), AOM_ICDF(17722), AOM_ICDF(23128),
            AOM_ICDF(24217), AOM_ICDF(24892), AOM_ICDF(26215), AOM_ICDF(27392),
            AOM_ICDF(28358), AOM_ICDF(30287), AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(8448), AOM_ICDF(10443), AOM_ICDF(20733), AOM_ICDF(23689),
            AOM_ICDF(24634), AOM_ICDF(25951), AOM_ICDF(26670), AOM_ICDF(27861),
            AOM_ICDF(28379), AOM_ICDF(29305), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6656), AOM_ICDF(9206), AOM_ICDF(24577), AOM_ICDF(25792),
            AOM_ICDF(26335), AOM_ICDF(27169), AOM_ICDF(27913), AOM_ICDF(28956),
            AOM_ICDF(29239), AOM_ICDF(29680), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7168), AOM_ICDF(8968), AOM_ICDF(15662), AOM_ICDF(22937),
            AOM_ICDF(23849), AOM_ICDF(24616), AOM_ICDF(25603), AOM_ICDF(26555),
            AOM_ICDF(27210), AOM_ICDF(29142), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9600), AOM_ICDF(11501), AOM_ICDF(19310), AOM_ICDF(21731),
            AOM_ICDF(22790), AOM_ICDF(23936), AOM_ICDF(25627), AOM_ICDF(27217),
            AOM_ICDF(27868), AOM_ICDF(29170), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6912), AOM_ICDF(8730), AOM_ICDF(17650), AOM_ICDF(19377),
            AOM_ICDF(21025), AOM_ICDF(23319), AOM_ICDF(24537), AOM_ICDF(26112),
            AOM_ICDF(26840), AOM_ICDF(28345), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7808), AOM_ICDF(9661), AOM_ICDF(20583), AOM_ICDF(21996),
            AOM_ICDF(23898), AOM_ICDF(26818), AOM_ICDF(28120), AOM_ICDF(30716),
            AOM_ICDF(31678), AOM_ICDF(32764), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6784), AOM_ICDF(8104), AOM_ICDF(15619), AOM_ICDF(18584),
            AOM_ICDF(20844), AOM_ICDF(22519), AOM_ICDF(23760), AOM_ICDF(25203),
            AOM_ICDF(27094), AOM_ICDF(28801), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8832), AOM_ICDF(10141), AOM_ICDF(17035), AOM_ICDF(20764),
            AOM_ICDF(21703), AOM_ICDF(22751), AOM_ICDF(23964), AOM_ICDF(25305),
            AOM_ICDF(26034), AOM_ICDF(29006), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8192), AOM_ICDF(9920), AOM_ICDF(19113), AOM_ICDF(20594),
            AOM_ICDF(21747), AOM_ICDF(23327), AOM_ICDF(24581), AOM_ICDF(26916),
            AOM_ICDF(27533), AOM_ICDF(28944), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6656), AOM_ICDF(8696), AOM_ICDF(18381), AOM_ICDF(20537),
            AOM_ICDF(21804), AOM_ICDF(21809), AOM_ICDF(22751), AOM_ICDF(24394),
            AOM_ICDF(24917), AOM_ICDF(25990), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6400), AOM_ICDF(9593), AOM_ICDF(20818), AOM_ICDF(23519),
            AOM_ICDF(24266), AOM_ICDF(25113), AOM_ICDF(26608), AOM_ICDF(27883),
            AOM_ICDF(28322), AOM_ICDF(29364), AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(12032), AOM_ICDF(14381), AOM_ICDF(16608), AOM_ICDF(24946),
            AOM_ICDF(26084), AOM_ICDF(26582), AOM_ICDF(27428), AOM_ICDF(28075),
            AOM_ICDF(29395), AOM_ICDF(30858), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9216), AOM_ICDF(12620), AOM_ICDF(18287), AOM_ICDF(24345),
            AOM_ICDF(25984), AOM_ICDF(26715), AOM_ICDF(27732), AOM_ICDF(28519),
            AOM_ICDF(29399), AOM_ICDF(30781), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6784), AOM_ICDF(8916), AOM_ICDF(10220), AOM_ICDF(26539),
            AOM_ICDF(27310), AOM_ICDF(27483), AOM_ICDF(28082), AOM_ICDF(28430),
            AOM_ICDF(29362), AOM_ICDF(31291), AOM_ICDF(32768), 0 },
          { AOM_ICDF(11904), AOM_ICDF(14838), AOM_ICDF(17359), AOM_ICDF(21663),
            AOM_ICDF(22931), AOM_ICDF(23619), AOM_ICDF(25620), AOM_ICDF(26653),
            AOM_ICDF(27823), AOM_ICDF(30547), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10752), AOM_ICDF(13504), AOM_ICDF(15536), AOM_ICDF(19057),
            AOM_ICDF(21753), AOM_ICDF(23883), AOM_ICDF(25202), AOM_ICDF(26266),
            AOM_ICDF(28196), AOM_ICDF(30589), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10496), AOM_ICDF(13193), AOM_ICDF(16787), AOM_ICDF(21011),
            AOM_ICDF(23929), AOM_ICDF(25651), AOM_ICDF(27958), AOM_ICDF(29330),
            AOM_ICDF(31022), AOM_ICDF(32761), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8448), AOM_ICDF(9968), AOM_ICDF(11749), AOM_ICDF(18062),
            AOM_ICDF(21841), AOM_ICDF(22669), AOM_ICDF(23852), AOM_ICDF(24444),
            AOM_ICDF(28118), AOM_ICDF(30007), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9728), AOM_ICDF(11168), AOM_ICDF(12602), AOM_ICDF(20819),
            AOM_ICDF(22194), AOM_ICDF(22764), AOM_ICDF(24366), AOM_ICDF(25022),
            AOM_ICDF(26414), AOM_ICDF(30460), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9216), AOM_ICDF(12712), AOM_ICDF(14357), AOM_ICDF(18346),
            AOM_ICDF(20486), AOM_ICDF(21549), AOM_ICDF(23170), AOM_ICDF(25794),
            AOM_ICDF(27129), AOM_ICDF(29574), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7808), AOM_ICDF(10733), AOM_ICDF(13057), AOM_ICDF(20252),
            AOM_ICDF(21906), AOM_ICDF(21912), AOM_ICDF(23057), AOM_ICDF(24233),
            AOM_ICDF(25700), AOM_ICDF(27439), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6784), AOM_ICDF(11352), AOM_ICDF(13778), AOM_ICDF(23877),
            AOM_ICDF(24995), AOM_ICDF(25424), AOM_ICDF(26830), AOM_ICDF(27688),
            AOM_ICDF(28779), AOM_ICDF(30368), AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(12288), AOM_ICDF(13728), AOM_ICDF(16480), AOM_ICDF(19841),
            AOM_ICDF(21570), AOM_ICDF(22715), AOM_ICDF(25385), AOM_ICDF(27000),
            AOM_ICDF(28329), AOM_ICDF(29994), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9344), AOM_ICDF(10991), AOM_ICDF(18817), AOM_ICDF(20972),
            AOM_ICDF(22137), AOM_ICDF(23231), AOM_ICDF(26025), AOM_ICDF(27711),
            AOM_ICDF(28244), AOM_ICDF(29428), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9344), AOM_ICDF(10900), AOM_ICDF(13206), AOM_ICDF(21344),
            AOM_ICDF(22332), AOM_ICDF(22987), AOM_ICDF(25127), AOM_ICDF(26440),
            AOM_ICDF(27231), AOM_ICDF(29502), AOM_ICDF(32768), 0 },
          { AOM_ICDF(12928), AOM_ICDF(14478), AOM_ICDF(15978), AOM_ICDF(18630),
            AOM_ICDF(19852), AOM_ICDF(20897), AOM_ICDF(24699), AOM_ICDF(26464),
            AOM_ICDF(27030), AOM_ICDF(30482), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9088), AOM_ICDF(10476), AOM_ICDF(13350), AOM_ICDF(15237),
            AOM_ICDF(18175), AOM_ICDF(20252), AOM_ICDF(23283), AOM_ICDF(25321),
            AOM_ICDF(26426), AOM_ICDF(29349), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10240), AOM_ICDF(11912), AOM_ICDF(15008), AOM_ICDF(17177),
            AOM_ICDF(19979), AOM_ICDF(23056), AOM_ICDF(26395), AOM_ICDF(29681),
            AOM_ICDF(30790), AOM_ICDF(32760), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8704), AOM_ICDF(9738), AOM_ICDF(11717), AOM_ICDF(15480),
            AOM_ICDF(18656), AOM_ICDF(20022), AOM_ICDF(22611), AOM_ICDF(24357),
            AOM_ICDF(27150), AOM_ICDF(29257), AOM_ICDF(32768), 0 },
          { AOM_ICDF(12928), AOM_ICDF(13548), AOM_ICDF(17978), AOM_ICDF(20602),
            AOM_ICDF(21814), AOM_ICDF(22427), AOM_ICDF(24568), AOM_ICDF(25881),
            AOM_ICDF(26823), AOM_ICDF(30817), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10496), AOM_ICDF(12149), AOM_ICDF(14082), AOM_ICDF(18054),
            AOM_ICDF(19032), AOM_ICDF(19994), AOM_ICDF(24086), AOM_ICDF(28427),
            AOM_ICDF(29156), AOM_ICDF(30680), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8448), AOM_ICDF(10158), AOM_ICDF(13867), AOM_ICDF(16506),
            AOM_ICDF(18584), AOM_ICDF(18592), AOM_ICDF(21472), AOM_ICDF(23767),
            AOM_ICDF(24646), AOM_ICDF(27279), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7296), AOM_ICDF(9684), AOM_ICDF(13471), AOM_ICDF(17701),
            AOM_ICDF(18934), AOM_ICDF(19878), AOM_ICDF(25115), AOM_ICDF(27238),
            AOM_ICDF(27972), AOM_ICDF(29583), AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(10880), AOM_ICDF(12163), AOM_ICDF(14497), AOM_ICDF(17112),
            AOM_ICDF(20859), AOM_ICDF(22562), AOM_ICDF(23599), AOM_ICDF(24638),
            AOM_ICDF(26861), AOM_ICDF(29399), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9984), AOM_ICDF(12476), AOM_ICDF(16360), AOM_ICDF(18889),
            AOM_ICDF(21414), AOM_ICDF(23474), AOM_ICDF(24563), AOM_ICDF(25909),
            AOM_ICDF(27195), AOM_ICDF(28828), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7168), AOM_ICDF(9268), AOM_ICDF(10737), AOM_ICDF(20063),
            AOM_ICDF(22315), AOM_ICDF(23302), AOM_ICDF(24152), AOM_ICDF(25195),
            AOM_ICDF(26645), AOM_ICDF(28845), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8960), AOM_ICDF(10727), AOM_ICDF(12449), AOM_ICDF(14263),
            AOM_ICDF(16523), AOM_ICDF(17608), AOM_ICDF(23352), AOM_ICDF(24676),
            AOM_ICDF(26478), AOM_ICDF(28886), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9856), AOM_ICDF(11109), AOM_ICDF(13309), AOM_ICDF(14975),
            AOM_ICDF(19055), AOM_ICDF(21670), AOM_ICDF(23144), AOM_ICDF(24460),
            AOM_ICDF(26212), AOM_ICDF(28107), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9984), AOM_ICDF(11586), AOM_ICDF(14565), AOM_ICDF(16562),
            AOM_ICDF(21107), AOM_ICDF(25444), AOM_ICDF(27218), AOM_ICDF(29429),
            AOM_ICDF(31451), AOM_ICDF(32763), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7168), AOM_ICDF(8268), AOM_ICDF(9704), AOM_ICDF(13144),
            AOM_ICDF(18443), AOM_ICDF(20065), AOM_ICDF(21653), AOM_ICDF(23607),
            AOM_ICDF(26506), AOM_ICDF(28854), AOM_ICDF(32768), 0 },
          { AOM_ICDF(11520), AOM_ICDF(13014), AOM_ICDF(14866), AOM_ICDF(18136),
            AOM_ICDF(20231), AOM_ICDF(21509), AOM_ICDF(23004), AOM_ICDF(24186),
            AOM_ICDF(25728), AOM_ICDF(29468), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10240), AOM_ICDF(12264), AOM_ICDF(14507), AOM_ICDF(16388),
            AOM_ICDF(18888), AOM_ICDF(20927), AOM_ICDF(22731), AOM_ICDF(24691),
            AOM_ICDF(26142), AOM_ICDF(28394), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8064), AOM_ICDF(10187), AOM_ICDF(12921), AOM_ICDF(15952),
            AOM_ICDF(19960), AOM_ICDF(19976), AOM_ICDF(21275), AOM_ICDF(23205),
            AOM_ICDF(25110), AOM_ICDF(26636), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8448), AOM_ICDF(11488), AOM_ICDF(14065), AOM_ICDF(19113),
            AOM_ICDF(21604), AOM_ICDF(22978), AOM_ICDF(24508), AOM_ICDF(25895),
            AOM_ICDF(27398), AOM_ICDF(29055), AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(10368), AOM_ICDF(11768), AOM_ICDF(16772), AOM_ICDF(19842),
            AOM_ICDF(22940), AOM_ICDF(27394), AOM_ICDF(28528), AOM_ICDF(30267),
            AOM_ICDF(31371), AOM_ICDF(32763), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9472), AOM_ICDF(11292), AOM_ICDF(18507), AOM_ICDF(20777),
            AOM_ICDF(23357), AOM_ICDF(27587), AOM_ICDF(28902), AOM_ICDF(30850),
            AOM_ICDF(31607), AOM_ICDF(32763), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8064), AOM_ICDF(9512), AOM_ICDF(13782), AOM_ICDF(20645),
            AOM_ICDF(24493), AOM_ICDF(26242), AOM_ICDF(28001), AOM_ICDF(29435),
            AOM_ICDF(30438), AOM_ICDF(32759), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8960), AOM_ICDF(10541), AOM_ICDF(15664), AOM_ICDF(17639),
            AOM_ICDF(19646), AOM_ICDF(22145), AOM_ICDF(25216), AOM_ICDF(28815),
            AOM_ICDF(30050), AOM_ICDF(32757), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9984), AOM_ICDF(11141), AOM_ICDF(15365), AOM_ICDF(16746),
            AOM_ICDF(21186), AOM_ICDF(25766), AOM_ICDF(27817), AOM_ICDF(30022),
            AOM_ICDF(31309), AOM_ICDF(32762), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9216), AOM_ICDF(10688), AOM_ICDF(16639), AOM_ICDF(17735),
            AOM_ICDF(21499), AOM_ICDF(26657), AOM_ICDF(28161), AOM_ICDF(30572),
            AOM_ICDF(31490), AOM_ICDF(32763), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8448), AOM_ICDF(9303), AOM_ICDF(13611), AOM_ICDF(16636),
            AOM_ICDF(20555), AOM_ICDF(23414), AOM_ICDF(24912), AOM_ICDF(27613),
            AOM_ICDF(29727), AOM_ICDF(32756), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9984), AOM_ICDF(11052), AOM_ICDF(16142), AOM_ICDF(19312),
            AOM_ICDF(21680), AOM_ICDF(23870), AOM_ICDF(25504), AOM_ICDF(28200),
            AOM_ICDF(29324), AOM_ICDF(32755), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10496), AOM_ICDF(12323), AOM_ICDF(16955), AOM_ICDF(18839),
            AOM_ICDF(21144), AOM_ICDF(24861), AOM_ICDF(26838), AOM_ICDF(29988),
            AOM_ICDF(30976), AOM_ICDF(32761), AOM_ICDF(32768), 0 },
          { AOM_ICDF(2944), AOM_ICDF(5973), AOM_ICDF(8904), AOM_ICDF(11875),
            AOM_ICDF(14864), AOM_ICDF(17853), AOM_ICDF(20824), AOM_ICDF(23810),
            AOM_ICDF(26784), AOM_ICDF(29776), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7424), AOM_ICDF(10097), AOM_ICDF(15588), AOM_ICDF(20217),
            AOM_ICDF(23899), AOM_ICDF(26460), AOM_ICDF(28308), AOM_ICDF(30155),
            AOM_ICDF(30951), AOM_ICDF(32761), AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(11648), AOM_ICDF(13133), AOM_ICDF(15050), AOM_ICDF(20481),
            AOM_ICDF(22470), AOM_ICDF(23425), AOM_ICDF(24337), AOM_ICDF(25160),
            AOM_ICDF(28964), AOM_ICDF(30480), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10240), AOM_ICDF(12616), AOM_ICDF(16631), AOM_ICDF(20485),
            AOM_ICDF(22290), AOM_ICDF(23628), AOM_ICDF(25235), AOM_ICDF(26353),
            AOM_ICDF(28107), AOM_ICDF(29655), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6784), AOM_ICDF(8002), AOM_ICDF(9066), AOM_ICDF(20038),
            AOM_ICDF(22926), AOM_ICDF(23324), AOM_ICDF(23951), AOM_ICDF(24537),
            AOM_ICDF(26916), AOM_ICDF(30231), AOM_ICDF(32768), 0 },
          { AOM_ICDF(11904), AOM_ICDF(14105), AOM_ICDF(15782), AOM_ICDF(19896),
            AOM_ICDF(22283), AOM_ICDF(23147), AOM_ICDF(24763), AOM_ICDF(25983),
            AOM_ICDF(27812), AOM_ICDF(29980), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10624), AOM_ICDF(11922), AOM_ICDF(13632), AOM_ICDF(15941),
            AOM_ICDF(20469), AOM_ICDF(22453), AOM_ICDF(24065), AOM_ICDF(25187),
            AOM_ICDF(27349), AOM_ICDF(29296), AOM_ICDF(32768), 0 },
          { AOM_ICDF(12032), AOM_ICDF(13085), AOM_ICDF(15468), AOM_ICDF(17768),
            AOM_ICDF(20613), AOM_ICDF(24388), AOM_ICDF(26385), AOM_ICDF(28430),
            AOM_ICDF(30938), AOM_ICDF(32761), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9728), AOM_ICDF(10538), AOM_ICDF(11493), AOM_ICDF(14765),
            AOM_ICDF(18460), AOM_ICDF(19471), AOM_ICDF(20302), AOM_ICDF(20935),
            AOM_ICDF(28192), AOM_ICDF(29926), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8960), AOM_ICDF(9890), AOM_ICDF(10962), AOM_ICDF(16685),
            AOM_ICDF(18880), AOM_ICDF(19480), AOM_ICDF(20674), AOM_ICDF(21477),
            AOM_ICDF(23815), AOM_ICDF(29341), AOM_ICDF(32768), 0 },
          { AOM_ICDF(14592), AOM_ICDF(16367), AOM_ICDF(17712), AOM_ICDF(20293),
            AOM_ICDF(22544), AOM_ICDF(23829), AOM_ICDF(24877), AOM_ICDF(26326),
            AOM_ICDF(27660), AOM_ICDF(29875), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8960), AOM_ICDF(10448), AOM_ICDF(12279), AOM_ICDF(16206),
            AOM_ICDF(18672), AOM_ICDF(18682), AOM_ICDF(20058), AOM_ICDF(21547),
            AOM_ICDF(25097), AOM_ICDF(27165), AOM_ICDF(32768), 0 },
          { AOM_ICDF(11136), AOM_ICDF(13840), AOM_ICDF(15762), AOM_ICDF(21710),
            AOM_ICDF(23038), AOM_ICDF(23734), AOM_ICDF(24863), AOM_ICDF(25882),
            AOM_ICDF(27765), AOM_ICDF(30071), AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(12544), AOM_ICDF(14124), AOM_ICDF(16964), AOM_ICDF(21907),
            AOM_ICDF(23808), AOM_ICDF(24496), AOM_ICDF(25724), AOM_ICDF(26715),
            AOM_ICDF(27992), AOM_ICDF(30455), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10368), AOM_ICDF(13606), AOM_ICDF(18247), AOM_ICDF(20869),
            AOM_ICDF(22590), AOM_ICDF(23749), AOM_ICDF(25088), AOM_ICDF(26378),
            AOM_ICDF(27277), AOM_ICDF(29808), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9088), AOM_ICDF(11031), AOM_ICDF(12899), AOM_ICDF(23497),
            AOM_ICDF(24465), AOM_ICDF(24851), AOM_ICDF(25995), AOM_ICDF(26815),
            AOM_ICDF(27885), AOM_ICDF(30555), AOM_ICDF(32768), 0 },
          { AOM_ICDF(11520), AOM_ICDF(14342), AOM_ICDF(15710), AOM_ICDF(19196),
            AOM_ICDF(21250), AOM_ICDF(21907), AOM_ICDF(24665), AOM_ICDF(26153),
            AOM_ICDF(27212), AOM_ICDF(30750), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9984), AOM_ICDF(11764), AOM_ICDF(13979), AOM_ICDF(16405),
            AOM_ICDF(19279), AOM_ICDF(20658), AOM_ICDF(23354), AOM_ICDF(25266),
            AOM_ICDF(26702), AOM_ICDF(29380), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10112), AOM_ICDF(12325), AOM_ICDF(15918), AOM_ICDF(19060),
            AOM_ICDF(21829), AOM_ICDF(23882), AOM_ICDF(26277), AOM_ICDF(27697),
            AOM_ICDF(30114), AOM_ICDF(32758), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9344), AOM_ICDF(10534), AOM_ICDF(12184), AOM_ICDF(16208),
            AOM_ICDF(19764), AOM_ICDF(20627), AOM_ICDF(22524), AOM_ICDF(23644),
            AOM_ICDF(26887), AOM_ICDF(29782), AOM_ICDF(32768), 0 },
          { AOM_ICDF(12928), AOM_ICDF(14013), AOM_ICDF(15625), AOM_ICDF(19107),
            AOM_ICDF(20654), AOM_ICDF(21451), AOM_ICDF(22910), AOM_ICDF(23873),
            AOM_ICDF(24776), AOM_ICDF(30239), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10368), AOM_ICDF(12818), AOM_ICDF(14610), AOM_ICDF(17350),
            AOM_ICDF(19568), AOM_ICDF(20710), AOM_ICDF(22971), AOM_ICDF(25114),
            AOM_ICDF(26340), AOM_ICDF(29127), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8960), AOM_ICDF(11192), AOM_ICDF(13720), AOM_ICDF(18429),
            AOM_ICDF(20409), AOM_ICDF(20417), AOM_ICDF(22250), AOM_ICDF(23318),
            AOM_ICDF(24647), AOM_ICDF(27248), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7808), AOM_ICDF(11416), AOM_ICDF(13918), AOM_ICDF(19028),
            AOM_ICDF(20181), AOM_ICDF(20839), AOM_ICDF(24380), AOM_ICDF(26018),
            AOM_ICDF(26967), AOM_ICDF(29845), AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(9856), AOM_ICDF(11020), AOM_ICDF(14928), AOM_ICDF(18159),
            AOM_ICDF(19421), AOM_ICDF(20921), AOM_ICDF(23466), AOM_ICDF(26664),
            AOM_ICDF(27475), AOM_ICDF(28881), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8704), AOM_ICDF(10302), AOM_ICDF(17323), AOM_ICDF(18907),
            AOM_ICDF(19868), AOM_ICDF(21184), AOM_ICDF(24171), AOM_ICDF(28033),
            AOM_ICDF(28625), AOM_ICDF(29353), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7936), AOM_ICDF(9197), AOM_ICDF(12604), AOM_ICDF(20616),
            AOM_ICDF(21514), AOM_ICDF(22371), AOM_ICDF(24239), AOM_ICDF(26138),
            AOM_ICDF(26863), AOM_ICDF(29239), AOM_ICDF(32768), 0 },
          { AOM_ICDF(11264), AOM_ICDF(12524), AOM_ICDF(16083), AOM_ICDF(18574),
            AOM_ICDF(19858), AOM_ICDF(20841), AOM_ICDF(24242), AOM_ICDF(27606),
            AOM_ICDF(28352), AOM_ICDF(29853), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8704), AOM_ICDF(10208), AOM_ICDF(13292), AOM_ICDF(15170),
            AOM_ICDF(17277), AOM_ICDF(19226), AOM_ICDF(22083), AOM_ICDF(25046),
            AOM_ICDF(26041), AOM_ICDF(27802), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9088), AOM_ICDF(10568), AOM_ICDF(15511), AOM_ICDF(17246),
            AOM_ICDF(20170), AOM_ICDF(22791), AOM_ICDF(25558), AOM_ICDF(30740),
            AOM_ICDF(31635), AOM_ICDF(32764), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7040), AOM_ICDF(8045), AOM_ICDF(10653), AOM_ICDF(13145),
            AOM_ICDF(15286), AOM_ICDF(16614), AOM_ICDF(19075), AOM_ICDF(23140),
            AOM_ICDF(26224), AOM_ICDF(28652), AOM_ICDF(32768), 0 },
          { AOM_ICDF(10240), AOM_ICDF(11032), AOM_ICDF(14258), AOM_ICDF(17629),
            AOM_ICDF(18914), AOM_ICDF(19898), AOM_ICDF(22412), AOM_ICDF(24961),
            AOM_ICDF(25815), AOM_ICDF(29156), AOM_ICDF(32768), 0 },
          { AOM_ICDF(11008), AOM_ICDF(12028), AOM_ICDF(14702), AOM_ICDF(16147),
            AOM_ICDF(17209), AOM_ICDF(18160), AOM_ICDF(21812), AOM_ICDF(27547),
            AOM_ICDF(28709), AOM_ICDF(30120), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7168), AOM_ICDF(9068), AOM_ICDF(14160), AOM_ICDF(16937),
            AOM_ICDF(18515), AOM_ICDF(18521), AOM_ICDF(20636), AOM_ICDF(24617),
            AOM_ICDF(25317), AOM_ICDF(26365), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6784), AOM_ICDF(8510), AOM_ICDF(14195), AOM_ICDF(17148),
            AOM_ICDF(18158), AOM_ICDF(19201), AOM_ICDF(23070), AOM_ICDF(27351),
            AOM_ICDF(27901), AOM_ICDF(29422), AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(10112), AOM_ICDF(11528), AOM_ICDF(15345), AOM_ICDF(19296),
            AOM_ICDF(21394), AOM_ICDF(21402), AOM_ICDF(22379), AOM_ICDF(23840),
            AOM_ICDF(24851), AOM_ICDF(26150), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8064), AOM_ICDF(10187), AOM_ICDF(17949), AOM_ICDF(20052),
            AOM_ICDF(22051), AOM_ICDF(22059), AOM_ICDF(23147), AOM_ICDF(24688),
            AOM_ICDF(25351), AOM_ICDF(26365), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6528), AOM_ICDF(8373), AOM_ICDF(11041), AOM_ICDF(21963),
            AOM_ICDF(23089), AOM_ICDF(23093), AOM_ICDF(24076), AOM_ICDF(24925),
            AOM_ICDF(25691), AOM_ICDF(27764), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9600), AOM_ICDF(11229), AOM_ICDF(14847), AOM_ICDF(17527),
            AOM_ICDF(19738), AOM_ICDF(19747), AOM_ICDF(21629), AOM_ICDF(23761),
            AOM_ICDF(24957), AOM_ICDF(27673), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8960), AOM_ICDF(10262), AOM_ICDF(13339), AOM_ICDF(15480),
            AOM_ICDF(19925), AOM_ICDF(19942), AOM_ICDF(21445), AOM_ICDF(23037),
            AOM_ICDF(24329), AOM_ICDF(25977), AOM_ICDF(32768), 0 },
          { AOM_ICDF(2944), AOM_ICDF(5973), AOM_ICDF(8904), AOM_ICDF(11875),
            AOM_ICDF(14864), AOM_ICDF(17853), AOM_ICDF(20824), AOM_ICDF(23810),
            AOM_ICDF(26784), AOM_ICDF(29776), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9472), AOM_ICDF(10564), AOM_ICDF(13426), AOM_ICDF(16561),
            AOM_ICDF(19685), AOM_ICDF(19697), AOM_ICDF(21076), AOM_ICDF(22583),
            AOM_ICDF(24891), AOM_ICDF(26983), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8448), AOM_ICDF(9493), AOM_ICDF(12221), AOM_ICDF(16542),
            AOM_ICDF(18394), AOM_ICDF(18401), AOM_ICDF(19580), AOM_ICDF(20971),
            AOM_ICDF(22031), AOM_ICDF(26770), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8704), AOM_ICDF(10772), AOM_ICDF(14209), AOM_ICDF(16381),
            AOM_ICDF(18911), AOM_ICDF(18921), AOM_ICDF(20436), AOM_ICDF(23374),
            AOM_ICDF(24475), AOM_ICDF(26095), AOM_ICDF(32768), 0 },
          { AOM_ICDF(7680), AOM_ICDF(9444), AOM_ICDF(13453), AOM_ICDF(16320),
            AOM_ICDF(18650), AOM_ICDF(18659), AOM_ICDF(19651), AOM_ICDF(21291),
            AOM_ICDF(22277), AOM_ICDF(23916), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6656), AOM_ICDF(9920), AOM_ICDF(14740), AOM_ICDF(19864),
            AOM_ICDF(21495), AOM_ICDF(21501), AOM_ICDF(22953), AOM_ICDF(24372),
            AOM_ICDF(25192), AOM_ICDF(26760), AOM_ICDF(32768), 0 },
      },
      {
          { AOM_ICDF(9728), AOM_ICDF(13958), AOM_ICDF(18881), AOM_ICDF(23624),
            AOM_ICDF(24754), AOM_ICDF(25553), AOM_ICDF(26709), AOM_ICDF(27940),
            AOM_ICDF(28977), AOM_ICDF(30413), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8832), AOM_ICDF(12572), AOM_ICDF(22433), AOM_ICDF(24653),
            AOM_ICDF(25676), AOM_ICDF(26551), AOM_ICDF(27571), AOM_ICDF(28688),
            AOM_ICDF(29198), AOM_ICDF(30174), AOM_ICDF(32768), 0 },
          { AOM_ICDF(5888), AOM_ICDF(8828), AOM_ICDF(11353), AOM_ICDF(23482),
            AOM_ICDF(24310), AOM_ICDF(24737), AOM_ICDF(25804), AOM_ICDF(26375),
            AOM_ICDF(27174), AOM_ICDF(29840), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9984), AOM_ICDF(13099), AOM_ICDF(16249), AOM_ICDF(19443),
            AOM_ICDF(20990), AOM_ICDF(22637), AOM_ICDF(24576), AOM_ICDF(25952),
            AOM_ICDF(26884), AOM_ICDF(29435), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8448), AOM_ICDF(11108), AOM_ICDF(15085), AOM_ICDF(18134),
            AOM_ICDF(20319), AOM_ICDF(21992), AOM_ICDF(23549), AOM_ICDF(24989),
            AOM_ICDF(27177), AOM_ICDF(29208), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9856), AOM_ICDF(13168), AOM_ICDF(18987), AOM_ICDF(22481),
            AOM_ICDF(24282), AOM_ICDF(26200), AOM_ICDF(27868), AOM_ICDF(30203),
            AOM_ICDF(31085), AOM_ICDF(32761), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6784), AOM_ICDF(9119), AOM_ICDF(12629), AOM_ICDF(16877),
            AOM_ICDF(20262), AOM_ICDF(21125), AOM_ICDF(22307), AOM_ICDF(23615),
            AOM_ICDF(27727), AOM_ICDF(29972), AOM_ICDF(32768), 0 },
          { AOM_ICDF(8320), AOM_ICDF(10230), AOM_ICDF(12783), AOM_ICDF(19005),
            AOM_ICDF(20213), AOM_ICDF(20668), AOM_ICDF(22039), AOM_ICDF(23045),
            AOM_ICDF(24146), AOM_ICDF(30478), AOM_ICDF(32768), 0 },
          { AOM_ICDF(9088), AOM_ICDF(11308), AOM_ICDF(15416), AOM_ICDF(18118),
            AOM_ICDF(19762), AOM_ICDF(20906), AOM_ICDF(22574), AOM_ICDF(25162),
            AOM_ICDF(25994), AOM_ICDF(28455), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6912), AOM_ICDF(10548), AOM_ICDF(15148), AOM_ICDF(20026),
            AOM_ICDF(21612), AOM_ICDF(21618), AOM_ICDF(22707), AOM_ICDF(24200),
            AOM_ICDF(24869), AOM_ICDF(26844), AOM_ICDF(32768), 0 },
          { AOM_ICDF(6656), AOM_ICDF(12164), AOM_ICDF(16993), AOM_ICDF(21568),
            AOM_ICDF(22933), AOM_ICDF(23648), AOM_ICDF(25322), AOM_ICDF(26602),
            AOM_ICDF(27806), AOM_ICDF(29841), AOM_ICDF(32768), 0 },
      },
#endif  // CONFIG_SMOOTH_HV
#else   // CONFIG_ALT_INTRA
      { { AOM_ICDF(17536), AOM_ICDF(19321), AOM_ICDF(21527), AOM_ICDF(25360),
          AOM_ICDF(27516), AOM_ICDF(28026), AOM_ICDF(29323), AOM_ICDF(30023),
          AOM_ICDF(30999), AOM_ICDF(32768), 0 },
        { AOM_ICDF(11776), AOM_ICDF(15466), AOM_ICDF(22360), AOM_ICDF(24865),
          AOM_ICDF(26991), AOM_ICDF(27889), AOM_ICDF(29299), AOM_ICDF(30519),
          AOM_ICDF(31398), AOM_ICDF(32768), 0 },
        { AOM_ICDF(9344), AOM_ICDF(12272), AOM_ICDF(13793), AOM_ICDF(25813),
          AOM_ICDF(27359), AOM_ICDF(27654), AOM_ICDF(28573), AOM_ICDF(29130),
          AOM_ICDF(30551), AOM_ICDF(32768), 0 },
        { AOM_ICDF(11648), AOM_ICDF(14123), AOM_ICDF(16454), AOM_ICDF(19948),
          AOM_ICDF(22780), AOM_ICDF(23846), AOM_ICDF(27087), AOM_ICDF(28995),
          AOM_ICDF(30380), AOM_ICDF(32768), 0 },
        { AOM_ICDF(9216), AOM_ICDF(12436), AOM_ICDF(15295), AOM_ICDF(17996),
          AOM_ICDF(24006), AOM_ICDF(25465), AOM_ICDF(27405), AOM_ICDF(28725),
          AOM_ICDF(30383), AOM_ICDF(32768), 0 },
        { AOM_ICDF(9344), AOM_ICDF(12181), AOM_ICDF(14433), AOM_ICDF(16634),
          AOM_ICDF(20355), AOM_ICDF(24317), AOM_ICDF(26133), AOM_ICDF(29295),
          AOM_ICDF(31344), AOM_ICDF(32768), 0 },
        { AOM_ICDF(8576), AOM_ICDF(10750), AOM_ICDF(12556), AOM_ICDF(17996),
          AOM_ICDF(22315), AOM_ICDF(23609), AOM_ICDF(25040), AOM_ICDF(26157),
          AOM_ICDF(30573), AOM_ICDF(32768), 0 },
        { AOM_ICDF(11008), AOM_ICDF(13303), AOM_ICDF(15432), AOM_ICDF(20646),
          AOM_ICDF(23506), AOM_ICDF(24100), AOM_ICDF(25624), AOM_ICDF(26824),
          AOM_ICDF(28055), AOM_ICDF(32768), 0 },
        { AOM_ICDF(9472), AOM_ICDF(12384), AOM_ICDF(14534), AOM_ICDF(17094),
          AOM_ICDF(20257), AOM_ICDF(22155), AOM_ICDF(24767), AOM_ICDF(28955),
          AOM_ICDF(30474), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7552), AOM_ICDF(14152), AOM_ICDF(17352), AOM_ICDF(22654),
          AOM_ICDF(25123), AOM_ICDF(25783), AOM_ICDF(27911), AOM_ICDF(29182),
          AOM_ICDF(30849), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(8064), AOM_ICDF(11538), AOM_ICDF(21987), AOM_ICDF(24941),
          AOM_ICDF(26913), AOM_ICDF(28136), AOM_ICDF(29222), AOM_ICDF(30469),
          AOM_ICDF(31331), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5504), AOM_ICDF(10403), AOM_ICDF(25080), AOM_ICDF(26762),
          AOM_ICDF(27933), AOM_ICDF(29104), AOM_ICDF(30092), AOM_ICDF(31576),
          AOM_ICDF(32004), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5632), AOM_ICDF(8706), AOM_ICDF(15097), AOM_ICDF(23714),
          AOM_ICDF(25344), AOM_ICDF(26072), AOM_ICDF(27380), AOM_ICDF(28580),
          AOM_ICDF(29840), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7424), AOM_ICDF(11186), AOM_ICDF(17593), AOM_ICDF(20154),
          AOM_ICDF(22974), AOM_ICDF(24351), AOM_ICDF(26916), AOM_ICDF(29956),
          AOM_ICDF(30967), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5888), AOM_ICDF(10193), AOM_ICDF(16895), AOM_ICDF(19031),
          AOM_ICDF(23735), AOM_ICDF(25576), AOM_ICDF(27514), AOM_ICDF(29813),
          AOM_ICDF(30471), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4864), AOM_ICDF(8352), AOM_ICDF(16459), AOM_ICDF(18062),
          AOM_ICDF(21263), AOM_ICDF(25378), AOM_ICDF(26937), AOM_ICDF(30376),
          AOM_ICDF(31619), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4992), AOM_ICDF(7922), AOM_ICDF(13842), AOM_ICDF(18004),
          AOM_ICDF(21779), AOM_ICDF(23527), AOM_ICDF(25115), AOM_ICDF(27357),
          AOM_ICDF(30232), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6656), AOM_ICDF(9716), AOM_ICDF(16379), AOM_ICDF(20053),
          AOM_ICDF(22487), AOM_ICDF(23613), AOM_ICDF(25437), AOM_ICDF(27270),
          AOM_ICDF(28516), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6016), AOM_ICDF(9674), AOM_ICDF(16891), AOM_ICDF(18684),
          AOM_ICDF(21147), AOM_ICDF(23093), AOM_ICDF(25512), AOM_ICDF(30132),
          AOM_ICDF(30894), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4608), AOM_ICDF(11318), AOM_ICDF(21038), AOM_ICDF(23650),
          AOM_ICDF(25303), AOM_ICDF(26262), AOM_ICDF(28295), AOM_ICDF(30479),
          AOM_ICDF(31212), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(10496), AOM_ICDF(12758), AOM_ICDF(14790), AOM_ICDF(24547),
          AOM_ICDF(26342), AOM_ICDF(26799), AOM_ICDF(27825), AOM_ICDF(28443),
          AOM_ICDF(30217), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7040), AOM_ICDF(11462), AOM_ICDF(17121), AOM_ICDF(24215),
          AOM_ICDF(26504), AOM_ICDF(27267), AOM_ICDF(28492), AOM_ICDF(29444),
          AOM_ICDF(30846), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5376), AOM_ICDF(8158), AOM_ICDF(9215), AOM_ICDF(26451),
          AOM_ICDF(27407), AOM_ICDF(27524), AOM_ICDF(27995), AOM_ICDF(28275),
          AOM_ICDF(29767), AOM_ICDF(32768), 0 },
        { AOM_ICDF(8704), AOM_ICDF(12652), AOM_ICDF(14145), AOM_ICDF(20101),
          AOM_ICDF(22879), AOM_ICDF(23675), AOM_ICDF(25629), AOM_ICDF(27079),
          AOM_ICDF(28923), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7424), AOM_ICDF(12374), AOM_ICDF(14366), AOM_ICDF(18855),
          AOM_ICDF(23842), AOM_ICDF(24358), AOM_ICDF(25639), AOM_ICDF(27087),
          AOM_ICDF(29706), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6400), AOM_ICDF(10005), AOM_ICDF(12939), AOM_ICDF(17753),
          AOM_ICDF(22206), AOM_ICDF(24790), AOM_ICDF(26785), AOM_ICDF(28164),
          AOM_ICDF(30520), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5632), AOM_ICDF(8176), AOM_ICDF(9713), AOM_ICDF(19053),
          AOM_ICDF(22343), AOM_ICDF(23222), AOM_ICDF(24453), AOM_ICDF(25070),
          AOM_ICDF(29761), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7040), AOM_ICDF(9754), AOM_ICDF(10833), AOM_ICDF(21229),
          AOM_ICDF(23540), AOM_ICDF(23943), AOM_ICDF(24839), AOM_ICDF(25675),
          AOM_ICDF(27033), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6784), AOM_ICDF(11758), AOM_ICDF(13481), AOM_ICDF(17236),
          AOM_ICDF(20210), AOM_ICDF(21768), AOM_ICDF(24303), AOM_ICDF(26948),
          AOM_ICDF(28676), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4864), AOM_ICDF(12712), AOM_ICDF(14201), AOM_ICDF(23863),
          AOM_ICDF(25952), AOM_ICDF(26386), AOM_ICDF(27632), AOM_ICDF(28635),
          AOM_ICDF(30362), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(13184), AOM_ICDF(15173), AOM_ICDF(17647), AOM_ICDF(21576),
          AOM_ICDF(24474), AOM_ICDF(25267), AOM_ICDF(27699), AOM_ICDF(29283),
          AOM_ICDF(30549), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7552), AOM_ICDF(11295), AOM_ICDF(18257), AOM_ICDF(20811),
          AOM_ICDF(23213), AOM_ICDF(24606), AOM_ICDF(27731), AOM_ICDF(30407),
          AOM_ICDF(31237), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7936), AOM_ICDF(10846), AOM_ICDF(12816), AOM_ICDF(22436),
          AOM_ICDF(24614), AOM_ICDF(25130), AOM_ICDF(26890), AOM_ICDF(28199),
          AOM_ICDF(29091), AOM_ICDF(32768), 0 },
        { AOM_ICDF(8576), AOM_ICDF(11411), AOM_ICDF(13830), AOM_ICDF(15918),
          AOM_ICDF(18996), AOM_ICDF(20044), AOM_ICDF(25114), AOM_ICDF(27835),
          AOM_ICDF(28972), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7680), AOM_ICDF(10816), AOM_ICDF(13646), AOM_ICDF(15966),
          AOM_ICDF(21162), AOM_ICDF(22012), AOM_ICDF(24701), AOM_ICDF(27506),
          AOM_ICDF(29644), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6784), AOM_ICDF(9423), AOM_ICDF(12524), AOM_ICDF(14773),
          AOM_ICDF(19447), AOM_ICDF(22804), AOM_ICDF(26073), AOM_ICDF(29211),
          AOM_ICDF(30642), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6784), AOM_ICDF(8916), AOM_ICDF(11059), AOM_ICDF(15861),
          AOM_ICDF(21174), AOM_ICDF(22338), AOM_ICDF(24620), AOM_ICDF(27071),
          AOM_ICDF(30899), AOM_ICDF(32768), 0 },
        { AOM_ICDF(9856), AOM_ICDF(11557), AOM_ICDF(13960), AOM_ICDF(18525),
          AOM_ICDF(21788), AOM_ICDF(22189), AOM_ICDF(24462), AOM_ICDF(26603),
          AOM_ICDF(27470), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7808), AOM_ICDF(10636), AOM_ICDF(13143), AOM_ICDF(15844),
          AOM_ICDF(18698), AOM_ICDF(20272), AOM_ICDF(24323), AOM_ICDF(30096),
          AOM_ICDF(31787), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6016), AOM_ICDF(10928), AOM_ICDF(14596), AOM_ICDF(18926),
          AOM_ICDF(21586), AOM_ICDF(22688), AOM_ICDF(26626), AOM_ICDF(29001),
          AOM_ICDF(30399), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(8832), AOM_ICDF(10983), AOM_ICDF(13451), AOM_ICDF(16582),
          AOM_ICDF(21656), AOM_ICDF(23109), AOM_ICDF(24845), AOM_ICDF(26207),
          AOM_ICDF(28796), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6784), AOM_ICDF(10844), AOM_ICDF(15554), AOM_ICDF(18073),
          AOM_ICDF(22954), AOM_ICDF(24901), AOM_ICDF(26776), AOM_ICDF(28649),
          AOM_ICDF(30419), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5120), AOM_ICDF(8252), AOM_ICDF(10072), AOM_ICDF(20108),
          AOM_ICDF(23535), AOM_ICDF(24346), AOM_ICDF(25761), AOM_ICDF(26418),
          AOM_ICDF(28675), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7680), AOM_ICDF(11012), AOM_ICDF(12627), AOM_ICDF(14595),
          AOM_ICDF(19462), AOM_ICDF(20888), AOM_ICDF(23348), AOM_ICDF(25703),
          AOM_ICDF(28159), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6656), AOM_ICDF(9818), AOM_ICDF(11790), AOM_ICDF(13813),
          AOM_ICDF(22731), AOM_ICDF(24737), AOM_ICDF(26557), AOM_ICDF(28061),
          AOM_ICDF(29697), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5632), AOM_ICDF(8918), AOM_ICDF(11620), AOM_ICDF(13802),
          AOM_ICDF(19950), AOM_ICDF(23764), AOM_ICDF(25734), AOM_ICDF(28537),
          AOM_ICDF(31809), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4480), AOM_ICDF(6580), AOM_ICDF(7808), AOM_ICDF(12281),
          AOM_ICDF(19375), AOM_ICDF(20970), AOM_ICDF(22860), AOM_ICDF(24602),
          AOM_ICDF(29929), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7040), AOM_ICDF(9553), AOM_ICDF(11457), AOM_ICDF(15102),
          AOM_ICDF(20291), AOM_ICDF(21280), AOM_ICDF(22985), AOM_ICDF(24475),
          AOM_ICDF(26613), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6528), AOM_ICDF(10423), AOM_ICDF(12605), AOM_ICDF(14621),
          AOM_ICDF(19031), AOM_ICDF(21505), AOM_ICDF(24585), AOM_ICDF(27558),
          AOM_ICDF(29532), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6016), AOM_ICDF(11659), AOM_ICDF(14463), AOM_ICDF(18867),
          AOM_ICDF(23653), AOM_ICDF(24903), AOM_ICDF(27115), AOM_ICDF(29389),
          AOM_ICDF(31382), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(8192), AOM_ICDF(10016), AOM_ICDF(13304), AOM_ICDF(16362),
          AOM_ICDF(21107), AOM_ICDF(25165), AOM_ICDF(26620), AOM_ICDF(28901),
          AOM_ICDF(30910), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5888), AOM_ICDF(8723), AOM_ICDF(16237), AOM_ICDF(18318),
          AOM_ICDF(22002), AOM_ICDF(25923), AOM_ICDF(27394), AOM_ICDF(29934),
          AOM_ICDF(31428), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4608), AOM_ICDF(7138), AOM_ICDF(9841), AOM_ICDF(18442),
          AOM_ICDF(22447), AOM_ICDF(24618), AOM_ICDF(26337), AOM_ICDF(27945),
          AOM_ICDF(30168), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6784), AOM_ICDF(8916), AOM_ICDF(12270), AOM_ICDF(14851),
          AOM_ICDF(19886), AOM_ICDF(22759), AOM_ICDF(25105), AOM_ICDF(28368),
          AOM_ICDF(29760), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5120), AOM_ICDF(7928), AOM_ICDF(11324), AOM_ICDF(13340),
          AOM_ICDF(21205), AOM_ICDF(24224), AOM_ICDF(25926), AOM_ICDF(28518),
          AOM_ICDF(30560), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4480), AOM_ICDF(6580), AOM_ICDF(10058), AOM_ICDF(11237),
          AOM_ICDF(16807), AOM_ICDF(25937), AOM_ICDF(27218), AOM_ICDF(30015),
          AOM_ICDF(31348), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4608), AOM_ICDF(6808), AOM_ICDF(9445), AOM_ICDF(12446),
          AOM_ICDF(18461), AOM_ICDF(21835), AOM_ICDF(23244), AOM_ICDF(26109),
          AOM_ICDF(30115), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5760), AOM_ICDF(7659), AOM_ICDF(10798), AOM_ICDF(14720),
          AOM_ICDF(19157), AOM_ICDF(21955), AOM_ICDF(23645), AOM_ICDF(26460),
          AOM_ICDF(28702), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5760), AOM_ICDF(8503), AOM_ICDF(11157), AOM_ICDF(13071),
          AOM_ICDF(17594), AOM_ICDF(22047), AOM_ICDF(24099), AOM_ICDF(29077),
          AOM_ICDF(30850), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4864), AOM_ICDF(9660), AOM_ICDF(14264), AOM_ICDF(17105),
          AOM_ICDF(21528), AOM_ICDF(24094), AOM_ICDF(26025), AOM_ICDF(28580),
          AOM_ICDF(30559), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(9600), AOM_ICDF(11139), AOM_ICDF(12998), AOM_ICDF(18660),
          AOM_ICDF(22158), AOM_ICDF(23501), AOM_ICDF(24659), AOM_ICDF(25736),
          AOM_ICDF(30296), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7168), AOM_ICDF(11068), AOM_ICDF(15984), AOM_ICDF(19969),
          AOM_ICDF(23169), AOM_ICDF(24704), AOM_ICDF(26216), AOM_ICDF(27572),
          AOM_ICDF(31368), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4480), AOM_ICDF(6801), AOM_ICDF(8018), AOM_ICDF(20908),
          AOM_ICDF(23071), AOM_ICDF(23583), AOM_ICDF(24301), AOM_ICDF(25062),
          AOM_ICDF(29427), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7168), AOM_ICDF(10068), AOM_ICDF(11753), AOM_ICDF(15843),
          AOM_ICDF(19742), AOM_ICDF(21358), AOM_ICDF(23809), AOM_ICDF(26189),
          AOM_ICDF(29067), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6016), AOM_ICDF(9047), AOM_ICDF(10622), AOM_ICDF(13931),
          AOM_ICDF(22462), AOM_ICDF(23858), AOM_ICDF(25911), AOM_ICDF(27277),
          AOM_ICDF(29722), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5888), AOM_ICDF(7568), AOM_ICDF(9931), AOM_ICDF(13533),
          AOM_ICDF(18431), AOM_ICDF(22063), AOM_ICDF(23777), AOM_ICDF(26025),
          AOM_ICDF(30555), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4352), AOM_ICDF(6239), AOM_ICDF(7379), AOM_ICDF(13739),
          AOM_ICDF(16917), AOM_ICDF(18090), AOM_ICDF(18835), AOM_ICDF(19651),
          AOM_ICDF(30360), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6528), AOM_ICDF(8988), AOM_ICDF(10288), AOM_ICDF(15534),
          AOM_ICDF(19495), AOM_ICDF(20386), AOM_ICDF(21934), AOM_ICDF(23034),
          AOM_ICDF(26988), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7040), AOM_ICDF(10055), AOM_ICDF(11652), AOM_ICDF(14757),
          AOM_ICDF(19622), AOM_ICDF(21715), AOM_ICDF(23615), AOM_ICDF(26761),
          AOM_ICDF(29483), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4736), AOM_ICDF(10102), AOM_ICDF(12315), AOM_ICDF(19078),
          AOM_ICDF(21348), AOM_ICDF(22621), AOM_ICDF(24246), AOM_ICDF(26044),
          AOM_ICDF(29931), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(10496), AOM_ICDF(12410), AOM_ICDF(14955), AOM_ICDF(19891),
          AOM_ICDF(23137), AOM_ICDF(23792), AOM_ICDF(25159), AOM_ICDF(26378),
          AOM_ICDF(28125), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7936), AOM_ICDF(12204), AOM_ICDF(17104), AOM_ICDF(20191),
          AOM_ICDF(23468), AOM_ICDF(24630), AOM_ICDF(26156), AOM_ICDF(27628),
          AOM_ICDF(28913), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6016), AOM_ICDF(8629), AOM_ICDF(10232), AOM_ICDF(23591),
          AOM_ICDF(25349), AOM_ICDF(25637), AOM_ICDF(26306), AOM_ICDF(27063),
          AOM_ICDF(28980), AOM_ICDF(32768), 0 },
        { AOM_ICDF(8704), AOM_ICDF(12088), AOM_ICDF(13461), AOM_ICDF(16646),
          AOM_ICDF(20516), AOM_ICDF(21455), AOM_ICDF(24062), AOM_ICDF(26579),
          AOM_ICDF(28368), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7296), AOM_ICDF(11177), AOM_ICDF(13117), AOM_ICDF(16196),
          AOM_ICDF(23378), AOM_ICDF(24708), AOM_ICDF(26440), AOM_ICDF(27997),
          AOM_ICDF(29078), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6272), AOM_ICDF(9377), AOM_ICDF(12575), AOM_ICDF(15616),
          AOM_ICDF(20919), AOM_ICDF(23697), AOM_ICDF(26603), AOM_ICDF(27566),
          AOM_ICDF(29903), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6528), AOM_ICDF(9091), AOM_ICDF(10478), AOM_ICDF(16445),
          AOM_ICDF(21081), AOM_ICDF(22320), AOM_ICDF(23871), AOM_ICDF(25087),
          AOM_ICDF(29258), AOM_ICDF(32768), 0 },
        { AOM_ICDF(8704), AOM_ICDF(11148), AOM_ICDF(12499), AOM_ICDF(17340),
          AOM_ICDF(20656), AOM_ICDF(21288), AOM_ICDF(22588), AOM_ICDF(23701),
          AOM_ICDF(24693), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7552), AOM_ICDF(11394), AOM_ICDF(12980), AOM_ICDF(15562),
          AOM_ICDF(19942), AOM_ICDF(21792), AOM_ICDF(25093), AOM_ICDF(28211),
          AOM_ICDF(28959), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5120), AOM_ICDF(11708), AOM_ICDF(13847), AOM_ICDF(19377),
          AOM_ICDF(22421), AOM_ICDF(23160), AOM_ICDF(25449), AOM_ICDF(27136),
          AOM_ICDF(29182), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(9984), AOM_ICDF(12031), AOM_ICDF(15190), AOM_ICDF(18673),
          AOM_ICDF(21422), AOM_ICDF(22812), AOM_ICDF(25690), AOM_ICDF(29118),
          AOM_ICDF(30458), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6144), AOM_ICDF(9680), AOM_ICDF(17436), AOM_ICDF(19610),
          AOM_ICDF(21820), AOM_ICDF(23485), AOM_ICDF(26313), AOM_ICDF(30826),
          AOM_ICDF(31843), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6016), AOM_ICDF(8315), AOM_ICDF(10607), AOM_ICDF(19333),
          AOM_ICDF(21572), AOM_ICDF(22553), AOM_ICDF(25266), AOM_ICDF(27288),
          AOM_ICDF(28551), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7168), AOM_ICDF(9668), AOM_ICDF(12646), AOM_ICDF(16257),
          AOM_ICDF(19648), AOM_ICDF(20899), AOM_ICDF(25304), AOM_ICDF(30465),
          AOM_ICDF(31625), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6144), AOM_ICDF(9368), AOM_ICDF(11836), AOM_ICDF(14130),
          AOM_ICDF(19153), AOM_ICDF(21157), AOM_ICDF(24876), AOM_ICDF(28452),
          AOM_ICDF(29396), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5504), AOM_ICDF(8486), AOM_ICDF(11996), AOM_ICDF(14412),
          AOM_ICDF(17968), AOM_ICDF(21814), AOM_ICDF(24424), AOM_ICDF(30682),
          AOM_ICDF(32059), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5376), AOM_ICDF(7195), AOM_ICDF(9592), AOM_ICDF(13331),
          AOM_ICDF(17569), AOM_ICDF(19460), AOM_ICDF(22371), AOM_ICDF(25458),
          AOM_ICDF(28942), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7424), AOM_ICDF(9206), AOM_ICDF(11783), AOM_ICDF(16456),
          AOM_ICDF(19253), AOM_ICDF(20390), AOM_ICDF(23775), AOM_ICDF(27007),
          AOM_ICDF(28425), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5888), AOM_ICDF(8303), AOM_ICDF(11361), AOM_ICDF(13440),
          AOM_ICDF(15848), AOM_ICDF(17549), AOM_ICDF(21532), AOM_ICDF(29564),
          AOM_ICDF(30665), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4608), AOM_ICDF(8788), AOM_ICDF(13284), AOM_ICDF(16621),
          AOM_ICDF(18983), AOM_ICDF(20286), AOM_ICDF(24577), AOM_ICDF(28960),
          AOM_ICDF(30314), AOM_ICDF(32768), 0 } },
      { { AOM_ICDF(8320), AOM_ICDF(15005), AOM_ICDF(19168), AOM_ICDF(24282),
          AOM_ICDF(26707), AOM_ICDF(27402), AOM_ICDF(28681), AOM_ICDF(29639),
          AOM_ICDF(30629), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5632), AOM_ICDF(13900), AOM_ICDF(22376), AOM_ICDF(24867),
          AOM_ICDF(26804), AOM_ICDF(27734), AOM_ICDF(29130), AOM_ICDF(30722),
          AOM_ICDF(31465), AOM_ICDF(32768), 0 },
        { AOM_ICDF(4992), AOM_ICDF(9115), AOM_ICDF(11055), AOM_ICDF(24893),
          AOM_ICDF(26316), AOM_ICDF(26661), AOM_ICDF(27663), AOM_ICDF(28301),
          AOM_ICDF(29418), AOM_ICDF(32768), 0 },
        { AOM_ICDF(7424), AOM_ICDF(12077), AOM_ICDF(14987), AOM_ICDF(19596),
          AOM_ICDF(22615), AOM_ICDF(23600), AOM_ICDF(26465), AOM_ICDF(28484),
          AOM_ICDF(29789), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6272), AOM_ICDF(11447), AOM_ICDF(14362), AOM_ICDF(18204),
          AOM_ICDF(23418), AOM_ICDF(24715), AOM_ICDF(26697), AOM_ICDF(28547),
          AOM_ICDF(29520), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5248), AOM_ICDF(10946), AOM_ICDF(15379), AOM_ICDF(18167),
          AOM_ICDF(22197), AOM_ICDF(25432), AOM_ICDF(27295), AOM_ICDF(30031),
          AOM_ICDF(30576), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5120), AOM_ICDF(9008), AOM_ICDF(11607), AOM_ICDF(18210),
          AOM_ICDF(22327), AOM_ICDF(23427), AOM_ICDF(24887), AOM_ICDF(26580),
          AOM_ICDF(29892), AOM_ICDF(32768), 0 },
        { AOM_ICDF(6656), AOM_ICDF(10124), AOM_ICDF(12689), AOM_ICDF(19922),
          AOM_ICDF(22480), AOM_ICDF(22807), AOM_ICDF(24441), AOM_ICDF(25579),
          AOM_ICDF(26787), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5376), AOM_ICDF(10084), AOM_ICDF(13983), AOM_ICDF(17113),
          AOM_ICDF(19996), AOM_ICDF(21614), AOM_ICDF(24403), AOM_ICDF(28651),
          AOM_ICDF(29938), AOM_ICDF(32768), 0 },
        { AOM_ICDF(5504), AOM_ICDF(14131), AOM_ICDF(17989), AOM_ICDF(23324),
          AOM_ICDF(25513), AOM_ICDF(26071), AOM_ICDF(27850), AOM_ICDF(29464),
          AOM_ICDF(30393), AOM_ICDF(32768), 0 } },
#endif  // CONFIG_ALT_INTRA
    };

static void init_mode_probs(FRAME_CONTEXT *fc) {
  av1_copy(fc->switchable_interp_prob, default_switchable_interp_prob);
  av1_copy(fc->partition_prob, default_partition_probs);
  av1_copy(fc->intra_inter_prob, default_intra_inter_p);
  av1_copy(fc->comp_inter_prob, default_comp_inter_p);
#if CONFIG_PALETTE
  av1_copy(fc->palette_y_size_cdf, default_palette_y_size_cdf);
  av1_copy(fc->palette_uv_size_cdf, default_palette_uv_size_cdf);
  av1_copy(fc->palette_y_color_index_cdf, default_palette_y_color_index_cdf);
  av1_copy(fc->palette_uv_color_index_cdf, default_palette_uv_color_index_cdf);
#endif  // CONFIG_PALETTE
#if CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->comp_inter_cdf, default_comp_inter_cdf);
#endif  // CONFIG_NEW_MULTISYMBOL
#if CONFIG_EXT_COMP_REFS
  av1_copy(fc->comp_ref_type_prob, default_comp_ref_type_p);
  av1_copy(fc->uni_comp_ref_prob, default_uni_comp_ref_p);
#if CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->comp_ref_type_cdf, default_comp_ref_type_cdf);
  av1_copy(fc->uni_comp_ref_cdf, default_uni_comp_ref_cdf);
#endif  // CONFIG_NEW_MULTISYMBOL
#endif  // CONFIG_EXT_COMP_REFS
  av1_copy(fc->comp_ref_prob, default_comp_ref_p);
#if CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->comp_ref_cdf, default_comp_ref_cdf);
#endif
#if CONFIG_LV_MAP
  av1_copy(fc->txb_skip, default_txb_skip);
  av1_copy(fc->nz_map, default_nz_map);
  av1_copy(fc->eob_flag, default_eob_flag);
  av1_copy(fc->dc_sign, default_dc_sign);
  av1_copy(fc->coeff_base, default_coeff_base);
  av1_copy(fc->coeff_lps, default_coeff_lps);
#endif
#if CONFIG_EXT_REFS
  av1_copy(fc->comp_bwdref_prob, default_comp_bwdref_p);
#if CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->comp_bwdref_cdf, default_comp_bwdref_cdf);
#endif
#endif  // CONFIG_EXT_REFS
  av1_copy(fc->single_ref_prob, default_single_ref_p);
#if CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->single_ref_cdf, default_single_ref_cdf);
#endif
#if CONFIG_EXT_INTER && CONFIG_COMPOUND_SINGLEREF
  av1_copy(fc->comp_inter_mode_prob, default_comp_inter_mode_p);
#endif  // CONFIG_EXT_INTER && CONFIG_COMPOUND_SINGLEREF
  av1_copy(fc->tx_size_probs, default_tx_size_prob);
#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
  fc->quarter_tx_size_prob = default_quarter_tx_size_prob;
#endif
#if CONFIG_VAR_TX
  av1_copy(fc->txfm_partition_prob, default_txfm_partition_probs);
#if CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->txfm_partition_cdf, default_txfm_partition_cdf);
#endif
#endif
  av1_copy(fc->skip_probs, default_skip_probs);
  av1_copy(fc->newmv_prob, default_newmv_prob);
  av1_copy(fc->zeromv_prob, default_zeromv_prob);
  av1_copy(fc->refmv_prob, default_refmv_prob);
  av1_copy(fc->drl_prob, default_drl_prob);
#if CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->newmv_cdf, default_newmv_cdf);
  av1_copy(fc->zeromv_cdf, default_zeromv_cdf);
  av1_copy(fc->refmv_cdf, default_refmv_cdf);
  av1_copy(fc->drl_cdf, default_drl_cdf);
#endif
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  av1_copy(fc->motion_mode_prob, default_motion_mode_prob);
  av1_copy(fc->motion_mode_cdf, default_motion_mode_cdf);
#if CONFIG_NCOBMC_ADAPT_WEIGHT && CONFIG_MOTION_VAR
  av1_copy(fc->ncobmc_mode_prob, default_ncobmc_mode_prob);
  av1_copy(fc->ncobmc_mode_cdf, default_ncobmc_mode_cdf);
#endif
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
  av1_copy(fc->obmc_prob, default_obmc_prob);
#if CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->obmc_cdf, default_obmc_cdf);
#endif
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
#if CONFIG_EXT_INTER
  av1_copy(fc->inter_compound_mode_probs, default_inter_compound_mode_probs);
  av1_copy(fc->inter_compound_mode_cdf, default_inter_compound_mode_cdf);
#if CONFIG_COMPOUND_SINGLEREF
  av1_copy(fc->inter_singleref_comp_mode_probs,
           default_inter_singleref_comp_mode_probs);
  av1_copy(fc->inter_singleref_comp_mode_cdf,
           default_inter_singleref_comp_mode_cdf);
#endif  // CONFIG_COMPOUND_SINGLEREF
  av1_copy(fc->compound_type_prob, default_compound_type_probs);
  av1_copy(fc->compound_type_cdf, default_compound_type_cdf);
#if CONFIG_INTERINTRA
  av1_copy(fc->interintra_prob, default_interintra_prob);
  av1_copy(fc->wedge_interintra_prob, default_wedge_interintra_prob);
#if CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->interintra_cdf, default_interintra_cdf);
  av1_copy(fc->wedge_interintra_cdf, default_wedge_interintra_cdf);
#endif  // CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->interintra_mode_prob, default_interintra_mode_prob);
  av1_copy(fc->interintra_mode_cdf, default_interintra_mode_cdf);
#endif  // CONFIG_INTERINTRA
#endif  // CONFIG_EXT_INTER
#if CONFIG_SUPERTX
  av1_copy(fc->supertx_prob, default_supertx_prob);
#endif  // CONFIG_SUPERTX
  av1_copy(fc->seg.tree_probs, default_segment_tree_probs);
  av1_copy(fc->seg.pred_probs, default_segment_pred_probs);
#if CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->seg.pred_cdf, default_segment_pred_cdf);
#endif
#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
  av1_copy(fc->intra_filter_probs, default_intra_filter_probs);
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  av1_copy(fc->filter_intra_probs, default_filter_intra_probs);
#endif  // CONFIG_FILTER_INTRA
  av1_copy(fc->inter_ext_tx_prob, default_inter_ext_tx_prob);
  av1_copy(fc->intra_ext_tx_prob, default_intra_ext_tx_prob);
#if CONFIG_LOOP_RESTORATION
  av1_copy(fc->switchable_restore_prob, default_switchable_restore_prob);
#endif  // CONFIG_LOOP_RESTORATION
  av1_copy(fc->y_mode_cdf, default_if_y_mode_cdf);
  av1_copy(fc->uv_mode_cdf, default_uv_mode_cdf);
  av1_copy(fc->switchable_interp_cdf, default_switchable_interp_cdf);
  av1_copy(fc->partition_cdf, default_partition_cdf);
  av1_copy(fc->intra_ext_tx_cdf, default_intra_ext_tx_cdf);
  av1_copy(fc->inter_ext_tx_cdf, default_inter_ext_tx_cdf);
#if CONFIG_NEW_MULTISYMBOL
  av1_copy(fc->skip_cdfs, default_skip_cdfs);
  av1_copy(fc->intra_inter_cdf, default_intra_inter_cdf);
#endif
#if CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP
  av1_copy(fc->intra_filter_cdf, default_intra_filter_cdf);
#endif  // CONFIG_EXT_INTRA && CONFIG_INTRA_INTERP
  av1_copy(fc->seg.tree_cdf, default_seg_tree_cdf);
  av1_copy(fc->tx_size_cdf, default_tx_size_cdf);
#if CONFIG_DELTA_Q
  av1_copy(fc->delta_q_prob, default_delta_q_probs);
  av1_copy(fc->delta_q_cdf, default_delta_q_cdf);
#if CONFIG_EXT_DELTA_Q
  av1_copy(fc->delta_lf_prob, default_delta_lf_probs);
  av1_copy(fc->delta_lf_cdf, default_delta_lf_cdf);
#endif
#endif  // CONFIG_DELTA_Q
#if CONFIG_CFL
  av1_copy(fc->cfl_alpha_cdf, default_cfl_alpha_cdf);
#endif
#if CONFIG_INTRABC
  fc->intrabc_prob = INTRABC_PROB_DEFAULT;
#endif
}

int av1_switchable_interp_ind[SWITCHABLE_FILTERS];
int av1_switchable_interp_inv[SWITCHABLE_FILTERS];

#if CONFIG_DUAL_FILTER && USE_EXTRA_FILTER
const aom_tree_index av1_switchable_interp_tree[TREE_SIZE(SWITCHABLE_FILTERS)] =
    {
      -EIGHTTAP_REGULAR, 2, 4, -MULTITAP_SHARP, -EIGHTTAP_SMOOTH,
      -EIGHTTAP_SMOOTH2,
    };
#else
const aom_tree_index av1_switchable_interp_tree[TREE_SIZE(SWITCHABLE_FILTERS)] =
    { -EIGHTTAP_REGULAR, 2, -EIGHTTAP_SMOOTH, -MULTITAP_SHARP };
#endif  // CONFIG_DUAL_FILTER

void av1_adapt_inter_frame_probs(AV1_COMMON *cm) {
  int i, j;
  FRAME_CONTEXT *fc = cm->fc;
  const FRAME_CONTEXT *pre_fc = cm->pre_fc;
  const FRAME_COUNTS *counts = &cm->counts;

  for (i = 0; i < INTRA_INTER_CONTEXTS; i++)
    fc->intra_inter_prob[i] = av1_mode_mv_merge_probs(
        pre_fc->intra_inter_prob[i], counts->intra_inter[i]);

  for (i = 0; i < COMP_INTER_CONTEXTS; i++)
    fc->comp_inter_prob[i] = av1_mode_mv_merge_probs(pre_fc->comp_inter_prob[i],
                                                     counts->comp_inter[i]);

#if CONFIG_EXT_COMP_REFS
  for (i = 0; i < COMP_REF_TYPE_CONTEXTS; i++)
    fc->comp_ref_type_prob[i] = av1_mode_mv_merge_probs(
        pre_fc->comp_ref_type_prob[i], counts->comp_ref_type[i]);

  for (i = 0; i < UNI_COMP_REF_CONTEXTS; i++)
    for (j = 0; j < (UNIDIR_COMP_REFS - 1); j++)
      fc->uni_comp_ref_prob[i][j] = av1_mode_mv_merge_probs(
          pre_fc->uni_comp_ref_prob[i][j], counts->uni_comp_ref[i][j]);
#endif  // CONFIG_EXT_COMP_REFS

#if CONFIG_EXT_REFS
  for (i = 0; i < REF_CONTEXTS; i++)
    for (j = 0; j < (FWD_REFS - 1); j++)
      fc->comp_ref_prob[i][j] = mode_mv_merge_probs(pre_fc->comp_ref_prob[i][j],
                                                    counts->comp_ref[i][j]);
  for (i = 0; i < REF_CONTEXTS; i++)
    for (j = 0; j < (BWD_REFS - 1); j++)
      fc->comp_bwdref_prob[i][j] = mode_mv_merge_probs(
          pre_fc->comp_bwdref_prob[i][j], counts->comp_bwdref[i][j]);
#else
  for (i = 0; i < REF_CONTEXTS; i++)
    for (j = 0; j < (COMP_REFS - 1); j++)
      fc->comp_ref_prob[i][j] = mode_mv_merge_probs(pre_fc->comp_ref_prob[i][j],
                                                    counts->comp_ref[i][j]);
#endif  // CONFIG_EXT_REFS

  for (i = 0; i < REF_CONTEXTS; i++)
    for (j = 0; j < (SINGLE_REFS - 1); j++)
      fc->single_ref_prob[i][j] = av1_mode_mv_merge_probs(
          pre_fc->single_ref_prob[i][j], counts->single_ref[i][j]);

#if CONFIG_EXT_INTER && CONFIG_COMPOUND_SINGLEREF
  for (i = 0; i < COMP_INTER_MODE_CONTEXTS; i++)
    fc->comp_inter_mode_prob[i] = av1_mode_mv_merge_probs(
        pre_fc->comp_inter_mode_prob[i], counts->comp_inter_mode[i]);

#endif  // CONFIG_EXT_INTER && CONFIG_COMPOUND_SINGLEREF

  for (i = 0; i < NEWMV_MODE_CONTEXTS; ++i)
    fc->newmv_prob[i] =
        av1_mode_mv_merge_probs(pre_fc->newmv_prob[i], counts->newmv_mode[i]);
  for (i = 0; i < ZEROMV_MODE_CONTEXTS; ++i)
    fc->zeromv_prob[i] =
        av1_mode_mv_merge_probs(pre_fc->zeromv_prob[i], counts->zeromv_mode[i]);
  for (i = 0; i < REFMV_MODE_CONTEXTS; ++i)
    fc->refmv_prob[i] =
        av1_mode_mv_merge_probs(pre_fc->refmv_prob[i], counts->refmv_mode[i]);

  for (i = 0; i < DRL_MODE_CONTEXTS; ++i)
    fc->drl_prob[i] =
        av1_mode_mv_merge_probs(pre_fc->drl_prob[i], counts->drl_mode[i]);

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  for (i = BLOCK_8X8; i < BLOCK_SIZES_ALL; ++i)
    aom_tree_merge_probs(av1_motion_mode_tree, pre_fc->motion_mode_prob[i],
                         counts->motion_mode[i], fc->motion_mode_prob[i]);
#if CONFIG_NCOBMC_ADAPT_WEIGHT
  for (i = 0; i < ADAPT_OVERLAP_BLOCKS; ++i)
    aom_tree_merge_probs(av1_ncobmc_mode_tree, pre_fc->ncobmc_mode_prob[i],
                         counts->ncobmc_mode[i], fc->ncobmc_mode_prob[i]);
#endif
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
  for (i = BLOCK_8X8; i < BLOCK_SIZES_ALL; ++i)
    fc->obmc_prob[i] =
        av1_mode_mv_merge_probs(pre_fc->obmc_prob[i], counts->obmc[i]);
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

#if CONFIG_SUPERTX
  for (i = 0; i < PARTITION_SUPERTX_CONTEXTS; ++i) {
    for (j = TX_8X8; j < TX_SIZES; ++j) {
      fc->supertx_prob[i][j] = av1_mode_mv_merge_probs(
          pre_fc->supertx_prob[i][j], counts->supertx[i][j]);
    }
  }
#endif  // CONFIG_SUPERTX

#if CONFIG_EXT_INTER
  for (i = 0; i < INTER_MODE_CONTEXTS; i++)
    aom_tree_merge_probs(
        av1_inter_compound_mode_tree, pre_fc->inter_compound_mode_probs[i],
        counts->inter_compound_mode[i], fc->inter_compound_mode_probs[i]);
#if CONFIG_COMPOUND_SINGLEREF
  for (i = 0; i < INTER_MODE_CONTEXTS; i++)
    aom_tree_merge_probs(av1_inter_singleref_comp_mode_tree,
                         pre_fc->inter_singleref_comp_mode_probs[i],
                         counts->inter_singleref_comp_mode[i],
                         fc->inter_singleref_comp_mode_probs[i]);
#endif  // CONFIG_COMPOUND_SINGLEREF
#if CONFIG_INTERINTRA
  if (cm->allow_interintra_compound) {
    for (i = 0; i < BLOCK_SIZE_GROUPS; ++i) {
      if (is_interintra_allowed_bsize_group(i))
        fc->interintra_prob[i] = av1_mode_mv_merge_probs(
            pre_fc->interintra_prob[i], counts->interintra[i]);
    }
    for (i = 0; i < BLOCK_SIZE_GROUPS; i++) {
      aom_tree_merge_probs(
          av1_interintra_mode_tree, pre_fc->interintra_mode_prob[i],
          counts->interintra_mode[i], fc->interintra_mode_prob[i]);
    }
#if CONFIG_WEDGE
    for (i = 0; i < BLOCK_SIZES_ALL; ++i) {
      if (is_interintra_allowed_bsize(i) && is_interintra_wedge_used(i))
        fc->wedge_interintra_prob[i] = av1_mode_mv_merge_probs(
            pre_fc->wedge_interintra_prob[i], counts->wedge_interintra[i]);
    }
#endif  // CONFIG_WEDGE
  }
#endif  // CONFIG_INTERINTRA

#if CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
  if (cm->allow_masked_compound) {
    for (i = 0; i < BLOCK_SIZES_ALL; ++i) {
      aom_tree_merge_probs(
          av1_compound_type_tree, pre_fc->compound_type_prob[i],
          counts->compound_interinter[i], fc->compound_type_prob[i]);
    }
  }
#endif  // CONFIG_COMPOUND_SEGMENT || CONFIG_WEDGE
#endif  // CONFIG_EXT_INTER

  if (cm->interp_filter == SWITCHABLE) {
    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
      aom_tree_merge_probs(
          av1_switchable_interp_tree, pre_fc->switchable_interp_prob[i],
          counts->switchable_interp[i], fc->switchable_interp_prob[i]);
  }
}

void av1_adapt_intra_frame_probs(AV1_COMMON *cm) {
  int i, j;
  FRAME_CONTEXT *fc = cm->fc;
  const FRAME_CONTEXT *pre_fc = cm->pre_fc;
  const FRAME_COUNTS *counts = &cm->counts;

  if (cm->tx_mode == TX_MODE_SELECT) {
    for (i = 0; i < MAX_TX_DEPTH; ++i) {
      for (j = 0; j < TX_SIZE_CONTEXTS; ++j)
        aom_tree_merge_probs(av1_tx_size_tree[i], pre_fc->tx_size_probs[i][j],
                             counts->tx_size[i][j], fc->tx_size_probs[i][j]);
    }
#if CONFIG_RECT_TX_EXT && (CONFIG_EXT_TX || CONFIG_VAR_TX)
    fc->quarter_tx_size_prob = av1_mode_mv_merge_probs(
        pre_fc->quarter_tx_size_prob, counts->quarter_tx_size);
#endif
  }

#if CONFIG_VAR_TX
  if (cm->tx_mode == TX_MODE_SELECT) {
    for (i = 0; i < TXFM_PARTITION_CONTEXTS; ++i)
      fc->txfm_partition_prob[i] = av1_mode_mv_merge_probs(
          pre_fc->txfm_partition_prob[i], counts->txfm_partition[i]);
  }
#endif

  for (i = 0; i < SKIP_CONTEXTS; ++i)
    fc->skip_probs[i] =
        av1_mode_mv_merge_probs(pre_fc->skip_probs[i], counts->skip[i]);

#if CONFIG_EXT_TX
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    int s;
    for (s = 1; s < EXT_TX_SETS_INTER; ++s) {
      if (use_inter_ext_tx_for_txsize[s][i]) {
        aom_tree_merge_probs(
            av1_ext_tx_inter_tree[s], pre_fc->inter_ext_tx_prob[s][i],
            counts->inter_ext_tx[s][i], fc->inter_ext_tx_prob[s][i]);
      }
    }
    for (s = 1; s < EXT_TX_SETS_INTRA; ++s) {
      if (use_intra_ext_tx_for_txsize[s][i]) {
        for (j = 0; j < INTRA_MODES; ++j)
          aom_tree_merge_probs(
              av1_ext_tx_intra_tree[s], pre_fc->intra_ext_tx_prob[s][i][j],
              counts->intra_ext_tx[s][i][j], fc->intra_ext_tx_prob[s][i][j]);
      }
    }
  }
#else
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    for (j = 0; j < TX_TYPES; ++j) {
      aom_tree_merge_probs(av1_ext_tx_tree, pre_fc->intra_ext_tx_prob[i][j],
                           counts->intra_ext_tx[i][j],
                           fc->intra_ext_tx_prob[i][j]);
    }
  }
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    aom_tree_merge_probs(av1_ext_tx_tree, pre_fc->inter_ext_tx_prob[i],
                         counts->inter_ext_tx[i], fc->inter_ext_tx_prob[i]);
  }
#endif  // CONFIG_EXT_TX

  if (cm->seg.temporal_update) {
    for (i = 0; i < PREDICTION_PROBS; i++)
      fc->seg.pred_probs[i] = av1_mode_mv_merge_probs(pre_fc->seg.pred_probs[i],
                                                      counts->seg.pred[i]);

    aom_tree_merge_probs(av1_segment_tree, pre_fc->seg.tree_probs,
                         counts->seg.tree_mispred, fc->seg.tree_probs);
  } else {
    aom_tree_merge_probs(av1_segment_tree, pre_fc->seg.tree_probs,
                         counts->seg.tree_total, fc->seg.tree_probs);
  }

#if CONFIG_EXT_PARTITION_TYPES
  for (i = 0; i < PARTITION_PLOFFSET; ++i)
    aom_tree_merge_probs(av1_partition_tree, pre_fc->partition_prob[i],
                         counts->partition[i], fc->partition_prob[i]);
  for (; i < PARTITION_CONTEXTS_PRIMARY; ++i)
    aom_tree_merge_probs(av1_ext_partition_tree, pre_fc->partition_prob[i],
                         counts->partition[i], fc->partition_prob[i]);
#else
  for (i = 0; i < PARTITION_CONTEXTS_PRIMARY; ++i) {
    aom_tree_merge_probs(av1_partition_tree, pre_fc->partition_prob[i],
                         counts->partition[i], fc->partition_prob[i]);
  }
#endif  // CONFIG_EXT_PARTITION_TYPES
#if CONFIG_UNPOISON_PARTITION_CTX
  for (i = PARTITION_CONTEXTS_PRIMARY;
       i < PARTITION_CONTEXTS_PRIMARY + PARTITION_BLOCK_SIZES; ++i) {
    unsigned int ct[2] = { counts->partition[i][PARTITION_VERT],
                           counts->partition[i][PARTITION_SPLIT] };
    assert(counts->partition[i][PARTITION_NONE] == 0);
    assert(counts->partition[i][PARTITION_HORZ] == 0);
    assert(fc->partition_prob[i][PARTITION_NONE] == 0);
    assert(fc->partition_prob[i][PARTITION_HORZ] == 0);
    fc->partition_prob[i][PARTITION_VERT] =
        av1_mode_mv_merge_probs(pre_fc->partition_prob[i][PARTITION_VERT], ct);
  }
  for (i = PARTITION_CONTEXTS_PRIMARY + PARTITION_BLOCK_SIZES;
       i < PARTITION_CONTEXTS_PRIMARY + 2 * PARTITION_BLOCK_SIZES; ++i) {
    unsigned int ct[2] = { counts->partition[i][PARTITION_HORZ],
                           counts->partition[i][PARTITION_SPLIT] };
    assert(counts->partition[i][PARTITION_NONE] == 0);
    assert(counts->partition[i][PARTITION_VERT] == 0);
    assert(fc->partition_prob[i][PARTITION_NONE] == 0);
    assert(fc->partition_prob[i][PARTITION_VERT] == 0);
    fc->partition_prob[i][PARTITION_HORZ] =
        av1_mode_mv_merge_probs(pre_fc->partition_prob[i][PARTITION_HORZ], ct);
  }
#endif
#if CONFIG_DELTA_Q
  for (i = 0; i < DELTA_Q_PROBS; ++i)
    fc->delta_q_prob[i] =
        mode_mv_merge_probs(pre_fc->delta_q_prob[i], counts->delta_q[i]);
#if CONFIG_EXT_DELTA_Q
  for (i = 0; i < DELTA_LF_PROBS; ++i)
    fc->delta_lf_prob[i] =
        mode_mv_merge_probs(pre_fc->delta_lf_prob[i], counts->delta_lf[i]);
#endif  // CONFIG_EXT_DELTA_Q
#endif
#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
  for (i = 0; i < INTRA_FILTERS + 1; ++i) {
    aom_tree_merge_probs(av1_intra_filter_tree, pre_fc->intra_filter_probs[i],
                         counts->intra_filter[i], fc->intra_filter_probs[i]);
  }
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  for (i = 0; i < PLANE_TYPES; ++i) {
    fc->filter_intra_probs[i] = av1_mode_mv_merge_probs(
        pre_fc->filter_intra_probs[i], counts->filter_intra[i]);
  }
#endif  // CONFIG_FILTER_INTRA
}

static void set_default_lf_deltas(struct loopfilter *lf) {
  lf->mode_ref_delta_enabled = 1;
  lf->mode_ref_delta_update = 1;

  lf->ref_deltas[INTRA_FRAME] = 1;
  lf->ref_deltas[LAST_FRAME] = 0;
#if CONFIG_EXT_REFS
  lf->ref_deltas[LAST2_FRAME] = lf->ref_deltas[LAST_FRAME];
  lf->ref_deltas[LAST3_FRAME] = lf->ref_deltas[LAST_FRAME];
  lf->ref_deltas[BWDREF_FRAME] = lf->ref_deltas[LAST_FRAME];
#endif  // CONFIG_EXT_REFS
  lf->ref_deltas[GOLDEN_FRAME] = -1;
#if CONFIG_ALTREF2
  lf->ref_deltas[ALTREF2_FRAME] = -1;
#endif  // CONFIG_ALTREF2
  lf->ref_deltas[ALTREF_FRAME] = -1;

  lf->mode_deltas[0] = 0;
  lf->mode_deltas[1] = 0;
}

void av1_setup_past_independence(AV1_COMMON *cm) {
  // Reset the segment feature data to the default stats:
  // Features disabled, 0, with delta coding (Default state).
  struct loopfilter *const lf = &cm->lf;

  int i;
  av1_clearall_segfeatures(&cm->seg);
  cm->seg.abs_delta = SEGMENT_DELTADATA;

  if (cm->last_frame_seg_map && !cm->frame_parallel_decode)
    memset(cm->last_frame_seg_map, 0, (cm->mi_rows * cm->mi_cols));

  if (cm->current_frame_seg_map)
    memset(cm->current_frame_seg_map, 0, (cm->mi_rows * cm->mi_cols));

  // Reset the mode ref deltas for loop filter
  av1_zero(lf->last_ref_deltas);
  av1_zero(lf->last_mode_deltas);
  set_default_lf_deltas(lf);

  // To force update of the sharpness
  lf->last_sharpness_level = -1;

  av1_default_coef_probs(cm);
  init_mode_probs(cm->fc);
  av1_init_mv_probs(cm);
#if CONFIG_PVQ
  av1_default_pvq_probs(cm);
#endif  // CONFIG_PVQ
#if CONFIG_ADAPT_SCAN
  av1_init_scan_order(cm);
#endif
  av1_convolve_init(cm);
  cm->fc->initialized = 1;

  if (cm->frame_type == KEY_FRAME || cm->error_resilient_mode ||
      cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL) {
    // Reset all frame contexts.
    for (i = 0; i < FRAME_CONTEXTS; ++i) cm->frame_contexts[i] = *cm->fc;
  } else if (cm->reset_frame_context == RESET_FRAME_CONTEXT_CURRENT) {
    // Reset only the frame context specified in the frame header.
    cm->frame_contexts[cm->frame_context_idx] = *cm->fc;
  }

  // prev_mip will only be allocated in encoder.
  if (frame_is_intra_only(cm) && cm->prev_mip && !cm->frame_parallel_decode)
    memset(cm->prev_mip, 0,
           cm->mi_stride * (cm->mi_rows + 1) * sizeof(*cm->prev_mip));

  cm->frame_context_idx = 0;
}
