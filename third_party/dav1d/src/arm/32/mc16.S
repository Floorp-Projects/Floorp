/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Janne Grunau
 * Copyright © 2020, Martin Storsjo
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

#define PREP_BIAS 8192

.macro avg d0, d00, d01, d1, d10, d11
        vld1.16         {q0, q1}, [r2, :128]!
        vld1.16         {q2, q3}, [r3, :128]!
        vqadd.s16       q0,  q0,  q2
        vqadd.s16       q1,  q1,  q3
        vmax.s16        q0,  q0,  q12 // -2*PREP_BIAS - 1 << intermediate_bits
        vmax.s16        q1,  q1,  q12 // -2*PREP_BIAS - 1 << intermediate_bits
        vqsub.s16       q0,  q0,  q12 // -2*PREP_BIAS - 1 << intermediate_bits
        vqsub.s16       q1,  q1,  q12 // -2*PREP_BIAS - 1 << intermediate_bits
        vshl.s16        \d0, q0,  q13 // -(intermediate_bits+1)
        vshl.s16        \d1, q1,  q13 // -(intermediate_bits+1)
.endm

.macro w_avg d0, d00, d01, d1, d10, d11
        vld1.16         {q0, q1}, [r2, :128]!
        vld1.16         {q2, q3}, [r3, :128]!
        // This difference requires a 17 bit range, and all bits are
        // significant for the following multiplication.
        vsubl.s16       \d0, d4,  d0
        vsubl.s16       q0,  d5,  d1
        vsubl.s16       \d1, d6,  d2
        vsubl.s16       q1,  d7,  d3
        vmul.s32        \d0, \d0, q4
        vmul.s32        q0,  q0,  q4
        vmul.s32        \d1, \d1, q4
        vmul.s32        q1,  q1,  q4
        vshr.s32        \d0, \d0, #4
        vshr.s32        q0,  q0,  #4
        vshr.s32        \d1, \d1, #4
        vshr.s32        q1,  q1,  #4
        vaddw.s16       \d0, \d0, d4
        vaddw.s16       q0,  q0,  d5
        vaddw.s16       \d1, \d1, d6
        vaddw.s16       q1,  q1,  d7
        vmovn.i32       \d00, \d0
        vmovn.i32       \d01, q0
        vmovn.i32       \d10, \d1
        vmovn.i32       \d11, q1
        vrshl.s16       \d0, \d0, q13 // -intermediate_bits
        vrshl.s16       \d1, \d1, q13 // -intermediate_bits
        vadd.s16        \d0, \d0, q12 // PREP_BIAS >> intermediate_bits
        vadd.s16        \d1, \d1, q12 // PREP_BIAS >> intermediate_bits
        vmin.s16        \d0, \d0, q15 // bitdepth_max
        vmin.s16        \d1, \d1, q15 // bitdepth_max
        vmax.s16        \d0, \d0, q14 // 0
        vmax.s16        \d1, \d1, q14 // 0
.endm

.macro mask d0, d00, d01, d1, d10, d11
        vld1.8          {q7},     [r6, :128]!
        vld1.16         {q0, q1}, [r2, :128]!
        vneg.s8         q7,  q7
        vld1.16         {q2, q3}, [r3, :128]!
        vmovl.s8        q6,  d14
        vmovl.s8        q7,  d15
        vmovl.s16       q4,  d12
        vmovl.s16       q5,  d13
        vmovl.s16       q6,  d14
        vmovl.s16       q7,  d15
        vsubl.s16       \d0, d4,  d0
        vsubl.s16       q0,  d5,  d1
        vsubl.s16       \d1, d6,  d2
        vsubl.s16       q1,  d7,  d3
        vmul.s32        \d0, \d0, q4
        vmul.s32        q0,  q0,  q5
        vmul.s32        \d1, \d1, q6
        vmul.s32        q1,  q1,  q7
        vshr.s32        \d0, \d0, #6
        vshr.s32        q0,  q0,  #6
        vshr.s32        \d1, \d1, #6
        vshr.s32        q1,  q1,  #6
        vaddw.s16       \d0, \d0, d4
        vaddw.s16       q0,  q0,  d5
        vaddw.s16       \d1, \d1, d6
        vaddw.s16       q1,  q1,  d7
        vmovn.i32       \d00, \d0
        vmovn.i32       \d01, q0
        vmovn.i32       \d10, \d1
        vmovn.i32       \d11, q1
        vrshl.s16       \d0, \d0, q13 // -intermediate_bits
        vrshl.s16       \d1, \d1, q13 // -intermediate_bits
        vadd.s16        \d0, \d0, q12 // PREP_BIAS >> intermediate_bits
        vadd.s16        \d1, \d1, q12 // PREP_BIAS >> intermediate_bits
        vmin.s16        \d0, \d0, q15 // bitdepth_max
        vmin.s16        \d1, \d1, q15 // bitdepth_max
        vmax.s16        \d0, \d0, q14 // 0
        vmax.s16        \d1, \d1, q14 // 0
.endm

