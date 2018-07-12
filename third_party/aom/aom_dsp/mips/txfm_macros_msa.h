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

#ifndef AOM_DSP_MIPS_TXFM_MACROS_MIPS_MSA_H_
#define AOM_DSP_MIPS_TXFM_MACROS_MIPS_MSA_H_

#include "aom_dsp/mips/macros_msa.h"

#define DOTP_CONST_PAIR(reg0, reg1, cnst0, cnst1, out0, out1) \
  {                                                           \
    v8i16 k0_m = __msa_fill_h(cnst0);                         \
    v4i32 s0_m, s1_m, s2_m, s3_m;                             \
                                                              \
    s0_m = (v4i32)__msa_fill_h(cnst1);                        \
    k0_m = __msa_ilvev_h((v8i16)s0_m, k0_m);                  \
                                                              \
    ILVRL_H2_SW((-reg1), reg0, s1_m, s0_m);                   \
    ILVRL_H2_SW(reg0, reg1, s3_m, s2_m);                      \
    DOTP_SH2_SW(s1_m, s0_m, k0_m, k0_m, s1_m, s0_m);          \
    SRARI_W2_SW(s1_m, s0_m, DCT_CONST_BITS);                  \
    out0 = __msa_pckev_h((v8i16)s0_m, (v8i16)s1_m);           \
                                                              \
    DOTP_SH2_SW(s3_m, s2_m, k0_m, k0_m, s1_m, s0_m);          \
    SRARI_W2_SW(s1_m, s0_m, DCT_CONST_BITS);                  \
    out1 = __msa_pckev_h((v8i16)s0_m, (v8i16)s1_m);           \
  }

#define DOT_ADD_SUB_SRARI_PCK(in0, in1, in2, in3, in4, in5, in6, in7, dst0,   \
                              dst1, dst2, dst3)                               \
  {                                                                           \
    v4i32 tp0_m, tp1_m, tp2_m, tp3_m, tp4_m;                                  \
    v4i32 tp5_m, tp6_m, tp7_m, tp8_m, tp9_m;                                  \
                                                                              \
    DOTP_SH4_SW(in0, in1, in0, in1, in4, in4, in5, in5, tp0_m, tp2_m, tp3_m,  \
                tp4_m);                                                       \
    DOTP_SH4_SW(in2, in3, in2, in3, in6, in6, in7, in7, tp5_m, tp6_m, tp7_m,  \
                tp8_m);                                                       \
    BUTTERFLY_4(tp0_m, tp3_m, tp7_m, tp5_m, tp1_m, tp9_m, tp7_m, tp5_m);      \
    BUTTERFLY_4(tp2_m, tp4_m, tp8_m, tp6_m, tp3_m, tp0_m, tp4_m, tp2_m);      \
    SRARI_W4_SW(tp1_m, tp9_m, tp7_m, tp5_m, DCT_CONST_BITS);                  \
    SRARI_W4_SW(tp3_m, tp0_m, tp4_m, tp2_m, DCT_CONST_BITS);                  \
    PCKEV_H4_SH(tp1_m, tp3_m, tp9_m, tp0_m, tp7_m, tp4_m, tp5_m, tp2_m, dst0, \
                dst1, dst2, dst3);                                            \
  }

#define DOT_SHIFT_RIGHT_PCK_H(in0, in1, in2)           \
  ({                                                   \
    v8i16 dst_m;                                       \
    v4i32 tp0_m, tp1_m;                                \
                                                       \
    DOTP_SH2_SW(in0, in1, in2, in2, tp1_m, tp0_m);     \
    SRARI_W2_SW(tp1_m, tp0_m, DCT_CONST_BITS);         \
    dst_m = __msa_pckev_h((v8i16)tp1_m, (v8i16)tp0_m); \
                                                       \
    dst_m;                                             \
  })

#define MADD_SHORT(m0, m1, c0, c1, res0, res1)                              \
  {                                                                         \
    v4i32 madd0_m, madd1_m, madd2_m, madd3_m;                               \
    v8i16 madd_s0_m, madd_s1_m;                                             \
                                                                            \
    ILVRL_H2_SH(m1, m0, madd_s0_m, madd_s1_m);                              \
    DOTP_SH4_SW(madd_s0_m, madd_s1_m, madd_s0_m, madd_s1_m, c0, c0, c1, c1, \
                madd0_m, madd1_m, madd2_m, madd3_m);                        \
    SRARI_W4_SW(madd0_m, madd1_m, madd2_m, madd3_m, DCT_CONST_BITS);        \
    PCKEV_H2_SH(madd1_m, madd0_m, madd3_m, madd2_m, res0, res1);            \
  }

#define MADD_BF(inp0, inp1, inp2, inp3, cst0, cst1, cst2, cst3, out0, out1,   \
                out2, out3)                                                   \
  {                                                                           \
    v8i16 madd_s0_m, madd_s1_m, madd_s2_m, madd_s3_m;                         \
    v4i32 tmp0_m, tmp1_m, tmp2_m, tmp3_m, m4_m, m5_m;                         \
                                                                              \
    ILVRL_H2_SH(inp1, inp0, madd_s0_m, madd_s1_m);                            \
    ILVRL_H2_SH(inp3, inp2, madd_s2_m, madd_s3_m);                            \
    DOTP_SH4_SW(madd_s0_m, madd_s1_m, madd_s2_m, madd_s3_m, cst0, cst0, cst2, \
                cst2, tmp0_m, tmp1_m, tmp2_m, tmp3_m);                        \
    BUTTERFLY_4(tmp0_m, tmp1_m, tmp3_m, tmp2_m, m4_m, m5_m, tmp3_m, tmp2_m);  \
    SRARI_W4_SW(m4_m, m5_m, tmp2_m, tmp3_m, DCT_CONST_BITS);                  \
    PCKEV_H2_SH(m5_m, m4_m, tmp3_m, tmp2_m, out0, out1);                      \
    DOTP_SH4_SW(madd_s0_m, madd_s1_m, madd_s2_m, madd_s3_m, cst1, cst1, cst3, \
                cst3, tmp0_m, tmp1_m, tmp2_m, tmp3_m);                        \
    BUTTERFLY_4(tmp0_m, tmp1_m, tmp3_m, tmp2_m, m4_m, m5_m, tmp3_m, tmp2_m);  \
    SRARI_W4_SW(m4_m, m5_m, tmp2_m, tmp3_m, DCT_CONST_BITS);                  \
    PCKEV_H2_SH(m5_m, m4_m, tmp3_m, tmp2_m, out2, out3);                      \
  }
#endif  // AOM_DSP_MIPS_TXFM_MACROS_MIPS_MSA_H_
