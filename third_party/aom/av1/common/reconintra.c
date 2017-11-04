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

#include <math.h>

#include "./av1_rtcd.h"
#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "aom_ports/system_state.h"

#if CONFIG_HIGHBITDEPTH
#include "aom_dsp/aom_dsp_common.h"
#endif  // CONFIG_HIGHBITDEPTH
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_ports/aom_once.h"
#include "av1/common/reconintra.h"
#include "av1/common/onyxc_int.h"
#if CONFIG_CFL
#include "av1/common/cfl.h"
#endif

enum {
  NEED_LEFT = 1 << 1,
  NEED_ABOVE = 1 << 2,
  NEED_ABOVERIGHT = 1 << 3,
  NEED_ABOVELEFT = 1 << 4,
  NEED_BOTTOMLEFT = 1 << 5,
};

#if CONFIG_INTRA_EDGE
#define INTRA_EDGE_FILT 3
#define INTRA_EDGE_TAPS 5
#if CONFIG_INTRA_EDGE_UPSAMPLE
#define MAX_UPSAMPLE_SZ 12
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
#endif  // CONFIG_INTRA_EDGE

#define INTRA_USES_EXT_TRANSFORMS 1
#define INTRA_USES_RECT_TRANSFORMS \
  (CONFIG_RECT_TX && (CONFIG_VAR_TX || CONFIG_EXT_TX))

static const uint8_t extend_modes[INTRA_MODES] = {
  NEED_ABOVE | NEED_LEFT,                   // DC
  NEED_ABOVE,                               // V
  NEED_LEFT,                                // H
  NEED_ABOVE | NEED_ABOVERIGHT,             // D45
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // D135
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // D117
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // D153
  NEED_LEFT | NEED_BOTTOMLEFT,              // D207
  NEED_ABOVE | NEED_ABOVERIGHT,             // D63
  NEED_LEFT | NEED_ABOVE,                   // SMOOTH
#if CONFIG_SMOOTH_HV
  NEED_LEFT | NEED_ABOVE,                   // SMOOTH_V
  NEED_LEFT | NEED_ABOVE,                   // SMOOTH_H
#endif                                      // CONFIG_SMOOTH_HV
  NEED_LEFT | NEED_ABOVE | NEED_ABOVELEFT,  // TM
};

static const uint16_t orders_128x128[1] = { 0 };
static const uint16_t orders_128x64[2] = { 0, 1 };
static const uint16_t orders_64x128[2] = { 0, 1 };
static const uint16_t orders_64x64[4] = {
  0, 1, 2, 3,
};
static const uint16_t orders_64x32[8] = {
  0, 2, 1, 3, 4, 6, 5, 7,
};
static const uint16_t orders_32x64[8] = {
  0, 1, 2, 3, 4, 5, 6, 7,
};
static const uint16_t orders_32x32[16] = {
  0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15,
};
static const uint16_t orders_32x16[32] = {
  0,  2,  8,  10, 1,  3,  9,  11, 4,  6,  12, 14, 5,  7,  13, 15,
  16, 18, 24, 26, 17, 19, 25, 27, 20, 22, 28, 30, 21, 23, 29, 31,
};
static const uint16_t orders_16x32[32] = {
  0,  1,  2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15,
  16, 17, 18, 19, 24, 25, 26, 27, 20, 21, 22, 23, 28, 29, 30, 31,
};
static const uint16_t orders_16x16[64] = {
  0,  1,  4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23,
  8,  9,  12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31,
  32, 33, 36, 37, 48, 49, 52, 53, 34, 35, 38, 39, 50, 51, 54, 55,
  40, 41, 44, 45, 56, 57, 60, 61, 42, 43, 46, 47, 58, 59, 62, 63,
};

static const uint16_t orders_64x16[16] = {
  0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15,
};
static const uint16_t orders_16x64[16] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};
static const uint16_t orders_32x8[64] = {
  0,  4,  16, 20, 1,  5,  17, 21, 2,  6,  18, 22, 3,  7,  19, 23,
  8,  12, 24, 28, 9,  13, 25, 29, 10, 14, 26, 30, 11, 15, 27, 31,
  32, 36, 48, 52, 33, 37, 49, 53, 34, 38, 50, 54, 35, 39, 51, 55,
  40, 44, 56, 60, 41, 45, 57, 61, 42, 46, 58, 62, 43, 47, 59, 63,
};
static const uint16_t orders_8x32[64] = {
  0,  1,  2,  3,  4,  5,  6,  7,  16, 17, 18, 19, 20, 21, 22, 23,
  8,  9,  10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31,
  32, 33, 34, 35, 36, 37, 38, 39, 48, 49, 50, 51, 52, 53, 54, 55,
  40, 41, 42, 43, 44, 45, 46, 47, 56, 57, 58, 59, 60, 61, 62, 63,
};

#if CONFIG_EXT_PARTITION
static const uint16_t orders_16x4[256] = {
  0,   4,   16,  20,  64,  68,  80,  84,  1,   5,   17,  21,  65,  69,  81,
  85,  2,   6,   18,  22,  66,  70,  82,  86,  3,   7,   19,  23,  67,  71,
  83,  87,  8,   12,  24,  28,  72,  76,  88,  92,  9,   13,  25,  29,  73,
  77,  89,  93,  10,  14,  26,  30,  74,  78,  90,  94,  11,  15,  27,  31,
  75,  79,  91,  95,  32,  36,  48,  52,  96,  100, 112, 116, 33,  37,  49,
  53,  97,  101, 113, 117, 34,  38,  50,  54,  98,  102, 114, 118, 35,  39,
  51,  55,  99,  103, 115, 119, 40,  44,  56,  60,  104, 108, 120, 124, 41,
  45,  57,  61,  105, 109, 121, 125, 42,  46,  58,  62,  106, 110, 122, 126,
  43,  47,  59,  63,  107, 111, 123, 127, 128, 132, 144, 148, 192, 196, 208,
  212, 129, 133, 145, 149, 193, 197, 209, 213, 130, 134, 146, 150, 194, 198,
  210, 214, 131, 135, 147, 151, 195, 199, 211, 215, 136, 140, 152, 156, 200,
  204, 216, 220, 137, 141, 153, 157, 201, 205, 217, 221, 138, 142, 154, 158,
  202, 206, 218, 222, 139, 143, 155, 159, 203, 207, 219, 223, 160, 164, 176,
  180, 224, 228, 240, 244, 161, 165, 177, 181, 225, 229, 241, 245, 162, 166,
  178, 182, 226, 230, 242, 246, 163, 167, 179, 183, 227, 231, 243, 247, 168,
  172, 184, 188, 232, 236, 248, 252, 169, 173, 185, 189, 233, 237, 249, 253,
  170, 174, 186, 190, 234, 238, 250, 254, 171, 175, 187, 191, 235, 239, 251,
  255,
};
static const uint16_t orders_4x16[256] = {
  0,   1,   2,   3,   4,   5,   6,   7,   16,  17,  18,  19,  20,  21,  22,
  23,  64,  65,  66,  67,  68,  69,  70,  71,  80,  81,  82,  83,  84,  85,
  86,  87,  8,   9,   10,  11,  12,  13,  14,  15,  24,  25,  26,  27,  28,
  29,  30,  31,  72,  73,  74,  75,  76,  77,  78,  79,  88,  89,  90,  91,
  92,  93,  94,  95,  32,  33,  34,  35,  36,  37,  38,  39,  48,  49,  50,
  51,  52,  53,  54,  55,  96,  97,  98,  99,  100, 101, 102, 103, 112, 113,
  114, 115, 116, 117, 118, 119, 40,  41,  42,  43,  44,  45,  46,  47,  56,
  57,  58,  59,  60,  61,  62,  63,  104, 105, 106, 107, 108, 109, 110, 111,
  120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
  135, 144, 145, 146, 147, 148, 149, 150, 151, 192, 193, 194, 195, 196, 197,
  198, 199, 208, 209, 210, 211, 212, 213, 214, 215, 136, 137, 138, 139, 140,
  141, 142, 143, 152, 153, 154, 155, 156, 157, 158, 159, 200, 201, 202, 203,
  204, 205, 206, 207, 216, 217, 218, 219, 220, 221, 222, 223, 160, 161, 162,
  163, 164, 165, 166, 167, 176, 177, 178, 179, 180, 181, 182, 183, 224, 225,
  226, 227, 228, 229, 230, 231, 240, 241, 242, 243, 244, 245, 246, 247, 168,
  169, 170, 171, 172, 173, 174, 175, 184, 185, 186, 187, 188, 189, 190, 191,
  232, 233, 234, 235, 236, 237, 238, 239, 248, 249, 250, 251, 252, 253, 254,
  255,
};
#endif

static const uint16_t orders_32x128[4] = {
  0, 1, 2, 3,
};
static const uint16_t orders_128x32[4] = {
  0, 1, 2, 3,
};

#if CONFIG_CB4X4 || CONFIG_EXT_PARTITION
static const uint16_t orders_16x8[128] = {
  0,  2,  8,  10, 32,  34,  40,  42,  1,  3,  9,  11, 33,  35,  41,  43,
  4,  6,  12, 14, 36,  38,  44,  46,  5,  7,  13, 15, 37,  39,  45,  47,
  16, 18, 24, 26, 48,  50,  56,  58,  17, 19, 25, 27, 49,  51,  57,  59,
  20, 22, 28, 30, 52,  54,  60,  62,  21, 23, 29, 31, 53,  55,  61,  63,
  64, 66, 72, 74, 96,  98,  104, 106, 65, 67, 73, 75, 97,  99,  105, 107,
  68, 70, 76, 78, 100, 102, 108, 110, 69, 71, 77, 79, 101, 103, 109, 111,
  80, 82, 88, 90, 112, 114, 120, 122, 81, 83, 89, 91, 113, 115, 121, 123,
  84, 86, 92, 94, 116, 118, 124, 126, 85, 87, 93, 95, 117, 119, 125, 127,
};
static const uint16_t orders_8x16[128] = {
  0,  1,  2,  3,  8,  9,  10, 11, 32,  33,  34,  35,  40,  41,  42,  43,
  4,  5,  6,  7,  12, 13, 14, 15, 36,  37,  38,  39,  44,  45,  46,  47,
  16, 17, 18, 19, 24, 25, 26, 27, 48,  49,  50,  51,  56,  57,  58,  59,
  20, 21, 22, 23, 28, 29, 30, 31, 52,  53,  54,  55,  60,  61,  62,  63,
  64, 65, 66, 67, 72, 73, 74, 75, 96,  97,  98,  99,  104, 105, 106, 107,
  68, 69, 70, 71, 76, 77, 78, 79, 100, 101, 102, 103, 108, 109, 110, 111,
  80, 81, 82, 83, 88, 89, 90, 91, 112, 113, 114, 115, 120, 121, 122, 123,
  84, 85, 86, 87, 92, 93, 94, 95, 116, 117, 118, 119, 124, 125, 126, 127,
};
static const uint16_t orders_8x8[256] = {
  0,   1,   4,   5,   16,  17,  20,  21,  64,  65,  68,  69,  80,  81,  84,
  85,  2,   3,   6,   7,   18,  19,  22,  23,  66,  67,  70,  71,  82,  83,
  86,  87,  8,   9,   12,  13,  24,  25,  28,  29,  72,  73,  76,  77,  88,
  89,  92,  93,  10,  11,  14,  15,  26,  27,  30,  31,  74,  75,  78,  79,
  90,  91,  94,  95,  32,  33,  36,  37,  48,  49,  52,  53,  96,  97,  100,
  101, 112, 113, 116, 117, 34,  35,  38,  39,  50,  51,  54,  55,  98,  99,
  102, 103, 114, 115, 118, 119, 40,  41,  44,  45,  56,  57,  60,  61,  104,
  105, 108, 109, 120, 121, 124, 125, 42,  43,  46,  47,  58,  59,  62,  63,
  106, 107, 110, 111, 122, 123, 126, 127, 128, 129, 132, 133, 144, 145, 148,
  149, 192, 193, 196, 197, 208, 209, 212, 213, 130, 131, 134, 135, 146, 147,
  150, 151, 194, 195, 198, 199, 210, 211, 214, 215, 136, 137, 140, 141, 152,
  153, 156, 157, 200, 201, 204, 205, 216, 217, 220, 221, 138, 139, 142, 143,
  154, 155, 158, 159, 202, 203, 206, 207, 218, 219, 222, 223, 160, 161, 164,
  165, 176, 177, 180, 181, 224, 225, 228, 229, 240, 241, 244, 245, 162, 163,
  166, 167, 178, 179, 182, 183, 226, 227, 230, 231, 242, 243, 246, 247, 168,
  169, 172, 173, 184, 185, 188, 189, 232, 233, 236, 237, 248, 249, 252, 253,
  170, 171, 174, 175, 186, 187, 190, 191, 234, 235, 238, 239, 250, 251, 254,
  255,
};

#if CONFIG_CB4X4 && CONFIG_EXT_PARTITION
static const uint16_t orders_4x8[512] = {
  0,   1,   2,   3,   8,   9,   10,  11,  32,  33,  34,  35,  40,  41,  42,
  43,  128, 129, 130, 131, 136, 137, 138, 139, 160, 161, 162, 163, 168, 169,
  170, 171, 4,   5,   6,   7,   12,  13,  14,  15,  36,  37,  38,  39,  44,
  45,  46,  47,  132, 133, 134, 135, 140, 141, 142, 143, 164, 165, 166, 167,
  172, 173, 174, 175, 16,  17,  18,  19,  24,  25,  26,  27,  48,  49,  50,
  51,  56,  57,  58,  59,  144, 145, 146, 147, 152, 153, 154, 155, 176, 177,
  178, 179, 184, 185, 186, 187, 20,  21,  22,  23,  28,  29,  30,  31,  52,
  53,  54,  55,  60,  61,  62,  63,  148, 149, 150, 151, 156, 157, 158, 159,
  180, 181, 182, 183, 188, 189, 190, 191, 64,  65,  66,  67,  72,  73,  74,
  75,  96,  97,  98,  99,  104, 105, 106, 107, 192, 193, 194, 195, 200, 201,
  202, 203, 224, 225, 226, 227, 232, 233, 234, 235, 68,  69,  70,  71,  76,
  77,  78,  79,  100, 101, 102, 103, 108, 109, 110, 111, 196, 197, 198, 199,
  204, 205, 206, 207, 228, 229, 230, 231, 236, 237, 238, 239, 80,  81,  82,
  83,  88,  89,  90,  91,  112, 113, 114, 115, 120, 121, 122, 123, 208, 209,
  210, 211, 216, 217, 218, 219, 240, 241, 242, 243, 248, 249, 250, 251, 84,
  85,  86,  87,  92,  93,  94,  95,  116, 117, 118, 119, 124, 125, 126, 127,
  212, 213, 214, 215, 220, 221, 222, 223, 244, 245, 246, 247, 252, 253, 254,
  255, 256, 257, 258, 259, 264, 265, 266, 267, 288, 289, 290, 291, 296, 297,
  298, 299, 384, 385, 386, 387, 392, 393, 394, 395, 416, 417, 418, 419, 424,
  425, 426, 427, 260, 261, 262, 263, 268, 269, 270, 271, 292, 293, 294, 295,
  300, 301, 302, 303, 388, 389, 390, 391, 396, 397, 398, 399, 420, 421, 422,
  423, 428, 429, 430, 431, 272, 273, 274, 275, 280, 281, 282, 283, 304, 305,
  306, 307, 312, 313, 314, 315, 400, 401, 402, 403, 408, 409, 410, 411, 432,
  433, 434, 435, 440, 441, 442, 443, 276, 277, 278, 279, 284, 285, 286, 287,
  308, 309, 310, 311, 316, 317, 318, 319, 404, 405, 406, 407, 412, 413, 414,
  415, 436, 437, 438, 439, 444, 445, 446, 447, 320, 321, 322, 323, 328, 329,
  330, 331, 352, 353, 354, 355, 360, 361, 362, 363, 448, 449, 450, 451, 456,
  457, 458, 459, 480, 481, 482, 483, 488, 489, 490, 491, 324, 325, 326, 327,
  332, 333, 334, 335, 356, 357, 358, 359, 364, 365, 366, 367, 452, 453, 454,
  455, 460, 461, 462, 463, 484, 485, 486, 487, 492, 493, 494, 495, 336, 337,
  338, 339, 344, 345, 346, 347, 368, 369, 370, 371, 376, 377, 378, 379, 464,
  465, 466, 467, 472, 473, 474, 475, 496, 497, 498, 499, 504, 505, 506, 507,
  340, 341, 342, 343, 348, 349, 350, 351, 372, 373, 374, 375, 380, 381, 382,
  383, 468, 469, 470, 471, 476, 477, 478, 479, 500, 501, 502, 503, 508, 509,
  510, 511,
};

