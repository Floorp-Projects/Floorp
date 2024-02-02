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

/*
static void warp_affine_8x8_c(pixel *dst, const ptrdiff_t dst_stride,
                              const pixel *src, const ptrdiff_t src_stride,
                              const int16_t *const abcd, int mx, int my
                              HIGHBD_DECL_SUFFIX)
*/
.macro FILTER_WARP_RND_P_LSX in0, in1, in2, in3, out0, out1, out2, out3
    vbsrl.v         vr2,    \in0,     \in1
    vbsrl.v         vr20,   \in0,     \in2
    addi.w          t4,     \in3,     512
    srai.w          t4,     t4,       10
    addi.w          t4,     t4,       64
    slli.w          t4,     t4,       3
    vldx            vr1,    t5,       t4
    add.w           t3,     t3,       t0   // tmx += abcd[0]

    addi.w          t4,     t3,       512
    srai.w          t4,     t4,       10
    addi.w          t4,     t4,       64
    slli.w          t4,     t4,       3
    vldx            vr29,   t5,       t4
    add.w           t3,     t3,       t0   // tmx += abcd[0]

    vilvl.d         vr2,    vr20,     vr2
    vilvl.d         vr1,    vr29,     vr1
    vmulwev.h.bu.b  vr3,    vr2,      vr1
    vmulwod.h.bu.b  vr20,   vr2,      vr1
    vilvl.d         vr2,    vr20,     vr3
    vhaddw.w.h      vr2,    vr2,      vr2
    vhaddw.d.w      vr2,    vr2,      vr2
    vhaddw.q.d      vr2,    vr2,      vr2
    vilvh.d         vr3,    vr20,     vr3
    vhaddw.w.h      vr3,    vr3,      vr3
    vhaddw.d.w      vr3,    vr3,      vr3
    vhaddw.q.d      vr3,    vr3,      vr3
    vextrins.w      \out0,  vr2,      \out1
    vextrins.w      \out2,  vr3,      \out3
.endm

.macro FILTER_WARP_CLIP_LSX in0, in1, in2, out0, out1
    add.w           \in0,     \in0,    \in1
    addi.w          t6,       \in0,    512
    srai.w          t6,       t6,      10
    addi.w          t6,       t6,      64
    slli.w          t6,       t6,      3
    fldx.d          f1,       t5,      t6
    vsllwil.h.b     vr1,      vr1,     0
    vmulwev.w.h     vr3,      \in2,    vr1
    vmaddwod.w.h    vr3,      \in2,    vr1
    vhaddw.d.w      vr3,      vr3,     vr3
    vhaddw.q.d      vr3,      vr3,     vr3
    vextrins.w      \out0,    vr3,     \out1
.endm

const warp_sh
.rept 2
.byte 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17
.endr
.rept 2
.byte 18, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.endr
endconst

.macro warp_lsx t, shift
function warp_affine_8x8\t\()_8bpc_lsx
    addi.d          sp,       sp,      -64
    fst.d           f24,      sp,      0
    fst.d           f25,      sp,      8
    fst.d           f26,      sp,      16
    fst.d           f27,      sp,      24
    fst.d           f28,      sp,      32
    fst.d           f29,      sp,      40
    fst.d           f30,      sp,      48
    fst.d           f31,      sp,      56

    la.local        t4,       warp_sh
    ld.h            t0,       a4,      0   // abcd[0]
    ld.h            t1,       a4,      2   // abcd[1]

    alsl.w          t2,       a3,      a3,     1
    addi.w          t3,       a5,      0
    la.local        t5,       dav1d_mc_warp_filter
    sub.d           a2,       a2,      t2
    addi.d          a2,       a2,      -3
    vld             vr0,      a2,      0
    vld             vr30,     t4,      0
    vld             vr31,     t4,      32

    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr4, 0x00, vr5, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr6, 0x00, vr7, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr8, 0x00, vr9, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr10, 0x00, vr11, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr4, 0x10, vr5, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr6, 0x10, vr7, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr8, 0x10, vr9, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr10, 0x10, vr11, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr4, 0x20, vr5, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr6, 0x20, vr7, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr8, 0x20, vr9, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr10, 0x20, vr11, 0x20

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr4, 0x30, vr5, 0x30
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr6, 0x30, vr7, 0x30
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr8, 0x30, vr9, 0x30
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr10, 0x30, vr11, 0x30

    add.w           a5,       t1,      a5
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr12, 0x00, vr13, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr14, 0x00, vr15, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr16, 0x00, vr17, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr18, 0x00, vr19, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr12, 0x10, vr13, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr14, 0x10, vr15, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr16, 0x10, vr17, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr18, 0x10, vr19, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr12, 0x20, vr13, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr14, 0x20, vr15, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr16, 0x20, vr17, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr18, 0x20, vr19, 0x20

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr12, 0x30, vr13, 0x30
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr14, 0x30, vr15, 0x30
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr16, 0x30, vr17, 0x30
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr18, 0x30, vr19, 0x30

    vsrarni.h.w       vr12,     vr4,     3
    vsrarni.h.w       vr13,     vr5,     3
    vsrarni.h.w       vr14,     vr6,     3
    vsrarni.h.w       vr15,     vr7,     3
    vsrarni.h.w       vr16,     vr8,     3
    vsrarni.h.w       vr17,     vr9,     3
    vsrarni.h.w       vr18,     vr10,    3
    vsrarni.h.w       vr19,     vr11,    3

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr4, 0x00, vr5, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr6, 0x00, vr7, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr8, 0x00, vr9, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr10, 0x00, vr11, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr4, 0x10, vr5, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr6, 0x10, vr7, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr8, 0x10, vr9, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr10, 0x10, vr11, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr4, 0x20, vr5, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr6, 0x20, vr7, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr8, 0x20, vr9, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr10, 0x20, vr11, 0x20

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr4, 0x30, vr5, 0x30
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr6, 0x30, vr7, 0x30
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr8, 0x30, vr9, 0x30
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr10, 0x30, vr11, 0x30

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr21, 0x00, vr22, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr23, 0x00, vr24, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr25, 0x00, vr26, 0x00
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr27, 0x00, vr28, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr21, 0x10, vr22, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr23, 0x10, vr24, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr25, 0x10, vr26, 0x10
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr27, 0x10, vr28, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    FILTER_WARP_RND_P_LSX   vr0, 0, 1, a5, vr21, 0x20, vr22, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 2, 3, t3, vr23, 0x20, vr24, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 4, 5, t3, vr25, 0x20, vr26, 0x20
    FILTER_WARP_RND_P_LSX   vr0, 6, 7, t3, vr27, 0x20, vr28, 0x20

    vsrarni.h.w     vr21,     vr4,     3
    vsrarni.h.w     vr22,     vr5,     3
    vsrarni.h.w     vr23,     vr6,     3
    vsrarni.h.w     vr24,     vr7,     3
    vsrarni.h.w     vr25,     vr8,     3
    vsrarni.h.w     vr26,     vr9,     3
    vsrarni.h.w     vr27,     vr10,    3
    vsrarni.h.w     vr28,     vr11,    3

    addi.w          t2,       a6,      0   // my
    ld.h            t7,       a4,      4   // abcd[2]
    ld.h            t8,       a4,      6   // abcd[3]

.ifnb \t
    slli.d          a1,       a1,      1
.endif

    FILTER_WARP_CLIP_LSX  t2, zero, vr12,  vr4, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr13,  vr4, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr14,  vr4, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr15,  vr4, 0x30
    FILTER_WARP_CLIP_LSX  t2, t7,   vr16,  vr5, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr17,  vr5, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr18,  vr5, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr19,  vr5, 0x30
.ifnb \t
    vssrarni.h.w    vr5,      vr4,     \shift
    vst             vr5,      a0,      0
.else
    vssrarni.hu.w   vr5,      vr4,     \shift
    vssrlni.bu.h    vr5,      vr5,     0
    fst.d           f5,       a0,      0
.endif

    vshuf.b         vr12,     vr21,    vr12,   vr30
    vshuf.b         vr13,     vr22,    vr13,   vr30
    vshuf.b         vr14,     vr23,    vr14,   vr30
    vshuf.b         vr15,     vr24,    vr15,   vr30
    vshuf.b         vr16,     vr25,    vr16,   vr30
    vshuf.b         vr17,     vr26,    vr17,   vr30
    vshuf.b         vr18,     vr27,    vr18,   vr30
    vshuf.b         vr19,     vr28,    vr19,   vr30
    vextrins.h      vr30,     vr31,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LSX  t2, zero, vr12,  vr4, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr13,  vr4, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr14,  vr4, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr15,  vr4, 0x30
    FILTER_WARP_CLIP_LSX  t2, t7,   vr16,  vr5, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr17,  vr5, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr18,  vr5, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr19,  vr5, 0x30
.ifnb \t
    vssrarni.h.w    vr5,      vr4,     \shift
    vstx            vr5,      a0,      a1
.else
    vssrarni.hu.w   vr5,      vr4,     \shift
    vssrlni.bu.h    vr5,      vr5,     0
    fstx.d          f5,       a0,      a1
.endif

    vaddi.bu        vr31,     vr31,    2
    vshuf.b         vr12,     vr21,    vr12,   vr30
    vshuf.b         vr13,     vr22,    vr13,   vr30
    vshuf.b         vr14,     vr23,    vr14,   vr30
    vshuf.b         vr15,     vr24,    vr15,   vr30
    vshuf.b         vr16,     vr25,    vr16,   vr30
    vshuf.b         vr17,     vr26,    vr17,   vr30
    vshuf.b         vr18,     vr27,    vr18,   vr30
    vshuf.b         vr19,     vr28,    vr19,   vr30
    vextrins.h      vr30,     vr31,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LSX  t2, zero, vr12,  vr4, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr13,  vr4, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr14,  vr4, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr15,  vr4, 0x30
    FILTER_WARP_CLIP_LSX  t2, t7,   vr16,  vr5, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr17,  vr5, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr18,  vr5, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr19,  vr5, 0x30
    alsl.d          a0,       a1,      a0,   1
.ifnb \t
    vssrarni.h.w    vr5,      vr4,     \shift
    vst             vr5,      a0,      0
.else
    vssrarni.hu.w   vr5,      vr4,     \shift
    vssrlni.bu.h    vr5,      vr5,     0
    fst.d           f5,       a0,      0
.endif

    vaddi.bu        vr31,     vr31,    2
    vshuf.b         vr12,     vr21,    vr12,   vr30
    vshuf.b         vr13,     vr22,    vr13,   vr30
    vshuf.b         vr14,     vr23,    vr14,   vr30
    vshuf.b         vr15,     vr24,    vr15,   vr30
    vshuf.b         vr16,     vr25,    vr16,   vr30
    vshuf.b         vr17,     vr26,    vr17,   vr30
    vshuf.b         vr18,     vr27,    vr18,   vr30
    vshuf.b         vr19,     vr28,    vr19,   vr30
    vextrins.h      vr30,     vr31,    0x70

    add.w           a6,       a6,       t8
    addi.w          t2,       a6,       0
    FILTER_WARP_CLIP_LSX  t2, zero, vr12,  vr4, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr13,  vr4, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr14,  vr4, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr15,  vr4, 0x30
    FILTER_WARP_CLIP_LSX  t2, t7,   vr16,  vr5, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr17,  vr5, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr18,  vr5, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr19,  vr5, 0x30
.ifnb \t
    vssrarni.h.w    vr5,      vr4,      \shift
    vstx            vr5,      a0,       a1
.else
    vssrarni.hu.w   vr5,      vr4,      \shift
    vssrlni.bu.h    vr5,      vr5,      0
    fstx.d          f5,       a0,       a1
.endif

    vaddi.bu        vr31,     vr31,    2
    vshuf.b         vr12,     vr21,    vr12,   vr30
    vshuf.b         vr13,     vr22,    vr13,   vr30
    vshuf.b         vr14,     vr23,    vr14,   vr30
    vshuf.b         vr15,     vr24,    vr15,   vr30
    vshuf.b         vr16,     vr25,    vr16,   vr30
    vshuf.b         vr17,     vr26,    vr17,   vr30
    vshuf.b         vr18,     vr27,    vr18,   vr30
    vshuf.b         vr19,     vr28,    vr19,   vr30
    vextrins.h      vr30,     vr31,    0x70

    add.w           a6,       a6,       t8
    addi.w          t2,       a6,       0
    FILTER_WARP_CLIP_LSX  t2, zero, vr12,  vr4, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr13,  vr4, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr14,  vr4, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr15,  vr4, 0x30
    FILTER_WARP_CLIP_LSX  t2, t7,   vr16,  vr5, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr17,  vr5, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr18,  vr5, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr19,  vr5, 0x30
    alsl.d          a0,       a1,       a0,   1
.ifnb \t
    vssrarni.h.w    vr5,      vr4,      \shift
    vst             vr5,      a0,       0
.else
    vssrarni.hu.w   vr5,      vr4,      \shift
    vssrlni.bu.h    vr5,      vr5,      0
    fst.d           f5,       a0,       0
.endif

    vaddi.bu        vr31,     vr31,    2
    vshuf.b         vr12,     vr21,    vr12,   vr30
    vshuf.b         vr13,     vr22,    vr13,   vr30
    vshuf.b         vr14,     vr23,    vr14,   vr30
    vshuf.b         vr15,     vr24,    vr15,   vr30
    vshuf.b         vr16,     vr25,    vr16,   vr30
    vshuf.b         vr17,     vr26,    vr17,   vr30
    vshuf.b         vr18,     vr27,    vr18,   vr30
    vshuf.b         vr19,     vr28,    vr19,   vr30
    vextrins.h      vr30,     vr31,    0x70

    add.w           a6,       a6,       t8
    addi.w          t2,       a6,       0
    FILTER_WARP_CLIP_LSX  t2, zero, vr12,  vr4, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr13,  vr4, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr14,  vr4, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr15,  vr4, 0x30
    FILTER_WARP_CLIP_LSX  t2, t7,   vr16,  vr5, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr17,  vr5, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr18,  vr5, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr19,  vr5, 0x30
.ifnb \t
    vssrarni.h.w    vr5,      vr4,      \shift
    vstx            vr5,      a0,       a1
.else
    vssrarni.hu.w   vr5,      vr4,      \shift
    vssrlni.bu.h    vr5,      vr5,      0
    fstx.d          f5,       a0,       a1
.endif

    vaddi.bu        vr31,     vr31,    2
    vshuf.b         vr12,     vr21,    vr12,   vr30
    vshuf.b         vr13,     vr22,    vr13,   vr30
    vshuf.b         vr14,     vr23,    vr14,   vr30
    vshuf.b         vr15,     vr24,    vr15,   vr30
    vshuf.b         vr16,     vr25,    vr16,   vr30
    vshuf.b         vr17,     vr26,    vr17,   vr30
    vshuf.b         vr18,     vr27,    vr18,   vr30
    vshuf.b         vr19,     vr28,    vr19,   vr30
    vextrins.h      vr30,     vr31,    0x70

    add.w           a6,       a6,       t8
    addi.w          t2,       a6,       0
    FILTER_WARP_CLIP_LSX  t2, zero, vr12,  vr4, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr13,  vr4, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr14,  vr4, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr15,  vr4, 0x30
    FILTER_WARP_CLIP_LSX  t2, t7,   vr16,  vr5, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr17,  vr5, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr18,  vr5, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr19,  vr5, 0x30
    alsl.d          a0,       a1,       a0,   1
.ifnb \t
    vssrarni.h.w    vr5,      vr4,      \shift
    vst             vr5,      a0,       0
.else
    vssrarni.hu.w   vr5,      vr4,      \shift
    vssrlni.bu.h    vr5,      vr5,      0
    fst.d           f5,       a0,       0
.endif

    vshuf.b         vr12,     vr21,    vr12,   vr30
    vshuf.b         vr13,     vr22,    vr13,   vr30
    vshuf.b         vr14,     vr23,    vr14,   vr30
    vshuf.b         vr15,     vr24,    vr15,   vr30
    vshuf.b         vr16,     vr25,    vr16,   vr30
    vshuf.b         vr17,     vr26,    vr17,   vr30
    vshuf.b         vr18,     vr27,    vr18,   vr30
    vshuf.b         vr19,     vr28,    vr19,   vr30

    add.w           a6,       a6,       t8
    addi.w          t2,       a6,       0
    FILTER_WARP_CLIP_LSX  t2, zero, vr12,  vr4, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr13,  vr4, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr14,  vr4, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr15,  vr4, 0x30
    FILTER_WARP_CLIP_LSX  t2, t7,   vr16,  vr5, 0x00
    FILTER_WARP_CLIP_LSX  t2, t7,   vr17,  vr5, 0x10
    FILTER_WARP_CLIP_LSX  t2, t7,   vr18,  vr5, 0x20
    FILTER_WARP_CLIP_LSX  t2, t7,   vr19,  vr5, 0x30
.ifnb \t
    vssrarni.h.w    vr5,      vr4,      \shift
    vstx            vr5,      a0,       a1
.else
    vssrarni.hu.w   vr5,      vr4,      \shift
    vssrlni.bu.h    vr5,      vr5,      0
    fstx.d          f5,       a0,       a1
.endif

    fld.d           f24,      sp,       0
    fld.d           f25,      sp,       8
    fld.d           f26,      sp,       16
    fld.d           f27,      sp,       24
    fld.d           f28,      sp,       32
    fld.d           f29,      sp,       40
    fld.d           f30,      sp,       48
    fld.d           f31,      sp,       56
    addi.d          sp,       sp,       64
endfunc
.endm

warp_lsx , 11
warp_lsx t, 7

.macro FILTER_WARP_RND_P_LASX in0, in1, in2, out0, out1, out2, out3
    xvshuf.b        xr2,    \in0,     \in0,     \in2

    addi.w          t4,     \in1,     512
    srai.w          t4,     t4,       10
    addi.w          t4,     t4,       64
    slli.w          t4,     t4,       3
    vldx            vr3,    t5,       t4
    add.w           t3,     t3,       t0   // tmx += abcd[0]

    addi.w          t4,     t3,       512
    srai.w          t4,     t4,       10
    addi.w          t4,     t4,       64
    slli.w          t4,     t4,       3
    vldx            vr4,    t5,       t4
    add.w           t3,     t3,       t0   // tmx += abcd[0]

    addi.w          t4,     t3,       512
    srai.w          t4,     t4,       10
    addi.w          t4,     t4,       64
    slli.w          t4,     t4,       3
    vldx            vr5,    t5,       t4
    add.w           t3,     t3,       t0   // tmx += abcd[0]

    addi.w          t4,     t3,       512
    srai.w          t4,     t4,       10
    addi.w          t4,     t4,       64
    slli.w          t4,     t4,       3
    vldx            vr6,    t5,       t4
    add.w           t3,     t3,       t0   // tmx += abcd[0]

    xvinsve0.d      xr3,    xr5,      1
    xvinsve0.d      xr3,    xr4,      2
    xvinsve0.d      xr3,    xr6,      3

    xvmulwev.h.bu.b xr4,    xr2,      xr3
    xvmulwod.h.bu.b xr5,    xr2,      xr3
    xvilvl.d        xr2,    xr5,      xr4
    xvilvh.d        xr3,    xr5,      xr4
    xvhaddw.w.h     xr2,    xr2,      xr2
    xvhaddw.w.h     xr3,    xr3,      xr3
    xvhaddw.d.w     xr2,    xr2,      xr2
    xvhaddw.d.w     xr3,    xr3,      xr3
    xvhaddw.q.d     xr2,    xr2,      xr2
    xvhaddw.q.d     xr3,    xr3,      xr3

    xvextrins.w     \out0,  xr2,      \out1
    xvextrins.w     \out2,  xr3,      \out3
.endm

.macro FILTER_WARP_CLIP_LASX in0, in1, in2, out0, out1
    add.w           \in0,     \in0,    \in1
    addi.w          t6,       \in0,    512
    srai.w          t6,       t6,      10
    addi.w          t6,       t6,      64
    slli.w          t6,       t6,      3
    fldx.d          f1,       t5,      t6

    add.w           t2,       t2,      t7
    addi.w          t6,       t2,      512
    srai.w          t6,       t6,      10
    addi.w          t6,       t6,      64
    slli.w          t6,       t6,      3
    fldx.d          f2,       t5,      t6

    vilvl.d         vr0,      vr2,     vr1
    vext2xv.h.b     xr0,      xr0
    xvmulwev.w.h    xr3,      \in2,    xr0
    xvmaddwod.w.h   xr3,      \in2,    xr0
    xvhaddw.d.w     xr3,      xr3,     xr3
    xvhaddw.q.d     xr3,      xr3,     xr3
    xvextrins.w     \out0,    xr3,     \out1
.endm

const shuf0
.byte  0, 1, 2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 6, 7, 8, 9
.byte  1, 2, 3, 4, 5, 6, 7, 8, 3, 4, 5, 6, 7, 8, 9, 10
endconst

.macro warp_lasx t, shift
function warp_affine_8x8\t\()_8bpc_lasx
    addi.d          sp,       sp,      -16
    ld.h            t0,       a4,      0   // abcd[0]
    ld.h            t1,       a4,      2   // abcd[1]
    fst.d           f24,      sp,      0
    fst.d           f25,      sp,      8

    alsl.w          t2,       a3,      a3,     1
    addi.w          t3,       a5,      0
    la.local        t4,       warp_sh
    la.local        t5,       dav1d_mc_warp_filter
    sub.d           a2,       a2,      t2
    addi.d          a2,       a2,      -3
    vld             vr0,      a2,      0
    xvld            xr24,     t4,      0
    xvld            xr25,     t4,      32
    la.local        t2,       shuf0
    xvld            xr1,      t2,      0
    xvpermi.q       xr0,      xr0,     0x00
    xvaddi.bu        xr9,    xr1,      4
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x00, xr8, 0x00
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x00, xr11, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x10, xr8, 0x10
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x10, xr11, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x20, xr8, 0x20
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x20, xr11, 0x20

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x30, xr8, 0x30
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x30, xr11, 0x30

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr12, 0x00, xr13, 0x00
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr14, 0x00, xr15, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr12, 0x10, xr13, 0x10
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr14, 0x10, xr15, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr12, 0x20, xr13, 0x20
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr14, 0x20, xr15, 0x20

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr12, 0x30, xr13, 0x30
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr14, 0x30, xr15, 0x30

    xvsrarni.h.w    xr12,     xr7,     3
    xvsrarni.h.w    xr13,     xr8,     3
    xvsrarni.h.w    xr14,     xr10,    3
    xvsrarni.h.w    xr15,     xr11,    3

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x00, xr8, 0x00
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x00, xr11, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x10, xr8, 0x10
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x10, xr11, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x20, xr8, 0x20
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x20, xr11, 0x20

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x30, xr8, 0x30
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x30, xr11, 0x30

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr16, 0x00, xr17, 0x00
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr18, 0x00, xr19, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr16, 0x10, xr17, 0x10
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr18, 0x10, xr19, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr16, 0x20, xr17, 0x20
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr18, 0x20, xr19, 0x20

    xvsrarni.h.w    xr16,     xr7,     3
    xvsrarni.h.w    xr17,     xr8,     3
    xvsrarni.h.w    xr18,     xr10,    3
    xvsrarni.h.w    xr19,     xr11,    3

    addi.w          t2,       a6,      0   // my
    ld.h            t7,       a4,      4   // abcd[2]
    ld.h            t8,       a4,      6   // abcd[3]

.ifnb \t
    slli.d          a1,       a1,      1
