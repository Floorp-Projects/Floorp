/*
 * Copyright © 2021, VideoLAN and dav1d authors
 * Copyright © 2021, Martin Storsjo
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

#include "src/arm/asm.S"
#include "util.S"
#include "src/arm/asm-offsets.h"

#define GRAIN_WIDTH 82

.macro gather_interleaved dst1, dst2, src1, src2, off
        vmov.u8         r11, \src1[0+\off]
        vmov.u8         r12, \src2[0+\off]
        add             r11, r11, r3
        vmov.u8         lr,  \src1[2+\off]
        add             r12, r12, r3
        vld1.8          {\dst1[0+\off]}, [r11]
        vmov.u8         r11, \src2[2+\off]
        add             lr,  lr,  r3
        vld1.8          {\dst2[0+\off]}, [r12]
        vmov.u8         r12, \src1[4+\off]
        add             r11, r11, r3
        vld1.8          {\dst1[2+\off]}, [lr]
        vmov.u8         lr,  \src2[4+\off]
        add             r12, r12, r3
        vld1.8          {\dst2[2+\off]}, [r11]
        vmov.u8         r11, \src1[6+\off]
        add             lr,  lr,  r3
        vld1.8          {\dst1[4+\off]}, [r12]
        vmov.u8         r12, \src2[6+\off]
        add             r11, r11, r3
        vld1.8          {\dst2[4+\off]}, [lr]
        add             r12, r12, r3
        vld1.8          {\dst1[6+\off]}, [r11]
        vld1.8          {\dst2[6+\off]}, [r12]
.endm

.macro gather dst1, dst2, dst3, dst4, src1, src2, src3, src4
        gather_interleaved \dst1, \dst3, \src1, \src3, 0
        gather_interleaved \dst1, \dst3, \src1, \src3, 1
        gather_interleaved \dst2, \dst4, \src2, \src4, 0
        gather_interleaved \dst2, \dst4, \src2, \src4, 1
.endm

function gather32_neon
        push            {r11-r12,lr}
        gather          d8,  d9,  d10, d11, d0,  d1,  d2,  d3
        pop             {r11-r12,pc}
endfunc

function gather16_neon
        push            {r11-r12,lr}
        gather_interleaved d8,  d9,  d0,  d1,  0
        gather_interleaved d8,  d9,  d0,  d1,  1
        pop             {r11-r12,pc}
endfunc

const overlap_coeffs_0, align=4
        .byte 27, 17, 0,  0,  0,  0,  0,  0
        .byte 17, 27, 32, 32, 32, 32, 32, 32
endconst

const overlap_coeffs_1, align=4
        .byte 23, 0,  0,  0,  0,  0,  0,  0
        .byte 22, 32, 32, 32, 32, 32, 32, 32
endconst

.macro calc_offset offx, offy, src, sx, sy
        and             \offy, \src,  #0xF     // randval & 0xF
        lsr             \offx, \src,  #4       // randval >> 4
.if \sy == 0
        add             \offy, \offy, \offy    // 2 * (randval & 0xF)
.endif
.if \sx == 0
        add             \offx, \offx, \offx    // 2 * (randval >> 4)
.endif
.endm

.macro add_offset dst, offx, offy, src, stride
        mla             \dst, \stride, \offy, \src // grain_lut += grain_stride * offy
        add             \dst, \dst, \offx          // grain_lut += offx
.endm

// void dav1d_fgy_32x32_8bpc_neon(pixel *const dst, const pixel *const src,
//                                const ptrdiff_t stride,
//                                const uint8_t scaling[SCALING_SIZE],
//                                const int scaling_shift,
//                                const entry grain_lut[][GRAIN_WIDTH],
//                                const int offsets[][2],
//                                const int h, const ptrdiff_t clip,
//                                const ptrdiff_t type);
function fgy_32x32_8bpc_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]   // scaling_shift, grain_lut
        ldrd            r6,  r7,  [sp, #108]   // offsets, h
        ldr             r8,       [sp, #116]   // clip
        mov             r9,  #GRAIN_WIDTH      // grain_lut stride

        neg             r4,  r4
        vdup.16         q13, r4                // -scaling_shift
        cmp             r8,  #0

        movrel_local    r12, overlap_coeffs_0

        beq             1f
        // clip
        vmov.i8         q14, #16
        vmov.i8         q15, #235
        b               2f
1:
        // no clip
        vmov.i8         q14, #0
        vmov.i8         q15, #255
2:

        vld1.8          {d24, d25}, [r12, :128] // overlap_coeffs

        add             r5,  r5,  #9           // grain_lut += 9
        add             r5,  r5,  r9,  lsl #3  // grain_lut += 8 * grain_stride
        add             r5,  r5,  r9           // grain_lut += grain_stride

        ldr             r10, [r6, #8]          // offsets[1][0]
        calc_offset     r10, r4,  r10, 0,   0
        add_offset      r4,  r10, r4,  r5,  r9
        ldr             r10, [r6, #4]          // offsets[0][1]
        calc_offset     r10, r11, r10, 0,   0
        add_offset      r11, r10, r11, r5,  r9
        ldr             r10, [r6, #12]         // offsets[1][1]
        calc_offset     r10, r8,  r10, 0,   0
        add_offset      r8,  r10, r8,  r5,  r9
        ldr             r6,  [r6]              // offsets[0][0]
        calc_offset     r6,  lr,  r6,  0,   0
        add_offset      r5,  r6,  lr,  r5,  r9

        add             r4,  r4,  #32          // grain_lut += BLOCK_SIZE * bx
        add             r6,  r11, r9,  lsl #5  // grain_lut += grain_stride * BLOCK_SIZE * by

        ldr             r10, [sp, #120]        // type
        adr             r11, L(fgy_loop_tbl)

        tst             r10, #1
        ldr             r10, [r11, r10, lsl #2]

        add             r8,  r8,  r9,  lsl #5  // grain_lut += grain_stride * BLOCK_SIZE * by
        add             r8,  r8,  #32          // grain_lut += BLOCK_SIZE * bx

        add             r11, r11, r10

        beq             1f
        // y overlap
        vdup.8          d14, d24[0]
        vdup.8          d15, d24[1]
        mov             r10, r7                // backup actual h
        mov             r7,  #2
1:
        bx              r11
endfunc

function fgy_loop_neon
L(fgy_loop_tbl):
        .word L(loop_00) - L(fgy_loop_tbl) + CONFIG_THUMB
        .word L(loop_01) - L(fgy_loop_tbl) + CONFIG_THUMB
        .word L(loop_10) - L(fgy_loop_tbl) + CONFIG_THUMB
        .word L(loop_11) - L(fgy_loop_tbl) + CONFIG_THUMB

.macro fgy ox, oy
L(loop_\ox\oy):
1:
.if \ox
        vld1.8          {d8},       [r4],       r9 // grain_lut old
.endif
.if \oy
        vld1.8          {q2, q3},   [r6],       r9 // grain_lut top
.endif
.if \ox && \oy
        vld1.8          {d10},      [r8],       r9 // grain_lut top old
.endif
        vld1.8          {q0,  q1},  [r1, :128], r2 // src
        vld1.8          {q10, q11}, [r5],       r9 // grain_lut

.if \ox
        vmull.s8        q4,  d8,  d24
        vmlal.s8        q4,  d20, d25
.endif

.if \oy
.if \ox
        vmull.s8        q5,  d10, d24
        vmlal.s8        q5,  d4,  d25
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d4,  q5,  #5
.endif

        vmull.s8        q4,  d20, d15
        vmull.s8        q5,  d21, d15
        vmull.s8        q8,  d22, d15
        vmull.s8        q9,  d23, d15
        vmlal.s8        q4,  d4,  d14
        vmlal.s8        q5,  d5,  d14
        vmlal.s8        q8,  d6,  d14
        vmlal.s8        q9,  d7,  d14
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d21, q5,  #5
        vqrshrn.s16     d22, q8,  #5
        vqrshrn.s16     d23, q9,  #5
.elseif \ox
        vqrshrn.s16     d20, q4,  #5
.endif

        bl              gather32_neon

        vmovl.s8        q8,  d20       // grain
        vmovl.s8        q9,  d21
        vmovl.s8        q10, d22
        vmovl.s8        q11, d23

        vmovl.u8        q2,  d8        // scaling
        vmovl.u8        q3,  d9
        vmovl.u8        q4,  d10
        vmovl.u8        q5,  d11

        vmul.i16        q8,  q8,  q2   // scaling * grain
        vmul.i16        q9,  q9,  q3
        vmul.i16        q10, q10, q4
        vmul.i16        q11, q11, q5

        vrshl.s16       q8,  q8,  q13  // round2(scaling * grain, scaling_shift)
        vrshl.s16       q9,  q9,  q13
        vrshl.s16       q10, q10, q13
        vrshl.s16       q11, q11, q13

        vaddw.u8        q8,  q8,  d0   // *src + noise
        vaddw.u8        q9,  q9,  d1
        vaddw.u8        q10, q10, d2
        vaddw.u8        q11, q11, d3

        vqmovun.s16     d0,  q8
        vqmovun.s16     d1,  q9
        vqmovun.s16     d2,  q10
        vqmovun.s16     d3,  q11

        vmax.u8         q0,  q0,  q14
        vmax.u8         q1,  q1,  q14
        vmin.u8         q0,  q0,  q15
        vmin.u8         q1,  q1,  q15

        subs            r7,  r7,  #1
.if \oy
        vdup.8          d14, d25[0]
        vdup.8          d15, d25[1]
.endif
        vst1.8          {q0, q1}, [r0, :128], r2 // dst
        bgt             1b

.if \oy
        cmp             r10, #2
        sub             r7,  r10, #2           // restore actual remaining h
        bgt             L(loop_\ox\()0)
.endif
        vpop            {q4-q7}
        pop             {r4-r11,pc}
.endm

        fgy             0, 0
        fgy             0, 1
        fgy             1, 0
        fgy             1, 1
endfunc

// void dav1d_fguv_32x32_420_8bpc_neon(pixel *const dst,
//                                     const pixel *const src,
//                                     const ptrdiff_t stride,
//                                     const uint8_t scaling[SCALING_SIZE],
//                                     const Dav1dFilmGrainData *const data,
//                                     const entry grain_lut[][GRAIN_WIDTH],
//                                     const pixel *const luma_row,
//                                     const ptrdiff_t luma_stride,
//                                     const int offsets[][2],
//                                     const ptrdiff_t h, const ptrdiff_t uv,
//                                     const ptrdiff_t is_id,
//                                     const ptrdiff_t type);
.macro fguv layout, sx, sy
function fguv_32x32_\layout\()_8bpc_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]   // data, grain_lut
        ldrd            r6,  r7,  [sp, #108]   // luma_row, luma_stride
        ldrd            r8,  r9,  [sp, #116]   // offsets, h
        ldrd            r10, r11, [sp, #124]   // uv, is_id

        // !csfl
        add             r10, r4,  r10, lsl #2  // + 4*uv
        add             r12, r10, #FGD_UV_LUMA_MULT
        add             lr,  r10, #FGD_UV_MULT
        add             r10, r10, #FGD_UV_OFFSET
        vld1.16         {d4[]},  [r12]         // uv_luma_mult
        vld1.16         {d4[2]}, [r10]         // uv_offset
        vld1.16         {d4[1]}, [lr]          // uv_mult

        ldr             lr,  [r4, #FGD_SCALING_SHIFT]
        ldr             r12, [r4, #FGD_CLIP_TO_RESTRICTED_RANGE]
        neg             lr,  lr                // -scaling_shift

        cmp             r12, #0
        vdup.16         q13, lr                // -scaling_shift

        beq             1f
        // clip
        cmp             r11, #0
        vmov.i8         q14, #16
        vmov.i8         q15, #240
        beq             2f
        // is_id
        vmov.i8         q15, #235
        b               2f
1:
        // no clip
        vmov.i8         q14, #0
        vmov.i8         q15, #255
2:

        mov             r10, #GRAIN_WIDTH      // grain_lut stride

        add             r5,  r5,  #(3 + (2 >> \sx)*3) // grain_lut += 9 or 6
.if \sy
        add             r5,  r5,  r10, lsl #2  // grain_lut += 4 * grain_stride
        add             r5,  r5,  r10, lsl #1  // grain_lut += 2 * grain_stride
.else
        add             r5,  r5,  r10, lsl #3  // grain_lut += 8 * grain_stride
        add             r5,  r5,  r10          // grain_lut += grain_stride
.endif

        ldr             r12, [r8, #8]          // offsets[1][0]
        calc_offset     r12, r4,  r12, \sx, \sy
        add_offset      r4,  r12, r4,  r5,  r10

        ldr             r12, [r8, #4]          // offsets[0][1]
        calc_offset     r12, lr,  r12, \sx, \sy
        add_offset      lr,  r12, lr,  r5,  r10

        ldr             r12, [r8, #12]         // offsets[1][1]
        calc_offset     r12, r11, r12, \sx, \sy
        add_offset      r11, r12, r11, r5,  r10

        ldr             r8,  [r8]              // offsets[0][0]
        calc_offset     r8,  r12, r8,  \sx, \sy
        add_offset      r5,  r8,  r12, r5,  r10

        add             r4,  r4,  #(32 >> \sx) // grain_lut += BLOCK_SIZE * bx
        add             r8,  lr,  r10, lsl #(5 - \sy) // grain_lut += grain_stride * BLOCK_SIZE * by
        add             r11, r11, r10, lsl #(5 - \sy) // grain_lut += grain_stride * BLOCK_SIZE * by
        add             r11, r11, #(32 >> \sx) // grain_lut += BLOCK_SIZE * bx

        movrel_local    r12, overlap_coeffs_\sx
        ldr             lr,  [sp, #132]        // type

        vld1.8          {d24, d25}, [r12, :128] // overlap_coeffs

        movrel_local    r12, L(fguv_loop_sx\sx\()_tbl)
#if CONFIG_THUMB
        // This uses movrel_local instead of adr above, because the target
        // can be out of range for adr. But movrel_local leaves the thumb bit
        // set on COFF (but probably wouldn't if building for thumb on ELF),
        // thus try to clear the bit for robustness.
        bic             r12, r12, #1
#endif

        tst             lr,  #1
        ldr             lr,  [r12, lr,  lsl #2]

        add             r12, r12, lr

        beq             1f
        // y overlap
        sub             lr,  r9,  #(2 >> \sy)  // backup remaining h
        mov             r9,  #(2 >> \sy)

1:

.if \sy
        vmov.i8         d6,  #23
        vmov.i8         d7,  #22
.else
        vmov.i8         d6,  #27
        vmov.i8         d7,  #17
.endif

.if \sy
        add             r7,  r7,  r7           // luma_stride *= 2
.endif

        bx              r12
endfunc
.endm

fguv 420, 1, 1
fguv 422, 1, 0
fguv 444, 0, 0

function fguv_loop_sx0_neon
L(fguv_loop_sx0_tbl):
        .word L(fguv_loop_sx0_csfl0_00) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl0_01) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl0_10) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl0_11) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl1_00) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl1_01) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl1_10) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl1_11) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB

.macro fguv_loop_sx0 csfl, ox, oy
L(fguv_loop_sx0_csfl\csfl\()_\ox\oy):
.if \oy
        mov             r12, lr
.endif
1:
.if \ox
        vld1.8          {d8},       [r4],        r10 // grain_lut old
.endif
.if \oy
        vld1.8          {q8, q9},   [r8],        r10 // grain_lut top
.endif
.if \ox && \oy
        vld1.8          {d10},      [r11],       r10 // grain_lut top old
.endif
        vld1.8          {q0,  q1},  [r6, :128],  r7  // luma
        vld1.8          {q10, q11}, [r5],        r10 // grain_lut

.if \ox
        vmull.s8        q4,  d8,  d24
        vmlal.s8        q4,  d20, d25
.endif

.if \oy
.if \ox
        vmull.s8        q5,  d10, d24
        vmlal.s8        q5,  d16, d25
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d16, q5,  #5
.endif

        vmull.s8        q4,  d20, d7
        vmull.s8        q5,  d21, d7
        vmull.s8        q6,  d22, d7
        vmull.s8        q7,  d23, d7
        vmlal.s8        q4,  d16, d6
        vmlal.s8        q5,  d17, d6
        vmlal.s8        q6,  d18, d6
        vmlal.s8        q7,  d19, d6
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d21, q5,  #5
        vqrshrn.s16     d22, q6,  #5
        vqrshrn.s16     d23, q7,  #5
.elseif \ox
        vqrshrn.s16     d20, q4,  #5
.endif
.if !\csfl
        vld1.8          {q8,  q9},  [r1, :128] // src
        vmovl.u8        q4,  d0
        vmovl.u8        q5,  d1
        vmovl.u8        q6,  d2
        vmovl.u8        q7,  d3
        vmovl.u8        q0,  d16
        vmovl.u8        q1,  d17
        vmovl.u8        q8,  d18
        vmovl.u8        q9,  d19
        vmul.i16        q4,  q4,  d4[0]
        vmul.i16        q5,  q5,  d4[0]
        vmul.i16        q6,  q6,  d4[0]
        vmul.i16        q7,  q7,  d4[0]
        vmul.i16        q0,  q0,  d4[1]
        vmul.i16        q1,  q1,  d4[1]
        vmul.i16        q8,  q8,  d4[1]
        vmul.i16        q9,  q9,  d4[1]
        vqadd.s16       q4,  q4,  q0
        vqadd.s16       q5,  q5,  q1
        vqadd.s16       q6,  q6,  q8
        vqadd.s16       q7,  q7,  q9
        vdup.16         q0,  d4[2]
        vshr.s16        q4,  q4,  #6
        vshr.s16        q5,  q5,  #6
        vshr.s16        q6,  q6,  #6
        vshr.s16        q7,  q7,  #6
        vadd.i16        q4,  q4,  q0
        vadd.i16        q5,  q5,  q0
        vadd.i16        q6,  q6,  q0
        vadd.i16        q7,  q7,  q0
        vqmovun.s16     d0,  q4
        vqmovun.s16     d1,  q5
        vqmovun.s16     d2,  q6
        vqmovun.s16     d3,  q7
.endif

        bl              gather32_neon

        vld1.8          {q0,  q1},  [r1, :128], r2 // src

        vmovl.s8        q8,  d20       // grain
        vmovl.s8        q9,  d21
        vmovl.s8        q10, d22
        vmovl.s8        q11, d23

        vmovl.u8        q6,  d8        // scaling
        vmovl.u8        q7,  d9
        vmovl.u8        q4,  d10
        vmovl.u8        q5,  d11

        vmul.i16        q8,  q8,  q6   // scaling * grain
        vmul.i16        q9,  q9,  q7
        vmul.i16        q10, q10, q4
        vmul.i16        q11, q11, q5

        vrshl.s16       q8,  q8,  q13  // round2(scaling * grain, scaling_shift)
        vrshl.s16       q9,  q9,  q13
        vrshl.s16       q10, q10, q13
        vrshl.s16       q11, q11, q13

        vaddw.u8        q8,  q8,  d0   // *src + noise
        vaddw.u8        q9,  q9,  d1
        vaddw.u8        q10, q10, d2
        vaddw.u8        q11, q11, d3

        vqmovun.s16     d0,  q8
        vqmovun.s16     d1,  q9
        vqmovun.s16     d2,  q10
        vqmovun.s16     d3,  q11

        vmax.u8         q0,  q0,  q14
        vmax.u8         q1,  q1,  q14
        vmin.u8         q0,  q0,  q15
        vmin.u8         q1,  q1,  q15

        subs            r9,  r9,  #1
.if \oy
        vdup.8          d6,  d25[0]
        vdup.8          d7,  d25[1]
.endif

        vst1.8          {q0, q1}, [r0, :128], r2 // dst
        bgt             1b

.if \oy
        cmp             r12, #0
        mov             r9,  r12               // restore actual remaining h
        bgt             L(fguv_loop_sx0_csfl\csfl\()_\ox\()0)
.endif
        b               9f
.endm
        fguv_loop_sx0   0, 0, 0
        fguv_loop_sx0   0, 0, 1
        fguv_loop_sx0   0, 1, 0
        fguv_loop_sx0   0, 1, 1
        fguv_loop_sx0   1, 0, 0
        fguv_loop_sx0   1, 0, 1
        fguv_loop_sx0   1, 1, 0
        fguv_loop_sx0   1, 1, 1

9:
        vpop            {q4-q7}
        pop             {r4-r11,pc}
endfunc

function fguv_loop_sx1_neon
L(fguv_loop_sx1_tbl):
        .word L(fguv_loop_sx1_csfl0_00) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl0_01) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl0_10) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl0_11) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl1_00) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl1_01) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl1_10) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl1_11) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB

.macro fguv_loop_sx1 csfl, ox, oy
L(fguv_loop_sx1_csfl\csfl\()_\ox\oy):
.if \oy
        mov             r12, lr
.endif
1:
.if \ox
        vld1.8          {d8},       [r4],        r10 // grain_lut old
.endif
.if \oy
        vld1.8          {q8},       [r8],        r10 // grain_lut top
.endif
.if \ox && \oy
        vld1.8          {d10},      [r11],       r10 // grain_lut top old
.endif
        vld1.8          {q0,  q1},  [r6, :128],  r7  // luma
        vld1.8          {q10},      [r5],        r10 // grain_lut
        vld1.8          {q11},      [r1, :128],  r2  // src

.if \ox
        vmull.s8        q4,  d8,  d24
        vmlal.s8        q4,  d20, d25
.endif

        vpaddl.u8       q0,  q0
        vpaddl.u8       q1,  q1
.if \oy
.if \ox
        vmull.s8        q5,  d10, d24
        vmlal.s8        q5,  d16, d25
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d16, q5,  #5
.endif

        vmull.s8        q4,  d20, d7
        vmull.s8        q5,  d21, d7
        vmlal.s8        q4,  d16, d6
        vmlal.s8        q5,  d17, d6
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d21, q5,  #5
.elseif \ox
        vqrshrn.s16     d20, q4,  #5
.endif
.if \csfl
        vrshrn.u16      d0,  q0,  #1
        vrshrn.u16      d1,  q1,  #1
.else
        vrshr.u16       q4,  q0,  #1
        vrshr.u16       q5,  q1,  #1
        vmovl.u8        q0,  d22
        vmovl.u8        q1,  d23
        vmul.i16        q4,  q4,  d4[0]
        vmul.i16        q5,  q5,  d4[0]
        vmul.i16        q0,  q0,  d4[1]
        vmul.i16        q1,  q1,  d4[1]
        vqadd.s16       q4,  q4,  q0
        vqadd.s16       q5,  q5,  q1
        vdup.16         q0,  d4[2]
        vshr.s16        q4,  q4,  #6
        vshr.s16        q5,  q5,  #6
        vadd.i16        q4,  q4,  q0
        vadd.i16        q5,  q5,  q0
        vqmovun.s16     d0,  q4
        vqmovun.s16     d1,  q5
.endif

        bl              gather16_neon

        vmovl.s8        q8,  d20       // grain
        vmovl.s8        q9,  d21

        vmovl.u8        q6,  d8        // scaling
        vmovl.u8        q7,  d9

        vmul.i16        q8,  q8,  q6   // scaling * grain
        vmul.i16        q9,  q9,  q7

        vrshl.s16       q8,  q8,  q13  // round2(scaling * grain, scaling_shift)
        vrshl.s16       q9,  q9,  q13

        vaddw.u8        q8,  q8,  d22  // *src + noise
        vaddw.u8        q9,  q9,  d23

        vqmovun.s16     d0,  q8
        vqmovun.s16     d1,  q9

        vmax.u8         q0,  q0,  q14
        vmin.u8         q0,  q0,  q15

        subs            r9,  r9,  #1
.if \oy
        vswp            d6,  d7
.endif
        vst1.8          {q0}, [r0, :128], r2 // dst
        bgt             1b

.if \oy
        cmp             r12, #0
        mov             r9,  r12               // restore actual remaining h
        bgt             L(fguv_loop_sx1_csfl\csfl\()_\ox\()0)
.endif

        b               9f
.endm
        fguv_loop_sx1   0, 0, 0
        fguv_loop_sx1   0, 0, 1
        fguv_loop_sx1   0, 1, 0
        fguv_loop_sx1   0, 1, 1
        fguv_loop_sx1   1, 0, 0
        fguv_loop_sx1   1, 0, 1
        fguv_loop_sx1   1, 1, 0
        fguv_loop_sx1   1, 1, 1

9:
        vpop            {q4-q7}
        pop             {r4-r11,pc}
endfunc