static const uint16_t orders_8x4[512] = {
  0,   2,   8,   10,  32,  34,  40,  42,  128, 130, 136, 138, 160, 162, 168,
  170, 1,   3,   9,   11,  33,  35,  41,  43,  129, 131, 137, 139, 161, 163,
  169, 171, 4,   6,   12,  14,  36,  38,  44,  46,  132, 134, 140, 142, 164,
  166, 172, 174, 5,   7,   13,  15,  37,  39,  45,  47,  133, 135, 141, 143,
  165, 167, 173, 175, 16,  18,  24,  26,  48,  50,  56,  58,  144, 146, 152,
  154, 176, 178, 184, 186, 17,  19,  25,  27,  49,  51,  57,  59,  145, 147,
  153, 155, 177, 179, 185, 187, 20,  22,  28,  30,  52,  54,  60,  62,  148,
  150, 156, 158, 180, 182, 188, 190, 21,  23,  29,  31,  53,  55,  61,  63,
  149, 151, 157, 159, 181, 183, 189, 191, 64,  66,  72,  74,  96,  98,  104,
  106, 192, 194, 200, 202, 224, 226, 232, 234, 65,  67,  73,  75,  97,  99,
  105, 107, 193, 195, 201, 203, 225, 227, 233, 235, 68,  70,  76,  78,  100,
  102, 108, 110, 196, 198, 204, 206, 228, 230, 236, 238, 69,  71,  77,  79,
  101, 103, 109, 111, 197, 199, 205, 207, 229, 231, 237, 239, 80,  82,  88,
  90,  112, 114, 120, 122, 208, 210, 216, 218, 240, 242, 248, 250, 81,  83,
  89,  91,  113, 115, 121, 123, 209, 211, 217, 219, 241, 243, 249, 251, 84,
  86,  92,  94,  116, 118, 124, 126, 212, 214, 220, 222, 244, 246, 252, 254,
  85,  87,  93,  95,  117, 119, 125, 127, 213, 215, 221, 223, 245, 247, 253,
  255, 256, 258, 264, 266, 288, 290, 296, 298, 384, 386, 392, 394, 416, 418,
  424, 426, 257, 259, 265, 267, 289, 291, 297, 299, 385, 387, 393, 395, 417,
  419, 425, 427, 260, 262, 268, 270, 292, 294, 300, 302, 388, 390, 396, 398,
  420, 422, 428, 430, 261, 263, 269, 271, 293, 295, 301, 303, 389, 391, 397,
  399, 421, 423, 429, 431, 272, 274, 280, 282, 304, 306, 312, 314, 400, 402,
  408, 410, 432, 434, 440, 442, 273, 275, 281, 283, 305, 307, 313, 315, 401,
  403, 409, 411, 433, 435, 441, 443, 276, 278, 284, 286, 308, 310, 316, 318,
  404, 406, 412, 414, 436, 438, 444, 446, 277, 279, 285, 287, 309, 311, 317,
  319, 405, 407, 413, 415, 437, 439, 445, 447, 320, 322, 328, 330, 352, 354,
  360, 362, 448, 450, 456, 458, 480, 482, 488, 490, 321, 323, 329, 331, 353,
  355, 361, 363, 449, 451, 457, 459, 481, 483, 489, 491, 324, 326, 332, 334,
  356, 358, 364, 366, 452, 454, 460, 462, 484, 486, 492, 494, 325, 327, 333,
  335, 357, 359, 365, 367, 453, 455, 461, 463, 485, 487, 493, 495, 336, 338,
  344, 346, 368, 370, 376, 378, 464, 466, 472, 474, 496, 498, 504, 506, 337,
  339, 345, 347, 369, 371, 377, 379, 465, 467, 473, 475, 497, 499, 505, 507,
  340, 342, 348, 350, 372, 374, 380, 382, 468, 470, 476, 478, 500, 502, 508,
  510, 341, 343, 349, 351, 373, 375, 381, 383, 469, 471, 477, 479, 501, 503,
  509, 511,
};

static const uint16_t orders_4x4[1024] = {
  0,    1,    4,    5,    16,   17,   20,   21,   64,   65,   68,   69,   80,
  81,   84,   85,   256,  257,  260,  261,  272,  273,  276,  277,  320,  321,
  324,  325,  336,  337,  340,  341,  2,    3,    6,    7,    18,   19,   22,
  23,   66,   67,   70,   71,   82,   83,   86,   87,   258,  259,  262,  263,
  274,  275,  278,  279,  322,  323,  326,  327,  338,  339,  342,  343,  8,
  9,    12,   13,   24,   25,   28,   29,   72,   73,   76,   77,   88,   89,
  92,   93,   264,  265,  268,  269,  280,  281,  284,  285,  328,  329,  332,
  333,  344,  345,  348,  349,  10,   11,   14,   15,   26,   27,   30,   31,
  74,   75,   78,   79,   90,   91,   94,   95,   266,  267,  270,  271,  282,
  283,  286,  287,  330,  331,  334,  335,  346,  347,  350,  351,  32,   33,
  36,   37,   48,   49,   52,   53,   96,   97,   100,  101,  112,  113,  116,
  117,  288,  289,  292,  293,  304,  305,  308,  309,  352,  353,  356,  357,
  368,  369,  372,  373,  34,   35,   38,   39,   50,   51,   54,   55,   98,
  99,   102,  103,  114,  115,  118,  119,  290,  291,  294,  295,  306,  307,
  310,  311,  354,  355,  358,  359,  370,  371,  374,  375,  40,   41,   44,
  45,   56,   57,   60,   61,   104,  105,  108,  109,  120,  121,  124,  125,
  296,  297,  300,  301,  312,  313,  316,  317,  360,  361,  364,  365,  376,
  377,  380,  381,  42,   43,   46,   47,   58,   59,   62,   63,   106,  107,
  110,  111,  122,  123,  126,  127,  298,  299,  302,  303,  314,  315,  318,
  319,  362,  363,  366,  367,  378,  379,  382,  383,  128,  129,  132,  133,
  144,  145,  148,  149,  192,  193,  196,  197,  208,  209,  212,  213,  384,
  385,  388,  389,  400,  401,  404,  405,  448,  449,  452,  453,  464,  465,
  468,  469,  130,  131,  134,  135,  146,  147,  150,  151,  194,  195,  198,
  199,  210,  211,  214,  215,  386,  387,  390,  391,  402,  403,  406,  407,
  450,  451,  454,  455,  466,  467,  470,  471,  136,  137,  140,  141,  152,
  153,  156,  157,  200,  201,  204,  205,  216,  217,  220,  221,  392,  393,
  396,  397,  408,  409,  412,  413,  456,  457,  460,  461,  472,  473,  476,
  477,  138,  139,  142,  143,  154,  155,  158,  159,  202,  203,  206,  207,
  218,  219,  222,  223,  394,  395,  398,  399,  410,  411,  414,  415,  458,
  459,  462,  463,  474,  475,  478,  479,  160,  161,  164,  165,  176,  177,
  180,  181,  224,  225,  228,  229,  240,  241,  244,  245,  416,  417,  420,
  421,  432,  433,  436,  437,  480,  481,  484,  485,  496,  497,  500,  501,
  162,  163,  166,  167,  178,  179,  182,  183,  226,  227,  230,  231,  242,
  243,  246,  247,  418,  419,  422,  423,  434,  435,  438,  439,  482,  483,
  486,  487,  498,  499,  502,  503,  168,  169,  172,  173,  184,  185,  188,
  189,  232,  233,  236,  237,  248,  249,  252,  253,  424,  425,  428,  429,
  440,  441,  444,  445,  488,  489,  492,  493,  504,  505,  508,  509,  170,
  171,  174,  175,  186,  187,  190,  191,  234,  235,  238,  239,  250,  251,
  254,  255,  426,  427,  430,  431,  442,  443,  446,  447,  490,  491,  494,
  495,  506,  507,  510,  511,  512,  513,  516,  517,  528,  529,  532,  533,
  576,  577,  580,  581,  592,  593,  596,  597,  768,  769,  772,  773,  784,
  785,  788,  789,  832,  833,  836,  837,  848,  849,  852,  853,  514,  515,
  518,  519,  530,  531,  534,  535,  578,  579,  582,  583,  594,  595,  598,
  599,  770,  771,  774,  775,  786,  787,  790,  791,  834,  835,  838,  839,
  850,  851,  854,  855,  520,  521,  524,  525,  536,  537,  540,  541,  584,
  585,  588,  589,  600,  601,  604,  605,  776,  777,  780,  781,  792,  793,
  796,  797,  840,  841,  844,  845,  856,  857,  860,  861,  522,  523,  526,
  527,  538,  539,  542,  543,  586,  587,  590,  591,  602,  603,  606,  607,
  778,  779,  782,  783,  794,  795,  798,  799,  842,  843,  846,  847,  858,
  859,  862,  863,  544,  545,  548,  549,  560,  561,  564,  565,  608,  609,
  612,  613,  624,  625,  628,  629,  800,  801,  804,  805,  816,  817,  820,
  821,  864,  865,  868,  869,  880,  881,  884,  885,  546,  547,  550,  551,
  562,  563,  566,  567,  610,  611,  614,  615,  626,  627,  630,  631,  802,
  803,  806,  807,  818,  819,  822,  823,  866,  867,  870,  871,  882,  883,
  886,  887,  552,  553,  556,  557,  568,  569,  572,  573,  616,  617,  620,
  621,  632,  633,  636,  637,  808,  809,  812,  813,  824,  825,  828,  829,
  872,  873,  876,  877,  888,  889,  892,  893,  554,  555,  558,  559,  570,
  571,  574,  575,  618,  619,  622,  623,  634,  635,  638,  639,  810,  811,
  814,  815,  826,  827,  830,  831,  874,  875,  878,  879,  890,  891,  894,
  895,  640,  641,  644,  645,  656,  657,  660,  661,  704,  705,  708,  709,
  720,  721,  724,  725,  896,  897,  900,  901,  912,  913,  916,  917,  960,
  961,  964,  965,  976,  977,  980,  981,  642,  643,  646,  647,  658,  659,
  662,  663,  706,  707,  710,  711,  722,  723,  726,  727,  898,  899,  902,
  903,  914,  915,  918,  919,  962,  963,  966,  967,  978,  979,  982,  983,
  648,  649,  652,  653,  664,  665,  668,  669,  712,  713,  716,  717,  728,
  729,  732,  733,  904,  905,  908,  909,  920,  921,  924,  925,  968,  969,
  972,  973,  984,  985,  988,  989,  650,  651,  654,  655,  666,  667,  670,
  671,  714,  715,  718,  719,  730,  731,  734,  735,  906,  907,  910,  911,
  922,  923,  926,  927,  970,  971,  974,  975,  986,  987,  990,  991,  672,
  673,  676,  677,  688,  689,  692,  693,  736,  737,  740,  741,  752,  753,
  756,  757,  928,  929,  932,  933,  944,  945,  948,  949,  992,  993,  996,
  997,  1008, 1009, 1012, 1013, 674,  675,  678,  679,  690,  691,  694,  695,
  738,  739,  742,  743,  754,  755,  758,  759,  930,  931,  934,  935,  946,
  947,  950,  951,  994,  995,  998,  999,  1010, 1011, 1014, 1015, 680,  681,
  684,  685,  696,  697,  700,  701,  744,  745,  748,  749,  760,  761,  764,
  765,  936,  937,  940,  941,  952,  953,  956,  957,  1000, 1001, 1004, 1005,
  1016, 1017, 1020, 1021, 682,  683,  686,  687,  698,  699,  702,  703,  746,
  747,  750,  751,  762,  763,  766,  767,  938,  939,  942,  943,  954,  955,
  958,  959,  1002, 1003, 1006, 1007, 1018, 1019, 1022, 1023,
};
#endif
#endif  // CONFIG_CB4X4 || CONFIG_EXT_PARTITION

#if CONFIG_EXT_PARTITION
/* clang-format off */
static const uint16_t *const orders[BLOCK_SIZES_ALL] = {
#if CONFIG_CB4X4
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  // 2X2,         2X4,            4X2
  orders_4x4,     orders_4x4,     orders_4x4,
#endif
  //                              4X4
                                  orders_4x4,
  // 4X8,         8X4,            8X8
  orders_4x8,     orders_8x4,     orders_8x8,
#else  // CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  //                              4X4
                                  orders_8x8,
  // 4X8,         8X4,            8X8
  orders_8x8,     orders_8x8,     orders_8x8,
#endif
  // 8X16,        16X8,           16X16
  orders_8x16,    orders_16x8,    orders_16x16,
  // 16X32,       32X16,          32X32
  orders_16x32,   orders_32x16,   orders_32x32,
  // 32X64,       64X32,          64X64
  orders_32x64,   orders_64x32,   orders_64x64,
  // 64x128,      128x64,         128x128
  orders_64x128,  orders_128x64,  orders_128x128,
  // 4x16,        16x4,           8x32
  orders_4x16,    orders_16x4,    orders_8x32,
  // 32x8,        16x64,          64x16
  orders_32x8,    orders_16x64,   orders_64x16,
  // 32x128,      128x32
  orders_32x128,  orders_128x32
};
/* clang-format on */
#else
/* clang-format off */
static const uint16_t *const orders[BLOCK_SIZES_ALL] = {
#if CONFIG_CB4X4
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  // 2X2,         2X4,            4X2
  orders_8x8,     orders_8x8,     orders_8x8,
#endif
  //                              4X4
                                  orders_8x8,
  // 4X8,         8X4,            8X8
  orders_8x16,    orders_16x8,    orders_16x16,
#else  // CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  //                              4X4
                                  orders_16x16,
  // 4X8,         8X4,            8X8
  orders_16x16,   orders_16x16,   orders_16x16,
#endif
  // 8X16,        16X8,           16X16
  orders_16x32,   orders_32x16,   orders_32x32,
  // 16X32,       32X16,          32X32
  orders_32x64,   orders_64x32,   orders_64x64,
  // 32X64,       64X32,          64X64
  orders_64x128,  orders_128x64,  orders_128x128,
  // 4x16,        16x4,           8x32
  orders_8x32,    orders_32x8,    orders_16x64,
  // 32x8,        16x64,          64x16
  orders_64x16,   orders_32x128,  orders_128x32
};
/* clang-format on */
#endif  // CONFIG_EXT_PARTITION

#if CONFIG_EXT_PARTITION_TYPES && !CONFIG_EXT_PARTITION_TYPES_AB
static const uint16_t orders_verta_64x64[4] = {
  0, 2, 1, 2,
};
static const uint16_t orders_verta_32x32[16] = {
  0, 2, 4, 6, 1, 2, 5, 6, 8, 10, 12, 14, 9, 10, 13, 14,
};
static const uint16_t orders_verta_16x16[64] = {
  0,  2,  4,  6,  16, 18, 20, 22, 1,  2,  5,  6,  17, 18, 21, 22,
  8,  10, 12, 14, 24, 26, 28, 30, 9,  10, 13, 14, 25, 26, 29, 30,
  32, 34, 36, 38, 48, 50, 52, 54, 33, 34, 37, 38, 49, 50, 53, 54,
  40, 42, 44, 46, 56, 58, 60, 62, 41, 42, 45, 46, 57, 58, 61, 62,
};
#if CONFIG_EXT_PARTITION || CONFIG_CB4X4
static const uint16_t orders_verta_8x8[256] = {
  0,   2,   4,   6,   16,  18,  20,  22,  64,  66,  68,  70,  80,  82,  84,
  86,  1,   2,   5,   6,   17,  18,  21,  22,  65,  66,  69,  70,  81,  82,
  85,  86,  8,   10,  12,  14,  24,  26,  28,  30,  72,  74,  76,  78,  88,
  90,  92,  94,  9,   10,  13,  14,  25,  26,  29,  30,  73,  74,  77,  78,
  89,  90,  93,  94,  32,  34,  36,  38,  48,  50,  52,  54,  96,  98,  100,
  102, 112, 114, 116, 118, 33,  34,  37,  38,  49,  50,  53,  54,  97,  98,
  101, 102, 113, 114, 117, 118, 40,  42,  44,  46,  56,  58,  60,  62,  104,
  106, 108, 110, 120, 122, 124, 126, 41,  42,  45,  46,  57,  58,  61,  62,
  105, 106, 109, 110, 121, 122, 125, 126, 128, 130, 132, 134, 144, 146, 148,
  150, 192, 194, 196, 198, 208, 210, 212, 214, 129, 130, 133, 134, 145, 146,
  149, 150, 193, 194, 197, 198, 209, 210, 213, 214, 136, 138, 140, 142, 152,
  154, 156, 158, 200, 202, 204, 206, 216, 218, 220, 222, 137, 138, 141, 142,
  153, 154, 157, 158, 201, 202, 205, 206, 217, 218, 221, 222, 160, 162, 164,
  166, 176, 178, 180, 182, 224, 226, 228, 230, 240, 242, 244, 246, 161, 162,
  165, 166, 177, 178, 181, 182, 225, 226, 229, 230, 241, 242, 245, 246, 168,
  170, 172, 174, 184, 186, 188, 190, 232, 234, 236, 238, 248, 250, 252, 254,
  169, 170, 173, 174, 185, 186, 189, 190, 233, 234, 237, 238, 249, 250, 253,
  254,
};
#endif  // CONFIG_EXT_PARTITION || CONFIG_CB4X4

