/*
 * Copyright © 2023, VideoLAN and dav1d authors
 * Copyright © 2023, Loongson Technology Corporation Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "src/loongarch/loongson_asm.S"

#define REST_UNIT_STRIDE (400)

.macro MADD_HU_BU in0, in1, out0, out1
    vsllwil.hu.bu vr12,     \in0,     0
    vexth.hu.bu   vr13,     \in0
    vmadd.h       \out0,    vr12,     \in1
    vmadd.h       \out1,    vr13,     \in1
.endm

const wiener_shuf
.byte 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18
endconst

/*
void wiener_filter_h_lsx(int32_t *hor_ptr,
                         uint8_t *tmp_ptr,
                         const int16_t filterh[8],
                         const int w, const int h)
*/
function wiener_filter_h_8bpc_lsx
    addi.d        sp,       sp,       -40
    fst.d         f24,      sp,       0
    fst.d         f25,      sp,       8
    fst.d         f26,      sp,       16
    fst.d         f27,      sp,       24
    fst.d         f28,      sp,       32
    li.w          t7,       1<<14          // clip_limit

    la.local      t1,       wiener_shuf
    vld           vr4,      t1,       0
    vld           vr14,     a2,       0    // filter[0][k]
    vreplvei.h    vr21,     vr14,     0
    vreplvei.h    vr22,     vr14,     1
    vreplvei.h    vr23,     vr14,     2
    vreplvei.h    vr24,     vr14,     3
    vreplvei.h    vr25,     vr14,     4
    vreplvei.h    vr26,     vr14,     5
    vreplvei.h    vr27,     vr14,     6
    vreplgr2vr.w  vr0,      t7

.WIENER_FILTER_H_H:
    addi.w        a4,       a4,       -1    // h
    addi.w        t0,       a3,       0     // w
    addi.d        t1,       a1,       0     // tmp_ptr
    addi.d        t2,       a0,       0     // hor_ptr

.WIENER_FILTER_H_W:
    addi.w        t0,       t0,       -16
    vld           vr5,      t1,       0
    vld           vr13,     t1,       16

    vsubi.bu      vr14,     vr4,      2
    vsubi.bu      vr15,     vr4,      1
    vshuf.b       vr6,      vr13,     vr5,     vr14  // 1 ... 8, 9 ... 16
    vshuf.b       vr7,      vr13,     vr5,     vr15  // 2 ... 9, 10 ... 17
    vshuf.b       vr8,      vr13,     vr5,     vr4   // 3 ... 10, 11 ... 18
    vaddi.bu      vr14,     vr4,      1
    vaddi.bu      vr15,     vr4,      2
    vshuf.b       vr9,      vr13,     vr5,     vr14  // 4 ... 11, 12 ... 19
    vshuf.b       vr10,     vr13,     vr5,     vr15  // 5 ... 12, 13 ... 20
    vaddi.bu      vr14,     vr4,      3
    vshuf.b       vr11,     vr13,     vr5,     vr14  // 6 ... 13, 14 ... 21

    vsllwil.hu.bu vr15,     vr8,      0    //  3  4  5  6  7  8  9 10
    vexth.hu.bu   vr16,     vr8            // 11 12 13 14 15 16 17 18
    vsllwil.wu.hu vr17,     vr15,     0    //  3  4  5  6
    vexth.wu.hu   vr18,     vr15           //  7  8  9 10
    vsllwil.wu.hu vr19,     vr16,     0    // 11 12 13 14
    vexth.wu.hu   vr20,     vr16           // 15 16 17 18
    vslli.w       vr17,     vr17,     7
    vslli.w       vr18,     vr18,     7
    vslli.w       vr19,     vr19,     7
    vslli.w       vr20,     vr20,     7
    vxor.v        vr15,     vr15,     vr15
    vxor.v        vr14,     vr14,     vr14

    MADD_HU_BU    vr5,   vr21,  vr14,  vr15
    MADD_HU_BU    vr6,   vr22,  vr14,  vr15
    MADD_HU_BU    vr7,   vr23,  vr14,  vr15
    MADD_HU_BU    vr8,   vr24,  vr14,  vr15
    MADD_HU_BU    vr9,   vr25,  vr14,  vr15
    MADD_HU_BU    vr10,  vr26,  vr14,  vr15
    MADD_HU_BU    vr11,  vr27,  vr14,  vr15

    vsllwil.w.h   vr5,      vr14,     0   //  0  1  2  3
    vexth.w.h     vr6,      vr14          //  4  5  6  7
    vsllwil.w.h   vr7,      vr15,     0   //  8  9 10 11
    vexth.w.h     vr8,      vr15          // 12 13 14 15
    vadd.w        vr17,     vr17,     vr5
    vadd.w        vr18,     vr18,     vr6
    vadd.w        vr19,     vr19,     vr7
    vadd.w        vr20,     vr20,     vr8
    vadd.w        vr17,     vr17,     vr0
    vadd.w        vr18,     vr18,     vr0
    vadd.w        vr19,     vr19,     vr0
    vadd.w        vr20,     vr20,     vr0

    vsrli.w       vr1,      vr0,      1
    vsubi.wu      vr1,      vr1,      1
    vxor.v        vr3,      vr3,      vr3
    vsrari.w      vr17,     vr17,     3
    vsrari.w      vr18,     vr18,     3
    vsrari.w      vr19,     vr19,     3
    vsrari.w      vr20,     vr20,     3
    vclip.w       vr17,     vr17,     vr3,     vr1
    vclip.w       vr18,     vr18,     vr3,     vr1
    vclip.w       vr19,     vr19,     vr3,     vr1
    vclip.w       vr20,     vr20,     vr3,     vr1

    vst           vr17,     t2,       0
    vst           vr18,     t2,       16
    vst           vr19,     t2,       32
    vst           vr20,     t2,       48
    addi.d        t1,       t1,       16
    addi.d        t2,       t2,       64
    blt           zero,     t0,       .WIENER_FILTER_H_W

    addi.d        a1,       a1,       REST_UNIT_STRIDE
    addi.d        a0,       a0,       (REST_UNIT_STRIDE << 2)
    bnez          a4,       .WIENER_FILTER_H_H

    fld.d         f24,      sp,       0
    fld.d         f25,      sp,       8
    fld.d         f26,      sp,       16
    fld.d         f27,      sp,       24
    fld.d         f28,      sp,       32
    addi.d        sp,       sp,       40
endfunc

.macro APPLY_FILTER in0, in1, in2
    alsl.d         t7,      \in0,     \in1,    2
    vld            vr10,    t7,       0
    vld            vr11,    t7,       16
    vld            vr12,    t7,       32
    vld            vr13,    t7,       48
    vmadd.w        vr14,    vr10,     \in2
    vmadd.w        vr15,    vr11,     \in2
    vmadd.w        vr16,    vr12,     \in2
    vmadd.w        vr17,    vr13,     \in2
.endm

.macro wiener_filter_v_8bpc_core_lsx
    vreplgr2vr.w  vr14,     t6
    vreplgr2vr.w  vr15,     t6
    vreplgr2vr.w  vr16,     t6
    vreplgr2vr.w  vr17,     t6

    addi.w        t7,       t2,       0      // j + index k
    mul.w         t7,       t7,       t8     // (j + index) * REST_UNIT_STRIDE
    add.w         t7,       t7,       t4     // (j + index) * REST_UNIT_STRIDE + i

    APPLY_FILTER  t7, a2, vr2
    APPLY_FILTER  t8, t7, vr3
    APPLY_FILTER  t8, t7, vr4
    APPLY_FILTER  t8, t7, vr5
    APPLY_FILTER  t8, t7, vr6
    APPLY_FILTER  t8, t7, vr7
    APPLY_FILTER  t8, t7, vr8
    vssrarni.hu.w vr15,     vr14,     11
    vssrarni.hu.w vr17,     vr16,     11
    vssrlni.bu.h  vr17,     vr15,     0
.endm

