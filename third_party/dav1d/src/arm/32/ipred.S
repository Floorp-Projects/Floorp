/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2019, Martin Storsjo
 * Copyright © 2019, B Krishnan Iyer
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

// void ipred_dc_128_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                             const pixel *const topleft,
//                             const int width, const int height, const int a,
//                             const int max_width, const int max_height);
function ipred_dc_128_8bpc_neon, export=1
        push            {r4, lr}
        ldr             r4,  [sp, #8]
        clz             r3,  r3
        adr             r2,  L(ipred_dc_128_tbl)
        sub             r3,  r3,  #25
        ldr             r3,  [r2,  r3,  lsl #2]
        mov             lr,  #128
        vdup.8          q0,  lr
        add             r2,  r2,  r3
        add             r12, r0,  r1
        lsl             r1,  r1,  #1
        bx              r2

        .align 2
L(ipred_dc_128_tbl):
        .word 640f - L(ipred_dc_128_tbl) + CONFIG_THUMB
        .word 320f - L(ipred_dc_128_tbl) + CONFIG_THUMB
        .word 16f  - L(ipred_dc_128_tbl) + CONFIG_THUMB
        .word 8f   - L(ipred_dc_128_tbl) + CONFIG_THUMB
        .word 4f   - L(ipred_dc_128_tbl) + CONFIG_THUMB
4:
        vst1.32         {d0[0]},  [r0,  :32], r1
        vst1.32         {d0[0]},  [r12, :32], r1
        subs            r4,  r4,  #4
        vst1.32         {d0[0]},  [r0,  :32], r1
        vst1.32         {d0[0]},  [r12, :32], r1
        bgt             4b
        pop             {r4, pc}
8:
        vst1.8          {d0},  [r0,  :64], r1
        vst1.8          {d0},  [r12, :64], r1
        subs            r4,  r4,  #4
        vst1.8          {d0},  [r0,  :64], r1
        vst1.8          {d0},  [r12, :64], r1
        bgt             8b
        pop             {r4, pc}
16:
        vst1.8          {d0,  d1}, [r0,  :128], r1
        vst1.8          {d0,  d1}, [r12, :128], r1
        subs            r4,  r4,  #4
        vst1.8          {d0,  d1}, [r0,  :128], r1
        vst1.8          {d0,  d1}, [r12, :128], r1
        bgt             16b
        pop             {r4, pc}
320:
        vdup.8          q1,  lr
32:
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128], r1
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128], r1
        subs            r4,  r4,  #4
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128], r1
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128], r1
        bgt             32b
        pop             {r4, pc}
640:
        vdup.8          q1,  lr
        vdup.8          q2,  lr
        vdup.8          q3,  lr
        sub             r1,  r1,  #32
64:
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128]!
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128]!
        vst1.8          {d4,  d5,  d6,  d7},  [r0,  :128], r1
        vst1.8          {d4,  d5,  d6,  d7},  [r12, :128], r1
        subs            r4,  r4,  #4
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128]!
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128]!
        vst1.8          {d4,  d5,  d6,  d7},  [r0,  :128], r1
        vst1.8          {d4,  d5,  d6,  d7},  [r12, :128], r1
        bgt             64b
        pop             {r4, pc}
endfunc