#if CONFIG_EXT_PARTITION
/* clang-format off */
static const uint16_t *const orders_verta[BLOCK_SIZES] = {
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  // 2X2,           2X4,              4X2
  orders_4x4,       orders_4x4,       orders_4x4,
#endif
  //                                  4X4
                                      orders_verta_8x8,
  // 4X8,           8X4,              8X8
  orders_verta_8x8, orders_verta_8x8, orders_verta_8x8,
  // 8X16,          16X8,             16X16
  orders_8x16,      orders_16x8,      orders_verta_16x16,
  // 16X32,         32X16,            32X32
  orders_16x32,     orders_32x16,     orders_verta_32x32,
  // 32X64,         64X32,            64X64
  orders_32x64,     orders_64x32,     orders_verta_64x64,
  // 64x128,        128x64,           128x128
  orders_64x128,    orders_128x64,    orders_128x128,
  // Note: We can't get 4:1 shaped blocks from a VERT_A type partition
};
/* clang-format on */
#else
/* clang-format off */
static const uint16_t *const orders_verta[BLOCK_SIZES] = {
#if CONFIG_CB4X4
#if CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  // 2X2,             2X4,                4X2
  orders_verta_8x8,   orders_verta_8x8,   orders_verta_8x8,
#endif
  //                                      4X4
                                          orders_verta_8x8,
  // 4X8,             8X4,                8X8
  orders_verta_8x8,   orders_verta_8x8,   orders_verta_16x16,
#else  // CONFIG_CHROMA_2X2 || CONFIG_CHROMA_SUB8X8
  //                                      4X4
                                          orders_verta_16x16,
  // 4X8,             8X4,                8X8
  orders_verta_16x16, orders_verta_16x16, orders_verta_16x16,
#endif
  // 8X16,            16X8,               16X16
  orders_16x32,       orders_32x16,       orders_verta_32x32,
  // 16X32,           32X16,              32X32
  orders_32x64,       orders_64x32,       orders_verta_64x64,
  // 32X64,           64X32,              64X64
  orders_64x128,      orders_128x64,      orders_128x128,
  // Note: We can't get 4:1 shaped blocks from a VERT_A type partition
};
/* clang-format on */
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_EXT_PARTITION_TYPES

static int has_top_right(const AV1_COMMON *cm, BLOCK_SIZE bsize, int mi_row,
                         int mi_col, int top_available, int right_available,
#if CONFIG_EXT_PARTITION_TYPES && !CONFIG_EXT_PARTITION_TYPES_AB
                         PARTITION_TYPE partition,
#endif  // CONFIG_EXT_PARTITION_TYPES && !CONFIG_EXT_PARTITION_TYPES_AB
                         TX_SIZE txsz, int row_off, int col_off, int ss_x) {
  if (!top_available || !right_available) return 0;

#if !CONFIG_CB4X4
  // TODO(bshacklett, huisu): Currently the RD loop traverses 4X8 blocks in
  // inverted N order while in the bitstream the subblocks are stored in Z
  // order. This discrepancy makes this function incorrect when considering 4X8
  // blocks in the RD loop, so we disable the extended right edge for these
  // blocks. The correct solution is to change the bitstream to store these
  // blocks in inverted N order, and then update this function appropriately.
  if (bsize == BLOCK_4X8 && row_off == 1) return 0;
#endif

  const int bw_unit = block_size_wide[bsize] >> tx_size_wide_log2[0];
  const int plane_bw_unit = AOMMAX(bw_unit >> ss_x, 1);
  const int top_right_count_unit = tx_size_wide_unit[txsz];

#if !CONFIG_CB4X4
  // Special handling for block sizes 4x8 and 4x4.
  if (ss_x == 0 && bw_unit < 2 && col_off == 0) return 1;
#endif

  if (row_off > 0) {  // Just need to check if enough pixels on the right.
#if CONFIG_EXT_PARTITION
    if (col_off + top_right_count_unit >=
        (block_size_wide[BLOCK_64X64] >> (tx_size_wide_log2[0] + ss_x)))
      return 0;
#endif
    return col_off + top_right_count_unit < plane_bw_unit;
  } else {
    // All top-right pixels are in the block above, which is already available.
    if (col_off + top_right_count_unit < plane_bw_unit) return 1;

    const int bw_in_mi_log2 = mi_width_log2_lookup[bsize];
    const int bh_in_mi_log2 = mi_height_log2_lookup[bsize];
    const int sb_mi_size = mi_size_high[cm->sb_size];
    const int blk_row_in_sb = (mi_row & (sb_mi_size - 1)) >> bh_in_mi_log2;
    const int blk_col_in_sb = (mi_col & (sb_mi_size - 1)) >> bw_in_mi_log2;

    // Top row of superblock: so top-right pixels are in the top and/or
    // top-right superblocks, both of which are already available.
    if (blk_row_in_sb == 0) return 1;

    // Rightmost column of superblock (and not the top row): so top-right pixels
    // fall in the right superblock, which is not available yet.
    if (((blk_col_in_sb + 1) << bw_in_mi_log2) >= sb_mi_size) return 0;

    // General case (neither top row nor rightmost column): check if the
    // top-right block is coded before the current block.
    const uint16_t *const order =
#if CONFIG_EXT_PARTITION_TYPES && !CONFIG_EXT_PARTITION_TYPES_AB
        (partition == PARTITION_VERT_A) ? orders_verta[bsize] :
#endif  // CONFIG_EXT_PARTITION_TYPES
                                        orders[bsize];
    const int this_blk_index =
        ((blk_row_in_sb + 0) << (MAX_MIB_SIZE_LOG2 - bw_in_mi_log2)) +
        blk_col_in_sb + 0;
    const uint16_t this_blk_order = order[this_blk_index];
    const int tr_blk_index =
        ((blk_row_in_sb - 1) << (MAX_MIB_SIZE_LOG2 - bw_in_mi_log2)) +
        blk_col_in_sb + 1;
    const uint16_t tr_blk_order = order[tr_blk_index];
    return tr_blk_order < this_blk_order;
  }
}

static int has_bottom_left(const AV1_COMMON *cm, BLOCK_SIZE bsize, int mi_row,
                           int mi_col, int bottom_available, int left_available,
                           TX_SIZE txsz, int row_off, int col_off, int ss_y) {
  if (!bottom_available || !left_available) return 0;

  if (col_off > 0) {
    // Bottom-left pixels are in the bottom-left block, which is not available.
    return 0;
  } else {
    const int bh_unit = block_size_high[bsize] >> tx_size_high_log2[0];
    const int plane_bh_unit = AOMMAX(bh_unit >> ss_y, 1);
    const int bottom_left_count_unit = tx_size_high_unit[txsz];

#if !CONFIG_CB4X4
    // Special handling for block sizes 8x4 and 4x4.
    if (ss_y == 0 && bh_unit < 2 && row_off == 0) return 1;
#endif

    // All bottom-left pixels are in the left block, which is already available.
    if (row_off + bottom_left_count_unit < plane_bh_unit) return 1;

    const int bw_in_mi_log2 = mi_width_log2_lookup[bsize];
    const int bh_in_mi_log2 = mi_height_log2_lookup[bsize];
    const int sb_mi_size = mi_size_high[cm->sb_size];
    const int blk_row_in_sb = (mi_row & (sb_mi_size - 1)) >> bh_in_mi_log2;
    const int blk_col_in_sb = (mi_col & (sb_mi_size - 1)) >> bw_in_mi_log2;

    // Leftmost column of superblock: so bottom-left pixels maybe in the left
    // and/or bottom-left superblocks. But only the left superblock is
    // available, so check if all required pixels fall in that superblock.
    if (blk_col_in_sb == 0) {
      const int blk_start_row_off = blk_row_in_sb
                                        << (bh_in_mi_log2 + MI_SIZE_LOG2 -
                                            tx_size_wide_log2[0]) >>
                                    ss_y;
      const int row_off_in_sb = blk_start_row_off + row_off;
      const int sb_height_unit =
          sb_mi_size << (MI_SIZE_LOG2 - tx_size_wide_log2[0]) >> ss_y;
      return row_off_in_sb + bottom_left_count_unit < sb_height_unit;
    }

    // Bottom row of superblock (and not the leftmost column): so bottom-left
    // pixels fall in the bottom superblock, which is not available yet.
    if (((blk_row_in_sb + 1) << bh_in_mi_log2) >= sb_mi_size) return 0;

    // General case (neither leftmost column nor bottom row): check if the
    // bottom-left block is coded before the current block.
    const uint16_t *const order = orders[bsize];
    const int this_blk_index =
        ((blk_row_in_sb + 0) << (MAX_MIB_SIZE_LOG2 - bw_in_mi_log2)) +
        blk_col_in_sb + 0;
    const uint16_t this_blk_order = order[this_blk_index];
    const int bl_blk_index =
        ((blk_row_in_sb + 1) << (MAX_MIB_SIZE_LOG2 - bw_in_mi_log2)) +
        blk_col_in_sb - 1;
    const uint16_t bl_blk_order = order[bl_blk_index];
    return bl_blk_order < this_blk_order;
  }
}

typedef void (*intra_pred_fn)(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left);

static intra_pred_fn pred[INTRA_MODES][TX_SIZES_ALL];
static intra_pred_fn dc_pred[2][2][TX_SIZES_ALL];

#if CONFIG_HIGHBITDEPTH
typedef void (*intra_high_pred_fn)(uint16_t *dst, ptrdiff_t stride,
                                   const uint16_t *above, const uint16_t *left,
                                   int bd);
static intra_high_pred_fn pred_high[INTRA_MODES][TX_SIZES_ALL];
static intra_high_pred_fn dc_pred_high[2][2][TX_SIZES_ALL];
#endif  // CONFIG_HIGHBITDEPTH

static void av1_init_intra_predictors_internal(void) {
#if CONFIG_EXT_INTRA
  assert(NELEMENTS(mode_to_angle_map) == INTRA_MODES);
#endif  // CONFIG_EXT_INTRA

#if CONFIG_TX64X64
#define INIT_RECTANGULAR(p, type)             \
  p[TX_4X8] = aom_##type##_predictor_4x8;     \
  p[TX_8X4] = aom_##type##_predictor_8x4;     \
  p[TX_8X16] = aom_##type##_predictor_8x16;   \
  p[TX_16X8] = aom_##type##_predictor_16x8;   \
  p[TX_16X32] = aom_##type##_predictor_16x32; \
  p[TX_32X16] = aom_##type##_predictor_32x16; \
  p[TX_32X64] = aom_##type##_predictor_32x64; \
  p[TX_64X32] = aom_##type##_predictor_64x32;
#else
#define INIT_RECTANGULAR(p, type)             \
  p[TX_4X8] = aom_##type##_predictor_4x8;     \
  p[TX_8X4] = aom_##type##_predictor_8x4;     \
  p[TX_8X16] = aom_##type##_predictor_8x16;   \
  p[TX_16X8] = aom_##type##_predictor_16x8;   \
  p[TX_16X32] = aom_##type##_predictor_16x32; \
  p[TX_32X16] = aom_##type##_predictor_32x16;
#endif  // CONFIG_TX64X64

#if CONFIG_TX64X64
#define INIT_NO_4X4(p, type)                  \
  p[TX_8X8] = aom_##type##_predictor_8x8;     \
  p[TX_16X16] = aom_##type##_predictor_16x16; \
  p[TX_32X32] = aom_##type##_predictor_32x32; \
  p[TX_64X64] = aom_##type##_predictor_64x64; \
  INIT_RECTANGULAR(p, type)
#else
#define INIT_NO_4X4(p, type)                  \
  p[TX_8X8] = aom_##type##_predictor_8x8;     \
  p[TX_16X16] = aom_##type##_predictor_16x16; \
  p[TX_32X32] = aom_##type##_predictor_32x32; \
  INIT_RECTANGULAR(p, type)
#endif  // CONFIG_TX64X64

#if CONFIG_CHROMA_2X2
#define INIT_ALL_SIZES(p, type)           \
  p[TX_2X2] = aom_##type##_predictor_2x2; \
  p[TX_4X4] = aom_##type##_predictor_4x4; \
  INIT_NO_4X4(p, type)
#else
#define INIT_ALL_SIZES(p, type)           \
  p[TX_4X4] = aom_##type##_predictor_4x4; \
  INIT_NO_4X4(p, type)
#endif

  INIT_ALL_SIZES(pred[V_PRED], v);
  INIT_ALL_SIZES(pred[H_PRED], h);
  INIT_ALL_SIZES(pred[D207_PRED], d207e);
  INIT_ALL_SIZES(pred[D45_PRED], d45e);
  INIT_ALL_SIZES(pred[D63_PRED], d63e);
  INIT_ALL_SIZES(pred[D117_PRED], d117);
  INIT_ALL_SIZES(pred[D135_PRED], d135);
  INIT_ALL_SIZES(pred[D153_PRED], d153);

  INIT_ALL_SIZES(pred[TM_PRED], paeth);
  INIT_ALL_SIZES(pred[SMOOTH_PRED], smooth);
#if CONFIG_SMOOTH_HV
  INIT_ALL_SIZES(pred[SMOOTH_V_PRED], smooth_v);
  INIT_ALL_SIZES(pred[SMOOTH_H_PRED], smooth_h);
#endif  // CONFIG_SMOOTH_HV

  INIT_ALL_SIZES(dc_pred[0][0], dc_128);
  INIT_ALL_SIZES(dc_pred[0][1], dc_top);
  INIT_ALL_SIZES(dc_pred[1][0], dc_left);
  INIT_ALL_SIZES(dc_pred[1][1], dc);

#if CONFIG_HIGHBITDEPTH
  INIT_ALL_SIZES(pred_high[V_PRED], highbd_v);
  INIT_ALL_SIZES(pred_high[H_PRED], highbd_h);
  INIT_ALL_SIZES(pred_high[D207_PRED], highbd_d207e);
  INIT_ALL_SIZES(pred_high[D45_PRED], highbd_d45e);
  INIT_ALL_SIZES(pred_high[D63_PRED], highbd_d63e);
  INIT_ALL_SIZES(pred_high[D117_PRED], highbd_d117);
  INIT_ALL_SIZES(pred_high[D135_PRED], highbd_d135);
  INIT_ALL_SIZES(pred_high[D153_PRED], highbd_d153);

  INIT_ALL_SIZES(pred_high[TM_PRED], highbd_paeth);
  INIT_ALL_SIZES(pred_high[SMOOTH_PRED], highbd_smooth);
#if CONFIG_SMOOTH_HV
  INIT_ALL_SIZES(pred_high[SMOOTH_V_PRED], highbd_smooth_v);
  INIT_ALL_SIZES(pred_high[SMOOTH_H_PRED], highbd_smooth_h);
#endif  // CONFIG_SMOOTH_HV

  INIT_ALL_SIZES(dc_pred_high[0][0], highbd_dc_128);
  INIT_ALL_SIZES(dc_pred_high[0][1], highbd_dc_top);
  INIT_ALL_SIZES(dc_pred_high[1][0], highbd_dc_left);
  INIT_ALL_SIZES(dc_pred_high[1][1], highbd_dc);
#endif  // CONFIG_HIGHBITDEPTH

#undef intra_pred_allsizes
}

#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
static int intra_subpel_interp(int base, int shift, const uint8_t *ref,
                               int ref_start_idx, int ref_end_idx,
                               INTRA_FILTER filter_type) {
  int val, k, idx, filter_idx = 0;
  const int16_t *filter = NULL;

  if (filter_type == INTRA_FILTER_LINEAR) {
    val = ref[base] * (256 - shift) + ref[base + 1] * shift;
    val = ROUND_POWER_OF_TWO(val, 8);
  } else {
    filter_idx = ROUND_POWER_OF_TWO(shift, 8 - SUBPEL_BITS);
    filter = av1_intra_filter_kernels[filter_type][filter_idx];

    if (filter_idx < (1 << SUBPEL_BITS)) {
      val = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) {
        idx = base + 1 - (SUBPEL_TAPS / 2) + k;
        idx = AOMMAX(AOMMIN(idx, ref_end_idx), ref_start_idx);
        val += ref[idx] * filter[k];
      }
      val = ROUND_POWER_OF_TWO(val, FILTER_BITS);
    } else {
      val = ref[base + 1];
    }
  }

  return val;
}
#endif  // CONFIG_INTRA_INTERP