.endif

    // y = 0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr20, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr20, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr20, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr20, 0x30

    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr21, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr21, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr21, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr21, 0x30

.ifnb \t
    xvssrarni.h.w   xr21,     xr20,     \shift
    xvpermi.q       xr22,     xr21,     0x01
    vilvl.h         vr23,     vr22,     vr21
    vilvh.h         vr21,     vr22,     vr21
    vst             vr23,     a0,       0
    vstx            vr21,     a0,       a1
.else
    xvssrarni.hu.w   xr21,    xr20,     \shift
    xvssrlni.bu.h    xr22,    xr21,     0
    xvpermi.q        xr23,    xr22,     0x01
    vilvl.b          vr21,    vr23,     vr22
    fst.d            f21,     a0,       0
    add.d            a0,      a0,       a1
    vstelm.d         vr21,    a0,       0,     1
.endif

    xvaddi.bu        xr25,     xr25,    2
    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr20, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr20, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr20, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr20, 0x30

    xvaddi.bu        xr25,     xr25,    2
    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr21, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr21, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr21, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr21, 0x30

.ifnb \t
    xvssrarni.h.w   xr21,     xr20,     \shift
    alsl.d          a0,       a1,       a0,     1
    xvpermi.q       xr22,     xr21,     0x01
    vilvl.h         vr23,     vr22,     vr21
    vilvh.h         vr21,     vr22,     vr21
    vst             vr23,     a0,       0
    vstx            vr21,     a0,       a1
.else
    xvssrarni.hu.w   xr21,    xr20,     11
    xvssrlni.bu.h    xr22,    xr21,     0
    xvpermi.q        xr23,    xr22,     0x01
    vilvl.b          vr21,    vr23,     vr22
    add.d            a0,      a0,       a1
    fst.d            f21,     a0,       0
    add.d            a0,      a0,       a1
    vstelm.d         vr21,    a0,       0,     1
.endif

    xvaddi.bu        xr25,     xr25,    2
    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr20, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr20, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr20, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr20, 0x30

    xvaddi.bu        xr25,     xr25,    2
    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr21, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr21, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr21, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr21, 0x30

.ifnb \t
    xvssrarni.h.w   xr21,     xr20,     \shift
    alsl.d          a0,       a1,       a0,     1
    xvpermi.q       xr22,     xr21,     0x01
    vilvl.h         vr23,     vr22,     vr21
    vilvh.h         vr21,     vr22,     vr21
    vst             vr23,     a0,       0
    vstx            vr21,     a0,       a1
.else
    xvssrarni.hu.w   xr21,    xr20,     11
    xvssrlni.bu.h    xr22,    xr21,     0
    xvpermi.q        xr23,    xr22,     0x01
    vilvl.b          vr21,    vr23,     vr22
    add.d            a0,      a0,       a1
    fst.d            f21,     a0,       0
    add.d            a0,      a0,       a1
    vstelm.d         vr21,    a0,       0,     1
.endif

    xvaddi.bu        xr25,     xr25,    2
    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr20, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr20, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr20, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr20, 0x30

    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr21, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr21, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr21, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr21, 0x30

.ifnb \t
    xvssrarni.h.w   xr21,     xr20,     \shift
    alsl.d          a0,       a1,       a0,     1
    xvpermi.q       xr22,     xr21,     0x01
    vilvl.h         vr23,     vr22,     vr21
    vilvh.h         vr21,     vr22,     vr21
    vst             vr23,     a0,       0
    vstx            vr21,     a0,       a1
.else
    xvssrarni.hu.w   xr21,    xr20,     11
    xvssrlni.bu.h    xr22,    xr21,     0
    xvpermi.q        xr23,    xr22,     0x01
    vilvl.b          vr21,    vr23,     vr22
    add.d            a0,      a0,       a1
    fst.d            f21,     a0,       0
    add.d            a0,      a0,       a1
    vstelm.d         vr21,    a0,       0,     1
.endif
    fld.d            f24,     sp,       0
    fld.d            f25,     sp,       8
    addi.d           sp,      sp,       16
endfunc
.endm

warp_lasx , 11
warp_lasx t, 7

/*
static void w_avg_c(pixel *dst, const ptrdiff_t dst_stride,
                    const int16_t *tmp1, const int16_t *tmp2,
                    const int w, int h,
                    const int weight HIGHBD_DECL_SUFFIX)
*/

#define bpc8_sh     5     // sh = intermediate_bits + 1
#define bpcw8_sh    8     // sh = intermediate_bits + 4

#define bpc_sh   bpc8_sh
#define bpcw_sh  bpcw8_sh

function avg_8bpc_lsx
    addi.d        t8,     a0,     0

    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .AVG_LSX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0  // The jump addresses are relative to AVG_LSX_JRTABLE
    add.d         t1,     t1,      t2 // Get absolute address
    jirl          $r0,    t1,      0

    .align   3
.AVG_LSX_JRTABLE:
    .hword .AVG_W128_LSX - .AVG_LSX_JRTABLE
    .hword .AVG_W64_LSX  - .AVG_LSX_JRTABLE
    .hword .AVG_W32_LSX  - .AVG_LSX_JRTABLE
    .hword .AVG_W16_LSX  - .AVG_LSX_JRTABLE
    .hword .AVG_W8_LSX   - .AVG_LSX_JRTABLE
    .hword .AVG_W4_LSX   - .AVG_LSX_JRTABLE

.AVG_W4_LSX:
    vld           vr0,    a2,     0
    vld           vr1,    a3,     0
    vadd.h        vr2,    vr0,    vr1
    vssrarni.bu.h vr3,    vr2,    bpc_sh
    vstelm.w      vr3,    a0,     0,    0
    add.d         a0,     a0,     a1
    vstelm.w      vr3,    a0,     0,    1
    addi.w        a5,     a5,     -2
    addi.d        a2,     a2,     16
    addi.d        a3,     a3,     16
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .AVG_W4_LSX
    b             .AVG_END_LSX

.AVG_W8_LSX:
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vadd.h        vr4,    vr0,    vr1
    vadd.h        vr5,    vr2,    vr3
    vssrarni.bu.h vr5,    vr4,    bpc_sh
    addi.w        a5,     a5,     -2
    addi.d        a2,     a2,     32
    vstelm.d      vr5,    a0,     0,    0
    add.d         a0,     a0,     a1
    vstelm.d      vr5,    a0,     0,    1
    addi.d        a3,     a3,     32
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .AVG_W8_LSX
    b             .AVG_END_LSX

.AVG_W16_LSX:
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vadd.h        vr4,    vr0,    vr1
    vadd.h        vr5,    vr2,    vr3
    vssrarni.bu.h vr5,    vr4,    bpc_sh
    addi.w        a5,     a5,     -1
    addi.d        a2,     a2,     32
    vst           vr5,    a0,     0
    addi.d        a3,     a3,     32
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .AVG_W16_LSX
    b             .AVG_END_LSX

.AVG_W32_LSX:
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr4,    a2,     32
    vld           vr6,    a2,     48
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vld           vr5,    a3,     32
    vld           vr7,    a3,     48
    vadd.h        vr0,    vr0,    vr1
    vadd.h        vr2,    vr2,    vr3
    vadd.h        vr4,    vr4,    vr5
    vadd.h        vr6,    vr6,    vr7
    vssrarni.bu.h vr2,    vr0,    bpc_sh
    vssrarni.bu.h vr6,    vr4,    bpc_sh
    addi.w        a5,     a5,     -1
    addi.d        a2,     a2,     64
    vst           vr2,    a0,     0
    vst           vr6,    a0,     16
    addi.d        a3,     a3,     64
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .AVG_W32_LSX
    b             .AVG_END_LSX

.AVG_W64_LSX:
.rept 4
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vadd.h        vr0,    vr0,    vr1
    vadd.h        vr2,    vr2,    vr3
    vssrarni.bu.h vr2,    vr0,    bpc_sh
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    vst           vr2,    a0,     0
    addi.d        a0,     a0,     16
.endr
    addi.w        a5,     a5,     -1
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    blt           zero,   a5,     .AVG_W64_LSX
    b             .AVG_END_LSX

.AVG_W128_LSX:
.rept 8
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vadd.h        vr0,    vr0,    vr1
    vadd.h        vr2,    vr2,    vr3
    vssrarni.bu.h vr2,    vr0,    bpc_sh
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    vst           vr2,    a0,     0
    addi.d        a0,     a0,     16
.endr
    addi.w        a5,     a5,     -1
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    blt           zero,   a5,     .AVG_W128_LSX
.AVG_END_LSX:
endfunc

function avg_8bpc_lasx
    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .AVG_LASX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0
    add.d         t1,     t1,      t2
    jirl          $r0,    t1,      0

    .align   3
.AVG_LASX_JRTABLE:
    .hword .AVG_W128_LASX - .AVG_LASX_JRTABLE
    .hword .AVG_W64_LASX  - .AVG_LASX_JRTABLE
    .hword .AVG_W32_LASX  - .AVG_LASX_JRTABLE
    .hword .AVG_W16_LASX  - .AVG_LASX_JRTABLE
    .hword .AVG_W8_LASX   - .AVG_LASX_JRTABLE
    .hword .AVG_W4_LASX   - .AVG_LASX_JRTABLE

.AVG_W4_LASX:
    vld            vr0,    a2,     0
    vld            vr1,    a3,     0
    vadd.h         vr0,    vr0,    vr1
    vssrarni.bu.h  vr1,    vr0,    bpc_sh
    vstelm.w       vr1,    a0,     0,    0
    add.d          a0,     a0,     a1
    vstelm.w       vr1,    a0,     0,    1
    addi.w         a5,     a5,     -2
    addi.d         a2,     a2,     16
    addi.d         a3,     a3,     16
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .AVG_W4_LASX
    b              .AVG_END_LASX
.AVG_W8_LASX:
    xvld           xr0,    a2,     0
    xvld           xr1,    a3,     0
    xvadd.h        xr2,    xr0,    xr1
    xvssrarni.bu.h xr1,    xr2,    bpc_sh
    xvstelm.d      xr1,    a0,     0,    0
    add.d          a0,     a0,     a1
    xvstelm.d      xr1,    a0,     0,    2
    addi.w         a5,     a5,     -2
    addi.d         a2,     a2,     32
    addi.d         a3,     a3,     32
    add.d          a0,     a1,     a0
    blt            zero,   a5,     .AVG_W8_LASX
    b              .AVG_END_LASX
.AVG_W16_LASX:
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvadd.h        xr4,    xr0,    xr1
    xvadd.h        xr5,    xr2,    xr3
    xvssrarni.bu.h xr5,    xr4,    bpc_sh
    xvpermi.d      xr2,    xr5,    0xd8
    xvpermi.d      xr3,    xr5,    0x8d
    vst            vr2,    a0,     0
    vstx           vr3,    a0,     a1
    addi.w         a5,     a5,     -2
    addi.d         a2,     a2,     64
    addi.d         a3,     a3,     64
    alsl.d         a0,     a1,     a0,   1
    blt            zero,   a5,     .AVG_W16_LASX
    b              .AVG_END_LASX
.AVG_W32_LASX:
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvadd.h        xr4,    xr0,    xr1
    xvadd.h        xr5,    xr2,    xr3
    xvssrarni.bu.h xr5,    xr4,    bpc_sh
    xvpermi.d      xr6,    xr5,    0xd8
    xvst           xr6,    a0,     0
    addi.w         a5,     a5,     -1
    addi.d         a2,     a2,     64
    addi.d         a3,     a3,     64
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .AVG_W32_LASX
    b              .AVG_END_LASX
.AVG_W64_LASX:
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr4,    a2,     64
    xvld           xr6,    a2,     96
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvld           xr5,    a3,     64
    xvld           xr7,    a3,     96
    xvadd.h        xr0,    xr0,    xr1
    xvadd.h        xr2,    xr2,    xr3
    xvadd.h        xr4,    xr4,    xr5
    xvadd.h        xr6,    xr6,    xr7
    xvssrarni.bu.h xr2,    xr0,    bpc_sh
    xvssrarni.bu.h xr6,    xr4,    bpc_sh
    xvpermi.d      xr1,    xr2,    0xd8
    xvpermi.d      xr3,    xr6,    0xd8
    xvst           xr1,    a0,     0
    xvst           xr3,    a0,     32
    addi.w         a5,     a5,     -1
    addi.d         a2,     a2,     128
    addi.d         a3,     a3,     128
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .AVG_W64_LASX
    b              .AVG_END_LASX
.AVG_W128_LASX:
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr4,    a2,     64
    xvld           xr6,    a2,     96
    xvld           xr8,    a2,     128
    xvld           xr10,   a2,     160
    xvld           xr12,   a2,     192
    xvld           xr14,   a2,     224
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvld           xr5,    a3,     64
    xvld           xr7,    a3,     96
    xvld           xr9,    a3,     128
    xvld           xr11,   a3,     160
    xvld           xr13,   a3,     192
    xvld           xr15,   a3,     224
    xvadd.h        xr0,    xr0,    xr1
    xvadd.h        xr2,    xr2,    xr3
    xvadd.h        xr4,    xr4,    xr5
    xvadd.h        xr6,    xr6,    xr7
    xvadd.h        xr8,    xr8,    xr9
    xvadd.h        xr10,   xr10,   xr11
    xvadd.h        xr12,   xr12,   xr13
    xvadd.h        xr14,   xr14,   xr15
    xvssrarni.bu.h xr2,    xr0,    bpc_sh
    xvssrarni.bu.h xr6,    xr4,    bpc_sh
    xvssrarni.bu.h xr10,   xr8,    bpc_sh
    xvssrarni.bu.h xr14,   xr12,   bpc_sh
    xvpermi.d      xr1,    xr2,    0xd8
    xvpermi.d      xr3,    xr6,    0xd8
    xvpermi.d      xr5,    xr10,   0xd8
    xvpermi.d      xr7,    xr14,   0xd8
    xvst           xr1,    a0,     0
    xvst           xr3,    a0,     32
    xvst           xr5,    a0,     64
    xvst           xr7,    a0,     96
    addi.w         a5,     a5,     -1
    addi.d         a2,     a2,     256
    addi.d         a3,     a3,     256
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .AVG_W128_LASX
.AVG_END_LASX:
endfunc

function w_avg_8bpc_lsx
    addi.d        t8,     a0,     0
    li.w          t2,     16
    sub.w         t2,     t2,     a6  // 16 - weight
    vreplgr2vr.h  vr21,   a6
    vreplgr2vr.h  vr22,   t2

    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .W_AVG_LSX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0
    add.d         t1,     t1,      t2
    jirl          $r0,    t1,      0

    .align   3
.W_AVG_LSX_JRTABLE:
    .hword .W_AVG_W128_LSX - .W_AVG_LSX_JRTABLE
    .hword .W_AVG_W64_LSX  - .W_AVG_LSX_JRTABLE
    .hword .W_AVG_W32_LSX  - .W_AVG_LSX_JRTABLE
    .hword .W_AVG_W16_LSX  - .W_AVG_LSX_JRTABLE
    .hword .W_AVG_W8_LSX   - .W_AVG_LSX_JRTABLE
    .hword .W_AVG_W4_LSX   - .W_AVG_LSX_JRTABLE

.W_AVG_W4_LSX:
    vld           vr0,    a2,     0
    vld           vr1,    a3,     0
    vmulwev.w.h   vr2,    vr0,    vr21
    vmulwod.w.h   vr3,    vr0,    vr21
    vmaddwev.w.h  vr2,    vr1,    vr22
    vmaddwod.w.h  vr3,    vr1,    vr22
    vssrarni.hu.w vr3,    vr2,    bpcw_sh
    vssrlni.bu.h  vr1,    vr3,    0
    vpickod.w     vr4,    vr2,    vr1
    vilvl.b       vr0,    vr4,    vr1
    fst.s         f0,     a0,     0
    add.d         a0,     a0,     a1
    vstelm.w      vr0,    a0,     0,   1
    addi.w        a5,     a5,     -2
    addi.d        a2,     a2,     16
    addi.d        a3,     a3,     16
    add.d         a0,     a1,     a0
    blt           zero,   a5,     .W_AVG_W4_LSX
    b             .W_AVG_END_LSX
.W_AVG_W8_LSX:
    vld           vr0,    a2,     0
    vld           vr1,    a3,     0
    vmulwev.w.h   vr2,    vr0,    vr21
    vmulwod.w.h   vr3,    vr0,    vr21
    vmaddwev.w.h  vr2,    vr1,    vr22
    vmaddwod.w.h  vr3,    vr1,    vr22
    vssrarni.hu.w vr3,    vr2,    bpcw_sh
    vssrlni.bu.h  vr1,    vr3,    0
    vpickod.w     vr4,    vr2,    vr1
    vilvl.b       vr0,    vr4,    vr1
    fst.d         f0,     a0,     0
    addi.w        a5,     a5,     -1
    addi.d        a2,     a2,     16
    addi.d        a3,     a3,     16
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .W_AVG_W8_LSX
    b             .W_AVG_END_LSX
.W_AVG_W16_LSX:
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vmulwev.w.h   vr4,    vr0,    vr21
    vmulwod.w.h   vr5,    vr0,    vr21
    vmulwev.w.h   vr6,    vr2,    vr21
    vmulwod.w.h   vr7,    vr2,    vr21
    vmaddwev.w.h  vr4,    vr1,    vr22
    vmaddwod.w.h  vr5,    vr1,    vr22
    vmaddwev.w.h  vr6,    vr3,    vr22
    vmaddwod.w.h  vr7,    vr3,    vr22
    vssrarni.hu.w vr6,    vr4,    bpcw_sh
    vssrarni.hu.w vr7,    vr5,    bpcw_sh
    vssrlrni.bu.h vr7,    vr6,    0
    vshuf4i.w     vr8,    vr7,    0x4E
    vilvl.b       vr0,    vr8,    vr7
    vst           vr0,    a0,     0
    addi.w        a5,     a5,     -1
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .W_AVG_W16_LSX
    b             .W_AVG_END_LSX
.W_AVG_W32_LSX:
.rept 2
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vmulwev.w.h   vr4,    vr0,    vr21
    vmulwod.w.h   vr5,    vr0,    vr21
    vmulwev.w.h   vr6,    vr2,    vr21
    vmulwod.w.h   vr7,    vr2,    vr21
    vmaddwev.w.h  vr4,    vr1,    vr22
    vmaddwod.w.h  vr5,    vr1,    vr22
    vmaddwev.w.h  vr6,    vr3,    vr22
    vmaddwod.w.h  vr7,    vr3,    vr22
    vssrarni.hu.w vr6,    vr4,    bpcw_sh
    vssrarni.hu.w vr7,    vr5,    bpcw_sh
    vssrlrni.bu.h vr7,    vr6,    0
    vshuf4i.w     vr8,    vr7,    0x4E
    vilvl.b       vr0,    vr8,    vr7
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a0,     a0,     16
.endr
    addi.w        a5,     a5,     -1
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    blt           zero,   a5,     .W_AVG_W32_LSX
    b             .W_AVG_END_LSX

.W_AVG_W64_LSX:
.rept 4
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vmulwev.w.h   vr4,    vr0,    vr21
    vmulwod.w.h   vr5,    vr0,    vr21
    vmulwev.w.h   vr6,    vr2,    vr21
    vmulwod.w.h   vr7,    vr2,    vr21
    vmaddwev.w.h  vr4,    vr1,    vr22
    vmaddwod.w.h  vr5,    vr1,    vr22
    vmaddwev.w.h  vr6,    vr3,    vr22
    vmaddwod.w.h  vr7,    vr3,    vr22
    vssrarni.hu.w vr6,    vr4,    bpcw_sh
    vssrarni.hu.w vr7,    vr5,    bpcw_sh
    vssrlrni.bu.h vr7,    vr6,    0
    vshuf4i.w     vr8,    vr7,    0x4E
    vilvl.b       vr0,    vr8,    vr7
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a0,     a0,     16
.endr
    addi.w        a5,     a5,     -1
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    blt           zero,   a5,     .W_AVG_W64_LSX
    b             .W_AVG_END_LSX

.W_AVG_W128_LSX:
.rept 8
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vmulwev.w.h   vr4,    vr0,    vr21
    vmulwod.w.h   vr5,    vr0,    vr21
    vmulwev.w.h   vr6,    vr2,    vr21
    vmulwod.w.h   vr7,    vr2,    vr21
    vmaddwev.w.h  vr4,    vr1,    vr22
    vmaddwod.w.h  vr5,    vr1,    vr22
    vmaddwev.w.h  vr6,    vr3,    vr22
    vmaddwod.w.h  vr7,    vr3,    vr22
    vssrarni.hu.w vr6,    vr4,    bpcw_sh
    vssrarni.hu.w vr7,    vr5,    bpcw_sh
    vssrlrni.bu.h vr7,    vr6,    0
    vshuf4i.w     vr8,    vr7,    0x4E
    vilvl.b       vr0,    vr8,    vr7
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a0,     a0,     16
.endr
    addi.w        a5,     a5,     -1
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    blt           zero,   a5,     .W_AVG_W128_LSX
.W_AVG_END_LSX:
endfunc

function w_avg_8bpc_lasx
    addi.d        t8,     a0,     0
    li.w          t2,     16
    sub.w         t2,     t2,     a6  // 16 - weight
    xvreplgr2vr.h xr21,   a6
    xvreplgr2vr.h xr22,   t2

    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .W_AVG_LASX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0
    add.d         t1,     t1,      t2
    jirl          $r0,    t1,      0

    .align   3
.W_AVG_LASX_JRTABLE:
    .hword .W_AVG_W128_LASX - .W_AVG_LASX_JRTABLE
    .hword .W_AVG_W64_LASX  - .W_AVG_LASX_JRTABLE
    .hword .W_AVG_W32_LASX  - .W_AVG_LASX_JRTABLE
    .hword .W_AVG_W16_LASX  - .W_AVG_LASX_JRTABLE
    .hword .W_AVG_W8_LASX   - .W_AVG_LASX_JRTABLE
    .hword .W_AVG_W4_LASX   - .W_AVG_LASX_JRTABLE

.W_AVG_W4_LASX:
    vld            vr0,    a2,     0
    vld            vr1,    a3,     0
    xvpermi.d      xr2,    xr0,    0xD8
    xvpermi.d      xr3,    xr1,    0xD8
    xvilvl.h       xr4,    xr3,    xr2
    xvmulwev.w.h   xr0,    xr4,    xr21
    xvmaddwod.w.h  xr0,    xr4,    xr22
    xvssrarni.hu.w xr1,    xr0,    bpcw_sh
    xvssrlni.bu.h  xr0,    xr1,    0
    fst.s          f0,     a0,     0
    add.d          a0,     a0,     a1
    xvstelm.w      xr0,    a0,     0,     4
    addi.w         a5,     a5,     -2
    addi.d         a2,     a2,     16
    addi.d         a3,     a3,     16
    add.d          a0,     a1,     a0
    blt            zero,   a5,     .W_AVG_W4_LASX
    b              .W_AVG_END_LASX

