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

.macro gather_interleaved dst1, dst2, src1, src2, src3, src4, off
        vmov.u16        r11, \src1[0+\off]
        vmov.u16        r12, \src3[0+\off]
        add             r11, r11, r3
        vmov.u16        lr,  \src1[2+\off]
        add             r12, r12, r3
        vld1.8          {\dst1[0+\off]}, [r11]
        vmov.u16        r11, \src3[2+\off]
        add             lr,  lr,  r3
        vld1.8          {\dst2[0+\off]}, [r12]
        vmov.u16        r12, \src2[0+\off]
        add             r11, r11, r3
        vld1.8          {\dst1[2+\off]}, [lr]
        vmov.u16        lr,  \src4[0+\off]
        add             r12, r12, r3
        vld1.8          {\dst2[2+\off]}, [r11]
        vmov.u16        r11, \src2[2+\off]
        add             lr,  lr,  r3
        vld1.8          {\dst1[4+\off]}, [r12]
        vmov.u16        r12, \src4[2+\off]
        add             r11, r11, r3
        vld1.8          {\dst2[4+\off]}, [lr]
        add             r12, r12, r3
        vld1.8          {\dst1[6+\off]}, [r11]
        vld1.8          {\dst2[6+\off]}, [r12]
.endm

.macro gather dst1, dst2, dst3, dst4, src1, src2, src3, src4, src5, src6, src7, src8
        gather_interleaved \dst1, \dst3, \src1, \src2, \src5, \src6, 0
        gather_interleaved \dst1, \dst3, \src1, \src2, \src5, \src6, 1
        gather_interleaved \dst2, \dst4, \src3, \src4, \src7, \src8, 0
        gather_interleaved \dst2, \dst4, \src3, \src4, \src7, \src8, 1
.endm

function gather32_neon
        push            {r11-r12,lr}
        gather          d8,  d9,  d10, d11, d0,  d1,  d2,  d3,  d4,  d5,  d6,  d7
        pop             {r11-r12,pc}
endfunc

function gather16_neon
        push            {r11-r12,lr}
        gather_interleaved d8,  d9,  d0,  d1,  d2,  d3,  0
        gather_interleaved d8,  d9,  d0,  d1,  d2,  d3,  1
        pop             {r11-r12,pc}
endfunc

const overlap_coeffs_0, align=4
        .short 27, 17, 0,  0
        .short 17, 27, 32, 32
endconst

const overlap_coeffs_1, align=4
        .short 23, 0,  0,  0
        .short 22, 32, 32, 32
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
        add             \dst, \dst, \offx, lsl #1  // grain_lut += offx
.endm