.macro bidir_fn type, bdmax
function \type\()_16bpc_neon, export=1
        push            {r4-r7,lr}
        ldr             r4,  [sp, #20]
        ldr             r5,  [sp, #24]
        ldr             r6,  [sp, #28]
        clz             r4,  r4
.ifnc \type, avg
        ldr             r7,  [sp, #32]
        vmov.i16        q14, #0
        vdup.16         q15, r7         // bitdepth_max
.endif
.ifc \type, w_avg
        vpush           {q4}
.endif
.ifc \type, mask
        vpush           {q4-q7}
.endif
        clz             r7,  \bdmax
        sub             r7,  r7,  #18   // intermediate_bits = clz(bitdepth_max) - 18
.ifc \type, avg
        mov             lr,  #1
        movw            r12, #2*PREP_BIAS
        lsl             lr,  lr,  r7    // 1 << intermediate_bits
        neg             r12, r12         // -2*PREP_BIAS
        add             r7,  r7,  #1
        sub             r12, r12, lr    // -2*PREP_BIAS - 1 << intermediate_bits
        neg             r7,  r7         // -(intermediate_bits+1)
        vdup.16         q12, r12         // -2*PREP_BIAS - 1 << intermediate_bits
        vdup.16         q13, r7         // -(intermediate_bits+1)
.else
        mov             r12, #PREP_BIAS
        lsr             r12, r12, r7    // PREP_BIAS >> intermediate_bits
        neg             r7,  r7         // -intermediate_bits
        vdup.16         q12, r12         // PREP_BIAS >> intermediate_bits
        vdup.16         q13, r7         // -intermediate_bits
.endif
.ifc \type, w_avg
        vdup.32         q4,  r6
        vneg.s32        q4,  q4
.endif
        adr             r7,  L(\type\()_tbl)
        sub             r4,  r4,  #24
        \type           q8,  d16, d17, q9,  d18, d19
        ldr             r4,  [r7, r4, lsl #2]
        add             r7,  r7,  r4
        bx              r7

        .align 2
L(\type\()_tbl):
        .word 1280f - L(\type\()_tbl) + CONFIG_THUMB
        .word 640f  - L(\type\()_tbl) + CONFIG_THUMB
        .word 320f  - L(\type\()_tbl) + CONFIG_THUMB
        .word 160f  - L(\type\()_tbl) + CONFIG_THUMB
        .word 80f   - L(\type\()_tbl) + CONFIG_THUMB
        .word 40f   - L(\type\()_tbl) + CONFIG_THUMB

40:
        add             r7,  r0,  r1
        lsl             r1,  r1,  #1
4:
        subs            r5,  r5,  #4
        vst1.16         {d16},  [r0, :64], r1
        vst1.16         {d17},  [r7, :64], r1
        vst1.16         {d18},  [r0, :64], r1
        vst1.16         {d19},  [r7, :64], r1
        ble             0f
        \type           q8,  d16, d17, q9,  d18, d19
        b               4b
80:
        add             r7,  r0,  r1
        lsl             r1,  r1,  #1
8:
        vst1.16         {q8},  [r0, :128], r1
        subs            r5,  r5,  #2
        vst1.16         {q9},  [r7, :128], r1
        ble             0f
        \type           q8,  d16, d17, q9,  d18, d19
        b               8b
160:
16:
        \type           q10, d20, d21, q11, d22, d23
        vst1.16         {q8,  q9},  [r0, :128], r1
        subs            r5,  r5,  #2
        vst1.16         {q10, q11}, [r0, :128], r1
        ble             0f
        \type           q8,  d16, d17, q9,  d18, d19
        b               16b
320:
        add             r7,  r0,  #32
32:
        \type           q10, d20, d21, q11, d22, d23
        vst1.16         {q8,  q9},  [r0, :128], r1
        subs            r5,  r5,  #1
        vst1.16         {q10, q11}, [r7, :128], r1
        ble             0f
        \type           q8,  d16, d17, q9,  d18, d19
        b               32b
640:
        add             r7,  r0,  #32
        mov             r12, #64
        sub             r1,  r1,  #64
64:
        \type           q10, d20, d21, q11, d22, d23
        vst1.16         {q8,  q9},  [r0, :128], r12
        \type           q8,  d16, d17, q9,  d18, d19
        vst1.16         {q10, q11}, [r7, :128], r12
        \type           q10, d20, d21, q11, d22, d23
        vst1.16         {q8,  q9},  [r0, :128], r1
        subs            r5,  r5,  #1
        vst1.16         {q10, q11}, [r7, :128], r1
        ble             0f
        \type           q8,  d16, d17, q9,  d18, d19
        b               64b
1280:
        add             r7,  r0,  #32
        mov             r12, #64
        sub             r1,  r1,  #192
128:
        \type           q10, d20, d21, q11, d22, d23
        vst1.16         {q8,  q9},  [r0, :128], r12
        \type           q8,  d16, d17, q9,  d18, d19
        vst1.16         {q10, q11}, [r7, :128], r12
        \type           q10, d20, d21, q11, d22, d23
        vst1.16         {q8,  q9},  [r0, :128], r12
        \type           q8,  d16, d17, q9,  d18, d19
        vst1.16         {q10, q11}, [r7, :128], r12
        \type           q10, d20, d21, q11, d22, d23
        vst1.16         {q8,  q9},  [r0, :128], r12
        \type           q8,  d16, d17, q9,  d18, d19
        vst1.16         {q10, q11}, [r7, :128], r12
        \type           q10, d20, d21, q11, d22, d23
        vst1.16         {q8,  q9},  [r0, :128], r1
        subs            r5,  r5,  #1
        vst1.16         {q10, q11}, [r7, :128], r1
        ble             0f
        \type           q8,  d16, d17, q9,  d18, d19
        b               128b
0:
.ifc \type, mask
        vpop            {q4-q7}
.endif
.ifc \type, w_avg
        vpop            {q4}
.endif
        pop             {r4-r7,pc}
endfunc
.endm

bidir_fn avg, r6
bidir_fn w_avg, r7
bidir_fn mask, r7


// This has got the same signature as the put_8tap functions,
// and assumes that r9 is set to (clz(w)-24).
function put_neon
        adr             r10, L(put_tbl)
        ldr             r9,  [r10, r9, lsl #2]
        add             r10, r10, r9
        bx              r10

        .align 2
L(put_tbl):
        .word 1280f - L(put_tbl) + CONFIG_THUMB
        .word 640f  - L(put_tbl) + CONFIG_THUMB
        .word 320f  - L(put_tbl) + CONFIG_THUMB
        .word 16f   - L(put_tbl) + CONFIG_THUMB
        .word 80f   - L(put_tbl) + CONFIG_THUMB
        .word 4f    - L(put_tbl) + CONFIG_THUMB
        .word 2f    - L(put_tbl) + CONFIG_THUMB

2:
        vld1.32         {d0[]}, [r2], r3
        vld1.32         {d1[]}, [r2], r3
        subs            r5,  r5,  #2
        vst1.32         {d0[0]}, [r0, :32], r1
        vst1.32         {d1[1]}, [r0, :32], r1
        bgt             2b
        pop             {r4-r11,pc}
4:
        vld1.16         {d0}, [r2], r3
        vld1.16         {d1}, [r2], r3
        subs            r5,  r5,  #2
        vst1.16         {d0}, [r0, :64], r1
        vst1.16         {d1}, [r0, :64], r1
        bgt             4b
        pop             {r4-r11,pc}
80:
        add             r8,  r0,  r1
        lsl             r1,  r1,  #1
        add             r9,  r2,  r3
        lsl             r3,  r3,  #1
8:
        vld1.16         {q0}, [r2], r3
        vld1.16         {q1}, [r9], r3
        subs            r5,  r5,  #2
        vst1.16         {q0}, [r0, :128], r1
        vst1.16         {q1}, [r8, :128], r1
        bgt             8b
        pop             {r4-r11,pc}
16:
        vld1.16         {q0,  q1},  [r2], r3
        subs            r5,  r5,  #1
        vst1.16         {q0,  q1},  [r0, :128], r1
        bgt             16b
        pop             {r4-r11,pc}
320:
        sub             r1,  r1,  #32
        sub             r3,  r3,  #32
32:
        vld1.16         {q0,  q1},  [r2]!
        vst1.16         {q0,  q1},  [r0, :128]!
        vld1.16         {q2,  q3},  [r2], r3
        subs            r5,  r5,  #1
        vst1.16         {q2,  q3},  [r0, :128], r1
        bgt             32b
        pop             {r4-r11,pc}
640:
        sub             r1,  r1,  #96
        sub             r3,  r3,  #96
64:
        vld1.16         {q8,  q9},  [r2]!
        vst1.16         {q8,  q9},  [r0, :128]!
        vld1.16         {q10, q11}, [r2]!
        vst1.16         {q10, q11}, [r0, :128]!
        vld1.16         {q12, q13}, [r2]!
        vst1.16         {q12, q13}, [r0, :128]!
        vld1.16         {q14, q15}, [r2], r3
        subs            r5,  r5,  #1
        vst1.16         {q14, q15}, [r0, :128], r1
        bgt             64b
        pop             {r4-r11,pc}
1280:
        sub             r1,  r1,  #224
        sub             r3,  r3,  #224
128:
        vld1.16         {q8,  q9},  [r2]!
        vst1.16         {q8,  q9},  [r0, :128]!
        vld1.16         {q10, q11}, [r2]!
        vst1.16         {q10, q11}, [r0, :128]!
        vld1.16         {q12, q13}, [r2]!
        vst1.16         {q12, q13}, [r0, :128]!
        vld1.16         {q14, q15}, [r2]!
        vst1.16         {q14, q15}, [r0, :128]!
        vld1.16         {q8,  q9},  [r2]!
        vst1.16         {q8,  q9},  [r0, :128]!
        vld1.16         {q10, q11}, [r2]!
        vst1.16         {q10, q11}, [r0, :128]!
        vld1.16         {q12, q13}, [r2]!
        vst1.16         {q12, q13}, [r0, :128]!
        vld1.16         {q14, q15}, [r2], r3
        subs            r5,  r5,  #1
        vst1.16         {q14, q15}, [r0, :128], r1
        bgt             128b
        pop             {r4-r11,pc}
endfunc

// This has got the same signature as the prep_8tap functions,
// and assumes that r9 is set to (clz(w)-24), r7 to intermediate_bits and
// r8 to w*2.
function prep_neon
        adr             r10, L(prep_tbl)
        ldr             r9,  [r10, r9, lsl #2]
        vdup.16         q15, r7   // intermediate_bits
        vmov.i16        q14, #PREP_BIAS
        add             r10, r10, r9
        bx              r10

        .align 2
L(prep_tbl):
        .word 1280f - L(prep_tbl) + CONFIG_THUMB
        .word 640f  - L(prep_tbl) + CONFIG_THUMB
        .word 320f  - L(prep_tbl) + CONFIG_THUMB
        .word 16f   - L(prep_tbl) + CONFIG_THUMB
        .word 80f   - L(prep_tbl) + CONFIG_THUMB
        .word 40f   - L(prep_tbl) + CONFIG_THUMB

40:
        add             r9,  r1,  r2
        lsl             r2,  r2,  #1
4:
        vld1.16         {d0}, [r1], r2
        vld1.16         {d1}, [r9], r2
        subs            r4,  r4,  #2
        vshl.s16        q0,  q0,  q15
        vsub.i16        q0,  q0,  q14
        vst1.16         {q0}, [r0, :128]!
        bgt             4b
        pop             {r4-r11,pc}
80:
        add             r9,  r1,  r2
        lsl             r2,  r2,  #1
8:
        vld1.16         {q0}, [r1], r2
        vld1.16         {q1}, [r9], r2
        subs            r4,  r4,  #2
        vshl.s16        q0,  q0,  q15
        vshl.s16        q1,  q1,  q15
        vsub.i16        q0,  q0,  q14
        vsub.i16        q1,  q1,  q14
        vst1.16         {q0, q1}, [r0, :128]!
        bgt             8b
        pop             {r4-r11,pc}
16:
        vld1.16         {q0, q1}, [r1], r2
        vshl.s16        q0,  q0,  q15
        vld1.16         {q2, q3}, [r1], r2
        subs            r4,  r4,  #2
        vshl.s16        q1,  q1,  q15
        vshl.s16        q2,  q2,  q15
        vshl.s16        q3,  q3,  q15
        vsub.i16        q0,  q0,  q14
        vsub.i16        q1,  q1,  q14
        vsub.i16        q2,  q2,  q14
        vst1.16         {q0, q1}, [r0, :128]!
        vsub.i16        q3,  q3,  q14
        vst1.16         {q2, q3}, [r0, :128]!
        bgt             16b
        pop             {r4-r11,pc}
320:
        sub             r2,  r2,  #32
32:
        vld1.16         {q0, q1}, [r1]!
        subs            r4,  r4,  #1
        vshl.s16        q0,  q0,  q15
        vld1.16         {q2, q3}, [r1], r2
        vshl.s16        q1,  q1,  q15
        vshl.s16        q2,  q2,  q15
        vshl.s16        q3,  q3,  q15
        vsub.i16        q0,  q0,  q14
        vsub.i16        q1,  q1,  q14
        vsub.i16        q2,  q2,  q14
        vst1.16         {q0, q1}, [r0, :128]!
        vsub.i16        q3,  q3,  q14
        vst1.16         {q2, q3}, [r0, :128]!
        bgt             32b
        pop             {r4-r11,pc}
640:
        sub             r2,  r2,  #96
64:
        vld1.16         {q0,  q1},  [r1]!
        subs            r4,  r4,  #1
        vshl.s16        q0,  q0,  q15
        vld1.16         {q2,  q3},  [r1]!
        vshl.s16        q1,  q1,  q15
        vld1.16         {q8,  q9},  [r1]!
        vshl.s16        q2,  q2,  q15
        vld1.16         {q10, q11}, [r1], r2
        vshl.s16        q3,  q3,  q15
        vshl.s16        q8,  q8,  q15
        vshl.s16        q9,  q9,  q15
        vshl.s16        q10, q10, q15
        vshl.s16        q11, q11, q15
        vsub.i16        q0,  q0,  q14
        vsub.i16        q1,  q1,  q14
        vsub.i16        q2,  q2,  q14
        vsub.i16        q3,  q3,  q14
        vsub.i16        q8,  q8,  q14
        vst1.16         {q0,  q1},  [r0, :128]!
        vsub.i16        q9,  q9,  q14
        vst1.16         {q2,  q3},  [r0, :128]!
        vsub.i16        q10, q10, q14
        vst1.16         {q8,  q9},  [r0, :128]!
        vsub.i16        q11, q11, q14
        vst1.16         {q10, q11}, [r0, :128]!
        bgt             64b
        pop             {r4-r11,pc}
1280:
        sub             r2,  r2,  #224
128:
        vld1.16         {q0,  q1},  [r1]!
        subs            r4,  r4,  #1
        vshl.s16        q0,  q0,  q15
        vld1.16         {q2,  q3},  [r1]!
        vshl.s16        q1,  q1,  q15
        vld1.16         {q8,  q9},  [r1]!
        vshl.s16        q2,  q2,  q15
        vld1.16         {q10, q11}, [r1]!
        vshl.s16        q3,  q3,  q15
        vshl.s16        q8,  q8,  q15
        vshl.s16        q9,  q9,  q15
        vshl.s16        q10, q10, q15
        vshl.s16        q11, q11, q15
        vsub.i16        q0,  q0,  q14
        vsub.i16        q1,  q1,  q14
        vsub.i16        q2,  q2,  q14
        vsub.i16        q3,  q3,  q14
        vsub.i16        q8,  q8,  q14
        vst1.16         {q0,  q1},  [r0, :128]!
        vld1.16         {q0,  q1},  [r1]!
        vsub.i16        q9,  q9,  q14
        vsub.i16        q10, q10, q14
        vst1.16         {q2,  q3},  [r0, :128]!
        vld1.16         {q2,  q3},  [r1]!
        vsub.i16        q11, q11, q14
        vshl.s16        q0,  q0,  q15
        vst1.16         {q8,  q9},  [r0, :128]!
        vld1.16         {q8,  q9},  [r1]!
        vshl.s16        q1,  q1,  q15
        vshl.s16        q2,  q2,  q15
        vst1.16         {q10, q11}, [r0, :128]!
        vld1.16         {q10, q11}, [r1], r2
        vshl.s16        q3,  q3,  q15
        vshl.s16        q8,  q8,  q15
        vshl.s16        q9,  q9,  q15
        vshl.s16        q10, q10, q15
        vshl.s16        q11, q11, q15
        vsub.i16        q0,  q0,  q14
        vsub.i16        q1,  q1,  q14
        vsub.i16        q2,  q2,  q14
        vsub.i16        q3,  q3,  q14
        vsub.i16        q8,  q8,  q14
        vst1.16         {q0,  q1},  [r0, :128]!
        vsub.i16        q9,  q9,  q14
        vst1.16         {q2,  q3},  [r0, :128]!
        vsub.i16        q10, q10, q14
        vst1.16         {q8,  q9},  [r0, :128]!
        vsub.i16        q11, q11, q14
        vst1.16         {q10, q11}, [r0, :128]!
        bgt             128b
        pop             {r4-r11,pc}
endfunc

.macro load_slice s0, s1, strd, wd, d0, d1, d2, d3, d4, d5, d6
        vld1.\wd        {\d0[]}, [\s0], \strd
        vld1.\wd        {\d1[]}, [\s1], \strd
.ifnb \d2
        vld1.\wd        {\d2[]}, [\s0], \strd
        vld1.\wd        {\d3[]}, [\s1], \strd
.endif
.ifnb \d4
        vld1.\wd        {\d4[]}, [\s0], \strd
.endif
.ifnb \d5
        vld1.\wd        {\d5[]}, [\s1], \strd
.endif
.ifnb \d6
        vld1.\wd        {\d6[]}, [\s0], \strd
.endif
.endm
.macro load_reg s0, s1, strd, d0, d1, d2, d3, d4, d5, d6
        vld1.16         {\d0}, [\s0], \strd
        vld1.16         {\d1}, [\s1], \strd
.ifnb \d2
        vld1.16         {\d2}, [\s0], \strd
        vld1.16         {\d3}, [\s1], \strd
.endif
.ifnb \d4
        vld1.16         {\d4}, [\s0], \strd
.endif
.ifnb \d5
        vld1.16         {\d5}, [\s1], \strd
.endif
.ifnb \d6
        vld1.16         {\d6}, [\s0], \strd
.endif
.endm
.macro load_regpair s0, s1, strd, d0, d1, d2, d3, d4, d5
        vld1.16         {\d0, \d1}, [\s0], \strd
.ifnb \d2
        vld1.16         {\d2, \d3}, [\s1], \strd
.endif
.ifnb \d4
        vld1.16         {\d4, \d5}, [\s0], \strd
.endif
.endm
.macro load_32 s0, s1, strd, d0, d1, d2, d3, d4, d5, d6
        load_slice      \s0, \s1, \strd, 32, \d0, \d1, \d2, \d3, \d4, \d5, \d6
.endm
.macro load_16s16 s0, s1, strd, d0, d1, d2, d3, d4, d5
        load_regpair    \s0, \s1, \strd, \d0, \d1, \d2, \d3, \d4, \d5
.endm
.macro interleave_1_32 r0, r1, r2, r3, r4
        vext.8          \r0, \r0, \r1, #4
        vext.8          \r1, \r1, \r2, #4
.ifnb \r3
        vext.8          \r2, \r2, \r3, #4
        vext.8          \r3, \r3, \r4, #4
.endif
.endm
.macro vmin_u16 c, r0, r1, r2, r3
        vmin.u16        \r0, \r0, \c
.ifnb \r1
        vmin.u16        \r1, \r1, \c
.endif
.ifnb \r2
        vmin.u16        \r2, \r2, \c
        vmin.u16        \r3, \r3, \c
.endif
.endm
.macro vsub_i16 c, r0, r1, r2, r3
        vsub.i16        \r0, \r0, \c
.ifnb \r1
        vsub.i16        \r1, \r1, \c
.endif
.ifnb \r2
        vsub.i16        \r2, \r2, \c
        vsub.i16        \r3, \r3, \c
.endif
.endm
.macro vmull_vmlal_4 d, s0, s1, s2, s3
        vmull.s16       \d,  \s0, d0[0]
        vmlal.s16       \d,  \s1, d0[1]
        vmlal.s16       \d,  \s2, d0[2]
        vmlal.s16       \d,  \s3, d0[3]
.endm
.macro vmull_vmlal_8 d, s0, s1, s2, s3, s4, s5, s6, s7
        vmull.s16       \d,  \s0, d0[0]
        vmlal.s16       \d,  \s1, d0[1]
        vmlal.s16       \d,  \s2, d0[2]
        vmlal.s16       \d,  \s3, d0[3]
        vmlal.s16       \d,  \s4, d1[0]
        vmlal.s16       \d,  \s5, d1[1]
        vmlal.s16       \d,  \s6, d1[2]
        vmlal.s16       \d,  \s7, d1[3]
.endm
.macro vqrshrun_s32 shift, q0, d0, q1, d1, q2, d2, q3, d3
        vqrshrun.s32    \d0, \q0, #\shift
.ifnb \q1
        vqrshrun.s32    \d1, \q1, #\shift
.endif
.ifnb \q2
        vqrshrun.s32    \d2, \q2, #\shift
        vqrshrun.s32    \d3, \q3, #\shift
.endif
.endm
.macro vmovn_i32 q0, d0, q1, d1, q2, d2, q3, d3
        vmovn.i32       \d0, \q0
.ifnb \q1
        vmovn.i32       \d1, \q1
.endif
.ifnb \q2
        vmovn.i32       \d2, \q2
        vmovn.i32       \d3, \q3
.endif
.endm
.macro vrshl_s32 shift, r0, r1, r2, r3
        vrshl.s32       \r0, \r0, \shift
        vrshl.s32       \r1, \r1, \shift
.ifnb \r2
        vrshl.s32       \r2, \r2, \shift
        vrshl.s32       \r3, \r3, \shift
.endif
.endm
.macro vst1_32 strd, r0, r1
        vst1.32         {\r0[0]}, [r0, :32], \strd
        vst1.32         {\r0[1]}, [r9, :32], \strd
.ifnb \r1
        vst1.32         {\r1[0]}, [r0, :32], \strd
        vst1.32         {\r1[1]}, [r9, :32], \strd
.endif
.endm
.macro vst1_reg strd, align, r0, r1, r2, r3, r4, r5, r6, r7
        vst1.16         {\r0}, [r0, \align], \strd
        vst1.16         {\r1}, [r9, \align], \strd
.ifnb \r2
        vst1.16         {\r2}, [r0, \align], \strd
        vst1.16         {\r3}, [r9, \align], \strd
.endif
.ifnb \r4
        vst1.16         {\r4}, [r0, \align], \strd
        vst1.16         {\r5}, [r9, \align], \strd
        vst1.16         {\r6}, [r0, \align], \strd
        vst1.16         {\r7}, [r9, \align], \strd
.endif
.endm
.macro finalize type, q0, q1, d0, d1, q2, q3, d2, d3
.ifc \type, put
        vqrshrun_s32    6,   \q0, \d0, \q1, \d1, \q2, \d2, \q3, \d3
        vmin_u16        q15, \q0, \q1
.else
        vrshl_s32       q14, \q0, \q1, \q2, \q3 // -(6-intermediate_bits)
        vmovn_i32       \q0, \d0, \q1, \d1, \q2, \d2, \q3, \d3
        vsub_i16        q15, \q0, \q1           // PREP_BIAS
.endif
.endm
.macro shift_store_4 type, strd, q0, q1, d0, d1, q2, q3, d2, d3
        finalize        \type, \q0, \q1, \d0, \d1, \q2, \q3, \d2, \d3
        vst1_reg        \strd, :64, \d0, \d1, \d2, \d3
.endm
.macro shift_store_8 type, strd, q0, q1, d0, d1, q2, q3, d2, d3
        finalize        \type, \q0, \q1, \d0, \d1, \q2, \q3, \d2, \d3
        vst1_reg        \strd, :128, \q0, \q1
.endm
.macro shift_store_16 type, strd, q0, q1, d0, d1, q2, q3, d2, d3
        finalize        \type, \q0, \q1, \d0, \d1, \q2, \q3, \d2, \d3
        vst1.16         {\q0, \q1}, [r0, :128], \strd
.endm

.macro make_8tap_fn op, type, type_h, type_v
function \op\()_8tap_\type\()_16bpc_neon, export=1
        push            {r4-r11,lr}
        movw            r9,  \type_h
        movw            r10, \type_v
        b               \op\()_8tap_neon
endfunc
.endm

// No spaces in these expressions, due to gas-preprocessor.
#define REGULAR ((0*15<<7)|3*15)
#define SMOOTH  ((1*15<<7)|4*15)
#define SHARP   ((2*15<<7)|3*15)

.macro filter_fn type, dst, d_strd, src, s_strd, w, h, mx, my, bdmax, ds2, sr2
make_8tap_fn \type, regular,        REGULAR, REGULAR
make_8tap_fn \type, regular_smooth, REGULAR, SMOOTH
make_8tap_fn \type, regular_sharp,  REGULAR, SHARP
make_8tap_fn \type, smooth,         SMOOTH,  SMOOTH
make_8tap_fn \type, smooth_regular, SMOOTH,  REGULAR
make_8tap_fn \type, smooth_sharp,   SMOOTH,  SHARP
make_8tap_fn \type, sharp,          SHARP,   SHARP
make_8tap_fn \type, sharp_regular,  SHARP,   REGULAR
make_8tap_fn \type, sharp_smooth,   SHARP,   SMOOTH

function \type\()_8tap_neon
        ldrd            r4,  r5,  [sp, #36]
        ldrd            r6,  r7,  [sp, #44]
.ifc \bdmax, r8
        ldr             r8,  [sp, #52]
.endif
        movw            r11, #0x4081  // (1 << 14) | (1 << 7) | (1 << 0)
        mul             \mx, \mx, r11
        mul             \my, \my, r11
        add             \mx, \mx, r9  // mx, 8tap_h, 4tap_h
        add             \my, \my, r10 // my, 8tap_v, 4tap_v

.ifc \type, prep
        lsl             \d_strd, \w, #1
.endif

        vdup.16         q15, \bdmax            // bitdepth_max
        clz             \bdmax,  \bdmax
        clz             r9,  \w
        sub             \bdmax,  \bdmax,  #18  // intermediate_bits = clz(bitdepth_max) - 18
        tst             \mx, #(0x7f << 14)
        sub             r9,  r9,  #24
        add             lr,  \bdmax, #6        // 6 + intermediate_bits
        rsb             r12, \bdmax, #6        // 6 - intermediate_bits
        movrel          r11, X(mc_subpel_filters), -8
        bne             L(\type\()_8tap_h)
        tst             \my, #(0x7f << 14)
        bne             L(\type\()_8tap_v)
        b               \type\()_neon

L(\type\()_8tap_h):
        cmp             \w,  #4
        ubfx            r10, \mx, #7,  #7
        and             \mx, \mx, #0x7f
        it              gt
        movgt           \mx, r10
        tst             \my, #(0x7f << 14)
        add             \mx, r11, \mx, lsl #3
        bne             L(\type\()_8tap_hv)

        adr             r10, L(\type\()_8tap_h_tbl)
        vdup.32         q14, r12           // 6 - intermediate_bits
        ldr             r9,  [r10, r9, lsl #2]
        vneg.s32        q14, q14           // -(6-intermediate_bits)
.ifc \type, put
        vdup.16         q13, \bdmax        // intermediate_bits
.else
        vmov.i16        q13, #PREP_BIAS
.endif
        add             r10, r10, r9
.ifc \type, put
        vneg.s16        q13, q13           // -intermediate_bits
.endif
        bx              r10

        .align 2
L(\type\()_8tap_h_tbl):
        .word 1280f - L(\type\()_8tap_h_tbl) + CONFIG_THUMB
        .word 640f  - L(\type\()_8tap_h_tbl) + CONFIG_THUMB
        .word 320f  - L(\type\()_8tap_h_tbl) + CONFIG_THUMB
        .word 160f  - L(\type\()_8tap_h_tbl) + CONFIG_THUMB
        .word 80f   - L(\type\()_8tap_h_tbl) + CONFIG_THUMB
        .word 40f   - L(\type\()_8tap_h_tbl) + CONFIG_THUMB
        .word 20f   - L(\type\()_8tap_h_tbl) + CONFIG_THUMB

20:     // 2xN h
.ifc \type, put
        add             \mx, \mx, #2
        vld1.32         {d0[]}, [\mx]
        sub             \src,  \src,  #2
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
        vmovl.s8        q0,  d0
2:
        vld1.16         {q2}, [\src], \s_strd
        vld1.16         {q3}, [\sr2], \s_strd
        vext.8          d5,  d4,  d5,  #2
        vext.8          d7,  d6,  d7,  #2
        subs            \h,  \h,  #2
        vtrn.32         d4,  d6
        vtrn.32         d5,  d7
        vmull.s16       q1,  d4,  d0[0]
        vmlal.s16       q1,  d5,  d0[1]
        vmlal.s16       q1,  d6,  d0[2]
        vmlal.s16       q1,  d7,  d0[3]
        vrshl.s32       q1,  q1,  q14 // -(6-intermediate_bits)
        vqmovun.s32     d2,  q1
        vrshl.s16       d2,  d2,  d26 // -intermediate_bits
        vmin.u16        d2,  d2,  d30
        vst1.32         {d2[0]}, [\dst, :32], \d_strd
        vst1.32         {d2[1]}, [\ds2, :32], \d_strd
        bgt             2b
        pop             {r4-r11,pc}
.endif

40:     // 4xN h
        add             \mx, \mx, #2
        vld1.32         {d0[]}, [\mx]
        sub             \src,  \src,  #2
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
        vmovl.s8        q0,  d0
4:
        vld1.16         {q8},  [\src], \s_strd
        vld1.16         {q11}, [\sr2], \s_strd
        vext.8          d18, d16, d17, #2
        vext.8          d19, d16, d17, #4
        vext.8          d20, d16, d17, #6
        vext.8          d24, d22, d23, #2
        vext.8          d25, d22, d23, #4
        vext.8          d21, d22, d23, #6
        subs            \h,  \h,  #2
        vmull.s16       q2,  d16, d0[0]
        vmlal.s16       q2,  d18, d0[1]
        vmlal.s16       q2,  d19, d0[2]
        vmlal.s16       q2,  d20, d0[3]
        vmull.s16       q3,  d22, d0[0]
        vmlal.s16       q3,  d24, d0[1]
        vmlal.s16       q3,  d25, d0[2]
        vmlal.s16       q3,  d21, d0[3]
        vrshl.s32       q2,  q2,  q14 // -(6-intermediate_bits)
        vrshl.s32       q3,  q3,  q14 // -(6-intermediate_bits)
.ifc \type, put
        vqmovun.s32     d4,  q2
        vqmovun.s32     d5,  q3
        vrshl.s16       q2,  q2,  q13 // -intermediate_bits
        vmin.u16        q2,  q2,  q15
.else
        vmovn.s32       d4,  q2
        vmovn.s32       d5,  q3
        vsub.i16        q2,  q2,  q13 // PREP_BIAS
.endif
        vst1.16         {d4}, [\dst, :64], \d_strd
        vst1.16         {d5}, [\ds2, :64], \d_strd
        bgt             4b
        pop             {r4-r11,pc}

80:
160:
320:
640:
1280:   // 8xN, 16xN, 32xN, ... h
        vpush           {q4-q5}
        vld1.8          {d0}, [\mx, :64]
        sub             \src,  \src,  #6
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        vmovl.s8        q0,  d0

        sub             \s_strd,  \s_strd,  \w, lsl #1
        sub             \s_strd,  \s_strd,  #16
.ifc \type, put
        lsl             \d_strd,  \d_strd,  #1
        sub             \d_strd,  \d_strd,  \w, lsl #1
.endif
81:
        vld1.16         {q8,  q9},  [\src]!
        vld1.16         {q10, q11}, [\sr2]!
        mov             \mx, \w

8:
        vmull.s16       q1,  d16, d0[0]
        vmull.s16       q2,  d17, d0[0]
        vmull.s16       q3,  d20, d0[0]
        vmull.s16       q4,  d21, d0[0]
.irpc i, 1234567
        vext.8          q12, q8,  q9,  #(2*\i)
        vext.8          q5,  q10, q11, #(2*\i)
.if \i < 4
        vmlal.s16       q1,  d24, d0[\i]
        vmlal.s16       q2,  d25, d0[\i]
        vmlal.s16       q3,  d10, d0[\i]
        vmlal.s16       q4,  d11, d0[\i]
.else
        vmlal.s16       q1,  d24, d1[\i-4]
        vmlal.s16       q2,  d25, d1[\i-4]
        vmlal.s16       q3,  d10, d1[\i-4]
        vmlal.s16       q4,  d11, d1[\i-4]
.endif
.endr
        subs            \mx, \mx, #8
        vrshl.s32       q1,  q1,  q14 // -(6-intermediate_bits)
        vrshl.s32       q2,  q2,  q14 // -(6-intermediate_bits)
        vrshl.s32       q3,  q3,  q14 // -(6-intermediate_bits)
        vrshl.s32       q4,  q4,  q14 // -(6-intermediate_bits)
.ifc \type, put
        vqmovun.s32     d2,  q1
        vqmovun.s32     d3,  q2
        vqmovun.s32     d4,  q3
        vqmovun.s32     d5,  q4
        vrshl.s16       q1,  q1,  q13 // -intermediate_bits
        vrshl.s16       q2,  q2,  q13 // -intermediate_bits
        vmin.u16        q1,  q1,  q15
        vmin.u16        q2,  q2,  q15
.else
        vmovn.s32       d2,  q1
        vmovn.s32       d3,  q2
        vmovn.s32       d4,  q3
        vmovn.s32       d5,  q4
        vsub.i16        q1,  q1,  q13 // PREP_BIAS
        vsub.i16        q2,  q2,  q13 // PREP_BIAS
.endif
        vst1.16         {q1}, [\dst, :128]!
        vst1.16         {q2}, [\ds2, :128]!
        ble             9f

        vmov            q8,  q9
        vmov            q10, q11
        vld1.16         {q9},  [\src]!
        vld1.16         {q11}, [\sr2]!
        b               8b

9:
        add             \dst,  \dst,  \d_strd
        add             \ds2,  \ds2,  \d_strd
        add             \src,  \src,  \s_strd
        add             \sr2,  \sr2,  \s_strd

        subs            \h,  \h,  #2
        bgt             81b
        vpop            {q4-q5}
        pop             {r4-r11,pc}


L(\type\()_8tap_v):
        cmp             \h,  #4
        ubfx            r10, \my, #7,  #7
        and             \my, \my, #0x7f
        it              gt
        movgt           \my, r10
        add             \my, r11, \my, lsl #3

.ifc \type, prep
        vdup.32         q14, r12        // 6 - intermediate_bits
        vmov.i16        q15, #PREP_BIAS
.endif
        adr             r10, L(\type\()_8tap_v_tbl)
        ldr             r9,  [r10, r9, lsl #2]
.ifc \type, prep
        vneg.s32        q14, q14        // -(6-intermediate_bits)
.endif
        add             r10, r10, r9
        bx              r10

        .align 2
L(\type\()_8tap_v_tbl):
        .word 1280f - L(\type\()_8tap_v_tbl) + CONFIG_THUMB
        .word 640f  - L(\type\()_8tap_v_tbl) + CONFIG_THUMB
        .word 320f  - L(\type\()_8tap_v_tbl) + CONFIG_THUMB
        .word 160f  - L(\type\()_8tap_v_tbl) + CONFIG_THUMB
        .word 80f   - L(\type\()_8tap_v_tbl) + CONFIG_THUMB
        .word 40f   - L(\type\()_8tap_v_tbl) + CONFIG_THUMB
        .word 20f   - L(\type\()_8tap_v_tbl) + CONFIG_THUMB

20:     // 2xN v
.ifc \type, put
        bgt             28f

        cmp             \h,  #2
        add             \my, \my, #2
        vld1.32         {d0[]}, [\my]
        sub             \src,  \src,  \s_strd
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        lsl             \d_strd,  \d_strd,  #1
        vmovl.s8        q0,  d0

        // 2x2 v
        load_32         \src, \sr2, \s_strd, d1, d2, d3, d4, d5
        interleave_1_32 d1,  d2,  d3,  d4,  d5
        bgt             24f
        vmull_vmlal_4   q8,  d1,  d2,  d3,  d4
        vqrshrun_s32    6,   q8,  d16
        vmin_u16        d30, d16
        vst1_32         \d_strd,  d16
        pop             {r4-r11,pc}

24:     // 2x4 v
        load_32         \sr2, \src, \s_strd, d6, d7
        interleave_1_32 d5,  d6,  d7
        vmull_vmlal_4   q8,  d1,  d2,  d3,  d4
        vmull_vmlal_4   q9,  d3,  d4,  d5,  d6
        vqrshrun_s32    6,   q8,  d16, q9,  d17
        vmin_u16        q15, q8
        vst1_32         \d_strd,  d16, d17
        pop             {r4-r11,pc}

28:     // 2x8, 2x16 v
        vld1.8          {d0}, [\my, :64]
        sub             \sr2,  \src,  \s_strd, lsl #1
        add             \ds2,  \dst,  \d_strd
        sub             \src,  \sr2,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
        vmovl.s8        q0,  d0

        load_32         \src, \sr2, \s_strd, d2, d3, d4, d5, d6, d7, d16
        interleave_1_32 d2,  d3,  d4,  d5,  d6
        interleave_1_32 d6,  d7,  d16
216:
        subs            \h,  \h,  #8
        load_32         \sr2, \src, \s_strd, d17, d18, d19, d20
        load_32         \sr2, \src, \s_strd, d21, d22, d23, d24
        interleave_1_32 d16, d17, d18, d19, d20
        interleave_1_32 d20, d21, d22, d23, d24
        vmull_vmlal_8   q13, d2,  d3,  d4,  d5,  d6,  d7,  d16, d17
        vmull_vmlal_8   q1,  d4,  d5,  d6,  d7,  d16, d17, d18, d19
        vmull_vmlal_8   q2,  d6,  d7,  d16, d17, d18, d19, d20, d21
        vmull_vmlal_8   q3,  d16, d17, d18, d19, d20, d21, d22, d23
        vqrshrun_s32    6,   q13, d26, q1,  d27, q2,  d2,  q3,  d3
        vmin_u16        q15, q13, q1
        vst1_32         \d_strd,  d26, d27
        vst1_32         \d_strd,  d2,  d3
        ble             0f
        vmov            q1,  q9
        vmov            q2,  q10
        vmov            q3,  q11
        vmov            d16, d24
        b               216b
0:
        pop             {r4-r11,pc}
.endif

40:
        bgt             480f

        // 4x2, 4x4 v
        cmp             \h,  #2
        add             \my, \my, #2
        vld1.32         {d0[]}, [\my]
        sub             \src, \src, \s_strd
        add             \ds2, \dst, \d_strd
        add             \sr2, \src, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0

        load_reg        \src, \sr2, \s_strd, d1, d2, d3, d4, d5
        vmull_vmlal_4   q8,  d1,  d2,  d3,  d4
        vmull_vmlal_4   q9,  d2,  d3,  d4,  d5
        shift_store_4   \type, \d_strd, q8, q9, d16, d17
        ble             0f
        load_reg        \sr2, \src, \s_strd, d6, d7
        vmull_vmlal_4   q8,  d3,  d4,  d5,  d6
        vmull_vmlal_4   q9,  d4,  d5,  d6,  d7
        shift_store_4   \type, \d_strd, q8, q9, d16, d17
0:
        pop             {r4-r11,pc}

480:    // 4x8, 4x16 v
        vld1.8          {d0}, [\my, :64]
        sub             \sr2, \src, \s_strd, lsl #1
        add             \ds2, \dst, \d_strd
        sub             \src, \sr2, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0

        load_reg        \src, \sr2, \s_strd, d16, d17, d18, d19, d20, d21, d22

48:
        subs            \h,  \h,  #4
        load_reg        \sr2, \src, \s_strd, d23, d24, d25, d26
        vmull_vmlal_8   q1,  d16, d17, d18, d19, d20, d21, d22, d23
        vmull_vmlal_8   q2,  d17, d18, d19, d20, d21, d22, d23, d24
        vmull_vmlal_8   q3,  d18, d19, d20, d21, d22, d23, d24, d25
        vmull_vmlal_8   q8,  d19, d20, d21, d22, d23, d24, d25, d26
        shift_store_4   \type, \d_strd, q1, q2, d2, d3, q3, q8, d4, d5
        ble             0f
        vmov            q8,  q10
        vmov            q9,  q11
        vmov            q10, q12
        vmov            d22, d26
        b               48b
0:
        pop             {r4-r11,pc}

80:
        bgt             880f

        // 8x2, 8x4 v
        cmp             \h,  #2
        add             \my, \my, #2
        vld1.32         {d0[]}, [\my]
        sub             \src, \src, \s_strd
        add             \ds2, \dst, \d_strd
        add             \sr2, \src, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0

        load_reg        \src, \sr2, \s_strd, q1, q2, q3, q8, q9
        vmull_vmlal_4   q10, d2,  d4,  d6,  d16
        vmull_vmlal_4   q11, d3,  d5,  d7,  d17
        vmull_vmlal_4   q12, d4,  d6,  d16, d18
        vmull_vmlal_4   q13, d5,  d7,  d17, d19
        shift_store_8   \type, \d_strd, q10, q11, d20, d21, q12, q13, d22, d23
        ble             0f
        load_reg        \sr2, \src, \s_strd, q10, q11
        vmull_vmlal_4   q1,  d6,  d16, d18, d20
        vmull_vmlal_4   q2,  d7,  d17, d19, d21
        vmull_vmlal_4   q12, d16, d18, d20, d22
        vmull_vmlal_4   q13, d17, d19, d21, d23
        shift_store_8   \type, \d_strd, q1, q2, d2, d3, q12, q13, d4, d5
0:
        pop             {r4-r11,pc}

880:    // 8x6, 8x8, 8x16, 8x32 v
1680:   // 16x8, 16x16, ...
320:    // 32x8, 32x16, ...
640:
1280:
        vpush           {q4-q7}
        vld1.8          {d0}, [\my, :64]
        sub             \src, \src, \s_strd
        sub             \src, \src, \s_strd, lsl #1
        vmovl.s8        q0,  d0
        mov             \my, \h
168:
        add             \ds2, \dst, \d_strd
        add             \sr2, \src, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        load_reg        \src, \sr2, \s_strd, q5, q6, q7, q8, q9, q10, q11

88:
        subs            \h,  \h,  #2
        load_reg        \sr2, \src, \s_strd, q12, q13
        vmull_vmlal_8   q1,  d10, d12, d14, d16, d18, d20, d22, d24
        vmull_vmlal_8   q2,  d11, d13, d15, d17, d19, d21, d23, d25
        vmull_vmlal_8   q3,  d12, d14, d16, d18, d20, d22, d24, d26
        vmull_vmlal_8   q4,  d13, d15, d17, d19, d21, d23, d25, d27
        shift_store_8   \type, \d_strd, q1, q2,  d2,  d3,  q3,  q4,  d4,  d5
        ble             9f
        subs            \h,  \h,  #2
        load_reg        \sr2, \src, \s_strd, q1,  q2
        vmull_vmlal_8   q3,  d14, d16, d18, d20, d22, d24, d26, d2
        vmull_vmlal_8   q4,  d15, d17, d19, d21, d23, d25, d27, d3
        vmull_vmlal_8   q5,  d16, d18, d20, d22, d24, d26, d2,  d4
        vmull_vmlal_8   q6,  d17, d19, d21, d23, d25, d27, d3,  d5
        shift_store_8   \type, \d_strd, q3, q4,  d6,  d7,  q5,  q6,  d8,  d9
        ble             9f
        vmov            q5,  q9
        vmov            q6,  q10
        vmov            q7,  q11
        vmov            q8,  q12
        vmov            q9,  q13
        vmov            q10, q1
        vmov            q11, q2
        b               88b
9:
        subs            \w,  \w,  #8
        ble             0f
        asr             \s_strd, \s_strd, #1
        asr             \d_strd, \d_strd, #1
        mls             \src, \s_strd, \my, \src
        mls             \dst, \d_strd, \my, \dst
        sub             \src, \src, \s_strd, lsl #3
        mov             \h,  \my
        add             \src, \src, #16
        add             \dst, \dst, #16
        b               168b
0:
        vpop            {q4-q7}
        pop             {r4-r11,pc}

160:
        bgt             1680b

        // 16x2, 16x4 v
        vpush           {q6-q7}
        add             \my, \my, #2
        vld1.32         {d0[]}, [\my]
        sub             \src, \src, \s_strd
        vmovl.s8        q0,  d0

        load_16s16      \src, \src, \s_strd, q6,  q7,  q8,  q9, q10, q11
16:
        load_16s16      \src, \src, \s_strd, q12, q13
        subs            \h,  \h,  #1
        vmull_vmlal_4   q1,  d12, d16, d20, d24
        vmull_vmlal_4   q2,  d13, d17, d21, d25
        vmull_vmlal_4   q3,  d14, d18, d22, d26
        vmull_vmlal_4   q6,  d15, d19, d23, d27
        shift_store_16  \type, \d_strd, q1, q2, d2, d3, q3, q6, d4, d5
        ble             0f
        vmov            q6,  q8
        vmov            q7,  q9
        vmov            q8,  q10
        vmov            q9,  q11
        vmov            q10, q12
        vmov            q11, q13
        b               16b
0:
        vpop            {q6-q7}
        pop             {r4-r11,pc}


L(\type\()_8tap_hv):
        cmp             \h,  #4
        ubfx            r10, \my, #7,  #7
        and             \my, \my, #0x7f
        it              gt
        movgt           \my, r10
4:
        add             \my, r11, \my, lsl #3

        adr             r10, L(\type\()_8tap_hv_tbl)
        neg             r12, r12           // -(6-intermediate_bits)
        ldr             r9,  [r10, r9, lsl #2]
        vdup.32         q14, r12           // -(6-intermediate_bits)
.ifc \type, put
        neg             r8,  lr            // -(6+intermeidate_bits)
.else
        vmov.i16        q13, #PREP_BIAS
.endif
        add             r10, r10, r9
.ifc \type, put
        vdup.32         q13, r8            // -(6+intermediate_bits)
.endif
        bx              r10

        .align 2
L(\type\()_8tap_hv_tbl):
        .word 1280f - L(\type\()_8tap_hv_tbl) + CONFIG_THUMB
        .word 640f  - L(\type\()_8tap_hv_tbl) + CONFIG_THUMB
        .word 320f  - L(\type\()_8tap_hv_tbl) + CONFIG_THUMB
        .word 160f  - L(\type\()_8tap_hv_tbl) + CONFIG_THUMB
        .word 80f   - L(\type\()_8tap_hv_tbl) + CONFIG_THUMB
        .word 40f   - L(\type\()_8tap_hv_tbl) + CONFIG_THUMB
        .word 20f   - L(\type\()_8tap_hv_tbl) + CONFIG_THUMB

20:
.ifc \type, put
        add             \mx, \mx, #2
        vld1.32         {d0[]}, [\mx]
        bgt             280f
        add             \my, \my, #2
        vld1.32         {d2[]}, [\my]

        // 2x2, 2x4 hv
        sub             \sr2, \src, #2
        sub             \src, \sr2, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2

        vld1.16         {q11}, [\src], \s_strd
        vext.8          d24, d22, d23, #2
        vmull.s16       q11, d22, d0
        vmull.s16       q12, d24, d0
        vpadd.s32       d22, d22, d23
        vpadd.s32       d23, d24, d25
        vpadd.s32       d22, d22, d23
        vrshl.s32       d16, d22, d28 // -(6-intermediate_bits)
        vmovn.i32       d16, q8
        bl              L(\type\()_8tap_filter_2)

        vext.8          d16, d16, d16, #4
        vext.8          d16, d16, d24, #4
        vmov            d17, d24

2:
        bl              L(\type\()_8tap_filter_2)

        vext.8          d18, d17, d24, #4
        vmull.s16       q2,  d16, d2[0]
        vmlal.s16       q2,  d17, d2[1]
        vmlal.s16       q2,  d18, d2[2]
        vmlal.s16       q2,  d24, d2[3]

        vrshl.s32       q2,  q2,  q13 // -(6+intermediate_bits)
        vqmovun.s32     d4,  q2
        vmin.u16        d4,  d4,  d30
        subs            \h,  \h,  #2
        vst1.32         {d4[0]}, [\dst, :32], \d_strd
        vst1.32         {d4[1]}, [\ds2, :32], \d_strd
        ble             0f
        vmov            d16, d18
        vmov            d17, d24
        b               2b

280:    // 2x8, 2x16, 2x32 hv
        vld1.8          {d2},  [\my, :64]
        sub             \src, \src, #2
        sub             \sr2, \src, \s_strd, lsl #1
        sub             \src, \sr2, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2

        vld1.16         {q11}, [\src], \s_strd
        vext.8          d24, d22, d23, #2
        vmull.s16       q11, d22, d0
        vmull.s16       q12, d24, d0
        vpadd.s32       d22, d22, d23
        vpadd.s32       d23, d24, d25
        vpadd.s32       d22, d22, d23
        vrshl.s32       d16, d22, d28 // -(6-intermediate_bits)
        vmovn.i32       d16, q8

        bl              L(\type\()_8tap_filter_2)

        vext.8          d16, d16, d16, #4
        vext.8          d16, d16, d24, #4
        vmov            d17, d24
        bl              L(\type\()_8tap_filter_2)
        vext.8          d18, d17, d24, #4
        vmov            d19, d24
        bl              L(\type\()_8tap_filter_2)
        vext.8          d20, d19, d24, #4
        vmov            d21, d24

28:
        bl              L(\type\()_8tap_filter_2)
        vext.8          d22, d21, d24, #4
        vmull.s16       q3,  d16, d2[0]
        vmlal.s16       q3,  d17, d2[1]
        vmlal.s16       q3,  d18, d2[2]
        vmlal.s16       q3,  d19, d2[3]
        vmlal.s16       q3,  d20, d3[0]
        vmlal.s16       q3,  d21, d3[1]
        vmlal.s16       q3,  d22, d3[2]
        vmlal.s16       q3,  d24, d3[3]

        vrshl.s32       q3,  q3,  q13 // -(6+intermediate_bits)
        vqmovun.s32     d6,  q3
        vmin.u16        d6,  d6,  d30
        subs            \h,  \h,  #2
        vst1.32         {d6[0]}, [\dst, :32], \d_strd
        vst1.32         {d6[1]}, [\ds2, :32], \d_strd
        ble             0f
        vmov            q8,  q9
        vmov            q9,  q10
        vmov            d20, d22
        vmov            d21, d24
        b               28b
0:
        pop             {r4-r11,pc}

L(\type\()_8tap_filter_2):
        vld1.16         {q11}, [\sr2], \s_strd
        vld1.16         {q12}, [\src], \s_strd
        vext.8          d23, d22, d23, #2
        vext.8          d25, d24, d25, #2
        vtrn.32         q11, q12
        vmull.s16       q3,  d22, d0[0]
        vmlal.s16       q3,  d23, d0[1]
        vmlal.s16       q3,  d24, d0[2]
        vmlal.s16       q3,  d25, d0[3]
        vrshl.s32       q3,  q3,  q14 // -(6-intermediate_bits)
        vmovn.i32       d24, q3
        bx              lr
.endif

40:
        add             \mx, \mx, #2
        vld1.32         {d0[]}, [\mx]
        bgt             480f
        add             \my, \my, #2
        vld1.32         {d2[]}, [\my]
        sub             \sr2, \src, #2
        sub             \src, \sr2, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2

        // 4x2, 4x4 hv
        vld1.16         {q11}, [\src], \s_strd
        vext.8          d24, d22, d23, #2
        vext.8          d25, d22, d23, #4
        vext.8          d23, d22, d23, #6
        vmull.s16       q10, d22, d0[0]
        vmlal.s16       q10, d24, d0[1]
        vmlal.s16       q10, d25, d0[2]
        vmlal.s16       q10, d23, d0[3]
        vrshl.s32       q10, q10, q14 // -(6-intermediate_bits)
        vmovn.i32       d17, q10

        bl              L(\type\()_8tap_filter_4)
        vmov            q9,  q12

4:
        bl              L(\type\()_8tap_filter_4)
        vmull.s16       q2,  d17, d2[0]
        vmlal.s16       q2,  d18, d2[1]
        vmlal.s16       q2,  d19, d2[2]
        vmlal.s16       q2,  d24, d2[3]
        vmull.s16       q3,  d18, d2[0]
        vmlal.s16       q3,  d19, d2[1]
        vmlal.s16       q3,  d24, d2[2]
        vmlal.s16       q3,  d25, d2[3]
.ifc \type, put
        vrshl.s32       q2,  q2,  q13 // -(6+intermediate_bits)
        vrshl.s32       q3,  q3,  q13 // -(6+intermediate_bits)
        vqmovun.s32     d4,  q2
        vqmovun.s32     d5,  q3
        vmin.u16        q2,  q2,  q15
.else
        vrshrn.i32      d4,  q2,  #6
        vrshrn.i32      d5,  q3,  #6
        vsub.i16        q2,  q2,  q13 // PREP_BIAS
.endif
        subs            \h,  \h,  #2

        vst1.16         {d4}, [\dst, :64], \d_strd
        vst1.16         {d5}, [\ds2, :64], \d_strd
        ble             0f
        vmov            d17, d19
        vmov            q9,  q12
        b               4b
0:
        pop             {r4-r11,pc}

480:    // 4x8, 4x16, 4x32 hv
        vpush           {d13-d15}
        vld1.8          {d2},  [\my, :64]
        sub             \src, \src, #2
        sub             \sr2, \src, \s_strd, lsl #1
        sub             \src, \sr2, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2

        vld1.16         {q11}, [\src], \s_strd
        vext.8          d24, d22, d23, #2
        vext.8          d25, d22, d23, #4
        vext.8          d23, d22, d23, #6
        vmull.s16       q10, d22, d0[0]
        vmlal.s16       q10, d24, d0[1]
        vmlal.s16       q10, d25, d0[2]
        vmlal.s16       q10, d23, d0[3]
        vrshl.s32       q10, q10, q14 // -(6-intermediate_bits)
        vmovn.i32       d13, q10

        bl              L(\type\()_8tap_filter_4)
        vmov            q7,  q12
        bl              L(\type\()_8tap_filter_4)
        vmov            q8,  q12
        bl              L(\type\()_8tap_filter_4)
        vmov            q9,  q12

48:
        bl              L(\type\()_8tap_filter_4)
        vmull.s16       q2,  d13, d2[0]
        vmlal.s16       q2,  d14, d2[1]
        vmlal.s16       q2,  d15, d2[2]
        vmlal.s16       q2,  d16, d2[3]
        vmlal.s16       q2,  d17, d3[0]
        vmlal.s16       q2,  d18, d3[1]
        vmlal.s16       q2,  d19, d3[2]
        vmlal.s16       q2,  d24, d3[3]
        vmull.s16       q3,  d14, d2[0]
        vmlal.s16       q3,  d15, d2[1]
        vmlal.s16       q3,  d16, d2[2]
        vmlal.s16       q3,  d17, d2[3]
        vmlal.s16       q3,  d18, d3[0]
        vmlal.s16       q3,  d19, d3[1]
        vmlal.s16       q3,  d24, d3[2]
        vmlal.s16       q3,  d25, d3[3]
.ifc \type, put
        vrshl.s32       q2,  q2,  q13 // -(6+intermediate_bits)
        vrshl.s32       q3,  q3,  q13 // -(6+intermediate_bits)
        vqmovun.s32     d4,  q2
        vqmovun.s32     d5,  q3
        vmin.u16        q2,  q2,  q15
.else
        vrshrn.i32      d4,  q2,  #6
        vrshrn.i32      d5,  q3,  #6
        vsub.i16        q2,  q2,  q13 // PREP_BIAS
.endif
        subs            \h,  \h,  #2
        vst1.16         {d4}, [\dst, :64], \d_strd
        vst1.16         {d5}, [\ds2, :64], \d_strd
        ble             0f
        vmov            d13, d15
        vmov            q7,  q8
        vmov            q8,  q9
        vmov            q9,  q12
        b               48b
0:
        vpop            {d13-d15}
        pop             {r4-r11,pc}

L(\type\()_8tap_filter_4):
        vld1.16         {q10}, [\sr2], \s_strd
        vld1.16         {q11}, [\src], \s_strd
        vext.8          d24, d20, d21, #2
        vext.8          d25, d20, d21, #4
        vext.8          d21, d20, d21, #6
        vmull.s16       q3,  d20, d0[0]
        vmlal.s16       q3,  d24, d0[1]
        vmlal.s16       q3,  d25, d0[2]
        vmlal.s16       q3,  d21, d0[3]
        vext.8          d24, d22, d23, #2
        vext.8          d25, d22, d23, #4
        vext.8          d23, d22, d23, #6
        vmull.s16       q10, d22, d0[0]
        vmlal.s16       q10, d24, d0[1]
        vmlal.s16       q10, d25, d0[2]
        vmlal.s16       q10, d23, d0[3]
        vrshl.s32       q3,  q3,  q14 // -(6-intermediate_bits)
        vrshl.s32       q10, q10, q14 // -(6-intermediate_bits)
        vmovn.i32       d24, q3
        vmovn.i32       d25, q10
        bx              lr

80:
160:
320:
        bgt             880f
        add             \my, \my, #2
        vld1.8          {d0},  [\mx, :64]
        vld1.32         {d2[]}, [\my]
        sub             \src,  \src,  #6
        sub             \src,  \src,  \s_strd
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2
        mov             \my, \h

164:    // 8x2, 8x4, 16x2, 16x4, 32x2, 32x4 hv
        add             \ds2, \dst, \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd, \d_strd, #1
        lsl             \s_strd, \s_strd, #1

        vld1.16         {q11, q12}, [\src], \s_strd
        vmull.s16       q2,  d22, d0[0]
        vmull.s16       q3,  d23, d0[0]
        vdup.32         q14, r12      // -(6-intermediate_bits)
.irpc i, 1234567
        vext.8          q10, q11, q12, #(2*\i)
.if \i < 4
        vmlal.s16       q2,  d20, d0[\i]
        vmlal.s16       q3,  d21, d0[\i]
.else
        vmlal.s16       q2,  d20, d1[\i - 4]
        vmlal.s16       q3,  d21, d1[\i - 4]
.endif
.endr
        vrshl.s32       q2,  q2,  q14 // -(6-intermediate_bits)
        vrshl.s32       q3,  q3,  q14 // -(6-intermediate_bits)
        vmovn.i32       d16, q2
        vmovn.i32       d17, q3

        bl              L(\type\()_8tap_filter_8)
        vmov            q9,  q11
        vmov            q10, q12

8:
        bl              L(\type\()_8tap_filter_8)
        vmull.s16       q2,  d16, d2[0]
        vmull.s16       q3,  d17, d2[0]
        vmull.s16       q13, d18, d2[0]
        vmull.s16       q14, d19, d2[0]
.ifc \type, put
        vdup.32         q8,  r8      // -(6+intermediate_bits)
.endif
        vmlal.s16       q2,  d18, d2[1]
        vmlal.s16       q3,  d19, d2[1]
        vmlal.s16       q13, d20, d2[1]
        vmlal.s16       q14, d21, d2[1]
        vmlal.s16       q2,  d20, d2[2]
        vmlal.s16       q3,  d21, d2[2]
        vmlal.s16       q13, d22, d2[2]
        vmlal.s16       q14, d23, d2[2]
        vmlal.s16       q2,  d22, d2[3]
        vmlal.s16       q3,  d23, d2[3]
        vmlal.s16       q13, d24, d2[3]
        vmlal.s16       q14, d25, d2[3]
.ifc \type, put
        vdup.16         q9,  \bdmax  // bitdepth_max
        vrshl.s32       q2,  q2,  q8 // -(6+intermediate_bits)
        vrshl.s32       q3,  q3,  q8 // -(6+intermediate_bits)
        vrshl.s32       q13, q13, q8 // -(6+intermediate_bits)
        vrshl.s32       q14, q14, q8 // -(6+intermediate_bits)
        vqmovun.s32     d4,  q2
        vqmovun.s32     d5,  q3
        vqmovun.s32     d6,  q13
        vqmovun.s32     d7,  q14
        vmin.u16        q2,  q2,  q15
        vmin.u16        q3,  q3,  q15
.else
        vmov.i16        q9,  #PREP_BIAS
        vrshrn.i32      d4,  q2,  #6
        vrshrn.i32      d5,  q3,  #6
        vrshrn.i32      d6,  q13, #6
        vrshrn.i32      d7,  q14, #6
        vsub.i16        q2,  q2,  q9 // PREP_BIAS
        vsub.i16        q3,  q3,  q9 // PREP_BIAS
.endif
        subs            \h,  \h,  #2
        vst1.16         {q2}, [\dst, :128], \d_strd
        vst1.16         {q3}, [\ds2, :128], \d_strd
        ble             9f
        vmov            q8,  q10
        vmov            q9,  q11
        vmov            q10, q12
        b               8b
9:
        subs            \w,  \w,  #8
        ble             0f
        asr             \s_strd,  \s_strd,  #1
        asr             \d_strd,  \d_strd,  #1
        mls             \src,  \s_strd,  \my,  \src
        mls             \dst,  \d_strd,  \my,  \dst
        sub             \src,  \src,  \s_strd,  lsl #2
        mov             \h,  \my
        add             \src,  \src,  #16
        add             \dst,  \dst,  #16
        b               164b
0:
        pop             {r4-r11,pc}

880:    // 8x8, 8x16, ..., 16x8, ..., 32x8, ... hv
640:
1280:
        vpush           {q4-q7}
        vld1.8          {d0}, [\mx, :64]
        vld1.8          {d2}, [\my, :64]
        sub             \src,  \src,  #6
        sub             \src,  \src,  \s_strd
        sub             \src,  \src,  \s_strd, lsl #1
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2
        mov             \my, \h

168:
        add             \ds2, \dst, \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd, \d_strd, #1
        lsl             \s_strd, \s_strd, #1

        vld1.16         {q11, q12}, [\src], \s_strd
        vmull.s16       q2,  d22, d0[0]
        vmull.s16       q3,  d23, d0[0]
        vdup.32         q14, r12      // -(6-intermediate_bits)
.irpc i, 1234567
        vext.8          q10, q11, q12, #(2*\i)
.if \i < 4
        vmlal.s16       q2,  d20, d0[\i]
        vmlal.s16       q3,  d21, d0[\i]
.else
        vmlal.s16       q2,  d20, d1[\i - 4]
        vmlal.s16       q3,  d21, d1[\i - 4]
.endif
.endr
        vrshl.s32       q2,  q2,  q14 // -(6-intermediate_bits)
        vrshl.s32       q3,  q3,  q14 // -(6-intermediate_bits)
        vmovn.i32       d8,  q2
        vmovn.i32       d9,  q3

        bl              L(\type\()_8tap_filter_8)
        vmov            q5,  q11
        vmov            q6,  q12
        bl              L(\type\()_8tap_filter_8)
        vmov            q7,  q11
        vmov            q8,  q12
        bl              L(\type\()_8tap_filter_8)
        vmov            q9,  q11
        vmov            q10, q12

88:
        bl              L(\type\()_8tap_filter_8)
        vmull.s16       q2,  d8,  d2[0]
        vmull.s16       q3,  d9,  d2[0]
        vmull.s16       q13, d10, d2[0]
        vmull.s16       q14, d11, d2[0]
.ifc \type, put
        vdup.32         q4,  r8      // -(6+intermediate_bits)
.endif
        vmlal.s16       q2,  d10, d2[1]
        vmlal.s16       q3,  d11, d2[1]
        vmlal.s16       q13, d12, d2[1]
        vmlal.s16       q14, d13, d2[1]
        vmlal.s16       q2,  d12, d2[2]
        vmlal.s16       q3,  d13, d2[2]
        vmlal.s16       q13, d14, d2[2]
        vmlal.s16       q14, d15, d2[2]
        vmlal.s16       q2,  d14, d2[3]
        vmlal.s16       q3,  d15, d2[3]
        vmlal.s16       q13, d16, d2[3]
        vmlal.s16       q14, d17, d2[3]
        vmlal.s16       q2,  d16, d3[0]
        vmlal.s16       q3,  d17, d3[0]
        vmlal.s16       q13, d18, d3[0]
        vmlal.s16       q14, d19, d3[0]
        vmlal.s16       q2,  d18, d3[1]
        vmlal.s16       q3,  d19, d3[1]
        vmlal.s16       q13, d20, d3[1]
        vmlal.s16       q14, d21, d3[1]
        vmlal.s16       q2,  d20, d3[2]
        vmlal.s16       q3,  d21, d3[2]
        vmlal.s16       q13, d22, d3[2]
        vmlal.s16       q14, d23, d3[2]
        vmlal.s16       q2,  d22, d3[3]
        vmlal.s16       q3,  d23, d3[3]
        vmlal.s16       q13, d24, d3[3]
        vmlal.s16       q14, d25, d3[3]
.ifc \type, put
        vrshl.s32       q2,  q2,  q4 // -(6+intermediate_bits)
        vrshl.s32       q3,  q3,  q4 // -(6+intermediate_bits)
        vrshl.s32       q13, q13, q4 // -(6+intermediate_bits)
        vrshl.s32       q14, q14, q4 // -(6+intermediate_bits)
        vqmovun.s32     d4,  q2
        vqmovun.s32     d5,  q3
        vqmovun.s32     d6,  q13
        vqmovun.s32     d7,  q14
        vmin.u16        q2,  q2,  q15
        vmin.u16        q3,  q3,  q15
.else
        vmov.i16        q5,  #PREP_BIAS
        vrshrn.i32      d4,  q2,  #6
        vrshrn.i32      d5,  q3,  #6
        vrshrn.i32      d6,  q13, #6
        vrshrn.i32      d7,  q14, #6
        vsub.i16        q2,  q2,  q5 // PREP_BIAS
        vsub.i16        q3,  q3,  q5 // PREP_BIAS
.endif
        subs            \h,  \h,  #2
        vst1.16         {q2}, [\dst, :128], \d_strd
        vst1.16         {q3}, [\ds2, :128], \d_strd
        ble             9f
        vmov            q4,  q6
        vmov            q5,  q7
        vmov            q6,  q8
        vmov            q7,  q9
        vmov            q8,  q10
        vmov            q9,  q11
        vmov            q10, q12
        b               88b
9:
        subs            \w,  \w,  #8
        ble             0f
        asr             \s_strd,  \s_strd,  #1
        asr             \d_strd,  \d_strd,  #1
        mls             \src,  \s_strd,  \my,  \src
        mls             \dst,  \d_strd,  \my,  \dst
        sub             \src,  \src,  \s_strd,  lsl #3
        mov             \h,  \my
        add             \src,  \src,  #16
        add             \dst,  \dst,  #16
        b               168b
0:
        vpop            {q4-q7}
        pop             {r4-r11,pc}

L(\type\()_8tap_filter_8):
        vld1.16         {q13, q14}, [\sr2], \s_strd
        vmull.s16       q2,  d26, d0[0]
        vmull.s16       q3,  d27, d0[0]
.irpc i, 1234567
        vext.8          q12, q13, q14, #(2*\i)
.if \i < 4
        vmlal.s16       q2,  d24, d0[\i]
        vmlal.s16       q3,  d25, d0[\i]
.else
        vmlal.s16       q2,  d24, d1[\i - 4]
        vmlal.s16       q3,  d25, d1[\i - 4]
.endif
.endr
        vdup.32         q12, r12      // -(6-intermediate_bits)
        vld1.16         {q13, q14}, [\src], \s_strd
        vrshl.s32       q2,  q2,  q12 // -(6-intermediate_bits)
        vrshl.s32       q3,  q3,  q12 // -(6-intermediate_bits)
        vmovn.i32       d4,  q2
        vmovn.i32       d5,  q3

        vmull.s16       q3,  d26, d0[0]
        vmull.s16       q11, d27, d0[0]
.irpc i, 1234567
        vext.8          q12, q13, q14, #(2*\i)
.if \i < 4
        vmlal.s16       q3,  d24, d0[\i]
        vmlal.s16       q11, d25, d0[\i]
.else
        vmlal.s16       q3,  d24, d1[\i - 4]
        vmlal.s16       q11, d25, d1[\i - 4]
.endif
.endr
        vdup.32         q13, r12      // -(6-intermediate_bits)
        vrshl.s32       q3,  q3,  q13 // -(6-intermediate_bits)
        vrshl.s32       q11, q11, q13 // -(6-intermediate_bits)

        vmovn.i32       d24, q3
        vmovn.i32       d25, q11
        vmov            q11, q2
        bx              lr
endfunc

function \type\()_bilin_16bpc_neon, export=1
        push            {r4-r11,lr}
        ldrd            r4,  r5,  [sp, #36]
        ldrd            r6,  r7,  [sp, #44]
.ifc \bdmax, r8
        ldr             r8,  [sp, #52]
.endif
        vdup.16         q1,  \mx
        vdup.16         q3,  \my
        rsb             r9,  \mx, #16
        rsb             r10, \my, #16
        vdup.16         q0,  r9
        vdup.16         q2,  r10
.ifc \type, prep
        lsl             \d_strd, \w, #1
.endif
        clz             \bdmax,   \bdmax       // bitdepth_max
        clz             r9,  \w
        sub             \bdmax,   \bdmax,  #18 // intermediate_bits = clz(bitdepth_max) - 18
        cmp             \mx, #0
        sub             r9,  r9,  #24
        rsb             r11, \bdmax, #4        // 4 - intermediate_bits
        add             r12, \bdmax, #4        // 4 + intermediate_bits
        bne             L(\type\()_bilin_h)
        cmp             \my, #0
        bne             L(\type\()_bilin_v)
        b               \type\()_neon

L(\type\()_bilin_h):
        cmp             \my, #0
        bne             L(\type\()_bilin_hv)

        adr             r10, L(\type\()_bilin_h_tbl)
        vdup.16         q15, r11               // 4 - intermediate_bits
        ldr             r9,  [r10, r9, lsl #2]
        vneg.s16        q15, q15               // -(4-intermediate_bits)
.ifc \type, put
        vdup.16         q14, \bdmax            // intermediate_bits
.else
        vmov.i16        q14, #PREP_BIAS
.endif
        add             r10, r10, r9
.ifc \type, put
        vneg.s16        q14, q14               // -intermediate_bits
.endif
        bx              r10

        .align 2
L(\type\()_bilin_h_tbl):
        .word 1280f - L(\type\()_bilin_h_tbl) + CONFIG_THUMB
        .word 640f  - L(\type\()_bilin_h_tbl) + CONFIG_THUMB
        .word 320f  - L(\type\()_bilin_h_tbl) + CONFIG_THUMB
        .word 160f  - L(\type\()_bilin_h_tbl) + CONFIG_THUMB
        .word 80f   - L(\type\()_bilin_h_tbl) + CONFIG_THUMB
        .word 40f   - L(\type\()_bilin_h_tbl) + CONFIG_THUMB
        .word 20f   - L(\type\()_bilin_h_tbl) + CONFIG_THUMB

20:     // 2xN h
.ifc \type, put
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
2:
        vld1.16         {d16}, [\src], \s_strd
        vld1.16         {d18}, [\sr2], \s_strd
        vext.8          d17, d16, d16, #2
        vext.8          d19, d18, d18, #2
        vtrn.32         d16, d18
        vtrn.32         d17, d19
        subs            \h,  \h,  #2
        vmul.i16        d16, d16, d0
        vmla.i16        d16, d17, d2
        vrshl.u16       d16, d16, d30
        vrshl.u16       d16, d16, d28
        vst1.32         {d16[0]}, [\dst, :32], \d_strd
        vst1.32         {d16[1]}, [\ds2, :32], \d_strd
        bgt             2b
        pop             {r4-r11,pc}
.endif

40:     // 4xN h
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
4:
        vld1.16         {q8},  [\src], \s_strd
        vld1.16         {q10}, [\sr2], \s_strd
        vext.8          q9,  q8,  q8,  #2
        vext.8          q11, q10, q10, #2
        vmov            d17, d20
        vmov            d19, d22
        subs            \h,  \h,  #2
        vmul.i16        q8,  q8,  q0
        vmla.i16        q8,  q9,  q1
        vrshl.u16       q8,  q8,  q15
.ifc \type, put
        vrshl.u16       q8,  q8,  q14
.else
        vsub.i16        q8,  q8,  q14
.endif
        vst1.16         {d16}, [\dst, :64], \d_strd
        vst1.16         {d17}, [\ds2, :64], \d_strd
        bgt             4b
        pop             {r4-r11,pc}

80:     // 8xN h
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
8:
        vld1.16         {d16, d17, d18}, [\src], \s_strd
        vld1.16         {d20, d21, d22}, [\sr2], \s_strd
        vext.8          q9,  q8,  q9,  #2
        vext.8          q11, q10, q11, #2
        subs            \h,  \h,  #2
        vmul.i16        q8,  q8,  q0
        vmla.i16        q8,  q9,  q1
        vmul.i16        q10, q10, q0
        vmla.i16        q10, q11, q1
        vrshl.u16       q8,  q8,  q15
        vrshl.u16       q10, q10, q15
.ifc \type, put
        vrshl.u16       q8,  q8,  q14
        vrshl.u16       q10, q10, q14
.else
        vsub.i16        q8,  q8,  q14
        vsub.i16        q10, q10, q14
.endif
        vst1.16         {q8},  [\dst, :128], \d_strd
        vst1.16         {q10}, [\ds2, :128], \d_strd
        bgt             8b
        pop             {r4-r11,pc}
160:
320:
640:
1280:   // 16xN, 32xN, ... h
        vpush           {q4-q7}
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1

        sub             \s_strd,  \s_strd,  \w, lsl #1
        sub             \s_strd,  \s_strd,  #16
.ifc \type, put
        lsl             \d_strd,  \d_strd,  #1
        sub             \d_strd,  \d_strd,  \w, lsl #1
.endif
161:
        vld1.16         {q4}, [\src]!
        vld1.16         {q9}, [\sr2]!
        mov             \mx, \w

16:
        vld1.16         {q5,  q6},  [\src]!
        vld1.16         {q10, q11}, [\sr2]!
        vext.8          q7,  q4,  q5,  #2
        vext.8          q8,  q5,  q6,  #2
        vext.8          q12, q9,  q10, #2
        vext.8          q13, q10, q11, #2
        vmul.i16        q4,  q4,  q0
        vmla.i16        q4,  q7,  q1
        vmul.i16        q5,  q5,  q0
        vmla.i16        q5,  q8,  q1
        vmul.i16        q9,  q9,  q0
        vmla.i16        q9,  q12, q1
        vmul.i16        q10, q10, q0
        vmla.i16        q10, q13, q1
        vrshl.u16       q4,  q4,  q15
        vrshl.u16       q5,  q5,  q15
        vrshl.u16       q9,  q9,  q15
        vrshl.u16       q10, q10, q15
        subs            \mx, \mx, #16
.ifc \type, put
        vrshl.u16       q4,  q4,  q14
        vrshl.u16       q5,  q5,  q14
        vrshl.u16       q9,  q9,  q14
        vrshl.u16       q10, q10, q14
.else
        vsub.i16        q4,  q4,  q14
        vsub.i16        q5,  q5,  q14
        vsub.i16        q9,  q9,  q14
        vsub.i16        q10, q10, q14
.endif
        vst1.16         {q4, q5},  [\dst, :128]!
        vst1.16         {q9, q10}, [\ds2, :128]!
        ble             9f

        vmov            q4,  q6
        vmov            q9,  q11
        b               16b

9:
        add             \dst,  \dst,  \d_strd
        add             \ds2,  \ds2,  \d_strd
        add             \src,  \src,  \s_strd
        add             \sr2,  \sr2,  \s_strd

        subs            \h,  \h,  #2
        bgt             161b
        vpop            {q4-q7}
        pop             {r4-r11,pc}


L(\type\()_bilin_v):
        cmp             \h,  #4
        adr             r10, L(\type\()_bilin_v_tbl)
.ifc \type, prep
        vdup.16         q15, r11      // 4 - intermediate_bits
.endif
        ldr             r9,  [r10, r9, lsl #2]
.ifc \type, prep
        vmov.i16        q14, #PREP_BIAS
        vneg.s16        q15, q15      // -(4-intermediate_bits)
.endif
        add             r10, r10, r9
        bx              r10

        .align 2
L(\type\()_bilin_v_tbl):
        .word 1280f - L(\type\()_bilin_v_tbl) + CONFIG_THUMB
        .word 640f  - L(\type\()_bilin_v_tbl) + CONFIG_THUMB
        .word 320f  - L(\type\()_bilin_v_tbl) + CONFIG_THUMB
        .word 160f  - L(\type\()_bilin_v_tbl) + CONFIG_THUMB
        .word 80f   - L(\type\()_bilin_v_tbl) + CONFIG_THUMB
        .word 40f   - L(\type\()_bilin_v_tbl) + CONFIG_THUMB
        .word 20f   - L(\type\()_bilin_v_tbl) + CONFIG_THUMB

20:     // 2xN v
.ifc \type, put
        cmp             \h,  #2
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        lsl             \d_strd,  \d_strd,  #1

        // 2x2 v
        vld1.32         {d16[]}, [\src], \s_strd
        bgt             24f
        vld1.32         {d17[]}, [\sr2], \s_strd
        vld1.32         {d18[]}, [\src], \s_strd
        vext.8          d16, d16, d17, #4
        vext.8          d17, d17, d18, #4
        vmul.i16        d16, d16, d4
        vmla.i16        d16, d17, d6
        vrshr.u16       d16, d16, #4
        vst1.32         {d16[0]}, [\dst, :32]
        vst1.32         {d16[1]}, [\ds2, :32]
        pop             {r4-r11,pc}
24:     // 2x4, 2x8, ... v
        vld1.32         {d17[]}, [\sr2], \s_strd
        vld1.32         {d18[]}, [\src], \s_strd
        vld1.32         {d19[]}, [\sr2], \s_strd
        vld1.32         {d20[]}, [\src], \s_strd
        vext.8          d16, d16, d17, #4
        vext.8          d17, d17, d18, #4
        vext.8          d18, d18, d19, #4
        vext.8          d19, d19, d20, #4
        vswp            d17, d18
        vmul.i16        q8,  q8,  q2
        vmla.i16        q8,  q9,  q3
        subs            \h,  \h,  #4
        vrshr.u16       q8,  q8,  #4
        vst1.32         {d16[0]}, [\dst, :32], \d_strd
        vst1.32         {d16[1]}, [\ds2, :32], \d_strd
        vst1.32         {d17[0]}, [\dst, :32], \d_strd
        vst1.32         {d17[1]}, [\ds2, :32], \d_strd
        ble             0f
        vmov            d16, d20
        b               24b
0:
        pop             {r4-r11,pc}
.endif

40:     // 4xN v
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        lsl             \d_strd,  \d_strd,  #1
        vld1.16         {d16}, [\src], \s_strd
4:
        vld1.16         {d17}, [\sr2], \s_strd
        vld1.16         {d19}, [\src], \s_strd
        vmov            d18, d17
        vmul.i16        q8,  q8,  q2
        vmla.i16        q8,  q9,  q3
        subs            \h,  \h,  #2
.ifc \type, put
        vrshr.u16       q8,  q8,  #4
.else
        vrshl.u16       q8,  q8,  q15
        vsub.i16        q8,  q8,  q14
.endif
        vst1.16         {d16}, [\dst, :64], \d_strd
        vst1.16         {d17}, [\ds2, :64], \d_strd
        ble             0f
        vmov            d16, d19
        b               4b
0:
        pop             {r4-r11,pc}

80:     // 8xN v
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        lsl             \d_strd,  \d_strd,  #1
        vld1.16         {q8},  [\src], \s_strd
8:
        vld1.16         {q9},  [\sr2], \s_strd
        vld1.16         {q10}, [\src], \s_strd
        vmul.i16        q8,  q8,  q2
        vmla.i16        q8,  q9,  q3
        vmul.i16        q9,  q9,  q2
        vmla.i16        q9,  q10, q3
        subs            \h,  \h,  #2
.ifc \type, put
        vrshr.u16       q8,  q8,  #4
        vrshr.u16       q9,  q9,  #4
.else
        vrshl.u16       q8,  q8,  q15
        vrshl.u16       q9,  q9,  q15
        vsub.i16        q8,  q8,  q14
        vsub.i16        q9,  q9,  q14
.endif
        vst1.16         {q8}, [\dst, :128], \d_strd
        vst1.16         {q9}, [\ds2, :128], \d_strd
        ble             0f
        vmov            q8,  q10
        b               8b
0:
        pop             {r4-r11,pc}

160:    // 16xN, 32xN, ...
320:
640:
1280:
        mov             \my, \h
1:
        add             \ds2, \dst, \d_strd
        add             \sr2, \src, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        vld1.16         {q8,  q9},  [\src], \s_strd
2:
        vld1.16         {q10, q11}, [\sr2], \s_strd
        vld1.16         {q12, q13}, [\src], \s_strd
        vmul.i16        q8,  q8,  q2
        vmla.i16        q8,  q10, q3
        vmul.i16        q9,  q9,  q2
        vmla.i16        q9,  q11, q3
        vmul.i16        q10, q10, q2
        vmla.i16        q10, q12, q3
        vmul.i16        q11, q11, q2
        vmla.i16        q11, q13, q3
        subs            \h,  \h,  #2
.ifc \type, put
        vrshr.u16       q8,  q8,  #4
        vrshr.u16       q9,  q9,  #4
        vrshr.u16       q10, q10, #4
        vrshr.u16       q11, q11, #4
.else
        vrshl.u16       q8,  q8,  q15
        vrshl.u16       q9,  q9,  q15
        vrshl.u16       q10, q10, q15
        vrshl.u16       q11, q11, q15
        vsub.i16        q8,  q8,  q14
        vsub.i16        q9,  q9,  q14
        vsub.i16        q10, q10, q14
        vsub.i16        q11, q11, q14
.endif
        vst1.16         {q8,  q9},  [\dst, :128], \d_strd
        vst1.16         {q10, q11}, [\ds2, :128], \d_strd
        ble             9f
        vmov            q8,  q12
        vmov            q9,  q13
        b               2b
9:
        subs            \w,  \w,  #16
        ble             0f
        asr             \s_strd, \s_strd, #1
        asr             \d_strd, \d_strd, #1
        mls             \src, \s_strd, \my, \src
        mls             \dst, \d_strd, \my, \dst
        sub             \src, \src, \s_strd, lsl #1
        mov             \h,  \my
        add             \src, \src, #32
        add             \dst, \dst, #32
        b               1b
0:
        pop             {r4-r11,pc}

L(\type\()_bilin_hv):
        adr             r10, L(\type\()_bilin_hv_tbl)
        vdup.16         q15, r11          // 4 - intermediate_bits
        ldr             r9,  [r10, r9, lsl #2]
        vneg.s16        q15, q15          // -(4-intermediate_bits)
.ifc \type, put
        vdup.32         q14, r12          // 4 + intermediate_bits
.else
        vmov.i16        q14, #PREP_BIAS
.endif
        add             r10, r10, r9
.ifc \type, put
        vneg.s32        q14, q14          // -(4+intermediate_bits)
.endif
        bx              r10

        .align 2
L(\type\()_bilin_hv_tbl):
        .word 1280f - L(\type\()_bilin_hv_tbl) + CONFIG_THUMB
        .word 640f  - L(\type\()_bilin_hv_tbl) + CONFIG_THUMB
        .word 320f  - L(\type\()_bilin_hv_tbl) + CONFIG_THUMB
        .word 160f  - L(\type\()_bilin_hv_tbl) + CONFIG_THUMB
        .word 80f   - L(\type\()_bilin_hv_tbl) + CONFIG_THUMB
        .word 40f   - L(\type\()_bilin_hv_tbl) + CONFIG_THUMB
        .word 20f   - L(\type\()_bilin_hv_tbl) + CONFIG_THUMB

20:     // 2xN hv
.ifc \type, put
        add             \sr2, \src, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        vld1.16         {d20}, [\src], \s_strd
        vext.8          d21, d20, d20, #2
        vmul.i16        d16, d20, d0
        vmla.i16        d16, d21, d2
        vrshl.u16       d16, d16, d30
        vext.8          d16, d16, d16, #4

2:
        vld1.16         {d20}, [\sr2], \s_strd
        vld1.16         {d22}, [\src], \s_strd
        vext.8          d21, d20, d20, #2
        vext.8          d23, d22, d22, #2
        vtrn.32         d20, d22
        vtrn.32         d21, d23
        vmul.i16        d18, d20, d0
        vmla.i16        d18, d21, d2
        vrshl.u16       d18, d18, d30

        vext.8          d16, d16, d18, #4

        vmull.u16       q8,  d16, d4
        vmlal.u16       q8,  d18, d6
        vrshl.u32       q8,  q8,  q14
        vmovn.i32       d16, q8
        subs            \h,  \h,  #2
        vst1.32         {d16[0]}, [\dst, :32], \d_strd
        vst1.32         {d16[1]}, [\ds2, :32], \d_strd
        ble             0f
        vmov            d16, d18
        b               2b
0:
        pop             {r4-r11,pc}
.endif

40:     // 4xN hv
        add             \sr2, \src, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        vld1.16         {q10}, [\src], \s_strd
        vext.8          d21, d20, d21, #2
        vmul.i16        d16, d20, d0
        vmla.i16        d16, d21, d2
        vrshl.u16       d16, d16, d30

4:
        vld1.16         {q10}, [\sr2], \s_strd
        vld1.16         {q11}, [\src], \s_strd
        vext.8          d21, d20, d21, #2
        vext.8          d23, d22, d23, #2
        vswp            d21, d22
        vmul.i16        q9,  q10, q0
        vmla.i16        q9,  q11, q1
        vrshl.u16       q9,  q9,  q15

        vmull.u16       q10, d16, d4
        vmlal.u16       q10, d18, d6
        vmull.u16       q11, d18, d4
        vmlal.u16       q11, d19, d6
.ifc \type, put
        vrshl.u32       q10, q10, q14
        vrshl.u32       q11, q11, q14
        vmovn.i32       d20, q10
        vmovn.i32       d21, q11
.else
        vrshrn.i32      d20, q10, #4
        vrshrn.i32      d21, q11, #4
        vsub.i16        q10, q10, q14
.endif
        subs            \h,  \h,  #2
        vst1.16         {d20}, [\dst, :64], \d_strd
        vst1.16         {d21}, [\ds2, :64], \d_strd
        ble             0f
        vmov            d16, d19
        b               4b
0:
        pop             {r4-r11,pc}

80:     // 8xN, 16xN, ... hv
160:
320:
640:
1280:
        mov             \my, \h

1:
        add             \sr2, \src, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        vld1.16         {d20, d21, d22}, [\src], \s_strd
        vext.8          q11, q10, q11, #2
        vmul.i16        q8,  q10, q0
        vmla.i16        q8,  q11, q1
        vrshl.u16       q8,  q8,  q15

2:
        vld1.16         {d20, d21, d22}, [\sr2], \s_strd
        vld1.16         {d24, d25, d26}, [\src], \s_strd
        vext.8          q11, q10, q11, #2
        vext.8          q13, q12, q13, #2
        vmul.i16        q9,  q10, q0
        vmla.i16        q9,  q11, q1
        vmul.i16        q10, q12, q0
        vmla.i16        q10, q13, q1
        vrshl.u16       q9,  q9,  q15
        vrshl.u16       q10, q10, q15

        vmull.u16       q11, d16, d4
        vmlal.u16       q11, d18, d6
        vmull.u16       q12, d17, d4
        vmlal.u16       q12, d19, d6
        vmull.u16       q8,  d18, d4
        vmlal.u16       q8,  d20, d6
        vmull.u16       q9,  d19, d4
        vmlal.u16       q9,  d21, d6
.ifc \type, put
        vrshl.u32       q11, q11, q14
        vrshl.u32       q12, q12, q14
        vrshl.u32       q8,  q8,  q14
        vrshl.u32       q9,  q9,  q14
        vmovn.i32       d22, q11
        vmovn.i32       d23, q12
        vmovn.i32       d16, q8
        vmovn.i32       d17, q9
.else
        vrshrn.i32      d22, q11, #4
        vrshrn.i32      d23, q12, #4
        vrshrn.i32      d16, q8,  #4
        vrshrn.i32      d17, q9,  #4
        vsub.i16        q11, q11, q14
        vsub.i16        q8,  q8,  q14
.endif
        subs            \h,  \h,  #2
        vst1.16         {q11}, [\dst, :128], \d_strd
        vst1.16         {q8},  [\ds2, :128], \d_strd
        ble             9f
        vmov            q8,  q10
        b               2b
9:
        subs            \w,  \w,  #8
        ble             0f
        asr             \s_strd,  \s_strd,  #1
        asr             \d_strd,  \d_strd,  #1
        mls             \src,  \s_strd,  \my,  \src
        mls             \dst,  \d_strd,  \my,  \dst
        sub             \src,  \src,  \s_strd,  lsl #1
        mov             \h,  \my
        add             \src,  \src,  #16
        add             \dst,  \dst,  #16
        b               1b
0:
        pop             {r4-r11,pc}
endfunc
.endm

filter_fn put,  r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10
filter_fn prep, r0, r8, r1, r2, r3, r4, r5, r6, r7, r9, r10