/*
void wiener_filter_v_lsx(uint8_t *p,
                         const ptrdiff_t p_stride,
                         const int32_t *hor,
                         const int16_t filterv[8],
                         const int w, const int h)
*/
function wiener_filter_v_8bpc_lsx
    li.w          t6,       -(1 << 18)

    li.w          t8,       REST_UNIT_STRIDE
    ld.h          t0,       a3,       0
    ld.h          t1,       a3,       2
    vreplgr2vr.w  vr2,      t0
    vreplgr2vr.w  vr3,      t1
    ld.h          t0,       a3,       4
    ld.h          t1,       a3,       6
    vreplgr2vr.w  vr4,      t0
    vreplgr2vr.w  vr5,      t1
    ld.h          t0,       a3,       8
    ld.h          t1,       a3,       10
    vreplgr2vr.w  vr6,      t0
    vreplgr2vr.w  vr7,      t1
    ld.h          t0,       a3,       12
    vreplgr2vr.w  vr8,      t0

    andi          t1,       a4,       0xf
    sub.w         t0,       a4,       t1    // w-w%16
    or            t2,       zero,     zero  // j
    or            t4,       zero,     zero
    beqz          t0,       .WIENER_FILTER_V_W_LT16

.WIENER_FILTER_V_H:
    andi          t1,       a4,       0xf
    add.d         t3,       zero,     a0     // p
    or            t4,       zero,     zero   // i

.WIENER_FILTER_V_W:

    wiener_filter_v_8bpc_core_lsx

    mul.w         t5,       t2,       a1   // j * stride
    add.w         t5,       t5,       t4   // j * stride + i
    add.d         t3,       a0,       t5
    addi.w        t4,       t4,       16
    vst           vr17,     t3,       0
    bne           t0,       t4,       .WIENER_FILTER_V_W

    beqz          t1,       .WIENER_FILTER_V_W_EQ16

    wiener_filter_v_8bpc_core_lsx

    addi.d        t3,       t3,       16
    andi          t1,       a4,       0xf

.WIENER_FILTER_V_ST_REM:
    vstelm.b      vr17,     t3,       0,    0
    vbsrl.v       vr17,     vr17,     1
    addi.d        t3,       t3,       1
    addi.w        t1,       t1,       -1
    bnez          t1,       .WIENER_FILTER_V_ST_REM
.WIENER_FILTER_V_W_EQ16:
    addi.w        t2,       t2,       1
    blt           t2,       a5,       .WIENER_FILTER_V_H
    b              .WIENER_FILTER_V_END

.WIENER_FILTER_V_W_LT16:
    andi          t1,       a4,       0xf
    add.d         t3,       zero,     a0

    wiener_filter_v_8bpc_core_lsx

    mul.w         t5,       t2,       a1   // j * stride
    add.d         t3,       a0,       t5

.WIENER_FILTER_V_ST_REM_1:
    vstelm.b      vr17,     t3,       0,    0
    vbsrl.v       vr17,     vr17,     1
    addi.d        t3,       t3,       1
    addi.w        t1,       t1,       -1
    bnez          t1,       .WIENER_FILTER_V_ST_REM_1

    addi.w        t2,       t2,       1
    blt           t2,       a5,       .WIENER_FILTER_V_W_LT16

.WIENER_FILTER_V_END:
endfunc

/*
void boxsum3_h(int32_t *sumsq, coef *sum, const pixel *src,
               const int w, const int h)
*/
function boxsum3_h_8bpc_lsx
    addi.d         a2,      a2,      REST_UNIT_STRIDE
    li.w           t0,      1
    addi.w         a3,      a3,      -2
    addi.w         a4,      a4,      -4

.LBS3_H_H:
    alsl.d         t1,      t0,      a1,    1     // sum_v    *sum_v = sum + x
    alsl.d         t2,      t0,      a0,    2     // sumsq_v  *sumsq_v = sumsq + x
    add.d          t3,      t0,      a2           // s
    addi.w         t5,      a3,      0
.LBS3_H_W:
    vld            vr0,     t3,      0
    vld            vr1,     t3,      REST_UNIT_STRIDE
    vld            vr2,     t3,      (REST_UNIT_STRIDE<<1)

    vilvl.b        vr3,     vr1,     vr0
    vhaddw.hu.bu   vr4,     vr3,     vr3
    vilvh.b        vr5,     vr1,     vr0
    vhaddw.hu.bu   vr6,     vr5,     vr5
    vsllwil.hu.bu  vr7,     vr2,     0
    vexth.hu.bu    vr8,     vr2
    // sum_v
    vadd.h         vr4,     vr4,     vr7
    vadd.h         vr6,     vr6,     vr8
    vst            vr4,     t1,      REST_UNIT_STRIDE<<1
    vst            vr6,     t1,      (REST_UNIT_STRIDE<<1)+16
    addi.d         t1,      t1,      32
    // sumsq
    vmulwev.h.bu   vr9,     vr3,     vr3
    vmulwod.h.bu   vr10,    vr3,     vr3
    vmulwev.h.bu   vr11,    vr5,     vr5
    vmulwod.h.bu   vr12,    vr5,     vr5
    vmul.h         vr7,     vr7,     vr7
    vmul.h         vr8,     vr8,     vr8
    vaddwev.w.hu   vr13,    vr10,    vr9
    vaddwod.w.hu   vr14,    vr10,    vr9
    vilvl.w        vr3,     vr14,    vr13
    vilvh.w        vr4,     vr14,    vr13
    vaddwev.w.hu   vr13,    vr12,    vr11
    vaddwod.w.hu   vr14,    vr12,    vr11
    vilvl.w        vr15,    vr14,    vr13
    vilvh.w        vr16,    vr14,    vr13
    vsllwil.wu.hu  vr9,     vr7,     0
    vexth.wu.hu    vr10,    vr7
    vsllwil.wu.hu  vr11,    vr8,     0
    vexth.wu.hu    vr12,    vr8
    vadd.w         vr9,     vr9,     vr3
    vadd.w         vr10,    vr10,    vr4
    vadd.w         vr11,    vr11,    vr15
    vadd.w         vr12,    vr12,    vr16
    vst            vr9,     t2,      REST_UNIT_STRIDE<<2
    vst            vr10,    t2,      (REST_UNIT_STRIDE<<2)+16
    vst            vr11,    t2,      (REST_UNIT_STRIDE<<2)+32
    vst            vr12,    t2,      (REST_UNIT_STRIDE<<2)+48
    addi.d         t2,      t2,      64

    addi.w         t5,      t5,      -16
    addi.d         t3,      t3,      16
    blt            zero,    t5,      .LBS3_H_W

    addi.d         a0,      a0,      REST_UNIT_STRIDE<<2
    addi.d         a1,      a1,      REST_UNIT_STRIDE<<1
    addi.d         a2,      a2,      REST_UNIT_STRIDE
    addi.d         a4,      a4,      -1
    blt            zero,    a4,      .LBS3_H_H

.LBS3_H_END:
endfunc

/*
void boxsum3_v(int32_t *sumsq, coef *sum,
               const int w, const int h)
*/
function boxsum3_v_8bpc_lsx
    addi.d         a0,      a0,      (REST_UNIT_STRIDE<<2)
    addi.d         a1,      a1,      (REST_UNIT_STRIDE<<1)
    addi.w         a3,      a3,      -4
    addi.w         a2,      a2,      -4

.LBS3_V_H:
    sub.w          t3,      a2,      zero
    addi.d         t0,      a0,      4
    addi.d         t1,      a1,      2
    addi.d         t5,      a0,      8
    addi.d         t6,      a1,      4

    vld            vr0,      t1,      0   // a 0 1 2 3 4 5 6 7
    vld            vr1,      t1,      2   // b 1 2 3 4 5 6 7 8
    vld            vr2,      t1,      4   // c 2 3 4 5 6 7 8 9
    vld            vr3,      t0,      0   // a2 0 1 2 3
    vld            vr4,      t0,      4   // b2 1 2 3 4
    vld            vr5,      t0,      8   // c2 2 3 4 5
    vld            vr6,      t0,      16  //    3 4 5 6
    vld            vr7,      t0,      20  //    4 5 6 7
    vld            vr8,      t0,      24  //    5 6 7 8
    vadd.h         vr9,      vr0,     vr1
    vadd.h         vr9,      vr9,     vr2
    vadd.w         vr10,     vr3,     vr4
    vadd.w         vr10,     vr10,    vr5
    vadd.w         vr11,     vr6,     vr7
    vadd.w         vr11,     vr11,    vr8
    vpickve2gr.h   t7,       vr2,     6
    vpickve2gr.w   t8,       vr8,     2
    vst            vr9,      t6,      0
    vst            vr10,     t5,      0
    vst            vr11,     t5,      16

    addi.d         t1,       t1,      16
    addi.d         t0,       t0,      32
    addi.d         t5,       t5,      32
    addi.d         t6,       t6,      16
    addi.d         t3,       t3,      -8
    ble            t3,       zero,    .LBS3_V_H0