// void ipred_v_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                        const pixel *const topleft,
//                        const int width, const int height, const int a,
//                        const int max_width, const int max_height);
function ipred_v_8bpc_neon, export=1
        push            {r4, lr}
        ldr             lr,  [sp, #8]
        clz             r3,  r3
        adr             r4,  L(ipred_v_tbl)
        sub             r3,  r3,  #25
        ldr             r3,  [r4,  r3,  lsl #2]
        add             r2,  r2,  #1
        add             r4,  r4,  r3
        add             r12, r0,  r1
        lsl             r1,  r1,  #1
        bx              r4

        .align 2
L(ipred_v_tbl):
        .word 640f - L(ipred_v_tbl) + CONFIG_THUMB
        .word 320f - L(ipred_v_tbl) + CONFIG_THUMB
        .word 160f - L(ipred_v_tbl) + CONFIG_THUMB
        .word 80f  - L(ipred_v_tbl) + CONFIG_THUMB
        .word 40f  - L(ipred_v_tbl) + CONFIG_THUMB
40:
        vld1.32         {d0[0]},  [r2]
4:
        vst1.32         {d0[0]},  [r0,  :32], r1
        vst1.32         {d0[0]},  [r12, :32], r1
        subs            lr,  lr,  #4
        vst1.32         {d0[0]},  [r0,  :32], r1
        vst1.32         {d0[0]},  [r12, :32], r1
        bgt             4b
        pop             {r4, pc}
80:
        vld1.8          {d0}, [r2]
8:
        vst1.8          {d0},  [r0,  :64], r1
        vst1.8          {d0},  [r12, :64], r1
        subs            lr,  lr,  #4
        vst1.8          {d0},  [r0,  :64], r1
        vst1.8          {d0},  [r12, :64], r1
        bgt             8b
        pop             {r4, pc}
160:
        vld1.8          {q0},  [r2]
16:
        vst1.8          {d0,  d1},  [r0,  :128], r1
        vst1.8          {d0,  d1},  [r12, :128], r1
        subs            lr,  lr,  #4
        vst1.8          {d0,  d1},  [r0,  :128], r1
        vst1.8          {d0,  d1},  [r12, :128], r1
        bgt             16b
        pop             {r4, pc}
320:
        vld1.8          {q0,  q1},  [r2]
32:
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128], r1
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128], r1
        subs            lr,  lr,  #4
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128], r1
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128], r1
        bgt             32b
        pop             {r4, pc}
640:
        vld1.8          {q0,  q1},  [r2]!
        sub             r1,  r1,  #32
        vld1.8          {q2,  q3},  [r2]
64:
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128]!
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128]!
        vst1.8          {d4,  d5,  d6,  d7},  [r0,  :128], r1
        vst1.8          {d4,  d5,  d6,  d7},  [r12, :128], r1
        subs            lr,  lr,  #4
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128]!
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128]!
        vst1.8          {d4,  d5,  d6,  d7},  [r0,  :128], r1
        vst1.8          {d4,  d5,  d6,  d7},  [r12, :128], r1
        bgt             64b
        pop             {r4, pc}
endfunc