// Directional prediction, zone 1: 0 < angle < 90
static void dr_prediction_z1(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                             const uint8_t *above, const uint8_t *left,
#if CONFIG_INTRA_INTERP
                             INTRA_FILTER filter_type,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                             int upsample_above,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                             int dx, int dy) {
  int r, c, x, base, shift, val;

  (void)left;
  (void)dy;
  assert(dy == 1);
  assert(dx > 0);

#if !CONFIG_INTRA_EDGE_UPSAMPLE
  const int upsample_above = 0;
#endif  // !CONFIG_INTRA_EDGE_UPSAMPLE
  const int max_base_x = ((bw + bh) - 1) << upsample_above;
  const int frac_bits = 8 - upsample_above;
  const int base_inc = 1 << upsample_above;
  x = dx;
  for (r = 0; r < bh; ++r, dst += stride, x += dx) {
    base = x >> frac_bits;
    shift = (x << upsample_above) & 0xFF;

    if (base >= max_base_x) {
      for (int i = r; i < bh; ++i) {
        memset(dst, above[max_base_x], bw * sizeof(dst[0]));
        dst += stride;
      }
      return;
    }

    for (c = 0; c < bw; ++c, base += base_inc) {
      if (base < max_base_x) {
#if CONFIG_INTRA_INTERP
        val = intra_subpel_interp(base, shift, above, 0, bw + bh - 1,
                                  filter_type);
#else   // CONFIG_INTRA_INTERP
        val = above[base] * (256 - shift) + above[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif  // CONFIG_INTRA_INTERP
        dst[c] = clip_pixel(val);
      } else {
        dst[c] = above[max_base_x];
      }
    }
  }
}

// Directional prediction, zone 2: 90 < angle < 180
static void dr_prediction_z2(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                             const uint8_t *above, const uint8_t *left,
#if CONFIG_INTRA_INTERP
                             INTRA_FILTER filter_type,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                             int upsample_above, int upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                             int dx, int dy) {
  int r, c, x, y, shift1, shift2, val, base1, base2;

  assert(dx > 0);
  assert(dy > 0);

#if !CONFIG_INTRA_EDGE_UPSAMPLE
  const int upsample_above = 0;
  const int upsample_left = 0;
#endif  // !CONFIG_INTRA_EDGE_UPSAMPLE
  const int min_base_x = -(1 << upsample_above);
  const int frac_bits_x = 8 - upsample_above;
  const int frac_bits_y = 8 - upsample_left;
  const int base_inc_x = 1 << upsample_above;
  x = -dx;
  for (r = 0; r < bh; ++r, x -= dx, dst += stride) {
    base1 = x >> frac_bits_x;
    y = (r << 8) - dy;
    for (c = 0; c < bw; ++c, base1 += base_inc_x, y -= dy) {
      if (base1 >= min_base_x) {
        shift1 = (x * (1 << upsample_above)) & 0xFF;
#if CONFIG_INTRA_INTERP
        val =
            intra_subpel_interp(base1, shift1, above, -1, bw - 1, filter_type);
#else
        val = above[base1] * (256 - shift1) + above[base1 + 1] * shift1;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif  // CONFIG_INTRA_INTERP
      } else {
        base2 = y >> frac_bits_y;
        assert(base2 >= -(1 << upsample_left));
        shift2 = (y * (1 << upsample_left)) & 0xFF;
#if CONFIG_INTRA_INTERP
        val = intra_subpel_interp(base2, shift2, left, -1, bh - 1, filter_type);
#else
        val = left[base2] * (256 - shift2) + left[base2 + 1] * shift2;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif  // CONFIG_INTRA_INTERP
      }
      dst[c] = clip_pixel(val);
    }
  }
}

// Directional prediction, zone 3: 180 < angle < 270
static void dr_prediction_z3(uint8_t *dst, ptrdiff_t stride, int bw, int bh,
                             const uint8_t *above, const uint8_t *left,
#if CONFIG_INTRA_INTERP
                             INTRA_FILTER filter_type,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                             int upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                             int dx, int dy) {
  int r, c, y, base, shift, val;

  (void)above;
  (void)dx;

  assert(dx == 1);
  assert(dy > 0);

#if !CONFIG_INTRA_EDGE_UPSAMPLE
  const int upsample_left = 0;
#endif  // !CONFIG_INTRA_EDGE_UPSAMPLE
  const int max_base_y = (bw + bh - 1) << upsample_left;
  const int frac_bits = 8 - upsample_left;
  const int base_inc = 1 << upsample_left;
  y = dy;
  for (c = 0; c < bw; ++c, y += dy) {
    base = y >> frac_bits;
    shift = (y << upsample_left) & 0xFF;

    for (r = 0; r < bh; ++r, base += base_inc) {
      if (base < max_base_y) {
#if CONFIG_INTRA_INTERP
        val =
            intra_subpel_interp(base, shift, left, 0, bw + bh - 1, filter_type);
#else   // CONFIG_INTRA_INTERP
        val = left[base] * (256 - shift) + left[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif  // CONFIG_INTRA_INTERP
        dst[r * stride + c] = clip_pixel(val);
      } else {
        for (; r < bh; ++r) dst[r * stride + c] = left[max_base_y];
        break;
      }
    }
  }
}

// Get the shift (up-scaled by 256) in X w.r.t a unit change in Y.
// If angle > 0 && angle < 90, dx = -((int)(256 / t));
// If angle > 90 && angle < 180, dx = (int)(256 / t);
// If angle > 180 && angle < 270, dx = 1;
static INLINE int get_dx(int angle) {
  if (angle > 0 && angle < 90) {
    return dr_intra_derivative[angle];
  } else if (angle > 90 && angle < 180) {
    return dr_intra_derivative[180 - angle];
  } else {
    // In this case, we are not really going to use dx. We may return any value.
    return 1;
  }
}

// Get the shift (up-scaled by 256) in Y w.r.t a unit change in X.
// If angle > 0 && angle < 90, dy = 1;
// If angle > 90 && angle < 180, dy = (int)(256 * t);
// If angle > 180 && angle < 270, dy = -((int)(256 * t));
static INLINE int get_dy(int angle) {
  if (angle > 90 && angle < 180) {
    return dr_intra_derivative[angle - 90];
  } else if (angle > 180 && angle < 270) {
    return dr_intra_derivative[270 - angle];
  } else {
    // In this case, we are not really going to use dy. We may return any value.
    return 1;
  }
}

static void dr_predictor(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                         const uint8_t *above, const uint8_t *left,
#if CONFIG_INTRA_INTERP
                         INTRA_FILTER filter_type,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                         int upsample_above, int upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                         int angle) {
  const int dx = get_dx(angle);
  const int dy = get_dy(angle);
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];
  assert(angle > 0 && angle < 270);

  if (angle > 0 && angle < 90) {
    dr_prediction_z1(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_INTERP
                     filter_type,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                     upsample_above,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                     dx, dy);
  } else if (angle > 90 && angle < 180) {
    dr_prediction_z2(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_INTERP
                     filter_type,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                     upsample_above, upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                     dx, dy);
  } else if (angle > 180 && angle < 270) {
    dr_prediction_z3(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_INTERP
                     filter_type,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                     upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                     dx, dy);
  } else if (angle == 90) {
    pred[V_PRED][tx_size](dst, stride, above, left);
  } else if (angle == 180) {
    pred[H_PRED][tx_size](dst, stride, above, left);
  }
}

#if CONFIG_HIGHBITDEPTH
#if CONFIG_INTRA_INTERP
static int highbd_intra_subpel_interp(int base, int shift, const uint16_t *ref,
                                      int ref_start_idx, int ref_end_idx,
                                      INTRA_FILTER filter_type) {
  int val, k, idx, filter_idx = 0;
  const int16_t *filter = NULL;

  if (filter_type == INTRA_FILTER_LINEAR) {
    val = ref[base] * (256 - shift) + ref[base + 1] * shift;
    val = ROUND_POWER_OF_TWO(val, 8);
  } else {
    filter_idx = ROUND_POWER_OF_TWO(shift, 8 - SUBPEL_BITS);
    filter = av1_intra_filter_kernels[filter_type][filter_idx];

    if (filter_idx < (1 << SUBPEL_BITS)) {
      val = 0;
      for (k = 0; k < SUBPEL_TAPS; ++k) {
        idx = base + 1 - (SUBPEL_TAPS / 2) + k;
        idx = AOMMAX(AOMMIN(idx, ref_end_idx), ref_start_idx);
        val += ref[idx] * filter[k];
      }
      val = ROUND_POWER_OF_TWO(val, FILTER_BITS);
    } else {
      val = ref[base + 1];
    }
  }

  return val;
}
#endif  // CONFIG_INTRA_INTERP

// Directional prediction, zone 1: 0 < angle < 90
static void highbd_dr_prediction_z1(uint16_t *dst, ptrdiff_t stride, int bw,
                                    int bh, const uint16_t *above,
                                    const uint16_t *left,
#if CONFIG_INTRA_INTERP
                                    INTRA_FILTER filter_type,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                                    int upsample_above,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                                    int dx, int dy, int bd) {
  int r, c, x, base, shift, val;

  (void)left;
  (void)dy;
  assert(dy == 1);
  assert(dx > 0);

#if !CONFIG_INTRA_EDGE_UPSAMPLE
  const int upsample_above = 0;
#endif  // !CONFIG_INTRA_EDGE_UPSAMPLE
  const int max_base_x = ((bw + bh) - 1) << upsample_above;
  const int frac_bits = 8 - upsample_above;
  const int base_inc = 1 << upsample_above;
  x = dx;
  for (r = 0; r < bh; ++r, dst += stride, x += dx) {
    base = x >> frac_bits;
    shift = (x << upsample_above) & 0xFF;

    if (base >= max_base_x) {
      for (int i = r; i < bh; ++i) {
        aom_memset16(dst, above[max_base_x], bw);
        dst += stride;
      }
      return;
    }

    for (c = 0; c < bw; ++c, base += base_inc) {
      if (base < max_base_x) {
#if CONFIG_INTRA_INTERP
        val = highbd_intra_subpel_interp(base, shift, above, 0, bw + bh - 1,
                                         filter_type);
#else
        val = above[base] * (256 - shift) + above[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif  // CONFIG_INTRA_INTERP
        dst[c] = clip_pixel_highbd(val, bd);
      } else {
        dst[c] = above[max_base_x];
      }
    }
  }
}

// Directional prediction, zone 2: 90 < angle < 180
static void highbd_dr_prediction_z2(uint16_t *dst, ptrdiff_t stride, int bw,
                                    int bh, const uint16_t *above,
                                    const uint16_t *left,
#if CONFIG_INTRA_INTERP
                                    INTRA_FILTER filter_type,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                                    int upsample_above, int upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                                    int dx, int dy, int bd) {
  int r, c, x, y, shift, val, base;

  assert(dx > 0);
  assert(dy > 0);

#if !CONFIG_INTRA_EDGE_UPSAMPLE
  const int upsample_above = 0;
  const int upsample_left = 0;
#endif  // !CONFIG_INTRA_EDGE_UPSAMPLE
  const int min_base_x = -(1 << upsample_above);
  const int frac_bits_x = 8 - upsample_above;
  const int frac_bits_y = 8 - upsample_left;
  for (r = 0; r < bh; ++r) {
    for (c = 0; c < bw; ++c) {
      y = r + 1;
      x = (c << 8) - y * dx;
      base = x >> frac_bits_x;
      if (base >= min_base_x) {
        shift = (x * (1 << upsample_above)) & 0xFF;
#if CONFIG_INTRA_INTERP
        val = highbd_intra_subpel_interp(base, shift, above, -1, bw - 1,
                                         filter_type);
#else
        val = above[base] * (256 - shift) + above[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif  // CONFIG_INTRA_INTERP
      } else {
        x = c + 1;
        y = (r << 8) - x * dy;
        base = y >> frac_bits_y;
        shift = (y * (1 << upsample_left)) & 0xFF;
#if CONFIG_INTRA_INTERP
        val = highbd_intra_subpel_interp(base, shift, left, -1, bh - 1,
                                         filter_type);
#else
        val = left[base] * (256 - shift) + left[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif  // CONFIG_INTRA_INTERP
      }
      dst[c] = clip_pixel_highbd(val, bd);
    }
    dst += stride;
  }
}

// Directional prediction, zone 3: 180 < angle < 270
static void highbd_dr_prediction_z3(uint16_t *dst, ptrdiff_t stride, int bw,
                                    int bh, const uint16_t *above,
                                    const uint16_t *left,
#if CONFIG_INTRA_INTERP
                                    INTRA_FILTER filter_type,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                                    int upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                                    int dx, int dy, int bd) {
  int r, c, y, base, shift, val;

  (void)above;
  (void)dx;
  assert(dx == 1);
  assert(dy > 0);

#if !CONFIG_INTRA_EDGE_UPSAMPLE
  const int upsample_left = 0;
#endif  // !CONFIG_INTRA_EDGE_UPSAMPLE
  const int max_base_y = (bw + bh - 1) << upsample_left;
  const int frac_bits = 8 - upsample_left;
  const int base_inc = 1 << upsample_left;
  y = dy;
  for (c = 0; c < bw; ++c, y += dy) {
    base = y >> frac_bits;
    shift = (y << upsample_left) & 0xFF;

    for (r = 0; r < bh; ++r, base += base_inc) {
      if (base < max_base_y) {
#if CONFIG_INTRA_INTERP
        val = highbd_intra_subpel_interp(base, shift, left, 0, bw + bh - 1,
                                         filter_type);
#else
        val = left[base] * (256 - shift) + left[base + 1] * shift;
        val = ROUND_POWER_OF_TWO(val, 8);
#endif  // CONFIG_INTRA_INTERP
        dst[r * stride + c] = clip_pixel_highbd(val, bd);
      } else {
        for (; r < bh; ++r) dst[r * stride + c] = left[max_base_y];
        break;
      }
    }
  }
}

static void highbd_dr_predictor(uint16_t *dst, ptrdiff_t stride,
                                TX_SIZE tx_size, const uint16_t *above,
                                const uint16_t *left,
#if CONFIG_INTRA_INTERP
                                INTRA_FILTER filter,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                                int upsample_above, int upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                                int angle, int bd) {
  const int dx = get_dx(angle);
  const int dy = get_dy(angle);
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];
  assert(angle > 0 && angle < 270);

  if (angle > 0 && angle < 90) {
    highbd_dr_prediction_z1(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_INTERP
                            filter,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                            upsample_above,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                            dx, dy, bd);
  } else if (angle > 90 && angle < 180) {
    highbd_dr_prediction_z2(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_INTERP
                            filter,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                            upsample_above, upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                            dx, dy, bd);
  } else if (angle > 180 && angle < 270) {
    highbd_dr_prediction_z3(dst, stride, bw, bh, above, left,
#if CONFIG_INTRA_INTERP
                            filter,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                            upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                            dx, dy, bd);
  } else if (angle == 90) {
    pred_high[V_PRED][tx_size](dst, stride, above, left, bd);
  } else if (angle == 180) {
    pred_high[H_PRED][tx_size](dst, stride, above, left, bd);
  }
}
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_EXT_INTRA

#if CONFIG_FILTER_INTRA
#if USE_3TAP_INTRA_FILTER
static int filter_intra_taps_3[TX_SIZES_ALL][FILTER_INTRA_MODES][3] = {
#if CONFIG_CHROMA_2X2
  {
      { 697, 836, -509 },
      { 993, 513, -482 },
      { 381, 984, -341 },
      { 642, 1169, -787 },
      { 590, 553, -119 },
      { 762, 385, -123 },
      { 358, 687, -21 },
      { 411, 1083, -470 },
      { 912, 814, -702 },
      { 883, 902, -761 },
  },
#endif
  {
      { 697, 836, -509 },
      { 993, 513, -482 },
      { 381, 984, -341 },
      { 642, 1169, -787 },
      { 590, 553, -119 },
      { 762, 385, -123 },
      { 358, 687, -21 },
      { 411, 1083, -470 },
      { 912, 814, -702 },
      { 883, 902, -761 },
  },
  {
      { 659, 816, -451 },
      { 980, 625, -581 },
      { 558, 962, -496 },
      { 681, 888, -545 },
      { 591, 613, 180 },
      { 778, 399, -153 },
      { 495, 641, -112 },
      { 671, 937, -584 },
      { 745, 940, -661 },
      { 839, 911, -726 },
  },
  {
      { 539, 927, -442 },
      { 1003, 714, -693 },
      { 349, 1271, -596 },
      { 820, 764, -560 },
      { 524, 816, -316 },
      { 780, 681, -437 },
      { 586, 795, -357 },
      { 551, 1135, -663 },
      { 593, 1061, -630 },
      { 974, 970, -920 },
  },
  {
      { 595, 919, -490 },
      { 945, 668, -579 },
      { 495, 962, -433 },
      { 385, 1551, -912 },
      { 455, 554, 15 },
      { 852, 478, -306 },
      { 177, 760, -87 },
      { -65, 1611, -522 },
      { 815, 894, -685 },
      { 846, 1010, -832 },
  },
#if CONFIG_TX64X64
  {
      { 595, 919, -490 },
      { 945, 668, -579 },
      { 495, 962, -433 },
      { 385, 1551, -912 },
      { 455, 554, 15 },
      { 852, 478, -306 },
      { 177, 760, -87 },
      { -65, 1611, -522 },
      { 815, 894, -685 },
      { 846, 1010, -832 },
  },
#endif  // CONFIG_TX64X64
  {
      { 697, 836, -509 },
      { 993, 513, -482 },
      { 381, 984, -341 },
      { 642, 1169, -787 },
      { 590, 553, -119 },
      { 762, 385, -123 },
      { 358, 687, -21 },
      { 411, 1083, -470 },
      { 912, 814, -702 },
      { 883, 902, -761 },
  },
  {
      { 697, 836, -509 },
      { 993, 513, -482 },
      { 381, 984, -341 },
      { 642, 1169, -787 },
      { 590, 553, -119 },
      { 762, 385, -123 },
      { 358, 687, -21 },
      { 411, 1083, -470 },
      { 912, 814, -702 },
      { 883, 902, -761 },
  },
  {
      { 659, 816, -451 },
      { 980, 625, -581 },
      { 558, 962, -496 },
      { 681, 888, -545 },
      { 591, 613, 180 },
      { 778, 399, -153 },
      { 495, 641, -112 },
      { 671, 937, -584 },
      { 745, 940, -661 },
      { 839, 911, -726 },
  },
  {
      { 659, 816, -451 },
      { 980, 625, -581 },
      { 558, 962, -496 },
      { 681, 888, -545 },
      { 591, 613, 180 },
      { 778, 399, -153 },
      { 495, 641, -112 },
      { 671, 937, -584 },
      { 745, 940, -661 },
      { 839, 911, -726 },
  },
  {
      { 539, 927, -442 },
      { 1003, 714, -693 },
      { 349, 1271, -596 },
      { 820, 764, -560 },
      { 524, 816, -316 },
      { 780, 681, -437 },
      { 586, 795, -357 },
      { 551, 1135, -663 },
      { 593, 1061, -630 },
      { 974, 970, -920 },
  },
  {
      { 539, 927, -442 },
      { 1003, 714, -693 },
      { 349, 1271, -596 },
      { 820, 764, -560 },
      { 524, 816, -316 },
      { 780, 681, -437 },
      { 586, 795, -357 },
      { 551, 1135, -663 },
      { 593, 1061, -630 },
      { 974, 970, -920 },
  },
  {
      { 697, 836, -509 },
      { 993, 513, -482 },
      { 381, 984, -341 },
      { 642, 1169, -787 },
      { 590, 553, -119 },
      { 762, 385, -123 },
      { 358, 687, -21 },
      { 411, 1083, -470 },
      { 912, 814, -702 },
      { 883, 902, -761 },
  },
  {
      { 697, 836, -509 },
      { 993, 513, -482 },
      { 381, 984, -341 },
      { 642, 1169, -787 },
      { 590, 553, -119 },
      { 762, 385, -123 },
      { 358, 687, -21 },
      { 411, 1083, -470 },
      { 912, 814, -702 },
      { 883, 902, -761 },
  },
  {
      { 659, 816, -451 },
      { 980, 625, -581 },
      { 558, 962, -496 },
      { 681, 888, -545 },
      { 591, 613, 180 },
      { 778, 399, -153 },
      { 495, 641, -112 },
      { 671, 937, -584 },
      { 745, 940, -661 },
      { 839, 911, -726 },
  },
  {
      { 659, 816, -451 },
      { 980, 625, -581 },
      { 558, 962, -496 },
      { 681, 888, -545 },
      { 591, 613, 180 },
      { 778, 399, -153 },
      { 495, 641, -112 },
      { 671, 937, -584 },
      { 745, 940, -661 },
      { 839, 911, -726 },
  }
};
#else
static int filter_intra_taps_4[TX_SIZES_ALL][FILTER_INTRA_MODES][4] = {
#if CONFIG_CHROMA_2X2
  {
      { 735, 881, -537, -54 },
      { 1005, 519, -488, -11 },
      { 383, 990, -343, -6 },
      { 442, 805, -542, 319 },
      { 658, 616, -133, -116 },
      { 875, 442, -141, -151 },
      { 386, 741, -23, -80 },
      { 390, 1027, -446, 51 },
      { 679, 606, -523, 262 },
      { 903, 922, -778, -23 },
  },
#endif
  {
      { 735, 881, -537, -54 },
      { 1005, 519, -488, -11 },
      { 383, 990, -343, -6 },
      { 442, 805, -542, 319 },
      { 658, 616, -133, -116 },
      { 875, 442, -141, -151 },
      { 386, 741, -23, -80 },
      { 390, 1027, -446, 51 },
      { 679, 606, -523, 262 },
      { 903, 922, -778, -23 },
  },
  {
      { 648, 803, -444, 16 },
      { 972, 620, -576, 7 },
      { 561, 967, -499, -5 },
      { 585, 762, -468, 144 },
      { 596, 619, -182, -9 },
      { 895, 459, -176, -153 },
      { 557, 722, -126, -129 },
      { 601, 839, -523, 105 },
      { 562, 709, -499, 251 },
      { 803, 872, -695, 43 },
  },
  {
      { 423, 728, -347, 111 },
      { 963, 685, -665, 23 },
      { 281, 1024, -480, 216 },
      { 640, 596, -437, 78 },
      { 429, 669, -259, 99 },
      { 740, 646, -415, 23 },
      { 568, 771, -346, 40 },
      { 404, 833, -486, 209 },
      { 398, 712, -423, 307 },
      { 939, 935, -887, 17 },
  },
  {
      { 477, 737, -393, 150 },
      { 881, 630, -546, 67 },
      { 506, 984, -443, -20 },
      { 114, 459, -270, 528 },
      { 433, 528, 14, 3 },
      { 837, 470, -301, -30 },
      { 181, 777, 89, -107 },
      { -29, 716, -232, 259 },
      { 589, 646, -495, 255 },
      { 740, 884, -728, 77 },
  },
#if CONFIG_TX64X64
  {
      { 477, 737, -393, 150 },
      { 881, 630, -546, 67 },
      { 506, 984, -443, -20 },
      { 114, 459, -270, 528 },
      { 433, 528, 14, 3 },
      { 837, 470, -301, -30 },
      { 181, 777, 89, -107 },
      { -29, 716, -232, 259 },
      { 589, 646, -495, 255 },
      { 740, 884, -728, 77 },
  },
#endif  // CONFIG_TX64X64
  {
      { 735, 881, -537, -54 },
      { 1005, 519, -488, -11 },
      { 383, 990, -343, -6 },
      { 442, 805, -542, 319 },
      { 658, 616, -133, -116 },
      { 875, 442, -141, -151 },
      { 386, 741, -23, -80 },
      { 390, 1027, -446, 51 },
      { 679, 606, -523, 262 },
      { 903, 922, -778, -23 },
  },
  {
      { 735, 881, -537, -54 },
      { 1005, 519, -488, -11 },
      { 383, 990, -343, -6 },
      { 442, 805, -542, 319 },
      { 658, 616, -133, -116 },
      { 875, 442, -141, -151 },
      { 386, 741, -23, -80 },
      { 390, 1027, -446, 51 },
      { 679, 606, -523, 262 },
      { 903, 922, -778, -23 },
  },
  {
      { 648, 803, -444, 16 },
      { 972, 620, -576, 7 },
      { 561, 967, -499, -5 },
      { 585, 762, -468, 144 },
      { 596, 619, -182, -9 },
      { 895, 459, -176, -153 },
      { 557, 722, -126, -129 },
      { 601, 839, -523, 105 },
      { 562, 709, -499, 251 },
      { 803, 872, -695, 43 },
  },
  {
      { 648, 803, -444, 16 },
      { 972, 620, -576, 7 },
      { 561, 967, -499, -5 },
      { 585, 762, -468, 144 },
      { 596, 619, -182, -9 },
      { 895, 459, -176, -153 },
      { 557, 722, -126, -129 },
      { 601, 839, -523, 105 },
      { 562, 709, -499, 251 },
      { 803, 872, -695, 43 },
  },
  {
      { 423, 728, -347, 111 },
      { 963, 685, -665, 23 },
      { 281, 1024, -480, 216 },
      { 640, 596, -437, 78 },
      { 429, 669, -259, 99 },
      { 740, 646, -415, 23 },
      { 568, 771, -346, 40 },
      { 404, 833, -486, 209 },
      { 398, 712, -423, 307 },
      { 939, 935, -887, 17 },
  },
  {
      { 423, 728, -347, 111 },
      { 963, 685, -665, 23 },
      { 281, 1024, -480, 216 },
      { 640, 596, -437, 78 },
      { 429, 669, -259, 99 },
      { 740, 646, -415, 23 },
      { 568, 771, -346, 40 },
      { 404, 833, -486, 209 },
      { 398, 712, -423, 307 },
      { 939, 935, -887, 17 },
  },
  {
      { 735, 881, -537, -54 },
      { 1005, 519, -488, -11 },
      { 383, 990, -343, -6 },
      { 442, 805, -542, 319 },
      { 658, 616, -133, -116 },
      { 875, 442, -141, -151 },
      { 386, 741, -23, -80 },
      { 390, 1027, -446, 51 },
      { 679, 606, -523, 262 },
      { 903, 922, -778, -23 },
  },
  {
      { 735, 881, -537, -54 },
      { 1005, 519, -488, -11 },
      { 383, 990, -343, -6 },
      { 442, 805, -542, 319 },
      { 658, 616, -133, -116 },
      { 875, 442, -141, -151 },
      { 386, 741, -23, -80 },
      { 390, 1027, -446, 51 },
      { 679, 606, -523, 262 },
      { 903, 922, -778, -23 },
  },
  {
      { 648, 803, -444, 16 },
      { 972, 620, -576, 7 },
      { 561, 967, -499, -5 },
      { 585, 762, -468, 144 },
      { 596, 619, -182, -9 },
      { 895, 459, -176, -153 },
      { 557, 722, -126, -129 },
      { 601, 839, -523, 105 },
      { 562, 709, -499, 251 },
      { 803, 872, -695, 43 },
  },
  {
      { 648, 803, -444, 16 },
      { 972, 620, -576, 7 },
      { 561, 967, -499, -5 },
      { 585, 762, -468, 144 },
      { 596, 619, -182, -9 },
      { 895, 459, -176, -153 },
      { 557, 722, -126, -129 },
      { 601, 839, -523, 105 },
      { 562, 709, -499, 251 },
      { 803, 872, -695, 43 },
  }
};
#endif

#if USE_3TAP_INTRA_FILTER
static void filter_intra_predictors_3tap(uint8_t *dst, ptrdiff_t stride,
                                         TX_SIZE tx_size, const uint8_t *above,
                                         const uint8_t *left, int mode) {
  int r, c;
  int mean, ipred;
#if CONFIG_TX64X64
  int buffer[65][65];
#else
  int buffer[33][33];
#endif  // CONFIG_TX64X64
  const int c0 = filter_intra_taps_3[tx_size][mode][0];
  const int c1 = filter_intra_taps_3[tx_size][mode][1];
  const int c2 = filter_intra_taps_3[tx_size][mode][2];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  mean = 0;
  for (r = 0; r < bh; ++r) {
    mean += (int)left[r];
  }
  for (c = 0; c < bw; ++c) {
    mean += (int)above[c];
  }
  mean = (mean + ((bw + bh) >> 1)) / (bw + bh);

  for (r = 0; r < bh; ++r) buffer[r + 1][0] = (int)left[r] - mean;

  for (c = 0; c < bw + 1; ++c) buffer[0][c] = (int)above[c - 1] - mean;

  for (r = 1; r < bh + 1; ++r)
    for (c = 1; c < bw + 1; ++c) {
      ipred = c0 * buffer[r - 1][c] + c1 * buffer[r][c - 1] +
              c2 * buffer[r - 1][c - 1];
      buffer[r][c] = ROUND_POWER_OF_TWO_SIGNED(ipred, FILTER_INTRA_PREC_BITS);
      buffer[r][c] = clip_pixel(buffer[r][c] + mean) - mean;
    }

  for (r = 0; r < bh; ++r) {
    for (c = 0; c < bw; ++c) {
      dst[c] = clip_pixel(buffer[r + 1][c + 1] + mean);
    }
    dst += stride;
  }
}
#else
static void filter_intra_predictors_4tap(uint8_t *dst, ptrdiff_t stride,
                                         TX_SIZE tx_size, const uint8_t *above,
                                         const uint8_t *left, int mode) {
  int r, c;
  int mean, ipred;
#if CONFIG_TX64X64
  int buffer[65][129];
#else
  int buffer[33][65];
#endif  // CONFIG_TX64X64
  const int c0 = filter_intra_taps_4[tx_size][mode][0];
  const int c1 = filter_intra_taps_4[tx_size][mode][1];
  const int c2 = filter_intra_taps_4[tx_size][mode][2];
  const int c3 = filter_intra_taps_4[tx_size][mode][3];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  mean = 0;
  for (r = 0; r < bh; ++r) {
    mean += (int)left[r];
  }
  for (c = 0; c < bw; ++c) {
    mean += (int)above[c];
  }
  mean = (mean + ((bw + bh) >> 1)) / (bw + bh);

  for (r = 0; r < bh; ++r) buffer[r + 1][0] = (int)left[r] - mean;

  for (c = 0; c < 2 * bw + 1; ++c) buffer[0][c] = (int)above[c - 1] - mean;

  for (r = 1; r < bh + 1; ++r)
    for (c = 1; c < 2 * bw + 1 - r; ++c) {
      ipred = c0 * buffer[r - 1][c] + c1 * buffer[r][c - 1] +
              c2 * buffer[r - 1][c - 1] + c3 * buffer[r - 1][c + 1];
      buffer[r][c] = ROUND_POWER_OF_TWO_SIGNED(ipred, FILTER_INTRA_PREC_BITS);
      buffer[r][c] = clip_pixel(buffer[r][c] + mean) - mean;
    }

  for (r = 0; r < bh; ++r) {
    for (c = 0; c < bw; ++c) {
      dst[c] = clip_pixel(buffer[r + 1][c + 1] + mean);
    }
    dst += stride;
  }
}
#endif

void av1_dc_filter_predictor_c(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                               const uint8_t *above, const uint8_t *left) {
#if USE_3TAP_INTRA_FILTER
  filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                               FILTER_DC_PRED);
#else
  filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                               FILTER_DC_PRED);
#endif
}

void av1_v_filter_predictor_c(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                              const uint8_t *above, const uint8_t *left) {
#if USE_3TAP_INTRA_FILTER
  filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                               FILTER_V_PRED);
#else
  filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                               FILTER_V_PRED);
