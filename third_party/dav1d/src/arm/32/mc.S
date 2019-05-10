/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Janne Grunau
 * Copyright © 2018, Martin Storsjo
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

.macro avg dst0, dst1, t0, t1, t2, t3
        vld1.16         {\t0,\t1},   [r2, :128]!
        vld1.16         {\t2,\t3},   [r3, :128]!
        vadd.i16        \t0,   \t0,  \t2
        vadd.i16        \t1,   \t1,  \t3
        vqrshrun.s16    \dst0, \t0,  #5
        vqrshrun.s16    \dst1, \t1,  #5
.endm

.macro w_avg dst0, dst1, t0, t1, t2, t3
        vld1.16         {\t0,\t1},   [r2, :128]!
        vld1.16         {\t2,\t3},   [r3, :128]!
        vsub.i16        \t0,   \t2,  \t0
        vsub.i16        \t1,   \t3,  \t1
        vqdmulh.s16     \t0,   \t0,  q15
        vqdmulh.s16     \t1,   \t1,  q15
        vadd.i16        \t0,   \t2,  \t0
        vadd.i16        \t1,   \t3,  \t1
        vqrshrun.s16    \dst0, \t0,  #4
        vqrshrun.s16    \dst1, \t1,  #4
.endm

.macro mask dst0, dst1, t0, t1, t2, t3
        vld1.8          {q14}, [lr, :128]!
        vld1.16         {\t0,\t1},   [r2, :128]!
        vmul.i8         q14,   q14,  q15
        vld1.16         {\t2,\t3},   [r3, :128]!
        vshll.i8        q13,   d28,  #8
        vshll.i8        q14,   d29,  #8
        vsub.i16        \t0,   \t2,  \t0
        vsub.i16        \t1,   \t3,  \t1
        vqdmulh.s16     \t0,   \t0,  q13
        vqdmulh.s16     \t1,   \t1,  q14
        vadd.i16        \t0,   \t2,  \t0
        vadd.i16        \t1,   \t3,  \t1
        vqrshrun.s16    \dst0, \t0,  #4
        vqrshrun.s16    \dst1, \t1,  #4
.endm

