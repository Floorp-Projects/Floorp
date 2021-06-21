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
        umov            w14, \src1[0]
        umov            w15, \src2[1]
        umov            w16, \src1[2]
        add             x14, x14, x3
        umov            w17, \src2[3]
        add             x15, x15, x3
        ld1             {\dst1}[0+\off], [x14]
        umov            w14, \src1[4]
        add             x16, x16, x3
        ld1             {\dst2}[1+\off], [x15]
        umov            w15, \src2[5]
        add             x17, x17, x3
        ld1             {\dst1}[2+\off], [x16]
        umov            w16, \src1[6]
        add             x14, x14, x3
        ld1             {\dst2}[3+\off], [x17]
        umov            w17, \src2[7]
        add             x15, x15, x3
        ld1             {\dst1}[4+\off], [x14]
        add             x16, x16, x3
        ld1             {\dst2}[5+\off], [x15]
        add             x17, x17, x3
        ld1             {\dst1}[6+\off], [x16]
        ld1             {\dst2}[7+\off], [x17]
.endm

.macro gather dst1, dst2, src1, src2, src3, src4
        gather_interleaved \dst1, \dst2, \src1, \src3, 0
        gather_interleaved \dst2, \dst1, \src3, \src1, 0
        gather_interleaved \dst1, \dst2, \src2, \src4, 8
        gather_interleaved \dst2, \dst1, \src4, \src2, 8
.endm

function gather32_neon
        gather          v6.b, v7.b, v0.h, v1.h, v2.h, v3.h
        ret
endfunc