.W_AVG_W8_LASX:
    xvld           xr0,    a2,     0
    xvld           xr1,    a3,     0
    xvmulwev.w.h   xr2,    xr0,    xr21
    xvmulwod.w.h   xr3,    xr0,    xr21
    xvmaddwev.w.h  xr2,    xr1,    xr22
    xvmaddwod.w.h  xr3,    xr1,    xr22
    xvssrarni.hu.w xr3,    xr2,    bpcw_sh
    xvssrlni.bu.h  xr1,    xr3,    0
    xvpickod.w     xr4,    xr2,    xr1
    xvilvl.b       xr0,    xr4,    xr1
    xvstelm.d      xr0,    a0,     0,     0
    add.d          a0,     a0,     a1
    xvstelm.d      xr0,    a0,     0,     2
    addi.w         a5,     a5,     -2
    addi.d         a2,     a2,     32
    addi.d         a3,     a3,     32
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .W_AVG_W8_LASX
    b              .W_AVG_END_LASX

.W_AVG_W16_LASX:
    xvld           xr0,    a2,     0
    xvld           xr1,    a3,     0
    xvmulwev.w.h   xr2,    xr0,    xr21
    xvmulwod.w.h   xr3,    xr0,    xr21
    xvmaddwev.w.h  xr2,    xr1,    xr22
    xvmaddwod.w.h  xr3,    xr1,    xr22
    xvssrarni.hu.w xr3,    xr2,    bpcw_sh
    xvssrlni.bu.h  xr1,    xr3,    0
    xvpickod.w     xr4,    xr2,    xr1
    xvilvl.b       xr0,    xr4,    xr1
    xvpermi.d      xr1,    xr0,    0xD8
    vst            vr1,    a0,     0
    addi.w         a5,     a5,     -1
    addi.d         a2,     a2,     32
    addi.d         a3,     a3,     32
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .W_AVG_W16_LASX
    b              .W_AVG_END_LSX

.W_AVG_W32_LASX:
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvmulwev.w.h   xr4,    xr0,    xr21
    xvmulwod.w.h   xr5,    xr0,    xr21
    xvmulwev.w.h   xr6,    xr2,    xr21
    xvmulwod.w.h   xr7,    xr2,    xr21
    xvmaddwev.w.h  xr4,    xr1,    xr22
    xvmaddwod.w.h  xr5,    xr1,    xr22
    xvmaddwev.w.h  xr6,    xr3,    xr22
    xvmaddwod.w.h  xr7,    xr3,    xr22
    xvssrarni.hu.w xr6,    xr4,    bpcw_sh
    xvssrarni.hu.w xr7,    xr5,    bpcw_sh
    xvssrlni.bu.h  xr7,    xr6,    0
    xvshuf4i.w     xr8,    xr7,    0x4E
    xvilvl.b       xr9,    xr8,    xr7
    xvpermi.d      xr0,    xr9,    0xD8
    xvst           xr0,    a0,     0
    addi.w         a5,     a5,     -1
    addi.d         a2,     a2,     64
    addi.d         a3,     a3,     64
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .W_AVG_W32_LASX
    b              .W_AVG_END_LASX

.W_AVG_W64_LASX:
.rept 2
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvmulwev.w.h   xr4,    xr0,    xr21
    xvmulwod.w.h   xr5,    xr0,    xr21
    xvmulwev.w.h   xr6,    xr2,    xr21
    xvmulwod.w.h   xr7,    xr2,    xr21
    xvmaddwev.w.h  xr4,    xr1,    xr22
    xvmaddwod.w.h  xr5,    xr1,    xr22
    xvmaddwev.w.h  xr6,    xr3,    xr22
    xvmaddwod.w.h  xr7,    xr3,    xr22
    xvssrarni.hu.w xr6,    xr4,    bpcw_sh
    xvssrarni.hu.w xr7,    xr5,    bpcw_sh
    xvssrlni.bu.h  xr7,    xr6,    0
    xvshuf4i.w     xr8,    xr7,    0x4E
    xvilvl.b       xr9,    xr8,    xr7
    xvpermi.d      xr0,    xr9,    0xD8
    xvst           xr0,    a0,     0
    addi.d         a2,     a2,     64
    addi.d         a3,     a3,     64
    addi.d         a0,     a0,     32
.endr
    addi.w         a5,     a5,     -1
    add.d          t8,     t8,     a1
    add.d          a0,     t8,     zero
    blt            zero,   a5,     .W_AVG_W64_LASX
    b              .W_AVG_END_LASX

.W_AVG_W128_LASX:
.rept 4
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvmulwev.w.h   xr4,    xr0,    xr21
    xvmulwod.w.h   xr5,    xr0,    xr21
    xvmulwev.w.h   xr6,    xr2,    xr21
    xvmulwod.w.h   xr7,    xr2,    xr21
    xvmaddwev.w.h  xr4,    xr1,    xr22
    xvmaddwod.w.h  xr5,    xr1,    xr22
    xvmaddwev.w.h  xr6,    xr3,    xr22
    xvmaddwod.w.h  xr7,    xr3,    xr22
    xvssrarni.hu.w xr6,    xr4,    bpcw_sh
    xvssrarni.hu.w xr7,    xr5,    bpcw_sh
    xvssrlni.bu.h  xr7,    xr6,    0
    xvshuf4i.w     xr8,    xr7,    0x4E
    xvilvl.b       xr9,    xr8,    xr7
    xvpermi.d      xr0,    xr9,    0xD8
    xvst           xr0,    a0,     0
    addi.d         a2,     a2,     64
    addi.d         a3,     a3,     64
    addi.d         a0,     a0,     32
.endr

    addi.w         a5,     a5,     -1
    add.d          t8,     t8,     a1
    add.d          a0,     t8,     zero
    blt            zero,   a5,     .W_AVG_W128_LASX
.W_AVG_END_LASX:
endfunc

#undef bpc_sh
#undef bpcw_sh

#define mask_sh         10
/*
static void mask_c(pixel *dst, const ptrdiff_t dst_stride,
                   const int16_t *tmp1, const int16_t *tmp2, const int w, int h,
                   const uint8_t *mask HIGHBD_DECL_SUFFIX)
*/
function mask_8bpc_lsx
    vldi          vr21,   0x440   // 64
    vxor.v        vr19,   vr19,   vr19
    addi.d        t8,     a0,     0
    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .MASK_LSX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0
    add.d         t1,     t1,      t2
    jirl          $r0,    t1,      0

    .align   3
.MASK_LSX_JRTABLE:
    .hword .MASK_W128_LSX - .MASK_LSX_JRTABLE
    .hword .MASK_W64_LSX  - .MASK_LSX_JRTABLE
    .hword .MASK_W32_LSX  - .MASK_LSX_JRTABLE
    .hword .MASK_W16_LSX  - .MASK_LSX_JRTABLE
    .hword .MASK_W8_LSX   - .MASK_LSX_JRTABLE
    .hword .MASK_W4_LSX   - .MASK_LSX_JRTABLE

.MASK_W4_LSX:
    vld           vr0,     a2,     0
    vld           vr1,     a3,     0
    fld.d         f22,     a6,     0

    vilvl.b       vr2,    vr19,   vr22
    vsub.h        vr3,    vr21,   vr2

    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vssrarni.hu.w vr5,    vr4,    mask_sh
    vssrlrni.bu.h vr1,    vr5,    0
    vpickod.w     vr4,    vr2,    vr1
    vilvl.b       vr0,    vr4,    vr1
    fst.s         f0,     a0,     0
    add.d         a0,     a0,     a1
    vstelm.w      vr0,    a0,     0,    1
    addi.d        a2,     a2,     16
    addi.d        a3,     a3,     16
    addi.d        a6,     a6,     8
    add.d         a0,     a0,     a1
    addi.w        a5,     a5,     -2
    blt           zero,   a5,     .MASK_W4_LSX
    b             .MASK_END_LSX
.MASK_W8_LSX:
    vld           vr0,    a2,     0
    vld           vr10,   a2,     16
    vld           vr1,    a3,     0
    vld           vr11,   a3,     16
    vld           vr22,   a6,     0

    vilvl.b       vr2,    vr19,   vr22
    vilvh.b       vr12,   vr19,   vr22
    vsub.h        vr3,    vr21,   vr2
    vsub.h        vr13,   vr21,   vr12

    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmulwev.w.h   vr14,   vr10,   vr12
    vmulwod.w.h   vr15,   vr10,   vr12
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vmaddwev.w.h  vr14,   vr11,   vr13
    vmaddwod.w.h  vr15,   vr11,   vr13
    vssrarni.hu.w vr14,   vr4,    mask_sh
    vssrarni.hu.w vr15,   vr5,    mask_sh
    vssrlrni.bu.h vr15,   vr14,   0
    vshuf4i.w     vr6,    vr15,   0x4E
    vilvl.b       vr0,    vr6,    vr15
    fst.d         f0,     a0,     0
    add.d         a0,     a0,     a1
    vstelm.d      vr0,    a0,     0,   1
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a6,     a6,     16
    add.d         a0,     a0,     a1
    addi.w        a5,     a5,     -2
    blt           zero,   a5,     .MASK_W8_LSX
    b             .MASK_END_LSX

.MASK_W16_LSX:
    vld           vr0,    a2,     0
    vld           vr10,   a2,     16
    vld           vr1,    a3,     0
    vld           vr11,   a3,     16
    vld           vr22,   a6,     0

    vilvl.b       vr2,    vr19,   vr22
    vilvh.b       vr12,   vr19,   vr22
    vsub.h        vr3,    vr21,   vr2
    vsub.h        vr13,   vr21,   vr12

    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmulwev.w.h   vr14,   vr10,   vr12
    vmulwod.w.h   vr15,   vr10,   vr12
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vmaddwev.w.h  vr14,   vr11,   vr13
    vmaddwod.w.h  vr15,   vr11,   vr13
    vssrarni.hu.w vr14,   vr4,    mask_sh
    vssrarni.hu.w vr15,   vr5,    mask_sh
    vssrlrni.bu.h vr15,   vr14,   0
    vshuf4i.w     vr6,    vr15,   0x4E
    vilvl.b       vr0,    vr6,    vr15
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a6,     a6,     16
    add.d         a0,     a0,     a1
    addi.w        a5,     a5,     -1
    blt           zero,   a5,     .MASK_W16_LSX
    b             .MASK_END_LSX
.MASK_W32_LSX:
.rept 2
    vld           vr0,    a2,     0
    vld           vr10,   a2,     16
    vld           vr1,    a3,     0
    vld           vr11,   a3,     16
    vld           vr22,   a6,     0
    vilvl.b       vr2,    vr19,   vr22
    vilvh.b       vr12,   vr19,   vr22
    vsub.h        vr3,    vr21,   vr2
    vsub.h        vr13,   vr21,   vr12
    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmulwev.w.h   vr14,   vr10,   vr12
    vmulwod.w.h   vr15,   vr10,   vr12
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vmaddwev.w.h  vr14,   vr11,   vr13
    vmaddwod.w.h  vr15,   vr11,   vr13
    vssrarni.hu.w vr14,   vr4,    mask_sh
    vssrarni.hu.w vr15,   vr5,    mask_sh
    vssrlrni.bu.h vr15,   vr14,   0
    vshuf4i.w     vr6,    vr15,   0x4E
    vilvl.b       vr0,    vr6,    vr15
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a6,     a6,     16
    addi.d        a0,     a0,     16
.endr
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    addi.w        a5,     a5,     -1
    blt           zero,   a5,     .MASK_W32_LSX
    b             .MASK_END_LSX
.MASK_W64_LSX:
.rept 4
    vld           vr0,    a2,     0
    vld           vr10,   a2,     16
    vld           vr1,    a3,     0
    vld           vr11,   a3,     16
    vld           vr22,   a6,     0
    vilvl.b       vr2,    vr19,   vr22
    vilvh.b       vr12,   vr19,   vr22
    vsub.h        vr3,    vr21,   vr2
    vsub.h        vr13,   vr21,   vr12
    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmulwev.w.h   vr14,   vr10,   vr12
    vmulwod.w.h   vr15,   vr10,   vr12
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vmaddwev.w.h  vr14,   vr11,   vr13
    vmaddwod.w.h  vr15,   vr11,   vr13
    vssrarni.hu.w vr14,   vr4,    mask_sh
    vssrarni.hu.w vr15,   vr5,    mask_sh
    vssrlrni.bu.h vr15,   vr14,   0
    vshuf4i.w     vr6,    vr15,   0x4E
    vilvl.b       vr0,    vr6,    vr15
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a6,     a6,     16
    addi.d        a0,     a0,     16
.endr
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    addi.w        a5,     a5,     -1
    blt           zero,   a5,     .MASK_W64_LSX
    b             .MASK_END_LSX
.MASK_W128_LSX:
.rept 8
    vld           vr0,    a2,     0
    vld           vr10,   a2,     16
    vld           vr1,    a3,     0
    vld           vr11,   a3,     16
    vld           vr22,   a6,     0
    vilvl.b       vr2,    vr19,   vr22
    vilvh.b       vr12,   vr19,   vr22
    vsub.h        vr3,    vr21,   vr2
    vsub.h        vr13,   vr21,   vr12
    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmulwev.w.h   vr14,   vr10,   vr12
    vmulwod.w.h   vr15,   vr10,   vr12
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vmaddwev.w.h  vr14,   vr11,   vr13
    vmaddwod.w.h  vr15,   vr11,   vr13
    vssrarni.hu.w vr14,   vr4,    mask_sh
    vssrarni.hu.w vr15,   vr5,    mask_sh
    vssrlrni.bu.h vr15,   vr14,   0
    vshuf4i.w     vr6,    vr15,   0x4E
    vilvl.b       vr0,    vr6,    vr15
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a6,     a6,     16
    addi.d        a0,     a0,     16
.endr
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    addi.w        a5,     a5,     -1
    blt           zero,   a5,     .MASK_W128_LSX
.MASK_END_LSX:
endfunc

function mask_8bpc_lasx
    xvldi         xr21,   0x440   // 64
    xvxor.v       xr19,   xr19,   xr19
    addi.d        t8,     a0,     0
    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .MASK_LASX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0
    add.d         t1,     t1,      t2
    jirl          $r0,    t1,      0

    .align   3
.MASK_LASX_JRTABLE:
    .hword .MASK_W128_LASX - .MASK_LASX_JRTABLE
    .hword .MASK_W64_LASX  - .MASK_LASX_JRTABLE
    .hword .MASK_W32_LASX  - .MASK_LASX_JRTABLE
    .hword .MASK_W16_LASX  - .MASK_LASX_JRTABLE
    .hword .MASK_W8_LASX   - .MASK_LASX_JRTABLE
    .hword .MASK_W4_LASX   - .MASK_LASX_JRTABLE

.MASK_W4_LASX:
    vld            vr0,    a2,     0
    vld            vr1,    a3,     0
    fld.d          f22,    a6,     0

    vilvl.h        vr4,    vr1,    vr0
    vilvh.h        vr14,   vr1,    vr0
    vilvl.b        vr2,    vr19,   vr22
    vsub.h         vr3,    vr21,   vr2
    xvpermi.q      xr14,   xr4,    0x20
    vilvl.h        vr5,    vr3,    vr2
    vilvh.h        vr15,   vr3,    vr2
    xvpermi.q      xr15,   xr5,    0x20
    xvmulwev.w.h   xr0,    xr14,   xr15
    xvmaddwod.w.h  xr0,    xr14,   xr15
    xvssrarni.hu.w xr1,    xr0,    mask_sh
    xvssrlni.bu.h  xr2,    xr1,    0
    fst.s          f2,     a0,     0
    add.d          a0,     a0,     a1
    xvstelm.w      xr2,    a0,     0,    4

    addi.d         a2,     a2,     16
    addi.d         a3,     a3,     16
    addi.d         a6,     a6,     8
    add.d          a0,     a0,     a1
    addi.w         a5,     a5,     -2
    blt            zero,   a5,     .MASK_W4_LASX
    b              .MASK_END_LASX

.MASK_W8_LASX:
    xvld           xr0,    a2,      0
    xvld           xr1,    a3,      0
    vld            vr22,   a6,      0

    vext2xv.hu.bu  xr2,    xr22
    xvsub.h        xr3,    xr21,    xr2
    xvmulwev.w.h   xr4,    xr0,     xr2
    xvmulwod.w.h   xr5,    xr0,     xr2
    xvmaddwev.w.h  xr4,    xr1,     xr3
    xvmaddwod.w.h  xr5,    xr1,     xr3
    xvssrarni.hu.w xr5,    xr4,     mask_sh
    xvssrlni.bu.h  xr1,    xr5,     0
    xvpickod.w     xr4,    xr2,     xr1
    xvilvl.b       xr0,    xr4,     xr1
    fst.d          f0,     a0,      0
    add.d          a0,     a0,      a1
    xvstelm.d      xr0,    a0,      0,    2

    addi.d         a2,     a2,      32
    addi.d         a3,     a3,      32
    addi.d         a6,     a6,      16
    add.d          a0,     a0,      a1
    addi.w         a5,     a5,      -2
    blt            zero,   a5,      .MASK_W8_LASX
    b              .MASK_END_LASX

.MASK_W16_LASX:
    xvld           xr0,    a2,      0
    xvld           xr1,    a3,      0
    vld            vr22,   a6,      0

    vext2xv.hu.bu  xr2,    xr22
    xvsub.h        xr3,    xr21,    xr2
    xvmulwev.w.h   xr4,    xr0,     xr2
    xvmulwod.w.h   xr5,    xr0,     xr2
    xvmaddwev.w.h  xr4,    xr1,     xr3
    xvmaddwod.w.h  xr5,    xr1,     xr3
    xvssrarni.hu.w xr5,    xr4,     mask_sh
    xvssrlni.bu.h  xr1,    xr5,     0
    xvpickod.w     xr4,    xr2,    xr1
    xvilvl.b       xr0,    xr4,    xr1
    xvpermi.d      xr1,    xr0,     0xD8
    vst            vr1,    a0,      0

    addi.d         a2,     a2,      32
    addi.d         a3,     a3,      32
    addi.d         a6,     a6,      16
    add.d          a0,     a0,      a1
    addi.w         a5,     a5,      -1
    blt            zero,   a5,      .MASK_W16_LASX
    b              .MASK_END_LASX
.MASK_W32_LASX:
    xvld           xr0,    a2,      0
    xvld           xr10,   a2,      32
    xvld           xr1,    a3,      0
    xvld           xr11,   a3,      32
    xvld           xr22,   a6,      0
    vext2xv.hu.bu  xr2,    xr22
    xvpermi.q      xr4,    xr22,    0x01
    vext2xv.hu.bu  xr12,   xr4
    xvsub.h        xr3,    xr21,    xr2
    xvsub.h        xr13,   xr21,    xr12

    xvmulwev.w.h   xr4,    xr0,     xr2
    xvmulwod.w.h   xr5,    xr0,     xr2
    xvmulwev.w.h   xr14,   xr10,    xr12
    xvmulwod.w.h   xr15,   xr10,    xr12
    xvmaddwev.w.h  xr4,    xr1,     xr3
    xvmaddwod.w.h  xr5,    xr1,     xr3
    xvmaddwev.w.h  xr14,   xr11,    xr13
    xvmaddwod.w.h  xr15,   xr11,    xr13
    xvssrarni.hu.w xr14,   xr4,     mask_sh
    xvssrarni.hu.w xr15,   xr5,     mask_sh
    xvssrlni.bu.h  xr15,   xr14,    0
    xvshuf4i.w     xr6,    xr15,    0x4E
    xvilvl.b       xr1,    xr6,     xr15
    xvpermi.d      xr0,    xr1,     0xD8
    xvst           xr0,    a0,      0

    addi.d         a2,     a2,      64
    addi.d         a3,     a3,      64
    addi.d         a6,     a6,      32
    add.d          a0,     a0,      a1
    addi.w         a5,     a5,      -1
    blt            zero,   a5,      .MASK_W32_LASX
    b              .MASK_END_LASX

.MASK_W64_LASX:
.rept 2
    xvld           xr0,    a2,      0
    xvld           xr10,   a2,      32
    xvld           xr1,    a3,      0
    xvld           xr11,   a3,      32
    xvld           xr22,   a6,      0
    vext2xv.hu.bu  xr2,    xr22
    xvpermi.q      xr4,    xr22,    0x01
    vext2xv.hu.bu  xr12,   xr4
    xvsub.h        xr3,    xr21,    xr2
    xvsub.h        xr13,   xr21,    xr12

    xvmulwev.w.h   xr4,    xr0,     xr2
    xvmulwod.w.h   xr5,    xr0,     xr2
    xvmulwev.w.h   xr14,   xr10,    xr12
    xvmulwod.w.h   xr15,   xr10,    xr12
    xvmaddwev.w.h  xr4,    xr1,     xr3
    xvmaddwod.w.h  xr5,    xr1,     xr3
    xvmaddwev.w.h  xr14,   xr11,    xr13
    xvmaddwod.w.h  xr15,   xr11,    xr13
    xvssrarni.hu.w xr14,   xr4,     mask_sh
    xvssrarni.hu.w xr15,   xr5,     mask_sh
    xvssrlni.bu.h  xr15,   xr14,    0
    xvshuf4i.w     xr6,    xr15,    0x4E
    xvilvl.b       xr1,    xr6,     xr15
    xvpermi.d      xr0,    xr1,     0xD8
    xvst           xr0,    a0,      0
    addi.d         a2,     a2,      64
    addi.d         a3,     a3,      64
    addi.d         a6,     a6,      32
    addi.d         a0,     a0,      32
.endr
    add.d          t8,     t8,     a1
    add.d          a0,     t8,     zero
    addi.w         a5,     a5,      -1
    blt            zero,   a5,      .MASK_W64_LASX
    b              .MASK_END_LASX

.MASK_W128_LASX:
.rept 4
    xvld           xr0,    a2,      0
    xvld           xr10,   a2,      32
    xvld           xr1,    a3,      0
    xvld           xr11,   a3,      32
    xvld           xr22,   a6,      0
    vext2xv.hu.bu  xr2,    xr22
    xvpermi.q      xr4,    xr22,    0x01
    vext2xv.hu.bu  xr12,   xr4
    xvsub.h        xr3,    xr21,    xr2
    xvsub.h        xr13,   xr21,    xr12

    xvmulwev.w.h   xr4,    xr0,     xr2
    xvmulwod.w.h   xr5,    xr0,     xr2
    xvmulwev.w.h   xr14,   xr10,    xr12
    xvmulwod.w.h   xr15,   xr10,    xr12
    xvmaddwev.w.h  xr4,    xr1,     xr3
    xvmaddwod.w.h  xr5,    xr1,     xr3
    xvmaddwev.w.h  xr14,   xr11,    xr13
    xvmaddwod.w.h  xr15,   xr11,    xr13
    xvssrarni.hu.w xr14,   xr4,     mask_sh
    xvssrarni.hu.w xr15,   xr5,     mask_sh
    xvssrlni.bu.h  xr15,   xr14,    0
    xvshuf4i.w     xr6,    xr15,    0x4E
    xvilvl.b       xr1,    xr6,     xr15
    xvpermi.d      xr0,    xr1,     0xD8
    xvst           xr0,    a0,      0

    addi.d         a2,     a2,      64
    addi.d         a3,     a3,      64
    addi.d         a6,     a6,      32
    addi.d         a0,     a0,      32
.endr
    add.d          t8,     t8,     a1
    add.d          a0,     t8,     zero
    addi.w         a5,     a5,      -1
    blt            zero,   a5,      .MASK_W128_LASX
.MASK_END_LASX:
endfunc