.macro bidir_fn type
function \type\()_8bpc_neon, export=1
        push            {r4-r6,lr}
        ldr             r4, [sp, #16]
        ldr             r5, [sp, #20]
        clz             r4,  r4
.ifnc \type, avg
        ldr             lr, [sp, #24]
.endif
.ifc \type, w_avg
        vdup.s16        q15, lr
        vneg.s16        q15, q15
        vshl.i16        q15, q15, #11
.endif
.ifc \type, mask
        vmov.i8         q15, #256-2
.endif
        adr             r12, L(\type\()_tbl)
        sub             r4,  r4,  #24
        ldr             r4,  [r12, r4, lsl #2]
        \type           d16, d17, q0,  q1,  q2,  q3
        add             r12, r12, r4
        bx              r12
        .align 2
L(\type\()_tbl):
        .word 1280f - L(\type\()_tbl) + CONFIG_THUMB
        .word 640f  - L(\type\()_tbl) + CONFIG_THUMB
        .word 320f  - L(\type\()_tbl) + CONFIG_THUMB
        .word 160f  - L(\type\()_tbl) + CONFIG_THUMB
        .word 80f   - L(\type\()_tbl) + CONFIG_THUMB
        .word 4f    - L(\type\()_tbl) + CONFIG_THUMB
4:
        add             r6,  r0,  r1
        lsl             r1,  r1,  #1
        cmp             r5,  #4
        vst1.32         {d16[0]},  [r0, :32], r1
        vst1.32         {d16[1]},  [r6, :32], r1
        vst1.32         {d17[0]},  [r0, :32], r1
        vst1.32         {d17[1]},  [r6, :32], r1
        beq             0f
        \type           d18, d19,  q0,  q1,  q2,  q3
        cmp             r5,  #8
        vst1.32         {d18[0]},  [r0, :32], r1
        vst1.32         {d18[1]},  [r6, :32], r1
        vst1.32         {d19[0]},  [r0, :32], r1
        vst1.32         {d19[1]},  [r6, :32], r1
        beq             0f
        \type           d16, d17, q0,  q1,  q2,  q3
        vst1.32         {d16[0]},  [r0, :32], r1
        vst1.32         {d16[1]},  [r6, :32], r1
        \type           d18, d19,  q0,  q1,  q2,  q3
        vst1.32         {d17[0]},  [r0, :32], r1
        vst1.32         {d17[1]},  [r6, :32], r1
        vst1.32         {d18[0]},  [r0, :32], r1
        vst1.32         {d18[1]},  [r6, :32], r1
        vst1.32         {d19[0]},  [r0, :32], r1
        vst1.32         {d19[1]},  [r6, :32], r1
        pop             {r4-r6,pc}
80:
        add             r6,  r0,  r1
        lsl             r1,  r1,  #1
8:
        vst1.8          {d16},  [r0, :64], r1
        \type           d18, d19, q0,  q1,  q2,  q3
        vst1.8          {d17},  [r6, :64], r1
        vst1.8          {d18},  [r0, :64], r1
        subs            r5,  r5,  #4
        vst1.8          {d19},  [r6, :64], r1
        ble             0f
        \type           d16, d17, q0,  q1,  q2,  q3
        b               8b
160:
        add             r6,  r0,  r1
        lsl             r1,  r1,  #1
16:
        \type           d18, d19, q0, q1, q2, q3
        vst1.8          {q8},  [r0, :128], r1
        \type           d20, d21, q0, q1, q2, q3
        vst1.8          {q9},  [r6, :128], r1
        \type           d22, d23, q0, q1, q2, q3
        vst1.8          {q10}, [r0, :128], r1
        subs            r5,  r5,  #4
        vst1.8          {q11}, [r6, :128], r1
        ble             0f
        \type           d16, d17, q0, q1, q2, q3
        b               16b
320:
        add             r6,  r0,  r1
        lsl             r1,  r1,  #1
32:
        \type           d18, d19, q0, q1, q2, q3
        \type           d20, d21, q0, q1, q2, q3
        vst1.8          {q8,  q9},  [r0, :128], r1
        \type           d22, d23, q0, q1, q2, q3
        subs            r5,  r5,  #2
        vst1.8          {q10, q11}, [r6, :128], r1
        ble             0f
        \type           d16, d17, q0, q1, q2, q3
        b               32b
640:
        add             r6,  r0,  #32
64:
        \type           d18, d19, q0, q1, q2, q3
        \type           d20, d21, q0, q1, q2, q3
        \type           d22, d23, q0, q1, q2, q3
        vst1.8          {q8,  q9},  [r0, :128], r1
        \type           d16, d17, q0, q1, q2, q3
        vst1.8          {q10, q11}, [r6, :128], r1
        \type           d18, d19, q0, q1, q2, q3
        \type           d20, d21, q0, q1, q2, q3
        vst1.8          {q8,  q9},  [r0, :128], r1
        \type           d22, d23, q0, q1, q2, q3
        subs            r5,  r5,  #2
        vst1.8          {q10, q11}, [r6, :128], r1
        ble             0f
        \type           d16, d17, q0, q1, q2, q3
        b               64b
1280:
        sub             r1,  r1,  #32
        add             r6,  r0,  #64
128:
        \type           d18, d19, q0, q1, q2, q3
        \type           d20, d21, q0, q1, q2, q3
        \type           d22, d23, q0, q1, q2, q3
        vst1.8          {q8,  q9},  [r0, :128]!
        \type           d16, d17, q0, q1, q2, q3
        vst1.8          {q10, q11}, [r0, :128], r1
        \type           d18, d19, q0, q1, q2, q3
        \type           d20, d21, q0, q1, q2, q3
        vst1.8          {q8,  q9},  [r6, :128]!
        \type           d22, d23, q0, q1, q2, q3
        subs            r5,  r5,  #1
        vst1.8          {q10, q11}, [r6, :128], r1
        ble             0f
        \type           d16, d17, q0, q1, q2, q3
        b               128b

0:
        pop             {r4-r6,pc}
endfunc
.endm

bidir_fn avg
bidir_fn w_avg
bidir_fn mask


// This has got the same signature as the put_8tap functions,
// assumes that the caller has loaded the h argument into r5,
// and assumes that r8 is set to (clz(w)-24).
function put_neon
        adr             r9,  L(put_tbl)
        ldr             r8,  [r9, r8, lsl #2]
        add             r9,  r9,  r8
        bx              r9

        .align 2
L(put_tbl):
        .word 1280f - L(put_tbl) + CONFIG_THUMB
        .word 640f  - L(put_tbl) + CONFIG_THUMB
        .word 32f   - L(put_tbl) + CONFIG_THUMB
        .word 160f  - L(put_tbl) + CONFIG_THUMB
        .word 8f    - L(put_tbl) + CONFIG_THUMB
        .word 4f    - L(put_tbl) + CONFIG_THUMB
        .word 2f    - L(put_tbl) + CONFIG_THUMB

2:
        vld1.16         {d0[]}, [r2], r3
        vld1.16         {d1[]}, [r2], r3
        subs            r5,  r5,  #2
        vst1.16         {d0[0]}, [r0, :16], r1
        vst1.16         {d1[0]}, [r0, :16], r1
        bgt             2b
        pop             {r4-r11,pc}
4:
        vld1.32         {d0[]}, [r2], r3
        vld1.32         {d1[]}, [r2], r3
        subs            r5,  r5,  #2
        vst1.32         {d0[0]}, [r0, :32], r1
        vst1.32         {d1[0]}, [r0, :32], r1
        bgt             4b
        pop             {r4-r11,pc}
8:
        vld1.8          {d0}, [r2], r3
        vld1.8          {d1}, [r2], r3
        subs            r5,  r5,  #2
        vst1.8          {d0}, [r0, :64], r1
        vst1.8          {d1}, [r0, :64], r1
        bgt             8b
        pop             {r4-r11,pc}
160:
        add             r8,  r0,  r1
        lsl             r1,  r1,  #1
        add             r9,  r2,  r3
        lsl             r3,  r3,  #1
16:
        vld1.8          {q0}, [r2], r3
        vld1.8          {q1}, [r9], r3
        subs            r5,  r5,  #2
        vst1.8          {q0}, [r0, :128], r1
        vst1.8          {q1}, [r8, :128], r1
        bgt             16b
        pop             {r4-r11,pc}
32:
        vld1.8          {q0,  q1},  [r2], r3
        subs            r5,  r5,  #1
        vst1.8          {q0,  q1},  [r0, :128], r1
        bgt             32b
        pop             {r4-r11,pc}
640:
        sub             r1,  r1,  #32
        sub             r3,  r3,  #32
64:
        vld1.8          {q0,  q1},  [r2]!
        vst1.8          {q0,  q1},  [r0, :128]!
        vld1.8          {q2,  q3},  [r2], r3
        subs            r5,  r5,  #1
        vst1.8          {q2,  q3},  [r0, :128], r1
        bgt             64b
        pop             {r4-r11,pc}
1280:
        sub             r1,  r1,  #96
        sub             r3,  r3,  #96
128:
        vld1.8          {q8,  q9},  [r2]!
        vst1.8          {q8,  q9},  [r0, :128]!
        vld1.8          {q10, q11}, [r2]!
        vst1.8          {q10, q11}, [r0, :128]!
        vld1.8          {q12, q13}, [r2]!
        vst1.8          {q12, q13}, [r0, :128]!
        vld1.8          {q14, q15}, [r2], r3
        subs            r5,  r5,  #1
        vst1.8          {q14, q15}, [r0, :128], r1
        bgt             128b
        pop             {r4-r11,pc}
endfunc


// This has got the same signature as the put_8tap functions,
// assumes that the caller has loaded the h argument into r4,
// and assumes that r8 is set to (clz(w)-24), and r7 to w*2.
function prep_neon
        adr             r9,  L(prep_tbl)
        ldr             r8,  [r9, r8, lsl #2]
        add             r9,  r9,  r8
        bx              r9

        .align 2
L(prep_tbl):
        .word 1280f - L(prep_tbl) + CONFIG_THUMB
        .word 640f  - L(prep_tbl) + CONFIG_THUMB
        .word 320f  - L(prep_tbl) + CONFIG_THUMB
        .word 160f  - L(prep_tbl) + CONFIG_THUMB
        .word 8f    - L(prep_tbl) + CONFIG_THUMB
        .word 4f    - L(prep_tbl) + CONFIG_THUMB

4:
        vld1.32         {d0[]}, [r1], r2
        vld1.32         {d2[]}, [r1], r2
        subs            r4,  r4,  #2
        vshll.u8        q0,  d0,  #4
        vshll.u8        q1,  d2,  #4
        vst1.16         {d1, d2}, [r0, :64]!
        bgt             4b
        pop             {r4-r11,pc}
8:
        vld1.8          {d0}, [r1], r2
        vld1.8          {d2}, [r1], r2
        subs            r4,  r4,  #2
        vshll.u8        q0,  d0,  #4
        vshll.u8        q1,  d2,  #4
        vst1.16         {q0, q1}, [r0, :128]!
        bgt             8b
        pop             {r4-r11,pc}
160:
        add             r9,  r1,  r2
        lsl             r2,  r2,  #1
        add             r8,  r0,  r7
        lsl             r7,  r7,  #1
16:
        vld1.8          {q2}, [r1], r2
        vld1.8          {q3}, [r9], r2
        subs            r4,  r4,  #2
        vshll.u8        q0,  d4,  #4
        vshll.u8        q1,  d5,  #4
        vshll.u8        q2,  d6,  #4
        vshll.u8        q3,  d7,  #4
        vst1.16         {q0, q1}, [r0, :128], r7
        vst1.16         {q2, q3}, [r8, :128], r7
        bgt             16b
        pop             {r4-r11,pc}
320:
        add             r8,  r0,  r3
32:
        vld1.8          {q0,  q1},  [r1], r2
        subs            r4,  r4,  #2
        vshll.u8        q8,  d0,  #4
        vshll.u8        q9,  d1,  #4
        vld1.8          {q2,  q3},  [r1], r2
        vshll.u8        q10, d2,  #4
        vshll.u8        q11, d3,  #4
        vshll.u8        q12, d4,  #4
        vst1.16         {q8,  q9},  [r0, :128], r7
        vshll.u8        q13, d5,  #4
        vst1.16         {q10, q11}, [r8, :128], r7
        vshll.u8        q14, d6,  #4
        vst1.16         {q12, q13}, [r0, :128], r7
        vshll.u8        q15, d7,  #4
        vst1.16         {q14, q15}, [r8, :128], r7
        bgt             32b
        pop             {r4-r11,pc}
640:
        sub             r2,  r2,  #32
        add             r8,  r0,  #32
        mov             r6,  #64
64:
        vld1.8          {q0,  q1},  [r1]!
        subs            r4,  r4,  #1
        vshll.u8        q8,  d0,  #4
        vshll.u8        q9,  d1,  #4
        vld1.8          {q2,  q3},  [r1], r2
        vshll.u8        q10, d2,  #4
        vshll.u8        q11, d3,  #4
        vshll.u8        q12, d4,  #4
        vst1.16         {q8,  q9},  [r0, :128], r6
        vshll.u8        q13, d5,  #4
        vshll.u8        q14, d6,  #4
        vst1.16         {q10, q11}, [r8, :128], r6
        vshll.u8        q15, d7,  #4
        vst1.16         {q12, q13}, [r0, :128], r6
        vst1.16         {q14, q15}, [r8, :128], r6
        bgt             64b
        pop             {r4-r11,pc}
1280:
        sub             r2,  r2,  #96
        add             r8,  r0,  #32
        mov             r6,  #64
128:
        vld1.8          {q0,  q1},  [r1]!
        vld1.8          {q2,  q3},  [r1]!
        vshll.u8        q10, d0,  #4
        vshll.u8        q11, d1,  #4
        vshll.u8        q12, d2,  #4
        vshll.u8        q13, d3,  #4
        vshll.u8        q14, d4,  #4
        vshll.u8        q15, d5,  #4
        vld1.8          {q8,  q9},  [r1]!
        vst1.16         {q10, q11}, [r0, :128], r6
        vst1.16         {q12, q13}, [r8, :128], r6
        vshll.u8        q0,  d6,  #4
        vshll.u8        q1,  d7,  #4
        vshll.u8        q2,  d16, #4
        vshll.u8        q3,  d17, #4
        vshll.u8        q8,  d18, #4
        vshll.u8        q9,  d19, #4
        vld1.8          {q10, q11}, [r1], r2
        vst1.16         {q14, q15}, [r0, :128], r6
        vst1.16         {q0,  q1},  [r8, :128], r6
        vshll.u8        q12, d20, #4
        vshll.u8        q13, d21, #4
        vshll.u8        q14, d22, #4
        vshll.u8        q15, d23, #4
        subs            r4,  r4,  #1
        vst1.16         {q2,  q3},  [r0, :128], r6
        vst1.16         {q8,  q9},  [r8, :128], r6
        vst1.16         {q12, q13}, [r0, :128], r6
        vst1.16         {q14, q15}, [r8, :128], r6
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
        vld1.8          {\d0}, [\s0], \strd
        vld1.8          {\d1}, [\s1], \strd
.ifnb \d2
        vld1.8          {\d2}, [\s0], \strd
        vld1.8          {\d3}, [\s1], \strd
.endif
.ifnb \d4
        vld1.8          {\d4}, [\s0], \strd
.endif
.ifnb \d5
        vld1.8          {\d5}, [\s1], \strd
.endif
.ifnb \d6
        vld1.8          {\d6}, [\s0], \strd
.endif
.endm
.macro load_16 s0, s1, strd, d0, d1, d2, d3, d4, d5, d6
        load_slice      \s0, \s1, \strd, 16, \d0, \d1, \d2, \d3, \d4, \d5, \d6
.endm
.macro load_32 s0, s1, strd, d0, d1, d2, d3, d4, d5, d6
        load_slice      \s0, \s1, \strd, 32, \d0, \d1, \d2, \d3, \d4, \d5, \d6
.endm
.macro interleave_1_16 r0, r1, r2, r3, r4
        vext.8          \r0, \r0, \r1, #6
        vext.8          \r1, \r1, \r2, #6
.ifnb \r3
        vext.8          \r2, \r2, \r3, #6
        vext.8          \r3, \r3, \r4, #6
.endif
.endm
.macro interleave_1_32 r0, r1, r2, r3, r4
        vext.8          \r0, \r0, \r1, #4
        vext.8          \r1, \r1, \r2, #4
.ifnb \r3
        vext.8          \r2, \r2, \r3, #4
        vext.8          \r3, \r3, \r4, #4
.endif
.endm
.macro vmovl_u8 q0, d0, q1, d1, q2, d2, q3, d3, q4, d4, q5, d5, q6, d6
        vmovl.u8        \q0, \d0
        vmovl.u8        \q1, \d1
.ifnb \q2
        vmovl.u8        \q2, \d2
        vmovl.u8        \q3, \d3
.endif
.ifnb \q4
        vmovl.u8        \q4, \d4
.endif
.ifnb \q5
        vmovl.u8        \q5, \d5
.endif
.ifnb \q6
        vmovl.u8        \q6, \d6
.endif
.endm
.macro mul_mla_4 d, s0, s1, s2, s3
        vmul.s16        \d,  \s0,  d0[0]
        vmla.s16        \d,  \s1,  d0[1]
        vmla.s16        \d,  \s2,  d0[2]
        vmla.s16        \d,  \s3,  d0[3]
.endm
.macro mul_mla_8_1 d0, d1, s0, s1, s2, s3, s4, s5, s6, s7, s8
        vmul.s16        \d0, \s0, d0[0]
        vmla.s16        \d0, \s1, d0[1]
        vmla.s16        \d0, \s2, d0[2]
        vmla.s16        \d0, \s3, d0[3]
        vmla.s16        \d0, \s4, d1[0]
        vmla.s16        \d0, \s5, d1[1]
        vmla.s16        \d0, \s6, d1[2]
        vmla.s16        \d0, \s7, d1[3]
        vmul.s16        \d1, \s1, d0[0]
        vmla.s16        \d1, \s2, d0[1]
        vmla.s16        \d1, \s3, d0[2]
        vmla.s16        \d1, \s4, d0[3]
        vmla.s16        \d1, \s5, d1[0]
        vmla.s16        \d1, \s6, d1[1]
        vmla.s16        \d1, \s7, d1[2]
        vmla.s16        \d1, \s8, d1[3]
.endm
.macro mul_mla_8_2 d0, d1, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9
        vmul.s16        \d0, \s0, d0[0]
        vmla.s16        \d0, \s1, d0[1]
        vmla.s16        \d0, \s2, d0[2]
        vmla.s16        \d0, \s3, d0[3]
        vmla.s16        \d0, \s4, d1[0]
        vmla.s16        \d0, \s5, d1[1]
        vmla.s16        \d0, \s6, d1[2]
        vmla.s16        \d0, \s7, d1[3]
        vmul.s16        \d1, \s2, d0[0]
        vmla.s16        \d1, \s3, d0[1]
        vmla.s16        \d1, \s4, d0[2]
        vmla.s16        \d1, \s5, d0[3]
        vmla.s16        \d1, \s6, d1[0]
        vmla.s16        \d1, \s7, d1[1]
        vmla.s16        \d1, \s8, d1[2]
        vmla.s16        \d1, \s9, d1[3]
.endm
.macro mul_mla_8_4 d0, d1, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11
        vmul.s16        \d0, \s0,  d0[0]
        vmla.s16        \d0, \s1,  d0[1]
        vmla.s16        \d0, \s2,  d0[2]
        vmla.s16        \d0, \s3,  d0[3]
        vmla.s16        \d0, \s4,  d1[0]
        vmla.s16        \d0, \s5,  d1[1]
        vmla.s16        \d0, \s6,  d1[2]
        vmla.s16        \d0, \s7,  d1[3]
        vmul.s16        \d1, \s4,  d0[0]
        vmla.s16        \d1, \s5,  d0[1]
        vmla.s16        \d1, \s6,  d0[2]
        vmla.s16        \d1, \s7,  d0[3]
        vmla.s16        \d1, \s8,  d1[0]
        vmla.s16        \d1, \s9,  d1[1]
        vmla.s16        \d1, \s10, d1[2]
        vmla.s16        \d1, \s11, d1[3]
.endm
.macro vqrshrun_s16 shift, q0, d0, q1, d1, q2, d2, q3, d3
        vqrshrun.s16    \d0, \q0, #\shift
.ifnb \q1
        vqrshrun.s16    \d1, \q1, #\shift
.endif
.ifnb \q2
        vqrshrun.s16    \d2, \q2, #\shift
        vqrshrun.s16    \d3, \q3, #\shift
.endif
.endm
.macro vrshr_s16 shift, r0, r1, r2, r3
        vrshr.s16       \r0, \r0, #\shift
.ifnb \r1
        vrshr.s16       \r1, \r1, #\shift
.endif
.ifnb \r2
        vrshr.s16       \r2, \r2, #\shift
        vrshr.s16       \r3, \r3, #\shift
.endif
.endm
.macro st_16 strd, reg, lanes
        vst1.16         {\reg[0]}, [r0, :16], \strd
        vst1.16         {\reg[1]}, [r8, :16], \strd
.if \lanes > 2
        vst1.16         {\reg[2]}, [r0, :16], \strd
        vst1.16         {\reg[3]}, [r8, :16], \strd
.endif
.endm
.macro st_32 strd, r0, r1
        vst1.32         {\r0[0]}, [r0, :32], \strd
        vst1.32         {\r0[1]}, [r8, :32], \strd
.ifnb \r1
        vst1.32         {\r1[0]}, [r0, :32], \strd
        vst1.32         {\r1[1]}, [r8, :32], \strd
.endif
.endm
.macro st_reg strd, align, r0, r1, r2, r3, r4, r5, r6, r7
        vst1.8          {\r0}, [r0, \align], \strd
        vst1.8          {\r1}, [r8, \align], \strd
.ifnb \r2
        vst1.8          {\r2}, [r0, \align], \strd
        vst1.8          {\r3}, [r8, \align], \strd
.endif
.ifnb \r4
        vst1.8          {\r4}, [r0, \align], \strd
        vst1.8          {\r5}, [r8, \align], \strd
        vst1.8          {\r6}, [r0, \align], \strd
        vst1.8          {\r7}, [r8, \align], \strd
.endif
.endm
.macro shift_store_4 type, strd, q0, d0, d1, q1, d2, d3
.ifc \type, put
        vqrshrun_s16    6,     \q0, \d0, \q1, \d2
        st_32           \strd, \d0, \d2
.else
        vrshr_s16       2,          \q0, \q1
        st_reg          \strd, :64, \d0, \d1, \d2, \d3
.endif
.endm
.macro shift_store_8 type, strd, q0, d0, q1, d1, q2, d2, q3, d3
.ifc \type, put
        vqrshrun_s16    6,          \q0, \d0, \q1, \d1, \q2, \d2, \q3, \d3
        st_reg          \strd, :64, \d0, \d1, \d2, \d3
.else
        vrshr_s16       2,          \q0, \q1, \q2, \q3
        st_reg          \strd, :128,\q0, \q1, \q2, \q3
.endif
.endm
.macro shift_store_16 type, strd, q0, d0, d1, q1, q2, d4, d5, q3
.ifc \type, put
        vqrshrun.s16    \d0,   \q0, #6
        vqrshrun.s16    \d1,   \q1, #6
        vqrshrun.s16    \d4,   \q2, #6
        vqrshrun.s16    \d5,   \q3, #6
        st_reg          \strd, :128, \q0, \q2
.else
        vrshr_s16       2,     \q0, \q1, \q2, \q3
        vst1.16         {\q0, \q1}, [r0, :128], \strd
        vst1.16         {\q2, \q3}, [r8, :128], \strd
.endif
.endm

.macro make_8tap_fn op, type, type_h, type_v
function \op\()_8tap_\type\()_8bpc_neon, export=1
        push            {r4-r11,lr}
        movw            r8,  \type_h
        movw            r9,  \type_v
        b               \op\()_8tap_neon
endfunc
.endm

// No spaces in these expressions, due to gas-preprocessor.
#define REGULAR ((0*15<<7)|3*15)
#define SMOOTH  ((1*15<<7)|4*15)
#define SHARP   ((2*15<<7)|3*15)

.macro filter_fn type, dst, d_strd, src, s_strd, w, h, mx, my, ds2, sr2, shift_hv
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
        movw            r10,  #0x4081  // (1 << 14) | (1 << 7) | (1 << 0)
        mul             \mx,  \mx, r10
        mul             \my,  \my, r10
        add             \mx,  \mx, r8 // mx, 8tap_h, 4tap_h
        add             \my,  \my, r9 // my, 8tap_v, 4tap_v
.ifc \type, prep
        lsl             \d_strd, \w, #1
.endif

        clz             r8,  \w
        tst             \mx, #(0x7f << 14)
        sub             r8,  r8,  #24
        movrel          r10, X(mc_subpel_filters), -8
        bne             L(\type\()_8tap_h)
        tst             \my, #(0x7f << 14)
        bne             L(\type\()_8tap_v)
        b               \type\()_neon

L(\type\()_8tap_h):
        cmp             \w,  #4
        ubfx            r9,  \mx, #7, #7
        and             \mx, \mx, #0x7f
        it              gt
        movgt           \mx,  r9
        tst             \my,  #(0x7f << 14)
        add             \mx, r10, \mx, lsl #3
        bne             L(\type\()_8tap_hv)

        adr             r9,  L(\type\()_8tap_h_tbl)
        ldr             r8,  [r9, r8, lsl #2]
        add             r9,  r9,  r8
        bx              r9

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
        add             \mx,  \mx,  #2
        vld1.32         {d0[]}, [\mx]
        sub             \src,  \src,  #1
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
        vmovl.s8        q0,  d0
2:
        vld1.8          {d4},  [\src], \s_strd
        vld1.8          {d6},  [\sr2], \s_strd
        vmovl.u8        q2,  d4
        vmovl.u8        q3,  d6
        vext.8          d5,  d4,  d5,  #2
        vext.8          d7,  d6,  d7,  #2
        subs            \h,  \h,  #2
        vtrn.32         d4,  d6
        vtrn.32         d5,  d7
        vmul.s16        d2,  d4,  d0[0]
        vmla.s16        d2,  d5,  d0[1]
        vmla.s16        d2,  d6,  d0[2]
        vmla.s16        d2,  d7,  d0[3]
        vrshr.s16       d2,  d2,  #2
        vqrshrun.s16    d2,  q1,  #4
        vst1.16         {d2[0]}, [\dst, :16], \d_strd
        vst1.16         {d2[1]}, [\ds2, :16], \d_strd
        bgt             2b
        pop             {r4-r11,pc}
.endif

40:     // 4xN h
        add             \mx,  \mx,  #2
        vld1.32         {d0[]}, [\mx]
        sub             \src,  \src,  #1
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
        vmovl.s8        q0,  d0
4:
        vld1.8          {d16}, [\src], \s_strd
        vld1.8          {d24}, [\sr2], \s_strd
        vmovl.u8        q8,  d16
        vmovl.u8        q12, d24
        vext.8          q9,  q8,  q8,  #2
        vext.8          q10, q8,  q8,  #4
        vext.8          q11, q8,  q8,  #6
        vext.8          q13, q12, q12, #2
        vext.8          q14, q12, q12, #4
        vext.8          q15, q12, q12, #6
        subs            \h,  \h,  #2
        vmul.s16        d4,  d16, d0[0]
        vmla.s16        d4,  d18, d0[1]
        vmla.s16        d4,  d20, d0[2]
        vmla.s16        d4,  d22, d0[3]
        vmul.s16        d5,  d24, d0[0]
        vmla.s16        d5,  d26, d0[1]
        vmla.s16        d5,  d28, d0[2]
        vmla.s16        d5,  d30, d0[3]
        vrshr.s16       q2,  q2,  #2
.ifc \type, put
        vqrshrun.s16    d4,  q2,  #4
        vst1.32         {d4[0]}, [\dst, :32], \d_strd
        vst1.32         {d4[1]}, [\ds2, :32], \d_strd
.else
        vst1.16         {d4}, [\dst, :64], \d_strd
        vst1.16         {d5}, [\ds2, :64], \d_strd
.endif
        bgt             4b
        pop             {r4-r11,pc}

80:     // 8xN h
        vld1.8          {d0}, [\mx]
        sub             \src,  \src,  #3
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
        vmovl.s8        q0,  d0
8:
        vld1.8          {q8},  [\src], \s_strd
        vld1.8          {q12}, [\sr2], \s_strd
        vmovl.u8        q9,  d17
        vmovl.u8        q8,  d16
        vmovl.u8        q13, d25
        vmovl.u8        q12, d24

        vmul.s16        q10, q8,  d0[0]
        vmul.s16        q14, q12, d0[0]
.irpc i, 1234567
        vext.8          q11, q8,  q9,  #(2*\i)
        vext.8          q15, q12, q13, #(2*\i)
.if \i < 4
        vmla.s16        q10, q11, d0[\i]
        vmla.s16        q14, q15, d0[\i]
.else
        vmla.s16        q10, q11, d1[\i-4]
        vmla.s16        q14, q15, d1[\i-4]
.endif
.endr
        subs            \h,  \h,  #2
        vrshr.s16       q10, q10, #2
        vrshr.s16       q14, q14, #2
.ifc \type, put
        vqrshrun.s16    d20, q10, #4
        vqrshrun.s16    d28, q14, #4
        vst1.8          {d20}, [\dst, :64], \d_strd
        vst1.8          {d28}, [\ds2, :64], \d_strd
.else
        vst1.16         {q10}, [\dst, :128], \d_strd
        vst1.16         {q14}, [\ds2, :128], \d_strd
.endif
        bgt             8b
        pop             {r4-r11,pc}

160:
320:
640:
1280:   // 16xN, 32xN, ... h
        // This could be done without touching q4-q6, by using only
        // one temporary for vext in the loop. That's slower on A7 and A53,
        // (but surprisingly, marginally faster on A8 and A73).
        vpush           {q4-q6}
        vld1.8          {d0}, [\mx]
        sub             \src,  \src,  #3
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        vmovl.s8        q0,  d0

        sub             \s_strd,  \s_strd,  \w
        sub             \s_strd,  \s_strd,  #8
.ifc \type, put
        lsl             \d_strd,  \d_strd,  #1
        sub             \d_strd,  \d_strd,  \w
.endif
161:
        vld1.8          {d16, d17, d18},  [\src]!
        vld1.8          {d24, d25, d26},  [\sr2]!
        mov             \mx, \w
        vmovl.u8        q10, d18
        vmovl.u8        q9,  d17
        vmovl.u8        q8,  d16
        vmovl.u8        q14, d26
        vmovl.u8        q13, d25
        vmovl.u8        q12, d24

16:
        vmul.s16        q1,  q8,  d0[0]
        vmul.s16        q2,  q9,  d0[0]
        vmul.s16        q3,  q12, d0[0]
        vmul.s16        q4,  q13, d0[0]
.irpc i, 1234567
        vext.8          q5,  q8,  q9,  #(2*\i)
        vext.8          q6,  q9,  q10, #(2*\i)
        vext.8          q11, q12, q13, #(2*\i)
        vext.8          q15, q13, q14, #(2*\i)
.if \i < 4
        vmla.s16        q1,  q5,  d0[\i]
        vmla.s16        q2,  q6,  d0[\i]
        vmla.s16        q3,  q11, d0[\i]
        vmla.s16        q4,  q15, d0[\i]
.else
        vmla.s16        q1,  q5,  d1[\i-4]
        vmla.s16        q2,  q6,  d1[\i-4]
        vmla.s16        q3,  q11, d1[\i-4]
        vmla.s16        q4,  q15, d1[\i-4]
.endif
.endr
        vrshr.s16       q1,  q1,  #2
        vrshr.s16       q2,  q2,  #2
        vrshr.s16       q3,  q3,  #2
        vrshr.s16       q4,  q4,  #2
        subs            \mx, \mx, #16
.ifc \type, put
        vqrshrun.s16    d2,  q1,  #4
        vqrshrun.s16    d3,  q2,  #4
        vqrshrun.s16    d4,  q3,  #4
        vqrshrun.s16    d5,  q4,  #4
        vst1.8          {q1}, [\dst, :128]!
        vst1.8          {q2}, [\ds2, :128]!
.else
        vst1.16         {q1, q2}, [\dst, :128]!
        vst1.16         {q3, q4}, [\ds2, :128]!
.endif
        ble             9f

        vmov            q8,  q10
        vmov            q12, q14
        vld1.8          {d18, d19}, [\src]!
        vld1.8          {d26, d27}, [\sr2]!
        vmovl.u8        q10, d19
        vmovl.u8        q9,  d18
        vmovl.u8        q14, d27
        vmovl.u8        q13, d26
        b               16b

9:
        add             \dst,  \dst,  \d_strd
        add             \ds2,  \ds2,  \d_strd
        add             \src,  \src,  \s_strd
        add             \sr2,  \sr2,  \s_strd

        subs            \h,  \h,  #2
        bgt             161b
        vpop            {q4-q6}
        pop             {r4-r11,pc}

L(\type\()_8tap_v):
        cmp             \h,  #4
        ubfx            r9,  \my, #7, #7
        and             \my, \my, #0x7f
        it              gt
        movgt           \my, r9
        add             \my, r10, \my, lsl #3

        adr             r9,  L(\type\()_8tap_v_tbl)
        ldr             r8,  [r9, r8, lsl #2]
        add             r9,  r9,  r8
        bx              r9

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
        load_16         \src, \sr2, \s_strd, d1, d2, d3, d4, d5
        interleave_1_16 d1, d2, d3, d4, d5
        bgt             24f
        vmovl_u8        q8, d1, q9, d2, q10, d3, q11, d4
        mul_mla_4       d6, d16, d18, d20, d22
        vqrshrun_s16    6,   q3,  d6
        st_16           \d_strd, d6, 2
        pop             {r4-r11,pc}

24:     // 2x4 v
        load_16         \sr2, \src, \s_strd, d6, d7
        interleave_1_16 d5, d6, d7
        vmovl_u8        q8, d1, q9, d2, q10, d3, q11, d4, q12, d5, q13, d6
        vmov            d17, d20
        vmov            d19, d22
        vmov            d21, d24
        vmov            d23, d26
        mul_mla_4       q3, q8, q9, q10, q11
        vqrshrun_s16    6,   q3,  d6
        st_16           \d_strd, d6, 4
        pop             {r4-r11,pc}

28:     // 2x8, 2x16 v
        vpush           {q4-q7}
        vld1.8          {d0}, [\my]
        sub             \sr2,  \src,  \s_strd, lsl #1
        add             \ds2,  \dst,  \d_strd
        sub             \src,  \sr2,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
        vmovl.s8        q0,  d0

        load_16         \src, \sr2, \s_strd, d2,  d4,  d6,  d8,  d10, d12, d14
        interleave_1_16 d2,  d4,  d6,  d8,  d10
        interleave_1_16 d10, d12, d14
        vmovl_u8        q1,  d2,  q2,  d4,  q3,  d6,  q4,  d8,  q5,  d10, q6,  d12
        vmov            d3,  d6
        vmov            d5,  d8
        vmov            d7,  d10
        vmov            d9,  d12
216:
        subs            \h,  \h,  #8
        load_16         \sr2, \src, \s_strd, d16, d18, d20, d22
        load_16         \sr2, \src, \s_strd, d24, d26, d28, d30
        interleave_1_16 d14, d16, d18, d20, d22
        interleave_1_16 d22, d24, d26, d28, d30
        vmovl_u8        q7,  d14, q8,  d16, q9,  d18, q10, d20
        vmovl_u8        q11, d22, q12, d24, q13, d26, q14, d28
        vmov            d11, d14
        vmov            d13, d16
        vmov            d15, d18
        vmov            d17, d20
        vmov            d19, d22
        vmov            d21, d24
        vmov            d23, d26
        vmov            d25, d28
        mul_mla_8_4     q1,  q2,  q1,  q2,  q3,  q4,  q5,  q6,  q7,  q8,  q9,  q10, q11, q12
        vqrshrun_s16    6,   q1,  d2,  q2,  d4
        st_16           \d_strd, d2, 4
        st_16           \d_strd, d4, 4
        ble             0f
        vmov            q1,  q9
        vmov            q2,  q10
        vmov            q3,  q11
        vmov            q4,  q12
        vmov            q5,  q13
        vmov            q6,  q14
        vmov            d14, d30
        b               216b
0:
        vpop            {q4-q7}
        pop             {r4-r11,pc}
.endif

40:
        bgt            480f

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

        load_32         \src, \sr2, \s_strd, d1, d2, d3, d4, d5
        interleave_1_32 d1,  d2,  d3,  d4,  d5
        vmovl_u8        q8,  d1,  q9,  d2,  q10, d3,  q11, d4
        mul_mla_4       q3,  q8,  q9,  q10, q11
        shift_store_4   \type, \d_strd, q3, d6, d7
        ble             0f
        load_32         \sr2, \src, \s_strd, d6, d7
        interleave_1_32 d5,  d6,  d7
        vmovl_u8        q12, d5,  q13, d6
        mul_mla_4       q3,  q10, q11, q12, q13
        shift_store_4   \type, \d_strd, q3, d6, d7
0:
        pop             {r4-r11,pc}

480:    // 4x8, 4x16 v
        vpush           {q4}
        vld1.8          {d0}, [\my]
        sub             \sr2, \src, \s_strd, lsl #1
        add             \ds2, \dst, \d_strd
        sub             \src, \sr2, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0

        load_32         \src, \sr2, \s_strd, d2,  d4,  d6,  d8,  d16, d18, d20
        interleave_1_32 d2,  d4,  d6
        interleave_1_32 d6,  d8,  d16, d18, d20
        vmovl_u8        q1,  d2,  q2,  d4,  q3,  d6,  q4,  d8,  q8,  d16, q9,  d18

48:
        subs            \h,  \h,  #4
        load_32         \sr2, \src, \s_strd, d22, d24, d26, d28
        interleave_1_32 d20, d22, d24, d26, d28
        vmovl_u8        q10, d20, q11, d22, q12, d24, q13, d26
        mul_mla_8_2     q1,  q2,  q1,  q2,  q3,  q4,  q8,  q9,  q10, q11, q12, q13
        shift_store_4   \type, \d_strd, q1,  d2,  d3,  q2,  d4,  d5
        ble             0f
        subs            \h,  \h,  #4
        load_32         \sr2,  \src, \s_strd, d30, d2,  d4,  d6
        interleave_1_32 d28, d30, d2,  d4,  d6
        vmovl_u8        q14, d28, q15, d30, q1,  d2,  q2,  d4
        mul_mla_8_2     q8,  q9,  q8,  q9,  q10, q11, q12, q13, q14, q15, q1,  q2
        shift_store_4   \type, \d_strd, q8,  d16, d17, q9,  d18, d19
        ble             0f
        subs            \h,  \h,  #4
        load_32         \sr2, \src, \s_strd, d8,  d16, d18, d20
        interleave_1_32 d6,  d8,  d16, d18, d20
        vmovl_u8        q3,  d6,  q4,  d8,  q8,  d16, q9, d18
        mul_mla_8_2     q12, q13, q12, q13, q14, q15, q1,  q2,  q3,  q4,  q8,  q9
        shift_store_4   \type, \d_strd, q12, d24, d25, q13, d26, d27
        b               48b
0:
        vpop            {q4}
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

        load_reg        \src, \sr2, \s_strd, d1, d2, d3, d4, d5
        vmovl_u8        q8,  d1,  q9,  d2,  q10, d3,  q11, d4,  q12, d5
        mul_mla_4       q1,  q8,  q9,  q10, q11
        mul_mla_4       q2,  q9,  q10, q11, q12
        shift_store_8   \type, \d_strd, q1, d2, q2, d4
        ble             0f
        load_reg        \sr2, \src, \s_strd, d6, d7
        vmovl_u8        q13, d6,  q14, d7
        mul_mla_4       q1,  q10, q11, q12, q13
        mul_mla_4       q2,  q11, q12, q13, q14
        shift_store_8   \type, \d_strd, q1, d2, q2, d4
0:
        pop             {r4-r11,pc}

880:    // 8x8, 8x16, 8x32 v
1680:   // 16x8, 16x16, ...
320:    // 32x8, 32x16, ...
640:
1280:
        vpush           {q4}
        vld1.8          {d0}, [\my]
        sub             \src, \src, \s_strd
        sub             \src, \src, \s_strd, lsl #1
        vmovl.s8        q0,  d0
        mov             \my, \h
168:
        add             \ds2, \dst, \d_strd
        add             \sr2, \src, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        load_reg        \src, \sr2, \s_strd, d2,  d4,  d6,  d8,  d16, d18, d20
        vmovl_u8        q1,  d2,  q2,  d4,  q3,  d6,  q4,  d8,  q8,  d16, q9,  d18, q10, d20

88:
        subs            \h,  \h,  #2
        load_reg        \sr2, \src, \s_strd, d22, d24
        vmovl_u8        q11, d22, q12, d24
        mul_mla_8_1     q1,  q2,  q1,  q2,  q3,  q4,  q8,  q9,  q10,  q11, q12
        shift_store_8   \type, \d_strd, q1,  d2,  q2,  d4
        ble             9f
        subs            \h,  \h,  #2
        load_reg        \sr2, \src, \s_strd, d26, d28
        vmovl_u8        q13, d26, q14, d28
        mul_mla_8_1     q3,  q4,  q3,  q4,  q8,  q9,  q10, q11, q12, q13, q14
        shift_store_8   \type, \d_strd, q3,  d6,  q4,  d8
        ble             9f
        subs            \h,  \h,  #4
        load_reg        \sr2, \src, \s_strd, d30, d2,  d4,  d6
        vmovl_u8        q15, d30, q1,  d2,  q2,  d4,  q3,  d6
        mul_mla_8_1     q8,  q9,  q8,  q9,  q10, q11, q12, q13, q14, q15, q1
        mul_mla_8_1     q10, q11, q10, q11, q12, q13, q14, q15, q1,  q2,  q3
        shift_store_8   \type, \d_strd, q8,  d16, q9,  d18, q10, d20, q11, d22
        ble             9f
        subs            \h,  \h,  #4
        load_reg        \sr2, \src, \s_strd, d8,  d16, d18, d20
        vmovl_u8        q4,  d8,  q8,  d16, q9,  d18, q10, d20
        mul_mla_8_1     q12, q13, q12, q13, q14, q15, q1,  q2,  q3,  q4,  q8
        mul_mla_8_1     q14, q15, q14, q15, q1,  q2,  q3,  q4,  q8,  q9,  q10
        shift_store_8   \type, \d_strd, q12, d24, q13, d26, q14, d28, q15, d30
        bgt             88b
9:
        subs            \w,  \w,  #8
        ble             0f
        asr             \s_strd, \s_strd, #1
        asr             \d_strd, \d_strd, #1
        mls             \src, \s_strd, \my, \src
        mls             \dst, \d_strd, \my, \dst
        sub             \src, \src, \s_strd, lsl #3
        mov             \h,  \my
        add             \src, \src, #8
.ifc \type, put
        add             \dst, \dst, #8
.else
        add             \dst, \dst, #16
.endif
        b               168b
0:
        vpop            {q4}
        pop             {r4-r11,pc}

160:
        bgt             1680b

        // 16x2, 16x4 v
        add             \my, \my, #2
        vld1.32         {d0[]}, [\my]
        sub             \src, \src, \s_strd
        add             \ds2, \dst, \d_strd
        add             \sr2, \src, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0

        cmp             \h,  #2
        load_reg        \src, \sr2, \s_strd, q11, q12, q13, q14, q15
        vmovl.u8        q1,  d22
        vmovl.u8        q2,  d24
        vmovl.u8        q3,  d26
        vmovl.u8        q8,  d28
        vmovl.u8        q9,  d30
        vmovl.u8        q11, d23
        vmovl.u8        q12, d25
        vmovl.u8        q13, d27
        vmovl.u8        q14, d29
        vmovl.u8        q15, d31
        mul_mla_4       q1,  q1,  q2,  q3,  q8
        mul_mla_4       q10, q2,  q3,  q8,  q9
        mul_mla_4       q2,  q11, q12, q13, q14
        mul_mla_4       q11, q12, q13, q14, q15
        shift_store_16  \type, \d_strd, q1, d2, d3, q2, q10, d20, d21, q11
        ble             0f
        load_reg        \sr2, \src, \s_strd, q10, q11
        vmovl.u8        q1,  d20
        vmovl.u8        q10, d21
        vmovl.u8        q12, d22
        vmovl.u8        q11, d23
        mul_mla_4       q2,  q3,  q8,  q9,  q1
        mul_mla_4       q3,  q13, q14, q15, q10
        mul_mla_4       q13, q8,  q9,  q1,  q12
        mul_mla_4       q14, q14, q15, q10, q11
        shift_store_16  \type, \d_strd, q2, d4, d5, q3, q13, d26, d27, q14
0:
        pop             {r4-r11,pc}

L(\type\()_8tap_hv):
        cmp             \h,  #4
        ubfx            r9,  \my, #7, #7
        and             \my, \my, #0x7f
        it              gt
        movgt           \my, r9
        add             \my,  r10, \my, lsl #3

        adr             r9,  L(\type\()_8tap_hv_tbl)
        ldr             r8,  [r9, r8, lsl #2]
        add             r9,  r9,  r8
        bx              r9

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
        add             \mx,  \mx,  #2
        vld1.32         {d0[]},  [\mx]
        bgt             280f
        add             \my,  \my,  #2
        vld1.32         {d2[]},  [\my]

        // 2x2, 2x4 hv
        sub             \sr2, \src, #1
        sub             \src, \sr2, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2


        vld1.8          {d26}, [\src], \s_strd
        vmovl.u8        q13, d26
        vext.8          q14, q13, q13, #2
        vmul.s16        d26, d26, d0
        vmul.s16        d28, d28, d0
        vpadd.s16       d26, d26, d28
        vpadd.s16       d26, d26, d26
        vrshr.s16       d16, d26, #2
        bl              L(\type\()_8tap_filter_2)

        vext.8          d16, d16, d16, #4
        vmov            d17, d26
        vext.8          d16, d16, d26, #4

2:
        bl              L(\type\()_8tap_filter_2)

        vext.8          d18, d17, d26, #4
        vmov            d19, d26
        vmull.s16       q2,  d16, d2[0]
        vmlal.s16       q2,  d17, d2[1]
        vmlal.s16       q2,  d18, d2[2]
        vmlal.s16       q2,  d19, d2[3]

        vqrshrn.s32     d4,  q2,  #\shift_hv
        vqmovun.s16     d4,  q2
        subs            \h,  \h,  #2
        vst1.16         {d4[0]}, [\dst, :16], \d_strd
        vst1.16         {d4[1]}, [\ds2, :16], \d_strd
        ble             0f
        vmov            d16, d18
        vmov            d17, d19
        b               2b

280:    // 2x8, 2x16, 2x32 hv
        vld1.8          {d2},  [\my]
        sub             \src, \src, #1
        sub             \sr2, \src, \s_strd, lsl #1
        sub             \src, \sr2, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2

        vld1.8          {d26}, [\src], \s_strd
        vmovl.u8        q13, d26
        vext.8          q14, q13, q13, #2
        vmul.s16        d26, d26, d0
        vmul.s16        d28, d28, d0
        vpadd.s16       d26, d26, d28
        vpadd.s16       d26, d26, d26
        vrshr.s16       d16, d26, #2

        bl              L(\type\()_8tap_filter_2)
        vext.8          d16, d16, d16, #4
        vmov            d17, d26
        vext.8          d16, d16, d26, #4
        bl              L(\type\()_8tap_filter_2)
        vext.8          d18, d17, d26, #4
        vmov            d19, d26
        bl              L(\type\()_8tap_filter_2)
        vext.8          d20, d19, d26, #4
        vmov            d21, d26

28:
        bl              L(\type\()_8tap_filter_2)
        vext.8          d22, d21, d26, #4
        vmov            d23, d26
        vmull.s16       q2,  d16, d2[0]
        vmlal.s16       q2,  d17, d2[1]
        vmlal.s16       q2,  d18, d2[2]
        vmlal.s16       q2,  d19, d2[3]
        vmlal.s16       q2,  d20, d3[0]
        vmlal.s16       q2,  d21, d3[1]
        vmlal.s16       q2,  d22, d3[2]
        vmlal.s16       q2,  d23, d3[3]

        vqrshrn.s32     d4,  q2,  #\shift_hv
        vqmovun.s16     d4,  q2
        subs            \h,  \h,  #2
        vst1.16         {d4[0]}, [\dst, :16], \d_strd
        vst1.16         {d4[1]}, [\ds2, :16], \d_strd
        ble             0f
        vmov            d16, d18
        vmov            d17, d19
        vmov            d18, d20
        vmov            d19, d21
        vmov            d20, d22
        vmov            d21, d23
        b               28b

0:
        pop             {r4-r11,pc}

L(\type\()_8tap_filter_2):
        vld1.8          {d28},  [\sr2], \s_strd
        vld1.8          {d30},  [\src], \s_strd
        vext.8          d29, d28, d28, #1
        vext.8          d31, d30, d30, #1
        vmovl.u8        q13, d28
        vmovl.u8        q14, d29
        vmov            d27, d28
        vmovl.u8        q14, d30
        vmovl.u8        q15, d31
        vtrn.32         d26, d28
        vtrn.32         d27, d30
        vmul.s16        d26, d26, d0[0]
        vmla.s16        d26, d27, d0[1]
        vmla.s16        d26, d28, d0[2]
        vmla.s16        d26, d30, d0[3]
        vrshr.s16       d26, d26, #2
        vext.8          d27, d26, d26, #4
        bx              lr
.endif

40:
        add             \mx, \mx, #2
        vld1.32         {d0[]},  [\mx]
        bgt             480f
        add             \my, \my,  #2
        vld1.32         {d2[]},  [\my]
        sub             \sr2, \src, #1
        sub             \src, \sr2, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2

        // 4x2, 4x4 hv
        vld1.8          {d30}, [\src], \s_strd
        vmovl.u8        q14, d30
        vext.8          d27, d28, d29, #2
        vext.8          d30, d28, d29, #4
        vext.8          d31, d28, d29, #6
        vmul.s16        d26, d28, d0[0]
        vmla.s16        d26, d27, d0[1]
        vmla.s16        d26, d30, d0[2]
        vmla.s16        d26, d31, d0[3]
        vrshr.s16       d16, d26, #2

        bl              L(\type\()_8tap_filter_4)
        vmov            d17, d26
        vmov            d18, d27

4:
        bl              L(\type\()_8tap_filter_4)
        vmull.s16       q2,  d16, d2[0]
        vmlal.s16       q2,  d17, d2[1]
        vmlal.s16       q2,  d18, d2[2]
        vmlal.s16       q2,  d26, d2[3]
        vmull.s16       q3,  d17, d2[0]
        vmlal.s16       q3,  d18, d2[1]
        vmlal.s16       q3,  d26, d2[2]
        vmlal.s16       q3,  d27, d2[3]
        vqrshrn.s32     d4,  q2,  #\shift_hv
        vqrshrn.s32     d6,  q3,  #\shift_hv
        subs            \h,  \h,  #2
.ifc \type, put
        vqmovun.s16     d4,  q2
        vqmovun.s16     d6,  q3
        vst1.32         {d4[0]}, [\dst, :32], \d_strd
        vst1.32         {d6[0]}, [\ds2, :32], \d_strd
.else
        vst1.16         {d4}, [\dst, :64], \d_strd
        vst1.16         {d6}, [\ds2, :64], \d_strd
.endif
        ble             0f
        vmov            d16, d18
        vmov            d17, d26
        vmov            d18, d27
        b               4b

480:    // 4x8, 4x16, 4x32 hv
        vld1.8          {d2},  [\my]
        sub             \src, \src, #1
        sub             \sr2, \src, \s_strd, lsl #1
        sub             \src, \sr2, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2

        vld1.8          {d30}, [\src], \s_strd
        vmovl.u8        q14, d30
        vext.8          d27, d28, d29, #2
        vext.8          d30, d28, d29, #4
        vext.8          d31, d28, d29, #6
        vmul.s16        d26, d28, d0[0]
        vmla.s16        d26, d27, d0[1]
        vmla.s16        d26, d30, d0[2]
        vmla.s16        d26, d31, d0[3]
        vrshr.s16       d16, d26, #2

        bl              L(\type\()_8tap_filter_4)
        vmov            d17, d26
        vmov            d18, d27
        bl              L(\type\()_8tap_filter_4)
        vmov            d19, d26
        vmov            d20, d27
        bl              L(\type\()_8tap_filter_4)
        vmov            d21, d26
        vmov            d22, d27

48:
        bl              L(\type\()_8tap_filter_4)
        vmull.s16       q2,  d16, d2[0]
        vmlal.s16       q2,  d17, d2[1]
        vmlal.s16       q2,  d18, d2[2]
        vmlal.s16       q2,  d19, d2[3]
        vmlal.s16       q2,  d20, d3[0]
        vmlal.s16       q2,  d21, d3[1]
        vmlal.s16       q2,  d22, d3[2]
        vmlal.s16       q2,  d26, d3[3]
        vmull.s16       q3,  d17, d2[0]
        vmlal.s16       q3,  d18, d2[1]
        vmlal.s16       q3,  d19, d2[2]
        vmlal.s16       q3,  d20, d2[3]
        vmlal.s16       q3,  d21, d3[0]
        vmlal.s16       q3,  d22, d3[1]
        vmlal.s16       q3,  d26, d3[2]
        vmlal.s16       q3,  d27, d3[3]
        vqrshrn.s32     d4,  q2,  #\shift_hv
        vqrshrn.s32     d6,  q3,  #\shift_hv
        subs            \h,  \h,  #2
.ifc \type, put
        vqmovun.s16     d4,  q2
        vqmovun.s16     d6,  q3
        vst1.32         {d4[0]}, [\dst, :32], \d_strd
        vst1.32         {d6[0]}, [\ds2, :32], \d_strd
.else
        vst1.16         {d4}, [\dst, :64], \d_strd
        vst1.16         {d6}, [\ds2, :64], \d_strd
.endif
        ble             0f
        vmov            d16, d18
        vmov            d17, d19
        vmov            d18, d20
        vmov            d19, d21
        vmov            d20, d22
        vmov            d21, d26
        vmov            d22, d27
        b               48b
0:
        pop             {r4-r11,pc}

L(\type\()_8tap_filter_4):
        vld1.8          {d30}, [\sr2], \s_strd
        vld1.8          {d31}, [\src], \s_strd
        vmovl.u8        q14, d30
        vext.8          d27, d28, d29, #2
        vext.8          d30, d28, d29, #4
        vext.8          d1,  d28, d29, #6
        vmul.s16        d26, d28, d0[0]
        vmla.s16        d26, d27, d0[1]
        vmla.s16        d26, d30, d0[2]
        vmla.s16        d26, d1,  d0[3]

        vmovl.u8        q14, d31
        vext.8          d30, d28, d29, #2
        vext.8          d31, d28, d29, #4
        vext.8          d1,  d28, d29, #6
        vmul.s16        d27, d28, d0[0]
        vmla.s16        d27, d30, d0[1]
        vmla.s16        d27, d31, d0[2]
        vmla.s16        d27, d1,  d0[3]
        vrshr.s16       d26, d26, #2
        vrshr.s16       d27, d27, #2
        bx              lr

80:
160:
320:
        bgt             880f
        vpush           {q4-q7}
        add             \my,  \my,  #2
        vld1.8          {d0},  [\mx]
        vld1.32         {d2[]},  [\my]
        sub             \src,  \src,  #3
        sub             \src,  \src,  \s_strd
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2
        mov             \my, \h

164:    // 8x2, 8x4, 16x2, 16x4, 32x2, 32x4 hv
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd, \d_strd, #1
        lsl             \s_strd, \s_strd, #1

        vld1.8          {q14},  [\src], \s_strd
        vmovl.u8        q12, d28
        vmovl.u8        q13, d29
        vmul.s16        q10, q12, d0[0]
.irpc i, 123
        vext.8          q14, q12, q13, #(2*\i)
        vmla.s16        q10, q14, d0[\i]
.endr
.irpc i, 4567
        vext.8          q14, q12, q13, #(2*\i)
        vmla.s16        q10, q14, d1[\i-4]
.endr
        vrshr.s16       q3,  q10, #2

        bl              L(\type\()_8tap_filter_8)
        vmov            q4,  q10
        vmov            q5,  q11

8:
        bl              L(\type\()_8tap_filter_8)
        vmull.s16       q12, d6,  d2[0]
        vmull.s16       q13, d7,  d2[0]
        vmull.s16       q14, d8,  d2[0]
        vmull.s16       q15, d9,  d2[0]
        vmlal.s16       q12, d8,  d2[1]
        vmlal.s16       q13, d9,  d2[1]
        vmlal.s16       q14, d10, d2[1]
        vmlal.s16       q15, d11, d2[1]
        vmlal.s16       q12, d10, d2[2]
        vmlal.s16       q13, d11, d2[2]
        vmlal.s16       q14, d20, d2[2]
        vmlal.s16       q15, d21, d2[2]
        vmlal.s16       q12, d20, d2[3]
        vmlal.s16       q13, d21, d2[3]
        vmlal.s16       q14, d22, d2[3]
        vmlal.s16       q15, d23, d2[3]
        vqrshrn.s32     d24, q12, #\shift_hv
        vqrshrn.s32     d25, q13, #\shift_hv
        vqrshrn.s32     d28, q14, #\shift_hv
        vqrshrn.s32     d29, q15, #\shift_hv
        subs            \h,  \h,  #2
.ifc \type, put
        vqmovun.s16     d24, q12
        vqmovun.s16     d28, q14
        vst1.8          {d24}, [\dst, :64], \d_strd
        vst1.8          {d28}, [\ds2, :64], \d_strd
.else
        vst1.16         {q12}, [\dst, :128], \d_strd
        vst1.16         {q14}, [\ds2, :128], \d_strd
.endif
        ble             9f
        vmov            q3,  q5
        vmov            q4,  q10
        vmov            q5,  q11
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
        add             \src,  \src,  #8
.ifc \type, put
        add             \dst,  \dst,  #8
.else
        add             \dst,  \dst,  #16
.endif
        b               164b

880:    // 8x8, 8x16, ..., 16x8, ..., 32x8, ... hv
640:
1280:
        vpush           {q4-q7}
        vld1.8          {d0},  [\mx]
        vld1.8          {d2},  [\my]
        sub             \src,  \src,  #3
        sub             \src,  \src,  \s_strd
        sub             \src,  \src,  \s_strd, lsl #1
        vmovl.s8        q0,  d0
        vmovl.s8        q1,  d2
        mov             \my, \h

168:
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd, \d_strd, #1
        lsl             \s_strd, \s_strd, #1

        vld1.8          {q14},  [\src], \s_strd
        vmovl.u8        q12, d28
        vmovl.u8        q13, d29
        vmul.s16        q10, q12, d0[0]
.irpc i, 123
        vext.8          q14, q12, q13, #(2*\i)
        vmla.s16        q10, q14, d0[\i]
.endr
.irpc i, 4567
        vext.8          q14, q12, q13, #(2*\i)
        vmla.s16        q10, q14, d1[\i-4]
.endr
        vrshr.s16       q3,  q10, #2

        bl              L(\type\()_8tap_filter_8)
        vmov            q4,  q10
        vmov            q5,  q11
        bl              L(\type\()_8tap_filter_8)
        vmov            q6,  q10
        vmov            q7,  q11
        bl              L(\type\()_8tap_filter_8)
        vmov            q8,  q10
        vmov            q9,  q11

88:
        bl              L(\type\()_8tap_filter_8)
        vmull.s16       q12, d6,  d2[0]
        vmull.s16       q13, d7,  d2[0]
        vmull.s16       q14, d8,  d2[0]
        vmull.s16       q15, d9,  d2[0]
        vmlal.s16       q12, d8,  d2[1]
        vmlal.s16       q13, d9,  d2[1]
        vmlal.s16       q14, d10, d2[1]
        vmlal.s16       q15, d11, d2[1]
        vmlal.s16       q12, d10, d2[2]
        vmlal.s16       q13, d11, d2[2]
        vmlal.s16       q14, d12, d2[2]
        vmlal.s16       q15, d13, d2[2]
        vmlal.s16       q12, d12, d2[3]
        vmlal.s16       q13, d13, d2[3]
        vmlal.s16       q14, d14, d2[3]
        vmlal.s16       q15, d15, d2[3]
        vmlal.s16       q12, d14, d3[0]
        vmlal.s16       q13, d15, d3[0]
        vmlal.s16       q14, d16, d3[0]
        vmlal.s16       q15, d17, d3[0]
        vmlal.s16       q12, d16, d3[1]
        vmlal.s16       q13, d17, d3[1]
        vmlal.s16       q14, d18, d3[1]
        vmlal.s16       q15, d19, d3[1]
        vmlal.s16       q12, d18, d3[2]
        vmlal.s16       q13, d19, d3[2]
        vmlal.s16       q14, d20, d3[2]
        vmlal.s16       q15, d21, d3[2]
        vmlal.s16       q12, d20, d3[3]
        vmlal.s16       q13, d21, d3[3]
        vmlal.s16       q14, d22, d3[3]
        vmlal.s16       q15, d23, d3[3]
        vqrshrn.s32     d24, q12, #\shift_hv
        vqrshrn.s32     d25, q13, #\shift_hv
        vqrshrn.s32     d28, q14, #\shift_hv
        vqrshrn.s32     d29, q15, #\shift_hv
        subs            \h,  \h,  #2
.ifc \type, put
        vqmovun.s16     d24, q12
        vqmovun.s16     d28, q14
        vst1.8          {d24}, [\dst, :64], \d_strd
        vst1.8          {d28}, [\ds2, :64], \d_strd
.else
        vst1.16         {q12}, [\dst, :128], \d_strd
        vst1.16         {q14}, [\ds2, :128], \d_strd
.endif
        ble             9f
        vmov            q3,  q5
        vmov            q4,  q6
        vmov            q5,  q7
        vmov            q6,  q8
        vmov            q7,  q9
        vmov            q8,  q10
        vmov            q9,  q11
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
        add             \src,  \src,  #8
.ifc \type, put
        add             \dst,  \dst,  #8
.else
        add             \dst,  \dst,  #16
.endif
        b               168b
0:
        vpop            {q4-q7}
        pop             {r4-r11,pc}

L(\type\()_8tap_filter_8):
        vld1.8          {q14},  [\sr2], \s_strd
        vld1.8          {q15},  [\src], \s_strd
        vmovl.u8        q12, d28
        vmovl.u8        q13, d29
        vmul.s16        q10, q12, d0[0]
.irpc i, 123
        vext.8          q14, q12, q13, #(2*\i)
        vmla.s16        q10, q14, d0[\i]
.endr
.irpc i, 4567
        vext.8          q14, q12, q13, #(2*\i)
        vmla.s16        q10, q14, d1[\i-4]
.endr
        vmovl.u8        q12, d30
        vmovl.u8        q13, d31
        vmul.s16        q11, q12, d0[0]
.irpc i, 123
        vext.8          q14, q12, q13, #(2*\i)
        vmla.s16        q11, q14, d0[\i]
.endr
.irpc i, 4567
        vext.8          q14, q12, q13, #(2*\i)
        vmla.s16        q11, q14, d1[\i-4]
.endr
        vrshr.s16       q10, q10, #2
        vrshr.s16       q11, q11, #2
        bx              lr
endfunc


function \type\()_bilin_8bpc_neon, export=1
        push            {r4-r11,lr}
        ldrd            r4,  r5,  [sp, #36]
        ldrd            r6,  r7,  [sp, #44]
        vdup.8          d1,  \mx
        vdup.8          d3,  \my
        rsb             r8,  \mx, #16
        rsb             r9,  \my, #16
        vdup.8          d0,  r8
        vdup.8          d2,  r9
.ifc \type, prep
        lsl             \d_strd, \w, #1
.endif
        clz             r8,  \w
        cmp             \mx, #0
        sub             r8,  r8,  #24
        bne             L(\type\()_bilin_h)
        cmp             \my, #0
        bne             L(\type\()_bilin_v)
        b               \type\()_neon

L(\type\()_bilin_h):
        cmp             \my, #0
        bne             L(\type\()_bilin_hv)

        adr             r9,  L(\type\()_bilin_h_tbl)
        ldr             r8,  [r9, r8, lsl #2]
        add             r9,  r9,  r8
        bx              r9

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
        vld1.32         {d4[]},  [\src], \s_strd
        vld1.32         {d6[]},  [\sr2], \s_strd
        vext.8          d5,  d4,  d4, #1
        vext.8          d7,  d6,  d6, #1
        vtrn.16         q2,  q3
        subs            \h,  \h,  #2
        vmull.u8        q3,  d4,  d0
        vmlal.u8        q3,  d5,  d1
        vqrshrn.u16     d4,  q3,  #4
        vst1.16         {d4[0]}, [\dst, :16], \d_strd
        vst1.16         {d4[1]}, [\ds2, :16], \d_strd
        bgt             2b
        pop             {r4-r11,pc}
.endif

40:     // 4xN h
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
4:
        vld1.8          {d4}, [\src], \s_strd
        vld1.8          {d6}, [\sr2], \s_strd
        vext.8          d5,  d4,  d4, #1
        vext.8          d7,  d6,  d6, #1
        vtrn.32         q2,  q3
        subs            \h,  \h,  #2
        vmull.u8        q3,  d4,  d0
        vmlal.u8        q3,  d5,  d1
.ifc \type, put
        vqrshrn.u16     d4,  q3,  #4
        vst1.32         {d4[0]}, [\dst, :32], \d_strd
        vst1.32         {d4[1]}, [\ds2, :32], \d_strd
.else
        vst1.16         {d6}, [\dst, :64], \d_strd
        vst1.16         {d7}, [\ds2, :64], \d_strd
.endif
        bgt             4b
        pop             {r4-r11,pc}

80:     // 8xN h
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
8:
        vld1.8          {q8},  [\src], \s_strd
        vld1.8          {q10}, [\sr2], \s_strd
        vext.8          q9,  q8,  q8,  #1
        vext.8          q11, q10, q10, #1
        subs            \h,  \h,  #2
        vmull.u8        q8,  d16, d0
        vmull.u8        q10, d20, d0
        vmlal.u8        q8,  d18, d1
        vmlal.u8        q10, d22, d1
.ifc \type, put
        vqrshrn.u16     d16,  q8,  #4
        vqrshrn.u16     d18,  q10, #4
        vst1.8          {d16}, [\dst, :64], \d_strd
        vst1.8          {d18}, [\ds2, :64], \d_strd
.else
        vst1.16         {q8},  [\dst, :128], \d_strd
        vst1.16         {q10}, [\ds2, :128], \d_strd
.endif
        bgt             8b
        pop             {r4-r11,pc}
160:
320:
640:
1280:   // 16xN, 32xN, ... h
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1

        sub             \s_strd,  \s_strd,  \w
        sub             \s_strd,  \s_strd,  #8
.ifc \type, put
        lsl             \d_strd,  \d_strd,  #1
        sub             \d_strd,  \d_strd,  \w
.endif
161:
        vld1.8          {d16},  [\src]!
        vld1.8          {d22},  [\sr2]!
        mov             \mx, \w

16:
        vld1.8          {d17,d18},  [\src]!
        vld1.8          {d23,d24},  [\sr2]!
        vext.8          q10, q8,  q9,  #1
        vext.8          q13, q11, q12, #1
        vmull.u8        q2,  d16, d0
        vmull.u8        q3,  d17, d0
        vmull.u8        q14, d22, d0
        vmull.u8        q15, d23, d0
        vmlal.u8        q2,  d20, d1
        vmlal.u8        q3,  d21, d1
        vmlal.u8        q14, d26, d1
        vmlal.u8        q15, d27, d1
        subs            \mx, \mx, #16
.ifc \type, put
        vqrshrn.u16     d4,  q2,  #4
        vqrshrn.u16     d5,  q3,  #4
        vqrshrn.u16     d28, q14, #4
        vqrshrn.u16     d29, q15, #4
        vst1.8          {q2},  [\dst, :128]!
        vst1.8          {q14}, [\ds2, :128]!
.else
        vst1.16         {q2,  q3},  [\dst, :128]!
        vst1.16         {q14, q15}, [\ds2, :128]!
.endif
        ble             9f

        vmov            d16, d18
        vmov            d22, d24
        b               16b

9:
        add             \dst,  \dst,  \d_strd
        add             \ds2,  \ds2,  \d_strd
        add             \src,  \src,  \s_strd
        add             \sr2,  \sr2,  \s_strd

        subs            \h,  \h,  #2
        bgt             161b
        pop             {r4-r11,pc}

L(\type\()_bilin_v):
        cmp             \h,  #4
        adr             r9,  L(\type\()_bilin_v_tbl)
        ldr             r8,  [r9, r8, lsl #2]
        add             r9,  r9,  r8
        bx              r9

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
        vld1.16         {d16[]}, [\src], \s_strd
        bgt             24f
        vld1.16         {d17[]}, [\sr2], \s_strd
        vld1.16         {d18[]}, [\src], \s_strd
        vext.8          d16, d16, d17, #6
        vext.8          d17, d17, d18, #6
        vmull.u8        q2,  d16, d2
        vmlal.u8        q2,  d17, d3
        vqrshrn.u16     d4,  q2,  #4
        vst1.16         {d4[0]}, [\dst, :16]
        vst1.16         {d4[1]}, [\ds2, :16]
        pop             {r4-r11,pc}
24:     // 2x4, 2x8, ... v
        vld1.16         {d17[]}, [\sr2], \s_strd
        vld1.16         {d18[]}, [\src], \s_strd
        vld1.16         {d19[]}, [\sr2], \s_strd
        vld1.16         {d20[]}, [\src], \s_strd
        vext.8          d16, d16, d17, #6
        vext.8          d17, d17, d18, #6
        vext.8          d18, d18, d19, #6
        vext.8          d19, d19, d20, #6
        vtrn.32         d16, d18
        vtrn.32         d17, d19
        vmull.u8        q2,  d16, d2
        vmlal.u8        q2,  d17, d3
        subs            \h,  \h,  #4
        vqrshrn.u16     d4,  q2,  #4
        vst1.16         {d4[0]}, [\dst, :16], \d_strd
        vst1.16         {d4[1]}, [\ds2, :16], \d_strd
        vst1.16         {d4[2]}, [\dst, :16], \d_strd
        vst1.16         {d4[3]}, [\ds2, :16], \d_strd
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
        vld1.32         {d16[]}, [\src], \s_strd
4:
        vld1.32         {d17[]}, [\sr2], \s_strd
        vld1.32         {d18[]}, [\src], \s_strd
        vext.8          d16, d16, d17, #4
        vext.8          d17, d17, d18, #4
        vmull.u8        q2,  d16, d2
        vmlal.u8        q2,  d17, d3
        subs            \h,  \h,  #2
.ifc \type, put
        vqrshrn.u16     d4,  q2,  #4
        vst1.32         {d4[0]}, [\dst, :32], \d_strd
        vst1.32         {d4[1]}, [\ds2, :32], \d_strd
.else
        vst1.16         {d4}, [\dst, :64], \d_strd
        vst1.16         {d5}, [\ds2, :64], \d_strd
.endif
        ble             0f
        vmov            d16,  d18
        b               4b
0:
        pop             {r4-r11,pc}

80:     // 8xN v
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        lsl             \d_strd,  \d_strd,  #1
        vld1.8          {d16}, [\src], \s_strd
8:
        vld1.8          {d17}, [\sr2], \s_strd
        vld1.8          {d18}, [\src], \s_strd
        vmull.u8        q2,  d16, d2
        vmull.u8        q3,  d17, d2
        vmlal.u8        q2,  d17, d3
        vmlal.u8        q3,  d18, d3
        subs            \h,  \h,  #2
.ifc \type, put
        vqrshrn.u16     d4,  q2,  #4
        vqrshrn.u16     d6,  q3,  #4
        vst1.8          {d4}, [\dst, :64], \d_strd
        vst1.8          {d6}, [\ds2, :64], \d_strd
.else
        vst1.16         {q2}, [\dst, :128], \d_strd
        vst1.16         {q3}, [\ds2, :128], \d_strd
.endif
        ble             0f
        vmov            d16, d18
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

        vld1.8          {q8},  [\src], \s_strd
2:
        vld1.8          {q9},  [\sr2], \s_strd
        vld1.8          {q10}, [\src], \s_strd
        vmull.u8        q12, d16, d2
        vmull.u8        q13, d17, d2
        vmull.u8        q14, d18, d2
        vmull.u8        q15, d19, d2
        vmlal.u8        q12, d18, d3
        vmlal.u8        q13, d19, d3
        vmlal.u8        q14, d20, d3
        vmlal.u8        q15, d21, d3
        subs            \h,  \h,  #2
.ifc \type, put
        vqrshrn.u16     d24, q12, #4
        vqrshrn.u16     d25, q13, #4
        vqrshrn.u16     d28, q14, #4
        vqrshrn.u16     d29, q15, #4
        vst1.8          {q12}, [\dst, :128], \d_strd
        vst1.8          {q14}, [\ds2, :128], \d_strd
.else
        vst1.16         {q12, q13}, [\dst, :128], \d_strd
        vst1.16         {q14, q15}, [\ds2, :128], \d_strd
.endif
        ble             9f
        vmov            q8,  q10
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
        add             \src, \src, #16
.ifc \type, put
        add             \dst, \dst, #16
.else
        add             \dst, \dst, #32
.endif
        b               1b
0:
        pop             {r4-r11,pc}

L(\type\()_bilin_hv):
        vmovl.u8        q2,  d2
        vmovl.u8        q3,  d3
        adr             r9,  L(\type\()_bilin_hv_tbl)
        ldr             r8,  [r9, r8, lsl #2]
        add             r9,  r9,  r8
        bx              r9

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

        vld1.32         {d28[]},  [\src], \s_strd
        vext.8          d29, d28, d28, #1
        vmull.u8        q8,  d28, d0
        vmlal.u8        q8,  d29, d1

2:
        vld1.32         {d28[]},  [\sr2], \s_strd
        vld1.32         {d30[]},  [\src], \s_strd
        vext.8          d29, d28, d28, #1
        vext.8          d31, d30, d30, #1
        vtrn.16         d28, d30
        vtrn.16         d29, d31
        vmull.u8        q9,  d28, d0
        vmlal.u8        q9,  d29, d1

        vtrn.32         d16, d18

        vmul.u16        d20, d16, d4
        vmla.u16        d20, d19, d6
        vqrshrn.u16     d20, q10, #8
        subs            \h,  \h,  #2
        vst1.16         {d20[0]}, [\dst, :16], \d_strd
        vst1.16         {d20[1]}, [\ds2, :16], \d_strd
        ble             0f
        vtrn.32         d19, d16
        b               2b
0:
        pop             {r4-r11,pc}
.endif

40:     // 4xN hv
        add             \sr2, \src, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        vld1.8          {d28},  [\src], \s_strd
        vext.8          d29, d28, d28, #1
        vmull.u8        q8,  d28, d0
        vmlal.u8        q8,  d29, d1

4:
        vld1.8          {d28},  [\sr2], \s_strd
        vld1.8          {d30},  [\src], \s_strd
        vext.8          d29, d28, d28, #1
        vext.8          d31, d30, d30, #1
        vtrn.32         d28, d30
        vtrn.32         d29, d31
        vmull.u8        q9,  d28, d0
        vmlal.u8        q9,  d29, d1

        vmov            d17, d18

        vmul.u16        q10, q8, q2
        vmla.u16        q10, q9, q3
        subs            \h,  \h,  #2
.ifc \type, put
        vqrshrn.u16     d20, q10, #8
        vst1.32         {d20[0]}, [\dst, :32], \d_strd
        vst1.32         {d20[1]}, [\ds2, :32], \d_strd
.else
        vrshr.u16       q10, q10, #4
        vst1.16         {d20}, [\dst, :64], \d_strd
        vst1.16         {d21}, [\ds2, :64], \d_strd
.endif
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

        vld1.8          {q12},  [\src], \s_strd
        vext.8          q13, q12, q12, #1
        vmull.u8        q8,  d24, d0
        vmlal.u8        q8,  d26, d1

2:
        vld1.8          {q12},  [\sr2], \s_strd
        vld1.8          {q14},  [\src], \s_strd
        vext.8          q13, q12, q12, #1
        vext.8          q15, q14, q14, #1
        vmull.u8        q9,  d24, d0
        vmlal.u8        q9,  d26, d1
        vmull.u8        q10, d28, d0
        vmlal.u8        q10, d30, d1

        vmul.u16        q8,  q8,  q2
        vmla.u16        q8,  q9,  q3
        vmul.u16        q9,  q9,  q2
        vmla.u16        q9,  q10, q3
        subs            \h,  \h,  #2
.ifc \type, put
        vqrshrn.u16     d16, q8,  #8
        vqrshrn.u16     d18, q9,  #8
        vst1.8          {d16}, [\dst, :64], \d_strd
        vst1.8          {d18}, [\ds2, :64], \d_strd
.else
        vrshr.u16       q8,  q8,  #4
        vrshr.u16       q9,  q9,  #4
        vst1.16         {q8}, [\dst, :128], \d_strd
        vst1.16         {q9}, [\ds2, :128], \d_strd
.endif
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
        add             \src,  \src,  #8
.ifc \type, put
        add             \dst,  \dst,  #8
.else
        add             \dst,  \dst,  #16
.endif
        b               1b
0:
        pop             {r4-r11,pc}
endfunc
.endm

filter_fn put,  r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, 10
filter_fn prep, r0, r7, r1, r2, r3, r4, r5, r6, r8, r9, 6