.LBS3_V_W8:
    vld            vr0,      t1,      0   // a 0 1 2 3 4 5 6 7
    vld            vr1,      t1,      2   // b 1 2 3 4 5 6 7 8
    vld            vr2,      t1,      4   // c 2 3 4 5 6 7 8 9
    vld            vr3,      t0,      0   // a2 0 1 2 3
    vld            vr4,      t0,      4   // b2 1 2 3 4
    vld            vr5,      t0,      8   // c2 2 3 4 5
    vld            vr6,      t0,      16  //    3 4 5 6
    vld            vr7,      t0,      20  //    4 5 6 7
    vld            vr8,      t0,      24  //    5 6 7 8
    vinsgr2vr.h    vr0,      t7,      0
    vinsgr2vr.w    vr3,      t8,      0
    vpickve2gr.h   t7,       vr2,     6
    vpickve2gr.w   t8,       vr8,     2
    vadd.h         vr9,      vr0,     vr1
    vadd.w         vr10,     vr3,     vr4
    vadd.w         vr11,     vr6,     vr7
    vadd.h         vr9,      vr9,     vr2
    vadd.w         vr10,     vr10,    vr5
    vadd.w         vr11,     vr11,    vr8
    vst            vr9,      t6,      0
    vst            vr10,     t5,      0
    vst            vr11,     t5,      16
    addi.d         t3,       t3,      -8
    addi.d         t1,       t1,      16
    addi.d         t0,       t0,      32
    addi.d         t5,       t5,      32
    addi.d         t6,       t6,      16
    blt            zero,     t3,       .LBS3_V_W8

.LBS3_V_H0:
    addi.d         a1,       a1,      REST_UNIT_STRIDE<<1
    addi.d         a0,       a0,      REST_UNIT_STRIDE<<2
    addi.w         a3,       a3,      -1
    bnez           a3,       .LBS3_V_H

.LBS3_V_END:
endfunc

/*
boxsum3_selfguided_filter(int32_t *sumsq, coef *sum,
                          const int w, const int h,
                          const unsigned s)
*/
function boxsum3_sgf_h_8bpc_lsx
    addi.d        a0,       a0,        REST_UNIT_STRIDE<<2
    addi.d        a0,       a0,        12   // AA
    addi.d        a1,       a1,        REST_UNIT_STRIDE<<1
    addi.d        a1,       a1,        6    // BB
    la.local      t8,       dav1d_sgr_x_by_x
    li.w          t6,       455
    vreplgr2vr.w  vr20,     t6
    li.w          t6,       255
    vreplgr2vr.w  vr22,     t6
    vaddi.wu      vr21,     vr22,      1  // 256
    vreplgr2vr.w  vr6,      a4
    vldi          vr19,     0x809
    addi.w        a2,       a2,        2  // w + 2
    addi.w        a3,       a3,        2  // h + 2

.LBS3SGF_H_H:
    addi.w        t2,       a2,        0
    addi.d        t0,       a0,        -4
    addi.d        t1,       a1,        -2

.LBS3SGF_H_W:
    addi.w        t2,       t2,        -8
    vld           vr0,      t0,        0   // AA[i]
    vld           vr1,      t0,        16
    vld           vr2,      t1,        0   // BB[i]

    vmul.w        vr4,      vr0,       vr19 // a * n
    vmul.w        vr5,      vr1,       vr19 // a * n
    vsllwil.w.h   vr9,      vr2,       0
    vexth.w.h     vr10,     vr2
    vmsub.w       vr4,      vr9,       vr9   // p
    vmsub.w       vr5,      vr10,      vr10   // p
    vmaxi.w       vr4,      vr4,       0
    vmaxi.w       vr5,      vr5,       0    // p
    vmul.w        vr4,      vr4,       vr6  // p * s
    vmul.w        vr5,      vr5,       vr6  // p * s
    vsrlri.w      vr4,      vr4,       20
    vsrlri.w      vr5,      vr5,       20   // z
    vmin.w        vr4,      vr4,       vr22
    vmin.w        vr5,      vr5,       vr22

    vpickve2gr.w  t6,       vr4,       0
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr7,      t7,        0
    vpickve2gr.w  t6,       vr4,       1
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr7,      t7,        1
    vpickve2gr.w  t6,       vr4,       2
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr7,      t7,        2
    vpickve2gr.w  t6,       vr4,       3
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr7,      t7,        3

    vpickve2gr.w  t6,       vr5,       0
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr8,      t7,        0
    vpickve2gr.w  t6,       vr5,       1
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr8,      t7,        1
    vpickve2gr.w  t6,       vr5,       2
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr8,      t7,        2
    vpickve2gr.w  t6,       vr5,       3
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr8,      t7,        3     // x

    vmul.w        vr9,      vr7,       vr9   // x * BB[i]
    vmul.w        vr10,     vr8,       vr10
    vmul.w        vr9,      vr9,       vr20  // x * BB[i] * sgr_one_by_x
    vmul.w        vr10,     vr10,      vr20
    vsrlri.w      vr9,      vr9,       12
    vsrlri.w      vr10,     vr10,      12
    vsub.w        vr7,      vr21,      vr7
    vsub.w        vr8,      vr21,      vr8
    vpickev.h     vr8,      vr8,       vr7

    vst           vr9,      t0,        0
    vst           vr10,     t0,        16
    vst           vr8,      t1,        0
    addi.d        t0,       t0,        32
    addi.d        t1,       t1,        16
    blt           zero,     t2,        .LBS3SGF_H_W

    addi.d        a0,       a0,        REST_UNIT_STRIDE<<2
    addi.d        a1,       a1,        REST_UNIT_STRIDE<<1
    addi.w        a3,       a3,        -1
    bnez          a3,       .LBS3SGF_H_H
endfunc

/*
boxsum3_selfguided_filter(coef *dst, pixel *src,
                  int32_t *sumsq, coef *sum,
                  const int w, const int h)
*/
function boxsum3_sgf_v_8bpc_lsx
    addi.d        a1,        a1,      (3*REST_UNIT_STRIDE+3)   // src
    addi.d        a2,        a2,      REST_UNIT_STRIDE<<2
    addi.d        a2,        a2,      (REST_UNIT_STRIDE<<2)+12
    addi.d        a3,        a3,      REST_UNIT_STRIDE<<2
    addi.d        a3,        a3,      6
.LBS3SGF_V_H:
    // A int32_t *sumsq
    addi.d        t0,        a2,      -(REST_UNIT_STRIDE<<2)   // -stride
    addi.d        t1,        a2,      0    // sumsq
    addi.d        t2,        a2,      REST_UNIT_STRIDE<<2      // +stride
    addi.d        t6,        a1,      0
    addi.w        t7,        a4,      0
    addi.d        t8,        a0,      0
    // B coef *sum
    addi.d        t3,        a3,      -(REST_UNIT_STRIDE<<1)   // -stride
    addi.d        t4,        a3,      0
    addi.d        t5,        a3,      REST_UNIT_STRIDE<<1