#endif
}

void av1_h_filter_predictor_c(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                              const uint8_t *above, const uint8_t *left) {
#if USE_3TAP_INTRA_FILTER
  filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                               FILTER_H_PRED);
#else
  filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                               FILTER_H_PRED);
#endif
}

void av1_d45_filter_predictor_c(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                                const uint8_t *above, const uint8_t *left) {
#if USE_3TAP_INTRA_FILTER
  filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                               FILTER_D45_PRED);
#else
  filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                               FILTER_D45_PRED);
#endif
}

void av1_d135_filter_predictor_c(uint8_t *dst, ptrdiff_t stride,
                                 TX_SIZE tx_size, const uint8_t *above,
                                 const uint8_t *left) {
#if USE_3TAP_INTRA_FILTER
  filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                               FILTER_D135_PRED);
#else
  filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                               FILTER_D135_PRED);
#endif
}

void av1_d117_filter_predictor_c(uint8_t *dst, ptrdiff_t stride,
                                 TX_SIZE tx_size, const uint8_t *above,
                                 const uint8_t *left) {
#if USE_3TAP_INTRA_FILTER
  filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                               FILTER_D117_PRED);
#else
  filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                               FILTER_D117_PRED);
#endif
}

void av1_d153_filter_predictor_c(uint8_t *dst, ptrdiff_t stride,
                                 TX_SIZE tx_size, const uint8_t *above,
                                 const uint8_t *left) {
#if USE_3TAP_INTRA_FILTER
  filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                               FILTER_D153_PRED);
#else
  filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                               FILTER_D153_PRED);
#endif
}

void av1_d207_filter_predictor_c(uint8_t *dst, ptrdiff_t stride,
                                 TX_SIZE tx_size, const uint8_t *above,
                                 const uint8_t *left) {
#if USE_3TAP_INTRA_FILTER
  filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                               FILTER_D207_PRED);
#else
  filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                               FILTER_D207_PRED);
#endif
}

void av1_d63_filter_predictor_c(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                                const uint8_t *above, const uint8_t *left) {
#if USE_3TAP_INTRA_FILTER
  filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                               FILTER_D63_PRED);
#else
  filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                               FILTER_D63_PRED);
#endif
}

void av1_tm_filter_predictor_c(uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size,
                               const uint8_t *above, const uint8_t *left) {
#if USE_3TAP_INTRA_FILTER
  filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                               FILTER_TM_PRED);
#else
  filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                               FILTER_TM_PRED);
#endif
}