/*
static void w_mask_c(pixel *dst, const ptrdiff_t dst_stride,
                     const int16_t *tmp1, const int16_t *tmp2, const int w, int h,
                     uint8_t *mask, const int sign,
                     const int ss_hor, const int ss_ver HIGHBD_DECL_SUFFIX)
*/
function w_mask_420_8bpc_lsx
    addi.d        sp,      sp,    -24
    fst.d         f24,     sp,    0
    fst.d         f25,     sp,    8
    fst.d         f26,     sp,    16
    vldi          vr20,    0x440
    vreplgr2vr.h  vr21,    a7
    vldi          vr22,    0x426

    clz.w         t0,      a4
    li.w          t1,      24
    sub.w         t0,      t0,      t1
    la.local      t1,      .WMASK420_LSX_JRTABLE
    alsl.d        t0,      t0,      t1,    1
    ld.h          t8,      t0,      0
    add.d         t1,      t1,      t8
    jirl          $r0,     t1,      0

    .align   3
.WMASK420_LSX_JRTABLE:
    .hword .WMASK420_W128_LSX - .WMASK420_LSX_JRTABLE
    .hword .WMASK420_W64_LSX  - .WMASK420_LSX_JRTABLE
    .hword .WMASK420_W32_LSX  - .WMASK420_LSX_JRTABLE
    .hword .WMASK420_W16_LSX  - .WMASK420_LSX_JRTABLE
    .hword .WMASK420_W8_LSX   - .WMASK420_LSX_JRTABLE
    .hword .WMASK420_W4_LSX   - .WMASK420_LSX_JRTABLE

.WMASK420_W4_LSX:
    vld           vr0,     a2,       0
    vld           vr1,     a2,       16
    vld           vr2,     a3,       0
    vld           vr3,     a3,       16
    addi.w        a5,      a5,       -4

    vabsd.h       vr4,     vr0,      vr2
    vabsd.h       vr5,     vr1,      vr3
    vaddi.hu      vr4,     vr4,      8
    vaddi.hu      vr5,     vr5,      8
    vsrli.h       vr4,     vr4,      8
    vsrli.h       vr5,     vr5,      8
    vadd.h        vr4,     vr4,      vr22
    vadd.h        vr5,     vr5,      vr22
    vmin.hu       vr6,     vr4,      vr20
    vmin.hu       vr7,     vr5,      vr20
    vsub.h        vr8,     vr20,     vr6
    vsub.h        vr9,     vr20,     vr7
    vmulwev.w.h   vr4,     vr6,      vr0
    vmulwod.w.h   vr5,     vr6,      vr0
    vmulwev.w.h   vr10,    vr7,      vr1
    vmulwod.w.h   vr11,    vr7,      vr1
    vmaddwev.w.h  vr4,     vr8,      vr2
    vmaddwod.w.h  vr5,     vr8,      vr2
    vmaddwev.w.h  vr10,    vr9,      vr3
    vmaddwod.w.h  vr11,    vr9,      vr3
    vilvl.w       vr0,     vr5,      vr4
    vilvh.w       vr1,     vr5,      vr4
    vilvl.w       vr2,     vr11,     vr10
    vilvh.w       vr3,     vr11,     vr10
    vssrarni.hu.w vr1,     vr0,      10
    vssrarni.hu.w vr3,     vr2,      10
    vssrlni.bu.h  vr3,     vr1,      0
    vstelm.w      vr3,     a0,       0,    0
    add.d         a0,      a0,       a1
    vstelm.w      vr3,     a0,       0,    1
    add.d         a0,      a0,       a1
    vstelm.w      vr3,     a0,       0,    2
    add.d         a0,      a0,       a1
    vstelm.w      vr3,     a0,       0,    3
    add.d         a0,      a0,       a1
    vpickev.h     vr0,     vr7,      vr6
    vpickod.h     vr1,     vr7,      vr6
    vadd.h        vr0,     vr0,      vr1
    vshuf4i.h     vr0,     vr0,      0xd8
    vhaddw.w.h    vr2,     vr0,      vr0
    vpickev.h     vr2,     vr2,      vr2
    vsub.h        vr2,     vr2,      vr21
    vaddi.hu      vr2,     vr2,      2
    vssrani.bu.h  vr2,     vr2,      2
    vstelm.w      vr2,     a6,       0,    0

    addi.d        a2,      a2,       32
    addi.d        a3,      a3,       32
    addi.d        a6,      a6,       4
    blt           zero,    a5,       .WMASK420_W4_LSX
    b             .END_W420

.WMASK420_W8_LSX:
    vld           vr0,     a2,       0
    vld           vr1,     a2,       16
    vld           vr2,     a3,       0
    vld           vr3,     a3,       16
    addi.w        a5,      a5,       -2

    vabsd.h       vr4,     vr0,      vr2
    vabsd.h       vr5,     vr1,      vr3
    vaddi.hu      vr4,     vr4,      8
    vaddi.hu      vr5,     vr5,      8
    vsrli.h       vr4,     vr4,      8
    vsrli.h       vr5,     vr5,      8
    vadd.h        vr4,     vr4,      vr22
    vadd.h        vr5,     vr5,      vr22
    vmin.hu       vr6,     vr4,      vr20
    vmin.hu       vr7,     vr5,      vr20
    vsub.h        vr8,     vr20,     vr6
    vsub.h        vr9,     vr20,     vr7
    vmulwev.w.h   vr4,     vr6,      vr0
    vmulwod.w.h   vr5,     vr6,      vr0
    vmulwev.w.h   vr10,    vr7,      vr1
    vmulwod.w.h   vr11,    vr7,      vr1
    vmaddwev.w.h  vr4,     vr8,      vr2
    vmaddwod.w.h  vr5,     vr8,      vr2
    vmaddwev.w.h  vr10,    vr9,      vr3
    vmaddwod.w.h  vr11,    vr9,      vr3
    vssrarni.hu.w vr10,    vr4,      10
    vssrarni.hu.w vr11,    vr5,      10
    vssrlni.bu.h  vr11,    vr10,     0
    vshuf4i.w     vr0,     vr11,     0x4E
    vilvl.b       vr3,     vr0,      vr11
    vstelm.d      vr3,     a0,       0,     0
    add.d         a0,      a0,       a1
    vstelm.d      vr3,     a0,       0,     1
    add.d         a0,      a0,       a1
    vpickev.h     vr0,     vr7,      vr6
    vpickod.h     vr1,     vr7,      vr6
    vadd.h        vr0,     vr0,      vr1
    vilvh.d       vr2,     vr0,      vr0
    vadd.h        vr2,     vr2,      vr0
    vsub.h        vr2,     vr2,      vr21
    vaddi.hu      vr2,     vr2,      2
    vssrani.bu.h  vr2,     vr2,      2
    vstelm.w      vr2,     a6,       0,     0

    addi.d        a2,      a2,       32
    addi.d        a3,      a3,       32
    addi.d        a6,      a6,       4
    blt           zero,    a5,       .WMASK420_W8_LSX
    b             .END_W420

.WMASK420_W16_LSX:
    vld           vr0,     a2,       0
    vld           vr1,     a2,       16
    alsl.d        a2,      a4,       a2,    1
    vld           vr2,     a2,       0
    vld           vr3,     a2,       16
    vld           vr4,     a3,       0
    vld           vr5,     a3,       16
    alsl.d        a3,      a4,       a3,    1
    vld           vr6,     a3,       0
    vld           vr7,     a3,       16

    vabsd.h       vr8,     vr0,      vr4
    vabsd.h       vr9,     vr1,      vr5
    vabsd.h       vr10,    vr2,      vr6
    vabsd.h       vr11,    vr3,      vr7
    vaddi.hu      vr8,     vr8,      8
    vaddi.hu      vr9,     vr9,      8
    vaddi.hu      vr10,    vr10,     8
    vaddi.hu      vr11,    vr11,     8
    vsrli.h       vr8,     vr8,      8
    vsrli.h       vr9,     vr9,      8
    vsrli.h       vr10,    vr10,     8
    vsrli.h       vr11,    vr11,     8
    vadd.h        vr8,     vr8,      vr22
    vadd.h        vr9,     vr9,      vr22
    vadd.h        vr10,    vr10,     vr22
    vadd.h        vr11,    vr11,     vr22
    vmin.hu       vr12,    vr8,      vr20
    vmin.hu       vr13,    vr9,      vr20
    vmin.hu       vr14,    vr10,     vr20
    vmin.hu       vr15,    vr11,     vr20
    vsub.h        vr16,    vr20,     vr12
    vsub.h        vr17,    vr20,     vr13
    vsub.h        vr18,    vr20,     vr14
    vsub.h        vr19,    vr20,     vr15
    vmulwev.w.h   vr8,     vr12,     vr0
    vmulwod.w.h   vr9,     vr12,     vr0
    vmulwev.w.h   vr10,    vr13,     vr1
    vmulwod.w.h   vr11,    vr13,     vr1
    vmulwev.w.h   vr23,    vr14,     vr2
    vmulwod.w.h   vr24,    vr14,     vr2
    vmulwev.w.h   vr25,    vr15,     vr3
    vmulwod.w.h   vr26,    vr15,     vr3
    vmaddwev.w.h  vr8,     vr16,     vr4
    vmaddwod.w.h  vr9,     vr16,     vr4
    vmaddwev.w.h  vr10,    vr17,     vr5
    vmaddwod.w.h  vr11,    vr17,     vr5
    vmaddwev.w.h  vr23,    vr18,     vr6
    vmaddwod.w.h  vr24,    vr18,     vr6
    vmaddwev.w.h  vr25,    vr19,     vr7
    vmaddwod.w.h  vr26,    vr19,     vr7
    vssrarni.hu.w vr10,    vr8,      10
    vssrarni.hu.w vr11,    vr9,      10
    vssrarni.hu.w vr25,    vr23,     10
    vssrarni.hu.w vr26,    vr24,     10
    vssrlni.bu.h  vr11,    vr10,     0
    vssrlni.bu.h  vr26,    vr25,     0
    vshuf4i.w     vr0,     vr11,     0x4E
    vshuf4i.w     vr1,     vr26,     0x4E
    vilvl.b       vr3,     vr0,      vr11
    vilvl.b       vr7,     vr1,      vr26
    vst           vr3,     a0,       0
    vstx          vr7,     a0,       a1
    vpickev.h     vr0,     vr13,     vr12
    vpickod.h     vr1,     vr13,     vr12
    vpickev.h     vr2,     vr15,     vr14
    vpickod.h     vr3,     vr15,     vr14
    vadd.h        vr4,     vr0,      vr1
    vadd.h        vr5,     vr2,      vr3
    vadd.h        vr4,     vr4,      vr5
    vsub.h        vr4,     vr4,      vr21
    vssrarni.bu.h vr4,     vr4,      2
    vstelm.d      vr4,     a6,       0,    0

    alsl.d        a2,      a4,       a2,   1
    alsl.d        a3,      a4,       a3,   1
    alsl.d        a0,      a1,       a0,   1
    addi.d        a6,      a6,       8
    addi.w        a5,      a5,       -2
    blt           zero,    a5,       .WMASK420_W16_LSX
    b    .END_W420

.WMASK420_W32_LSX:
.WMASK420_W64_LSX:
.WMASK420_W128_LSX:

.LOOP_W32_420_LSX:
    add.d         t1,       a2,       zero
    add.d         t2,       a3,       zero
    add.d         t3,       a0,       zero
    add.d         t4,       a6,       zero
    alsl.d        t5,       a4,       t1,     1
    alsl.d        t6,       a4,       t2,     1
    or            t7,       a4,       a4

.W32_420_LSX:
    vld           vr0,      t1,       0
    vld           vr1,      t1,       16
    vld           vr2,      t2,       0
    vld           vr3,      t2,       16
    vld           vr4,      t5,       0
    vld           vr5,      t5,       16
    vld           vr6,      t6,       0
    vld           vr7,      t6,       16
    addi.d        t1,       t1,       32
    addi.d        t2,       t2,       32
    addi.d        t5,       t5,       32
    addi.d        t6,       t6,       32
    addi.w        t7,       t7,       -16
    vabsd.h       vr8,      vr0,      vr2
    vabsd.h       vr9,      vr1,      vr3
    vabsd.h       vr10,     vr4,      vr6
    vabsd.h       vr11,     vr5,      vr7
    vaddi.hu      vr8,      vr8,      8
    vaddi.hu      vr9,      vr9,      8
    vaddi.hu      vr10,     vr10,     8
    vaddi.hu      vr11,     vr11,     8
    vsrli.h       vr8,      vr8,      8
    vsrli.h       vr9,      vr9,      8
    vsrli.h       vr10,     vr10,     8
    vsrli.h       vr11,     vr11,     8
    vadd.h        vr8,      vr8,      vr22
    vadd.h        vr9,      vr9,      vr22
    vadd.h        vr10,     vr10,     vr22
    vadd.h        vr11,     vr11,     vr22
    vmin.hu       vr12,     vr8,      vr20
    vmin.hu       vr13,     vr9,      vr20
    vmin.hu       vr14,     vr10,     vr20
    vmin.hu       vr15,     vr11,     vr20
    vsub.h        vr16,     vr20,     vr12
    vsub.h        vr17,     vr20,     vr13
    vsub.h        vr18,     vr20,     vr14
    vsub.h        vr19,     vr20,     vr15
    vmulwev.w.h   vr8,      vr12,     vr0
    vmulwod.w.h   vr9,      vr12,     vr0
    vmulwev.w.h   vr10,     vr13,     vr1
    vmulwod.w.h   vr11,     vr13,     vr1
    vmulwev.w.h   vr23,     vr14,     vr4
    vmulwod.w.h   vr24,     vr14,     vr4
    vmulwev.w.h   vr25,     vr15,     vr5
    vmulwod.w.h   vr26,     vr15,     vr5
    vmaddwev.w.h  vr8,      vr16,     vr2
    vmaddwod.w.h  vr9,      vr16,     vr2
    vmaddwev.w.h  vr10,     vr17,     vr3
    vmaddwod.w.h  vr11,     vr17,     vr3
    vmaddwev.w.h  vr23,     vr18,     vr6
    vmaddwod.w.h  vr24,     vr18,     vr6
    vmaddwev.w.h  vr25,     vr19,     vr7
    vmaddwod.w.h  vr26,     vr19,     vr7
    vssrarni.hu.w vr10,     vr8,      10
    vssrarni.hu.w vr11,     vr9,      10
    vssrarni.hu.w vr25,     vr23,     10
    vssrarni.hu.w vr26,     vr24,     10
    vssrlni.bu.h  vr11,     vr10,     0
    vssrlni.bu.h  vr26,     vr25,     0
    vshuf4i.w     vr8,      vr11,     0x4E
    vshuf4i.w     vr9,      vr26,     0x4E
    vilvl.b       vr3,      vr8,      vr11
    vilvl.b       vr7,      vr9,      vr26
    vst           vr3,      t3,       0
    vstx          vr7,      a1,       t3
    addi.d        t3,       t3,       16
    vpickev.h     vr8,      vr13,     vr12
    vpickod.h     vr9,      vr13,     vr12
    vpickev.h     vr10,     vr15,     vr14
    vpickod.h     vr11,     vr15,     vr14
    vadd.h        vr8,      vr8,      vr9
    vadd.h        vr10,     vr10,     vr11
    vadd.h        vr12,     vr8,      vr10
    vsub.h        vr12,     vr12,     vr21
    vssrarni.bu.h vr12,     vr12,     2
    vstelm.d      vr12,     t4,       0,     0
    addi.d        t4,       t4,       8
    bne           t7,       zero,     .W32_420_LSX

    alsl.d        a2,       a4,       a2,     2
    alsl.d        a3,       a4,       a3,     2
    alsl.d        a0,       a1,       a0,     1
    srai.w        t8,       a4,       1
    add.d         a6,       a6,       t8
    addi.w        a5,       a5,       -2
    blt           zero,     a5,       .LOOP_W32_420_LSX

.END_W420:
    fld.d            f24,     sp,    0
    fld.d            f25,     sp,    8
    fld.d            f26,     sp,    16
    addi.d           sp,      sp,    24
endfunc

function w_mask_420_8bpc_lasx
    xvldi          xr20,    0x440
    xvreplgr2vr.h  xr21,    a7
    xvldi          xr22,    0x426

    clz.w          t0,      a4
    li.w           t1,      24
    sub.w          t0,      t0,      t1
    la.local       t1,      .WMASK420_LASX_JRTABLE
    alsl.d         t0,      t0,      t1,    1
    ld.h           t8,      t0,      0
    add.d          t1,      t1,      t8
    jirl           $r0,     t1,      0

    .align   3
.WMASK420_LASX_JRTABLE:
    .hword .WMASK420_W128_LASX - .WMASK420_LASX_JRTABLE
    .hword .WMASK420_W64_LASX  - .WMASK420_LASX_JRTABLE
    .hword .WMASK420_W32_LASX  - .WMASK420_LASX_JRTABLE
    .hword .WMASK420_W16_LASX  - .WMASK420_LASX_JRTABLE
    .hword .WMASK420_W8_LASX   - .WMASK420_LASX_JRTABLE
    .hword .WMASK420_W4_LASX   - .WMASK420_LASX_JRTABLE

.WMASK420_W4_LASX:
    xvld           xr0,     a2,     0
    xvld           xr1,     a3,     0
    addi.w         a5,      a5,     -4

    xvabsd.h       xr2,     xr0,    xr1
    xvaddi.hu      xr2,     xr2,    8
    xvsrli.h       xr2,     xr2,    8
    xvadd.h        xr2,     xr2,    xr22
    xvmin.hu       xr3,     xr2,    xr20
    xvsub.h        xr4,     xr20,   xr3
    xvmulwev.w.h   xr5,     xr3,    xr0
    xvmulwod.w.h   xr6,     xr3,    xr0
    xvmaddwev.w.h  xr5,     xr4,    xr1
    xvmaddwod.w.h  xr6,     xr4,    xr1
    xvilvl.w       xr7,     xr6,    xr5
    xvilvh.w       xr8,     xr6,    xr5
    xvssrarni.hu.w xr8,     xr7,    10
    xvssrlni.bu.h  xr9,     xr8,    0
    vstelm.w       vr9,     a0,     0,     0
    add.d          a0,      a0,     a1
    vstelm.w       vr9,     a0,     0,     1
    add.d          a0,      a0,     a1
    xvstelm.w      xr9,     a0,     0,     4
    add.d          a0,      a0,     a1
    xvstelm.w      xr9,     a0,     0,     5
    add.d          a0,      a0,     a1

    xvhaddw.w.h    xr3,     xr3,    xr3
    xvpermi.d      xr4,     xr3,    0xb1
    xvadd.h        xr3,     xr3,    xr4
    xvpickev.h     xr3,     xr3,    xr3
    xvsub.h        xr3,     xr3,    xr21
    xvssrarni.bu.h xr3,     xr3,    2
    vstelm.h       vr3,     a6,     0,     0
    xvstelm.h      xr3,     a6,     2,     8

    addi.d         a2,     a2,      32
    addi.d         a3,     a3,      32
    addi.d         a6,     a6,      4
    blt            zero,   a5,      .WMASK420_W4_LASX
    b              .END_W420_LASX

.WMASK420_W8_LASX:
    xvld           xr0,      a2,     0
    xvld           xr1,      a2,     32
    xvld           xr2,      a3,     0
    xvld           xr3,      a3,     32
    addi.w         a5,       a5,     -4

    xvabsd.h       xr4,      xr0,    xr2
    xvabsd.h       xr5,      xr1,    xr3
    xvaddi.hu      xr4,      xr4,    8
    xvaddi.hu      xr5,      xr5,    8
    xvsrli.h       xr4,      xr4,    8
    xvsrli.h       xr5,      xr5,    8
    xvadd.h        xr4,      xr4,    xr22
    xvadd.h        xr5,      xr5,    xr22
    xvmin.hu       xr6,      xr4,    xr20
    xvmin.hu       xr7,      xr5,    xr20
    xvsub.h        xr8,      xr20,   xr6
    xvsub.h        xr9,      xr20,   xr7
    xvmulwev.w.h   xr10,     xr6,    xr0
    xvmulwod.w.h   xr11,     xr6,    xr0
    xvmulwev.w.h   xr12,     xr7,    xr1
    xvmulwod.w.h   xr13,     xr7,    xr1
    xvmaddwev.w.h  xr10,     xr8,    xr2
    xvmaddwod.w.h  xr11,     xr8,    xr2
    xvmaddwev.w.h  xr12,     xr9,    xr3
    xvmaddwod.w.h  xr13,     xr9,    xr3
    xvssrarni.hu.w xr12,     xr10,   10
    xvssrarni.hu.w xr13,     xr11,   10
    xvssrlni.bu.h  xr13,     xr12,   0
    xvshuf4i.w     xr1,      xr13,   0x4E
    xvilvl.b       xr17,     xr1,    xr13
    vstelm.d       vr17,     a0,     0,     0
    add.d          a0,       a0,     a1
    xvstelm.d      xr17,     a0,     0,     2
    add.d          a0,       a0,     a1
    xvstelm.d      xr17,     a0,     0,     1
    add.d          a0,       a0,     a1
    xvstelm.d      xr17,     a0,     0,     3
    add.d          a0,       a0,     a1

    xvhaddw.w.h    xr6,      xr6,    xr6
    xvhaddw.w.h    xr7,      xr7,    xr7
    xvpickev.h     xr8,      xr7,    xr6
    xvpermi.q      xr9,      xr8,    0x01
    vadd.h         vr8,      vr8,    vr9
    vsub.h         vr8,      vr8,    vr21
    vssrarni.bu.h  vr8,      vr8,    2
    vstelm.d       vr8,      a6,     0,    0
    addi.d         a2,       a2,     64
    addi.d         a3,       a3,     64
    addi.d         a6,       a6,     8
    blt            zero,     a5,     .WMASK420_W8_LASX
    b              .END_W420_LASX

.WMASK420_W16_LASX:
    xvld           xr0,      a2,     0
    xvld           xr1,      a2,     32
    xvld           xr2,      a3,     0
    xvld           xr3,      a3,     32
    addi.w         a5,       a5,     -2

    xvabsd.h       xr4,      xr0,    xr2
    xvabsd.h       xr5,      xr1,    xr3
    xvaddi.hu      xr4,      xr4,    8
    xvaddi.hu      xr5,      xr5,    8
    xvsrli.h       xr4,      xr4,    8
    xvsrli.h       xr5,      xr5,    8
    xvadd.h        xr4,      xr4,    xr22
    xvadd.h        xr5,      xr5,    xr22
    xvmin.hu       xr4,      xr4,    xr20
    xvmin.hu       xr5,      xr5,    xr20
    xvsub.h        xr6,      xr20,   xr4
    xvsub.h        xr7,      xr20,   xr5
    xvmulwev.w.h   xr8,      xr4,    xr0
    xvmulwod.w.h   xr9,      xr4,    xr0
    xvmulwev.w.h   xr10,     xr5,    xr1
    xvmulwod.w.h   xr11,     xr5,    xr1
    xvmaddwev.w.h  xr8,      xr6,    xr2
    xvmaddwod.w.h  xr9,      xr6,    xr2
    xvmaddwev.w.h  xr10,     xr7,    xr3
    xvmaddwod.w.h  xr11,     xr7,    xr3
    xvssrarni.hu.w xr10,     xr8,    10
    xvssrarni.hu.w xr11,     xr9,    10
    xvssrlni.bu.h  xr11,     xr10,   0
    xvshuf4i.w     xr8,      xr11,   0x4E
    xvilvl.b       xr15,     xr8,    xr11
    xvpermi.d      xr16,     xr15,   0xd8
    vst            vr16,     a0,     0
    add.d          a0,       a0,     a1
    xvpermi.q      xr16,     xr16,   0x01
    vst            vr16,     a0,     0
    add.d          a0,       a0,     a1

    xvhaddw.w.h    xr4,      xr4,    xr4
    xvhaddw.w.h    xr5,      xr5,    xr5
    xvadd.h        xr4,      xr5,    xr4
    xvpickev.h     xr6,      xr4,    xr4
    xvpermi.d      xr7,      xr6,    0x08
    vsub.h         vr7,      vr7,    vr21
    vssrarni.bu.h  vr7,      vr7,    2
    vstelm.d       vr7,      a6,     0,    0

    addi.d         a2,       a2,     64
    addi.d         a3,       a3,     64
    addi.d         a6,       a6,     8
    blt            zero,     a5,     .WMASK420_W16_LASX
    b              .END_W420_LASX

