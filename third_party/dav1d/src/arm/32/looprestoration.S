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
#include "util.S"

// void dav1d_wiener_filter_h_8bpc_neon(int16_t *dst, const pixel (*left)[4],
//                                      const pixel *src, ptrdiff_t stride,
//                                      const int16_t fh[7], const intptr_t w,
//                                      int h, enum LrEdgeFlags edges);
function wiener_filter_h_8bpc_neon, export=1
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
        // r9 = (pixels valid - 4)
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

// void dav1d_wiener_filter_v_8bpc_neon(pixel *dst, ptrdiff_t stride,
//                                      const int16_t *mid, int w, int h,
//                                      const int16_t fv[7], enum LrEdgeFlags edges,
//                                      ptrdiff_t mid_stride);
function wiener_filter_v_8bpc_neon, export=1
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

// void dav1d_copy_narrow_8bpc_neon(pixel *dst, ptrdiff_t stride,
//                                  const pixel *src, int w, int h);
function copy_narrow_8bpc_neon, export=1
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

#define SUM_STRIDE (384+16)

// void dav1d_sgr_box3_h_8bpc_neon(int32_t *sumsq, int16_t *sum,
//                                 const pixel (*left)[4],
//                                 const pixel *src, const ptrdiff_t stride,
//                                 const int w, const int h,
//                                 const enum LrEdgeFlags edges);
function sgr_box3_h_8bpc_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]
        ldrd            r6,  r7,  [sp, #108]
        add             r5,  r5,  #2 // w += 2

        // Set up pointers for reading/writing alternate rows
        add             r10, r0,  #(4*SUM_STRIDE)   // sumsq
        add             r11, r1,  #(2*SUM_STRIDE)   // sum
        add             r12, r3,  r4                // src
        lsl             r4,  r4,  #1
        mov             r9,       #(2*2*SUM_STRIDE) // double sum stride

        // Subtract the aligned width from the output stride.
        // With LR_HAVE_RIGHT, align to 8, without it, align to 4.
        tst             r7,  #2 // LR_HAVE_RIGHT
        bne             0f
        // !LR_HAVE_RIGHT
        add             lr,  r5,  #3
        bic             lr,  lr,  #3
        b               1f
0:
        add             lr,  r5,  #7
        bic             lr,  lr,  #7
1:
        sub             r9,  r9,  lr, lsl #1

        // Store the width for the vertical loop
        mov             r8,  r5

        // Subtract the number of pixels read from the input from the stride
        add             lr,  r5,  #14
        bic             lr,  lr,  #7
        sub             r4,  r4,  lr

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             r7,  #1 // LR_HAVE_LEFT
        beq             2f
        // LR_HAVE_LEFT
        cmp             r2,  #0
        bne             0f
        // left == NULL
        sub             r3,  r3,  #2
        sub             r12, r12, #2
        b               1f
0:      // LR_HAVE_LEFT, left != NULL
2:      // !LR_HAVE_LEFT, increase the stride.
        // For this case we don't read the left 2 pixels from the src pointer,
        // but shift it as if we had done that.
        add             r4,  r4,  #2


1:      // Loop vertically
        vld1.8          {q0}, [r3]!
        vld1.8          {q4}, [r12]!

        tst             r7,  #1 // LR_HAVE_LEFT
        beq             0f
        cmp             r2,  #0
        beq             2f
        // LR_HAVE_LEFT, left != NULL
        vld1.32         {d3[]}, [r2]!
        // Move r3/r12 back to account for the last 2 bytes we loaded earlier,
        // which we'll shift out.
        sub             r3,  r3,  #2
        sub             r12, r12, #2
        vld1.32         {d11[]}, [r2]!
        vext.8          q0,  q1,  q0,  #14
        vext.8          q4,  q5,  q4,  #14
        b               2f
0:
        // !LR_HAVE_LEFT, fill q1 with the leftmost byte
        // and shift q0 to have 2x the first byte at the front.
        vdup.8          q1,  d0[0]
        vdup.8          q5,  d8[0]
        // Move r3 back to account for the last 2 bytes we loaded before,
        // which we shifted out.
        sub             r3,  r3,  #2
        sub             r12, r12, #2
        vext.8          q0,  q1,  q0,  #14
        vext.8          q4,  q5,  q4,  #14

2:
        vmull.u8        q1,  d0,  d0
        vmull.u8        q2,  d1,  d1
        vmull.u8        q5,  d8,  d8
        vmull.u8        q6,  d9,  d9

        tst             r7,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             lr,  r5, #(2 + 16 - 2 + 1)
        ldrb            r11, [r3,  lr]
        ldrb            lr,  [r12, lr]
        // Fill q14/q15 with the right padding pixel
        vdup.8          q14, r11
        vdup.8          q15, lr
        // Restore r11 after using it for a temporary value
        add             r11, r1,  #(2*SUM_STRIDE)
3:      // !LR_HAVE_RIGHT
        // If we'll have to pad the right edge we need to quit early here.
        cmp             r5,  #10
        bge             4f   // If w >= 10, all used input pixels are valid
        cmp             r5,  #6
        bge             5f   // If w >= 6, we can filter 4 pixels
        b               6f

4:      // Loop horizontally
.macro vaddl_u16_n      dst1, dst2, src1, src2, src3, src4, w
        vaddl.u16       \dst1,  \src1,  \src3
.if \w > 4
        vaddl.u16       \dst2,  \src2,  \src4
.endif
.endm
.macro vaddw_u16_n      dst1, dst2, src1, src2, w
        vaddw.u16       \dst1,  \dst1,  \src1
.if \w > 4
        vaddw.u16       \dst2,  \dst2,  \src2
.endif
.endm
.macro vadd_i32_n       dst1, dst2, src1, src2, w
        vadd.i32        \dst1,  \dst1,  \src1
.if \w > 4
        vadd.i32        \dst2,  \dst2,  \src2
.endif
.endm

.macro add3 w
        vext.8          d16, d0,  d1,  #1
        vext.8          d17, d0,  d1,  #2
        vext.8          d18, d8,  d9,  #1
        vext.8          d19, d8,  d9,  #2
        vaddl.u8        q3,  d0,  d16
        vaddw.u8        q3,  q3,  d17
        vaddl.u8        q7,  d8,  d18
        vaddw.u8        q7,  q7,  d19

        vext.8          q8,  q1,  q2,  #2
        vext.8          q9,  q1,  q2,  #4
        vext.8          q10, q5,  q6,  #2
        vext.8          q11, q5,  q6,  #4

        vaddl_u16_n     q12, q13, d2,  d3,  d16, d17, \w
        vaddw_u16_n     q12, q13, d18, d19, \w

        vaddl_u16_n     q8,  q9,  d10, d11, d20, d21, \w
        vaddw_u16_n     q8,  q9,  d22, d23, \w
.endm
        add3            8
        vst1.16         {q3},       [r1,  :128]!
        vst1.16         {q7},       [r11, :128]!
        vst1.32         {q12, q13}, [r0,  :128]!
        vst1.32         {q8,  q9},  [r10, :128]!

        subs            r5,  r5,  #8
        ble             9f
        tst             r7,  #2 // LR_HAVE_RIGHT
        vld1.8          {d6},  [r3]!
        vld1.8          {d14}, [r12]!
        vmov            q1,  q2
        vmov            q5,  q6
        vext.8          q0,  q0,  q3,  #8
        vext.8          q4,  q4,  q7,  #8
        vmull.u8        q2,  d6,  d6
        vmull.u8        q6,  d14, d14

        bne             4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

5:      // Produce 4 pixels, 6 <= w < 10
        add3            4
        vst1.16         {d6},  [r1,  :64]!
        vst1.16         {d14}, [r11, :64]!
        vst1.32         {q12}, [r0,  :128]!
        vst1.32         {q8},  [r10, :128]!

        subs            r5,  r5,  #4 // 2 <= w < 6
        vext.8          q0,  q0,  q0,  #4
        vext.8          q4,  q4,  q4,  #4

6:      // Pad the right edge and produce the last few pixels.
        // 2 <= w < 6, 2-5 pixels valid in q0
        sub             lr,  r5,  #2
        // lr = (pixels valid - 2)
        adr             r11, L(box3_variable_shift_tbl)
        ldr             lr,  [r11, lr, lsl #2]
        add             r11, r11, lr
        bx              r11

        .align 2
L(box3_variable_shift_tbl):
        .word 22f - L(box3_variable_shift_tbl) + CONFIG_THUMB
        .word 33f - L(box3_variable_shift_tbl) + CONFIG_THUMB
        .word 44f - L(box3_variable_shift_tbl) + CONFIG_THUMB
        .word 55f - L(box3_variable_shift_tbl) + CONFIG_THUMB

        // Shift q0 right, shifting out invalid pixels,
        // shift q0 left to the original offset, shifting in padding pixels.
22:     // 2 pixels valid
        vext.8          q0,  q0,  q0,  #2
        vext.8          q4,  q4,  q4,  #2
        vext.8          q0,  q0,  q14, #14
        vext.8          q4,  q4,  q15, #14
        b               88f
33:     // 3 pixels valid
        vext.8          q0,  q0,  q0,  #3
        vext.8          q4,  q4,  q4,  #3
        vext.8          q0,  q0,  q14, #13
        vext.8          q4,  q4,  q15, #13
        b               88f
44:     // 4 pixels valid
        vext.8          q0,  q0,  q0,  #4
        vext.8          q4,  q4,  q4,  #4
        vext.8          q0,  q0,  q14, #12
        vext.8          q4,  q4,  q15, #12
        b               88f
55:     // 5 pixels valid
        vext.8          q0,  q0,  q0,  #5
        vext.8          q4,  q4,  q4,  #5
        vext.8          q0,  q0,  q14, #11
        vext.8          q4,  q4,  q15, #11

88:
        // Restore r11 after using it for a temporary value above
        add             r11, r1,  #(2*SUM_STRIDE)
        vmull.u8        q1,  d0,  d0
        vmull.u8        q2,  d1,  d1
        vmull.u8        q5,  d8,  d8
        vmull.u8        q6,  d9,  d9

        add3            4
        subs            r5,  r5,  #4
        vst1.16         {d6},  [r1,  :64]!
        vst1.16         {d14}, [r11, :64]!
        vst1.32         {q12}, [r0,  :128]!
        vst1.32         {q8},  [r10, :128]!
        ble             9f
        vext.8          q0,  q0,  q0,  #4
        vext.8          q1,  q1,  q2,  #8
        vext.8          q4,  q4,  q4,  #4
        vext.8          q5,  q5,  q6,  #8
        // Only one needed pixel left, but do a normal 4 pixel
        // addition anyway
        add3            4
        vst1.16         {d6},  [r1,  :64]!
        vst1.16         {d14}, [r11, :64]!
        vst1.32         {q12}, [r0,  :128]!
        vst1.32         {q8},  [r10, :128]!

9:
        subs            r6,  r6,  #2
        ble             0f
        // Jump to the next row and loop horizontally
        add             r0,  r0,  r9, lsl #1
        add             r10, r10, r9, lsl #1
        add             r1,  r1,  r9
        add             r11, r11, r9
        add             r3,  r3,  r4
        add             r12, r12, r4
        mov             r5,  r8
        b               1b
0:
        vpop            {q4-q7}
        pop             {r4-r11,pc}
.purgem add3
endfunc

// void dav1d_sgr_box5_h_8bpc_neon(int32_t *sumsq, int16_t *sum,
//                                 const pixel (*left)[4],
//                                 const pixel *src, const ptrdiff_t stride,
//                                 const int w, const int h,
//                                 const enum LrEdgeFlags edges);
function sgr_box5_h_8bpc_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]
        ldrd            r6,  r7,  [sp, #108]
        add             r5,  r5,  #2 // w += 2

        // Set up pointers for reading/writing alternate rows
        add             r10, r0,  #(4*SUM_STRIDE)   // sumsq
        add             r11, r1,  #(2*SUM_STRIDE)   // sum
        add             r12, r3,  r4                // src
        lsl             r4,  r4,  #1
        mov             r9,       #(2*2*SUM_STRIDE) // double sum stride

        // Subtract the aligned width from the output stride.
        // With LR_HAVE_RIGHT, align to 8, without it, align to 4.
        // Subtract the number of pixels read from the input from the stride.
        tst             r7,  #2 // LR_HAVE_RIGHT
        bne             0f
        // !LR_HAVE_RIGHT
        add             lr,  r5,  #3
        bic             lr,  lr,  #3
        add             r8,  r5,  #13
        b               1f
0:
        add             lr,  r5,  #7
        bic             lr,  lr,  #7
        add             r8,  r5,  #15
1:
        sub             r9,  r9,  lr, lsl #1
        bic             r8,  r8,  #7
        sub             r4,  r4,  r8

        // Store the width for the vertical loop
        mov             r8,  r5

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             r7,  #1 // LR_HAVE_LEFT
        beq             2f
        // LR_HAVE_LEFT
        cmp             r2,  #0
        bne             0f
        // left == NULL
        sub             r3,  r3,  #3
        sub             r12, r12, #3
        b               1f
0:      // LR_HAVE_LEFT, left != NULL
2:      // !LR_HAVE_LEFT, increase the stride.
        // For this case we don't read the left 3 pixels from the src pointer,
        // but shift it as if we had done that.
        add             r4,  r4,  #3

1:      // Loop vertically
        vld1.8          {q0}, [r3]!
        vld1.8          {q4}, [r12]!

        tst             r7,  #1 // LR_HAVE_LEFT
        beq             0f
        cmp             r2,  #0
        beq             2f
        // LR_HAVE_LEFT, left != NULL
        vld1.32         {d3[]}, [r2]!
        // Move r3/r12 back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             r3,  r3,  #3
        sub             r12, r12, #3
        vld1.32         {d11[]}, [r2]!
        vext.8          q0,  q1,  q0,  #13
        vext.8          q4,  q5,  q4,  #13
        b               2f
0:
        // !LR_HAVE_LEFT, fill q1 with the leftmost byte
        // and shift q0 to have 3x the first byte at the front.
        vdup.8          q1,  d0[0]
        vdup.8          q5,  d8[0]
        // Move r3 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             r3,  r3,  #3
        sub             r12, r12, #3
        vext.8          q0,  q1,  q0,  #13
        vext.8          q4,  q5,  q4,  #13

2:
        vmull.u8        q1,  d0,  d0
        vmull.u8        q2,  d1,  d1
        vmull.u8        q5,  d8,  d8
        vmull.u8        q6,  d9,  d9

        tst             r7,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             lr,  r5, #(2 + 16 - 3 + 1)
        ldrb            r11, [r3,  lr]
        ldrb            lr,  [r12, lr]
        // Fill q14/q15 with the right padding pixel
        vdup.8          q14, r11
        vdup.8          q15, lr
        // Restore r11 after using it for a temporary value
        add             r11, r1,  #(2*SUM_STRIDE)
3:      // !LR_HAVE_RIGHT
        // If we'll have to pad the right edge we need to quit early here.
        cmp             r5,  #11
        bge             4f   // If w >= 11, all used input pixels are valid
        cmp             r5,  #7
        bge             5f   // If w >= 7, we can produce 4 pixels
        b               6f

4:      // Loop horizontally
.macro add5 w
        vext.8          d16, d0,  d1,  #1
        vext.8          d17, d0,  d1,  #2
        vext.8          d18, d0,  d1,  #3
        vext.8          d19, d0,  d1,  #4
        vext.8          d20, d8,  d9,  #1
        vext.8          d21, d8,  d9,  #2
        vext.8          d22, d8,  d9,  #3
        vext.8          d23, d8,  d9,  #4
        vaddl.u8        q3,  d0,  d16
        vaddl.u8        q12, d17, d18
        vaddl.u8        q7,  d8,  d20
        vaddl.u8        q13, d21, d22
        vaddw.u8        q3,  q3,  d19
        vaddw.u8        q7,  q7,  d23
        vadd.u16        q3,  q3,  q12
        vadd.u16        q7,  q7,  q13

        vext.8          q8,  q1,  q2,  #2
        vext.8          q9,  q1,  q2,  #4
        vext.8          q10, q1,  q2,  #6
        vext.8          q11, q1,  q2,  #8
        vaddl_u16_n     q12, q13, d2,  d3,  d16, d17, \w
        vaddl_u16_n     q8,  q9,  d18, d19, d20, d21, \w
        vaddw_u16_n     q12, q13, d22, d23, \w
        vadd_i32_n      q12, q13, q8,  q9, \w
        vext.8          q8,  q5,  q6,  #2
        vext.8          q9,  q5,  q6,  #4
        vext.8          q10, q5,  q6,  #6
        vext.8          q11, q5,  q6,  #8
.if \w > 4
        vaddl_u16_n     q1,  q5,  d10, d11, d16, d17, 8
        vaddl_u16_n     q8,  q9,  d18, d19, d20, d21, 8
        vaddw_u16_n     q1,  q5,  d22, d23, 8
        vadd.i32        q10, q1,  q8
        vadd.i32        q11, q5,  q9
.else
        // Can't clobber q1/q5 if only doing 4 pixels
        vaddl.u16       q8,  d10, d16
        vaddl.u16       q9,  d18, d20
        vaddw.u16       q8,  q8,  d22
        vadd.i32        q10, q8,  q9
.endif
.endm
        add5            8
        vst1.16         {q3},       [r1,  :128]!
        vst1.16         {q7},       [r11, :128]!
        vst1.32         {q12, q13}, [r0,  :128]!
        vst1.32         {q10, q11}, [r10, :128]!

        subs            r5,  r5,  #8
        ble             9f
        tst             r7,  #2 // LR_HAVE_RIGHT
        vld1.8          {d6},  [r3]!
        vld1.8          {d14}, [r12]!
        vmov            q1,  q2
        vmov            q5,  q6
        vext.8          q0,  q0,  q3,  #8
        vext.8          q4,  q4,  q7,  #8
        vmull.u8        q2,  d6,  d6
        vmull.u8        q6,  d14, d14
        bne             4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

5:      // Produce 4 pixels, 7 <= w < 11
        add5            4
        vst1.16         {d6},  [r1,  :64]!
        vst1.16         {d14}, [r11, :64]!
        vst1.32         {q12}, [r0,  :128]!
        vst1.32         {q10}, [r10, :128]!

        subs            r5,  r5,  #4 // 3 <= w < 7
        vext.8          q0,  q0,  q0,  #4
        vext.8          q4,  q4,  q4,  #4

6:      // Pad the right edge and produce the last few pixels.
        // w < 7, w+1 pixels valid in q0/q4
        sub             lr,   r5,  #1
        // lr = pixels valid - 2
        adr             r11, L(box5_variable_shift_tbl)
        ldr             lr,  [r11, lr, lsl #2]
        add             r11, r11, lr
        bx              r11

        .align 2
L(box5_variable_shift_tbl):
        .word 22f - L(box5_variable_shift_tbl) + CONFIG_THUMB
        .word 33f - L(box5_variable_shift_tbl) + CONFIG_THUMB
        .word 44f - L(box5_variable_shift_tbl) + CONFIG_THUMB
        .word 55f - L(box5_variable_shift_tbl) + CONFIG_THUMB
        .word 66f - L(box5_variable_shift_tbl) + CONFIG_THUMB
        .word 77f - L(box5_variable_shift_tbl) + CONFIG_THUMB

        // Shift q0 right, shifting out invalid pixels,
        // shift q0 left to the original offset, shifting in padding pixels.
22:     // 2 pixels valid
        vext.8          q0,  q0,  q0,  #2
        vext.8          q4,  q4,  q4,  #2
        vext.8          q0,  q0,  q14, #14
        vext.8          q4,  q4,  q15, #14
        b               88f
33:     // 3 pixels valid
        vext.8          q0,  q0,  q0,  #3
        vext.8          q4,  q4,  q4,  #3
        vext.8          q0,  q0,  q14, #13
        vext.8          q4,  q4,  q15, #13
        b               88f
44:     // 4 pixels valid
        vext.8          q0,  q0,  q0,  #4
        vext.8          q4,  q4,  q4,  #4
        vext.8          q0,  q0,  q14, #12
        vext.8          q4,  q4,  q15, #12
        b               88f
55:     // 5 pixels valid
        vext.8          q0,  q0,  q0,  #5
        vext.8          q4,  q4,  q4,  #5
        vext.8          q0,  q0,  q14, #11
        vext.8          q4,  q4,  q15, #11
        b               88f
66:     // 6 pixels valid
        vext.8          q0,  q0,  q0,  #6
        vext.8          q4,  q4,  q4,  #6
        vext.8          q0,  q0,  q14, #10
        vext.8          q4,  q4,  q15, #10
        b               88f
77:     // 7 pixels valid
        vext.8          q0,  q0,  q0,  #7
        vext.8          q4,  q4,  q4,  #7
        vext.8          q0,  q0,  q14, #9
        vext.8          q4,  q4,  q15, #9

88:
        // Restore r11 after using it for a temporary value above
        add             r11, r1,  #(2*SUM_STRIDE)
        vmull.u8        q1,  d0,  d0
        vmull.u8        q2,  d1,  d1
        vmull.u8        q5,  d8,  d8
        vmull.u8        q6,  d9,  d9

        add5            4
        subs            r5,  r5,  #4
        vst1.16         {d6},  [r1,  :64]!
        vst1.16         {d14}, [r11, :64]!
        vst1.32         {q12}, [r0,  :128]!
        vst1.32         {q10}, [r10, :128]!
        ble             9f
        vext.8          q0,  q0,  q0,  #4
        vext.8          q1,  q1,  q2,  #8
        vext.8          q4,  q4,  q4,  #4
        vext.8          q5,  q5,  q6,  #8
        add5            4
        vst1.16         {d6},  [r1,  :64]!
        vst1.16         {d14}, [r11, :64]!
        vst1.32         {q12}, [r0,  :128]!
        vst1.32         {q10}, [r10, :128]!

9:
        subs            r6,  r6,  #2
        ble             0f
        // Jump to the next row and loop horizontally
        add             r0,  r0,  r9, lsl #1
        add             r10, r10, r9, lsl #1
        add             r1,  r1,  r9
        add             r11, r11, r9
        add             r3,  r3,  r4
        add             r12, r12, r4
        mov             r5,  r8
        b               1b
0:
        vpop            {q4-q7}
        pop             {r4-r11,pc}
.purgem add5
endfunc

// void dav1d_sgr_box3_v_neon(int32_t *sumsq, int16_t *sum,
//                            const int w, const int h,
//                            const enum LrEdgeFlags edges);
function sgr_box3_v_neon, export=1
        push            {r4-r9,lr}
        ldr             r4,  [sp, #28]
        add             r12, r3,  #2 // Number of output rows to move back
        mov             lr,  r3      // Number of input rows to move back
        add             r2,  r2,  #2 // Actual summed width
        mov             r7,       #(4*SUM_STRIDE) // sumsq stride
        mov             r8,       #(2*SUM_STRIDE) // sum stride
        sub             r0,  r0,  #(4*SUM_STRIDE) // sumsq -= stride
        sub             r1,  r1,  #(2*SUM_STRIDE) // sum   -= stride

        tst             r4,  #4 // LR_HAVE_TOP
        beq             0f
        // If have top, read from row -2.
        sub             r5,  r0,  #(4*SUM_STRIDE)
        sub             r6,  r1,  #(2*SUM_STRIDE)
        add             lr,  lr,  #2
        b               1f
0:
        // !LR_HAVE_TOP
        // If we don't have top, read from row 0 even if
        // we start writing to row -1.
        add             r5,  r0,  #(4*SUM_STRIDE)
        add             r6,  r1,  #(2*SUM_STRIDE)
1:

        tst             r4,  #8 // LR_HAVE_BOTTOM
        beq             1f
        // LR_HAVE_BOTTOM
        add             r3,  r3,  #2  // Sum all h+2 lines with the main loop
        add             lr,  lr,  #2
1:
        mov             r9,  r3       // Backup of h for next loops

1:
        // Start of horizontal loop; start one vertical filter slice.
        // Start loading rows into q8-q13 and q0-q2 taking top
        // padding into consideration.
        tst             r4,  #4 // LR_HAVE_TOP
        vld1.32         {q8,  q9},  [r5, :128], r7
        vld1.16         {q0},       [r6, :128], r8
        beq             2f
        // LR_HAVE_TOP
        vld1.32         {q10, q11}, [r5, :128], r7
        vld1.16         {q1},       [r6, :128], r8
        vld1.32         {q12, q13}, [r5, :128], r7
        vld1.16         {q2},       [r6, :128], r8
        b               3f
2:      // !LR_HAVE_TOP
        vmov            q10, q8
        vmov            q11, q9
        vmov            q1,  q0
        vmov            q12, q8
        vmov            q13, q9
        vmov            q2,  q0

3:
        subs            r3,  r3,  #1
.macro add3
        vadd.i32        q8,  q8,  q10
        vadd.i32        q9,  q9,  q11
        vadd.i16        q0,  q0,  q1
        vadd.i32        q8,  q8,  q12
        vadd.i32        q9,  q9,  q13
        vadd.i16        q0,  q0,  q2
        vst1.32         {q8, q9}, [r0, :128], r7
        vst1.16         {q0},     [r1, :128], r8
.endm
        add3
        vmov            q8,  q10
        vmov            q9,  q11
        vmov            q0,  q1
        vmov            q10, q12
        vmov            q11, q13
        vmov            q1,  q2
        ble             4f
        vld1.32         {q12, q13}, [r5, :128], r7
        vld1.16         {q2},       [r6, :128], r8
        b               3b

4:
        tst             r4,  #8 // LR_HAVE_BOTTOM
        bne             5f
        // !LR_HAVE_BOTTOM
        // Produce two more rows, extending the already loaded rows.
        add3
        vmov            q8,  q10
        vmov            q9,  q11
        vmov            q0,  q1
        add3

5:      // End of one vertical slice.
        subs            r2,  r2,  #8
        ble             0f
        // Move pointers back up to the top and loop horizontally.
        // Input pointers
        mls             r5,  r7,  lr,  r5
        mls             r6,  r8,  lr,  r6
        // Output pointers
        mls             r0,  r7,  r12, r0
        mls             r1,  r8,  r12, r1
        add             r0,  r0,  #32
        add             r1,  r1,  #16
        add             r5,  r5,  #32
        add             r6,  r6,  #16
        mov             r3,  r9
        b               1b

0:
        pop             {r4-r9,pc}
.purgem add3
endfunc

// void dav1d_sgr_box5_v_neon(int32_t *sumsq, int16_t *sum,
//                            const int w, const int h,
//                            const enum LrEdgeFlags edges);
function sgr_box5_v_neon, export=1
        push            {r4-r9,lr}
        vpush           {q5-q7}
        ldr             r4,  [sp, #76]
        add             r12, r3,  #2 // Number of output rows to move back
        mov             lr,  r3      // Number of input rows to move back
        add             r2,  r2,  #8 // Actual summed width
        mov             r7,       #(4*SUM_STRIDE) // sumsq stride
        mov             r8,       #(2*SUM_STRIDE) // sum stride
        sub             r0,  r0,  #(4*SUM_STRIDE) // sumsq -= stride
        sub             r1,  r1,  #(2*SUM_STRIDE) // sum   -= stride

        tst             r4,  #4 // LR_HAVE_TOP
        beq             0f
        // If have top, read from row -2.
        sub             r5,  r0,  #(4*SUM_STRIDE)
        sub             r6,  r1,  #(2*SUM_STRIDE)
        add             lr,  lr,  #2
        b               1f
0:
        // !LR_HAVE_TOP
        // If we don't have top, read from row 0 even if
        // we start writing to row -1.
        add             r5,  r0,  #(4*SUM_STRIDE)
        add             r6,  r1,  #(2*SUM_STRIDE)
1:

        tst             r4,  #8 // LR_HAVE_BOTTOM
        beq             0f
        // LR_HAVE_BOTTOM
        add             r3,  r3,  #2  // Handle h+2 lines with the main loop
        add             lr,  lr,  #2
        b               1f
0:
        // !LR_HAVE_BOTTOM
        sub             r3,  r3,  #1  // Handle h-1 lines with the main loop
1:
        mov             r9,  r3       // Backup of h for next loops

1:
        // Start of horizontal loop; start one vertical filter slice.
        // Start loading rows into q6-q15 and q0-q3,q5 taking top
        // padding into consideration.
        tst             r4,  #4 // LR_HAVE_TOP
        vld1.32         {q6,  q7},  [r5, :128], r7
        vld1.16         {q0},       [r6, :128], r8
        beq             2f
        // LR_HAVE_TOP
        vld1.32         {q10, q11}, [r5, :128], r7
        vld1.16         {q2},       [r6, :128], r8
        vmov            q8,  q6
        vmov            q9,  q7
        vmov            q1,  q0
        vld1.32         {q12, q13}, [r5, :128], r7
        vld1.16         {q3},       [r6, :128], r8
        b               3f
2:      // !LR_HAVE_TOP
        vmov            q8,  q6
        vmov            q9,  q7
        vmov            q1,  q0
        vmov            q10, q6
        vmov            q11, q7
        vmov            q2,  q0
        vmov            q12, q6
        vmov            q13, q7
        vmov            q3,  q0

3:
        cmp             r3,  #0
        beq             4f
        vld1.32         {q14, q15}, [r5, :128], r7
        vld1.16         {q5},       [r6, :128], r8

3:
        // Start of vertical loop
        subs            r3,  r3,  #2
.macro add5
        vadd.i32        q6,  q6,  q8
        vadd.i32        q7,  q7,  q9
        vadd.i16        q0,  q0,  q1
        vadd.i32        q6,  q6,  q10
        vadd.i32        q7,  q7,  q11
        vadd.i16        q0,  q0,  q2
        vadd.i32        q6,  q6,  q12
        vadd.i32        q7,  q7,  q13
        vadd.i16        q0,  q0,  q3
        vadd.i32        q6,  q6,  q14
        vadd.i32        q7,  q7,  q15
        vadd.i16        q0,  q0,  q5
        vst1.32         {q6, q7}, [r0, :128], r7
        vst1.16         {q0},     [r1, :128], r8
.endm
        add5
.macro shift2
        vmov            q6,  q10
        vmov            q7,  q11
        vmov            q0,  q2
        vmov            q8,  q12
        vmov            q9,  q13
        vmov            q1,  q3
        vmov            q10, q14
        vmov            q11, q15
        vmov            q2,  q5
.endm
        shift2
        add             r0,  r0,  r7
        add             r1,  r1,  r8
        ble             5f
        vld1.32         {q12, q13}, [r5, :128], r7
        vld1.16         {q3},       [r6, :128], r8
        vld1.32         {q14, q15}, [r5, :128], r7
        vld1.16         {q5},       [r6, :128], r8
        b               3b

4:
        // h == 1, !LR_HAVE_BOTTOM.
        // Pad the last row with the only content row, and add.
        vmov            q14, q12
        vmov            q15, q13
        vmov            q5,  q3
        add5
        shift2
        add             r0,  r0,  r7
        add             r1,  r1,  r8
        add5
        b               6f

5:
        tst             r4,  #8 // LR_HAVE_BOTTOM
        bne             6f
        // !LR_HAVE_BOTTOM
        cmp             r3,  #0
        bne             5f
        // The intended three edge rows left; output the one at h-2 and
        // the past edge one at h.
        vld1.32         {q12, q13}, [r5, :128], r7
        vld1.16         {q3},       [r6, :128], r8
        // Pad the past-edge row from the last content row.
        vmov            q14, q12
        vmov            q15, q13
        vmov            q5,  q3
        add5
        shift2
        add             r0,  r0,  r7
        add             r1,  r1,  r8
        // The last two rows are already padded properly here.
        add5
        b               6f

5:
        // r3 == -1, two rows left, output one.
        // Pad the last two rows from the mid one.
        vmov            q12, q10
        vmov            q13, q11
        vmov            q3,  q2
        vmov            q14, q10
        vmov            q15, q11
        vmov            q5,  q2
        add5
        add             r0,  r0,  r7
        add             r1,  r1,  r8
        b               6f

6:      // End of one vertical slice.
        subs            r2,  r2,  #8
        ble             0f
        // Move pointers back up to the top and loop horizontally.
        // Input pointers
        mls             r5,  r7,  lr,  r5
        mls             r6,  r8,  lr,  r6
        // Output pointers
        mls             r0,  r7,  r12, r0
        mls             r1,  r8,  r12, r1
        add             r0,  r0,  #32
        add             r1,  r1,  #16
        add             r5,  r5,  #32
        add             r6,  r6,  #16
        mov             r3,  r9
        b               1b

0:
        vpop            {q5-q7}
        pop             {r4-r9,pc}
.purgem add5
endfunc

// void dav1d_sgr_calc_ab1_neon(int32_t *a, int16_t *b,
//                              const int w, const int h, const int strength);
// void dav1d_sgr_calc_ab2_neon(int32_t *a, int16_t *b,
//                              const int w, const int h, const int strength);
function sgr_calc_ab1_neon, export=1
        push            {r4-r5,lr}
        vpush           {q4-q7}
        ldr             r4,  [sp, #76]
        add             r3,  r3,  #2   // h += 2
        vmov.i32        q15, #9        // n
        movw            r5,  #455
        mov             lr,  #SUM_STRIDE
        b               sgr_calc_ab_neon
endfunc

function sgr_calc_ab2_neon, export=1
        push            {r4-r5,lr}
        vpush           {q4-q7}
        ldr             r4,  [sp, #76]
        add             r3,  r3,  #3   // h += 3
        asr             r3,  r3,  #1   // h /= 2
        vmov.i32        q15, #25       // n
        mov             r5,  #164
        mov             lr,  #(2*SUM_STRIDE)
endfunc

function sgr_calc_ab_neon
        movrel          r12, X(sgr_x_by_x)
        vld1.8          {q8, q9}, [r12, :128]!
        vmov.i8         q11, #5
        vmov.i8         d10, #55       // idx of last 5
        vld1.8          {q10},    [r12, :128]
        vmov.i8         d11, #72       // idx of last 4
        vmov.i8         d12, #101      // idx of last 3
        vmov.i8         d13, #169      // idx of last 2
        vmov.i8         d14, #254      // idx of last 1
        vmov.i8         d15, #32       // elements consumed in first vtbl
        add             r2,  r2,  #2   // w += 2
        add             r12, r2,  #7
        bic             r12, r12, #7   // aligned w
        sub             r12, lr,  r12  // increment between rows
        vmov.i16        q13, #256
        vdup.32         q12, r4
        vdup.32         q14, r5        // one_by_x
        sub             r0,  r0,  #(4*(SUM_STRIDE))
        sub             r1,  r1,  #(2*(SUM_STRIDE))
        mov             r4,  r2        // backup of w
        vsub.i8         q8,  q8,  q11
        vsub.i8         q9,  q9,  q11
        vsub.i8         q10, q10, q11
1:
        subs            r2,  r2,  #8
        vld1.32         {q0, q1}, [r0, :128] // a
        vld1.16         {q2},     [r1, :128] // b
        vmul.i32        q0,  q0,  q15  // a * n
        vmul.i32        q1,  q1,  q15  // a * n
        vmull.u16       q3,  d4,  d4   // b * b
        vmull.u16       q4,  d5,  d5   // b * b
        vqsub.u32       q0,  q0,  q3   // imax(a * n - b * b, 0)
        vqsub.u32       q1,  q1,  q4   // imax(a * n - b * b, 0)
        vmul.i32        q0,  q0,  q12  // p * s
        vmul.i32        q1,  q1,  q12  // p * s
        vqshrn.u32      d0,  q0,  #16
        vqshrn.u32      d1,  q1,  #16
        vqrshrn.u16     d0,  q0,  #4   // imin(z, 255)

        vcgt.u8         d2,  d0,  d10  // = -1 if sgr_x_by_x[d0] < 5
        vcgt.u8         d3,  d0,  d11  // = -1 if sgr_x_by_x[d0] < 4
        vtbl.8          d1,  {q8, q9}, d0
        vcgt.u8         d6,  d0,  d12  // = -1 if sgr_x_by_x[d0] < 3
        vsub.i8         d9,  d0,  d15  // indices for vtbx
        vcgt.u8         d7,  d0,  d13  // = -1 if sgr_x_by_x[d0] < 2
        vadd.i8         d2,  d2,  d3
        vtbx.8          d1,  {q10}, d9
        vcgt.u8         d8,  d0,  d14  // = -1 if sgr_x_by_x[d0] < 1
        vadd.i8         d6,  d6,  d7
        vadd.i8         d8,  d8,  d22
        vadd.i8         d2,  d2,  d6
        vadd.i8         d1,  d1,  d8
        vadd.i8         d1,  d1,  d2
        vmovl.u8        q0,  d1        // x

        vmull.u16       q1,  d0,  d4   // x * BB[i]
        vmull.u16       q2,  d1,  d5   // x * BB[i]
        vmul.i32        q1,  q1,  q14  // x * BB[i] * sgr_one_by_x
        vmul.i32        q2,  q2,  q14  // x * BB[i] * sgr_one_by_x
        vrshr.s32       q1,  q1,  #12  // AA[i]
        vrshr.s32       q2,  q2,  #12  // AA[i]
        vsub.i16        q0,  q13, q0   // 256 - x

        vst1.32         {q1, q2}, [r0, :128]!
        vst1.16         {q0},     [r1, :128]!
        bgt             1b

        subs            r3,  r3,  #1
        ble             0f
        add             r0,  r0,  r12, lsl #2
        add             r1,  r1,  r12, lsl #1
        mov             r2,  r4
        b               1b
0:
        vpop            {q4-q7}
        pop             {r4-r5,pc}
endfunc

#define FILTER_OUT_STRIDE 384

// void dav1d_sgr_finish_filter1_8bpc_neon(int16_t *tmp,
//                                         const pixel *src, const ptrdiff_t stride,
//                                         const int32_t *a, const int16_t *b,
//                                         const int w, const int h);
function sgr_finish_filter1_8bpc_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]
        ldr             r6,  [sp, #108]
        sub             r7,  r3,  #(4*SUM_STRIDE)
        add             r8,  r3,  #(4*SUM_STRIDE)
        sub             r9,  r4,  #(2*SUM_STRIDE)
        add             r10, r4,  #(2*SUM_STRIDE)
        mov             r11, #SUM_STRIDE
        mov             r12, #FILTER_OUT_STRIDE
        add             lr,  r5,  #3
        bic             lr,  lr,  #3 // Aligned width
        sub             r2,  r2,  lr
        sub             r12, r12, lr
        sub             r11, r11, lr
        sub             r11, r11, #4 // We read 4 extra elements from both a and b
        mov             lr,  r5
        vmov.i16        q14, #3
        vmov.i32        q15, #3
1:
        vld1.16         {q0},       [r9]!
        vld1.16         {q1},       [r4]!
        vld1.16         {q2},       [r10]!
        vld1.32         {q8,  q9},  [r7]!
        vld1.32         {q10, q11}, [r3]!
        vld1.32         {q12, q13}, [r8]!

2:
        subs            r5,  r5,  #4
        vext.8          d6,  d0,  d1,  #2  // -stride
        vext.8          d7,  d2,  d3,  #2  // 0
        vext.8          d8,  d4,  d5,  #2  // +stride
        vext.8          d9,  d0,  d1,  #4  // +1-stride
        vext.8          d10, d2,  d3,  #4  // +1
        vext.8          d11, d4,  d5,  #4  // +1+stride
        vadd.i16        d2,  d2,  d6       // -1, -stride
        vadd.i16        d7,  d7,  d8       // 0, +stride
        vadd.i16        d0,  d0,  d9       // -1-stride, +1-stride
        vadd.i16        d2,  d2,  d7
        vadd.i16        d4,  d4,  d11      // -1+stride, +1+stride
        vadd.i16        d2,  d2,  d10      // +1
        vadd.i16        d0,  d0,  d4

        vext.8          q3,  q8,  q9,  #4  // -stride
        vshl.i16        d2,  d2,  #2
        vext.8          q4,  q8,  q9,  #8  // +1-stride
        vext.8          q5,  q10, q11, #4  // 0
        vext.8          q6,  q10, q11, #8  // +1
        vmla.i16        d2,  d0,  d28      // * 3 -> a
        vadd.i32        q3,  q3,  q10      // -stride, -1
        vadd.i32        q8,  q8,  q4       // -1-stride, +1-stride
        vadd.i32        q5,  q5,  q6       // 0, +1
        vadd.i32        q8,  q8,  q12      // -1+stride
        vadd.i32        q3,  q3,  q5
        vext.8          q7,  q12, q13, #4  // +stride
        vext.8          q10, q12, q13, #8  // +1+stride
        vld1.32         {d24[0]}, [r1]!    // src
        vadd.i32        q3,  q3,  q7       // +stride
        vadd.i32        q8,  q8,  q10      // +1+stride
        vshl.i32        q3,  q3,  #2
        vmla.i32        q3,  q8,  q15      // * 3 -> b
        vmovl.u8        q12, d24           // src
        vmov            d0,  d1
        vmlal.u16       q3,  d2,  d24      // b + a * src
        vmov            d2,  d3
        vrshrn.i32      d6,  q3,  #9
        vmov            d4,  d5
        vst1.16         {d6}, [r0]!

        ble             3f
        vmov            q8,  q9
        vmov            q10, q11
        vmov            q12, q13
        vld1.16         {d1},  [r9]!
        vld1.16         {d3},  [r4]!
        vld1.16         {d5},  [r10]!
        vld1.32         {q9},  [r7]!
        vld1.32         {q11}, [r3]!
        vld1.32         {q13}, [r8]!
        b               2b

3:
        subs            r6,  r6,  #1
        ble             0f
        mov             r5,  lr
        add             r0,  r0,  r12, lsl #1
        add             r1,  r1,  r2
        add             r3,  r3,  r11, lsl #2
        add             r7,  r7,  r11, lsl #2
        add             r8,  r8,  r11, lsl #2
        add             r4,  r4,  r11, lsl #1
        add             r9,  r9,  r11, lsl #1
        add             r10, r10, r11, lsl #1
        b               1b
0:
        vpop            {q4-q7}
        pop             {r4-r11,pc}
endfunc

// void dav1d_sgr_finish_filter2_8bpc_neon(int16_t *tmp,
//                                         const pixel *src, const ptrdiff_t stride,
//                                         const int32_t *a, const int16_t *b,
//                                         const int w, const int h);
function sgr_finish_filter2_8bpc_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]
        ldr             r6,  [sp, #108]
        add             r7,  r3,  #(4*(SUM_STRIDE))
        sub             r3,  r3,  #(4*(SUM_STRIDE))
        add             r8,  r4,  #(2*(SUM_STRIDE))
        sub             r4,  r4,  #(2*(SUM_STRIDE))
        mov             r9,  #(2*SUM_STRIDE)
        mov             r10, #FILTER_OUT_STRIDE
        add             r11, r5,  #7
        bic             r11, r11, #7 // Aligned width
        sub             r2,  r2,  r11
        sub             r10, r10, r11
        sub             r9,  r9,  r11
        sub             r9,  r9,  #4 // We read 4 extra elements from a
        sub             r12, r9,  #4 // We read 8 extra elements from b
        mov             lr,  r5

1:
        vld1.16         {q0,  q1},  [r4]!
        vld1.16         {q2,  q3},  [r8]!
        vld1.32         {q8,  q9},  [r3]!
        vld1.32         {q11, q12}, [r7]!
        vld1.32         {q10},      [r3]!
        vld1.32         {q13},      [r7]!

2:
        vmov.i16        q14, #5
        vmov.i16        q15, #6
        subs            r5,  r5,  #8
        vext.8          q4,  q0,  q1,  #4  // +1-stride
        vext.8          q5,  q2,  q3,  #4  // +1+stride
        vext.8          q6,  q0,  q1,  #2  // -stride
        vext.8          q7,  q2,  q3,  #2  // +stride
        vadd.i16        q0,  q0,  q4       // -1-stride, +1-stride
        vadd.i16        q5,  q2,  q5       // -1+stride, +1+stride
        vadd.i16        q2,  q6,  q7       // -stride, +stride
        vadd.i16        q0,  q0,  q5

        vext.8          q4,  q8,  q9,  #8  // +1-stride
        vext.8          q5,  q9,  q10, #8
        vext.8          q6,  q11, q12, #8  // +1+stride
        vext.8          q7,  q12, q13, #8
        vmul.i16        q0,  q0,  q14      // * 5
        vmla.i16        q0,  q2,  q15      // * 6
        vadd.i32        q4,  q4,  q8       // -1-stride, +1-stride
        vadd.i32        q5,  q5,  q9
        vadd.i32        q6,  q6,  q11      // -1+stride, +1+stride
        vadd.i32        q7,  q7,  q12
        vadd.i32        q4,  q4,  q6
        vadd.i32        q5,  q5,  q7
        vext.8          q6,  q8,  q9,  #4  // -stride
        vext.8          q7,  q9,  q10, #4
        vext.8          q8,  q11, q12, #4  // +stride
        vext.8          q11, q12, q13, #4

        vld1.8          {d4}, [r1]!

        vmov.i32        q14, #5
        vmov.i32        q15, #6

        vadd.i32        q6,  q6,  q8       // -stride, +stride
        vadd.i32        q7,  q7,  q11
        vmul.i32        q4,  q4,  q14      // * 5
        vmla.i32        q4,  q6,  q15      // * 6
        vmul.i32        q5,  q5,  q14      // * 5
        vmla.i32        q5,  q7,  q15      // * 6

        vmovl.u8        q2,  d4
        vmlal.u16       q4,  d0,  d4       // b + a * src
        vmlal.u16       q5,  d1,  d5       // b + a * src
        vmov            q0,  q1
        vrshrn.i32      d8,  q4,  #9
        vrshrn.i32      d9,  q5,  #9
        vmov            q2,  q3
        vst1.16         {q4}, [r0]!

        ble             3f
        vmov            q8,  q10
        vmov            q11, q13
        vld1.16         {q1},       [r4]!
        vld1.16         {q3},       [r8]!
        vld1.32         {q9,  q10}, [r3]!
        vld1.32         {q12, q13}, [r7]!
        b               2b

3:
        subs            r6,  r6,  #1
        ble             0f
        mov             r5,  lr
        add             r0,  r0,  r10, lsl #1
        add             r1,  r1,  r2
        add             r3,  r3,  r9,  lsl #2
        add             r7,  r7,  r9,  lsl #2
        add             r4,  r4,  r12, lsl #1
        add             r8,  r8,  r12, lsl #1

        vld1.32         {q8, q9}, [r3]!
        vld1.16         {q0, q1}, [r4]!
        vld1.32         {q10},    [r3]!

        vmov.i16        q12, #5
        vmov.i16        q13, #6

4:
        subs            r5,  r5,  #8
        vext.8          q3,  q0,  q1,  #4  // +1
        vext.8          q2,  q0,  q1,  #2  // 0
        vadd.i16        q0,  q0,  q3       // -1, +1

        vext.8          q4,  q8,  q9,  #4  // 0
        vext.8          q5,  q9,  q10, #4
        vext.8          q6,  q8,  q9,  #8  // +1
        vext.8          q7,  q9,  q10, #8
        vmul.i16        q2,  q2,  q13      // * 6
        vmla.i16        q2,  q0,  q12      // * 5 -> a
        vld1.8          {d22}, [r1]!
        vadd.i32        q8,  q8,  q6       // -1, +1
        vadd.i32        q9,  q9,  q7
        vmovl.u8        q11, d22
        vmul.i32        q4,  q4,  q15      // * 6
        vmla.i32        q4,  q8,  q14      // * 5 -> b
        vmul.i32        q5,  q5,  q15      // * 6
        vmla.i32        q5,  q9,  q14      // * 5 -> b

        vmlal.u16       q4,  d4,  d22      // b + a * src
        vmlal.u16       q5,  d5,  d23
        vmov            q0,  q1
        vrshrn.i32      d8,  q4,  #8
        vrshrn.i32      d9,  q5,  #8
        vmov            q8,  q10
        vst1.16         {q4}, [r0]!

        ble             5f
        vld1.16         {q1},      [r4]!
        vld1.32         {q9, q10}, [r3]!
        b               4b

5:
        subs            r6,  r6,  #1
        ble             0f
        mov             r5,  lr
        sub             r3,  r3,  r11, lsl #2 // Rewind r3/r4 to where they started
        sub             r4,  r4,  r11, lsl #1
        add             r0,  r0,  r10, lsl #1
        add             r1,  r1,  r2
        sub             r3,  r3,  #16
        sub             r4,  r4,  #16
        b               1b
0:
        vpop            {q4-q7}
        pop             {r4-r11,pc}
endfunc

// void dav1d_sgr_weighted1_8bpc_neon(pixel *dst, const ptrdiff_t dst_stride,
//                                    const pixel *src, const ptrdiff_t src_stride,
//                                    const int16_t *t1, const int w, const int h,
//                                    const int wt);
function sgr_weighted1_8bpc_neon, export=1
        push            {r4-r9,lr}
        ldrd            r4,  r5,  [sp, #28]
        ldrd            r6,  r7,  [sp, #36]
        ldr             r8,  [sp, #44]
        vdup.16         d31, r7
        cmp             r6,  #2
        add             r9,  r0,  r1
        add             r12, r2,  r3
        add             lr,  r4,  #2*FILTER_OUT_STRIDE
        mov             r7,  #(4*FILTER_OUT_STRIDE)
        lsl             r1,  r1,  #1
        lsl             r3,  r3,  #1
        add             r8,  r5,  #7
        bic             r8,  r8,  #7 // Aligned width
        sub             r1,  r1,  r8
        sub             r3,  r3,  r8
        sub             r7,  r7,  r8, lsl #1
        mov             r8,  r5
        blt             2f
1:
        vld1.8          {d0},  [r2]!
        vld1.8          {d16}, [r12]!
        vld1.16         {q1},  [r4]!
        vld1.16         {q9},  [lr]!
        subs            r5,  r5,  #8
        vshll.u8        q0,  d0,  #4     // u
        vshll.u8        q8,  d16, #4     // u
        vsub.i16        q1,  q1,  q0     // t1 - u
        vsub.i16        q9,  q9,  q8     // t1 - u
        vshll.u16       q2,  d0,  #7     // u << 7
        vshll.u16       q3,  d1,  #7     // u << 7
        vshll.u16       q10, d16, #7     // u << 7
        vshll.u16       q11, d17, #7     // u << 7
        vmlal.s16       q2,  d2,  d31    // v
        vmlal.s16       q3,  d3,  d31    // v
        vmlal.s16       q10, d18, d31    // v
        vmlal.s16       q11, d19, d31    // v
        vrshrn.i32      d4,  q2,  #11
        vrshrn.i32      d5,  q3,  #11
        vrshrn.i32      d20, q10, #11
        vrshrn.i32      d21, q11, #11
        vqmovun.s16     d4,  q2
        vqmovun.s16     d20, q10
        vst1.8          {d4},  [r0]!
        vst1.8          {d20}, [r9]!
        bgt             1b

        sub             r6,  r6,  #2
        cmp             r6,  #1
        blt             0f
        mov             r5,  r8
        add             r0,  r0,  r1
        add             r9,  r9,  r1
        add             r2,  r2,  r3
        add             r12, r12, r3
        add             r4,  r4,  r7
        add             lr,  lr,  r7
        beq             2f
        b               1b

2:
        vld1.8          {d0}, [r2]!
        vld1.16         {q1}, [r4]!
        subs            r5,  r5,  #8
        vshll.u8        q0,  d0,  #4     // u
        vsub.i16        q1,  q1,  q0     // t1 - u
        vshll.u16       q2,  d0,  #7     // u << 7
        vshll.u16       q3,  d1,  #7     // u << 7
        vmlal.s16       q2,  d2,  d31    // v
        vmlal.s16       q3,  d3,  d31    // v
        vrshrn.i32      d4,  q2,  #11
        vrshrn.i32      d5,  q3,  #11
        vqmovun.s16     d2,  q2
        vst1.8          {d2}, [r0]!
        bgt             2b
0:
        pop             {r4-r9,pc}
endfunc

// void dav1d_sgr_weighted2_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                                    const pixel *src, const ptrdiff_t src_stride,
//                                    const int16_t *t1, const int16_t *t2,
//                                    const int w, const int h,
//                                    const int16_t wt[2]);
function sgr_weighted2_8bpc_neon, export=1
        push            {r4-r11,lr}
        ldrd            r4,  r5,  [sp, #36]
        ldrd            r6,  r7,  [sp, #44]
        ldr             r8,  [sp, #52]
        cmp             r7,  #2
        add             r10, r0,  r1
        add             r11, r2,  r3
        add             r12, r4,  #2*FILTER_OUT_STRIDE
        add             lr,  r5,  #2*FILTER_OUT_STRIDE
        vld2.16         {d30[], d31[]}, [r8] // wt[0], wt[1]
        mov             r8,  #4*FILTER_OUT_STRIDE
        lsl             r1,  r1,  #1
        lsl             r3,  r3,  #1
        add             r9,  r6,  #7
        bic             r9,  r9,  #7 // Aligned width
        sub             r1,  r1,  r9
        sub             r3,  r3,  r9
        sub             r8,  r8,  r9, lsl #1
        mov             r9,  r6
        blt             2f
1:
        vld1.8          {d0},  [r2]!
        vld1.8          {d16}, [r11]!
        vld1.16         {q1},  [r4]!
        vld1.16         {q9},  [r12]!
        vld1.16         {q2},  [r5]!
        vld1.16         {q10}, [lr]!
        subs            r6,  r6,  #8
        vshll.u8        q0,  d0,  #4     // u
        vshll.u8        q8,  d16, #4     // u
        vsub.i16        q1,  q1,  q0     // t1 - u
        vsub.i16        q2,  q2,  q0     // t2 - u
        vsub.i16        q9,  q9,  q8     // t1 - u
        vsub.i16        q10, q10, q8     // t2 - u
        vshll.u16       q3,  d0,  #7     // u << 7
        vshll.u16       q0,  d1,  #7     // u << 7
        vshll.u16       q11, d16, #7     // u << 7
        vshll.u16       q8,  d17, #7     // u << 7
        vmlal.s16       q3,  d2,  d30    // wt[0] * (t1 - u)
        vmlal.s16       q3,  d4,  d31    // wt[1] * (t2 - u)
        vmlal.s16       q0,  d3,  d30    // wt[0] * (t1 - u)
        vmlal.s16       q0,  d5,  d31    // wt[1] * (t2 - u)
        vmlal.s16       q11, d18, d30    // wt[0] * (t1 - u)
        vmlal.s16       q11, d20, d31    // wt[1] * (t2 - u)
        vmlal.s16       q8,  d19, d30    // wt[0] * (t1 - u)
        vmlal.s16       q8,  d21, d31    // wt[1] * (t2 - u)
        vrshrn.i32      d6,  q3,  #11
        vrshrn.i32      d7,  q0,  #11
        vrshrn.i32      d22, q11, #11
        vrshrn.i32      d23, q8,  #11
        vqmovun.s16     d6,  q3
        vqmovun.s16     d22, q11
        vst1.8          {d6},  [r0]!
        vst1.8          {d22}, [r10]!
        bgt             1b

        subs            r7,  r7,  #2
        cmp             r7,  #1
        blt             0f
        mov             r6,  r9
        add             r0,  r0,  r1
        add             r10, r10, r1
        add             r2,  r2,  r3
        add             r11, r11, r3
        add             r4,  r4,  r8
        add             r12, r12, r8
        add             r5,  r5,  r8
        add             lr,  lr,  r8
        beq             2f
        b               1b

2:
        vld1.8          {d0}, [r2]!
        vld1.16         {q1}, [r4]!
        vld1.16         {q2}, [r5]!
        subs            r6,  r6,  #8
        vshll.u8        q0,  d0,  #4     // u
        vsub.i16        q1,  q1,  q0     // t1 - u
        vsub.i16        q2,  q2,  q0     // t2 - u
        vshll.u16       q3,  d0,  #7     // u << 7
        vshll.u16       q0,  d1,  #7     // u << 7
        vmlal.s16       q3,  d2,  d30    // wt[0] * (t1 - u)
        vmlal.s16       q3,  d4,  d31    // wt[1] * (t2 - u)
        vmlal.s16       q0,  d3,  d30    // wt[0] * (t1 - u)
        vmlal.s16       q0,  d5,  d31    // wt[1] * (t2 - u)
        vrshrn.i32      d6,  q3,  #11
        vrshrn.i32      d7,  q0,  #11
        vqmovun.s16     d6,  q3
        vst1.8          {d6}, [r0]!
        bgt             1b
0:
        pop             {r4-r11,pc}
endfunc