static void filter_intra_predictors(FILTER_INTRA_MODE mode, uint8_t *dst,
                                    ptrdiff_t stride, TX_SIZE tx_size,
                                    const uint8_t *above, const uint8_t *left) {
  switch (mode) {
    case FILTER_DC_PRED:
      av1_dc_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_V_PRED:
      av1_v_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_H_PRED:
      av1_h_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_D45_PRED:
      av1_d45_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_D135_PRED:
      av1_d135_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_D117_PRED:
      av1_d117_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_D153_PRED:
      av1_d153_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_D207_PRED:
      av1_d207_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_D63_PRED:
      av1_d63_filter_predictor(dst, stride, tx_size, above, left);
      break;
    case FILTER_TM_PRED:
      av1_tm_filter_predictor(dst, stride, tx_size, above, left);
      break;
    default: assert(0);
  }
}
#if CONFIG_HIGHBITDEPTH
#if USE_3TAP_INTRA_FILTER
static void highbd_filter_intra_predictors_3tap(uint16_t *dst, ptrdiff_t stride,
                                                TX_SIZE tx_size,
                                                const uint16_t *above,
                                                const uint16_t *left, int mode,
                                                int bd) {
  int r, c;
  int mean, ipred;
#if CONFIG_TX64X64
  int preds[65][65];
#else
  int preds[33][33];
#endif  // CONFIG_TX64X64
  const int c0 = filter_intra_taps_3[tx_size][mode][0];
  const int c1 = filter_intra_taps_3[tx_size][mode][1];
  const int c2 = filter_intra_taps_3[tx_size][mode][2];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  mean = 0;
  for (r = 0; r < bh; ++r) {
    mean += (int)left[r];
  }
  for (c = 0; c < bw; ++c) {
    mean += (int)above[c];
  }
  mean = (mean + ((bw + bh) >> 1)) / (bw + bh);

  for (r = 0; r < bh; ++r) preds[r + 1][0] = (int)left[r] - mean;

  for (c = 0; c < bw + 1; ++c) preds[0][c] = (int)above[c - 1] - mean;

  for (r = 1; r < bh + 1; ++r)
    for (c = 1; c < bw + 1; ++c) {
      ipred = c0 * preds[r - 1][c] + c1 * preds[r][c - 1] +
              c2 * preds[r - 1][c - 1];
      preds[r][c] = ROUND_POWER_OF_TWO_SIGNED(ipred, FILTER_INTRA_PREC_BITS);
      preds[r][c] = clip_pixel_highbd(preds[r][c] + mean, bd) - mean;
    }

  for (r = 0; r < bh; ++r) {
    for (c = 0; c < bw; ++c) {
      dst[c] = clip_pixel_highbd(preds[r + 1][c + 1] + mean, bd);
    }
    dst += stride;
  }
}
#else
static void highbd_filter_intra_predictors_4tap(uint16_t *dst, ptrdiff_t stride,
                                                TX_SIZE tx_size,
                                                const uint16_t *above,
                                                const uint16_t *left, int mode,
                                                int bd) {
  int r, c;
  int mean, ipred;
#if CONFIG_TX64X64
  int preds[65][129];
#else
  int preds[33][65];
#endif  // CONFIG_TX64X64
  const int c0 = filter_intra_taps_4[tx_size][mode][0];
  const int c1 = filter_intra_taps_4[tx_size][mode][1];
  const int c2 = filter_intra_taps_4[tx_size][mode][2];
  const int c3 = filter_intra_taps_4[tx_size][mode][3];
  const int bw = tx_size_wide[tx_size];
  const int bh = tx_size_high[tx_size];

  mean = 0;
  for (r = 0; r < bh; ++r) {
    mean += (int)left[r];
  }
  for (c = 0; c < bw; ++c) {
    mean += (int)above[c];
  }
  mean = (mean + ((bw + bh) >> 1)) / (bw + bh);

  for (r = 0; r < bh; ++r) preds[r + 1][0] = (int)left[r] - mean;

  for (c = 0; c < 2 * bw + 1; ++c) preds[0][c] = (int)above[c - 1] - mean;

  for (r = 1; r < bh + 1; ++r)
    for (c = 1; c < 2 * bw + 1 - r; ++c) {
      ipred = c0 * preds[r - 1][c] + c1 * preds[r][c - 1] +
              c2 * preds[r - 1][c - 1] + c3 * preds[r - 1][c + 1];
      preds[r][c] = ROUND_POWER_OF_TWO_SIGNED(ipred, FILTER_INTRA_PREC_BITS);
      preds[r][c] = clip_pixel_highbd(preds[r][c] + mean, bd) - mean;
    }

  for (r = 0; r < bh; ++r) {
    for (c = 0; c < bw; ++c) {
      dst[c] = clip_pixel_highbd(preds[r + 1][c + 1] + mean, bd);
    }
    dst += stride;
  }
}
#endif

void av1_highbd_dc_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                      TX_SIZE tx_size, const uint16_t *above,
                                      const uint16_t *left, int bd) {
#if USE_3TAP_INTRA_FILTER
  highbd_filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                                      FILTER_DC_PRED, bd);
#else
  highbd_filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                                      FILTER_DC_PRED, bd);
#endif
}

void av1_highbd_v_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                     TX_SIZE tx_size, const uint16_t *above,
                                     const uint16_t *left, int bd) {
#if USE_3TAP_INTRA_FILTER
  highbd_filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                                      FILTER_V_PRED, bd);
#else
  highbd_filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                                      FILTER_V_PRED, bd);
#endif
}

void av1_highbd_h_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                     TX_SIZE tx_size, const uint16_t *above,
                                     const uint16_t *left, int bd) {
#if USE_3TAP_INTRA_FILTER
  highbd_filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                                      FILTER_H_PRED, bd);
#else
  highbd_filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                                      FILTER_H_PRED, bd);
#endif
}

void av1_highbd_d45_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                       TX_SIZE tx_size, const uint16_t *above,
                                       const uint16_t *left, int bd) {
#if USE_3TAP_INTRA_FILTER
  highbd_filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                                      FILTER_D45_PRED, bd);
#else
  highbd_filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                                      FILTER_D45_PRED, bd);
#endif
}

void av1_highbd_d135_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                        TX_SIZE tx_size, const uint16_t *above,
                                        const uint16_t *left, int bd) {
#if USE_3TAP_INTRA_FILTER
  highbd_filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                                      FILTER_D135_PRED, bd);
#else
  highbd_filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                                      FILTER_D135_PRED, bd);
#endif
}

void av1_highbd_d117_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                        TX_SIZE tx_size, const uint16_t *above,
                                        const uint16_t *left, int bd) {
#if USE_3TAP_INTRA_FILTER
  highbd_filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                                      FILTER_D117_PRED, bd);
#else
  highbd_filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                                      FILTER_D117_PRED, bd);
#endif
}

void av1_highbd_d153_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                        TX_SIZE tx_size, const uint16_t *above,
                                        const uint16_t *left, int bd) {
#if USE_3TAP_INTRA_FILTER
  highbd_filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                                      FILTER_D153_PRED, bd);
#else
  highbd_filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                                      FILTER_D153_PRED, bd);
#endif
}

void av1_highbd_d207_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                        TX_SIZE tx_size, const uint16_t *above,
                                        const uint16_t *left, int bd) {
#if USE_3TAP_INTRA_FILTER
  highbd_filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                                      FILTER_D207_PRED, bd);
#else
  highbd_filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                                      FILTER_D207_PRED, bd);
#endif
}

void av1_highbd_d63_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                       TX_SIZE tx_size, const uint16_t *above,
                                       const uint16_t *left, int bd) {
#if USE_3TAP_INTRA_FILTER
  highbd_filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                                      FILTER_D63_PRED, bd);
#else
  highbd_filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                                      FILTER_D63_PRED, bd);
#endif
}

void av1_highbd_tm_filter_predictor_c(uint16_t *dst, ptrdiff_t stride,
                                      TX_SIZE tx_size, const uint16_t *above,
                                      const uint16_t *left, int bd) {
#if USE_3TAP_INTRA_FILTER
  highbd_filter_intra_predictors_3tap(dst, stride, tx_size, above, left,
                                      FILTER_TM_PRED, bd);
#else
  highbd_filter_intra_predictors_4tap(dst, stride, tx_size, above, left,
                                      FILTER_TM_PRED, bd);
#endif
}

static void highbd_filter_intra_predictors(FILTER_INTRA_MODE mode,
                                           uint16_t *dst, ptrdiff_t stride,
                                           TX_SIZE tx_size,
                                           const uint16_t *above,
                                           const uint16_t *left, int bd) {
  switch (mode) {
    case FILTER_DC_PRED:
      av1_highbd_dc_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_V_PRED:
      av1_highbd_v_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_H_PRED:
      av1_highbd_h_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_D45_PRED:
      av1_highbd_d45_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_D135_PRED:
      av1_highbd_d135_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_D117_PRED:
      av1_highbd_d117_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_D153_PRED:
      av1_highbd_d153_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_D207_PRED:
      av1_highbd_d207_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_D63_PRED:
      av1_highbd_d63_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    case FILTER_TM_PRED:
      av1_highbd_tm_filter_predictor(dst, stride, tx_size, above, left, bd);
      break;
    default: assert(0);
  }
}
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_INTRA_EDGE
static int intra_edge_filter_strength(int bsz, int delta) {
  const int d = abs(delta);
  int strength = 0;

  switch (bsz) {
    case 4:
      if (d < 56) {
        strength = 0;
      } else if (d < 90) {
        strength = 1;
      }
      break;
    case 8:
      if (d < 8) {
        strength = 0;
      } else if (d < 32) {
        strength = 1;
      } else if (d < 90) {
        strength = 3;
      }
      break;
    case 16:
      if (d < 4) {
        strength = 0;
      } else if (d < 16) {
        strength = 1;
      } else if (d < 90) {
        strength = 3;
      }
      break;
    case 32:
      if (d < 16) {
        strength = 2;
      } else if (d < 90) {
        strength = 3;
      }
      break;
    default: strength = 0; break;
  }

  return strength;
}

void av1_filter_intra_edge_c(uint8_t *p, int sz, int strength) {
  if (!strength) return;

  const int kernel[INTRA_EDGE_FILT][INTRA_EDGE_TAPS] = {
    { 0, 4, 8, 4, 0 }, { 0, 5, 6, 5, 0 }, { 2, 4, 4, 4, 2 }
  };
  const int filt = strength - 1;
  uint8_t edge[129];

  memcpy(edge, p, sz * sizeof(*p));
  for (int i = 1; i < sz - 1; i++) {
    int s = 0;
    for (int j = 0; j < INTRA_EDGE_TAPS; j++) {
      int k = i - 2 + j;
      k = (k < 0) ? 0 : k;
      k = (k > sz - 1) ? sz - 1 : k;
      s += edge[k] * kernel[filt][j];
    }
    s = (s + 8) >> 4;
    p[i] = s;
  }
}

#if CONFIG_HIGHBITDEPTH
void av1_filter_intra_edge_high_c(uint16_t *p, int sz, int strength) {
  if (!strength) return;

  const int kernel[INTRA_EDGE_FILT][INTRA_EDGE_TAPS] = {
    { 0, 4, 8, 4, 0 }, { 0, 5, 6, 5, 0 }, { 2, 4, 4, 4, 2 }
  };
  const int filt = strength - 1;
  uint16_t edge[129];

  memcpy(edge, p, sz * sizeof(*p));
  for (int i = 1; i < sz - 1; i++) {
    int s = 0;
    for (int j = 0; j < INTRA_EDGE_TAPS; j++) {
      int k = i - 2 + j;
      k = (k < 0) ? 0 : k;
      k = (k > sz - 1) ? sz - 1 : k;
      s += edge[k] * kernel[filt][j];
    }
    s = (s + 8) >> 4;
    p[i] = s;
  }
}
#endif  // CONFIG_HIGHBITDEPTH

#if CONFIG_INTRA_EDGE_UPSAMPLE
static int use_intra_edge_upsample(int bsz, int delta) {
  const int d = abs(delta);
  return (bsz == 4 && d > 0 && d < 56);
}

void av1_upsample_intra_edge_c(uint8_t *p, int sz) {
  // interpolate half-sample positions
  assert(sz <= MAX_UPSAMPLE_SZ);

  uint8_t in[MAX_UPSAMPLE_SZ + 3];
  // copy p[-1..(sz-1)] and extend first and last samples
  in[0] = p[-1];
  in[1] = p[-1];
  for (int i = 0; i < sz; i++) {
    in[i + 2] = p[i];
  }
  in[sz + 2] = p[sz - 1];

  // interpolate half-sample edge positions
  p[-2] = in[0];
  for (int i = 0; i < sz; i++) {
    int s = -in[i] + (9 * in[i + 1]) + (9 * in[i + 2]) - in[i + 3];
    s = clip_pixel((s + 8) >> 4);
    p[2 * i - 1] = s;
    p[2 * i] = in[i + 2];
  }
}

#if CONFIG_HIGHBITDEPTH
void av1_upsample_intra_edge_high_c(uint16_t *p, int sz, int bd) {
  // interpolate half-sample positions
  assert(sz <= MAX_UPSAMPLE_SZ);

  uint16_t in[MAX_UPSAMPLE_SZ + 3];
  // copy p[-1..(sz-1)] and extend first and last samples
  in[0] = p[-1];
  in[1] = p[-1];
  for (int i = 0; i < sz; i++) {
    in[i + 2] = p[i];
  }
  in[sz + 2] = p[sz - 1];

  // interpolate half-sample edge positions
  p[-2] = in[0];
  for (int i = 0; i < sz; i++) {
    int s = -in[i] + (9 * in[i + 1]) + (9 * in[i + 2]) - in[i + 3];
    s = (s + 8) >> 4;
    s = clip_pixel_highbd(s, bd);
    p[2 * i - 1] = s;
    p[2 * i] = in[i + 2];
  }
}
#endif  // CONFIG_HIGHBITDEPTH
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE

#endif  // CONFIG_INTRA_EDGE