.WMASK420_W32_LASX:
.WMASK420_W64_LASX:
.WMASK420_W128_LASX:

.LOOP_W32_420_LASX:
    add.d          t1,       a2,       zero
    add.d          t2,       a3,       zero
    add.d          t3,       a0,       zero
    add.d          t4,       a6,       zero
    alsl.d         t5,       a4,       t1,     1
    alsl.d         t6,       a4,       t2,     1
    or             t7,       a4,       a4
.W32_420_LASX:
    xvld           xr0,      t1,       0
    xvld           xr1,      t2,       0
    xvld           xr2,      t5,       0
    xvld           xr3,      t6,       0
    addi.d         t1,       t1,       32
    addi.d         t2,       t2,       32
    addi.d         t5,       t5,       32
    addi.d         t6,       t6,       32
    addi.w         t7,       t7,       -16
    xvabsd.h       xr4,      xr0,      xr1
    xvabsd.h       xr5,      xr2,      xr3
    xvaddi.hu      xr4,      xr4,      8
    xvaddi.hu      xr5,      xr5,      8
    xvsrli.h       xr4,      xr4,      8
    xvsrli.h       xr5,      xr5,      8
    xvadd.h        xr4,      xr4,      xr22
    xvadd.h        xr5,      xr5,      xr22
    xvmin.hu       xr6,      xr4,      xr20
    xvmin.hu       xr7,      xr5,      xr20
    xvsub.h        xr8,      xr20,     xr6
    xvsub.h        xr9,      xr20,     xr7
    xvmulwev.w.h   xr10,     xr6,      xr0
    xvmulwod.w.h   xr11,     xr6,      xr0
    xvmulwev.w.h   xr12,     xr7,      xr2
    xvmulwod.w.h   xr13,     xr7,      xr2
    xvmaddwev.w.h  xr10,     xr8,      xr1
    xvmaddwod.w.h  xr11,     xr8,      xr1
    xvmaddwev.w.h  xr12,     xr9,      xr3
    xvmaddwod.w.h  xr13,     xr9,      xr3
    xvssrarni.hu.w xr12,     xr10,     10
    xvssrarni.hu.w xr13,     xr11,     10
    xvssrlni.bu.h  xr13,     xr12,     0
    xvshuf4i.w     xr10,     xr13,     0x4E
    xvilvl.b       xr17,     xr10,     xr13
    xvpermi.d      xr18,     xr17,     0x08
    xvpermi.d      xr19,     xr17,     0x0d
    vst            vr18,     t3,       0
    vstx           vr19,     t3,       a1
    addi.d         t3,       t3,       16

    xvhaddw.w.h    xr6,      xr6,      xr6
    xvhaddw.w.h    xr7,      xr7,      xr7
    xvadd.h        xr6,      xr7,      xr6
    xvpickev.h     xr7,      xr6,      xr6
    xvpermi.d      xr8,      xr7,      0x08
    vsub.h         vr9,      vr8,      vr21
    vssrarni.bu.h  vr9,      vr9,      2
    vstelm.d       vr9,      t4,       0,      0
    addi.d         t4,       t4,       8
    bne            t7,       zero,     .W32_420_LASX

    alsl.d         a2,       a4,       a2,     2
    alsl.d         a3,       a4,       a3,     2
    alsl.d         a0,       a1,       a0,     1
    srai.w         t8,       a4,       1
    add.d          a6,       a6,       t8
    addi.w         a5,       a5,       -2
    blt            zero,     a5,       .LOOP_W32_420_LASX

.END_W420_LASX:
endfunc

#undef bpc_sh
#undef bpcw_sh

.macro  vhaddw.d.h  in0
    vhaddw.w.h  \in0,  \in0,  \in0
    vhaddw.d.w  \in0,  \in0,  \in0
.endm
.macro  vhaddw.q.w  in0
    vhaddw.d.w  \in0,  \in0,  \in0
    vhaddw.q.d  \in0,  \in0,  \in0
.endm
.macro PUT_H_8W in0
    vbsrl.v          vr2,    \in0,  1
    vbsrl.v          vr3,    \in0,  2
    vbsrl.v          vr4,    \in0,  3
    vbsrl.v          vr5,    \in0,  4
    vbsrl.v          vr6,    \in0,  5
    vbsrl.v          vr7,    \in0,  6
    vbsrl.v          vr10,   \in0,  7
    vilvl.d          vr2,    vr2,   \in0
    vilvl.d          vr3,    vr4,   vr3
    vilvl.d          vr4,    vr6,   vr5
    vilvl.d          vr5,    vr10,  vr7
    vdp2.h.bu.b      \in0,   vr2,   vr8
    vdp2.h.bu.b      vr2,    vr3,   vr8
    vdp2.h.bu.b      vr3,    vr4,   vr8
    vdp2.h.bu.b      vr4,    vr5,   vr8
    vhaddw.d.h       \in0
    vhaddw.d.h       vr2
    vhaddw.d.h       vr3
    vhaddw.d.h       vr4
    vpickev.w        \in0,   vr2,   \in0
    vpickev.w        vr2,    vr4,   vr3
    vpickev.h        \in0,   vr2,   \in0
    vadd.h           \in0,   \in0,  vr9
.endm
.macro FILTER_8TAP_4W in0
    vbsrl.v          vr10,   \in0,  1
    vbsrl.v          vr11,   \in0,  2
    vbsrl.v          vr12,   \in0,  3
    vilvl.d          vr10,   vr10, \in0
    vilvl.d          vr11,   vr12,  vr11
    vdp2.h.bu.b      vr7,    vr10,  vr8
    vdp2.h.bu.b      vr10,   vr11,  vr8
    vhaddw.d.h       vr7
    vhaddw.d.h       vr10
    vpickev.w        \in0,   vr10,  vr7
.endm
.macro FILTER_8TAP_8W in0
    vbsrl.v         vr10,    \in0,  1
    vbsrl.v         vr11,    \in0,  2
    vbsrl.v         vr12,    \in0,  3
    vbsrl.v         vr13,    \in0,  4
    vbsrl.v         vr14,    \in0,  5
    vbsrl.v         vr15,    \in0,  6
    vbsrl.v         vr16,    \in0,  7
    vilvl.d         vr10,    vr10,  \in0
    vilvl.d         vr11,    vr12,  vr11
    vilvl.d         vr12,    vr14,  vr13
    vilvl.d         vr13,    vr16,  vr15
    vdp2.h.bu.b     vr14,    vr10,  vr8
    vdp2.h.bu.b     vr15,    vr11,  vr8
    vdp2.h.bu.b     vr16,    vr12,  vr8
    vdp2.h.bu.b     vr17,    vr13,  vr8
    vhaddw.d.h      vr14
    vhaddw.d.h      vr15
    vhaddw.d.h      vr16
    vhaddw.d.h      vr17
    vpickev.w       vr13,    vr15,  vr14
    vpickev.w       vr14,    vr17,  vr16
    vpickev.h       \in0,    vr14,  vr13 //x0 ... x7
    vsrari.h        \in0,    \in0,  2
.endm
.macro FILTER_8TAP_8W_CLIP_STORE
    vdp2.w.h        vr12,    vr0,   vr9
    vdp2.w.h        vr13,    vr1,   vr9
    vdp2.w.h        vr14,    vr2,   vr9
    vdp2.w.h        vr15,    vr3,   vr9
    vdp2.w.h        vr16,    vr4,   vr9
    vdp2.w.h        vr17,    vr5,   vr9
    vdp2.w.h        vr18,    vr6,   vr9
    vdp2.w.h        vr19,    vr7,   vr9
    vhaddw.q.w      vr12
    vhaddw.q.w      vr13
    vhaddw.q.w      vr14
    vhaddw.q.w      vr15
    vhaddw.q.w      vr16
    vhaddw.q.w      vr17
    vhaddw.q.w      vr18
    vhaddw.q.w      vr19
    vpackev.w       vr12,    vr13,  vr12
    vpackev.w       vr13,    vr15,  vr14
    vpackev.d       vr12,    vr13,  vr12
    vpackev.w       vr14,    vr17,  vr16
    vpackev.w       vr15,    vr19,  vr18
    vpackev.d       vr13,    vr15,  vr14
    vssrarni.hu.w   vr13,    vr12,  10
    vssrani.bu.h    vr13,    vr13,  0
    vstelm.d        vr13,    a0,    0,   0
    add.d           a0,      a0,    a1
.endm
.macro VEXTRINS_Hx8 in0
    vextrins.h      vr0,     \in0,  0x70
    vextrins.h      vr1,     \in0,  0x71
    vextrins.h      vr2,     \in0,  0x72
    vextrins.h      vr3,     \in0,  0x73
    vextrins.h      vr4,     \in0,  0x74
    vextrins.h      vr5,     \in0,  0x75
    vextrins.h      vr6,     \in0,  0x76
    vextrins.h      vr7,     \in0,  0x77
.endm
.macro VBSRL_Vx8
    vbsrl.v         vr0,     vr0,   2
    vbsrl.v         vr1,     vr1,   2
    vbsrl.v         vr2,     vr2,   2
    vbsrl.v         vr3,     vr3,   2
    vbsrl.v         vr4,     vr4,   2
    vbsrl.v         vr5,     vr5,   2
    vbsrl.v         vr6,     vr6,   2
    vbsrl.v         vr7,     vr7,   2
.endm

.macro PUT_8TAP_8BPC_LSX lable
    li.w             t0,     4
    la.local         t6,     dav1d_mc_subpel_filters
    slli.d           t2,     a3,    1  //src_stride*2
    add.d            t3,     t2,    a3 //src_stride*3
    slli.d           t4,     t2,    1  //src_stride*4

    bnez             a6,     .l_\lable\()put_h //mx
    bnez             a7,     .l_\lable\()put_v //my

    clz.w            t1,     a4
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()put_hv0_jtable
    alsl.d           t1,     t1,    t5,   3
    ld.d             t6,     t1,    0
    add.d            t5,     t5,    t6
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()put_hv0_jtable:
    .dword .l_\lable\()put_hv0_128w - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_64w  - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_32w  - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_16w  - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_8w   - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_4w   - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_2w   - .l_\lable\()put_hv0_jtable

.l_\lable\()put_hv0_2w:
    vldrepl.h        vr0,    a2,    0
    add.d            a2,     a2,    a3
    vldrepl.h        vr1,    a2,    0
    vstelm.h         vr0,    a0,    0,     0
    add.d            a0,     a0,    a1
    vstelm.h         vr1,    a0,    0,     0
    add.d            a2,     a2,    a3
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_2w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_4w:
    fld.s            f0,     a2,    0
    fldx.s           f1,     a2,    a3
    fst.s            f0,     a0,    0
    fstx.s           f1,     a0,    a1
    alsl.d           a2,     a3,    a2,    1
    alsl.d           a0,     a1,    a0,    1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_4w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_8w:
    fld.d            f0,     a2,    0
    fldx.d           f1,     a2,    a3
    fst.d            f0,     a0,    0
    fstx.d           f1,     a0,    a1
    alsl.d           a2,     a3,    a2,    1
    alsl.d           a0,     a1,    a0,    1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_8w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_16w:
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    vst              vr0,    a0,    0
    vstx             vr1,    a0,    a1
    alsl.d           a2,     a3,    a2,    1
    alsl.d           a0,     a1,    a0,    1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_16w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_32w:
    vld              vr0,    a2,    0
    vld              vr1,    a2,    16
    add.d            a2,     a2,    a3
    vld              vr2,    a2,    0
    vld              vr3,    a2,    16
    vst              vr0,    a0,    0
    vst              vr1,    a0,    16
    add.d            a0,     a0,    a1
    vst              vr2,    a0,    0
    vst              vr3,    a0,    16
    add.d            a2,     a2,    a3
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_32w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_64w:
    vld              vr0,    a2,    0
    vld              vr1,    a2,    16
    vld              vr2,    a2,    32
    vld              vr3,    a2,    48
    add.d            a2,     a2,    a3
    vld              vr4,    a2,    0
    vld              vr5,    a2,    16
    vld              vr6,    a2,    32
    vld              vr7,    a2,    48
    add.d            a2,     a2,    a3
    vst              vr0,    a0,    0
    vst              vr1,    a0,    16
    vst              vr2,    a0,    32
    vst              vr3,    a0,    48
    add.d            a0,     a0,    a1
    vst              vr4,    a0,    0
    vst              vr5,    a0,    16
    vst              vr6,    a0,    32
    vst              vr7,    a0,    48
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_64w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_128w:
    vld              vr0,    a2,    0
    vld              vr1,    a2,    16
    vld              vr2,    a2,    32
    vld              vr3,    a2,    48
    vld              vr4,    a2,    64
    vld              vr5,    a2,    80
    vld              vr6,    a2,    96
    vld              vr7,    a2,    112
    add.d            a2,     a2,    a3
    vld              vr8,    a2,    0
    vld              vr9,    a2,    16
    vld              vr10,   a2,    32
    vld              vr11,   a2,    48
    vld              vr12,   a2,    64
    vld              vr13,   a2,    80
    vld              vr14,   a2,    96
    vld              vr15,   a2,    112
    add.d            a2,     a2,    a3
    vst              vr0,    a0,    0
    vst              vr1,    a0,    16
    vst              vr2,    a0,    32
    vst              vr3,    a0,    48
    vst              vr4,    a0,    64
    vst              vr5,    a0,    80
    vst              vr6,    a0,    96
    vst              vr7,    a0,    112
    add.d            a0,     a0,    a1
    vst              vr8,    a0,    0
    vst              vr9,    a0,    16
    vst              vr10,   a0,    32
    vst              vr11,   a0,    48
    vst              vr12,   a0,    64
    vst              vr13,   a0,    80
    vst              vr14,   a0,    96
    vst              vr15,   a0,    112
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_128w
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_h:
    bnez             a7,     .l_\lable\()put_hv //if(fh) && if (fv)
    ld.d             t5,     sp,    0  //filter_type
    andi             t1,     t5,    3
    blt              t0,     a4,    .l_\lable\()put_h_idx_fh
    andi             t1,     t5,    1
    addi.w           t1,     t1,    3

.l_\lable\()put_h_idx_fh:
    addi.w           t5,     zero,  120
    mul.w            t1,     t1,    t5
    addi.w           t5,     a6,    -1
    slli.w           t5,     t5,    3
    add.w            t1,     t1,    t5
    add.d            t1,     t6,    t1 //fh's offset
    vldrepl.d        vr8,    t1,    0
    addi.d           a2,     a2,    -3
    li.w             t1,     34
    vreplgr2vr.h     vr9,    t1

    clz.w            t1,     a4
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()put_h_jtable
    alsl.d           t1,     t1,    t5,   3
    ld.d             t6,     t1,    0
    add.d            t5,     t5,    t6
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()put_h_jtable:
    .dword .l_\lable\()put_h_128w - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_64w  - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_32w  - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_16w  - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_8w   - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_4w   - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_2w   - .l_\lable\()put_h_jtable

.l_\lable\()put_h_2w:
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    add.d            a2,     a2,    t2

    vbsrl.v          vr2,    vr0,   1
    vilvl.d          vr0,    vr2,   vr0
    vdp2.h.bu.b      vr2,    vr0,   vr8
    vhaddw.w.h       vr0,    vr2,   vr2
    vhaddw.d.w       vr0,    vr0,   vr0
    vbsrl.v          vr2,    vr1,   1
    vilvl.d          vr1,    vr2,   vr1
    vdp2.h.bu.b      vr2,    vr1,   vr8
    vhaddw.w.h       vr1,    vr2,   vr2
    vhaddw.d.w       vr1,    vr1,   vr1
    vpickev.w        vr0,    vr1,   vr0
    vpickev.h        vr0,    vr0,   vr0
    vadd.h           vr0,    vr0,   vr9
    vssrani.bu.h     vr0,    vr0,   6

    vstelm.h         vr0,    a0,    0,   0
    add.d            a0,     a0,    a1
    vstelm.h         vr0,    a0,    0,   1
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_h_2w
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_h_4w:
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    add.d            a2,     a2,    t2

    vbsrl.v          vr2,    vr0,   1
    vbsrl.v          vr3,    vr0,   2
    vbsrl.v          vr4,    vr0,   3
    vilvl.d          vr0,    vr2,   vr0 //x0 x1
    vilvl.d          vr2,    vr4,   vr3 //x2 x3
    vdp2.h.bu.b      vr3,    vr0,   vr8
    vdp2.h.bu.b      vr4,    vr2,   vr8
    vhaddw.w.h       vr0,    vr3,   vr3
    vhaddw.d.w       vr0,    vr0,   vr0
    vhaddw.w.h       vr2,    vr4,   vr4
    vhaddw.d.w       vr2,    vr2,   vr2
    vpickev.w        vr5,    vr2,   vr0
    vbsrl.v          vr2,    vr1,   1
    vbsrl.v          vr3,    vr1,   2
    vbsrl.v          vr4,    vr1,   3
    vilvl.d          vr0,    vr2,   vr1 //x0 x1
    vilvl.d          vr2,    vr4,   vr3 //x2 x3
    vdp2.h.bu.b      vr3,    vr0,   vr8
    vdp2.h.bu.b      vr4,    vr2,   vr8
    vhaddw.w.h       vr0,    vr3,   vr3
    vhaddw.d.w       vr0,    vr0,   vr0
    vhaddw.w.h       vr2,    vr4,   vr4
    vhaddw.d.w       vr2,    vr2,   vr2
    vpickev.w        vr6,    vr2,   vr0
    vpickev.h        vr0,    vr6,   vr5
    vadd.h           vr0,    vr0,   vr9
    vssrani.bu.h     vr0,    vr0,   6

    vstelm.w         vr0,    a0,    0,    0
    add.d            a0,     a0,    a1
    vstelm.w         vr0,    a0,    0,    1
    add.d            a0,     a0,    a1
    addi.d           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_h_4w
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_h_8w:
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    add.d            a2,     a2,    t2
    PUT_H_8W         vr0
    PUT_H_8W         vr1
    vssrani.bu.h     vr1,    vr0,   6
    vstelm.d         vr1,    a0,    0,    0
    add.d            a0,     a0,    a1
    vstelm.d         vr1,    a0,    0,    1
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_h_8w
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_h_16w:
.l_\lable\()put_h_32w:
.l_\lable\()put_h_64w:
.l_\lable\()put_h_128w:
    addi.d           t0,     a2,    0 //src
    addi.w           t5,     a5,    0 //h
    addi.d           t8,     a0,    0 //dst
.l_\lable\()put_h_16w_loop:
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    add.d            a2,     a2,    t2
    PUT_H_8W         vr0
    PUT_H_8W         vr1
    vssrani.bu.h     vr1,    vr0,   6
    vstelm.d         vr1,    a0,    0,   0
    add.d            a0,     a0,    a1
    vstelm.d         vr1,    a0,    0,   1
    add.d            a0,     a0,    a1
    addi.d           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_h_16w_loop
    addi.d           a2,     t0,    8
    addi.d           t0,     t0,    8
    addi.d           a0,     t8,    8
    addi.d           t8,     t8,    8
    addi.w           a5,     t5,    0
    addi.w           a4,     a4,    -8
    bnez             a4,     .l_\lable\()put_h_16w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_v:
    ld.d             t1,     sp,    0  //filter_type
    srli.w           t1,     t1,    2
    blt              t0,     a5,    .l_\lable\()put_v_idx_fv
    andi             t1,     t1,    1
    addi.w           t1,     t1,    3

.l_\lable\()put_v_idx_fv:
    addi.w           t5,     zero,  120
    mul.w            t1,     t1,    t5
    addi.w           t5,     a7,    -1
    slli.w           t5,     t5,    3
    add.w            t1,     t1,    t5
    add.d            t1,     t6,    t1 //fv's offset
    vldrepl.d        vr8,    t1,    0
    sub.d            a2,     a2,    t3

    clz.w            t1,     a4
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()put_v_jtable
    alsl.d           t1,     t1,    t5,   3
    ld.d             t6,     t1,    0
    add.d            t5,     t5,    t6
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()put_v_jtable:
    .dword .l_\lable\()put_v_128w - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_64w  - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_32w  - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_16w  - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_8w   - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_4w   - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_2w   - .l_\lable\()put_v_jtable

.l_\lable\()put_v_2w:
    fld.s            f0,     a2,    0
    fldx.s           f1,     a2,    a3
    fldx.s           f2,     a2,    t2
    add.d            a2,     a2,    t3
    fld.s            f3,     a2,    0
    fldx.s           f4,     a2,    a3
    fldx.s           f5,     a2,    t2
    fldx.s           f6,     a2,    t3
    add.d            a2,     a2,    t4
    vilvl.b          vr0,    vr1,   vr0
    vilvl.b          vr1,    vr3,   vr2
    vilvl.b          vr2,    vr5,   vr4
    vilvl.b          vr3,    vr7,   vr6
    vilvl.h          vr0,    vr1,   vr0
    vilvl.h          vr1,    vr3,   vr2
    vilvl.w          vr0,    vr1,   vr0

.l_\lable\()put_v_2w_loop:
    fld.s            f7,     a2,    0  //h0
    fldx.s           f10,    a2,    a3 //h1
    add.d            a2,     a2,    t2

    vextrins.b       vr0,    vr7,   0x70
    vextrins.b       vr0,    vr7,   0xf1
    vbsrl.v          vr1,    vr0,   1
    vextrins.b       vr1,    vr10,  0x70
    vextrins.b       vr1,    vr10,  0xf1
    vdp2.h.bu.b      vr10,   vr0,   vr8
    vdp2.h.bu.b      vr11,   vr1,   vr8
    vbsrl.v          vr0,    vr1,   1
    vhaddw.d.h       vr10
    vhaddw.d.h       vr11
    vpickev.w        vr10,   vr11,  vr10
    vssrarni.hu.w    vr10,   vr10,  6
    vssrani.bu.h     vr10,   vr10,  0

    vstelm.h         vr10,   a0,    0,   0
    add.d            a0,     a0,    a1
    vstelm.h         vr10,   a0,    0,   1
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_v_2w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_v_4w:
    fld.s            f0,     a2,    0
    fldx.s           f1,     a2,    a3
    fldx.s           f2,     a2,    t2
    add.d            a2,     a2,    t3
    fld.s            f3,     a2,    0
    fldx.s           f4,     a2,    a3
    fldx.s           f5,     a2,    t2
    fldx.s           f6,     a2,    t3
    add.d            a2,     a2,    t4

    vilvl.b          vr0,    vr1,   vr0
    vilvl.b          vr1,    vr3,   vr2
    vilvl.b          vr2,    vr5,   vr4
    vilvl.b          vr3,    vr7,   vr6
    vilvl.h          vr0,    vr1,   vr0
    vilvl.h          vr1,    vr3,   vr2
    vilvl.w          vr2,    vr1,   vr0
    vilvh.w          vr3,    vr1,   vr0