// void dav1d_fgy_32x32_16bpc_neon(pixel *const dst, const pixel *const src,
//                                 const ptrdiff_t stride,
//                                 const uint8_t scaling[SCALING_SIZE],
//                                 const int scaling_shift,
//                                 const entry grain_lut[][GRAIN_WIDTH],
//                                 const int offsets[][2],
//                                 const int h, const ptrdiff_t clip,
//                                 const ptrdiff_t type,
//                                 const int bitdepth_max);
function fgy_32x32_16bpc_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]   // scaling_shift, grain_lut
        ldrd            r6,  r7,  [sp, #108]   // offsets, h
        ldr             r8,       [sp, #116]   // clip
        mov             r9,  #GRAIN_WIDTH*2    // grain_lut stride
        ldr             r10,      [sp, #124]   // bitdepth_max

        eor             r4,  r4,  #15          // 15 - scaling_shift
        vdup.16         q6,  r10               // bitdepth_max
        clz             r10, r10
        vdup.16         q13, r4                // 15 - scaling_shift
        rsb             r10, r10, #24          // bitdepth_min_8
        cmp             r8,  #0
        vdup.16         q12, r10               // bitdepth_min_8

        movrel_local    r12, overlap_coeffs_0

        beq             1f
        // clip
        vmov.i16        q14, #16
        vmov.i16        q15, #235
        vshl.s16        q14, q14, q12
        vshl.s16        q15, q15, q12
        b               2f
1:
        // no clip
        vmov.i16        q14, #0
        vmov            q15, q6
2:
        vshr.u16        q6,  q6,  #1           // grain_max

        vld1.16         {d24, d25}, [r12, :128] // overlap_coeffs

        add             r5,  r5,  #18          // grain_lut += 9
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

        add             r4,  r4,  #32*2        // grain_lut += BLOCK_SIZE * bx
        add             r6,  r11, r9,  lsl #5  // grain_lut += grain_stride * BLOCK_SIZE * by

        ldr             r10, [sp, #120]        // type
        adr             r11, L(fgy_loop_tbl)

        tst             r10, #1
        ldr             r10, [r11, r10, lsl #2]

        add             r8,  r8,  r9,  lsl #5  // grain_lut += grain_stride * BLOCK_SIZE * by
        add             r8,  r8,  #32*2        // grain_lut += BLOCK_SIZE * bx

        add             r11, r11, r10

        beq             1f
        // y overlap
        vdup.16         d14, d24[0]
        vdup.16         d15, d24[1]
        mov             r10, r7                // backup actual h
        mov             r7,  #2
1:
        sub             r2,  r2,  #32          // src_stride   -= 32
        sub             r9,  r9,  #32          // grain_stride -= 32
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
        vld1.16         {d0},       [r4],       r9 // grain_lut old
.endif
.if \oy
        vld1.16         {q2,  q3},  [r6]!          // grain_lut top
.endif
.if \ox && \oy
        vld1.16         {d2},       [r8],       r9 // grain_lut top old
.endif
.if \oy
        vld1.16         {q4,  q5},  [r6],       r9 // grain_lut top
.endif
.if !\ox && !\oy
        vld1.16         {q0,  q1},  [r1, :128]!    // src
.endif
        vld1.16         {q8,  q9},  [r5]!          // grain_lut
.if !\ox && !\oy
        vld1.16         {q2,  q3},  [r1, :128], r2 // src
.endif
.if !\oy
        vmvn.i16        q5,  #0xf000               // 0x0fff
.endif
        vld1.16         {q10, q11}, [r5],       r9 // grain_lut

.if \ox
        add             r4,  r4,  #32
        vmull.s16       q0,  d0,  d24
        vmlal.s16       q0,  d16, d25
.endif

.if \oy
.if \ox
        add             r8,  r8,  #32
        vmull.s16       q1,  d2,  d24
        vmlal.s16       q1,  d4,  d25
        vqrshrn.s32     d16, q0,  #5
        vmvn            d0,  d12                   // grain_min
        vqrshrn.s32     d4,  q1,  #5
        vmin.s16        d16, d16, d12
        vmin.s16        d4,  d4,  d12
        vmax.s16        d16, d16, d0
        vmax.s16        d4,  d4,  d0
.endif

        vmull.s16       q0,  d4,  d14
        vmull.s16       q1,  d5,  d14
        vmull.s16       q2,  d6,  d14
        vmull.s16       q3,  d7,  d14
        vmlal.s16       q0,  d16, d15
        vmlal.s16       q1,  d17, d15
        vmlal.s16       q2,  d18, d15
        vmlal.s16       q3,  d19, d15
        vmull.s16       q8,  d20, d15
        vmull.s16       q9,  d21, d15
        vmull.s16       q10, d22, d15
        vmull.s16       q11, d23, d15
        vmlal.s16       q8,  d8,  d14
        vmlal.s16       q9,  d9,  d14
        vmlal.s16       q10, d10, d14
        vmlal.s16       q11, d11, d14
        vmvn            q4,  q6                   // grain_min
        vqrshrn.s32     d0,  q0,  #5
        vqrshrn.s32     d1,  q1,  #5
        vqrshrn.s32     d2,  q2,  #5
        vqrshrn.s32     d3,  q3,  #5
        vqrshrn.s32     d4,  q8,  #5
        vqrshrn.s32     d5,  q9,  #5
        vqrshrn.s32     d6,  q10, #5
        vqrshrn.s32     d7,  q11, #5
        vmin.s16        q8,  q0,  q6
        vmin.s16        q9,  q1,  q6
        vld1.16         {q0,  q1},  [r1, :128]!    // src
        vmin.s16        q10, q2,  q6
        vmin.s16        q11, q3,  q6
        vmax.s16        q8,  q8,  q4
        vmax.s16        q9,  q9,  q4
        vld1.16         {q2,  q3},  [r1, :128], r2 // src
        vmvn.i16        q5,  #0xf000               // 0x0fff
        vmax.s16        q10, q10, q4
        vmax.s16        q11, q11, q4
.elseif \ox
        vmvn            d4,  d12                   // grain_min
        vqrshrn.s32     d16, q0,  #5
        vld1.16         {q0,  q1},  [r1, :128]!    // src
        vmin.s16        d16, d16, d12
        vmax.s16        d16, d16, d4
        vld1.16         {q2,  q3},  [r1, :128], r2 // src
.endif

        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        vand            q0,  q0,  q5
        vand            q1,  q1,  q5
        vand            q2,  q2,  q5
        vand            q3,  q3,  q5

        bl              gather32_neon

.if \ox || \oy
        vpush           {q6-q7}
.endif

        vmovl.u8        q6,  d8        // scaling
        vmovl.u8        q7,  d9
        vmovl.u8        q4,  d10
        vmovl.u8        q5,  d11

        vshl.u16        q6,  q6,  q13  // scaling << (15 - scaling_shift)
        vshl.u16        q7,  q7,  q13
        vshl.u16        q4,  q4,  q13
        vshl.u16        q5,  q5,  q13

        vqrdmulh.s16    q8,  q8,  q6   // round2((scaling << (15 - scaling_shift) * grain, 15)
        vqrdmulh.s16    q9,  q9,  q7
        vqrdmulh.s16    q10, q10, q4
        vqrdmulh.s16    q11, q11, q5

.if \ox || \oy
        vpop            {q6-q7}
.endif

        vqadd.s16       q0,  q0,  q8   // *src + noise
        vqadd.s16       q1,  q1,  q9
        vqadd.s16       q2,  q2,  q10
        vqadd.s16       q3,  q3,  q11

        vmax.s16        q0,  q0,  q14
        vmax.s16        q1,  q1,  q14
        vmax.s16        q2,  q2,  q14
        vmax.s16        q3,  q3,  q14
        vmin.s16        q0,  q0,  q15
        vmin.s16        q1,  q1,  q15
        vmin.s16        q2,  q2,  q15
        vmin.s16        q3,  q3,  q15

        vst1.16         {q0, q1}, [r0, :128]!    // dst
        subs            r7,  r7,  #1
.if \oy
        vdup.16         d14, d25[0]
        vdup.16         d15, d25[1]
.endif
        vst1.16         {q2, q3}, [r0, :128], r2 // dst
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

// void dav1d_fguv_32x32_420_16bpc_neon(pixel *const dst,
//                                      const pixel *const src,
//                                      const ptrdiff_t stride,
//                                      const uint8_t scaling[SCALING_SIZE],
//                                      const Dav1dFilmGrainData *const data,
//                                      const entry grain_lut[][GRAIN_WIDTH],
//                                      const pixel *const luma_row,
//                                      const ptrdiff_t luma_stride,
//                                      const int offsets[][2],
//                                      const ptrdiff_t h, const ptrdiff_t uv,
//                                      const ptrdiff_t is_id,
//                                      const ptrdiff_t type,
//                                      const int bitdepth_max);
.macro fguv layout, sx, sy
function fguv_32x32_\layout\()_16bpc_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]   // data, grain_lut
        ldrd            r10, r11, [sp, #124]   // uv, is_id
        ldr             r6,       [sp, #136]   // bitdepth_max

        clz             r7,  r6
        rsb             r7,  r7,  #24          // bitdepth_min_8

        // !csfl
        add             r10, r4,  r10, lsl #2  // + 4*uv
        add             r12, r10, #FGD_UV_LUMA_MULT
        add             lr,  r10, #FGD_UV_MULT
        ldrh            r10, [r10, #FGD_UV_OFFSET] // uv_offset
        vld1.16         {d30[]},  [r12]        // uv_luma_mult
        lsl             r10, r10, r7           // uv_offset << bitdepth_min_8
        vld1.16         {d30[1]}, [lr]         // uv_mult

        ldr             lr,  [r4, #FGD_SCALING_SHIFT]
        ldr             r12, [r4, #FGD_CLIP_TO_RESTRICTED_RANGE]
        eor             lr,  lr,  #15          // 15 - scaling_shift

        vmov.16         d30[2], r10            // uv_offset << bitdepth_min_8

        cmp             r12, #0
        vdup.16         q13, lr                // 15 - scaling_shift

        beq             1f
        // clip
        cmp             r11, #0
        mov             r8,  #16
        mov             r9,  #240
        lsl             r8,  r8,  r7
        lsl             r9,  r9,  r7
        beq             2f
        // is_id
        mov             r9,  #235
        lsl             r9,  r9,  r7
        b               2f
1:
        // no clip
        mov             r8,  #0
        mov             r9,  r6                // bitdepth_max
2:
        vmov.16         d30[3], r6             // bitdepth_max
        vdup.16         d31, r8                // clip_min

        mov             r10, #GRAIN_WIDTH*2    // grain_lut stride

.if \sy
        mov             r6,  #23
        mov             r7,  #22
.else
        mov             r6,  #27
        mov             r7,  #17
.endif
        vmov.16         d31[1], r9             // clip_max

        ldrd            r8,  r9,  [sp, #116]   // offsets, h

        add             r5,  r5,  #(2*(3 + (2 >> \sx)*3)) // grain_lut += 9 or 6
.if \sy
        add             r5,  r5,  r10, lsl #2  // grain_lut += 4 * grain_stride
        add             r5,  r5,  r10, lsl #1  // grain_lut += 2 * grain_stride
.else
        add             r5,  r5,  r10, lsl #3  // grain_lut += 8 * grain_stride
        add             r5,  r5,  r10          // grain_lut += grain_stride
.endif
        vmov.16         d31[2], r6             // overlap y [0]

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

        vmov.16         d31[3], r7             // overlap y [1]

        add             r4,  r4,  #2*(32 >> \sx)      // grain_lut += BLOCK_SIZE * bx
        add             r8,  lr,  r10, lsl #(5 - \sy) // grain_lut += grain_stride * BLOCK_SIZE * by
        add             r11, r11, r10, lsl #(5 - \sy) // grain_lut += grain_stride * BLOCK_SIZE * by
        add             r11, r11, #2*(32 >> \sx)      // grain_lut += BLOCK_SIZE * bx

        movrel_local    r12, overlap_coeffs_\sx
        ldr             lr,       [sp, #132]   // type
        ldrd            r6,  r7,  [sp, #108]   // luma_row, luma_stride

        vld1.16         {d24, d25}, [r12, :128] // overlap_coeffs

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
        add             r7,  r7,  r7           // luma_stride *= 2
.endif
        sub             r7,  r7,  #32          // luma_stride -= 32

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
        sub             r2,  r2,  #32          // src_stride   -= 32
        sub             r10, r10, #32          // grain_stride -= 32
.if \oy
        mov             r12, lr
.endif
L(fguv_loop_sx0_csfl\csfl\()_\ox\oy\()_loopstart):
1:
.if \ox
        vld1.16         {d0},       [r4],       r10 // grain_lut old
.endif
.if \oy
        vld1.16         {q2,  q3},  [r8]!           // grain_lut top
.endif
.if \ox && \oy
        vld1.16         {d2},       [r11],      r10 // grain_lut top old
.endif
.if !\ox && !\oy
        vld1.16         {q0,  q1},  [r6, :128]!     // luma
.endif
        vld1.16         {q8,  q9},  [r5]!           // grain_lut
.if \oy
        vld1.16         {q4,  q5},  [r8],       r10 // grain_lut top
.endif
.if !\ox && !\oy
        vld1.16         {q2,  q3},  [r6, :128], r7  // luma
.endif
.if \oy
        vdup.16         d28, d31[2]                 // overlap y coeff
        vdup.16         d29, d31[3]                 // overlap y coeff
.endif
        vld1.16         {q10, q11}, [r5],       r10 // grain_lut

.if \ox
        vdup.16         q7,  d30[3]                // bitdepth_max
        add             r4,  r4,  #32
        vmull.s16       q0,  d0,  d24
        vshr.u16        q7,  q7,  #1               // grain_max
        vmlal.s16       q0,  d16, d25
        vmvn            q6,  q7                    // grain_min
.endif

.if \oy
.if \ox
        add             r11, r11, #32
        vmull.s16       q1,  d2,  d24
        vmlal.s16       q1,  d4,  d25
        vqrshrn.s32     d16, q0,  #5
        vqrshrn.s32     d4,  q1,  #5
        vmin.s16        d4,  d4,  d14
        vmin.s16        d16, d16, d14
        vmax.s16        d4,  d4,  d12
        vmax.s16        d16, d16, d12
.endif

        vmull.s16       q0,  d4,  d28
        vmull.s16       q1,  d5,  d28
        vmull.s16       q2,  d6,  d28
        vmull.s16       q3,  d7,  d28
.if !\ox
        vdup.16         q7,  d30[3]                // bitdepth_max
.endif
        vmlal.s16       q0,  d16, d29
        vmlal.s16       q1,  d17, d29
        vmlal.s16       q2,  d18, d29
        vmlal.s16       q3,  d19, d29
.if !\ox
        vshr.u16        q7,  q7,  #1               // grain_max
.endif
        vmull.s16       q8,  d20, d29
        vmull.s16       q9,  d21, d29
        vmull.s16       q10, d22, d29
        vmull.s16       q11, d23, d29
.if !\ox
        vmvn            q6,  q7                    // grain_min
.endif
        vmlal.s16       q8,  d8,  d28
        vmlal.s16       q9,  d9,  d28
        vmlal.s16       q10, d10, d28
        vmlal.s16       q11, d11, d28
        vqrshrn.s32     d0,  q0,  #5
        vqrshrn.s32     d1,  q1,  #5
        vqrshrn.s32     d2,  q2,  #5
        vqrshrn.s32     d3,  q3,  #5
        vqrshrn.s32     d4,  q8,  #5
        vqrshrn.s32     d5,  q9,  #5
        vqrshrn.s32     d6,  q10, #5
        vqrshrn.s32     d7,  q11, #5
        vmin.s16        q8,  q0,  q7
        vmin.s16        q9,  q1,  q7
        vld1.16         {q0,  q1},  [r6, :128]!    // luma
        vmin.s16        q10, q2,  q7
        vmin.s16        q11, q3,  q7
        vmax.s16        q8,  q8,  q6
        vmax.s16        q9,  q9,  q6
        vld1.16         {q2,  q3},  [r6, :128], r7 // luma
        vmax.s16        q10, q10, q6
        vmax.s16        q11, q11, q6
.elseif \ox
        vqrshrn.s32     d16, q0,  #5
        vld1.16         {q0,  q1},  [r6, :128]!    // luma
        vmin.s16        d16, d16, d14
        vld1.16         {q2,  q3},  [r6, :128], r7 // luma
        vmax.s16        d16, d16, d12
.endif

.if !\csfl
        vdup.16         d28, d30[0]   // uv_luma_mult
        vld1.16         {q4,  q5},  [r1, :128]! // src
        vdup.16         d29, d30[1]   // uv_mult
        vmull.s16       q6,  d0,  d28
        vmull.s16       q7,  d1,  d28
        vmull.s16       q0,  d2,  d28
        vmull.s16       q1,  d3,  d28
        vmlal.s16       q6,  d8,  d29
        vmlal.s16       q7,  d9,  d29
        vmlal.s16       q0,  d10, d29
        vmlal.s16       q1,  d11, d29
        vld1.16         {q4,  q5},  [r1, :128]  // src
        sub             r1,  r1,  #32
        vshrn.s32       d12, q6,  #6
        vshrn.s32       d13, q7,  #6
        vshrn.s32       d14, q0,  #6
        vshrn.s32       d15, q1,  #6
        vmull.s16       q0,  d4,  d28
        vmull.s16       q1,  d5,  d28
        vmull.s16       q2,  d6,  d28
        vmull.s16       q3,  d7,  d28
        vmlal.s16       q0,  d8,  d29
        vmlal.s16       q1,  d9,  d29
        vmlal.s16       q2,  d10, d29
        vmlal.s16       q3,  d11, d29
        vdup.16         q14, d30[2]   // uv_offset
        vshrn.s32       d0,  q0,  #6
        vshrn.s32       d1,  q1,  #6
        vshrn.s32       d2,  q2,  #6
        vshrn.s32       d3,  q3,  #6
        vdup.16         q4,  d30[3]   // bitdepth_max
        vmov.i16        q5,  #0
        vadd.i16        q6,  q6,  q14
        vadd.i16        q7,  q7,  q14
        vadd.i16        q2,  q0,  q14
        vadd.i16        q3,  q1,  q14
        vmin.s16        q0,  q6,  q4
        vmin.s16        q1,  q7,  q4
        vmin.s16        q2,  q2,  q4
        vmin.s16        q3,  q3,  q4
        vmax.s16        q0,  q0,  q5
        vmax.s16        q1,  q1,  q5
        vmax.s16        q2,  q2,  q5
        vmax.s16        q3,  q3,  q5
.else
        vdup.16         q14, d30[3]  // bitdepth_max
        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        vand            q0,  q0,  q14
        vand            q1,  q1,  q14
        vand            q2,  q2,  q14
        vand            q3,  q3,  q14
.endif

        bl              gather32_neon

        vld1.16         {q0,  q1},  [r1, :128]!    // src

        vmovl.u8        q6,  d8        // scaling
        vmovl.u8        q7,  d9
        vmovl.u8        q4,  d10
        vmovl.u8        q5,  d11

        vld1.16         {q2,  q3},  [r1, :128], r2 // src

        vshl.u16        q6,  q6,  q13  // scaling << (15 - scaling_shift)
        vshl.u16        q7,  q7,  q13
        vshl.u16        q4,  q4,  q13
        vshl.u16        q5,  q5,  q13

        vqrdmulh.s16    q8,  q8,  q6   // round2((scaling << (15 - scaling_shift) * grain, 15)
        vqrdmulh.s16    q9,  q9,  q7
        vqrdmulh.s16    q10, q10, q4
        vqrdmulh.s16    q11, q11, q5


        vdup.16         q4,  d31[0]    // clip_min
        vdup.16         q5,  d31[1]    // clip_max

        vqadd.s16       q0,  q0,  q8   // *src + noise
        vqadd.s16       q1,  q1,  q9
        vqadd.s16       q2,  q2,  q10
        vqadd.s16       q3,  q3,  q11

.if \oy
        vmov.32         lr,  d25[0] // 2 first 16 bit coeffs from overlap x
.endif

        vmax.s16        q0,  q0,  q4
        vmax.s16        q1,  q1,  q4
        vmax.s16        q2,  q2,  q4
        vmax.s16        q3,  q3,  q4
        vmin.s16        q0,  q0,  q5
        vmin.s16        q1,  q1,  q5
        vmin.s16        q2,  q2,  q5
        vmin.s16        q3,  q3,  q5

        vst1.16         {q0, q1}, [r0, :128]! // dst

        subs            r9,  r9,  #1
.if \oy
        vmov.32         d31[1], lr  // new coeffs for overlap y
.endif

        vst1.16         {q2, q3}, [r0, :128], r2 // dst
        bgt             1b

.if \oy
        cmp             r12, #0
        mov             r9,  r12               // restore actual remaining h
        bgt             L(fguv_loop_sx0_csfl\csfl\()_\ox\()0_loopstart)
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
        vld1.16         {d0},       [r4],       r10 // grain_lut old
.endif
.if \ox && \oy
        vld1.16         {d2},       [r11],      r10 // grain_lut top old
.endif
.if \oy
        vld1.16         {q2,  q3},  [r8],       r10 // grain_lut top
.endif
.if !\ox && !\oy
        vld1.16         {q0,  q1},  [r6, :128]!     // luma
.endif
        vld1.16         {q8,  q9},  [r5],       r10 // grain_lut
.if \oy
        vdup.16         d28, d31[2]                 // overlap y coeff
        vdup.16         d29, d31[3]                 // overlap y coeff
.endif
.if !\ox && !\oy
        vld1.16         {q2,  q3},  [r6, :128], r7  // luma
.endif

.if \ox
        vdup.16         q7,  d30[3]                // bitdepth_max
        vmull.s16       q0,  d0,  d24
        vshr.u16        q7,  q7,  #1               // grain_max
        vmlal.s16       q0,  d16, d25
        vmvn            q6,  q7                    // grain_min
.endif

.if \oy
.if \ox
        vmull.s16       q1,  d2,  d24
        vmlal.s16       q1,  d4,  d25
        vqrshrn.s32     d16, q0,  #5
        vqrshrn.s32     d4,  q1,  #5
        vmin.s16        d4,  d4,  d14
        vmin.s16        d16, d16, d14
        vmax.s16        d4,  d4,  d12
        vmax.s16        d16, d16, d12
.endif

        vmull.s16       q0,  d4,  d28
        vmull.s16       q1,  d5,  d28
        vmull.s16       q2,  d6,  d28
        vmull.s16       q3,  d7,  d28
.if !\ox
        vdup.16         q7,  d30[3]                // bitdepth_max
.endif
        vmlal.s16       q0,  d16, d29
        vmlal.s16       q1,  d17, d29
        vmlal.s16       q2,  d18, d29
        vmlal.s16       q3,  d19, d29
.if !\ox
        vshr.u16        q7,  q7,  #1               // grain_max
.endif
        vqrshrn.s32     d16, q0,  #5
        vqrshrn.s32     d17, q1,  #5
        vqrshrn.s32     d18, q2,  #5
        vqrshrn.s32     d19, q3,  #5
.if !\ox
        vmvn            q6,  q7                    // grain_min
.endif
        vld1.16         {q0,  q1},  [r6, :128]!    // luma
        vmin.s16        q8,  q8,  q7
        vmin.s16        q9,  q9,  q7
        vmax.s16        q8,  q8,  q6
        vmax.s16        q9,  q9,  q6
        vld1.16         {q2,  q3},  [r6, :128], r7 // luma
.elseif \ox
        vqrshrn.s32     d16, q0,  #5
        vld1.16         {q0,  q1},  [r6, :128]!    // luma
        vmin.s16        d16, d16, d14
        vld1.16         {q2,  q3},  [r6, :128], r7 // luma
        vmax.s16        d16, d16, d12
.endif

        vpadd.i16       d0,  d0,  d1
        vpadd.i16       d1,  d2,  d3
        vpadd.i16       d2,  d4,  d5
        vpadd.i16       d3,  d6,  d7
        vrshr.u16       q0,  q0,  #1
        vrshr.u16       q1,  q1,  #1
.if !\csfl
        vdup.16         d28, d30[0]   // uv_luma_mult
        vld1.16         {q2,  q3},  [r1, :128], r2 // src
        vdup.16         d29, d30[1]   // uv_mult
        vmull.s16       q6,  d0,  d28
        vmull.s16       q7,  d1,  d28
        vmull.s16       q0,  d2,  d28
        vmull.s16       q1,  d3,  d28
        vmlal.s16       q6,  d4,  d29
        vmlal.s16       q7,  d5,  d29
        vmlal.s16       q0,  d6,  d29
        vmlal.s16       q1,  d7,  d29
        vshrn.s32       d12, q6,  #6
        vshrn.s32       d13, q7,  #6
        vshrn.s32       d14, q0,  #6
        vshrn.s32       d15, q1,  #6
        vdup.16         q14, d30[2]   // uv_offset
        vdup.16         q4,  d30[3]   // bitdepth_max
        vmov.i16        q5,  #0
        vadd.i16        q6,  q6,  q14
        vadd.i16        q7,  q7,  q14
        vmin.s16        q0,  q6,  q4
        vmin.s16        q1,  q7,  q4
        vmax.s16        q0,  q0,  q5
        vmax.s16        q1,  q1,  q5
.else
        vdup.16         q14, d30[3]  // bitdepth_max
        vld1.16         {q2,  q3},  [r1, :128], r2 // src

        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        vand            q0,  q0,  q14
        vand            q1,  q1,  q14
.endif

        bl              gather16_neon

        vmovl.u8        q6,  d8        // scaling
        vmovl.u8        q7,  d9

        vshl.u16        q6,  q6,  q13  // scaling << (15 - scaling_shift)
        vshl.u16        q7,  q7,  q13

        vqrdmulh.s16    q8,  q8,  q6   // round2((scaling << (15 - scaling_shift) * grain, 15)
        vqrdmulh.s16    q9,  q9,  q7


        vdup.16         q4,  d31[0]    // clip_min
        vdup.16         q5,  d31[1]    // clip_max

        vqadd.s16       q0,  q2,  q8   // *src + noise
        vqadd.s16       q1,  q3,  q9

.if \oy
        // Swap the two last coefficients of d31, place them first in d28
        vrev64.16       d28, d31
.endif

        vmax.s16        q0,  q0,  q4
        vmax.s16        q1,  q1,  q4
        vmin.s16        q0,  q0,  q5
        vmin.s16        q1,  q1,  q5

        subs            r9,  r9,  #1
.if \oy
        // Take the first two 16 bit coefficients of d28 and place them at the
        // end of d31
        vtrn.32         d31, d28
.endif

        vst1.16         {q0, q1}, [r0, :128], r2 // dst
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