.LBS3SGF_V_W:
    vld           vr0,       t0,      0   // P[i - REST_UNIT_STRIDE]
    vld           vr1,       t0,      16
    vld           vr2,       t1,      -4  // P[i-1]
    vld           vr3,       t1,      12
    vld           vr4,       t2,      0   // P[i + REST_UNIT_STRIDE]
    vld           vr5,       t2,      16
    vld           vr6,       t1,      0   // p[i]
    vld           vr7,       t1,      16
    vld           vr8,       t1,      4   // p[i+1]
    vld           vr9,       t1,      20

    vld           vr10,      t0,      -4  // P[i - 1 - REST_UNIT_STRIDE]
    vld           vr11,      t0,      12
    vld           vr12,      t2,      -4  // P[i - 1 + REST_UNIT_STRIDE]
    vld           vr13,      t2,      12
    vld           vr14,      t0,      4   // P[i + 1 - REST_UNIT_STRIDE]
    vld           vr15,      t0,      20
    vld           vr16,      t2,      4   // P[i + 1 + REST_UNIT_STRIDE]
    vld           vr17,      t2,      20

    vadd.w        vr0,       vr2,     vr0
    vadd.w        vr4,       vr6,     vr4
    vadd.w        vr0,       vr0,     vr8
    vadd.w        vr20,      vr0,     vr4
    vslli.w       vr20,      vr20,    2      // 0 1 2 3
    vadd.w        vr0,       vr1,     vr3
    vadd.w        vr4,       vr5,     vr7
    vadd.w        vr0,       vr0,     vr9
    vadd.w        vr21,      vr0,     vr4
    vslli.w       vr21,      vr21,    2      // 4 5 6 7
    vadd.w        vr12,      vr10,    vr12
    vadd.w        vr16,      vr14,    vr16
    vadd.w        vr22,      vr12,    vr16
    vslli.w       vr23,      vr22,    1
    vadd.w        vr22,      vr23,    vr22
    vadd.w        vr11,      vr11,    vr13
    vadd.w        vr15,      vr15,    vr17
    vadd.w        vr0,       vr11,    vr15
    vslli.w       vr23,      vr0,     1
    vadd.w        vr23,      vr23,    vr0
    vadd.w        vr20,      vr20,    vr22   // b
    vadd.w        vr21,      vr21,    vr23

    // B coef *sum
    vld           vr0,       t3,      0   // P[i - REST_UNIT_STRIDE]
    vld           vr1,       t4,      -2  // p[i - 1]
    vld           vr2,       t4,      0   // p[i]
    vld           vr3,       t4,      2   // p[i + 1]
    vld           vr4,       t5,      0   // P[i + REST_UNIT_STRIDE]
    vld           vr5,       t3,      -2  // P[i - 1 - REST_UNIT_STRIDE]
    vld           vr6,       t5,      -2  // P[i - 1 + REST_UNIT_STRIDE]
    vld           vr7,       t3,      2   // P[i + 1 - REST_UNIT_STRIDE]
    vld           vr8,       t5,      2   // P[i + 1 + REST_UNIT_STRIDE]
    vaddwev.w.h   vr9,       vr0,     vr1
    vaddwod.w.h   vr10,      vr0,     vr1
    vaddwev.w.h   vr11,      vr2,     vr3
    vaddwod.w.h   vr12,      vr2,     vr3
    vadd.w        vr9,       vr11,    vr9
    vadd.w        vr10,      vr12,    vr10
    vilvl.w       vr11,      vr10,    vr9    // 0 1 2 3
    vilvh.w       vr12,      vr10,    vr9    // 4 5 6 7
    vsllwil.w.h   vr0,       vr4,     0
    vexth.w.h     vr1,       vr4
    vadd.w        vr0,       vr11,    vr0
    vadd.w        vr1,       vr12,    vr1
    vslli.w       vr0,       vr0,     2
    vslli.w       vr1,       vr1,     2
    vaddwev.w.h   vr9,       vr5,     vr6
    vaddwod.w.h   vr10,      vr5,     vr6
    vaddwev.w.h   vr11,      vr7,     vr8
    vaddwod.w.h   vr12,      vr7,     vr8
    vadd.w        vr9,       vr11,    vr9
    vadd.w        vr10,      vr12,    vr10
    vilvl.w       vr13,      vr10,    vr9
    vilvh.w       vr14,      vr10,    vr9
    vslli.w       vr15,      vr13,    1
    vslli.w       vr16,      vr14,    1
    vadd.w        vr15,      vr13,    vr15   // a
    vadd.w        vr16,      vr14,    vr16
    vadd.w        vr22,      vr0,     vr15
    vadd.w        vr23,      vr1,     vr16
    vld           vr0,       t6,      0      // src
    vsllwil.hu.bu vr0,       vr0,     0
    vsllwil.wu.hu vr1,       vr0,     0
    vexth.wu.hu   vr2,       vr0
    vmadd.w       vr20,      vr22,    vr1
    vmadd.w       vr21,      vr23,    vr2
    vssrlrni.h.w  vr21,      vr20,    9
    vst           vr21,      t8,      0
    addi.d        t8,        t8,      16

    addi.d        t0,        t0,      32
    addi.d        t1,        t1,      32
    addi.d        t2,        t2,      32
    addi.d        t3,        t3,      16
    addi.d        t4,        t4,      16
    addi.d        t5,        t5,      16
    addi.d        t6,        t6,      8
    addi.w        t7,        t7,      -8
    blt           zero,      t7,      .LBS3SGF_V_W

    addi.w        a5,        a5,      -1
    addi.d        a0,        a0,      384*2
    addi.d        a1,        a1,      REST_UNIT_STRIDE
    addi.d        a3,        a3,      REST_UNIT_STRIDE<<1
    addi.d        a2,        a2,      REST_UNIT_STRIDE<<2
    bnez          a5,        .LBS3SGF_V_H
endfunc

#define FILTER_OUT_STRIDE (384)

/*
sgr_3x3_finish_c(const pixel *p, const ptrdiff_t stride,
                   const int16_t *dst, const int w1;
                   const int w, const int h);
*/
function sgr_3x3_finish_8bpc_lsx
    vreplgr2vr.w  vr3,     a3            // w1
    andi          t4,      a4,       0x7
    sub.w         t5,      a4,       t4

    beq           zero,    t5,       .LSGR3X3_REM

.LSGR3X3_H:
    addi.d        t0,      a0,       0
    addi.d        t1,      a2,       0
    addi.w        t2,      t5,       0
    andi          t4,      a4,       0x7
.LSGR3X3_W:
    vld           vr0,     t0,       0
    vld           vr1,     t1,       0
    vsllwil.hu.bu vr2,     vr0,      4   // u 8 h
    vsllwil.wu.hu vr4,     vr2,      0   // p
    vexth.wu.hu   vr5,     vr2           // p
    vslli.w       vr6,     vr4,      7
    vslli.w       vr7,     vr5,      7
    vsllwil.w.h   vr8,     vr1,      0   // dst
    vexth.w.h     vr9,     vr1           // dst
    vsub.w        vr8,     vr8,      vr4
    vsub.w        vr9,     vr9,      vr5
    vmadd.w       vr6,     vr8,      vr3  // v 0 - 3
    vmadd.w       vr7,     vr9,      vr3  // v 4 - 7
    vssrarni.hu.w vr7,     vr6,      11
    vssrlni.bu.h  vr7,     vr7,      0
    vstelm.d      vr7,     t0,       0,    0
    addi.d        t0,      t0,       8
    addi.d        t1,      t1,       16
    addi.d        t2,      t2,       -8
    bne           zero,    t2,       .LSGR3X3_W

    beq           t4,      zero,     .LSGR3X3_NOREM

    vld           vr0,     t0,       0
    vld           vr1,     t1,       0
    vsllwil.hu.bu vr2,     vr0,      4   // u 8 h
    vsllwil.wu.hu vr4,     vr2,      0   // p
    vexth.wu.hu   vr5,     vr2           // p
    vslli.w       vr6,     vr4,      7
    vslli.w       vr7,     vr5,      7
    vsllwil.w.h   vr8,     vr1,      0   // dst
    vexth.w.h     vr9,     vr1           // dst
    vsub.w        vr8,     vr8,      vr4
    vsub.w        vr9,     vr9,      vr5
    vmadd.w       vr6,     vr8,      vr3  // v 0 - 3
    vmadd.w       vr7,     vr9,      vr3  // v 4 - 7
    vssrarni.hu.w vr7,     vr6,      11
    vssrlni.bu.h  vr7,     vr7,      0

.LSGR3X3_ST:
    vstelm.b      vr7,     t0,       0,    0
    addi.d        t0,      t0,       1
    vbsrl.v       vr7,     vr7,      1
    addi.w        t4,      t4,       -1
    bnez          t4,      .LSGR3X3_ST

.LSGR3X3_NOREM:
    addi.w        a5,      a5,       -1
    add.d         a0,      a0,       a1
    addi.d        a2,      a2,       (FILTER_OUT_STRIDE<<1)
    bnez          a5,      .LSGR3X3_H
    b             .LSGR3X3_END