.l_\lable\()put_v_4w_loop:
    fld.s            f7,     a2,    0
    fldx.s           f10,    a2,    a3
    add.d            a2,     a2,    t2

    vextrins.b       vr2,    vr7,   0x70
    vextrins.b       vr2,    vr7,   0xf1 //x0x1(h0)
    vbsrl.v          vr4,    vr2,   1
    vextrins.b       vr4,    vr10,  0x70
    vextrins.b       vr4,    vr10,  0xf1 //x0x1(h1)
    vdp2.h.bu.b      vr11,   vr2,   vr8
    vdp2.h.bu.b      vr12,   vr4,   vr8
    vbsrl.v          vr2,    vr4,   1

    vextrins.b       vr3,    vr7,   0x72
    vextrins.b       vr3,    vr7,   0xf3 //x2x3(h0)
    vbsrl.v          vr4,    vr3,   1
    vextrins.b       vr4,    vr10,  0x72
    vextrins.b       vr4,    vr10,  0xf3 //x2x3(h1)
    vdp2.h.bu.b      vr13,   vr3,   vr8
    vdp2.h.bu.b      vr14,   vr4,   vr8
    vbsrl.v          vr3,    vr4,   1

    vhaddw.d.h       vr11
    vhaddw.d.h       vr12
    vhaddw.d.h       vr13
    vhaddw.d.h       vr14

    vpickev.w        vr11,   vr13,  vr11
    vpickev.w        vr12,   vr14,  vr12
    vpickev.h        vr11,   vr12,  vr11
    vssrarni.bu.h    vr11,   vr11,  6
    vstelm.w         vr11,   a0,    0,   0
    add.d            a0,     a0,    a1
    vstelm.w         vr11,   a0,    0,   1
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_v_4w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_v_8w:
.l_\lable\()put_v_16w:
.l_\lable\()put_v_32w:
.l_\lable\()put_v_64w:
.l_\lable\()put_v_128w:
    addi.d           t0,     a2,    0 //src
    addi.d           t5,     a5,    0 //h
    addi.d           t8,     a0,    0 //dst
.l_\lable\()put_v_8w_loop0:
    fld.d            f0,     a2,    0
    fldx.d           f1,     a2,    a3
    fldx.d           f2,     a2,    t2
    add.d            a2,     a2,    t3
    fld.d            f3,     a2,    0
    fldx.d           f4,     a2,    a3
    fldx.d           f5,     a2,    t2
    fldx.d           f6,     a2,    t3
    add.d            a2,     a2,    t4

    vilvl.b          vr0,    vr1,   vr0
    vilvl.b          vr1,    vr3,   vr2
    vilvl.b          vr2,    vr5,   vr4
    vilvl.b          vr3,    vr7,   vr6
    vilvl.h          vr4,    vr1,   vr0
    vilvh.h          vr5,    vr1,   vr0
    vilvl.h          vr6,    vr3,   vr2
    vilvh.h          vr7,    vr3,   vr2
    vilvl.w          vr0,    vr6,   vr4 // x0x1
    vilvh.w          vr1,    vr6,   vr4 // x2x3
    vilvl.w          vr2,    vr7,   vr5 // x4x5
    vilvh.w          vr3,    vr7,   vr5 // x6x7
.l_\lable\()put_v_8w_loop:
    fld.d            f7,     a2,    0
    fldx.d           f10,    a2,    a3
    add.d            a2,     a2,    t2
    //h0
    vextrins.b       vr0,    vr7,   0x70
    vextrins.b       vr0,    vr7,   0xf1
    vextrins.b       vr1,    vr7,   0x72
    vextrins.b       vr1,    vr7,   0xf3
    vextrins.b       vr2,    vr7,   0x74
    vextrins.b       vr2,    vr7,   0xf5
    vextrins.b       vr3,    vr7,   0x76
    vextrins.b       vr3,    vr7,   0xf7
    vdp2.h.bu.b      vr11,   vr0,   vr8
    vdp2.h.bu.b      vr12,   vr1,   vr8
    vdp2.h.bu.b      vr13,   vr2,   vr8
    vdp2.h.bu.b      vr14,   vr3,   vr8
    vhaddw.d.h       vr11
    vhaddw.d.h       vr12
    vhaddw.d.h       vr13
    vhaddw.d.h       vr14
    vpickev.w        vr11,   vr12,  vr11
    vpickev.w        vr12,   vr14,  vr13
    vpickev.h        vr11,   vr12,  vr11
    vssrarni.bu.h    vr11,   vr11,  6
    fst.d            f11,    a0,    0
    add.d            a0,     a0,    a1
    //h1
    vbsrl.v          vr0,    vr0,   1
    vbsrl.v          vr1,    vr1,   1
    vbsrl.v          vr2,    vr2,   1
    vbsrl.v          vr3,    vr3,   1
    vextrins.b       vr0,    vr10,  0x70
    vextrins.b       vr0,    vr10,  0xf1
    vextrins.b       vr1,    vr10,  0x72
    vextrins.b       vr1,    vr10,  0xf3
    vextrins.b       vr2,    vr10,  0x74
    vextrins.b       vr2,    vr10,  0xf5
    vextrins.b       vr3,    vr10,  0x76
    vextrins.b       vr3,    vr10,  0xf7
    vdp2.h.bu.b      vr11,   vr0,   vr8
    vdp2.h.bu.b      vr12,   vr1,   vr8
    vdp2.h.bu.b      vr13,   vr2,   vr8
    vdp2.h.bu.b      vr14,   vr3,   vr8
    vhaddw.d.h       vr11
    vhaddw.d.h       vr12
    vhaddw.d.h       vr13
    vhaddw.d.h       vr14
    vpickev.w        vr11,   vr12,  vr11
    vpickev.w        vr12,   vr14,  vr13
    vpickev.h        vr11,   vr12,  vr11
    vssrarni.bu.h    vr11,   vr11,  6
    fst.d            f11,    a0,    0
    add.d            a0,     a0,    a1
    vbsrl.v          vr0,    vr0,   1
    vbsrl.v          vr1,    vr1,   1
    vbsrl.v          vr2,    vr2,   1
    vbsrl.v          vr3,    vr3,   1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_v_8w_loop
    addi.d           a2,     t0,    8
    addi.d           t0,     t0,    8
    addi.d           a0,     t8,    8
    addi.d           t8,     t8,    8
    addi.d           a5,     t5,    0
    addi.w           a4,     a4,    -8
    bnez             a4,     .l_\lable\()put_v_8w_loop0
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_hv:
    ld.d             t5,     sp,    0  //filter_type
    andi             t1,     t5,    3
    blt              t0,     a4,    .l_\lable\()put_hv_idx_fh
    andi             t1,     t5,    1
    addi.w           t1,     t1,    3
.l_\lable\()put_hv_idx_fh:
    addi.w           t5,     zero,  120
    mul.w            t1,     t1,    t5
    addi.w           t5,     a6,    -1
    slli.w           t5,     t5,    3
    add.w            t1,     t1,    t5
    add.d            t1,     t6,    t1 //fh's offset
    vldrepl.d        vr8,    t1,    0
    ld.d             t1,     sp,    0  //filter_type
    srli.w           t1,     t1,    2
    blt              t0,     a5,    .l_\lable\()put_hv_idx_fv
    andi             t1,     t1,    1
    addi.w           t1,     t1,    3
.l_\lable\()put_hv_idx_fv:
    addi.w           t5,     zero,  120
    mul.w            t1,     t1,    t5
    addi.w           t5,     a7,    -1
    slli.w           t5,     t5,    3
    add.w            t1,     t1,    t5
    add.d            t1,     t6,    t1 //fv's offset
    vldrepl.d        vr9,    t1,    0
    vexth.h.b        vr9,    vr9

    sub.d            a2,     a2,    t3
    addi.d           a2,     a2,    -3

    clz.w            t1,     a4
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()put_hv_jtable
    alsl.d           t1,     t1,    t5,   3
    ld.d             t6,     t1,    0
    add.d            t5,     t5,    t6
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()put_hv_jtable:
    .dword .l_\lable\()put_hv_128w - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_64w  - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_32w  - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_16w  - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_8w   - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_4w   - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_2w   - .l_\lable\()put_hv_jtable

.l_\lable\()put_hv_2w:
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    vldx             vr2,    a2,    t2
    add.d            a2,     a2,    t3
    vld              vr3,    a2,    0
    vldx             vr4,    a2,    a3
    vldx             vr5,    a2,    t2
    vldx             vr6,    a2,    t3
    add.d            a2,     a2,    t4

    vbsrl.v          vr10,   vr0,   1
    vbsrl.v          vr11,   vr1,   1
    vbsrl.v          vr12,   vr2,   1
    vbsrl.v          vr13,   vr3,   1
    vbsrl.v          vr14,   vr4,   1
    vbsrl.v          vr15,   vr5,   1
    vbsrl.v          vr16,   vr6,   1
    vilvl.d          vr0,    vr10,  vr0
    vilvl.d          vr1,    vr11,  vr1
    vilvl.d          vr2,    vr12,  vr2
    vilvl.d          vr3,    vr13,  vr3
    vilvl.d          vr4,    vr14,  vr4
    vilvl.d          vr5,    vr15,  vr5
    vilvl.d          vr6,    vr16,  vr6
    vdp2.h.bu.b      vr10,   vr0,   vr8
    vdp2.h.bu.b      vr11,   vr1,   vr8
    vdp2.h.bu.b      vr12,   vr2,   vr8
    vdp2.h.bu.b      vr13,   vr3,   vr8
    vdp2.h.bu.b      vr14,   vr4,   vr8
    vdp2.h.bu.b      vr15,   vr5,   vr8
    vdp2.h.bu.b      vr16,   vr6,   vr8
    vhaddw.d.h       vr10
    vhaddw.d.h       vr11
    vhaddw.d.h       vr12
    vhaddw.d.h       vr13
    vhaddw.d.h       vr14
    vhaddw.d.h       vr15
    vhaddw.d.h       vr16

    vpackev.w        vr10,   vr11,  vr10
    vpackev.w        vr12,   vr13,  vr12
    vpackod.d        vr11,   vr12,  vr10
    vpackev.d        vr10,   vr12,  vr10

    vpackev.w        vr12,   vr15,  vr14
    vpackev.w        vr16,   vr17,  vr16
    vpackod.d        vr13,   vr16,  vr12
    vpackev.d        vr12,   vr16,  vr12

    vpickev.h        vr10,   vr12,  vr10 //0 1 2  3  4  5  6  * (h0)
    vpickev.h        vr11,   vr13,  vr11 //8 9 10 11 12 13 14 * (h1)
    vsrari.h         vr10,   vr10,  2
    vsrari.h         vr11,   vr11,  2
.l_\lable\()put_hv_2w_loop:
    vld              vr7,    a2,    0
    vldx             vr12,   a2,    a3
    add.d            a2,     a2,    t2

    vbsrl.v          vr1,    vr7,   1
    vbsrl.v          vr2,    vr12,  1
    vilvl.d          vr0,    vr1,   vr7
    vilvl.d          vr1,    vr2,   vr12
    vdp2.h.bu.b      vr2,    vr0,   vr8
    vdp2.h.bu.b      vr3,    vr1,   vr8
    vhaddw.d.h       vr2
    vhaddw.d.h       vr3
    vpickev.w        vr2,    vr3,   vr2
    vpickev.h        vr2,    vr2,   vr2
    vsrari.h         vr2,    vr2,   2
    vextrins.h       vr10,   vr2,   0x70 //0 1 2 3 4 5 6 7
    vextrins.h       vr11,   vr2,   0x71
    vbsrl.v          vr12,   vr10,  2
    vbsrl.v          vr13,   vr11,  2
    vextrins.h       vr12,   vr2,   0x72 //1 2 3 4 5 6 7 8
    vextrins.h       vr13,   vr2,   0x73
    vdp2.w.h         vr0,    vr10,  vr9
    vdp2.w.h         vr1,    vr11,  vr9
    vdp2.w.h         vr2,    vr12,  vr9
    vdp2.w.h         vr3,    vr13,  vr9
    vhaddw.q.w       vr0
    vhaddw.q.w       vr1
    vhaddw.q.w       vr2
    vhaddw.q.w       vr3
    vpackev.w        vr0,    vr1,   vr0
    vpackev.w        vr1,    vr3,   vr2
    vpackev.d        vr0,    vr1,   vr0
    vssrarni.hu.w    vr0,    vr0,   10
    vssrani.bu.h     vr0,    vr0,   0
    vbsrl.v          vr10,   vr12,  2
    vbsrl.v          vr11,   vr13,  2
    vstelm.h         vr0,    a0,    0,   0
    add.d            a0,     a0,    a1
    vstelm.h         vr0,    a0,    0,   1
    add.d            a0,     a0,    a1
    addi.d           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv_2w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_hv_4w:
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    vldx             vr2,    a2,    t2
    add.d            a2,     a2,    t3
    vld              vr3,    a2,    0
    vldx             vr4,    a2,    a3
    vldx             vr5,    a2,    t2
    vldx             vr6,    a2,    t3
    add.d            a2,     a2,    t4
    FILTER_8TAP_4W   vr0 //x0 x1 x2 x3
    FILTER_8TAP_4W   vr1
    FILTER_8TAP_4W   vr2
    FILTER_8TAP_4W   vr3
    FILTER_8TAP_4W   vr4
    FILTER_8TAP_4W   vr5
    FILTER_8TAP_4W   vr6
    vpackev.h        vr0,    vr1,   vr0
    vpackev.h        vr1,    vr3,   vr2
    vpackev.h        vr2,    vr5,   vr4
    vpackev.h        vr3,    vr7,   vr6
    vilvl.w          vr4,    vr1,   vr0
    vilvh.w          vr5,    vr1,   vr0
    vilvl.w          vr6,    vr3,   vr2
    vilvh.w          vr7,    vr3,   vr2
    vilvl.d          vr0,    vr6,   vr4 //0 1 2 3 4 5 6 *
    vilvh.d          vr1,    vr6,   vr4
    vilvl.d          vr2,    vr7,   vr5
    vilvh.d          vr3,    vr7,   vr5
    vsrari.h         vr0,    vr0,   2
    vsrari.h         vr1,    vr1,   2
    vsrari.h         vr2,    vr2,   2
    vsrari.h         vr3,    vr3,   2
.l_\lable\()put_hv_4w_loop:
    vld              vr4,    a2,    0
    vldx             vr5,    a2,    a3
    add.d            a2,     a2,    t2
    FILTER_8TAP_4W   vr4
    FILTER_8TAP_4W   vr5
    vpickev.h        vr4,    vr5,   vr4
    vsrari.h         vr4,    vr4,   2
    vextrins.h       vr0,    vr4,   0x70
    vextrins.h       vr1,    vr4,   0x71
    vextrins.h       vr2,    vr4,   0x72
    vextrins.h       vr3,    vr4,   0x73
    vbsrl.v          vr5,    vr0,   2
    vbsrl.v          vr6,    vr1,   2
    vbsrl.v          vr7,    vr2,   2
    vbsrl.v          vr10,   vr3,   2
    vextrins.h       vr5,    vr4,   0x74
    vextrins.h       vr6,    vr4,   0x75
    vextrins.h       vr7,    vr4,   0x76
    vextrins.h       vr10,   vr4,   0x77
    vdp2.w.h         vr11,   vr0,   vr9
    vdp2.w.h         vr12,   vr1,   vr9
    vdp2.w.h         vr13,   vr2,   vr9
    vdp2.w.h         vr14,   vr3,   vr9
    vhaddw.q.w       vr11
    vhaddw.q.w       vr12
    vhaddw.q.w       vr13
    vhaddw.q.w       vr14
    vpackev.w        vr0,    vr12,  vr11
    vpackev.w        vr1,    vr14,  vr13
    vpackev.d        vr0,    vr1,   vr0
    vdp2.w.h         vr11,   vr5,   vr9
    vdp2.w.h         vr12,   vr6,   vr9
    vdp2.w.h         vr13,   vr7,   vr9
    vdp2.w.h         vr14,   vr10,  vr9
    vhaddw.q.w       vr11
    vhaddw.q.w       vr12
    vhaddw.q.w       vr13
    vhaddw.q.w       vr14
    vpackev.w        vr1,    vr12,  vr11
    vpackev.w        vr2,    vr14,  vr13
    vpackev.d        vr1,    vr2,   vr1
    vssrarni.hu.w    vr1,    vr0,   10
    vssrani.bu.h     vr1,    vr1,   0
    vstelm.w         vr1,    a0,    0,    0
    add.d            a0,     a0,    a1
    vstelm.w         vr1,    a0,    0,    1
    add.d            a0,     a0,    a1
    vbsrl.v          vr0,    vr5,   2
    vbsrl.v          vr1,    vr6,   2
    vbsrl.v          vr2,    vr7,   2
    vbsrl.v          vr3,    vr10,  2
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv_4w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_hv_8w:
.l_\lable\()put_hv_16w:
.l_\lable\()put_hv_32w:
.l_\lable\()put_hv_64w:
.l_\lable\()put_hv_128w:
    addi.d          t0,      a2,    0 //src
    addi.d          t5,      a5,    0 //h
    addi.d          t8,      a0,    0 //dst
.l_\lable\()put_hv_8w_loop0:
    vld             vr0,     a2,    0
    vldx            vr1,     a2,    a3
    vldx            vr2,     a2,    t2
    add.d           a2,      a2,    t3
    vld             vr3,     a2,    0
    vldx            vr4,     a2,    a3
    vldx            vr5,     a2,    t2
    vldx            vr6,     a2,    t3
    add.d           a2,      a2,    t4
    FILTER_8TAP_8W  vr0
    FILTER_8TAP_8W  vr1
    FILTER_8TAP_8W  vr2
    FILTER_8TAP_8W  vr3
    FILTER_8TAP_8W  vr4
    FILTER_8TAP_8W  vr5
    FILTER_8TAP_8W  vr6
    LSX_TRANSPOSE8x8_H vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7,\
                       vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7,\
                       vr10,vr11,vr12,vr13,vr14,vr15,vr16,vr17
.l_\lable\()put_hv_8w_loop:
    vld             vr20,    a2,    0
    vldx            vr21,    a2,    a3
    add.d           a2,      a2,    t2
    FILTER_8TAP_8W  vr20
    FILTER_8TAP_8W  vr21
    VEXTRINS_Hx8    vr20
    FILTER_8TAP_8W_CLIP_STORE
    VBSRL_Vx8
    VEXTRINS_Hx8    vr21
    FILTER_8TAP_8W_CLIP_STORE
    VBSRL_Vx8
    addi.w          a5,      a5,    -2
    bnez            a5,      .l_\lable\()put_hv_8w_loop
    addi.d          a2,      t0,    8
    addi.d          t0,      t0,    8
    addi.d          a0,      t8,    8
    addi.d          t8,      t8,    8
    addi.d          a5,      t5,    0
    addi.w          a4,      a4,    -8
    bnez            a4,      .l_\lable\()put_hv_8w_loop0
.l_\lable\()end_put_8tap:
.endm

function put_8tap_regular_8bpc_lsx
    addi.d   sp, sp,  -16
    st.d   zero, sp,  0
    PUT_8TAP_8BPC_LSX 0
    addi.d   sp, sp,  16
endfunc

function put_8tap_smooth_regular_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 1
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 1
    addi.d   sp, sp,  16
endfunc

function put_8tap_sharp_regular_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 2
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 2
    addi.d   sp, sp,  16
endfunc

function put_8tap_regular_smooth_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 4
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 4
    addi.d   sp, sp,  16
endfunc

function put_8tap_smooth_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 5
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 5
    addi.d   sp, sp,  16
endfunc

function put_8tap_sharp_smooth_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 6
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 6
    addi.d   sp, sp,  16
endfunc

function put_8tap_regular_sharp_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 8
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 8
    addi.d   sp, sp,  16
endfunc

function put_8tap_smooth_sharp_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 9
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 9
    addi.d   sp, sp,  16
endfunc

function put_8tap_sharp_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 10
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 10
    addi.d   sp, sp,  16
endfunc

const shufb1
.byte 0,1,2,3,4,5,6,7,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,1,2,3,4,5,6,7,8
endconst

.macro SHUFB in0, in1, tmp, out
    xvbsrl.v  \tmp, \in0, 2
    xvpermi.q \tmp, \in0, 0x20
    xvshuf.b  \out, \tmp, \tmp, \in1
.endm

.macro HADDWDH in0
    xvhaddw.w.h \in0, \in0, \in0
    xvhaddw.d.w \in0, \in0, \in0
.endm

.macro HADDWQW in0
    xvhaddw.d.w \in0, \in0, \in0
    xvhaddw.q.d \in0, \in0, \in0
.endm

.macro PREP_W16_H in0
    xvbsrl.v         xr4,    \in0,    4
    xvbsrl.v         xr5,    \in0,    8
    xvpermi.q        xr9,    \in0,    0x31
    xvpackev.d       xr5,     xr9,    xr5
    xvbsrl.v         xr6,     xr5,    4
    SHUFB           \in0,     xr23,   xr9,   \in0
    SHUFB            xr4,     xr23,   xr9,    xr4
    SHUFB            xr5,     xr23,   xr9,    xr5
    SHUFB            xr6,     xr23,   xr9,    xr6
    xvdp2.h.bu.b     xr10,   \in0,    xr22
    xvdp2.h.bu.b     xr11,    xr4,    xr22
    xvdp2.h.bu.b     xr12,    xr5,    xr22
    xvdp2.h.bu.b     xr13,    xr6,    xr22
    HADDWDH          xr10
    HADDWDH          xr11
    HADDWDH          xr12
    HADDWDH          xr13
    xvpickev.w       xr10,    xr11,   xr10
    xvpickev.w       xr11,    xr13,   xr12
    xvpermi.d        xr10,    xr10,   0xd8
    xvpermi.d        xr11,    xr11,   0xd8
    xvpickev.h       xr10,    xr11,   xr10
    xvpermi.d        xr10,    xr10,   0xd8
    xvsrari.h       \in0,     xr10,   2
.endm

.macro PREP_8TAP_8BPC_LASX lable
    li.w             t0,     4
    la.local         t6,     dav1d_mc_subpel_filters
    la.local         t7,     shufb1
    xvld             xr23,   t7,    0
    slli.d           t2,     a2,    1  //src_stride*2
    add.d            t3,     t2,    a2 //src_stride*3
    slli.d           t4,     t2,    1

    bnez             a5,     .l_\lable\()h //mx
    bnez             a6,     .l_\lable\()v

    clz.w            t1,     a3
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()prep_hv0_jtable
    alsl.d           t1,     t1,    t5,   1
    ld.h             t8,     t1,    0
    add.d            t5,     t5,    t8
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()prep_hv0_jtable:
    .hword .l_\lable\()hv0_128w - .l_\lable\()prep_hv0_jtable
    .hword .l_\lable\()hv0_64w  - .l_\lable\()prep_hv0_jtable
    .hword .l_\lable\()hv0_32w  - .l_\lable\()prep_hv0_jtable
    .hword .l_\lable\()hv0_16w  - .l_\lable\()prep_hv0_jtable
    .hword .l_\lable\()hv0_8w   - .l_\lable\()prep_hv0_jtable
    .hword .l_\lable\()hv0_4w   - .l_\lable\()prep_hv0_jtable

