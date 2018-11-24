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

#if BITDEPTH == 8

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
        rbit            r4,  r4
        adr             r12, L(\type\()_tbl)
        clz             r4,  r4
        ldr             r4,  [r12, r4, lsl #2]
        \type           d16, d17, q0,  q1,  q2,  q3
        add             r12, r12, r4
        bx              r12
        .align 2
L(\type\()_tbl):
        .word 0, 0
        .word 4f    - L(\type\()_tbl) + CONFIG_THUMB
        .word 80f   - L(\type\()_tbl) + CONFIG_THUMB
        .word 160f  - L(\type\()_tbl) + CONFIG_THUMB
        .word 320f  - L(\type\()_tbl) + CONFIG_THUMB
        .word 640f  - L(\type\()_tbl) + CONFIG_THUMB
        .word 1280f - L(\type\()_tbl) + CONFIG_THUMB
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

#endif /* BITDEPTH == 8 */
