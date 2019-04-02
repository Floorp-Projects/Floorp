/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2019, Martin Storsjo
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

// void dav1d_wiener_filter_h_neon(int16_t *dst, const pixel (*left)[4],
//                                 const pixel *src, ptrdiff_t stride,
//                                 const int16_t fh[7], const intptr_t w,
//                                 int h, enum LrEdgeFlags edges);
function wiener_filter_h_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4}
        ldrd            r4,  r5,  [sp, #52]
        ldrd            r6,  r7,  [sp, #60]
        mov             r8,  r5
        vld1.16         {q0},  [r4]
        movw            r9,  #(1 << 14) - (1 << 2)
        vdup.16         q14,  r9
        vmov.s16        q15,  #2048
        // Calculate mid_stride
        add             r10, r5,  #7
        bic             r10, r10, #7
        lsl             r10, r10, #1

        // Clear the last unused element of q0, to allow filtering a single
        // pixel with one plain vmul+vpadd.
        mov             r12, #0
        vmov.16         d1[3], r12

        // Set up pointers for reading/writing alternate rows
        add             r12, r0,  r10
        lsl             r10, r10, #1
        add             lr,  r2,  r3
        lsl             r3,  r3,  #1

        // Subtract the width from mid_stride
        sub             r10, r10, r5, lsl #1

        // For w >= 8, we read (w+5)&~7+8 pixels, for w < 8 we read 16 pixels.
        cmp             r5,  #8
        add             r11, r5,  #13
        bic             r11, r11, #7
        bge             1f
        mov             r11, #16
1:
        sub             r3,  r3,  r11

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             r7,  #1 // LR_HAVE_LEFT
        beq             2f
        // LR_HAVE_LEFT
        cmp             r1,  #0
        bne             0f
        // left == NULL
        sub             r2,  r2,  #3
        sub             lr,  lr,  #3
        b               1f
0:      // LR_HAVE_LEFT, left != NULL
2:      // !LR_HAVE_LEFT, increase the stride.
        // For this case we don't read the left 3 pixels from the src pointer,
        // but shift it as if we had done that.
        add             r3,  r3,  #3


1:      // Loop vertically
        vld1.8          {q2},  [r2]!
        vld1.8          {q9},  [lr]!

        tst             r7,  #1 // LR_HAVE_LEFT
        beq             0f
        cmp             r1,  #0
        beq             2f
        // LR_HAVE_LEFT, left != NULL
        vld1.32         {d3[1]},  [r1]!
        // Move r2/lr back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             r2,  r2,  #3
        sub             lr,  lr,  #3
        vld1.32         {d17[1]},  [r1]!
        vext.8          q2,  q1,  q2,  #13
        vext.8          q9,  q8,  q9,  #13
        b               2f
0:
        // !LR_HAVE_LEFT, fill q1 with the leftmost byte
        // and shift q2 to have 3x the first byte at the front.
        vdup.8          q1, d4[0]
        vdup.8          q8, d18[0]
        // Move r2 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             r2,  r2,  #3
        sub             lr,  lr,  #3
        vext.8          q2,  q1,  q2,  #13
        vext.8          q9,  q8,  q9,  #13

2:
        vmovl.u8        q1,  d4
        vmovl.u8        q2,  d5
        vmovl.u8        q8,  d18
        vmovl.u8        q9,  d19

        tst             r7,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             r9,  r5, #14
        ldrb            r11, [r2, r9]
        ldrb            r9,  [lr, r9]
        // Fill q12/q13 with the right padding pixel
        vdup.8          d24, r11
        vdup.8          d26, r9
        vmovl.u8        q12, d24
        vmovl.u8        q13, d26
3:      // !LR_HAVE_RIGHT
        // If we'll have to pad the right edge we need to quit early here.
        cmp             r5,  #11
        bge             4f   // If w >= 11, all used input pixels are valid
        cmp             r5,  #7
        bge             5f   // If w >= 7, we can filter 4 pixels
        b               6f

4:      // Loop horizontally
.macro filter_8
        // This is tuned as some sort of compromise between Cortex A7, A8,
        // A9 and A53.
        vmul.s16        q3,  q1,  d0[0]
        vext.8          q10, q1,  q2,  #2
        vext.8          q11, q1,  q2,  #4
        vmla.s16        q3,  q10, d0[1]
        vmla.s16        q3,  q11, d0[2]
        vext.8          q10, q1,  q2,  #6
        vext.8          q11, q1,  q2,  #8
        vmla.s16        q3,  q10, d0[3]
        vmla.s16        q3,  q11, d1[0]
        vext.8          q10, q1,  q2,  #10
        vext.8          q11, q1,  q2,  #12
        vmla.s16        q3,  q10, d1[1]
        vmla.s16        q3,  q11, d1[2]

        vmul.s16        q10, q8,  d0[0]
        vext.8          q11, q8,  q9,  #2
        vext.8          q4,  q8,  q9,  #4
        vmla.s16        q10, q11, d0[1]
        vmla.s16        q10, q4,  d0[2]
        vext.8          q11, q8,  q9,  #6
        vext.8          q4,  q8,  q9,  #8
        vmla.s16        q10, q11, d0[3]
        vmla.s16        q10, q4,  d1[0]
        vext.8          q11, q8,  q9,  #10
        vext.8          q4,  q8,  q9,  #12
        vmla.s16        q10, q11, d1[1]
        vmla.s16        q10, q4,  d1[2]

        vext.8          q1,  q1,  q2,  #6
        vext.8          q8,  q8,  q9,  #6
        vshl.s16        q1,  q1,  #7
        vshl.s16        q8,  q8,  #7
        vsub.s16        q1,  q1,  q14
        vsub.s16        q8,  q8,  q14
        vqadd.s16       q3,  q3,  q1
        vqadd.s16       q10, q10, q8
        vshr.s16        q3,  q3,  #3
        vshr.s16        q10, q10, #3
        vadd.s16        q3,  q3,  q15
        vadd.s16        q10, q10, q15
.endm
        filter_8
        vst1.16         {q3},  [r0,  :128]!
        vst1.16         {q10}, [r12, :128]!

        subs            r5,  r5,  #8
        ble             9f
        tst             r7,  #2 // LR_HAVE_RIGHT
        vmov            q1,  q2
        vmov            q8,  q9
        vld1.8          {d4},  [r2]!
        vld1.8          {d18}, [lr]!
        vmovl.u8        q2,  d4
        vmovl.u8        q9,  d18
        bne             4b // If we don't need to pad, just keep filtering.
        b               3b // If we need to pad, check how many pixels we have left.

5:      // Filter 4 pixels, 7 <= w < 11
.macro filter_4
        vmul.s16        d6,  d2,  d0[0]
        vext.8          q10, q1,  q2,  #2
        vext.8          q11, q1,  q2,  #4
        vmla.s16        d6,  d20, d0[1]
        vmla.s16        d6,  d22, d0[2]
        vext.8          q10, q1,  q2,  #6
        vext.8          q11, q1,  q2,  #8
        vmla.s16        d6,  d20, d0[3]
        vmla.s16        d6,  d22, d1[0]
        vext.8          q10, q1,  q2,  #10
        vext.8          q11, q1,  q2,  #12
        vmla.s16        d6,  d20, d1[1]
        vmla.s16        d6,  d22, d1[2]

        vmul.s16        d20, d16, d0[0]
        vext.8          q11, q8,  q9,  #2
        vext.8          q4,  q8,  q9,  #4
        vmla.s16        d20, d22, d0[1]
        vmla.s16        d20, d8,  d0[2]
        vext.8          q11, q8,  q9,  #6
        vext.8          q4,  q8,  q9,  #8
        vmla.s16        d20, d22, d0[3]
        vmla.s16        d20, d8,  d1[0]
        vext.8          q11, q8,  q9,  #10
        vext.8          q4,  q8,  q9,  #12
        vmla.s16        d20, d22, d1[1]
        vmla.s16        d20, d8,  d1[2]

        vext.8          q11, q1,  q2,  #6
        vshl.s16        d22, d22, #7
        vsub.s16        d22, d22, d28
        vqadd.s16       d6,  d6,  d22
        vext.8          q11, q8,  q9,  #6
        vshl.s16        d22, d22, #7
        vsub.s16        d22, d22, d28
        vqadd.s16       d20, d20, d22
        vshr.s16        d6,  d6,  #3
        vshr.s16        d20, d20, #3
        vadd.s16        d6,  d6,  d30
        vadd.s16        d20, d20, d30
.endm
        filter_4
        vst1.16         {d6},  [r0,  :64]!
        vst1.16         {d20}, [r12, :64]!

        subs            r5,  r5,  #4 // 3 <= w < 7
        vext.8          q1,  q1,  q2,  #8
        vext.8          q2,  q2,  q2,  #8
        vext.8          q8,  q8,  q9,  #8
        vext.8          q9,  q9,  q9,  #8

6:      // Pad the right edge and filter the last few pixels.
        // w < 7, w+3 pixels valid in q1-q2
        cmp             r5,  #5
        blt             7f
        bgt             8f
        // w == 5, 8 pixels valid in q1, q2 invalid
        vmov            q2,  q12
        vmov            q9,  q13
        b               88f

7:      // 1 <= w < 5, 4-7 pixels valid in q1
        sub             r9,  r5,  #1
        // w9 = (pixels valid - 4)
        adr             r11, L(variable_shift_tbl)
        ldr             r9,  [r11, r9, lsl #2]
        add             r11, r11, r9
        vmov            q2,  q12
        vmov            q9,  q13
        bx              r11

        .align 2
L(variable_shift_tbl):
        .word 44f - L(variable_shift_tbl) + CONFIG_THUMB
        .word 55f - L(variable_shift_tbl) + CONFIG_THUMB
        .word 66f - L(variable_shift_tbl) + CONFIG_THUMB
        .word 77f - L(variable_shift_tbl) + CONFIG_THUMB

44:     // 4 pixels valid in d2/d16, fill d3/d17 with padding.
        vmov            d3,  d4
        vmov            d17, d18
        b               88f
        // Shift q1 right, shifting out invalid pixels,
        // shift q1 left to the original offset, shifting in padding pixels.
55:     // 5 pixels valid
        vext.8          q1,  q1,  q1,  #10
        vext.8          q1,  q1,  q2,  #6
        vext.8          q8,  q8,  q8,  #10
        vext.8          q8,  q8,  q9,  #6
        b               88f
66:     // 6 pixels valid
        vext.8          q1,  q1,  q1,  #12
        vext.8          q1,  q1,  q2,  #4
        vext.8          q8,  q8,  q8,  #12
        vext.8          q8,  q8,  q9,  #4
        b               88f
77:     // 7 pixels valid
        vext.8          q1,  q1,  q1,  #14
        vext.8          q1,  q1,  q2,  #2
        vext.8          q8,  q8,  q8,  #14
        vext.8          q8,  q8,  q9,  #2
        b               88f

8:      // w > 5, w == 6, 9 pixels valid in q1-q2, 1 pixel valid in q2
        vext.8          q2,  q2,  q2,  #2
        vext.8          q2,  q2,  q12, #14
        vext.8          q9,  q9,  q9,  #2
        vext.8          q9,  q9,  q13, #14

88:
        // w < 7, q1-q2 padded properly
        cmp             r5,  #4
        blt             888f

        // w >= 4, filter 4 pixels
        filter_4
        vst1.16         {d6},  [r0,  :64]!
        vst1.16         {d20}, [r12, :64]!
        subs            r5,  r5,  #4 // 0 <= w < 4
        vext.8          q1,  q1,  q2,  #8
        vext.8          q8,  q8,  q9,  #8
        beq             9f
888:    // 1 <= w < 4, filter 1 pixel at a time
        vmul.s16        q3,  q1,  q0
        vmul.s16        q10, q8,  q0
        vpadd.s16       d6,  d6,  d7
        vpadd.s16       d7,  d20, d21
        vdup.16         d24, d2[3]
        vpadd.s16       d6,  d6,  d7
        vdup.16         d25, d16[3]
        vpadd.s16       d6,  d6,  d6
        vtrn.16         d24, d25
        vshl.s16        d24, d24,  #7
        vsub.s16        d24, d24,  d28
        vqadd.s16       d6,  d6,   d24
        vshr.s16        d6,  d6,   #3
        vadd.s16        d6,  d6,   d30
        vst1.s16        {d6[0]}, [r0,  :16]!
        vst1.s16        {d6[1]}, [r12, :16]!
        subs            r5,  r5,  #1
        vext.8          q1,  q1,  q2,  #2
        vext.8          q8,  q8,  q9,  #2
        bgt             888b

9:
        subs            r6,  r6,  #2
        ble             0f
        // Jump to the next row and loop horizontally
        add             r0,  r0,  r10
        add             r12, r12, r10
        add             r2,  r2,  r3
        add             lr,  lr,  r3
        mov             r5,  r8
        b               1b
0:
        vpop            {q4}
        pop             {r4-r11,pc}
.purgem filter_8
.purgem filter_4
endfunc

// void dav1d_wiener_filter_v_neon(pixel *dst, ptrdiff_t stride,
//                                 const int16_t *mid, int w, int h,
//                                 const int16_t fv[7], enum LrEdgeFlags edges,
//                                 ptrdiff_t mid_stride);
function wiener_filter_v_neon, export=1
        push            {r4-r7,lr}
        ldrd            r4,  r5,  [sp, #20]
        ldrd            r6,  r7,  [sp, #28]
        mov             lr,  r4
        vmov.s16        q1,  #0
        mov             r12, #128
        vld1.16         {q0},  [r5]
        vmov.s16        d2[3], r12
        vadd.s16        q0,  q0,  q1

        // Calculate the number of rows to move back when looping vertically
        mov             r12, r4
        tst             r6,  #4 // LR_HAVE_TOP
        beq             0f
        sub             r2,  r2,  r7,  lsl #1
        add             r12, r12, #2
0:
        tst             r6,  #8 // LR_HAVE_BOTTOM
        beq             1f
        add             r12, r12, #2

1:      // Start of horizontal loop; start one vertical filter slice.
        // Load rows into q8-q11 and pad properly.
        tst             r6,  #4 // LR_HAVE_TOP
        vld1.16         {q8},  [r2, :128], r7
        beq             2f
        // LR_HAVE_TOP
        vld1.16         {q10}, [r2, :128], r7
        vmov            q9,  q8
        vld1.16         {q11}, [r2, :128], r7
        b               3f
2:      // !LR_HAVE_TOP
        vmov            q9,  q8
        vmov            q10, q8
        vmov            q11, q8

3:
        cmp             r4,  #4
        blt             5f
        // Start filtering normally; fill in q12-q14 with unique rows.
        vld1.16         {q12}, [r2, :128], r7
        vld1.16         {q13}, [r2, :128], r7
        vld1.16         {q14}, [r2, :128], r7

4:
.macro filter compare
        subs            r4,  r4,  #1
        // Interleaving the mul/mla chains actually hurts performance
        // significantly on Cortex A53, thus keeping mul/mla tightly
        // chained like this.
        vmull.s16       q2,  d16,  d0[0]
        vmlal.s16       q2,  d18,  d0[1]
        vmlal.s16       q2,  d20,  d0[2]
        vmlal.s16       q2,  d22,  d0[3]
        vmlal.s16       q2,  d24,  d1[0]
        vmlal.s16       q2,  d26,  d1[1]
        vmlal.s16       q2,  d28,  d1[2]
        vmull.s16       q3,  d17,  d0[0]
        vmlal.s16       q3,  d19,  d0[1]
        vmlal.s16       q3,  d21,  d0[2]
        vmlal.s16       q3,  d23,  d0[3]
        vmlal.s16       q3,  d25,  d1[0]
        vmlal.s16       q3,  d27,  d1[1]
        vmlal.s16       q3,  d29,  d1[2]
        vqrshrun.s32    d4,  q2,   #11
        vqrshrun.s32    d5,  q3,   #11
        vqmovun.s16     d4,  q2
        vst1.8          {d4}, [r0], r1
.if \compare
        cmp             r4,  #4
.else
        ble             9f
.endif
        vmov            q8,  q9
        vmov            q9,  q10
        vmov            q10, q11
        vmov            q11, q12
        vmov            q12, q13
        vmov            q13, q14
.endm
        filter          1
        blt             7f
        vld1.16         {q14}, [r2, :128], r7
        b               4b

5:      // Less than 4 rows in total; not all of q12-q13 are filled yet.
        tst             r6,  #8 // LR_HAVE_BOTTOM
        beq             6f
        // LR_HAVE_BOTTOM
        cmp             r4,  #2
        // We load at least 2 rows in all cases.
        vld1.16         {q12}, [r2, :128], r7
        vld1.16         {q13}, [r2, :128], r7
        bgt             53f // 3 rows in total
        beq             52f // 2 rows in total
51:     // 1 row in total, q11 already loaded, load edge into q12-q14.
        vmov            q13, q12
        b               8f
52:     // 2 rows in total, q11 already loaded, load q12 with content data
        // and 2 rows of edge.
        vld1.16         {q14}, [r2, :128], r7
        vmov            q15,  q14
        b               8f
53:
        // 3 rows in total, q11 already loaded, load q12 and q13 with content
        // and 2 rows of edge.
        vld1.16         {q14}, [r2, :128], r7
        vld1.16         {q15}, [r2, :128], r7
        vmov            q1,  q15
        b               8f

6:
        // !LR_HAVE_BOTTOM
        cmp             r4,  #2
        bgt             63f // 3 rows in total
        beq             62f // 2 rows in total
61:     // 1 row in total, q11 already loaded, pad that into q12-q14.
        vmov            q12, q11
        vmov            q13, q11
        vmov            q14, q11
        b               8f
62:     // 2 rows in total, q11 already loaded, load q12 and pad that into q12-q15.
        vld1.16         {q12}, [r2, :128], r7
        vmov            q13, q12
        vmov            q14, q12
        vmov            q15, q12
        b               8f
63:
        // 3 rows in total, q11 already loaded, load q12 and q13 and pad q13 into q14-q15,q1.
        vld1.16         {q12}, [r2, :128], r7
        vld1.16         {q13}, [r2, :128], r7
        vmov            q14, q13
        vmov            q15, q13
        vmov            q1,  q13
        b               8f

7:
        // All registers up to q13 are filled already, 3 valid rows left.
        // < 4 valid rows left; fill in padding and filter the last
        // few rows.
        tst             r6,  #8 // LR_HAVE_BOTTOM
        beq             71f
        // LR_HAVE_BOTTOM; load 2 rows of edge.
        vld1.16         {q14}, [r2, :128], r7
        vld1.16         {q15}, [r2, :128], r7
        vmov            q1,  q15
        b               8f
71:
        // !LR_HAVE_BOTTOM, pad 3 rows
        vmov            q14, q13
        vmov            q15, q13
        vmov            q1,  q13

8:      // At this point, all registers up to q14-15,q1 are loaded with
        // edge/padding (depending on how many rows are left).
        filter          0 // This branches to 9f when done
        vmov            q14, q15
        vmov            q15, q1
        b               8b

9:      // End of one vertical slice.
        subs            r3,  r3,  #8
        ble             0f
        // Move pointers back up to the top and loop horizontally.
        mls             r0,  r1,  lr,  r0
        mls             r2,  r7,  r12, r2
        add             r0,  r0,  #8
        add             r2,  r2,  #16
        mov             r4,  lr
        b               1b

0:
        pop             {r4-r7,pc}
.purgem filter
endfunc

// void dav1d_copy_narrow_neon(pixel *dst, ptrdiff_t stride,
//                             const pixel *src, int w, int h);
function copy_narrow_neon, export=1
        push            {r4,lr}
        ldr             r4, [sp, #8]
        adr             r12, L(copy_narrow_tbl)
        ldr             r3,  [r12, r3, lsl #2]
        add             r12, r12, r3
        bx              r12

        .align 2
L(copy_narrow_tbl):
        .word 0
        .word 10f - L(copy_narrow_tbl) + CONFIG_THUMB
        .word 20f - L(copy_narrow_tbl) + CONFIG_THUMB
        .word 30f - L(copy_narrow_tbl) + CONFIG_THUMB
        .word 40f - L(copy_narrow_tbl) + CONFIG_THUMB
        .word 50f - L(copy_narrow_tbl) + CONFIG_THUMB
        .word 60f - L(copy_narrow_tbl) + CONFIG_THUMB
        .word 70f - L(copy_narrow_tbl) + CONFIG_THUMB

10:
        add             r3,  r0,  r1
        lsl             r1,  r1,  #1
18:
        subs            r4,  r4,  #8
        blt             110f
        vld1.8          {d0}, [r2, :64]!
        vst1.8          {d0[0]}, [r0], r1
        vst1.8          {d0[1]}, [r3], r1
        vst1.8          {d0[2]}, [r0], r1
        vst1.8          {d0[3]}, [r3], r1
        vst1.8          {d0[4]}, [r0], r1
        vst1.8          {d0[5]}, [r3], r1
        vst1.8          {d0[6]}, [r0], r1
        vst1.8          {d0[7]}, [r3], r1
        ble             0f
        b               18b
110:
        add             r4,  r4,  #8
        asr             r1,  r1,  #1
11:
        subs            r4,  r4,  #1
        vld1.8          {d0[]},  [r2]!
        vst1.8          {d0[0]}, [r0], r1
        bgt             11b
0:
        pop             {r4,pc}

20:
        add             r3,  r0,  r1
        lsl             r1,  r1,  #1
24:
        subs            r4,  r4,  #4
        blt             210f
        vld1.16         {d0}, [r2, :64]!
        vst1.16         {d0[0]}, [r0, :16], r1
        vst1.16         {d0[1]}, [r3, :16], r1
        vst1.16         {d0[2]}, [r0, :16], r1
        vst1.16         {d0[3]}, [r3, :16], r1
        ble             0f
        b               24b
210:
        add             r4,  r4,  #4
        asr             r1,  r1,  #1
22:
        subs            r4,  r4,  #1
        vld1.16         {d0[]},  [r2]!
        vst1.16         {d0[0]}, [r0], r1
        bgt             22b
0:
        pop             {r4,pc}

30:
        ldrh            r3,  [r2]
        ldrb            r12, [r2, #2]
        add             r2,  r2,  #3
        subs            r4,  r4,  #1
        strh            r3,  [r0]
        strb            r12, [r0, #2]
        add             r0,  r0,  r1
        bgt             30b
        pop             {r4,pc}

40:
        add             r3,  r0,  r1
        lsl             r1,  r1,  #1
42:
        subs            r4,  r4,  #2
        blt             41f
        vld1.8          {d0}, [r2, :64]!
        vst1.32         {d0[0]}, [r0, :32], r1
        vst1.32         {d0[1]}, [r3, :32], r1
        ble             0f
        b               42b
41:
        vld1.32         {d0[]},  [r2]
        vst1.32         {d0[0]}, [r0]
0:
        pop             {r4,pc}

50:
        ldr             r3,  [r2]
        ldrb            r12, [r2, #4]
        add             r2,  r2,  #5
        subs            r4,  r4,  #1
        str             r3,  [r0]
        strb            r12, [r0, #4]
        add             r0,  r0,  r1
        bgt             50b
        pop             {r4,pc}

60:
        ldr             r3,  [r2]
        ldrh            r12, [r2, #4]
        add             r2,  r2,  #6
        subs            r4,  r4,  #1
        str             r3,  [r0]
        strh            r12, [r0, #4]
        add             r0,  r0,  r1
        bgt             60b
        pop             {r4,pc}

70:
        ldr             r3,  [r2]
        ldrh            r12, [r2, #4]
        ldrb            lr,  [r2, #6]
        add             r2,  r2,  #7
        subs            r4,  r4,  #1
        str             r3,  [r0]
        strh            r12, [r0, #4]
        strb            lr,  [r0, #6]
        add             r0,  r0,  r1
        bgt             70b
        pop             {r4,pc}
endfunc