// void ipred_h_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                        const pixel *const topleft,
//                        const int width, const int height, const int a,
//                        const int max_width, const int max_height);
function ipred_h_8bpc_neon, export=1
        push            {r4-r5, lr}
        ldr             r4,  [sp, #12]
        clz             r3,  r3
        adr             r5,  L(ipred_h_tbl)
        sub             r3,  r3,  #25
        ldr             r3,  [r5,  r3,  lsl #2]
        sub             r2,  r2,  #4
        mov             lr,  #-4
        add             r5,  r5,  r3
        add             r12, r0,  r1
        lsl             r1,  r1,  #1
        bx              r5

        .align 2
L(ipred_h_tbl):
        .word 640f - L(ipred_h_tbl) + CONFIG_THUMB
        .word 320f - L(ipred_h_tbl) + CONFIG_THUMB
        .word 160f - L(ipred_h_tbl) + CONFIG_THUMB
        .word 8f   - L(ipred_h_tbl) + CONFIG_THUMB
        .word 4f   - L(ipred_h_tbl) + CONFIG_THUMB
4:
        vld4.8          {d0[],  d1[],  d2[],  d3[]},  [r2],  lr
        vst1.32         {d3[0]},  [r0,  :32], r1
        vst1.32         {d2[0]},  [r12, :32], r1
        subs            r4,  r4,  #4
        vst1.32         {d1[0]},  [r0,  :32], r1
        vst1.32         {d0[0]},  [r12, :32], r1
        bgt             4b
        pop             {r4-r5, pc}
8:
        vld4.8          {d0[],  d1[],  d2[],  d3[]},  [r2],  lr
        vst1.8          {d3},  [r0,  :64], r1
        vst1.8          {d2},  [r12, :64], r1
        subs            r4,  r4,  #4
        vst1.8          {d1},  [r0,  :64], r1
        vst1.8          {d0},  [r12, :64], r1
        bgt             8b
        pop             {r4-r5, pc}
160:
        add             r2,  r2,  #3
        mov             lr,  #-1
16:
        vld1.8          {d0[],  d1[]},  [r2],  lr
        subs            r4,  r4,  #4
        vld1.8          {d2[],  d3[]},  [r2],  lr
        vst1.8          {q0},  [r0,    :128],  r1
        vld1.8          {d4[],  d5[]},  [r2],  lr
        vst1.8          {q1},  [r12,   :128],  r1
        vld1.8          {d6[],  d7[]},  [r2],  lr
        vst1.8          {q2},  [r0,    :128],  r1
        vst1.8          {q3},  [r12,   :128],  r1
        bgt             16b
        pop             {r4-r5, pc}
320:
        add             r2,  r2,  #3
        mov             lr,  #-1
        sub             r1,  r1,  #16
32:
        vld1.8          {d0[],  d1[]}, [r2],  lr
        subs            r4,  r4,  #4
        vld1.8          {d2[],  d3[]}, [r2],  lr
        vst1.8          {q0},  [r0,   :128]!
        vld1.8          {d4[],  d5[]}, [r2],  lr
        vst1.8          {q1},  [r12,  :128]!
        vld1.8          {d6[],  d7[]}, [r2],  lr
        vst1.8          {q0},  [r0,   :128],  r1
        vst1.8          {q1},  [r12,  :128],  r1
        vst1.8          {q2},  [r0,   :128]!
        vst1.8          {q3},  [r12,  :128]!
        vst1.8          {q2},  [r0,   :128],  r1
        vst1.8          {q3},  [r12,  :128],  r1
        bgt             32b
        pop             {r4-r5, pc}
640:
        add             r2,  r2,  #3
        mov             lr,  #-1
        sub             r1,  r1,  #48
64:
        vld1.8          {d0[],  d1[]},  [r2],  lr
        subs            r4,  r4,  #4
        vld1.8          {d2[],  d3[]},  [r2],  lr
        vst1.8          {q0},  [r0,    :128]!
        vld1.8          {d4[],  d5[]},  [r2],  lr
        vst1.8          {q1},  [r12,   :128]!
        vld1.8          {d6[],  d7[]},  [r2],  lr
        vst1.8          {q0},  [r0,    :128]!
        vst1.8          {q1},  [r12,   :128]!
        vst1.8          {q0},  [r0,    :128]!
        vst1.8          {q1},  [r12,   :128]!
        vst1.8          {q0},  [r0,    :128],  r1
        vst1.8          {q1},  [r12,   :128],  r1
        vst1.8          {q2},  [r0,    :128]!
        vst1.8          {q3},  [r12,   :128]!
        vst1.8          {q2},  [r0,    :128]!
        vst1.8          {q3},  [r12,   :128]!
        vst1.8          {q2},  [r0,    :128]!
        vst1.8          {q3},  [r12,   :128]!
        vst1.8          {q2},  [r0,    :128],  r1
        vst1.8          {q3},  [r12,   :128],  r1
        bgt             64b
        pop             {r4-r5, pc}
endfunc

// void ipred_dc_top_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                             const pixel *const topleft,
//                             const int width, const int height, const int a,
//                             const int max_width, const int max_height);
function ipred_dc_top_8bpc_neon, export=1
        push            {r4-r5, lr}
        ldr             r4,  [sp, #12]
        clz             r3,  r3
        adr             r5,  L(ipred_dc_top_tbl)
        sub             r3,  r3,  #25
        ldr             r3,  [r5,  r3,  lsl #2]
        add             r2,  r2,  #1
        add             r5,  r5,  r3
        add             r12, r0,  r1
        lsl             r1,  r1,  #1
        bx              r5

        .align 2
L(ipred_dc_top_tbl):
        .word 640f - L(ipred_dc_top_tbl) + CONFIG_THUMB
        .word 320f - L(ipred_dc_top_tbl) + CONFIG_THUMB
        .word 160f - L(ipred_dc_top_tbl) + CONFIG_THUMB
        .word 80f  - L(ipred_dc_top_tbl) + CONFIG_THUMB
        .word 40f  - L(ipred_dc_top_tbl) + CONFIG_THUMB
40:
        vld1.32         {d0[]},  [r2]
        vpaddl.u8       d0,  d0
        vpadd.u16       d0,  d0
        vrshrn.u16      d0,  q0,  #2
        vdup.8          d0,  d0[0]
4:
        vst1.32         {d0[0]},  [r0,  :32], r1
        vst1.32         {d0[0]},  [r12, :32], r1
        subs            r4,  r4,  #4
        vst1.32         {d0[0]},  [r0,  :32], r1
        vst1.32         {d0[0]},  [r12, :32], r1
        bgt             4b
        pop             {r4-r5, pc}
80:
        vld1.8          {d0},  [r2]
        vpaddl.u8       d0,  d0
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        vrshrn.u16      d0,  q0,  #3
        vdup.8          d0,  d0[0]
8:
        vst1.8          {d0},  [r0,  :64], r1
        vst1.8          {d0},  [r12, :64], r1
        subs            r4,  r4,  #4
        vst1.8          {d0},  [r0,  :64], r1
        vst1.8          {d0},  [r12, :64], r1
        bgt             8b
        pop             {r4-r5, pc}
160:
        vld1.8          {d0,  d1},  [r2]
        vaddl.u8        q0,  d0,  d1
        vadd.u16        d0,  d0,  d1
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        vrshrn.u16      d0,  q0,  #4
        vdup.8          q0,  d0[0]
16:
        vst1.8          {d0,  d1},  [r0,  :128], r1
        vst1.8          {d0,  d1},  [r12, :128], r1
        subs            r4,  r4,  #4
        vst1.8          {d0,  d1},  [r0,  :128], r1
        vst1.8          {d0,  d1},  [r12, :128], r1
        bgt             16b
        pop             {r4-r5, pc}
320:
        vld1.8          {d0,  d1,  d2,  d3},  [r2]
        vaddl.u8        q0,  d0,  d1
        vaddl.u8        q1,  d2,  d3
        vadd.u16        q0,  q0,  q1
        vadd.u16        d0,  d0,  d1
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        vrshrn.u16      d4,  q0,  #5
        vdup.8          q0,  d4[0]
        vdup.8          q1,  d4[0]
32:
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128], r1
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128], r1
        subs            r4,  r4,  #4
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128], r1
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128], r1
        bgt             32b
        pop             {r4-r5, pc}