function gather16_neon
        gather_interleaved v6.b, v7.b, v0.h, v1.h, 0
        gather_interleaved v7.b, v6.b, v1.h, v0.h, 0
        ins             v6.d[1], v7.d[0]
        ret
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
        madd            \dst, \stride, \offy, \src // grain_lut += grain_stride * offy
        add             \dst, \dst, \offx, uxtw #1 // grain_lut += offx
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
        str             x30, [sp, #-80]!
        stp             d8,  d9,  [sp, #16]
        stp             d10, d11, [sp, #32]
        stp             d12, d13, [sp, #48]
        str             d14,      [sp, #64]
        eor             w4,  w4,  #15          // 15 - scaling_shift
        ldr             w11, [x6, #8]          // offsets[1][0]
        ldr             w13, [x6, #4]          // offsets[0][1]
        ldr             w15, [x6, #12]         // offsets[1][1]
        ldr             w10, [sp, #96]         // bitdepth_max
        ldr             w6,  [x6]              // offsets[0][0]
        dup             v26.8h,  w10           // bitdepth_max
        clz             w10, w10
        ldr             w8,  [sp, #80]         // clip
        sub             w10, w10, #24          // -bitdepth_min_8
        mov             x9,  #GRAIN_WIDTH*2    // grain_lut stride
        neg             w10, w10               // bitdepth_min_8

        dup             v29.8h,  w4            // 15 - scaling_shift
        dup             v27.8h,  w10           // bitdepth_min_8

        movrel          x16, overlap_coeffs_0

        cbz             w8,  1f
        // clip
        movi            v30.8h,  #16
        movi            v31.8h,  #235
        sshl            v30.8h,  v30.8h,  v27.8h
        sshl            v31.8h,  v31.8h,  v27.8h
        b               2f
1:
        // no clip
        movi            v30.8h,  #0
        mov             v31.16b, v26.16b       // bitdepth_max
2:

        ushr            v26.8h,  v26.8h,  #1   // grain_max
        not             v25.16b, v26.16b       // grain_min

        ld1             {v27.4h, v28.4h}, [x16] // overlap_coeffs

        add             x5,  x5,  #18          // grain_lut += 9
        add             x5,  x5,  x9,  lsl #3  // grain_lut += 8 * grain_stride
        add             x5,  x5,  x9           // grain_lut += grain_stride

        calc_offset     w11, w12, w11, 0,  0
        calc_offset     w13, w14, w13, 0,  0
        calc_offset     w15, w16, w15, 0,  0
        calc_offset     w6,  w10, w6,  0,  0

        add_offset      x12, w11, x12, x5,  x9
        add_offset      x14, w13, x14, x5,  x9
        add_offset      x16, w15, x16, x5,  x9
        add_offset      x5,  w6,  x10, x5,  x9

        ldr             w11, [sp, #88]         // type
        adr             x13, L(fgy_loop_tbl)

        add             x4,  x12, #32*2        // grain_lut += BLOCK_SIZE * bx
        add             x6,  x14, x9,  lsl #5  // grain_lut += grain_stride * BLOCK_SIZE * by

        tst             w11, #1
        ldrh            w11, [x13, w11, uxtw #1]

        add             x8,  x16, x9,  lsl #5  // grain_lut += grain_stride * BLOCK_SIZE * by
        add             x8,  x8,  #32*2        // grain_lut += BLOCK_SIZE * bx

        sub             x11, x13, w11, uxtw

        b.eq            1f
        // y overlap
        dup             v8.8h,   v27.h[0]
        dup             v9.8h,   v27.h[1]
        mov             w10, w7                // backup actual h
        mov             w7,  #2
1:
        br              x11
endfunc

function fgy_loop_neon
.macro fgy ox, oy
L(loop_\ox\oy):
1:
        ld1             {v0.8h,  v1.8h,  v2.8h,  v3.8h},  [x1],  x2 // src
.if \ox
        ld1             {v20.4h},                         [x4],  x9 // grain_lut old
.endif
.if \oy
        ld1             {v21.8h, v22.8h, v23.8h, v24.8h}, [x6],  x9 // grain_lut top
.endif
.if \ox && \oy
        ld1             {v14.4h},                         [x8],  x9 // grain_lut top old
.endif
        mvni            v4.8h,   #0xf0, lsl #8 // 0x0fff
        ld1             {v16.8h, v17.8h, v18.8h, v19.8h}, [x5],  x9 // grain_lut

        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        and             v0.16b,  v0.16b,  v4.16b
        and             v1.16b,  v1.16b,  v4.16b
        and             v2.16b,  v2.16b,  v4.16b
        and             v3.16b,  v3.16b,  v4.16b
        bl              gather32_neon

.if \ox
        smull           v20.4s,  v20.4h,  v27.4h
        smlal           v20.4s,  v16.4h,  v28.4h
.endif

.if \oy
.if \ox
        smull           v14.4s,  v14.4h,  v27.4h
        smlal           v14.4s,  v21.4h,  v28.4h
        sqrshrn         v20.4h,  v20.4s,  #5
        sqrshrn         v14.4h,  v14.4s,  #5
        smin            v20.4h,  v20.4h,  v26.4h
        smin            v14.4h,  v14.4h,  v26.4h
        smax            v20.4h,  v20.4h,  v25.4h
        smax            v14.4h,  v14.4h,  v25.4h
.endif

.if \ox
        smull           v10.4s,  v20.4h,  v9.4h
.else
        smull           v10.4s,  v16.4h,  v9.4h
.endif
        smull2          v11.4s,  v16.8h,  v9.8h
        smull           v12.4s,  v17.4h,  v9.4h
        smull2          v13.4s,  v17.8h,  v9.8h
        smull           v16.4s,  v18.4h,  v9.4h
        smull2          v17.4s,  v18.8h,  v9.8h
        smull           v18.4s,  v19.4h,  v9.4h
        smull2          v19.4s,  v19.8h,  v9.8h
.if \ox
        smlal           v10.4s,  v14.4h,  v8.4h
.else
        smlal           v10.4s,  v21.4h,  v8.4h
.endif
        smlal2          v11.4s,  v21.8h,  v8.8h
        smlal           v12.4s,  v22.4h,  v8.4h
        smlal2          v13.4s,  v22.8h,  v8.8h
        smlal           v16.4s,  v23.4h,  v8.4h
        smlal2          v17.4s,  v23.8h,  v8.8h
        smlal           v18.4s,  v24.4h,  v8.4h
        smlal2          v19.4s,  v24.8h,  v8.8h
        sqrshrn         v10.4h,  v10.4s,  #5
        sqrshrn2        v10.8h,  v11.4s,  #5
        sqrshrn         v11.4h,  v12.4s,  #5
        sqrshrn2        v11.8h,  v13.4s,  #5
        sqrshrn         v12.4h,  v16.4s,  #5
        sqrshrn2        v12.8h,  v17.4s,  #5
        sqrshrn         v13.4h,  v18.4s,  #5
        sqrshrn2        v13.8h,  v19.4s,  #5
        smin            v16.8h,  v10.8h,  v26.8h
        smin            v17.8h,  v11.8h,  v26.8h
        smin            v18.8h,  v12.8h,  v26.8h
        smin            v19.8h,  v13.8h,  v26.8h
        smax            v16.8h,  v16.8h,  v25.8h
        smax            v17.8h,  v17.8h,  v25.8h
        smax            v18.8h,  v18.8h,  v25.8h
        smax            v19.8h,  v19.8h,  v25.8h
.endif

        uxtl            v4.8h,   v6.8b            // scaling
.if \ox && !\oy
        sqrshrn         v20.4h,  v20.4s,  #5
.endif
        uxtl2           v5.8h,   v6.16b
.if \ox && !\oy
        smin            v20.4h,  v20.4h,  v26.4h
.endif
        uxtl            v6.8h,   v7.8b
.if \ox && !\oy
        smax            v20.4h,  v20.4h,  v25.4h
.endif
        uxtl2           v7.8h,   v7.16b
.if \ox && !\oy
        ins             v16.d[0], v20.d[0]
.endif
        ushl            v4.8h,   v4.8h,   v29.8h  // scaling << (15 - scaling_shift)
        ushl            v5.8h,   v5.8h,   v29.8h
        ushl            v6.8h,   v6.8h,   v29.8h
        ushl            v7.8h,   v7.8h,   v29.8h

        sqrdmulh        v20.8h,  v16.8h,  v4.8h   // round2((scaling << (15 - scaling_shift) * grain, 15)
        sqrdmulh        v21.8h,  v17.8h,  v5.8h
        sqrdmulh        v22.8h,  v18.8h,  v6.8h
        sqrdmulh        v23.8h,  v19.8h,  v7.8h

        usqadd          v0.8h,   v20.8h           // *src + noise
        usqadd          v1.8h,   v21.8h
        usqadd          v2.8h,   v22.8h
        usqadd          v3.8h,   v23.8h

        umax            v0.8h,   v0.8h,   v30.8h
        umax            v1.8h,   v1.8h,   v30.8h
        umax            v2.8h,   v2.8h,   v30.8h
        umax            v3.8h,   v3.8h,   v30.8h
        umin            v0.8h,   v0.8h,   v31.8h
        umin            v1.8h,   v1.8h,   v31.8h
        umin            v2.8h,   v2.8h,   v31.8h
        umin            v3.8h,   v3.8h,   v31.8h

        subs            w7,  w7,  #1
.if \oy
        dup             v8.8h,   v28.h[0]
        dup             v9.8h,   v28.h[1]
.endif
        st1             {v0.8h, v1.8h, v2.8h, v3.8h},  [x0], x2 // dst
        b.gt            1b

.if \oy
        cmp             w10, #2
        sub             w7,  w10, #2           // restore actual remaining h
        b.gt            L(loop_\ox\()0)
.endif
        ldr             d14,      [sp, #64]
        ldp             d12, d13, [sp, #48]
        ldp             d10, d11, [sp, #32]
        ldp             d8,  d9,  [sp, #16]
        ldr             x30, [sp], #80
        ret
.endm

        fgy             0, 0
        fgy             0, 1
        fgy             1, 0
        fgy             1, 1

L(fgy_loop_tbl):
        .hword L(fgy_loop_tbl) - L(loop_00)
        .hword L(fgy_loop_tbl) - L(loop_01)
        .hword L(fgy_loop_tbl) - L(loop_10)
        .hword L(fgy_loop_tbl) - L(loop_11)
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
        str             x30,      [sp, #-80]!
        stp             d8,  d9,  [sp, #16]
        stp             d10, d11, [sp, #32]
        stp             d12, d13, [sp, #48]
        stp             d14, d15, [sp, #64]

        ldp             x8,  x9,  [sp, #80]    // offsets, h
        ldp             x10, x11, [sp, #96]    // uv, is_id
        ldr             w16,      [sp, #120]   // bitdepth_max

        ldr             w13, [x4, #FGD_SCALING_SHIFT]
        ldr             w12, [x4, #FGD_CLIP_TO_RESTRICTED_RANGE]
        dup             v23.8h,  w16           // bitdepth_max
        clz             w16, w16
        eor             w13, w13, #15          // 15 - scaling_shift
        sub             w16, w16, #24          // -bitdepth_min_8

        // !csfl
        add             x10, x4,  x10, lsl #2  // + 4*uv
        add             x14, x10, #FGD_UV_LUMA_MULT
        add             x15, x10, #FGD_UV_MULT
        add             x10, x10, #FGD_UV_OFFSET
        neg             w16, w16               // bitdepth_min_8
        ld1r            {v8.8h},  [x14]        // uv_luma_mult
        ld1r            {v24.8h}, [x10]        // uv_offset
        ld1r            {v9.8h},  [x15]        // uv_mult

        dup             v29.8h,  w13           // 15 - scaling_shift
        dup             v27.8h,  w16           // bitdepth_min_8

        cbz             w12, 1f
        // clip
        movi            v30.8h,  #16
        movi            v31.8h,  #240
        sshl            v30.8h,  v30.8h,  v27.8h
        sshl            v31.8h,  v31.8h,  v27.8h
        cbz             w11, 2f
        // is_id
        movi            v31.8h,  #235
        sshl            v31.8h,  v31.8h,  v27.8h
        b               2f
1:
        // no clip
        movi            v30.8h,  #0
        mov             v31.16b, v23.16b       // bitdepth_max
2:

        ushr            v15.8h,  v23.8h,  #1   // grain_max
        sshl            v24.8h,  v24.8h,  v27.8h // uv_offset << bitdepth_min_8
        not             v14.16b, v15.16b       // grain_min

        ldr             w12, [x8, #8]          // offsets[1][0]
        ldr             w14, [x8, #4]          // offsets[0][1]
        ldr             w16, [x8, #12]         // offsets[1][1]
        ldr             w8,  [x8]              // offsets[0][0]

        mov             x10, #GRAIN_WIDTH*2    // grain_lut stride

        add             x5,  x5,  #(2*(3 + (2 >> \sx)*3)) // grain_lut += 9 or 6
.if \sy
        add             x5,  x5,  x10, lsl #2  // grain_lut += 4 * grain_stride
        add             x5,  x5,  x10, lsl #1  // grain_lut += 2 * grain_stride
.else
        add             x5,  x5,  x10, lsl #3  // grain_lut += 8 * grain_stride
        add             x5,  x5,  x10          // grain_lut += grain_stride
.endif

        calc_offset     w12, w13, w12, \sx, \sy
        calc_offset     w14, w15, w14, \sx, \sy
        calc_offset     w16, w17, w16, \sx, \sy
        calc_offset     w8,  w11, w8,  \sx, \sy

        add_offset      x13, w12, x13, x5,  x10
        add_offset      x15, w14, x15, x5,  x10
        add_offset      x17, w16, x17, x5,  x10
        add_offset      x5,  w8,  x11, x5,  x10

        add             x4,  x13, #2*(32 >> \sx)      // grain_lut += BLOCK_SIZE * bx
        add             x8,  x15, x10, lsl #(5 - \sy) // grain_lut += grain_stride * BLOCK_SIZE * by
        add             x11, x17, x10, lsl #(5 - \sy) // grain_lut += grain_stride * BLOCK_SIZE * by
        add             x11, x11, #2*(32 >> \sx)      // grain_lut += BLOCK_SIZE * bx

        ldr             w13, [sp, #112]        // type

        movrel          x16, overlap_coeffs_\sx
        adr             x14, L(fguv_loop_sx\sx\()_tbl)

        ld1             {v27.4h, v28.4h}, [x16] // overlap_coeffs
        tst             w13, #1
        ldrh            w13, [x14, w13, uxtw #1]

        b.eq            1f
        // y overlap
        sub             w12, w9,  #(2 >> \sy)  // backup remaining h
        mov             w9,  #(2 >> \sy)

1:
        sub             x13, x14, w13, uxtw

.if \sy
        movi            v25.8h,  #23
        movi            v26.8h,  #22
.else
        movi            v25.8h,  #27
        movi            v26.8h,  #17
.endif

.if \sy
        add             x7,  x7,  x7           // luma_stride *= 2
.endif

        br              x13
endfunc
.endm

fguv 420, 1, 1
fguv 422, 1, 0
fguv 444, 0, 0

function fguv_loop_sx0_neon
.macro fguv_loop_sx0 csfl, ox, oy
L(fguv_loop_sx0_csfl\csfl\()_\ox\oy):
1:
.if \ox
        ld1             {v4.4h}, [x4],  x10  // grain_lut old
.endif
.if \oy
        ld1             {v0.8h,  v1.8h,  v2.8h,  v3.8h},  [x8],  x10 // grain_lut top
.endif
.if \ox && \oy
        ld1             {v5.4h}, [x11], x10  // grain_lut top old
.endif
        ld1             {v16.8h, v17.8h, v18.8h, v19.8h}, [x5],  x10 // grain_lut

.if \ox
        smull           v4.4s,   v4.4h,   v27.4h
        smlal           v4.4s,   v16.4h,  v28.4h
.endif

.if \oy
.if \ox
        smull           v5.4s,   v5.4h,   v27.4h
        smlal           v5.4s,   v0.4h,   v28.4h
        sqrshrn         v4.4h,   v4.4s,   #5
        sqrshrn         v5.4h,   v5.4s,   #5
        smin            v4.4h,   v4.4h,   v15.4h
        smin            v5.4h,   v5.4h,   v15.4h
        smax            v4.4h,   v4.4h,   v14.4h
        smax            v5.4h,   v5.4h,   v14.4h
        ins             v16.d[0], v4.d[0]
        ins             v0.d[0],  v5.d[0]
.endif

        smull           v6.4s,   v16.4h,  v26.4h
        smull2          v7.4s,   v16.8h,  v26.8h
        smull           v10.4s,  v17.4h,  v26.4h
        smull2          v11.4s,  v17.8h,  v26.8h
        smull           v16.4s,  v18.4h,  v26.4h
        smull2          v17.4s,  v18.8h,  v26.8h
        smull           v18.4s,  v19.4h,  v26.4h
        smull2          v19.4s,  v19.8h,  v26.8h
        smlal           v6.4s,   v0.4h,   v25.4h
        smlal2          v7.4s,   v0.8h,   v25.8h
        smlal           v10.4s,  v1.4h,   v25.4h
        smlal2          v11.4s,  v1.8h,   v25.8h
        smlal           v16.4s,  v2.4h,   v25.4h
        smlal2          v17.4s,  v2.8h,   v25.8h
        smlal           v18.4s,  v3.4h,   v25.4h
        smlal2          v19.4s,  v3.8h,   v25.8h
        sqrshrn         v6.4h,   v6.4s,   #5
        sqrshrn2        v6.8h,   v7.4s,   #5
        sqrshrn         v7.4h,   v10.4s,  #5
        sqrshrn2        v7.8h,   v11.4s,  #5
        sqrshrn         v10.4h,  v16.4s,  #5
        sqrshrn2        v10.8h,  v17.4s,  #5
        sqrshrn         v11.4h,  v18.4s,  #5
        sqrshrn2        v11.8h,  v19.4s,  #5
.endif

.if \ox && !\oy
        sqrshrn         v4.4h,   v4.4s,   #5
        smin            v4.4h,   v4.4h,   v15.4h
.endif
        ld1             {v0.8h,  v1.8h,  v2.8h,  v3.8h},  [x6],  x7 // luma
.if \oy
        smin            v16.8h,  v6.8h,   v15.8h
        smin            v17.8h,  v7.8h,   v15.8h
        smin            v18.8h,  v10.8h,  v15.8h
        smin            v19.8h,  v11.8h,  v15.8h
        smax            v16.8h,  v16.8h,  v14.8h
        smax            v17.8h,  v17.8h,  v14.8h
        smax            v18.8h,  v18.8h,  v14.8h
        smax            v19.8h,  v19.8h,  v14.8h
.endif

.if \ox && !\oy
        smax            v4.4h,   v4.4h,   v14.4h
.endif
        ld1             {v10.8h, v11.8h, v12.8h, v13.8h}, [x1],  x2 // src
.if \ox && !\oy
        ins             v16.d[0], v4.d[0]
.endif

.if !\csfl
        smull           v4.4s,   v0.4h,   v8.4h
        smull2          v5.4s,   v0.8h,   v8.8h
        smull           v6.4s,   v1.4h,   v8.4h
        smull2          v7.4s,   v1.8h,   v8.8h
        smull           v0.4s,   v2.4h,   v8.4h
        smull2          v1.4s,   v2.8h,   v8.8h
        smull           v2.4s,   v3.4h,   v8.4h
        smull2          v3.4s,   v3.8h,   v8.8h
        smlal           v4.4s,   v10.4h,  v9.4h
        smlal2          v5.4s,   v10.8h,  v9.8h
        smlal           v6.4s,   v11.4h,  v9.4h
        smlal2          v7.4s,   v11.8h,  v9.8h
        smlal           v0.4s,   v12.4h,  v9.4h
        smlal2          v1.4s,   v12.8h,  v9.8h
        smlal           v2.4s,   v13.4h,  v9.4h
        smlal2          v3.4s,   v13.8h,  v9.8h
        shrn            v4.4h,   v4.4s,   #6
        shrn2           v4.8h,   v5.4s,   #6
        shrn            v5.4h,   v6.4s,   #6
        shrn2           v5.8h,   v7.4s,   #6
        shrn            v6.4h,   v0.4s,   #6
        shrn2           v6.8h,   v1.4s,   #6
        shrn            v7.4h,   v2.4s,   #6
        shrn2           v7.8h,   v3.4s,   #6
        add             v0.8h,   v4.8h,   v24.8h
        add             v1.8h,   v5.8h,   v24.8h
        add             v2.8h,   v6.8h,   v24.8h
        add             v3.8h,   v7.8h,   v24.8h
        movi            v20.8h,  #0
        smin            v0.8h,   v0.8h,   v23.8h
        smin            v1.8h,   v1.8h,   v23.8h
        smin            v2.8h,   v2.8h,   v23.8h
        smin            v3.8h,   v3.8h,   v23.8h
        smax            v0.8h,   v0.8h,   v20.8h
        smax            v1.8h,   v1.8h,   v20.8h
        smax            v2.8h,   v2.8h,   v20.8h
        smax            v3.8h,   v3.8h,   v20.8h
.else
        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        and             v0.16b,  v0.16b,  v23.16b
        and             v1.16b,  v1.16b,  v23.16b
        and             v2.16b,  v2.16b,  v23.16b
        and             v3.16b,  v3.16b,  v23.16b
.endif

        bl              gather32_neon

        uxtl            v4.8h,   v6.8b            // scaling
        uxtl2           v5.8h,   v6.16b
        uxtl            v6.8h,   v7.8b
        uxtl2           v7.8h,   v7.16b

        ushl            v4.8h,   v4.8h,   v29.8h  // scaling << (15 - scaling_shift)
        ushl            v5.8h,   v5.8h,   v29.8h
        ushl            v6.8h,   v6.8h,   v29.8h
        ushl            v7.8h,   v7.8h,   v29.8h

        sqrdmulh        v16.8h,  v16.8h,  v4.8h   // round2((scaling << (15 - scaling_shift) * grain, 15)
        sqrdmulh        v17.8h,  v17.8h,  v5.8h
        sqrdmulh        v18.8h,  v18.8h,  v6.8h
        sqrdmulh        v19.8h,  v19.8h,  v7.8h

        usqadd          v10.8h,  v16.8h           // *src + noise
        usqadd          v11.8h,  v17.8h
        usqadd          v12.8h,  v18.8h
        usqadd          v13.8h,  v19.8h

        umax            v0.8h,   v10.8h,  v30.8h
        umax            v1.8h,   v11.8h,  v30.8h
        umax            v2.8h,   v12.8h,  v30.8h
        umax            v3.8h,   v13.8h,  v30.8h
        umin            v0.8h,   v0.8h,   v31.8h
        umin            v1.8h,   v1.8h,   v31.8h
        umin            v2.8h,   v2.8h,   v31.8h
        umin            v3.8h,   v3.8h,   v31.8h

        subs            w9,  w9,  #1
.if \oy
        dup             v25.8h,  v28.h[0]
        dup             v26.8h,  v28.h[1]
.endif
        st1             {v0.8h, v1.8h, v2.8h, v3.8h},  [x0], x2 // dst
        b.gt            1b

.if \oy
        cmp             w12, #0
        mov             w9,  w12               // restore actual remaining h
        b.gt            L(fguv_loop_sx0_csfl\csfl\()_\ox\()0)
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
        ldp             d14, d15, [sp, #64]
        ldp             d12, d13, [sp, #48]
        ldp             d10, d11, [sp, #32]
        ldp             d8,  d9,  [sp, #16]
        ldr             x30,      [sp], #80
        ret

L(fguv_loop_sx0_tbl):
        .hword L(fguv_loop_sx0_tbl) - L(fguv_loop_sx0_csfl0_00)
        .hword L(fguv_loop_sx0_tbl) - L(fguv_loop_sx0_csfl0_01)
        .hword L(fguv_loop_sx0_tbl) - L(fguv_loop_sx0_csfl0_10)
        .hword L(fguv_loop_sx0_tbl) - L(fguv_loop_sx0_csfl0_11)
        .hword L(fguv_loop_sx0_tbl) - L(fguv_loop_sx0_csfl1_00)
        .hword L(fguv_loop_sx0_tbl) - L(fguv_loop_sx0_csfl1_01)
        .hword L(fguv_loop_sx0_tbl) - L(fguv_loop_sx0_csfl1_10)
        .hword L(fguv_loop_sx0_tbl) - L(fguv_loop_sx0_csfl1_11)
endfunc

function fguv_loop_sx1_neon
.macro fguv_loop_sx1 csfl, ox, oy
L(fguv_loop_sx1_csfl\csfl\()_\ox\oy):
1:
.if \ox
        ld1             {v18.4h}, [x4],  x10  // grain_lut old
.endif
.if \oy
        ld1             {v20.8h, v21.8h},  [x8],  x10 // grain_lut top
.endif
.if \ox && \oy
        ld1             {v19.4h}, [x11], x10  // grain_lut top old
.endif
        ld1             {v16.8h, v17.8h}, [x5],  x10 // grain_lut

.if \ox
        smull           v18.4s,  v18.4h,  v27.4h
        smlal           v18.4s,  v16.4h,  v28.4h
.endif

.if \oy
.if \ox
        smull           v19.4s,  v19.4h,  v27.4h
        smlal           v19.4s,  v20.4h,  v28.4h
        sqrshrn         v18.4h,  v18.4s,  #5
        sqrshrn         v19.4h,  v19.4s,  #5
        smin            v18.4h,  v18.4h,  v15.4h
        smin            v19.4h,  v19.4h,  v15.4h
        smax            v18.4h,  v18.4h,  v14.4h
        smax            v19.4h,  v19.4h,  v14.4h
        ins             v16.d[0], v18.d[0]
        ins             v20.d[0], v19.d[0]
.endif

        smull           v0.4s,   v16.4h,  v26.4h
        smull2          v1.4s,   v16.8h,  v26.8h
        smull           v2.4s,   v17.4h,  v26.4h
        smull2          v3.4s,   v17.8h,  v26.8h
        smlal           v0.4s,   v20.4h,  v25.4h
        smlal2          v1.4s,   v20.8h,  v25.8h
        smlal           v2.4s,   v21.4h,  v25.4h
        smlal2          v3.4s,   v21.8h,  v25.8h
        sqrshrn         v16.4h,  v0.4s,   #5
        sqrshrn2        v16.8h,  v1.4s,   #5
        sqrshrn         v17.4h,  v2.4s,   #5
        sqrshrn2        v17.8h,  v3.4s,   #5
.endif

.if \ox && !\oy
        sqrshrn         v18.4h,  v18.4s,  #5
        smin            v18.4h,  v18.4h,  v15.4h
.endif
        ld1             {v0.8h,  v1.8h,  v2.8h,  v3.8h},  [x6],  x7 // luma
.if \oy
        smin            v16.8h,  v16.8h,  v15.8h
        smin            v17.8h,  v17.8h,  v15.8h
        smax            v16.8h,  v16.8h,  v14.8h
        smax            v17.8h,  v17.8h,  v14.8h
.endif

.if \ox && !\oy
        smax            v18.4h,  v18.4h,  v14.4h
.endif
        ld1             {v10.8h, v11.8h},  [x1],  x2 // src
.if \ox && !\oy
        ins             v16.d[0], v18.d[0]
.endif
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v1.8h,   v2.8h,   v3.8h
        urshr           v0.8h,   v0.8h,   #1
        urshr           v1.8h,   v1.8h,   #1
.if !\csfl
        smull           v2.4s,   v0.4h,   v8.4h
        smull2          v3.4s,   v0.8h,   v8.8h
        smull           v0.4s,   v1.4h,   v8.4h
        smull2          v1.4s,   v1.8h,   v8.8h
        smlal           v2.4s,   v10.4h,  v9.4h
        smlal2          v3.4s,   v10.8h,  v9.8h
        smlal           v0.4s,   v11.4h,  v9.4h
        smlal2          v1.4s,   v11.8h,  v9.8h
        shrn            v2.4h,   v2.4s,   #6
        shrn2           v2.8h,   v3.4s,   #6
        shrn            v3.4h,   v0.4s,   #6
        shrn2           v3.8h,   v1.4s,   #6
        add             v0.8h,   v2.8h,   v24.8h
        add             v1.8h,   v3.8h,   v24.8h
        movi            v2.8h,   #0
        smin            v0.8h,   v0.8h,   v23.8h
        smin            v1.8h,   v1.8h,   v23.8h
        smax            v0.8h,   v0.8h,   v2.8h
        smax            v1.8h,   v1.8h,   v2.8h
.else
        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        and             v0.16b,  v0.16b,  v23.16b
        and             v1.16b,  v1.16b,  v23.16b
.endif

        bl              gather16_neon

        uxtl            v4.8h,   v6.8b            // scaling
        uxtl2           v5.8h,   v6.16b

        ushl            v4.8h,   v4.8h,   v29.8h  // scaling << (15 - scaling_shift)
        ushl            v5.8h,   v5.8h,   v29.8h

        sqrdmulh        v16.8h,  v16.8h,  v4.8h   // round2((scaling << (15 - scaling_shift) * grain, 15)
        sqrdmulh        v17.8h,  v17.8h,  v5.8h

        usqadd          v10.8h,  v16.8h           // *src + noise
        usqadd          v11.8h,  v17.8h

        umax            v0.8h,   v10.8h,  v30.8h
        umax            v1.8h,   v11.8h,  v30.8h
        umin            v0.8h,   v0.8h,   v31.8h
        umin            v1.8h,   v1.8h,   v31.8h

.if \oy
        mov             v16.16b, v25.16b
.endif
        subs            w9,  w9,  #1
.if \oy
        mov             v25.16b, v26.16b
        mov             v26.16b, v16.16b
.endif
        st1             {v0.8h, v1.8h},  [x0], x2 // dst
        b.gt            1b

.if \oy
        cmp             w12, #0
        mov             w9,  w12               // restore actual remaining h
        b.gt            L(fguv_loop_sx1_csfl\csfl\()_\ox\()0)
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
        ldp             d14, d15, [sp, #64]
        ldp             d12, d13, [sp, #48]
        ldp             d10, d11, [sp, #32]
        ldp             d8,  d9,  [sp, #16]
        ldr             x30,      [sp], #80
        ret

L(fguv_loop_sx1_tbl):
        .hword L(fguv_loop_sx1_tbl) - L(fguv_loop_sx1_csfl0_00)
        .hword L(fguv_loop_sx1_tbl) - L(fguv_loop_sx1_csfl0_01)
        .hword L(fguv_loop_sx1_tbl) - L(fguv_loop_sx1_csfl0_10)
        .hword L(fguv_loop_sx1_tbl) - L(fguv_loop_sx1_csfl0_11)
        .hword L(fguv_loop_sx1_tbl) - L(fguv_loop_sx1_csfl1_00)
        .hword L(fguv_loop_sx1_tbl) - L(fguv_loop_sx1_csfl1_01)
        .hword L(fguv_loop_sx1_tbl) - L(fguv_loop_sx1_csfl1_10)
        .hword L(fguv_loop_sx1_tbl) - L(fguv_loop_sx1_csfl1_11)
endfunc