.LSGR3X3_REM:
    andi          t4,      a4,       0x7
    addi.d        t0,      a0,       0
    vld           vr0,     t0,       0
    vld           vr1,     a2,       0
    vsllwil.hu.bu vr2,     vr0,      4   // u 8 h
    vsllwil.wu.hu vr4,     vr2,      0   // p
    vexth.wu.hu   vr5,     vr2           // p
    vslli.w       vr6,     vr4,      7
    vslli.w       vr7,     vr5,      7
    vsllwil.w.h   vr8,     vr1,      0   // dst
    vexth.w.h     vr9,     vr1           // dst
    vsub.w        vr8,     vr8,      vr4
    vsub.w        vr9,     vr9,      vr5
    vmadd.w       vr6,     vr8,      vr3  // v 0 - 3
    vmadd.w       vr7,     vr9,      vr3  // v 4 - 7
    vssrarni.hu.w vr7,     vr6,      11
    vssrlni.bu.h  vr7,     vr7,      0

.LSGR3X3_REM_ST:
    vstelm.b      vr7,     t0,       0,    0
    addi.d        t0,      t0,       1
    vbsrl.v       vr7,     vr7,      1
    addi.w        t4,      t4,       -1
    bnez          t4,      .LSGR3X3_REM_ST
    addi.w        a5,      a5,       -1
    add.d         a0,      a0,       a1
    addi.d        a2,      a2,       (FILTER_OUT_STRIDE<<1)
    bnez          a5,      .LSGR3X3_REM

.LSGR3X3_END:
endfunc

/*
void boxsum5(int32_t *sumsq, coef *sum,
             const pixel *const src,
             const int w, const int h)
*/
function boxsum5_h_8bpc_lsx
    addi.w        a4,      a4,        -4
    addi.d        a0,      a0,        REST_UNIT_STRIDE<<2
    addi.d        a1,      a1,        REST_UNIT_STRIDE<<1
    li.w          t6,      1
.LBOXSUM5_H_H:
    addi.w        t3,      a3,        0
    addi.d        t2,      a2,        0
    addi.d        t0,      a0,        0
    addi.d        t1,      a1,        0

.LBOXSUM5_H_W:
    vld           vr0,     t2,        0                   // a
    vld           vr1,     t2,        REST_UNIT_STRIDE    // b
    vld           vr2,     t2,        REST_UNIT_STRIDE<<1 // c
    vld           vr3,     t2,        REST_UNIT_STRIDE*3  // d
    vld           vr4,     t2,        REST_UNIT_STRIDE<<2 // e

    vilvl.b       vr5,     vr1,       vr0
    vilvh.b       vr6,     vr1,       vr0
    vilvl.b       vr7,     vr3,       vr2
    vilvh.b       vr8,     vr3,       vr2
    //sum_v
    vhaddw.hu.bu  vr9,     vr5,       vr5  // 0 1  2  3  4  5  6  7
    vhaddw.hu.bu  vr10,    vr6,       vr6  // 8 9 10 11 12 13 14 15  a+b
    vhaddw.hu.bu  vr11,    vr7,       vr7
    vhaddw.hu.bu  vr12,    vr8,       vr8
    vadd.h        vr9,     vr9,       vr11
    vadd.h        vr10,    vr10,      vr12  // a + b + c + d
    vsllwil.hu.bu vr11,    vr4,       0
    vexth.hu.bu   vr12,    vr4
    vadd.h        vr9,     vr9,       vr11
    vadd.h        vr10,    vr10,      vr12
    vst           vr9,     t1,        0
    vst           vr10,    t1,        16
    addi.d        t1,      t1,        32

    // sumsq
    vmulwev.h.bu  vr9,     vr5,       vr5  // a*a 0 1  2  3  4  5  6  7
    vmulwev.h.bu  vr10,    vr6,       vr6  // a*a 8 9 10 11 12 13 14 15
    vmulwod.h.bu  vr13,    vr5,       vr5  // b*b 0 1  2  3  4  5  6  7
    vmulwod.h.bu  vr14,    vr6,       vr6  // b*b 8 9 10 11 12 13 14 15
    vmulwev.h.bu  vr15,    vr7,       vr7  // c*c 0 1  2  3  4  5  6  7
    vmulwev.h.bu  vr16,    vr8,       vr8  // c*c 8 9 10 11 12 13 14 15
    vmulwod.h.bu  vr17,    vr7,       vr7  // d*d 0 1  2  3  4  5  6  7
    vmulwod.h.bu  vr18,    vr8,       vr8  // d*d 8 9 10 11 12 13 14 15
    vaddwev.w.hu  vr5,     vr9,       vr13  // 0 2 4 6
    vaddwod.w.hu  vr6,     vr9,       vr13  // 1 3 5 7
    vaddwev.w.hu  vr7,     vr10,      vr14  // 8 10 12 14
    vaddwod.w.hu  vr8,     vr10,      vr14  // 9 11 13 15   a + b
    vaddwev.w.hu  vr19,    vr15,      vr17  // 0 2 4 6
    vaddwod.w.hu  vr20,    vr15,      vr17  // 1 3 5 7
    vaddwev.w.hu  vr21,    vr16,      vr18  // 8 10 12 14
    vaddwod.w.hu  vr22,    vr16,      vr18  // 9 11 13 15   c + d
    vadd.w        vr5,     vr5,       vr19
    vadd.w        vr6,     vr6,       vr20
    vadd.w        vr7,     vr7,       vr21
    vadd.w        vr8,     vr8,       vr22
    vilvl.w       vr19,    vr6,       vr5
    vilvh.w       vr20,    vr6,       vr5
    vilvl.w       vr21,    vr8,       vr7
    vilvh.w       vr22,    vr8,       vr7
    vmul.h        vr11,    vr11,      vr11
    vmul.h        vr12,    vr12,      vr12
    vsllwil.wu.hu vr0,     vr11,      0
    vexth.wu.hu   vr1,     vr11
    vsllwil.wu.hu vr2,     vr12,      0
    vexth.wu.hu   vr3,     vr12
    vadd.w        vr19,    vr19,      vr0
    vadd.w        vr20,    vr20,      vr1
    vadd.w        vr21,    vr21,      vr2
    vadd.w        vr22,    vr22,      vr3
    vst           vr19,    t0,        0
    vst           vr20,    t0,        16
    vst           vr21,    t0,        32
    vst           vr22,    t0,        48
    addi.d        t0,      t0,        64
    addi.d        t2,      t2,        16
    addi.w        t3,      t3,        -16
    blt           zero,    t3,        .LBOXSUM5_H_W

    addi.d        a0,      a0,        REST_UNIT_STRIDE<<2
    addi.d        a1,      a1,        REST_UNIT_STRIDE<<1
    addi.d        a2,      a2,        REST_UNIT_STRIDE
    addi.d        a4,      a4,        -1
    bnez          a4,      .LBOXSUM5_H_H
endfunc

/*
void boxsum5_h(int32_t *sumsq, coef *sum,
               const int w, const int h)
*/
function boxsum5_v_8bpc_lsx
    addi.d         a0,      a0,      (REST_UNIT_STRIDE<<2)
    addi.d         a1,      a1,      (REST_UNIT_STRIDE<<1)
    addi.w         a3,      a3,      -4
    addi.w         a2,      a2,      -4

.LBOXSUM5_V_H:
    addi.w         t3,      a2,      0
    addi.d         t0,      a0,      0
    addi.d         t1,      a1,      0
    addi.d         t2,      a0,      8
    addi.d         t3,      a1,      4
    addi.d         t4,      a2,      0

    vld            vr0,     t1,      0   // a 0 1 2 3 4 5 6 7
    vld            vr1,     t1,      2   // b 1 2 3 4 5 6 7 8
    vld            vr2,     t1,      4   // c 2
    vld            vr3,     t1,      6   // d 3
    vld            vr4,     t1,      8   // e 4 5 6 7 8 9 10 11
    vadd.h         vr5,     vr0,     vr1
    vadd.h         vr6,     vr2,     vr3
    vpickve2gr.w   t5,      vr4,     2
    vadd.h         vr5,     vr5,     vr6
    vadd.h         vr5,     vr5,     vr4
    vst            vr5,     t3,      0

    vld            vr0,     t0,      0  // 0 1 2 3   a
    vld            vr1,     t0,      4  // 1 2 3 4   b
    vld            vr2,     t0,      8  // 2 3 4 5   c
    vld            vr3,     t0,      12 // 3 4 5 6   d
    vld            vr4,     t0,      16 // 4 5 6 7   e  a
    vld            vr5,     t0,      20 // 5 6 7 8      b
    vld            vr6,     t0,      24 // 6 7 8 9      c
    vld            vr7,     t0,      28 // 7 8 9 10     d
    vld            vr8,     t0,      32 // 8 9 10 11    e

    vadd.w         vr9,     vr0,     vr1
    vadd.w         vr10,    vr2,     vr3
    vadd.w         vr9,     vr9,     vr10
    vadd.w         vr9,     vr9,     vr4
    vadd.w         vr10,    vr4,     vr5
    vadd.w         vr11,    vr6,     vr7
    vadd.w         vr10,    vr10,    vr8
    vadd.w         vr10,    vr10,    vr11
    vst            vr9,     t2,      0
    vst            vr10,    t2,      16

    addi.d         t3,      t3,      16
    addi.d         t1,      t1,      16
    addi.d         t0,      t0,      32
    addi.d         t2,      t2,      32
    addi.w         t4,      t4,      -8
    ble            t4,      zero,    .LBOXSUM5_V_H1