640:
        vld1.8          {d0,  d1,  d2,  d3},  [r2]!
        vaddl.u8        q0,  d0,  d1
        vld1.8          {d4,  d5,  d6,  d7},  [r2]
        vaddl.u8        q1,  d2,  d3
        vaddl.u8        q2,  d4,  d5
        vaddl.u8        q3,  d6,  d7
        vadd.u16        q0,  q0,  q1
        vadd.u16        q1,  q2,  q3
        vadd.u16        q0,  q0,  q1
        vadd.u16        d0,  d0,  d1
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        vrshrn.u16      d18, q0,  #6
        vdup.8          q0,  d18[0]
        vdup.8          q1,  d18[0]
        vdup.8          q2,  d18[0]
        vdup.8          q3,  d18[0]
        sub             r1,  r1,  #32
64:
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128]!
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128]!
        vst1.8          {d4,  d5,  d6,  d7},  [r0,  :128], r1
        vst1.8          {d4,  d5,  d6,  d7},  [r12, :128], r1
        subs            r4,  r4,  #4
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128]!
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128]!
        vst1.8          {d4,  d5,  d6,  d7},  [r0,  :128], r1
        vst1.8          {d4,  d5,  d6,  d7},  [r12, :128], r1
        bgt             64b
        pop             {r4-r5, pc}
endfunc