#if CONFIG_HIGHBITDEPTH
static void build_intra_predictors_high(
    const MACROBLOCKD *xd, const uint8_t *ref8, int ref_stride, uint8_t *dst8,
    int dst_stride, PREDICTION_MODE mode, TX_SIZE tx_size, int n_top_px,
    int n_topright_px, int n_left_px, int n_bottomleft_px, int plane) {
  int i;
  uint16_t *dst = CONVERT_TO_SHORTPTR(dst8);
  uint16_t *ref = CONVERT_TO_SHORTPTR(ref8);
  DECLARE_ALIGNED(16, uint16_t, left_data[MAX_TX_SIZE * 2 + 32]);
  DECLARE_ALIGNED(16, uint16_t, above_data[MAX_TX_SIZE * 2 + 32]);
  uint16_t *const above_row = above_data + 16;
  uint16_t *const left_col = left_data + 16;
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
#if !INTRA_USES_RECT_TRANSFORMS
  assert(txwpx == txhpx);
#endif  // !INTRA_USES_RECT_TRANSFORMS
  int need_left = extend_modes[mode] & NEED_LEFT;
  int need_above = extend_modes[mode] & NEED_ABOVE;
  int need_above_left = extend_modes[mode] & NEED_ABOVELEFT;
  const uint16_t *above_ref = ref - ref_stride;
#if CONFIG_EXT_INTRA
  int p_angle = 0;
  const int is_dr_mode = av1_is_directional_mode(mode, xd->mi[0]->mbmi.sb_type);
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  const FILTER_INTRA_MODE_INFO *filter_intra_mode_info =
      &xd->mi[0]->mbmi.filter_intra_mode_info;
  const FILTER_INTRA_MODE filter_intra_mode =
      filter_intra_mode_info->filter_intra_mode[plane != 0];
#endif  // CONFIG_FILTER_INTRA
  int base = 128 << (xd->bd - 8);

  // base-1 base-1 base-1 .. base-1 base-1 base-1 base-1 base-1 base-1
  // base+1   A      B  ..     Y      Z
  // base+1   C      D  ..     W      X
  // base+1   E      F  ..     U      V
  // base+1   G      H  ..     S      T      T      T      T      T
  aom_memset16(left_data, base + 1, sizeof(left_data) / sizeof(*left_data));

#if CONFIG_EXT_INTRA
  if (is_dr_mode) {
    p_angle = mode_to_angle_map[mode] +
              xd->mi[0]->mbmi.angle_delta[plane != 0] * ANGLE_STEP;
    if (p_angle <= 90)
      need_above = 1, need_left = 0, need_above_left = 1;
    else if (p_angle < 180)
      need_above = 1, need_left = 1, need_above_left = 1;
    else
      need_above = 0, need_left = 1, need_above_left = 1;
  }
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  if (filter_intra_mode_info->use_filter_intra_mode[plane != 0])
    need_left = need_above = need_above_left = 1;
#endif  // CONFIG_FILTER_INTRA

  (void)plane;
  assert(n_top_px >= 0);
  assert(n_topright_px >= 0);
  assert(n_left_px >= 0);
  assert(n_bottomleft_px >= 0);

  if ((!need_above && n_left_px == 0) || (!need_left && n_top_px == 0)) {
#if CONFIG_INTRA_EDGE
    int val;
    if (need_left) {
      val = (n_top_px > 0) ? above_ref[0] : base + 1;
    } else {
      val = (n_left_px > 0) ? ref[-1] : base - 1;
    }
#else
    const int val = need_left ? base + 1 : base - 1;
#endif  // CONFIG_INTRA_EDGE
    for (i = 0; i < txhpx; ++i) {
      aom_memset16(dst, val, txwpx);
      dst += dst_stride;
    }
    return;
  }

  // NEED_LEFT
  if (need_left) {
#if CONFIG_EXT_INTRA || CONFIG_FILTER_INTRA
    int need_bottom = !!(extend_modes[mode] & NEED_BOTTOMLEFT);
#if CONFIG_FILTER_INTRA
    if (filter_intra_mode_info->use_filter_intra_mode[plane != 0])
      need_bottom = 0;
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_EXT_INTRA
    if (is_dr_mode) need_bottom = p_angle > 180;
#endif  // CONFIG_EXT_INTRA
#else
    const int need_bottom = !!(extend_modes[mode] & NEED_BOTTOMLEFT);
#endif  // CONFIG_EXT_INTRA || CONFIG_FILTER_INTRA
    const int num_left_pixels_needed = txhpx + (need_bottom ? txwpx : 0);
    i = 0;
    if (n_left_px > 0) {
      for (; i < n_left_px; i++) left_col[i] = ref[i * ref_stride - 1];
      if (need_bottom && n_bottomleft_px > 0) {
        assert(i == txhpx);
        for (; i < txhpx + n_bottomleft_px; i++)
          left_col[i] = ref[i * ref_stride - 1];
      }
      if (i < num_left_pixels_needed)
        aom_memset16(&left_col[i], left_col[i - 1], num_left_pixels_needed - i);
    } else {
#if CONFIG_INTRA_EDGE
      if (n_top_px > 0) {
        aom_memset16(left_col, above_ref[0], num_left_pixels_needed);
      } else {
#endif  // CONFIG_INTRA_EDGE
        aom_memset16(left_col, base + 1, num_left_pixels_needed);
#if CONFIG_INTRA_EDGE
      }
#endif  // CONFIG_INTRA_EDGE
    }
  }

  // NEED_ABOVE
  if (need_above) {
#if CONFIG_EXT_INTRA || CONFIG_FILTER_INTRA
    int need_right = !!(extend_modes[mode] & NEED_ABOVERIGHT);
#if CONFIG_FILTER_INTRA
    if (filter_intra_mode_info->use_filter_intra_mode[plane != 0])
      need_right = 1;
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_EXT_INTRA
    if (is_dr_mode) need_right = p_angle < 90;
#endif  // CONFIG_EXT_INTRA
#else
    const int need_right = !!(extend_modes[mode] & NEED_ABOVERIGHT);
#endif  // CONFIG_EXT_INTRA || CONFIG_FILTER_INTRA
    const int num_top_pixels_needed = txwpx + (need_right ? txhpx : 0);
    if (n_top_px > 0) {
      memcpy(above_row, above_ref, n_top_px * sizeof(above_ref[0]));
      i = n_top_px;
      if (need_right && n_topright_px > 0) {
        assert(n_top_px == txwpx);
        memcpy(above_row + txwpx, above_ref + txwpx,
               n_topright_px * sizeof(above_ref[0]));
        i += n_topright_px;
      }
      if (i < num_top_pixels_needed)
        aom_memset16(&above_row[i], above_row[i - 1],
                     num_top_pixels_needed - i);
    } else {
#if CONFIG_INTRA_EDGE
      if (n_left_px > 0) {
        aom_memset16(above_row, ref[-1], num_top_pixels_needed);
      } else {
#endif  // CONFIG_INTRA_EDGE
        aom_memset16(above_row, base - 1, num_top_pixels_needed);
#if CONFIG_INTRA_EDGE
      }
#endif  // CONFIG_INTRA_EDGE
    }
  }

  if (need_above_left) {
#if CONFIG_INTRA_EDGE
    if (n_top_px > 0 && n_left_px > 0) {
      above_row[-1] = above_ref[-1];
    } else if (n_top_px > 0) {
      above_row[-1] = above_ref[0];
    } else if (n_left_px > 0) {
      above_row[-1] = ref[-1];
    } else {
      above_row[-1] = base;
    }
#else
    above_row[-1] =
        n_top_px > 0 ? (n_left_px > 0 ? above_ref[-1] : base + 1) : base - 1;
#endif  // CONFIG_INTRA_EDGE
    left_col[-1] = above_row[-1];
  }

#if CONFIG_FILTER_INTRA
  if (filter_intra_mode_info->use_filter_intra_mode[plane != 0]) {
    highbd_filter_intra_predictors(filter_intra_mode, dst, dst_stride, tx_size,
                                   above_row, left_col, xd->bd);
    return;
  }
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_EXT_INTRA
  if (is_dr_mode) {
#if CONFIG_INTRA_INTERP
    INTRA_FILTER filter = INTRA_FILTER_LINEAR;
    if (plane == 0 && av1_is_intra_filter_switchable(p_angle))
      filter = xd->mi[0]->mbmi.intra_filter;
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE
    const int need_right = p_angle < 90;
    const int need_bottom = p_angle > 180;
    if (p_angle != 90 && p_angle != 180) {
      const int ab_le = need_above_left ? 1 : 0;
      if (need_above && n_top_px > 0) {
        const int strength = intra_edge_filter_strength(txwpx, p_angle - 90);
        const int n_px = n_top_px + ab_le + (need_right ? n_topright_px : 0);
        av1_filter_intra_edge_high(above_row - ab_le, n_px, strength);
      }
      if (need_left && n_left_px > 0) {
        const int strength = intra_edge_filter_strength(txhpx, p_angle - 180);
        const int n_px =
            n_left_px + ab_le + (need_bottom ? n_bottomleft_px : 0);
        av1_filter_intra_edge_high(left_col - ab_le, n_px, strength);
      }
    }
#if CONFIG_INTRA_EDGE_UPSAMPLE
    const int upsample_above = use_intra_edge_upsample(txwpx, p_angle - 90);
    if (need_above && upsample_above) {
      const int n_px = txwpx + (need_right ? txhpx : 0);
      av1_upsample_intra_edge_high(above_row, n_px, xd->bd);
    }
    const int upsample_left = use_intra_edge_upsample(txhpx, p_angle - 180);
    if (need_left && upsample_left) {
      const int n_px = txhpx + (need_bottom ? txwpx : 0);
      av1_upsample_intra_edge_high(left_col, n_px, xd->bd);
    }
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
#endif  // CONFIG_INTRA_EDGE
    highbd_dr_predictor(dst, dst_stride, tx_size, above_row, left_col,
#if CONFIG_INTRA_INTERP
                        filter,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                        upsample_above, upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                        p_angle, xd->bd);
    return;
  }
#endif  // CONFIG_EXT_INTRA

  // predict
  if (mode == DC_PRED) {
    dc_pred_high[n_left_px > 0][n_top_px > 0][tx_size](
        dst, dst_stride, above_row, left_col, xd->bd);
  } else {
    pred_high[mode][tx_size](dst, dst_stride, above_row, left_col, xd->bd);
  }
}
#endif  // CONFIG_HIGHBITDEPTH

static void build_intra_predictors(const MACROBLOCKD *xd, const uint8_t *ref,
                                   int ref_stride, uint8_t *dst, int dst_stride,
                                   PREDICTION_MODE mode, TX_SIZE tx_size,
                                   int n_top_px, int n_topright_px,
                                   int n_left_px, int n_bottomleft_px,
                                   int plane) {
  int i;
  const uint8_t *above_ref = ref - ref_stride;
  DECLARE_ALIGNED(16, uint8_t, left_data[MAX_TX_SIZE * 2 + 32]);
  DECLARE_ALIGNED(16, uint8_t, above_data[MAX_TX_SIZE * 2 + 32]);
  uint8_t *const above_row = above_data + 16;
  uint8_t *const left_col = left_data + 16;
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
#if !INTRA_USES_RECT_TRANSFORMS
  assert(txwpx == txhpx);
#endif  // !INTRA_USES_RECT_TRANSFORMS
  int need_left = extend_modes[mode] & NEED_LEFT;
  int need_above = extend_modes[mode] & NEED_ABOVE;
  int need_above_left = extend_modes[mode] & NEED_ABOVELEFT;
#if CONFIG_EXT_INTRA
  int p_angle = 0;
  const MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int is_dr_mode = av1_is_directional_mode(mode, mbmi->sb_type);
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  const FILTER_INTRA_MODE_INFO *filter_intra_mode_info =
      &xd->mi[0]->mbmi.filter_intra_mode_info;
  const FILTER_INTRA_MODE filter_intra_mode =
      filter_intra_mode_info->filter_intra_mode[plane != 0];
#endif  // CONFIG_FILTER_INTRA

  // 127 127 127 .. 127 127 127 127 127 127
  // 129  A   B  ..  Y   Z
  // 129  C   D  ..  W   X
  // 129  E   F  ..  U   V
  // 129  G   H  ..  S   T   T   T   T   T
  // ..
  memset(left_data, 129, sizeof(left_data));

#if CONFIG_EXT_INTRA
  if (is_dr_mode) {
    p_angle = mode_to_angle_map[mode] +
              xd->mi[0]->mbmi.angle_delta[plane != 0] * ANGLE_STEP;
    if (p_angle <= 90)
      need_above = 1, need_left = 0, need_above_left = 1;
    else if (p_angle < 180)
      need_above = 1, need_left = 1, need_above_left = 1;
    else
      need_above = 0, need_left = 1, need_above_left = 1;
  }
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  if (filter_intra_mode_info->use_filter_intra_mode[plane != 0])
    need_left = need_above = need_above_left = 1;
#endif  // CONFIG_FILTER_INTRA

  (void)xd;
  (void)plane;
  assert(n_top_px >= 0);
  assert(n_topright_px >= 0);
  assert(n_left_px >= 0);
  assert(n_bottomleft_px >= 0);

  if ((!need_above && n_left_px == 0) || (!need_left && n_top_px == 0)) {
#if CONFIG_INTRA_EDGE
    int val;
    if (need_left) {
      val = (n_top_px > 0) ? above_ref[0] : 129;
    } else {
      val = (n_left_px > 0) ? ref[-1] : 127;
    }
#else
    const int val = need_left ? 129 : 127;
#endif  // CONFIG_INTRA_EDGE
    for (i = 0; i < txhpx; ++i) {
      memset(dst, val, txwpx);
      dst += dst_stride;
    }
    return;
  }

  // NEED_LEFT
  if (need_left) {
#if CONFIG_EXT_INTRA || CONFIG_FILTER_INTRA
    int need_bottom = !!(extend_modes[mode] & NEED_BOTTOMLEFT);
#if CONFIG_FILTER_INTRA
    if (filter_intra_mode_info->use_filter_intra_mode[plane != 0])
      need_bottom = 0;
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_EXT_INTRA
    if (is_dr_mode) need_bottom = p_angle > 180;
#endif  // CONFIG_EXT_INTRA
#else
    const int need_bottom = !!(extend_modes[mode] & NEED_BOTTOMLEFT);
#endif  // CONFIG_EXT_INTRA || CONFIG_FILTER_INTRA
    const int num_left_pixels_needed = txhpx + (need_bottom ? txwpx : 0);
    i = 0;
    if (n_left_px > 0) {
      for (; i < n_left_px; i++) left_col[i] = ref[i * ref_stride - 1];
      if (need_bottom && n_bottomleft_px > 0) {
        assert(i == txhpx);
        for (; i < txhpx + n_bottomleft_px; i++)
          left_col[i] = ref[i * ref_stride - 1];
      }
      if (i < num_left_pixels_needed)
        memset(&left_col[i], left_col[i - 1], num_left_pixels_needed - i);
    } else {
#if CONFIG_INTRA_EDGE
      if (n_top_px > 0) {
        memset(left_col, above_ref[0], num_left_pixels_needed);
      } else {
#endif  // CONFIG_INTRA_EDGE
        memset(left_col, 129, num_left_pixels_needed);
#if CONFIG_INTRA_EDGE
      }
#endif  // CONFIG_INTRA_EDGE
    }
  }

  // NEED_ABOVE
  if (need_above) {
#if CONFIG_EXT_INTRA || CONFIG_FILTER_INTRA
    int need_right = !!(extend_modes[mode] & NEED_ABOVERIGHT);
#if CONFIG_FILTER_INTRA
    if (filter_intra_mode_info->use_filter_intra_mode[plane != 0])
      need_right = 1;
#endif  // CONFIG_FILTER_INTRA
#if CONFIG_EXT_INTRA
    if (is_dr_mode) need_right = p_angle < 90;
#endif  // CONFIG_EXT_INTRA
#else
    const int need_right = !!(extend_modes[mode] & NEED_ABOVERIGHT);
#endif  // CONFIG_EXT_INTRA || CONFIG_FITLER_INTRA
    const int num_top_pixels_needed = txwpx + (need_right ? txhpx : 0);
    if (n_top_px > 0) {
      memcpy(above_row, above_ref, n_top_px);
      i = n_top_px;
      if (need_right && n_topright_px > 0) {
        assert(n_top_px == txwpx);
        memcpy(above_row + txwpx, above_ref + txwpx, n_topright_px);
        i += n_topright_px;
      }
      if (i < num_top_pixels_needed)
        memset(&above_row[i], above_row[i - 1], num_top_pixels_needed - i);
    } else {
#if CONFIG_INTRA_EDGE
      if (n_left_px > 0) {
        memset(above_row, ref[-1], num_top_pixels_needed);
      } else {
#endif  // CONFIG_INTRA_EDGE
        memset(above_row, 127, num_top_pixels_needed);
#if CONFIG_INTRA_EDGE
      }
#endif  // CONFIG_INTRA_EDGE
    }
  }

  if (need_above_left) {
#if CONFIG_INTRA_EDGE
    if (n_top_px > 0 && n_left_px > 0) {
      above_row[-1] = above_ref[-1];
    } else if (n_top_px > 0) {
      above_row[-1] = above_ref[0];
    } else if (n_left_px > 0) {
      above_row[-1] = ref[-1];
    } else {
      above_row[-1] = 128;
    }
#else
    above_row[-1] = n_top_px > 0 ? (n_left_px > 0 ? above_ref[-1] : 129) : 127;
#endif  // CONFIG_INTRA_EDGE
    left_col[-1] = above_row[-1];
  }

#if CONFIG_FILTER_INTRA
  if (filter_intra_mode_info->use_filter_intra_mode[plane != 0]) {
    filter_intra_predictors(filter_intra_mode, dst, dst_stride, tx_size,
                            above_row, left_col);
    return;
  }
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_EXT_INTRA
  if (is_dr_mode) {
#if CONFIG_INTRA_INTERP
    INTRA_FILTER filter = INTRA_FILTER_LINEAR;
    if (plane == 0 && av1_is_intra_filter_switchable(p_angle))
      filter = xd->mi[0]->mbmi.intra_filter;
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE
    const int need_right = p_angle < 90;
    const int need_bottom = p_angle > 180;
    if (p_angle != 90 && p_angle != 180) {
      const int ab_le = need_above_left ? 1 : 0;
      if (need_above && n_top_px > 0) {
        const int strength = intra_edge_filter_strength(txwpx, p_angle - 90);
        const int n_px = n_top_px + ab_le + (need_right ? n_topright_px : 0);
        av1_filter_intra_edge(above_row - ab_le, n_px, strength);
      }
      if (need_left && n_left_px > 0) {
        const int strength = intra_edge_filter_strength(txhpx, p_angle - 180);
        const int n_px =
            n_left_px + ab_le + (need_bottom ? n_bottomleft_px : 0);
        av1_filter_intra_edge(left_col - ab_le, n_px, strength);
      }
    }
#if CONFIG_INTRA_EDGE_UPSAMPLE
    const int upsample_above = use_intra_edge_upsample(txwpx, p_angle - 90);
    if (need_above && upsample_above) {
      const int n_px = txwpx + (need_right ? txhpx : 0);
      av1_upsample_intra_edge(above_row, n_px);
    }
    const int upsample_left = use_intra_edge_upsample(txhpx, p_angle - 180);
    if (need_left && upsample_left) {
      const int n_px = txhpx + (need_bottom ? txwpx : 0);
      av1_upsample_intra_edge(left_col, n_px);
    }
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
#endif  // CONFIG_INTRA_EDGE
    dr_predictor(dst, dst_stride, tx_size, above_row, left_col,
#if CONFIG_INTRA_INTERP
                 filter,
#endif  // CONFIG_INTRA_INTERP
#if CONFIG_INTRA_EDGE_UPSAMPLE
                 upsample_above, upsample_left,
#endif  // CONFIG_INTRA_EDGE_UPSAMPLE
                 p_angle);
    return;
  }
#endif  // CONFIG_EXT_INTRA

  // predict
  if (mode == DC_PRED) {
    dc_pred[n_left_px > 0][n_top_px > 0][tx_size](dst, dst_stride, above_row,
                                                  left_col);
  } else {
    pred[mode][tx_size](dst, dst_stride, above_row, left_col);
  }
}

