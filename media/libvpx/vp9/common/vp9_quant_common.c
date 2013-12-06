/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vp9/common/vp9_common.h"
#include "vp9/common/vp9_quant_common.h"
#include "vp9/common/vp9_seg_common.h"

#if 1
static const int16_t dc_qlookup[QINDEX_RANGE] = {
  4,       8,    8,    9,   10,   11,   12,   12,
  13,     14,   15,   16,   17,   18,   19,   19,
  20,     21,   22,   23,   24,   25,   26,   26,
  27,     28,   29,   30,   31,   32,   32,   33,
  34,     35,   36,   37,   38,   38,   39,   40,
  41,     42,   43,   43,   44,   45,   46,   47,
  48,     48,   49,   50,   51,   52,   53,   53,
  54,     55,   56,   57,   57,   58,   59,   60,
  61,     62,   62,   63,   64,   65,   66,   66,
  67,     68,   69,   70,   70,   71,   72,   73,
  74,     74,   75,   76,   77,   78,   78,   79,
  80,     81,   81,   82,   83,   84,   85,   85,
  87,     88,   90,   92,   93,   95,   96,   98,
  99,    101,  102,  104,  105,  107,  108,  110,
  111,   113,  114,  116,  117,  118,  120,  121,
  123,   125,  127,  129,  131,  134,  136,  138,
  140,   142,  144,  146,  148,  150,  152,  154,
  156,   158,  161,  164,  166,  169,  172,  174,
  177,   180,  182,  185,  187,  190,  192,  195,
  199,   202,  205,  208,  211,  214,  217,  220,
  223,   226,  230,  233,  237,  240,  243,  247,
  250,   253,  257,  261,  265,  269,  272,  276,
  280,   284,  288,  292,  296,  300,  304,  309,
  313,   317,  322,  326,  330,  335,  340,  344,
  349,   354,  359,  364,  369,  374,  379,  384,
  389,   395,  400,  406,  411,  417,  423,  429,
  435,   441,  447,  454,  461,  467,  475,  482,
  489,   497,  505,  513,  522,  530,  539,  549,
  559,   569,  579,  590,  602,  614,  626,  640,
  654,   668,  684,  700,  717,  736,  755,  775,
  796,   819,  843,  869,  896,  925,  955,  988,
  1022, 1058, 1098, 1139, 1184, 1232, 1282, 1336,
};

static const int16_t ac_qlookup[QINDEX_RANGE] = {
  4,       8,    9,   10,   11,   12,   13,   14,
  15,     16,   17,   18,   19,   20,   21,   22,
  23,     24,   25,   26,   27,   28,   29,   30,
  31,     32,   33,   34,   35,   36,   37,   38,
  39,     40,   41,   42,   43,   44,   45,   46,
  47,     48,   49,   50,   51,   52,   53,   54,
  55,     56,   57,   58,   59,   60,   61,   62,
  63,     64,   65,   66,   67,   68,   69,   70,
  71,     72,   73,   74,   75,   76,   77,   78,
  79,     80,   81,   82,   83,   84,   85,   86,
  87,     88,   89,   90,   91,   92,   93,   94,
  95,     96,   97,   98,   99,  100,  101,  102,
  104,   106,  108,  110,  112,  114,  116,  118,
  120,   122,  124,  126,  128,  130,  132,  134,
  136,   138,  140,  142,  144,  146,  148,  150,
  152,   155,  158,  161,  164,  167,  170,  173,
  176,   179,  182,  185,  188,  191,  194,  197,
  200,   203,  207,  211,  215,  219,  223,  227,
  231,   235,  239,  243,  247,  251,  255,  260,
  265,   270,  275,  280,  285,  290,  295,  300,
  305,   311,  317,  323,  329,  335,  341,  347,
  353,   359,  366,  373,  380,  387,  394,  401,
  408,   416,  424,  432,  440,  448,  456,  465,
  474,   483,  492,  501,  510,  520,  530,  540,
  550,   560,  571,  582,  593,  604,  615,  627,
  639,   651,  663,  676,  689,  702,  715,  729,
  743,   757,  771,  786,  801,  816,  832,  848,
  864,   881,  898,  915,  933,  951,  969,  988,
  1007, 1026, 1046, 1066, 1087, 1108, 1129, 1151,
  1173, 1196, 1219, 1243, 1267, 1292, 1317, 1343,
  1369, 1396, 1423, 1451, 1479, 1508, 1537, 1567,
  1597, 1628, 1660, 1692, 1725, 1759, 1793, 1828,
};

void vp9_init_quant_tables(void) { }
#else
static int16_t dc_qlookup[QINDEX_RANGE];
static int16_t ac_qlookup[QINDEX_RANGE];

#define ACDC_MIN 8

// TODO(dkovalev) move to common and reuse
static double poly3(double a, double b, double c, double d, double x) {
  return a*x*x*x + b*x*x + c*x + d;
}

void vp9_init_quant_tables() {
  int i, val = 4;

  // A "real" q of 1.0 forces lossless mode.
  // In practice non lossless Q's between 1.0 and 2.0 (represented here by
  // integer values from 5-7 give poor rd results (lower psnr and often
  // larger size than the lossless encode. To block out those "not very useful"
  // values we increment the ac and dc q lookup values by 4 after position 0.
  ac_qlookup[0] = val;
  dc_qlookup[0] = val;
  val += 4;

  for (i = 1; i < QINDEX_RANGE; i++) {
    const int ac_val = val;

    val = (int)(val * 1.01975);
    if (val == ac_val)
      ++val;

    ac_qlookup[i] = (int16_t)ac_val;
    dc_qlookup[i] = (int16_t)MAX(ACDC_MIN, poly3(0.000000305, -0.00065, 0.9,
                                                 0.5, ac_val));
  }
}
#endif

int16_t vp9_dc_quant(int qindex, int delta) {
  return dc_qlookup[clamp(qindex + delta, 0, MAXQ)];
}

int16_t vp9_ac_quant(int qindex, int delta) {
  return ac_qlookup[clamp(qindex + delta, 0, MAXQ)];
}


int vp9_get_qindex(struct segmentation *seg, int segment_id, int base_qindex) {
  if (vp9_segfeature_active(seg, segment_id, SEG_LVL_ALT_Q)) {
    const int data = vp9_get_segdata(seg, segment_id, SEG_LVL_ALT_Q);
    return seg->abs_delta == SEGMENT_ABSDATA ?
                             data :  // Abs value
                             clamp(base_qindex + data, 0, MAXQ);  // Delta value
  } else {
    return base_qindex;
  }
}