.LBOXSUM5_V_W:
    vld            vr0,     t1,      0   // a 0 1 2 3 4 5 6 7
    vld            vr1,     t1,      2   // b 1 2 3 4 5 6 7 8
    vld            vr2,     t1,      4   // c 2
    vld            vr3,     t1,      6   // d 3
    vld            vr4,     t1,      8   // e 4 5 6 7 8 9 10 11
    vinsgr2vr.w    vr0,     t5,      0
    vpickve2gr.w   t5,      vr4,     2
    vextrins.h     vr1,     vr0,     0x01
    vadd.h         vr5,     vr0,     vr1
    vadd.h         vr6,     vr2,     vr3
    vadd.h         vr5,     vr5,     vr6
    vadd.h         vr5,     vr5,     vr4
    vst            vr5,     t3,      0

    vaddi.hu       vr0,     vr8,     0  // 8  9 10 11  a
    vld            vr1,     t0,      4  // 9 10 11 12  b
    vld            vr2,     t0,      8  // 10 11 12 13 c
    vld            vr3,     t0,      12 // 14 15 16 17 d
    vld            vr4,     t0,      16 // 15 16 17 18 e  a
    vld            vr5,     t0,      20 // 16 17 18 19    b
    vld            vr6,     t0,      24 // 17 18 19 20    c
    vld            vr7,     t0,      28 // 18 19 20 21    d
    vld            vr8,     t0,      32 // 19 20 21 22    e
    vextrins.w     vr1,     vr0,     0x01
    vadd.w         vr9,     vr0,     vr1
    vadd.w         vr10,    vr2,     vr3
    vadd.w         vr9,     vr9,     vr10
    vadd.w         vr9,     vr9,     vr4
    vadd.w         vr10,    vr4,     vr5
    vadd.w         vr11,    vr6,     vr7
    vadd.w         vr10,    vr10,    vr8
    vadd.w         vr10,    vr10,    vr11
    vst            vr9,     t2,      0
    vst            vr10,    t2,      16

    addi.d         t3,      t3,      16
    addi.d         t1,      t1,      16
    addi.d         t0,      t0,      32
    addi.d         t2,      t2,      32
    addi.w         t4,      t4,      -8
    blt            zero,    t4,      .LBOXSUM5_V_W

.LBOXSUM5_V_H1:
    addi.d         a1,       a1,      REST_UNIT_STRIDE<<1
    addi.d         a0,       a0,      REST_UNIT_STRIDE<<2
    addi.w         a3,       a3,      -1
    bnez           a3,       .LBOXSUM5_V_H
endfunc

/*
selfguided_filter(int32_t *sumsq, coef *sum,
                  const int w, const int h,
                  const unsigned s)
*/
function boxsum5_sgf_h_8bpc_lsx
    addi.d        a0,       a0,        REST_UNIT_STRIDE<<2
    addi.d        a0,       a0,        12   // AA
    addi.d        a1,       a1,        REST_UNIT_STRIDE<<1
    addi.d        a1,       a1,        6    // BB
    la.local      t8,       dav1d_sgr_x_by_x
    li.w          t6,       164
    vreplgr2vr.w  vr20,     t6
    li.w          t6,       255
    vreplgr2vr.w  vr22,     t6
    vaddi.wu      vr21,     vr22,      1  // 256
    vreplgr2vr.w  vr6,      a4
    vldi          vr19,     0x819
    addi.w        a2,       a2,        2  // w + 2
    addi.w        a3,       a3,        2  // h + 2

.LBS5SGF_H_H:
    addi.w        t2,       a2,        0
    addi.d        t0,       a0,        -4
    addi.d        t1,       a1,        -2

.LBS5SGF_H_W:
    vld           vr0,      t0,        0   // AA[i]
    vld           vr1,      t0,        16
    vld           vr2,      t1,        0   // BB[i]

    vmul.w        vr4,      vr0,       vr19 // a * n
    vmul.w        vr5,      vr1,       vr19 // a * n
    vsllwil.w.h   vr9,      vr2,       0
    vexth.w.h     vr10,     vr2
    vmsub.w       vr4,      vr9,       vr9   // p
    vmsub.w       vr5,      vr10,      vr10   // p
    vmaxi.w       vr4,      vr4,       0
    vmaxi.w       vr5,      vr5,       0    // p
    vmul.w        vr4,      vr4,       vr6  // p * s
    vmul.w        vr5,      vr5,       vr6  // p * s
    vsrlri.w      vr4,      vr4,       20
    vsrlri.w      vr5,      vr5,       20   // z
    vmin.w        vr4,      vr4,       vr22
    vmin.w        vr5,      vr5,       vr22

    // load table data
    vpickve2gr.w  t6,       vr4,       0
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr7,      t7,        0
    vpickve2gr.w  t6,       vr4,       1
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr7,      t7,        1
    vpickve2gr.w  t6,       vr4,       2
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr7,      t7,        2
    vpickve2gr.w  t6,       vr4,       3
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr7,      t7,        3

    vpickve2gr.w  t6,       vr5,       0
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr8,      t7,        0
    vpickve2gr.w  t6,       vr5,       1
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr8,      t7,        1
    vpickve2gr.w  t6,       vr5,       2
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr8,      t7,        2
    vpickve2gr.w  t6,       vr5,       3
    ldx.bu        t7,       t8,        t6
    vinsgr2vr.w   vr8,      t7,        3     // x

    vmul.w        vr9,      vr7,       vr9   // x * BB[i]
    vmul.w        vr10,     vr8,       vr10
    vmul.w        vr9,      vr9,       vr20  // x * BB[i] * sgr_one_by_x
    vmul.w        vr10,     vr10,      vr20
    vsrlri.w      vr9,      vr9,       12
    vsrlri.w      vr10,     vr10,      12
    vsub.w        vr7,      vr21,      vr7
    vsub.w        vr8,      vr21,      vr8
    vpickev.h     vr8,      vr8,       vr7
    vst           vr9,      t0,        0
    vst           vr10,     t0,        16
    vst           vr8,      t1,        0
    addi.d        t0,       t0,        32
    addi.d        t1,       t1,        16
    addi.w        t2,       t2,        -8
    blt           zero,     t2,        .LBS5SGF_H_W

    addi.d        a0,       a0,        REST_UNIT_STRIDE<<2
    addi.d        a0,       a0,        REST_UNIT_STRIDE<<2
    addi.d        a1,       a1,        REST_UNIT_STRIDE<<2
    addi.w        a3,       a3,        -2
    blt           zero,     a3,        .LBS5SGF_H_H
endfunc

/*
selfguided_filter(coef *dst, pixel *src,
                  int32_t *sumsq, coef *sum,
                  const int w, const int h)
*/
function boxsum5_sgf_v_8bpc_lsx
    addi.d        a1,        a1,       3*REST_UNIT_STRIDE+3       // src
    addi.d        a2,        a2,       (2*REST_UNIT_STRIDE+3)<<1  // A
    addi.d        a2,        a2,       (2*REST_UNIT_STRIDE+3)<<1
    addi.d        a3,        a3,       (2*REST_UNIT_STRIDE+3)<<1  // B
    addi.w        a5,        a5,       -1
    vldi          vr10,      0x806
    vldi          vr11,      0x805
    vldi          vr22,      0x406