// void ipred_dc_left_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                              const pixel *const topleft,
//                              const int width, const int height, const int a,
//                              const int max_width, const int max_height);
function ipred_dc_left_8bpc_neon, export=1
        push            {r4-r5, lr}
        ldr             r4,  [sp, #12]
        sub             r2,  r2,  r4
        clz             r3,  r3
        clz             lr,  r4
        sub             lr,  lr,  #25
        adr             r5,  L(ipred_dc_left_tbl)
        sub             r3,  r3,  #20
        ldr             r3,  [r5,  r3,  lsl #2]
        ldr             lr,  [r5,  lr,  lsl #2]
        add             r3,  r5,  r3
        add             r5,  r5,  lr
        add             r12, r0,  r1
        lsl             r1,  r1,  #1
        bx              r5

        .align 2
L(ipred_dc_left_tbl):
        .word L(ipred_dc_left_h64) - L(ipred_dc_left_tbl) + CONFIG_THUMB
        .word L(ipred_dc_left_h32) - L(ipred_dc_left_tbl) + CONFIG_THUMB
        .word L(ipred_dc_left_h16) - L(ipred_dc_left_tbl) + CONFIG_THUMB
        .word L(ipred_dc_left_h8)  - L(ipred_dc_left_tbl) + CONFIG_THUMB
        .word L(ipred_dc_left_h4)  - L(ipred_dc_left_tbl) + CONFIG_THUMB
        .word L(ipred_dc_left_w64) - L(ipred_dc_left_tbl) + CONFIG_THUMB
        .word L(ipred_dc_left_w32) - L(ipred_dc_left_tbl) + CONFIG_THUMB
        .word L(ipred_dc_left_w16) - L(ipred_dc_left_tbl) + CONFIG_THUMB
        .word L(ipred_dc_left_w8)  - L(ipred_dc_left_tbl) + CONFIG_THUMB
        .word L(ipred_dc_left_w4)  - L(ipred_dc_left_tbl) + CONFIG_THUMB

L(ipred_dc_left_h4):
        vld1.32         {d0[]},  [r2]
        vpaddl.u8       d0,  d0
        vpadd.u16       d0,  d0
        vrshrn.u16      d0,  q0,  #2
        vdup.8          q0,  d0[0]
        bx              r3
L(ipred_dc_left_w4):
        vst1.32         {d0[0]},  [r0,  :32], r1
        vst1.32         {d0[0]},  [r12, :32], r1
        subs            r4,  r4,  #4
        vst1.32         {d0[0]},  [r0,  :32], r1
        vst1.32         {d0[0]},  [r12, :32], r1
        bgt             L(ipred_dc_left_w4)
        pop             {r4-r5, pc}
L(ipred_dc_left_h8):
        vld1.8          {d0},  [r2]
        vpaddl.u8       d0,  d0
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        vrshrn.u16      d0,  q0,  #3
        vdup.8          q0,  d0[0]
        bx              r3
L(ipred_dc_left_w8):
        vst1.8          {d0},  [r0,  :64], r1
        vst1.8          {d0},  [r12, :64], r1
        subs            r4,  r4,  #4
        vst1.8          {d0},  [r0,  :64], r1
        vst1.8          {d0},  [r12, :64], r1
        bgt             L(ipred_dc_left_w8)
        pop             {r4-r5, pc}
L(ipred_dc_left_h16):
        vld1.8          {d0,  d1},  [r2]
        vaddl.u8        q0,  d0,  d1
        vadd.u16        d0,  d0,  d1
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        vrshrn.u16      d0,  q0,  #4
        vdup.8          q0,  d0[0]
        bx              r3
L(ipred_dc_left_w16):
        vst1.8          {d0,  d1},  [r0,  :128], r1
        vst1.8          {d0,  d1},  [r12, :128], r1
        subs            r4,  r4,  #4
        vst1.8          {d0,  d1},  [r0,  :128], r1
        vst1.8          {d0,  d1},  [r12, :128], r1
        bgt             L(ipred_dc_left_w16)
        pop             {r4-r5, pc}
L(ipred_dc_left_h32):
        vld1.8          {d0,  d1,  d2,  d3},  [r2]
        vaddl.u8        q0,  d0,  d1
        vaddl.u8        q1,  d2,  d3
        vadd.u16        q0,  q0,  q1
        vadd.u16        d0,  d0,  d1
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        vrshrn.u16      d0,  q0,  #5
        vdup.8          q0,  d0[0]
        bx              r3
L(ipred_dc_left_w32):
        vmov.8          q1,  q0
1:
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128], r1
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128], r1
        subs            r4,  r4,  #4
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128], r1
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128], r1
        bgt             1b
        pop             {r4-r5, pc}