.l_\lable\()hv0_4w:
    fld.s            f0,     a1,    0
    fldx.s           f1,     a1,    a2
    fldx.s           f2,     a1,    t2
    fldx.s           f3,     a1,    t3
    add.d            a1,     a1,    t4
    xvpackev.w       xr0,    xr1,   xr0
    xvpackev.w       xr1,    xr3,   xr2
    xvpermi.q        xr0,    xr1,   0x02
    xvsllwil.hu.bu   xr0,    xr0,   4
    xvst             xr0,    a0,    0
    addi.d           a0,     a0,    32
    addi.d           a4,     a4,    -4
    bnez             a4,     .l_\lable\()hv0_4w
    b                .l_\lable\()end_pre_8tap
.l_\lable\()hv0_8w:
    fld.d            f0,     a1,    0
    fldx.d           f1,     a1,    a2
    fldx.d           f2,     a1,    t2
    fldx.d           f3,     a1,    t3
    add.d            a1,     a1,    t4
    xvpermi.q        xr0,    xr1,   0x02
    xvpermi.q        xr2,    xr3,   0x02
    xvsllwil.hu.bu   xr0,    xr0,   4
    xvsllwil.hu.bu   xr2,    xr2,   4
    xvst             xr0,    a0,    0
    xvst             xr2,    a0,    32
    addi.d           a0,     a0,    64
    addi.d           a4,     a4,    -4
    bnez             a4,     .l_\lable\()hv0_8w
    b                .l_\lable\()end_pre_8tap
.l_\lable\()hv0_16w:
    vld              vr0,    a1,    0
    vldx             vr1,    a1,    a2
    vldx             vr2,    a1,    t2
    vldx             vr3,    a1,    t3
    add.d            a1,     a1,    t4
    vext2xv.hu.bu    xr0,    xr0
    vext2xv.hu.bu    xr1,    xr1
    vext2xv.hu.bu    xr2,    xr2
    vext2xv.hu.bu    xr3,    xr3
    xvslli.h         xr0,    xr0,   4
    xvslli.h         xr1,    xr1,   4
    xvslli.h         xr2,    xr2,   4
    xvslli.h         xr3,    xr3,   4
    xvst             xr0,    a0,    0
    xvst             xr1,    a0,    32
    xvst             xr2,    a0,    64
    xvst             xr3,    a0,    96
    addi.d           a0,     a0,    128
    addi.d           a4,     a4,    -4
    bnez             a4,     .l_\lable\()hv0_16w
    b                .l_\lable\()end_pre_8tap
.l_\lable\()hv0_32w:
    xvld             xr0,    a1,    0
    xvldx            xr1,    a1,    a2
    xvldx            xr2,    a1,    t2
    xvldx            xr3,    a1,    t3
    add.d            a1,     a1,    t4
    xvpermi.d        xr4,    xr0,   0xD8
    xvpermi.d        xr5,    xr1,   0xD8
    xvpermi.d        xr6,    xr2,   0xD8
    xvpermi.d        xr7,    xr3,   0xD8
    xvpermi.d        xr10,   xr0,   0x32
    xvpermi.d        xr11,   xr1,   0x32
    xvpermi.d        xr12,   xr2,   0x32
    xvpermi.d        xr13,   xr3,   0x32
    xvsllwil.hu.bu   xr0,    xr4,   4
    xvsllwil.hu.bu   xr1,    xr5,   4
    xvsllwil.hu.bu   xr2,    xr6,   4
    xvsllwil.hu.bu   xr3,    xr7,   4
    xvsllwil.hu.bu   xr4,    xr10,  4
    xvsllwil.hu.bu   xr5,    xr11,  4
    xvsllwil.hu.bu   xr6,    xr12,  4
    xvsllwil.hu.bu   xr7,    xr13,  4
    xvst             xr0,    a0,    0
    xvst             xr4,    a0,    32
    xvst             xr1,    a0,    64
    xvst             xr5,    a0,    96
    xvst             xr2,    a0,    128
    xvst             xr6,    a0,    160
    xvst             xr3,    a0,    192
    xvst             xr7,    a0,    224
    addi.d           a0,     a0,    256
    addi.d           a4,     a4,    -4
    bnez             a4,     .l_\lable\()hv0_32w
    b                .l_\lable\()end_pre_8tap
.l_\lable\()hv0_64w:
.l_\lable\()hv0_128w:
    addi.d           t0,     a1,    0
    addi.d           t5,     a4,    0
    srli.w           t7,     a3,    5
    slli.w           t7,     t7,    6
    addi.d           t8,     a0,    0
.l_\lable\()hv0_32_loop:
    xvld             xr0,    a1,    0
    xvldx            xr1,    a1,    a2
    xvldx            xr2,    a1,    t2
    xvldx            xr3,    a1,    t3
    add.d            a1,     a1,    t4
    xvpermi.d        xr4,    xr0,   0xD8
    xvpermi.d        xr5,    xr1,   0xD8
    xvpermi.d        xr6,    xr2,   0xD8
    xvpermi.d        xr7,    xr3,   0xD8
    xvpermi.d        xr10,   xr0,   0x32
    xvpermi.d        xr11,   xr1,   0x32
    xvpermi.d        xr12,   xr2,   0x32
    xvpermi.d        xr13,   xr3,   0x32
    xvsllwil.hu.bu   xr0,    xr4,   4
    xvsllwil.hu.bu   xr1,    xr5,   4
    xvsllwil.hu.bu   xr2,    xr6,   4
    xvsllwil.hu.bu   xr3,    xr7,   4
    xvsllwil.hu.bu   xr4,    xr10,  4
    xvsllwil.hu.bu   xr5,    xr11,  4
    xvsllwil.hu.bu   xr6,    xr12,  4
    xvsllwil.hu.bu   xr7,    xr13,  4
    xvst             xr0,    a0,    0
    xvst             xr4,    a0,    32
    add.d            t1,     a0,    t7
    xvst             xr1,    t1,    0
    xvst             xr5,    t1,    32
    add.d            t1,     t1,    t7
    xvst             xr2,    t1,    0
    xvst             xr6,    t1,    32
    add.d            t1,     t1,    t7
    xvst             xr3,    t1,    0
    xvst             xr7,    t1,    32
    add.d            a0,     t1,    t7
    addi.d           a4,     a4,    -4
    bnez             a4,     .l_\lable\()hv0_32_loop
    addi.d           a1,     t0,    32
    addi.d           t0,     t0,    32
    addi.d           a0,     t8,    64
    addi.d           t8,     t8,    64
    addi.d           a4,     t5,    0
    addi.d           a3,     a3,    -32
    bnez             a3,     .l_\lable\()hv0_32_loop
    b                .l_\lable\()end_pre_8tap

.l_\lable\()h:
    bnez             a6,     .l_\lable\()hv //if(fh) && if (fv)

    andi             t1,    a7,    3
    blt              t0,    a3,    .l_\lable\()h_idx_fh
    andi             t1,    a7,    1
    addi.w           t1,    t1,    3
.l_\lable\()h_idx_fh:
    addi.w           t5,    zero,  120
    mul.w            t1,    t1,    t5
    addi.w           t5,    a5,    -1
    slli.w           t5,    t5,    3
    add.w            t1,    t1,    t5
    add.d            t1,    t6,    t1 //fh's offset
    xvldrepl.d       xr22,  t1,    0

    addi.d           a1,     a1,    -3
    clz.w            t1,     a3
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()prep_h_jtable
    alsl.d           t1,     t1,    t5,   1
    ld.h             t8,     t1,    0
    add.d            t5,     t5,    t8
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()prep_h_jtable:
    .hword .l_\lable\()h_128w - .l_\lable\()prep_h_jtable
    .hword .l_\lable\()h_64w  - .l_\lable\()prep_h_jtable
    .hword .l_\lable\()h_32w  - .l_\lable\()prep_h_jtable
    .hword .l_\lable\()h_16w  - .l_\lable\()prep_h_jtable
    .hword .l_\lable\()h_8w   - .l_\lable\()prep_h_jtable
    .hword .l_\lable\()h_4w   - .l_\lable\()prep_h_jtable

.l_\lable\()h_4w:
    xvld             xr0,    a1,    0
    xvldx            xr1,    a1,    a2
    xvldx            xr2,    a1,    t2
    xvldx            xr3,    a1,    t3
    add.d            a1,     a1,    t4

    SHUFB            xr0,    xr23,  xr9,   xr0
    SHUFB            xr1,    xr23,  xr9,   xr1
    SHUFB            xr2,    xr23,  xr9,   xr2
    SHUFB            xr3,    xr23,  xr9,   xr3

    xvdp2.h.bu.b     xr10,   xr0,   xr22
    xvdp2.h.bu.b     xr12,   xr1,   xr22
    xvdp2.h.bu.b     xr14,   xr2,   xr22
    xvdp2.h.bu.b     xr16,   xr3,   xr22

    HADDWDH          xr10    //h0 mid0 mid1 mid2 mid3
    HADDWDH          xr12    //h1 mid4 mid5 mid6 mid7
    HADDWDH          xr14    //h2
    HADDWDH          xr16    //h3

    xvpickev.w       xr10,   xr12,    xr10
    xvpickev.w       xr14,   xr16,    xr14
    xvpermi.d        xr10,   xr10,    0xd8
    xvpermi.d        xr14,   xr14,    0xd8
    xvpickev.h       xr10,   xr14,    xr10
    xvpermi.d        xr10,   xr10,    0xd8
    xvsrari.h        xr10,   xr10,    2

    xvst             xr10,   a0,      0
    addi.d           a0,     a0,      32
    addi.w           a4,     a4,      -4
    bnez             a4,     .l_\lable\()h_4w
    b                .l_\lable\()end_pre_8tap

.l_\lable\()h_8w:
    xvld             xr0,    a1,      0
    xvldx            xr2,    a1,      a2
    xvldx            xr4,    a1,      t2
    xvldx            xr6,    a1,      t3
    add.d            a1,     a1,      t4

    xvbsrl.v         xr1,    xr0,     4
    xvbsrl.v         xr3,    xr2,     4
    xvbsrl.v         xr5,    xr4,     4
    xvbsrl.v         xr7,    xr6,     4

    SHUFB            xr0,    xr23,    xr9,    xr10
    SHUFB            xr1,    xr23,    xr9,    xr11
    SHUFB            xr2,    xr23,    xr9,    xr12
    SHUFB            xr3,    xr23,    xr9,    xr13
    SHUFB            xr4,    xr23,    xr9,    xr14
    SHUFB            xr5,    xr23,    xr9,    xr15
    SHUFB            xr6,    xr23,    xr9,    xr16
    SHUFB            xr7,    xr23,    xr9,    xr17

    xvdp2.h.bu.b     xr0,    xr10,    xr22
    xvdp2.h.bu.b     xr1,    xr11,    xr22
    xvdp2.h.bu.b     xr2,    xr12,    xr22
    xvdp2.h.bu.b     xr3,    xr13,    xr22
    xvdp2.h.bu.b     xr4,    xr14,    xr22
    xvdp2.h.bu.b     xr5,    xr15,    xr22
    xvdp2.h.bu.b     xr6,    xr16,    xr22
    xvdp2.h.bu.b     xr7,    xr17,    xr22

    HADDWDH          xr0
    HADDWDH          xr1
    HADDWDH          xr2
    HADDWDH          xr3
    HADDWDH          xr4
    HADDWDH          xr5
    HADDWDH          xr6
    HADDWDH          xr7

    xvpickev.w       xr0,    xr1,    xr0
    xvpickev.w       xr2,    xr3,    xr2
    xvpermi.d        xr0,    xr0,    0xd8
    xvpermi.d        xr2,    xr2,    0xd8
    xvpickev.h       xr0,    xr2,    xr0
    xvpermi.d        xr0,    xr0,    0xd8
    xvsrari.h        xr0,    xr0,    2

    xvpickev.w       xr4,    xr5,    xr4
    xvpickev.w       xr6,    xr7,    xr6
    xvpermi.d        xr4,    xr4,    0xd8
    xvpermi.d        xr6,    xr6,    0xd8
    xvpickev.h       xr4,    xr6,    xr4
    xvpermi.d        xr4,    xr4,    0xd8
    xvsrari.h        xr4,    xr4,    2

    xvst             xr0,    a0,     0
    xvst             xr4,    a0,     32
    addi.d           a0,     a0,     64
    addi.d           a4,     a4,     -4
    bnez             a4,     .l_\lable\()h_8w
    b                .l_\lable\()end_pre_8tap

.l_\lable\()h_16w:
    xvld             xr0,    a1,     0
    xvldx            xr1,    a1,     a2
    xvldx            xr2,    a1,     t2
    xvldx            xr3,    a1,     t3
    add.d            a1,     a1,     t4

    PREP_W16_H       xr0
    PREP_W16_H       xr1
    PREP_W16_H       xr2
    PREP_W16_H       xr3

    xvst             xr0,    a0,     0
    xvst             xr1,    a0,     32
    xvst             xr2,    a0,     64
    xvst             xr3,    a0,     96

    addi.d           a0,     a0,     128
    addi.w           a4,     a4,     -4
    bnez             a4,     .l_\lable\()h_16w
    b                .l_\lable\()end_pre_8tap

.l_\lable\()h_32w:
.l_\lable\()h_64w:
.l_\lable\()h_128w:
    addi.d           t0,     a1,     0 //src
    addi.d           t5,     a4,     0 //h
    srli.w           t7,     a3,     4 //w
    slli.w           t7,     t7,     5 //store offset
    addi.d           t8,     a0,     0 //dst
.l_\lable\()h_16_loop:
    xvld             xr0,    a1,     0
    xvldx            xr1,    a1,     a2
    xvldx            xr2,    a1,     t2
    xvldx            xr3,    a1,     t3
    add.d            a1,     a1,     t4

    PREP_W16_H       xr0
    PREP_W16_H       xr1
    PREP_W16_H       xr2
    PREP_W16_H       xr3

    xvst             xr0,    a0,     0
    xvstx            xr1,    a0,     t7
    slli.w           t1,     t7,     1
    xvstx            xr2,    a0,     t1
    add.w            t1,     t1,     t7
    xvstx            xr3,    a0,     t1
    slli.w           t1,     t7,     2
    add.d            a0,     a0,     t1
    addi.d           a4,     a4,     -4
    bnez             a4,     .l_\lable\()h_16_loop

    addi.d           a1,     t0,     16
    addi.d           t0,     t0,     16
    addi.d           a0,     t8,     32
    addi.d           t8,     t8,     32
    addi.d           a4,     t5,     0
    addi.d           a3,     a3,     -16
    bnez             a3,     .l_\lable\()h_16_loop
    b                .l_\lable\()end_pre_8tap
.l_\lable\()hv:
    andi             t1,    a7,    3
    blt              t0,    a3,    .l_\lable\()hv_idx_fh
    andi             t1,    a7,    1
    addi.w           t1,    t1,    3
.l_\lable\()hv_idx_fh:
    addi.w           t5,    zero,  120
    mul.w            t1,    t1,    t5
    addi.w           t5,    a5,    -1
    slli.w           t5,    t5,    3
    add.w            t1,    t1,    t5
    add.d            t1,    t6,    t1 //fh's offset
    xvldrepl.d       xr22,  t1,    0
    srli.w           a7,    a7,    2
    blt              t0,    a4,    .l_\lable\()hv_idx_fv
    andi             a7,    a7,    1
    addi.w           a7,    a7,    3
.l_\lable\()hv_idx_fv:
    addi.w           t5,     zero,  120
    mul.w            a7,     a7,    t5
    addi.w           t5,     a6,    -1
    slli.w           t5,     t5,    3
    add.w            a7,     a7,    t5
    add.d            a7,     t6,    a7 //fv's offset
    xvldrepl.d       xr8,    a7,    0
    xvsllwil.h.b     xr8,    xr8,   0

    sub.d            a1,     a1,     t3
    addi.d           a1,     a1,     -3
    beq              a3,     t0,     .l_\lable\()hv_4w
    b                .l_\lable\()hv_8w
.l_\lable\()hv_4w:
    xvld             xr0,    a1,     0
    xvldx            xr1,    a1,     a2
    xvldx            xr2,    a1,     t2
    xvldx            xr3,    a1,     t3
    add.d            a1,     a1,     t4
    xvld             xr4,    a1,     0
    xvldx            xr5,    a1,     a2
    xvldx            xr6,    a1,     t2

    SHUFB            xr0,    xr23,   xr9,   xr0
    SHUFB            xr1,    xr23,   xr9,   xr1
    SHUFB            xr2,    xr23,   xr9,   xr2
    SHUFB            xr3,    xr23,   xr9,   xr3

    SHUFB            xr4,    xr23,   xr9,   xr4
    SHUFB            xr5,    xr23,   xr9,   xr5
    SHUFB            xr6,    xr23,   xr9,   xr6

    xvdp2.h.bu.b     xr10,   xr0,    xr22
    xvdp2.h.bu.b     xr11,   xr1,    xr22
    xvdp2.h.bu.b     xr12,   xr2,    xr22
    xvdp2.h.bu.b     xr13,   xr3,    xr22

    xvdp2.h.bu.b     xr14,   xr4,    xr22
    xvdp2.h.bu.b     xr15,   xr5,    xr22
    xvdp2.h.bu.b     xr16,   xr6,    xr22

    HADDWDH          xr10    //h0 mid0 mid1 mid2 mid3
    HADDWDH          xr11    //h1 mid4 mid5 mid6 mid7
    HADDWDH          xr12    //h2
    HADDWDH          xr13    //h3

    xvpackev.w       xr10,   xr11,   xr10
    xvpackev.w       xr12,   xr13,   xr12
    xvpackev.d       xr11,   xr12,   xr10
    xvpackod.d       xr10,   xr12,   xr10
    xvpickev.h       xr11,   xr10,   xr11
    xvsrari.h        xr11,   xr11,   2

    HADDWDH          xr14    //h4
    HADDWDH          xr15    //h5
    HADDWDH          xr16    //h6

    xvpackev.w       xr14,   xr15,   xr14
    xvpackev.w       xr16,   xr17,   xr16
    xvpackev.d       xr17,   xr16,   xr14
    xvpackod.d       xr14,   xr16,   xr14
    xvpickev.h       xr13,   xr14,   xr17
    xvsrari.h        xr13,   xr13,   2

    xvpackev.d       xr18,   xr13,   xr11 //0 4 8 12 16 20 24 *  2 6 10 14 18 22 26 *
    xvpackod.d       xr19,   xr13,   xr11 //1 5 9 13 17 21 25 *  3 7 11 15 19 23 27 *
.l_\lable\()hv_w4_loop:
    xvldx            xr0,    a1,     t3
    add.d            a1,     a1,     t4
    xvld             xr1,    a1,     0
    xvldx            xr2,    a1,     a2
    xvldx            xr3,    a1,     t2

    SHUFB            xr0,    xr23,   xr9,   xr0
    SHUFB            xr1,    xr23,   xr9,   xr1
    SHUFB            xr2,    xr23,   xr9,   xr2
    SHUFB            xr3,    xr23,   xr9,   xr3

    xvdp2.h.bu.b     xr10,   xr0,    xr22
    xvdp2.h.bu.b     xr12,   xr1,    xr22
    xvdp2.h.bu.b     xr14,   xr2,    xr22
    xvdp2.h.bu.b     xr16,   xr3,    xr22

    HADDWDH          xr10    //h0 mid0 mid1 mid2 mid3
    HADDWDH          xr12    //h1 mid4 mid5 mid6 mid7
    HADDWDH          xr14    //h2
    HADDWDH          xr16    //h3

    xvpackev.w       xr10,   xr12,    xr10
    xvpackev.w       xr14,   xr16,    xr14
    xvpackev.d       xr12,   xr14,    xr10
    xvpackod.d       xr10,   xr14,    xr10
    xvpickev.h       xr12,   xr10,    xr12
    xvsrari.h        xr12,   xr12,    2

    xvextrins.h      xr18,   xr12,    0x70 //0 4 8 12 16 20 24  0(x0)   2 6 10 14 18 22 26  2(x2)
    xvextrins.h      xr19,   xr12,    0x74 //1 5 9 13 17 21 25  0(x1)   3 7 11 15 19 23 27  2(x3)

    xvdp2.w.h        xr0,    xr18,    xr8
    xvdp2.w.h        xr2,    xr19,    xr8
    HADDWQW          xr0
    HADDWQW          xr2
    xvpackev.w       xr0,    xr2,     xr0

    xvbsrl.v         xr18,   xr18,    2
    xvbsrl.v         xr19,   xr19,    2
    xvextrins.h      xr18,   xr12,    0x71
    xvextrins.h      xr19,   xr12,    0x75
    xvdp2.w.h        xr2,    xr18,    xr8
    xvdp2.w.h        xr4,    xr19,    xr8
    HADDWQW          xr2
    HADDWQW          xr4
    xvpackev.w       xr2,    xr4,     xr2

    xvbsrl.v         xr18,   xr18,    2
    xvbsrl.v         xr19,   xr19,    2
    xvextrins.h      xr18,   xr12,    0x72
    xvextrins.h      xr19,   xr12,    0x76
    xvdp2.w.h        xr4,    xr18,    xr8
    xvdp2.w.h        xr9,    xr19,    xr8
    HADDWQW          xr4
    HADDWQW          xr9
    xvpackev.w       xr4,    xr9,     xr4

    xvbsrl.v         xr18,   xr18,    2
    xvbsrl.v         xr19,   xr19,    2
    xvextrins.h      xr18,   xr12,    0x73
    xvextrins.h      xr19,   xr12,    0x77
    xvdp2.w.h        xr9,    xr18,    xr8
    xvdp2.w.h        xr11,   xr19,    xr8
    HADDWQW          xr9
    HADDWQW          xr11
    xvpackev.w       xr9,    xr11,    xr9

    xvpackev.d       xr0,    xr2,     xr0
    xvpackev.d       xr4,    xr9,     xr4
    xvsrari.w        xr0,    xr0,     6
    xvsrari.w        xr4,    xr4,     6
    xvpermi.d        xr0,    xr0,     0xd8
    xvpermi.d        xr4,    xr4,     0xd8
    xvpickev.h       xr0,    xr4,     xr0
    xvpermi.d        xr0,    xr0,     0xd8
    xvst             xr0,    a0,      0
    addi.d           a0,     a0,      32

    xvbsrl.v         xr18,   xr18,    2
    xvbsrl.v         xr19,   xr19,    2

    addi.d           a4,     a4,      -4
    bnez             a4,     .l_\lable\()hv_w4_loop
    b                .l_\lable\()end_pre_8tap

.l_\lable\()hv_8w:
    addi.d           t0,     a1,      0
    addi.d           t5,     a4,      0
    srli.w           t7,     a3,      3
    slli.w           t7,     t7,      4 // store offset
    addi.d           t8,     a0,      0
