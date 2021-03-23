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

#define GRAIN_WIDTH 82

.macro gather_interleaved dst1, dst2, src1, src2, off
        umov            w14, \src1[0+\off]
        umov            w15, \src2[1+\off]
        umov            w16, \src1[2+\off]
        add             x14, x14, x3
        umov            w17, \src2[3+\off]
        add             x15, x15, x3
        ld1             {\dst1}[0+\off], [x14]
        umov            w14, \src1[4+\off]
        add             x16, x16, x3
        ld1             {\dst2}[1+\off], [x15]
        umov            w15, \src2[5+\off]
        add             x17, x17, x3
        ld1             {\dst1}[2+\off], [x16]
        umov            w16, \src1[6+\off]
        add             x14, x14, x3
        ld1             {\dst2}[3+\off], [x17]
        umov            w17, \src2[7+\off]
        add             x15, x15, x3
        ld1             {\dst1}[4+\off], [x14]
        add             x16, x16, x3
        ld1             {\dst2}[5+\off], [x15]
        add             x17, x17, x3
        ld1             {\dst1}[6+\off], [x16]
        ld1             {\dst2}[7+\off], [x17]
.endm

.macro gather dst1, dst2, src1, src2
        gather_interleaved \dst1, \dst2, \src1, \src2, 0
        gather_interleaved \dst2, \dst1, \src2, \src1, 0
        gather_interleaved \dst1, \dst2, \src1, \src2, 8
        gather_interleaved \dst2, \dst1, \src2, \src1, 8
.endm

function gather_neon
        gather          v4.b, v5.b, v0.b, v1.b
        ret
endfunc

const overlap_coeffs, align=4
        .byte 27, 17, 0,  0,  0,  0,  0,  0
        .byte 17, 27, 32, 32, 32, 32, 32, 32
endconst

.macro calc_offset offx, offy, src
        and             \offy, \src,  #0xF     // randval & 0xF
        lsr             \offx, \src,  #4       // randval >> 4
        add             \offy, \offy, \offy    // 2 * (randval & 0xF)
        add             \offx, \offx, \offx    // 2 * (randval >> 4)
.endm

.macro add_offset dst, offx, offy, src
        madd            \dst, x9,  \offy, \src // grain_lut += grain_stride * offy
        add             \dst, \dst, \offx, uxtw // grain_lut += offx
.endm