L(ipred_dc_left_h64):
        vld1.8          {d0,  d1,  d2,  d3},  [r2]!
        vld1.8          {d4,  d5,  d6,  d7},  [r2]
        vaddl.u8        q0,  d0,  d1
        vaddl.u8        q1,  d2,  d3
        vaddl.u8        q2,  d4,  d5
        vaddl.u8        q3,  d6,  d7
        vadd.u16        q0,  q0,  q1
        vadd.u16        q1,  q2,  q3
        vadd.u16        q0,  q0,  q1
        vadd.u16        d0,  d0,  d1
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        vrshrn.u16      d0,  q0,  #6
        vdup.8          q0,  d0[0]
        bx              r3
L(ipred_dc_left_w64):
        sub             r1,  r1,  #32
        vmov.8          q1,  q0
        vmov.8          q2,  q0
        vmov.8          q3,  q0
1:
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128]!
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128]!
        vst1.8          {d4,  d5,  d6,  d7},  [r0,  :128], r1
        vst1.8          {d4,  d5,  d6,  d7},  [r12, :128], r1
        subs            r4,  r4, #4
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128]!
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128]!
        vst1.8          {d4,  d5,  d6,  d7},  [r0,  :128], r1
        vst1.8          {d4,  d5,  d6,  d7},  [r12, :128], r1
        bgt             1b
        pop             {r4-r5, pc}
endfunc