.l_\lable\()hv_8w_loop0:
    xvld             xr0,    a1,      0
    xvldx            xr2,    a1,      a2
    xvldx            xr4,    a1,      t2
    xvldx            xr6,    a1,      t3

    add.d            a1,     a1,      t4
    xvld             xr10,   a1,      0
    xvldx            xr11,   a1,      a2
    xvldx            xr12,   a1,      t2

    xvbsrl.v         xr1,    xr0,     4
    xvbsrl.v         xr3,    xr2,     4
    xvbsrl.v         xr5,    xr4,     4
    xvbsrl.v         xr7,    xr6,     4

    SHUFB            xr0,    xr23,    xr9,    xr13
    SHUFB            xr1,    xr23,    xr9,    xr14
    SHUFB            xr2,    xr23,    xr9,    xr15
    SHUFB            xr3,    xr23,    xr9,    xr16
    SHUFB            xr4,    xr23,    xr9,    xr17
    SHUFB            xr5,    xr23,    xr9,    xr18
    SHUFB            xr6,    xr23,    xr9,    xr19
    SHUFB            xr7,    xr23,    xr9,    xr20

    xvdp2.h.bu.b     xr0,    xr13,    xr22
    xvdp2.h.bu.b     xr1,    xr14,    xr22
    xvdp2.h.bu.b     xr2,    xr15,    xr22
    xvdp2.h.bu.b     xr3,    xr16,    xr22
    xvdp2.h.bu.b     xr4,    xr17,    xr22
    xvdp2.h.bu.b     xr5,    xr18,    xr22
    xvdp2.h.bu.b     xr6,    xr19,    xr22
    xvdp2.h.bu.b     xr7,    xr20,    xr22

    HADDWDH          xr0
    HADDWDH          xr1
    HADDWDH          xr2
    HADDWDH          xr3
    HADDWDH          xr4
    HADDWDH          xr5
    HADDWDH          xr6
    HADDWDH          xr7

    xvpackev.w       xr0,    xr2,    xr0
    xvpackev.w       xr2,    xr6,    xr4
    xvpackev.d       xr16,   xr2,    xr0
    xvpackod.d       xr0,    xr2,    xr0
    xvpickev.h       xr0,    xr0,    xr16
    xvsrari.h        xr0,    xr0,    2   // 0 8 16 24  1 9 17 25  2 10 18 26  3 11 19 27

    xvpackev.w       xr1,    xr3,    xr1
    xvpackev.w       xr3,    xr7,    xr5
    xvpackev.d       xr16,   xr3,    xr1
    xvpackod.d       xr1,    xr3,    xr1
    xvpickev.h       xr1,    xr1,    xr16
    xvsrari.h        xr1,    xr1,    2   // 4 12 20 28  5 13 21 29  6 14 22 30  7 15 23 31

    xvbsrl.v         xr13,   xr10,    4
    xvbsrl.v         xr14,   xr11,    4
    xvbsrl.v         xr15,   xr12,    4

    SHUFB            xr10,   xr23,   xr9,    xr10
    SHUFB            xr13,   xr23,   xr9,    xr13
    SHUFB            xr11,   xr23,   xr9,    xr11
    SHUFB            xr14,   xr23,   xr9,    xr14
    SHUFB            xr12,   xr23,   xr9,    xr12
    SHUFB            xr15,   xr23,   xr9,    xr15

    xvdp2.h.bu.b     xr4,    xr10,   xr22
    xvdp2.h.bu.b     xr5,    xr13,   xr22
    xvdp2.h.bu.b     xr6,    xr11,   xr22
    xvdp2.h.bu.b     xr7,    xr14,   xr22
    xvdp2.h.bu.b     xr9,    xr12,   xr22
    xvdp2.h.bu.b     xr10,   xr15,   xr22

    HADDWDH          xr4
    HADDWDH          xr5
    HADDWDH          xr6
    HADDWDH          xr7
    HADDWDH          xr9
    HADDWDH          xr10

    xvpackev.w       xr4,    xr6,    xr4
    xvpackev.w       xr9,    xr12,   xr9
    xvpackev.d       xr16,   xr9,    xr4
    xvpackod.d       xr11,   xr9,    xr4
    xvpickev.h       xr2,    xr11,   xr16
    xvsrari.h        xr2,    xr2,    2   // 32 40 48 *  33 41 49 *  34 42 50 *  35 43 51 *

    xvpackev.w       xr5,    xr7,    xr5
    xvpackev.w       xr10,   xr12,   xr10
    xvpackev.d       xr16,   xr10,   xr5
    xvpackod.d       xr11,   xr10,   xr5
    xvpickev.h       xr3,    xr11,   xr16
    xvsrari.h        xr3,    xr3,    2   // 36 44 52 *  37 45 53 *  38 46 54 *  39 47 56 *

    xvpackev.d       xr18,   xr2,    xr0 // 0 8 16 24 32 40 48 *  2 10 18 26 34 42 50 *
    xvpackod.d       xr19,   xr2,    xr0 // 1 9 17 25 33 41 49 *  3 11 19 27 35 43 51 *
    xvpackev.d       xr20,   xr3,    xr1 // 4 12 20 28 36 44 52 *  6 14 22 30 38 46 54 *
    xvpackod.d       xr21,   xr3,    xr1 // 5 13 21 29 37 45 53 *  7 15 23 31 39 47 55 *

.l_\lable\()hv_8w_loop:
    xvldx            xr0,    a1,     t3
    add.d            a1,     a1,     t4
    xvld             xr2,    a1,     0
    xvldx            xr4,    a1,     a2
    xvldx            xr6,    a1,     t2

    xvbsrl.v         xr1,    xr0,    4
    xvbsrl.v         xr3,    xr2,    4
    xvbsrl.v         xr5,    xr4,    4
    xvbsrl.v         xr7,    xr6,    4

    SHUFB            xr0,    xr23,   xr9,   xr0
    SHUFB            xr1,    xr23,   xr9,   xr1
    SHUFB            xr2,    xr23,   xr9,   xr2
    SHUFB            xr3,    xr23,   xr9,   xr3
    SHUFB            xr4,    xr23,   xr9,   xr4
    SHUFB            xr5,    xr23,   xr9,   xr5
    SHUFB            xr6,    xr23,   xr9,   xr6
    SHUFB            xr7,    xr23,   xr9,   xr7

    xvdp2.h.bu.b     xr10,   xr0,    xr22
    xvdp2.h.bu.b     xr11,   xr1,    xr22
    xvdp2.h.bu.b     xr12,   xr2,    xr22
    xvdp2.h.bu.b     xr13,   xr3,    xr22
    xvdp2.h.bu.b     xr14,   xr4,    xr22
    xvdp2.h.bu.b     xr15,   xr5,    xr22
    xvdp2.h.bu.b     xr16,   xr6,    xr22
    xvdp2.h.bu.b     xr17,   xr7,    xr22

    HADDWDH          xr10
    HADDWDH          xr11
    HADDWDH          xr12
    HADDWDH          xr13
    HADDWDH          xr14
    HADDWDH          xr15
    HADDWDH          xr16
    HADDWDH          xr17

    xvpackev.w       xr0,    xr12,   xr10
    xvpackev.w       xr2,    xr16,   xr14
    xvpackev.d       xr9,    xr2,    xr0
    xvpackod.d       xr0,    xr2,    xr0
    xvpickev.h       xr0,    xr0,    xr9
    xvsrari.h        xr0,    xr0,    2   // 56 64 72 80  57 65 73 81  58 66 74 82  59 67 75 83

    xvpackev.w       xr1,    xr13,   xr11
    xvpackev.w       xr3,    xr17,   xr15
    xvpackev.d       xr9,    xr3,    xr1
    xvpackod.d       xr1,    xr3,    xr1
    xvpickev.h       xr1,    xr1,    xr9
    xvsrari.h        xr1,    xr1,    2   // 60 68 76 84  61 69 77 85  62 70 78 86  63 71 79 87

    xvextrins.h      xr18,   xr0,    0x70 // 0 8 16 24 32 40 48 (56)  2 10 18 26 34 42 50 (58)
    xvextrins.h      xr19,   xr0,    0x74 // 1 9 17 25 33 41 49 (57)  3 11 19 27 35 43 51 (59)
    xvextrins.h      xr20,   xr1,    0x70
    xvextrins.h      xr21,   xr1,    0x74

    //h - 1
    xvdp2.w.h        xr10,   xr18,   xr8
    xvdp2.w.h        xr11,   xr19,   xr8
    xvdp2.w.h        xr12,   xr20,   xr8
    xvdp2.w.h        xr13,   xr21,   xr8

    HADDWQW          xr10
    HADDWQW          xr11
    HADDWQW          xr12
    HADDWQW          xr13

    xvpackev.w       xr2,    xr11,   xr10 //0 1 * * 2 3 * *
    xvpackev.w       xr3,    xr13,   xr12 //4 5 * * 6 7 * *
    xvpackev.d       xr2,    xr3,    xr2  //0 1 4 5  2 3 6 7
    //h - 2
    xvbsrl.v         xr4,    xr18,   2
    xvbsrl.v         xr5,    xr19,   2
    xvbsrl.v         xr6,    xr20,   2
    xvbsrl.v         xr7,    xr21,   2
    xvextrins.h      xr4,    xr0,    0x71
    xvextrins.h      xr5,    xr0,    0x75
    xvextrins.h      xr6,    xr1,    0x71
    xvextrins.h      xr7,    xr1,    0x75

    xvdp2.w.h        xr10,   xr4,    xr8
    xvdp2.w.h        xr11,   xr5,    xr8
    xvdp2.w.h        xr12,   xr6,    xr8
    xvdp2.w.h        xr13,   xr7,    xr8

    HADDWQW          xr10
    HADDWQW          xr11
    HADDWQW          xr12
    HADDWQW          xr13

    xvpackev.w       xr14,   xr11,   xr10
    xvpackev.w       xr15,   xr13,   xr12
    xvpackev.d       xr14,   xr15,   xr14 //8 9 12 13  10 11 14 15
    //h - 3
    xvbsrl.v         xr4,    xr4,    2
    xvbsrl.v         xr5,    xr5,    2
    xvbsrl.v         xr6,    xr6,    2
    xvbsrl.v         xr7,    xr7,    2
    xvextrins.h      xr4,    xr0,    0x72
    xvextrins.h      xr5,    xr0,    0x76
    xvextrins.h      xr6,    xr1,    0x72
    xvextrins.h      xr7,    xr1,    0x76

    xvdp2.w.h        xr10,   xr4,    xr8
    xvdp2.w.h        xr11,   xr5,    xr8
    xvdp2.w.h        xr12,   xr6,    xr8
    xvdp2.w.h        xr13,   xr7,    xr8

    HADDWQW          xr10
    HADDWQW          xr11
    HADDWQW          xr12
    HADDWQW          xr13

    xvpackev.w       xr15,   xr11,   xr10
    xvpackev.w       xr16,   xr13,   xr12
    xvpackev.d       xr15,   xr16,   xr15 //16 17 20 21  18 19 22 23
    //h - 4
    xvbsrl.v         xr4,    xr4,    2
    xvbsrl.v         xr5,    xr5,    2
    xvbsrl.v         xr6,    xr6,    2
    xvbsrl.v         xr7,    xr7,    2
    xvextrins.h      xr4,    xr0,    0x73
    xvextrins.h      xr5,    xr0,    0x77
    xvextrins.h      xr6,    xr1,    0x73
    xvextrins.h      xr7,    xr1,    0x77

    xvdp2.w.h        xr10,   xr4,    xr8
    xvdp2.w.h        xr11,   xr5,    xr8
    xvdp2.w.h        xr12,   xr6,    xr8
    xvdp2.w.h        xr13,   xr7,    xr8

    HADDWQW          xr10
    HADDWQW          xr11
    HADDWQW          xr12
    HADDWQW          xr13

    xvpackev.w       xr16,   xr11,   xr10
    xvpackev.w       xr17,   xr13,   xr12
    xvpackev.d       xr16,   xr17,   xr16 //24 25 28 29  26 27 30 31

    xvsrari.w        xr2,    xr2,    6
    xvsrari.w        xr14,   xr14,   6
    xvsrari.w        xr15,   xr15,   6
    xvsrari.w        xr16,   xr16,   6

    xvpermi.d        xr2,    xr2,    0xd8
    xvpermi.d        xr14,   xr14,   0xd8
    xvpermi.d        xr15,   xr15,   0xd8
    xvpermi.d        xr16,   xr16,   0xd8
    xvpickev.h       xr2,    xr14,   xr2
    xvpickev.h       xr3,    xr16,   xr15
    xvpermi.d        xr2,    xr2,    0xd8
    xvpermi.d        xr3,    xr3,    0xd8

    xvpermi.q        xr10,   xr2,    0x31
    xvpermi.q        xr11,   xr3,    0x31

    vst              vr2,    a0,     0
    vstx             vr10,   a0,     t7 //32
    slli.w           t1,     t7,     1  //64
    vstx             vr3,    a0,     t1
    add.w            t1,     t1,     t7 //96
    vstx             vr11,   a0,     t1
    slli.w           t1,     t7,     2  //128
    add.d            a0,     a0,     t1

    xvbsrl.v         xr18,   xr4,    2
    xvbsrl.v         xr19,   xr5,    2
    xvbsrl.v         xr20,   xr6,    2
    xvbsrl.v         xr21,   xr7,    2

    addi.d           a4,     a4,     -4
    bnez             a4,     .l_\lable\()hv_8w_loop

    addi.d           a1,     t0,     8
    addi.d           t0,     t0,     8
    addi.d           a0,     t8,     16
    addi.d           t8,     t8,     16
    addi.d           a4,     t5,     0
    addi.d           a3,     a3,    -8
    bnez             a3,     .l_\lable\()hv_8w_loop0
    b                .l_\lable\()end_pre_8tap
.l_\lable\()v:

    srli.w           a7,    a7,    2
    blt              t0,    a4,    .l_\lable\()v_idx_fv
    andi             a7,    a7,    1
    addi.w           a7,    a7,    3
.l_\lable\()v_idx_fv:
    addi.w           t5,     zero,  120
    mul.w            a7,     a7,    t5
    addi.w           t5,     a6,    -1
    slli.w           t5,     t5,    3
    add.w            a7,     a7,    t5
    add.d            a7,     t6,    a7 //fv's offset
    xvldrepl.d       xr8,    a7,     0

    sub.d            a1,     a1,     t3
    beq              a3,     t0,     .l_\lable\()v_4w
    blt              t0,     a3,     .l_\lable\()v_8w
.l_\lable\()v_4w:
    fld.s            f0,     a1,     0
    fldx.s           f1,     a1,     a2
    fldx.s           f2,     a1,     t2
    add.d            a1,     a1,     t3
    fld.s            f3,     a1,     0
    fldx.s           f4,     a1,     a2
    fldx.s           f5,     a1,     t2
    fldx.s           f6,     a1,     t3

    xvilvl.b         xr0,    xr1,    xr0 // 0 1  8 9  16 17 24 25
    xvilvl.b         xr1,    xr3,    xr2 // 2 3 10 11 18 19 26 27
    xvilvl.b         xr2,    xr5,    xr4 // 4 5 12 13 20 21 28 29
    xvilvl.b         xr3,    xr7,    xr6 // 6 7 14 15 22 23 30 31
    xvilvl.h         xr0,    xr1,    xr0 // 0 1 2 3  8  9  10 11  16 17 18 19  24 25 26 27
    xvilvl.h         xr1,    xr3,    xr2 // 4 5 6 7  12 13 14 15  20 21 22 23  28 29 30 31
    xvilvl.w         xr2,    xr1,    xr0
    xvilvh.w         xr0,    xr1,    xr0
    xvpermi.q        xr0,    xr2,    0x20

.l_\lable\()v_4w_loop:
    add.d            a1,     a1,     t4
    fld.s            f7,     a1,     0  //h0
    fldx.s           f10,    a1,     a2 //h1
    fldx.s           f11,    a1,     t2 //h2
    fldx.s           f12,    a1,     t3 //h3

    xvbsrl.v         xr9,    xr7,    2
    xvpermi.q        xr9,    xr7,    0x20
    xvextrins.b      xr0,    xr9,    0x70
    xvextrins.b      xr0,    xr9,    0xf1

    xvbsrl.v         xr1,    xr0,    1
    xvbsrl.v         xr7,    xr10,   2
    xvpermi.q        xr7,    xr10,   0x20
    xvextrins.b      xr1,    xr7,    0x70
    xvextrins.b      xr1,    xr7,    0xf1

    xvbsrl.v         xr2,    xr1,    1
    xvbsrl.v         xr7,    xr11,   2
    xvpermi.q        xr7,    xr11,   0x20
    xvextrins.b      xr2,    xr7,    0x70
    xvextrins.b      xr2,    xr7,    0xf1

    xvbsrl.v         xr3,    xr2,    1
    xvbsrl.v         xr7,    xr12,   2
    xvpermi.q        xr7,    xr12,   0x20
    xvextrins.b      xr3,    xr7,    0x70
    xvextrins.b      xr3,    xr7,    0xf1
    xvbsrl.v         xr4,    xr3,    1

    xvdp2.h.bu.b     xr10,   xr0,    xr8
    xvdp2.h.bu.b     xr11,   xr1,    xr8
    xvdp2.h.bu.b     xr12,   xr2,    xr8
    xvdp2.h.bu.b     xr13,   xr3,    xr8
    HADDWDH          xr10
    HADDWDH          xr11
    HADDWDH          xr12
    HADDWDH          xr13
    xvpickev.w       xr10,   xr11,   xr10
    xvpickev.w       xr11,   xr13,   xr12
    xvpermi.d        xr10,   xr10,   0xd8
    xvpermi.d        xr11,   xr11,   0xd8
    xvpickev.h       xr10,   xr11,   xr10
    xvpermi.d        xr10,   xr10,   0xd8
    xvsrari.h        xr10,   xr10,   2

    xvaddi.bu        xr0,    xr4,    0

    xvst             xr10,   a0,     0
    addi.d           a0,     a0,     32
    addi.w           a4,     a4,     -4
    bnez             a4,     .l_\lable\()v_4w_loop
    b                .l_\lable\()end_pre_8tap

.l_\lable\()v_8w:
    addi.d           t0,     a1,     0
    addi.d           t5,     a4,     0
    srli.w           t7,     a3,     2
    slli.w           t7,     t7,     3
    addi.d           t8,     a0,     0
.l_\lable\()v_8w_loop0:
    fld.s            f0,     a1,     0
    fldx.s           f1,     a1,     a2
    fldx.s           f2,     a1,     t2
    add.d            a1,     a1,     t3
    fld.s            f3,     a1,     0
    fldx.s           f4,     a1,     a2
    fldx.s           f5,     a1,     t2
    fldx.s           f6,     a1,     t3

    xvilvl.b         xr0,    xr1,    xr0 // 0 1  8 9  16 17 24 25
    xvilvl.b         xr1,    xr3,    xr2 // 2 3 10 11 18 19 26 27
    xvilvl.b         xr2,    xr5,    xr4 // 4 5 12 13 20 21 28 29
    xvilvl.b         xr3,    xr7,    xr6 // 6 7 14 15 22 23 30 31
    xvilvl.h         xr0,    xr1,    xr0 // 0 1 2 3  8  9  10 11  16 17 18 19  24 25 26 27
    xvilvl.h         xr1,    xr3,    xr2 // 4 5 6 7  12 13 14 15  20 21 22 23  28 29 30 31
    xvilvl.w         xr2,    xr1,    xr0
    xvilvh.w         xr0,    xr1,    xr0
    xvpermi.q        xr0,    xr2,    0x20

.l_\lable\()v_8w_loop:
    add.d            a1,     a1,     t4
    fld.s            f7,     a1,     0  //h0
    fldx.s           f10,    a1,     a2 //h1
    fldx.s           f11,    a1,     t2 //h2
    fldx.s           f12,    a1,     t3 //h3

    xvbsrl.v         xr9,    xr7,    2
    xvpermi.q        xr9,    xr7,    0x20
    xvextrins.b      xr0,    xr9,    0x70
    xvextrins.b      xr0,    xr9,    0xf1

    xvbsrl.v         xr1,    xr0,    1
    xvbsrl.v         xr7,    xr10,   2
    xvpermi.q        xr7,    xr10,   0x20
    xvextrins.b      xr1,    xr7,    0x70
    xvextrins.b      xr1,    xr7,    0xf1

    xvbsrl.v         xr2,    xr1,    1
    xvbsrl.v         xr7,    xr11,   2
    xvpermi.q        xr7,    xr11,   0x20
    xvextrins.b      xr2,    xr7,    0x70
    xvextrins.b      xr2,    xr7,    0xf1

    xvbsrl.v         xr3,    xr2,    1
    xvbsrl.v         xr7,    xr12,   2
    xvpermi.q        xr7,    xr12,   0x20
    xvextrins.b      xr3,    xr7,    0x70
    xvextrins.b      xr3,    xr7,    0xf1
    xvbsrl.v         xr4,    xr3,    1

    xvdp2.h.bu.b     xr10,   xr0,    xr8
    xvdp2.h.bu.b     xr11,   xr1,    xr8
    xvdp2.h.bu.b     xr12,   xr2,    xr8
    xvdp2.h.bu.b     xr13,   xr3,    xr8
    HADDWDH          xr10
    HADDWDH          xr11
    HADDWDH          xr12
    HADDWDH          xr13
    xvpickev.w       xr10,   xr11,   xr10
    xvpickev.w       xr11,   xr13,   xr12
    xvpermi.d        xr10,   xr10,   0xd8
    xvpermi.d        xr11,   xr11,   0xd8
    xvpickev.h       xr10,   xr11,   xr10
    xvpermi.d        xr10,   xr10,   0xd8
    xvsrari.h        xr10,   xr10,   2

    xvaddi.bu        xr0,    xr4,    0

    xvstelm.d        xr10,   a0,     0,    0
    add.d            a0,     a0,     t7
    xvstelm.d        xr10,   a0,     0,    1
    add.d            a0,     a0,     t7
    xvstelm.d        xr10,   a0,     0,    2
    add.d            a0,     a0,     t7
    xvstelm.d        xr10,   a0,     0,    3
    add.d            a0,     a0,     t7
    addi.w           a4,     a4,     -4
    bnez             a4,     .l_\lable\()v_8w_loop

    addi.d           a1,     t0,     4
    addi.d           t0,     t0,     4
    addi.d           a0,     t8,     8
    addi.d           t8,     t8,     8
    addi.d           a4,     t5,     0
    addi.d           a3,     a3,     -4
    bnez             a3,     .l_\lable\()v_8w_loop0

.l_\lable\()end_pre_8tap:
.endm

function prep_8tap_regular_8bpc_lasx
    addi.w a7, zero, 0
    PREP_8TAP_8BPC_LASX 0
endfunc

function prep_8tap_smooth_regular_8bpc_lasx
    addi.w a7, zero, 1
    PREP_8TAP_8BPC_LASX 1
endfunc

function prep_8tap_sharp_regular_8bpc_lasx
    addi.w a7, zero, 2
    PREP_8TAP_8BPC_LASX 2
endfunc

function prep_8tap_regular_smooth_8bpc_lasx
    addi.w a7, zero, 4
    PREP_8TAP_8BPC_LASX 4
endfunc

function prep_8tap_smooth_8bpc_lasx
    addi.w a7, zero, 5
    PREP_8TAP_8BPC_LASX 5
endfunc

function prep_8tap_sharp_smooth_8bpc_lasx
    addi.w a7, zero, 6
    PREP_8TAP_8BPC_LASX 6
endfunc

function prep_8tap_regular_sharp_8bpc_lasx
    addi.w a7, zero, 8
    PREP_8TAP_8BPC_LASX 8
endfunc

function prep_8tap_smooth_sharp_8bpc_lasx
    addi.w a7, zero, 9
    PREP_8TAP_8BPC_LASX 9
endfunc

function prep_8tap_sharp_8bpc_lasx
    addi.w a7, zero, 10
    PREP_8TAP_8BPC_LASX 10
endfunc