// void dav1d_fgy_32x32_XY_8bpc_neon(pixel *const dst, const pixel *const src,
//                                   const ptrdiff_t stride,
//                                   const uint8_t scaling[SCALING_SIZE],
//                                   const int scaling_shift,
//                                   const entry grain_lut[][GRAIN_WIDTH],
//                                   const int offsets[][2],
//                                   const int h, const ptrdiff_t clip);
.macro fgy ox, oy
function fgy_32x32_\ox\oy\()_8bpc_neon, export=1
        str             x30, [sp, #-16]!
.if \ox
        ldr             w11, [x6, #8]          // offsets[1][0]
.endif
.if \oy
        ldr             w13, [x6, #4]          // offsets[0][1]
.endif
.if \ox && \oy
        ldr             w15, [x6, #12]         // offsets[1][1]
.endif
        ldr             w6,  [x6]              // offsets[0][0]
        ldr             w8,  [sp, #16]         // clip
        mov             x9,  #GRAIN_WIDTH      // grain_lut stride

        neg             w4,  w4
        dup             v29.8h, w4             // -scaling_shift

.if \ox || \oy
        movrel          x16, overlap_coeffs
.endif

        cbz             w8,  1f
        // clip
        movi            v30.16b, #16
        movi            v31.16b, #235
        b               2f
1:
        // no clip
        movi            v30.16b, #0
        movi            v31.16b, #255
2:

.if \ox || \oy
        ld1             {v27.8b, v28.8b}, [x16] // overlap_coeffs
.endif

        add             x5,  x5,  #9           // grain_lut += 9
        add             x5,  x5,  x9,  lsl #3  // grain_lut += 8 * grain_stride
        add             x5,  x5,  x9           // grain_lut += grain_stride

.if \ox
        calc_offset     w11, w12, w11
.endif
.if \oy
        calc_offset     w13, w14, w13
.endif
.if \ox && \oy
        calc_offset     w15, w16, w15
.endif
        calc_offset     w6,  w10, w6

.if \ox
        add_offset      x12, w11, x12, x5
.endif
.if \oy
        add_offset      x14, w13, x14, x5
.endif
.if \ox && \oy
        add_offset      x16, w15, x16, x5
.endif
        add_offset      x5,  w6,  x10, x5

.if \ox
        add             x4,  x12, #32          // grain_lut += BLOCK_SIZE * bx
.endif
.if \oy
        add             x6,  x14, x9,  lsl #5  // grain_lut += grain_stride * BLOCK_SIZE * by
.endif
.if \ox && \oy
        add             x8,  x16, x9,  lsl #5  // grain_lut += grain_stride * BLOCK_SIZE * by
        add             x8,  x8,  #32          // grain_lut += BLOCK_SIZE * bx
.endif

.if \oy
        dup             v6.16b,  v27.b[0]
        dup             v7.16b,  v27.b[1]
        mov             w10, w7                // backup actual h
        mov             w7,  #2
.endif

L(loop_\ox\oy):
1:
        ld1             {v0.16b,  v1.16b},  [x1],  x2 // src
.if \ox
        ld1             {v20.8b},           [x4],  x9 // grain_lut old
.endif
.if \oy
        ld1             {v22.16b, v23.16b}, [x6],  x9 // grain_lut top
.endif
.if \ox && \oy
        ld1             {v21.8b},           [x8],  x9 // grain_lut top old
.endif
        ld1             {v18.16b, v19.16b}, [x5],  x9 // grain_lut

        bl              gather_neon

.if \ox
        smull           v20.8h,  v20.8b,  v27.8b
        smlal           v20.8h,  v18.8b,  v28.8b
.endif

.if \oy
.if \ox
        smull           v21.8h,  v21.8b,  v27.8b
        smlal           v21.8h,  v22.8b,  v28.8b
        sqrshrn         v20.8b,  v20.8h,  #5
        sqrshrn         v21.8b,  v21.8h,  #5
.endif

.if \ox
        smull           v16.8h,  v20.8b,  v7.8b
.else
        smull           v16.8h,  v18.8b,  v7.8b
.endif
        smull2          v17.8h,  v18.16b, v7.16b
        smull           v18.8h,  v19.8b,  v7.8b
        smull2          v19.8h,  v19.16b, v7.16b
.if \ox
        smlal           v16.8h,  v21.8b,  v6.8b
.else
        smlal           v16.8h,  v22.8b,  v6.8b
.endif
        smlal2          v17.8h,  v22.16b, v6.16b
        smlal           v18.8h,  v23.8b,  v6.8b
        smlal2          v19.8h,  v23.16b, v6.16b
        sqrshrn         v22.8b,  v16.8h,  #5
        sqrshrn2        v22.16b, v17.8h,  #5
        sqrshrn         v23.8b,  v18.8h,  #5
        sqrshrn2        v23.16b, v19.8h,  #5
.endif

        // sxtl of grain
.if \oy
        sxtl            v16.8h,  v22.8b
        sxtl2           v17.8h,  v22.16b
        sxtl            v18.8h,  v23.8b
        sxtl2           v19.8h,  v23.16b
.elseif \ox
        sqrshrn         v20.8b,  v20.8h,  #5
        sxtl2           v17.8h,  v18.16b
        sxtl            v18.8h,  v19.8b
        sxtl2           v19.8h,  v19.16b
        sxtl            v16.8h,  v20.8b
.else
        sxtl            v16.8h,  v18.8b
        sxtl2           v17.8h,  v18.16b
        sxtl            v18.8h,  v19.8b
        sxtl2           v19.8h,  v19.16b
.endif

        uxtl            v2.8h,   v4.8b   // scaling
        uxtl2           v3.8h,   v4.16b
        uxtl            v4.8h,   v5.8b
        uxtl2           v5.8h,   v5.16b

        mul             v16.8h,  v16.8h,  v2.8h   // scaling * grain
        mul             v17.8h,  v17.8h,  v3.8h
        mul             v18.8h,  v18.8h,  v4.8h
        mul             v19.8h,  v19.8h,  v5.8h

        srshl           v16.8h,  v16.8h,  v29.8h  // round2(scaling * grain, scaling_shift)
        srshl           v17.8h,  v17.8h,  v29.8h
        srshl           v18.8h,  v18.8h,  v29.8h
        srshl           v19.8h,  v19.8h,  v29.8h

        uaddw           v16.8h,  v16.8h,  v0.8b   // *src + noise
        uaddw2          v17.8h,  v17.8h,  v0.16b
        uaddw           v18.8h,  v18.8h,  v1.8b
        uaddw2          v19.8h,  v19.8h,  v1.16b

        sqxtun          v0.8b,   v16.8h
        sqxtun2         v0.16b,  v17.8h
        sqxtun          v1.8b,   v18.8h
        sqxtun2         v1.16b,  v19.8h

        umax            v0.16b,  v0.16b,  v30.16b
        umax            v1.16b,  v1.16b,  v30.16b
        umin            v0.16b,  v0.16b,  v31.16b
        umin            v1.16b,  v1.16b,  v31.16b

        subs            w7,  w7,  #1
.if \oy
        dup             v6.16b,  v28.b[0]
        dup             v7.16b,  v28.b[1]
.endif
        st1             {v0.16b,  v1.16b},  [x0], x2 // dst
        b.gt            1b

.if \oy
        cmp             w10, #2
        sub             w7,  w10, #2           // restore actual remaining h
        b.gt            L(loop_\ox\()0)
.endif
        ldr             x30, [sp], #16
        ret
endfunc
.endm

fgy 0, 0
fgy 0, 1
fgy 1, 0
fgy 1, 1