// void ipred_dc_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                         const pixel *const topleft,
//                         const int width, const int height, const int a,
//                         const int max_width, const int max_height);
function ipred_dc_8bpc_neon, export=1
        push            {r4-r6, lr}
        ldr             r4,  [sp, #16]
        sub             r2,  r2,  r4
        add             lr,  r3,  r4        // width + height
        clz             r3,  r3
        clz             r12, r4
        vdup.16         q15, lr             // width + height
        mov             r6,  #0
        adr             r5,  L(ipred_dc_tbl)
        rbit            lr,  lr             // rbit(width + height)
        sub             r3,  r3,  #20       // 25 leading bits, minus table offset 5
        sub             r12, r12, #25
        clz             lr,  lr             // ctz(width + height)
        ldr             r3,  [r5,  r3,  lsl #2]
        ldr             r12, [r5,  r12, lsl #2]
        neg             lr,  lr             // -ctz(width + height)
        add             r3,  r5,  r3
        add             r5,  r5,  r12
        vshr.u16        q15, q15, #1        // (width + height) >> 1
        vdup.16         q14, lr             // -ctz(width + height)
        add             r12, r0,  r1
        lsl             r1,  r1,  #1
        bx              r5

        .align 2
L(ipred_dc_tbl):
        .word L(ipred_dc_h64) - L(ipred_dc_tbl) + CONFIG_THUMB
        .word L(ipred_dc_h32) - L(ipred_dc_tbl) + CONFIG_THUMB
        .word L(ipred_dc_h16) - L(ipred_dc_tbl) + CONFIG_THUMB
        .word L(ipred_dc_h8)  - L(ipred_dc_tbl) + CONFIG_THUMB
        .word L(ipred_dc_h4)  - L(ipred_dc_tbl) + CONFIG_THUMB
        .word L(ipred_dc_w64) - L(ipred_dc_tbl) + CONFIG_THUMB
        .word L(ipred_dc_w32) - L(ipred_dc_tbl) + CONFIG_THUMB
        .word L(ipred_dc_w16) - L(ipred_dc_tbl) + CONFIG_THUMB
        .word L(ipred_dc_w8)  - L(ipred_dc_tbl) + CONFIG_THUMB
        .word L(ipred_dc_w4)  - L(ipred_dc_tbl) + CONFIG_THUMB

L(ipred_dc_h4):
        vld1.32         {d0[0]},  [r2]!
        vpaddl.u8       d0,  d0
        vpadd.u16       d0,  d0
        bx              r3
L(ipred_dc_w4):
        add             r2,  r2,  #1
        vld1.32         {d1[0]},  [r2]
        vmov.32         d1[1],  r6
        vadd.s16        d0,  d0,  d30
        vpaddl.u8       d1,  d1
        vpadd.u16       d1,  d1
        vpadd.u16       d1,  d1
        cmp             r4,  #4
        vadd.s16        d0,  d0,  d1
        vshl.u16        d0,  d0,  d28
        beq             1f                  // h = 8/16
        movw            lr,  #(0x3334/2)
        movw            r5,  #(0x5556/2)
        cmp             r4,  #16
        it              ne
        movne           lr,  r5
        vdup.16         d30, lr
        vqdmulh.s16     d0,  d0,  d30
1:
        vdup.8          d0,  d0[0]
2:
        vst1.32         {d0[0]},  [r0,  :32], r1
        vst1.32         {d0[0]},  [r12, :32], r1
        subs            r4,  r4,  #4
        vst1.32         {d0[0]},  [r0,  :32], r1
        vst1.32         {d0[0]},  [r12, :32], r1
        bgt             2b
        pop             {r4-r6, pc}

L(ipred_dc_h8):
        vld1.8          {d0},  [r2]!
        vpaddl.u8       d0,  d0
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        bx              r3
L(ipred_dc_w8):
        add             r2,  r2,  #1
        vld1.8          {d2},  [r2]
        vadd.s16        d0,  d0,  d30
        vpaddl.u8       d2,  d2
        vpadd.u16       d2,  d2
        vpadd.u16       d2,  d2
        cmp             r4,  #8
        vadd.s16        d0,  d0,  d2
        vshl.u16        d0,  d0,  d28
        beq             1f                  // h = 4/16/32
        cmp             r4,  #32
        movw            lr,  #(0x3334/2)
        movw            r5,  #(0x5556/2)
        it              ne
        movne           lr,  r5
        vdup.16         q12, lr
        vqdmulh.s16     d0,  d0,  d24
1:
        vdup.8          d0,  d0[0]
2:
        vst1.8          {d0},  [r0,  :64], r1
        vst1.8          {d0},  [r12, :64], r1
        subs            r4,  r4,  #4
        vst1.8          {d0},  [r0,  :64], r1
        vst1.8          {d0},  [r12, :64], r1
        bgt             2b
        pop             {r4-r6, pc}

L(ipred_dc_h16):
        vld1.8          {d0,  d1},  [r2]!
        vaddl.u8        q0,  d0,  d1
        vadd.u16        d0,  d0,  d1
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        bx              r3
L(ipred_dc_w16):
        add             r2,  r2,  #1
        vld1.8          {d2,  d3},  [r2]
        vadd.s16        d0,  d0,  d30
        vaddl.u8        q1,  d2,  d3
        vadd.u16        d2,  d2,  d3
        vpadd.u16       d2,  d2
        vpadd.u16       d2,  d2
        cmp             r4,  #16
        vadd.s16        d0,  d0,  d2
        vshl.u16        d0,  d0,  d28
        beq             1f                  // h = 4/8/32/64
        tst             r4,  #(32+16+8)     // 16 added to make a consecutive bitmask
        movw            lr,  #(0x3334/2)
        movw            r5,  #(0x5556/2)
        it              ne
        movne           lr,  r5
        vdup.16         q12, lr
        vqdmulh.s16     d0,  d0,  d24
1:
        vdup.8          q0,  d0[0]
2:
        vst1.8          {d0,  d1},  [r0,  :128], r1
        vst1.8          {d0,  d1},  [r12, :128], r1
        subs            r4,  r4, #4
        vst1.8          {d0,  d1},  [r0,  :128], r1
        vst1.8          {d0,  d1},  [r12, :128], r1
        bgt             2b
        pop             {r4-r6, pc}

L(ipred_dc_h32):
        vld1.8          {d0,  d1,  d2,  d3},  [r2]!
        vaddl.u8        q0,  d0,  d1
        vaddl.u8        q1,  d2,  d3
        vadd.u16        q0,  q0,  q1
        vadd.u16        d0,  d0,  d1
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        bx              r3
L(ipred_dc_w32):
        add             r2,  r2,  #1
        vld1.8          {d2,  d3,  d4,  d5},  [r2]
        vadd.s16        d0,  d0,  d30
        vaddl.u8        q2,  d4,  d5
        vadd.u16        d4,  d4,  d5
        vaddl.u8        q1,  d2,  d3
        vadd.u16        d2,  d2,  d3
        vpadd.u16       d4,  d4
        vpadd.u16       d2,  d2
        vpadd.u16       d4,  d4
        vpadd.u16       d2,  d2
        cmp             r4,  #32
        vadd.s16        d0,  d0,  d4
        vadd.s16        d0,  d0,  d2
        vshl.u16        d4,  d0,  d28
        beq             1f                  // h = 8/16/64
        cmp             r4,  #8
        movw            lr,  #(0x3334/2)
        movw            r5,  #(0x5556/2)
        it              ne
        movne           lr,  r5
        vdup.16         q12, lr
        vqdmulh.s16     d4,  d4,  d24
1:
        vdup.8          q0,  d4[0]
        vdup.8          q1,  d4[0]
2:
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128], r1
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128], r1
        subs            r4,  r4,  #4
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128], r1
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128], r1
        bgt             2b
        pop             {r4-r6, pc}