static void predict_intra_block_helper(const AV1_COMMON *cm,
                                       const MACROBLOCKD *xd, int wpx, int hpx,
                                       TX_SIZE tx_size, PREDICTION_MODE mode,
                                       const uint8_t *ref, int ref_stride,
                                       uint8_t *dst, int dst_stride,
                                       int col_off, int row_off, int plane) {
  BLOCK_SIZE bsize = xd->mi[0]->mbmi.sb_type;
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  const int txw = tx_size_wide_unit[tx_size];
#if CONFIG_CB4X4 && CONFIG_CHROMA_SUB8X8
  const int have_top = row_off || (pd->subsampling_y ? xd->chroma_up_available
                                                     : xd->up_available);
  const int have_left =
      col_off ||
      (pd->subsampling_x ? xd->chroma_left_available : xd->left_available);
#else
  const int have_top = row_off || xd->up_available;
  const int have_left = col_off || xd->left_available;
#endif
  const int x = col_off << tx_size_wide_log2[0];
  const int y = row_off << tx_size_high_log2[0];
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  const int txwpx = tx_size_wide[tx_size];
  const int txhpx = tx_size_high[tx_size];
#if !INTRA_USES_RECT_TRANSFORMS
  assert(txwpx == txhpx);
#endif  // !INTRA_USES_RECT_TRANSFORMS
#if CONFIG_CB4X4 && !CONFIG_CHROMA_2X2 && !CONFIG_CHROMA_SUB8X8
  const int xr_chr_offset = (pd->subsampling_x && bsize < BLOCK_8X8) ? 2 : 0;
  const int yd_chr_offset = (pd->subsampling_y && bsize < BLOCK_8X8) ? 2 : 0;
#else
  const int xr_chr_offset = 0;
  const int yd_chr_offset = 0;
#endif

  // Distance between the right edge of this prediction block to
  // the frame right edge
  const int xr = (xd->mb_to_right_edge >> (3 + pd->subsampling_x)) +
                 (wpx - x - txwpx) - xr_chr_offset;
  // Distance between the bottom edge of this prediction block to
  // the frame bottom edge
  const int yd = (xd->mb_to_bottom_edge >> (3 + pd->subsampling_y)) +
                 (hpx - y - txhpx) - yd_chr_offset;
  const int right_available = mi_col + ((col_off + txw) << pd->subsampling_x >>
                                        (MI_SIZE_LOG2 - tx_size_wide_log2[0])) <
                              xd->tile.mi_col_end;
  const int bottom_available = (yd > 0);
#if CONFIG_EXT_PARTITION_TYPES && !CONFIG_EXT_PARTITION_TYPES_AB
  const PARTITION_TYPE partition = xd->mi[0]->mbmi.partition;
#endif

#if CONFIG_CB4X4 && !CONFIG_CHROMA_2X2
  // force 4x4 chroma component block size.
  bsize = scale_chroma_bsize(bsize, pd->subsampling_x, pd->subsampling_y);
#endif

  const int have_top_right =
      has_top_right(cm, bsize, mi_row, mi_col, have_top, right_available,
#if CONFIG_EXT_PARTITION_TYPES && !CONFIG_EXT_PARTITION_TYPES_AB
                    partition,
#endif  // CONFIG_EXT_PARTITION_TYPES && !CONFIG_EXT_PARTITION_TYPES_AB
                    tx_size, row_off, col_off, pd->subsampling_x);
  const int have_bottom_left =
      has_bottom_left(cm, bsize, mi_row, mi_col, bottom_available, have_left,
                      tx_size, row_off, col_off, pd->subsampling_y);
  if (xd->mi[0]->mbmi.palette_mode_info.palette_size[plane != 0] > 0) {
    const int stride = wpx;
    int r, c;
    const uint8_t *const map = xd->plane[plane != 0].color_index_map;
    uint16_t *palette = xd->mi[0]->mbmi.palette_mode_info.palette_colors +
                        plane * PALETTE_MAX_SIZE;

#if CONFIG_HIGHBITDEPTH
    if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
      uint16_t *dst16 = CONVERT_TO_SHORTPTR(dst);
      for (r = 0; r < txhpx; ++r) {
        for (c = 0; c < txwpx; ++c) {
          dst16[r * dst_stride + c] = palette[map[(r + y) * stride + c + x]];
        }
      }
    } else {
#endif  // CONFIG_HIGHBITDEPTH
      for (r = 0; r < txhpx; ++r) {
        for (c = 0; c < txwpx; ++c) {
          dst[r * dst_stride + c] =
              (uint8_t)palette[map[(r + y) * stride + c + x]];
        }
      }
#if CONFIG_HIGHBITDEPTH
    }
#endif  // CONFIG_HIGHBITDEPTH
    return;
  }

#if CONFIG_HIGHBITDEPTH
  if (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH) {
    build_intra_predictors_high(
        xd, ref, ref_stride, dst, dst_stride, mode, tx_size,
        have_top ? AOMMIN(txwpx, xr + txwpx) : 0,
        have_top_right ? AOMMIN(txwpx, xr) : 0,
        have_left ? AOMMIN(txhpx, yd + txhpx) : 0,
        have_bottom_left ? AOMMIN(txhpx, yd) : 0, plane);
    return;
  }
#endif
  build_intra_predictors(xd, ref, ref_stride, dst, dst_stride, mode, tx_size,
                         have_top ? AOMMIN(txwpx, xr + txwpx) : 0,
                         have_top_right ? AOMMIN(txwpx, xr) : 0,
                         have_left ? AOMMIN(txhpx, yd + txhpx) : 0,
                         have_bottom_left ? AOMMIN(txhpx, yd) : 0, plane);
}

void av1_predict_intra_block_facade(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                    int plane, int block_idx, int blk_col,
                                    int blk_row, TX_SIZE tx_size) {
  const MODE_INFO *mi = xd->mi[0];
  const MB_MODE_INFO *const mbmi = &mi->mbmi;
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int dst_stride = pd->dst.stride;
  uint8_t *dst =
      &pd->dst.buf[(blk_row * dst_stride + blk_col) << tx_size_wide_log2[0]];
  const int block_raster_idx =
      av1_block_index_to_raster_order(tx_size, block_idx);
  const PREDICTION_MODE mode = (plane == AOM_PLANE_Y)
                                   ? get_y_mode(mi, block_raster_idx)
                                   : get_uv_mode(mbmi->uv_mode);
#if CONFIG_CFL
  if (plane != AOM_PLANE_Y && mbmi->uv_mode == UV_CFL_PRED) {
    if (plane == AOM_PLANE_U && blk_col == 0 && blk_row == 0) {
      // Avoid computing the CfL parameters twice, if they have already been
      // computed in cfl_rd_pick_alpha.
      if (!xd->cfl->are_parameters_computed)
        cfl_compute_parameters(xd, tx_size);
    }
    cfl_predict_block(xd, dst, dst_stride, blk_row, blk_col, tx_size, plane);
    return;
  }
#endif

  av1_predict_intra_block(cm, xd, pd->width, pd->height,
                          txsize_to_bsize[tx_size], mode, dst, dst_stride, dst,
                          dst_stride, blk_col, blk_row, plane);
}

#if INTRA_USES_EXT_TRANSFORMS
// Copy the given row of dst into the equivalent row of ref, saving
// the overwritten data to tmp. Returns zero if no copy happened (so
// no restore is needed)
//
// Note that ref_row and dst_row follow the usual hibd convention
// where you convert to a uint16_t* with CONVERT_TO_SHORTPTR(). tmp
// does not follow that convention: it's a genuine pointer which is
// correctly aligned and sized for either 8 or 16 bit data.
//
// matching_strides is a boolean flag which should be nonzero if ref
// and dst have the same stride.
static int overwrite_ref_row(int matching_strides, int buf_flags,
                             int block_width, const uint8_t *dst_row,
                             uint8_t *ref_row, uint8_t *tmp_row) {
  if (ref_row == dst_row && matching_strides) return 0;

  int row_bytes = block_width;

#if CONFIG_HIGHBITDEPTH
  if (buf_flags & YV12_FLAG_HIGHBITDEPTH) {
    row_bytes *= 2;
    ref_row = (uint8_t *)CONVERT_TO_SHORTPTR(ref_row);
    dst_row = (const uint8_t *)CONVERT_TO_SHORTPTR(dst_row);
  }
#else
  (void)buf_flags;
#endif  // CONFIG_HIGHBITDEPTH

  memcpy(tmp_row, ref_row, row_bytes);
  memcpy(ref_row, dst_row, row_bytes);
  return 1;
}

static void restore_ref_row(int buf_flags, int block_width,
                            const uint8_t *tmp_row, uint8_t *ref_row) {
  int row_bytes = block_width;
#if CONFIG_HIGHBITDEPTH
  if (buf_flags & YV12_FLAG_HIGHBITDEPTH) {
    row_bytes *= 2;
    ref_row = (uint8_t *)CONVERT_TO_SHORTPTR(ref_row);
  }
#else
  (void)buf_flags;
#endif  // CONFIG_HIGHBITDEPTH

  memcpy(ref_row, tmp_row, row_bytes);
}

// The column equivalent of overwrite_ref_row. ref_row and dst_row
// point at the relevant column of the first row of the block.
static int overwrite_ref_col(int buf_flags, int block_height,
                             const uint8_t *dst_row, int dst_stride,
                             uint8_t *ref_row, int ref_stride,
                             uint8_t *tmp_row) {
  if (ref_row == dst_row && ref_stride == dst_stride) return 0;

#if CONFIG_HIGHBITDEPTH
  if (buf_flags & YV12_FLAG_HIGHBITDEPTH) {
    uint16_t *tmp_16 = (uint16_t *)tmp_row;
    uint16_t *ref_16 = CONVERT_TO_SHORTPTR(ref_row);
    const uint16_t *dst_16 = CONVERT_TO_SHORTPTR(dst_row);

    for (int i = 0; i < block_height; ++i) {
      tmp_16[i] = ref_16[i * ref_stride];
      ref_16[i * ref_stride] = dst_16[i * dst_stride];
    }
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    for (int i = 0; i < block_height; ++i) {
      tmp_row[i] = ref_row[i * ref_stride];
      ref_row[i * ref_stride] = dst_row[i * dst_stride];
    }
#if CONFIG_HIGHBITDEPTH
  }
#else
  (void)buf_flags;
#endif  // CONFIG_HIGHBITDEPTH
  return 1;
}

static void restore_ref_col(int buf_flags, int block_height,
                            const uint8_t *tmp_row, uint8_t *ref_row,
                            int ref_stride) {
#if CONFIG_HIGHBITDEPTH
  if (buf_flags & YV12_FLAG_HIGHBITDEPTH) {
    const uint16_t *tmp_16 = (const uint16_t *)tmp_row;
    uint16_t *ref_16 = CONVERT_TO_SHORTPTR(ref_row);

    for (int i = 0; i < block_height; ++i) {
      ref_16[i * ref_stride] = tmp_16[i];
    }
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    for (int i = 0; i < block_height; ++i) {
      ref_row[i * ref_stride] = tmp_row[i];
    }
#if CONFIG_HIGHBITDEPTH
  }
#else
  (void)buf_flags;
#endif  // CONFIG_HIGHBITDEPTH
}
#endif  // #if INTRA_USES_EXT_TRANSFORMS

void av1_predict_intra_block(const AV1_COMMON *cm, const MACROBLOCKD *xd,
                             int wpx, int hpx, BLOCK_SIZE bsize,
                             PREDICTION_MODE mode, const uint8_t *ref,
                             int ref_stride, uint8_t *dst, int dst_stride,
                             int col_off, int row_off, int plane) {
  const int block_width = block_size_wide[bsize];
  const int block_height = block_size_high[bsize];
#if INTRA_USES_RECT_TRANSFORMS
  const TX_SIZE tx_size = max_txsize_rect_lookup[bsize];
  assert(tx_size < TX_SIZES_ALL);
#else
  const TX_SIZE tx_size = max_txsize_lookup[bsize];
  assert(tx_size < TX_SIZES);
#endif  // INTRA_USES_RECT_TRANSFORMS

  // Start by running the helper to predict either the entire block
  // (if the block is square or the same size as tx_size) or the top
  // or left of the block if it's tall and thin or short and wide.
  predict_intra_block_helper(cm, xd, wpx, hpx, tx_size, mode, ref, ref_stride,
                             dst, dst_stride, col_off, row_off, plane);

// If we're not using extended transforms, this function should
// always be called with a square block.
#if !INTRA_USES_EXT_TRANSFORMS
  assert(block_width == block_height);
#endif  // !INTRA_USES_EXT_TRANSFORMS

  // If the block is square, we're done.
  if (block_width == block_height) return;

#if INTRA_USES_EXT_TRANSFORMS
// If we're using rectangular transforms, we might be done even
// though the block isn't square.
#if INTRA_USES_RECT_TRANSFORMS
  if (block_width == tx_size_wide[tx_size] &&
      block_height == tx_size_high[tx_size])
    return;

  // A block should only fail to have a matching transform if it's
  // large and rectangular (such large transform sizes aren't
  // available).
  assert(block_width >= 32 && block_height >= 32);
#endif  // INTRA_USES_RECT_TRANSFORMS

  assert((block_width == wpx && block_height == hpx) ||
         (block_width == (wpx >> 1) && block_height == hpx) ||
         (block_width == wpx && block_height == (hpx >> 1)));

// The tmp buffer needs to be big enough to hold MAX_SB_SIZE samples
// from the image. If CONFIG_HIGHBITDEPTH is enabled, it also needs
// to be big enough and correctly aligned to hold 16-bit entries.
#if CONFIG_HIGHBITDEPTH
  uint16_t tmp_buf[MAX_SB_SIZE];
#else
  uint8_t tmp_buf[MAX_SB_SIZE];
#endif  // CONFIG_HIGHBITDEPTH
  uint8_t *tmp = (uint8_t *)tmp_buf;

  if (block_width < block_height) {
    // The block is tall and thin. We've already done the top part,
    // and need to repeat the prediction down the rest of the block.

    const int tx_height = tx_size_high[tx_size];
    const int tx_height_off = tx_height >> tx_size_wide_log2[0];
    assert(tx_height_off << tx_size_wide_log2[0] == tx_height);

    int next_row_off = row_off + tx_height_off;
    int next_row_idx = tx_height;

    while (next_row_idx < block_height) {
      const int last_row_idx = next_row_idx - 1;

      // Cast away the const to make a mutable pointer to the last
      // row of ref. This will be snapshotted and restored later.
      uint8_t *last_ref_row = (uint8_t *)ref + last_row_idx * ref_stride;
      uint8_t *last_dst_row = dst + last_row_idx * dst_stride;

      const int needs_restore =
          overwrite_ref_row(ref_stride == dst_stride, xd->cur_buf->flags,
                            block_width, last_dst_row, last_ref_row, tmp);

      const uint8_t *next_ref_row = ref + next_row_idx * ref_stride;
      uint8_t *next_dst_row = dst + next_row_idx * dst_stride;

      predict_intra_block_helper(cm, xd, wpx, hpx, tx_size, mode, next_ref_row,
                                 ref_stride, next_dst_row, dst_stride, col_off,
                                 next_row_off, plane);

      if (needs_restore)
        restore_ref_row(xd->cur_buf->flags, block_width, tmp, last_ref_row);

      next_row_idx += tx_height;
      next_row_off += tx_height_off;
    }
  } else {
    // The block is short and wide. We've already done the left part,
    // and need to repeat the prediction to the right.

    const int tx_width = tx_size_wide[tx_size];
    const int tx_width_off = tx_width >> tx_size_wide_log2[0];
    assert(tx_width_off << tx_size_wide_log2[0] == tx_width);

    int next_col_off = col_off + tx_width_off;
    int next_col_idx = tx_width;

    while (next_col_idx < block_width) {
      const int last_col_idx = next_col_idx - 1;

      // Cast away the const to make a mutable pointer to ref,
      // starting at the last column written. This will be
      // snapshotted and restored later.
      uint8_t *last_ref_col = (uint8_t *)ref + last_col_idx;
      uint8_t *last_dst_col = dst + last_col_idx;

      const int needs_restore =
          overwrite_ref_col(xd->cur_buf->flags, block_height, last_dst_col,
                            dst_stride, last_ref_col, ref_stride, tmp);

      const uint8_t *next_ref_col = ref + next_col_idx;
      uint8_t *next_dst_col = dst + next_col_idx;

      predict_intra_block_helper(cm, xd, wpx, hpx, tx_size, mode, next_ref_col,
                                 ref_stride, next_dst_col, dst_stride,
                                 next_col_off, row_off, plane);

      if (needs_restore)
        restore_ref_col(xd->cur_buf->flags, block_height, tmp, last_ref_col,
                        ref_stride);

      next_col_idx += tx_width;
      next_col_off += tx_width_off;
    }
  }
#endif  // INTRA_USES_EXT_TRANSFORMS
}

void av1_init_intra_predictors(void) {
  once(av1_init_intra_predictors_internal);
}