.LBS5SGF_V_H:
    addi.d        t0,        a0,       0
    addi.d        t1,        a1,       0
    addi.d        t2,        a2,       0
    addi.d        t3,        a3,       0
    addi.w        t4,        a4,       0

    addi.d        t5,        a0,       384*2
    addi.d        t6,        a1,       REST_UNIT_STRIDE
    addi.d        t7,        a2,       REST_UNIT_STRIDE<<2
    addi.d        t8,        a3,       REST_UNIT_STRIDE<<1   // B
.LBS5SGF_V_W:
    // a
    vld           vr0,       t3,       -REST_UNIT_STRIDE*2
    vld           vr1,       t3,       REST_UNIT_STRIDE*2
    vld           vr2,       t3,       (-REST_UNIT_STRIDE-1)*2
    vld           vr3,       t3,       (REST_UNIT_STRIDE-1)*2
    vld           vr4,       t3,       (1-REST_UNIT_STRIDE)*2
    vld           vr5,       t3,       (1+REST_UNIT_STRIDE)*2
    vaddwev.w.h   vr6,       vr0,      vr1
    vaddwod.w.h   vr7,       vr0,      vr1
    vmul.w        vr6,       vr6,      vr10
    vmul.w        vr7,       vr7,      vr10
    vaddwev.w.h   vr8,       vr2,      vr3
    vaddwod.w.h   vr9,       vr2,      vr3
    vaddwev.w.h   vr12,      vr4,      vr5
    vaddwod.w.h   vr13,      vr4,      vr5
    vadd.w        vr8,       vr8,      vr12
    vadd.w        vr9,       vr9,      vr13
    vmadd.w       vr6,       vr8,      vr11
    vmadd.w       vr7,       vr9,      vr11
    vilvl.w       vr18,      vr7,      vr6
    vilvh.w       vr19,      vr7,      vr6
    // b
    vld           vr0,       t2,       -REST_UNIT_STRIDE*4
    vld           vr1,       t2,       -REST_UNIT_STRIDE*4+16
    vld           vr2,       t2,       REST_UNIT_STRIDE*4
    vld           vr3,       t2,       REST_UNIT_STRIDE*4+16
    vld           vr4,       t2,       (-REST_UNIT_STRIDE-1)*4
    vld           vr5,       t2,       (-REST_UNIT_STRIDE-1)*4+16
    vld           vr8,       t2,       (REST_UNIT_STRIDE-1)*4
    vld           vr9,       t2,       (REST_UNIT_STRIDE-1)*4+16
    vld           vr12,      t2,       (1-REST_UNIT_STRIDE)*4
    vld           vr13,      t2,       (1-REST_UNIT_STRIDE)*4+16
    vld           vr14,      t2,       (1+REST_UNIT_STRIDE)*4
    vld           vr15,      t2,       (1+REST_UNIT_STRIDE)*4+16
    vadd.w        vr0,       vr0,      vr2  // 0 1 2 3
    vadd.w        vr1,       vr1,      vr3  // 4 5 6 7
    vmul.w        vr20,      vr0,      vr10
    vmul.w        vr21,      vr1,      vr10
    vadd.w        vr4,       vr4,      vr8  // 0 1 2 3
    vadd.w        vr5,       vr5,      vr9  // 4 5 6 7
    vadd.w        vr12,      vr12,     vr14
    vadd.w        vr13,      vr13,     vr15
    vadd.w        vr12,      vr12,     vr4
    vadd.w        vr13,      vr13,     vr5
    vmadd.w       vr20,      vr12,     vr11
    vmadd.w       vr21,      vr13,     vr11
    vld           vr2,       t1,       0
    vsllwil.hu.bu vr2,       vr2,      0
    vsllwil.wu.hu vr3,       vr2,      0
    vexth.wu.hu   vr4,       vr2
    vmadd.w       vr20,      vr18,     vr3
    vmadd.w       vr21,      vr19,     vr4
    vssrlrni.h.w  vr21,      vr20,     9
    vst           vr21,      t0,       0

    addi.d        t1,        t1,       8
    addi.d        t2,        t2,       32
    addi.d        t3,        t3,       16

    // a
    vld           vr0,       t8,       0
    vld           vr1,       t8,       -2
    vld           vr2,       t8,       2
    vmulwev.w.h   vr3,       vr0,      vr22
    vmulwod.w.h   vr4,       vr0,      vr22
    vaddwev.w.h   vr5,       vr1,      vr2
    vaddwod.w.h   vr6,       vr1,      vr2
    vmadd.w       vr3,       vr5,      vr11
    vmadd.w       vr4,       vr6,      vr11
    vilvl.w       vr19,      vr4,      vr3
    vilvh.w       vr20,      vr4,      vr3
    // b
    vld           vr0,       t7,       0
    vld           vr1,       t7,       -4
    vld           vr2,       t7,       4
    vld           vr5,       t7,       16
    vld           vr6,       t7,       12
    vld           vr7,       t7,       20
    vmul.w        vr8,       vr0,      vr10
    vmul.w        vr9,       vr5,      vr10
    vadd.w        vr12,      vr1,      vr2
    vadd.w        vr13,      vr6,      vr7
    vmadd.w       vr8,       vr12,     vr11
    vmadd.w       vr9,       vr13,     vr11
    vld           vr2,       t6,       0
    vsllwil.hu.bu vr2,       vr2,      0
    vsllwil.wu.hu vr3,       vr2,      0
    vexth.wu.hu   vr4,       vr2
    vmadd.w       vr8,       vr19,     vr3
    vmadd.w       vr9,       vr20,     vr4
    vssrlrni.h.w  vr9,       vr8,      8
    vst           vr9,       t0,       384*2

    addi.d        t0,        t0,       16
    addi.d        t8,        t8,       16
    addi.d        t7,        t7,       32
    addi.d        t6,        t6,       8
    addi.w        t4,        t4,       -8
    blt           zero,      t4,       .LBS5SGF_V_W

    addi.w        a5,        a5,       -2
    addi.d        a0,        a0,       384*4                // dst
    addi.d        a1,        a1,       REST_UNIT_STRIDE<<1  // src
    addi.d        a2,        a2,       REST_UNIT_STRIDE<<2  //
    addi.d        a2,        a2,       REST_UNIT_STRIDE<<2
    addi.d        a3,        a3,       REST_UNIT_STRIDE<<2  //
    blt           zero,      a5,       .LBS5SGF_V_H
    bnez          a5,        .LBS5SGF_END