L(ipred_dc_h64):
        vld1.8          {d0,  d1,  d2,  d3},  [r2]!
        vaddl.u8        q0,  d0,  d1
        vld1.8          {d4,  d5,  d6,  d7},  [r2]!
        vaddl.u8        q1,  d2,  d3
        vaddl.u8        q2,  d4,  d5
        vaddl.u8        q3,  d6,  d7
        vadd.u16        q0,  q0,  q1
        vadd.u16        q1,  q2,  q3
        vadd.u16        q0,  q0,  q1
        vadd.u16        d0,  d0,  d1
        vpadd.u16       d0,  d0
        vpadd.u16       d0,  d0
        bx              r3
L(ipred_dc_w64):
        add             r2,  r2,  #1
        vld1.8          {d2,  d3,  d4,  d5},  [r2]!
        vadd.s16        d0,  d0,  d30
        vaddl.u8        q2,  d4,  d5
        vaddl.u8        q1,  d2,  d3
        vadd.u16        d4,  d4,  d5
        vadd.u16        d2,  d2,  d3
        vld1.8          {d16, d17, d18, d19}, [r2]
        vpadd.u16       d4,  d4
        vpadd.u16       d2,  d2
        vpadd.u16       d4,  d4
        vpadd.u16       d2,  d2
        vaddl.u8        q8,  d16, d17
        vaddl.u8        q9,  d18, d19
        vadd.u16        d16, d16, d17
        vadd.u16        d18, d18, d19
        vpadd.u16       d16, d16
        vpadd.u16       d18, d18
        vpadd.u16       d16, d16
        vpadd.u16       d18, d18
        vadd.u16        d2,  d2,  d4
        vadd.u16        d3,  d16, d18
        cmp             r4,  #64
        vadd.s16        d0,  d0,  d2
        vadd.s16        d0,  d0,  d3
        vshl.u16        d18, d0,  d28
        beq             1f                  // h = 16/32
        movw            lr,  #(0x5556/2)
        movt            lr,  #(0x3334/2)
        mov             r5,  r4
        and             r5,  r5,  #31
        lsr             lr,  lr,  r5
        vdup.16         d30, lr
        vqdmulh.s16     d18, d18, d30
1:
        sub             r1,  r1,  #32
        vdup.8          q0,  d18[0]
        vdup.8          q1,  d18[0]
        vdup.8          q2,  d18[0]
        vdup.8          q3,  d18[0]
2:
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128]!
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128]!
        vst1.8          {d4,  d5,  d6,  d7},  [r0,  :128], r1
        vst1.8          {d4,  d5,  d6,  d7},  [r12, :128], r1
        subs            r4,  r4,  #4
        vst1.8          {d0,  d1,  d2,  d3},  [r0,  :128]!
        vst1.8          {d0,  d1,  d2,  d3},  [r12, :128]!
        vst1.8          {d4,  d5,  d6,  d7},  [r0,  :128], r1
        vst1.8          {d4,  d5,  d6,  d7},  [r12, :128], r1
        bgt             2b
        pop             {r4-r6, pc}
endfunc