.LBS5SGF_V_W1:
    // a
    vld           vr0,       a3,       -REST_UNIT_STRIDE*2
    vld           vr1,       a3,       REST_UNIT_STRIDE*2
    vld           vr2,       a3,       (-REST_UNIT_STRIDE-1)*2
    vld           vr3,       a3,       (REST_UNIT_STRIDE-1)*2
    vld           vr4,       a3,       (1-REST_UNIT_STRIDE)*2
    vld           vr5,       a3,       (1+REST_UNIT_STRIDE)*2
    vaddwev.w.h   vr6,       vr0,      vr1
    vaddwod.w.h   vr7,       vr0,      vr1
    vmul.w        vr6,       vr6,      vr10
    vmul.w        vr7,       vr7,      vr10
    vaddwev.w.h   vr8,       vr2,      vr3
    vaddwod.w.h   vr9,       vr2,      vr3
    vaddwev.w.h   vr12,      vr4,      vr5
    vaddwod.w.h   vr13,      vr4,      vr5
    vadd.w        vr8,       vr8,      vr12
    vadd.w        vr9,       vr9,      vr13
    vmadd.w       vr6,       vr8,      vr11
    vmadd.w       vr7,       vr9,      vr11
    vilvl.w       vr18,      vr7,      vr6
    vilvh.w       vr19,      vr7,      vr6
    // b
    vld           vr0,       a2,       -REST_UNIT_STRIDE*4
    vld           vr1,       a2,       -REST_UNIT_STRIDE*4+16
    vld           vr2,       a2,       REST_UNIT_STRIDE*4
    vld           vr3,       a2,       REST_UNIT_STRIDE*4+16
    vld           vr4,       a2,       (-REST_UNIT_STRIDE-1)*4
    vld           vr5,       a2,       (-REST_UNIT_STRIDE-1)*4+16
    vld           vr8,       a2,       (REST_UNIT_STRIDE-1)*4
    vld           vr9,       a2,       (REST_UNIT_STRIDE-1)*4+16
    vld           vr12,      a2,       (1-REST_UNIT_STRIDE)*4
    vld           vr13,      a2,       (1-REST_UNIT_STRIDE)*4+16
    vld           vr14,      a2,       (1+REST_UNIT_STRIDE)*4
    vld           vr15,      a2,       (1+REST_UNIT_STRIDE)*4+16
    vadd.w        vr0,       vr0,      vr2  // 0 1 2 3
    vadd.w        vr1,       vr1,      vr3  // 4 5 6 7
    vmul.w        vr20,      vr0,      vr10
    vmul.w        vr21,      vr1,      vr10
    vadd.w        vr4,       vr4,      vr8  // 0 1 2 3
    vadd.w        vr5,       vr5,      vr9  // 4 5 6 7
    vadd.w        vr12,      vr12,     vr14
    vadd.w        vr13,      vr13,     vr15
    vadd.w        vr12,      vr12,     vr4
    vadd.w        vr13,      vr13,     vr5
    vmadd.w       vr20,      vr12,     vr11
    vmadd.w       vr21,      vr13,     vr11
    vld           vr2,       a1,       0
    vsllwil.hu.bu vr2,       vr2,      0
    vsllwil.wu.hu vr3,       vr2,      0
    vexth.wu.hu   vr4,       vr2
    vmadd.w       vr20,      vr18,     vr3
    vmadd.w       vr21,      vr19,     vr4
    vssrlrni.h.w  vr21,      vr20,     9
    vst           vr21,      a0,       0
    addi.d        a3,        a3,       16
    addi.d        a2,        a2,       32
    addi.d        a1,        a1,       8
    addi.d        a0,        a0,       16
    addi.w        a4,        a4,       -8
    blt           zero,      a4,       .LBS5SGF_V_W1
.LBS5SGF_END:
endfunc

/*
void dav1d_sgr_mix_finish_lsx(uint8_t *p, const ptrdiff_t stride,
                              const int16_t *dst0, const int16_t *dst1,
                              const int w0, const int w1,
                              const int w, const int h);
*/
function sgr_mix_finish_8bpc_lsx
    vreplgr2vr.w  vr3,     a4            // w0
    vreplgr2vr.w  vr13,    a5            // w1
    andi          t4,      a6,       0x7
    sub.w         t5,      a6,       t4

    beq           zero,    t5,      .LSGRMIX_REM

.LSGRMIX_H:
    addi.d        t0,      a0,       0
    addi.d        t1,      a2,       0   // dst0
    addi.d        t3,      a3,       0   // dst1
    addi.w        t2,      t5,       0
    andi          t4,      a6,       0x7
.LSGRMIX_W:
    vld           vr0,     t0,       0
    vld           vr1,     t1,       0
    vld           vr10,    t3,       0
    vsllwil.hu.bu vr2,     vr0,      4   // u 8 h
    vsllwil.wu.hu vr4,     vr2,      0   // u 0 1 2 3
    vexth.wu.hu   vr5,     vr2           // u 4 5 6 7
    vslli.w       vr6,     vr4,      7
    vslli.w       vr7,     vr5,      7
    vsllwil.w.h   vr8,     vr1,      0   // dst0
    vexth.w.h     vr9,     vr1           // dst0
    vsub.w        vr8,     vr8,      vr4
    vsub.w        vr9,     vr9,      vr5
    vmadd.w       vr6,     vr8,      vr3  // v 0 - 3
    vmadd.w       vr7,     vr9,      vr3  // v 4 - 7

    vsllwil.w.h   vr11,    vr10,     0    // dst1
    vexth.w.h     vr12,    vr10           // dst1
    vsub.w        vr11,    vr11,     vr4
    vsub.w        vr12,    vr12,     vr5
    vmadd.w       vr6,     vr11,     vr13
    vmadd.w       vr7,     vr12,     vr13

    vssrarni.hu.w vr7,     vr6,      11
    vssrlni.bu.h  vr7,     vr7,      0
    vstelm.d      vr7,     t0,       0,    0
    addi.d        t0,      t0,       8
    addi.d        t1,      t1,       16
    addi.d        t3,      t3,       16
    addi.d        t2,      t2,       -8
    bne           zero,    t2,       .LSGRMIX_W

    beq           t4,      zero,     .LSGRMIX_W8

    vld           vr0,     t0,       0
    vld           vr1,     t1,       0
    vld           vr10,    t3,       0
    vsllwil.hu.bu vr2,     vr0,      4   // u 8 h
    vsllwil.wu.hu vr4,     vr2,      0   // p
    vexth.wu.hu   vr5,     vr2           // p
    vslli.w       vr6,     vr4,      7
    vslli.w       vr7,     vr5,      7
    vsllwil.w.h   vr8,     vr1,      0   // dst
    vexth.w.h     vr9,     vr1           // dst
    vsub.w        vr8,     vr8,      vr4
    vsub.w        vr9,     vr9,      vr5
    vmadd.w       vr6,     vr8,      vr3  // v 0 - 3
    vmadd.w       vr7,     vr9,      vr3  // v 4 - 7

    vsllwil.w.h   vr11,    vr10,     0    // dst1
    vexth.w.h     vr12,    vr10           // dst1
    vsub.w        vr11,    vr11,     vr4
    vsub.w        vr12,    vr12,     vr5
    vmadd.w       vr6,     vr11,     vr13
    vmadd.w       vr7,     vr12,     vr13

    vssrarni.hu.w vr7,     vr6,      11
    vssrlni.bu.h  vr7,     vr7,      0

.LSGRMIX_ST:
    vstelm.b      vr7,     t0,       0,    0
    addi.d        t0,      t0,       1
    vbsrl.v       vr7,     vr7,      1
    addi.w        t4,      t4,       -1
    bnez          t4,      .LSGRMIX_ST

.LSGRMIX_W8:
    addi.w        a7,      a7,       -1
    add.d         a0,      a0,       a1
    addi.d        a2,      a2,       (FILTER_OUT_STRIDE<<1)
    addi.d        a3,      a3,       (FILTER_OUT_STRIDE<<1)
    bnez          a7,      .LSGRMIX_H
    b             .LSGR_MIX_END

.LSGRMIX_REM:
    andi          t4,      a6,       0x7
    vld           vr0,     a0,       0
    vld           vr1,     a2,       0
    vld           vr10,    a3,       0
    vsllwil.hu.bu vr2,     vr0,      4   // u 8 h
    vsllwil.wu.hu vr4,     vr2,      0   // p
    vexth.wu.hu   vr5,     vr2           // p
    vslli.w       vr6,     vr4,      7
    vslli.w       vr7,     vr5,      7
    vsllwil.w.h   vr8,     vr1,      0   // dst
    vexth.w.h     vr9,     vr1           // dst
    vsub.w        vr8,     vr8,      vr4
    vsub.w        vr9,     vr9,      vr5
    vmadd.w       vr6,     vr8,      vr3  // v 0 - 3
    vmadd.w       vr7,     vr9,      vr3  // v 4 - 7

    vsllwil.w.h   vr11,    vr10,     0    // dst1
    vexth.w.h     vr12,    vr10           // dst1
    vsub.w        vr11,    vr11,     vr4
    vsub.w        vr12,    vr12,     vr5
    vmadd.w       vr6,     vr11,     vr13
    vmadd.w       vr7,     vr12,     vr13

    vssrarni.hu.w vr7,     vr6,      11
    vssrlni.bu.h  vr7,     vr7,      0
    addi.d        t0,      a0,       0
.LSGRMIX_REM_ST:
    vstelm.b      vr7,     t0,       0,    0
    addi.d        t0,      t0,       1
    vbsrl.v       vr7,     vr7,      1
    addi.w        t4,      t4,       -1
    bnez          t4,      .LSGRMIX_REM_ST

    addi.w        a7,      a7,       -1
    add.d         a0,      a0,       a1
    addi.d        a2,      a2,       (FILTER_OUT_STRIDE<<1)
    addi.d        a3,      a3,       (FILTER_OUT_STRIDE<<1)
    bnez          a7,      .LSGRMIX_REM

.LSGR_MIX_END:
endfunc
