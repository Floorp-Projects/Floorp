/*
 * Copyright © 2018, VideoLAN and dav1d authors
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

// void dav1d_wiener_filter_h_16bpc_neon(int16_t *dst, const pixel (*left)[4],
//                                       const pixel *src, ptrdiff_t stride,
//                                       const int16_t fh[7], const intptr_t w,
//                                       int h, enum LrEdgeFlags edges,
//                                       const int bitdepth_max);
function wiener_filter_h_16bpc_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]
        ldrd            r6,  r7,  [sp, #108]
        ldr             r8,       [sp, #116] // bitdepth_max
        vld1.16         {q0}, [r4, :128]
        clz             r8,  r8
        vmov.i32        q14, #1
        sub             r9,  r8,  #38  // -(bitdepth + 6)
        sub             r8,  r8,  #25  // -round_bits_h
        neg             r9,  r9        // bitdepth + 6
        vdup.32         q1,  r9
        vdup.32         q13, r8        // -round_bits_h
        vmov.i16        q15, #8192
        vshl.u32        q14, q14, q1   // 1 << (bitdepth + 6)
        mov             r8,  r5
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
        sub             r3,  r3,  r11, lsl #1

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             r7,  #1 // LR_HAVE_LEFT
        beq             2f
        // LR_HAVE_LEFT
        cmp             r1,  #0
        bne             0f
        // left == NULL
        sub             r2,  r2,  #6
        sub             lr,  lr,  #6
        b               1f
0:      // LR_HAVE_LEFT, left != NULL
2:      // !LR_HAVE_LEFT, increase the stride.
        // For this case we don't read the left 3 pixels from the src pointer,
        // but shift it as if we had done that.
        add             r3,  r3,  #6


1:      // Loop vertically
        vld1.16         {q2, q3}, [r2]!
        vld1.16         {q4, q5}, [lr]!

        tst             r7,  #1 // LR_HAVE_LEFT
        beq             0f
        cmp             r1,  #0
        beq             2f
        // LR_HAVE_LEFT, left != NULL
        vld1.16         {d3},  [r1]!
        // Move r2/lr back to account for the last 3 pixels we loaded earlier,
        // which we'll shift out.
        sub             r2,  r2,  #6
        sub             lr,  lr,  #6
        vld1.16         {d13}, [r1]!
        vext.8          q3,  q2,  q3,  #10
        vext.8          q2,  q1,  q2,  #10
        vext.8          q5,  q4,  q5,  #10
        vext.8          q4,  q6,  q4,  #10
        b               2f
0:
        // !LR_HAVE_LEFT, fill q1 with the leftmost pixel
        // and shift q2/q3 to have 3x the first pixel at the front.
        vdup.16         q1,  d4[0]
        vdup.16         q6,  d8[0]
        // Move r2 back to account for the last 3 pixels we loaded before,
        // which we shifted out.
        sub             r2,  r2,  #6
        sub             lr,  lr,  #6
        vext.8          q3,  q2,  q3,  #10
        vext.8          q2,  q1,  q2,  #10
        vext.8          q5,  q4,  q5,  #10
        vext.8          q4,  q6,  q4,  #10

2:

        tst             r7,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that pixel to pad with
        // here since we can find it pretty easily from here.
        sub             r9,  r5,  #14
        lsl             r9,  r9,  #1
        ldrh            r11, [r2, r9]
        ldrh            r9,  [lr, r9]
        // Fill q11/q12 with the right padding pixel
        vdup.16         q11, r11
        vdup.16         q12, r9
3:      // !LR_HAVE_RIGHT
        // If we'll have to pad the right edge we need to quit early here.
        cmp             r5,  #11
        bge             4f   // If w >= 11, all used input pixels are valid
        cmp             r5,  #7
        bge             5f   // If w >= 7, we can filter 4 pixels
        b               6f

4:      // Loop horizontally
        vext.8          q8,  q2,  q3,  #2
        vext.8          q9,  q2,  q3,  #4
        vext.8          q10, q2,  q3,  #6
        vmull.s16       q6,  d4,  d0[0]
        vmlal.s16       q6,  d16, d0[1]
        vmlal.s16       q6,  d18, d0[2]
        vmlal.s16       q6,  d20, d0[3]
        vmull.s16       q7,  d5,  d0[0]
        vmlal.s16       q7,  d17, d0[1]
        vmlal.s16       q7,  d19, d0[2]
        vmlal.s16       q7,  d21, d0[3]
        vext.8          q8,  q2,  q3,  #8
        vext.8          q9,  q2,  q3,  #10
        vext.8          q10, q2,  q3,  #12
        vmlal.s16       q6,  d16, d1[0]
        vmlal.s16       q6,  d18, d1[1]
        vmlal.s16       q6,  d20, d1[2]
        vmlal.s16       q7,  d17, d1[0]
        vmlal.s16       q7,  d19, d1[1]
        vmlal.s16       q7,  d21, d1[2]
        vext.8          q2,  q4,  q5,  #2
        vext.8          q10, q4,  q5,  #6
        vmull.s16       q8,  d8,  d0[0]
        vmlal.s16       q8,  d4,  d0[1]
        vmlal.s16       q8,  d20, d0[3]
        vmull.s16       q9,  d9,  d0[0]
        vmlal.s16       q9,  d5,  d0[1]
        vmlal.s16       q9,  d21, d0[3]
        vext.8          q2,  q4,  q5,  #4
        vext.8          q10, q4,  q5,  #8
        vmlal.s16       q8,  d4,  d0[2]
        vmlal.s16       q8,  d20, d1[0]
        vmlal.s16       q9,  d5,  d0[2]
        vmlal.s16       q9,  d21, d1[0]
        vext.8          q2,  q4,  q5,  #10
        vext.8          q10, q4,  q5,  #12
        vmlal.s16       q8,  d4,  d1[1]
        vmlal.s16       q8,  d20, d1[2]
        vmlal.s16       q9,  d5,  d1[1]
        vmlal.s16       q9,  d21, d1[2]

        vmvn.i16        q10, #0x8000 // 0x7fff = (1 << 15) - 1
        vadd.i32        q6,  q6,  q14
        vadd.i32        q7,  q7,  q14
        vadd.i32        q8,  q8,  q14
        vadd.i32        q9,  q9,  q14
        vrshl.s32       q6,  q6,  q13
        vrshl.s32       q7,  q7,  q13
        vrshl.s32       q8,  q8,  q13
        vrshl.s32       q9,  q9,  q13
        vqmovun.s32     d12, q6
        vqmovun.s32     d13, q7
        vqmovun.s32     d14, q8
        vqmovun.s32     d15, q9
        vmin.u16        q6,  q6,  q10
        vmin.u16        q7,  q7,  q10
        vsub.i16        q6,  q6,  q15
        vsub.i16        q7,  q7,  q15
        vst1.16         {q6}, [r0,  :128]!
        vst1.16         {q7}, [r12, :128]!

        subs            r5,  r5,  #8
        ble             9f
        tst             r7,  #2 // LR_HAVE_RIGHT
        vmov            q2,  q3
        vmov            q4,  q5
        vld1.16         {q3}, [r2]!
        vld1.16         {q5}, [lr]!
        bne             4b // If we don't need to pad, just keep filtering.
        b               3b // If we need to pad, check how many pixels we have left.

5:      // Filter 4 pixels, 7 <= w < 11
.macro filter_4
        vext.8          d18, d4,  d5,  #6
        vext.8          d16, d4,  d5,  #2
        vext.8          d17, d4,  d5,  #4
        vext.8          d19, d5,  d6,  #2
        vext.8          d20, d5,  d6,  #4
        vmull.s16       q6,  d4,  d0[0]
        vmlal.s16       q6,  d16, d0[1]
        vmlal.s16       q6,  d17, d0[2]
        vmlal.s16       q6,  d18, d0[3]
        vmlal.s16       q6,  d5,  d1[0]
        vmlal.s16       q6,  d19, d1[1]
        vmlal.s16       q6,  d20, d1[2]

        vext.8          d18, d8,  d9,  #6
        vext.8          d16, d8,  d9,  #2
        vext.8          d17, d8,  d9,  #4
        vext.8          d19, d9,  d10, #2
        vext.8          d20, d9,  d10, #4
        vmull.s16       q7,  d8,  d0[0]
        vmlal.s16       q7,  d16, d0[1]
        vmlal.s16       q7,  d17, d0[2]
        vmlal.s16       q7,  d18, d0[3]
        vmlal.s16       q7,  d9,  d1[0]
        vmlal.s16       q7,  d19, d1[1]
        vmlal.s16       q7,  d20, d1[2]

        vmvn.i16        q10, #0x8000 // 0x7fff = (1 << 15) - 1
        vadd.i32        q6,  q6,  q14
        vadd.i32        q7,  q7,  q14
        vrshl.s32       q6,  q6,  q13
        vrshl.s32       q7,  q7,  q13
        vqmovun.s32     d12, q6
        vqmovun.s32     d13, q7
        vmin.u16        q6,  q6,  q10
        vsub.i16        q6,  q6,  q15
.endm
        filter_4
        vst1.16         {d12}, [r0,  :64]!
        vst1.16         {d13}, [r12, :64]!

        subs            r5,  r5,  #4 // 3 <= w < 7
        vext.8          q2,  q2,  q3,  #8
        vext.8          q3,  q3,  q3,  #8
        vext.8          q4,  q4,  q5,  #8
        vext.8          q5,  q5,  q5,  #8

6:      // Pad the right edge and filter the last few pixels.
        // w < 7, w+3 pixels valid in q2-q3
        cmp             r5,  #5
        blt             7f
        bgt             8f
        // w == 5, 8 pixels valid in q2, q3 invalid
        vmov            q3,  q11
        vmov            q5,  q12
        b               88f

7:      // 1 <= w < 5, 4-7 pixels valid in q2
        sub             r9,  r5,  #1
        // r9 = (pixels valid - 4)
        adr             r11, L(variable_shift_tbl)
        ldr             r9,  [r11, r9, lsl #2]
        add             r11, r11, r9
        vmov            q3,  q11
        vmov            q5,  q12
        bx              r11

        .align 2
L(variable_shift_tbl):
        .word 44f - L(variable_shift_tbl) + CONFIG_THUMB
        .word 55f - L(variable_shift_tbl) + CONFIG_THUMB
        .word 66f - L(variable_shift_tbl) + CONFIG_THUMB
        .word 77f - L(variable_shift_tbl) + CONFIG_THUMB

44:     // 4 pixels valid in q2/q4, fill the high half with padding.
        vmov            d5,  d6
        vmov            d9,  d10
        b               88f
        // Shift q2 right, shifting out invalid pixels,
        // shift q2 left to the original offset, shifting in padding pixels.
55:     // 5 pixels valid
        vext.8          q2,  q2,  q2,  #10
        vext.8          q2,  q2,  q3,  #6
        vext.8          q4,  q4,  q4,  #10
        vext.8          q4,  q4,  q5,  #6
        b               88f
66:     // 6 pixels valid
        vext.8          q2,  q2,  q2,  #12
        vext.8          q2,  q2,  q3,  #4
        vext.8          q4,  q4,  q4,  #12
        vext.8          q4,  q4,  q5,  #4
        b               88f
77:     // 7 pixels valid
        vext.8          q2,  q2,  q2,  #14
        vext.8          q2,  q2,  q3,  #2
        vext.8          q4,  q4,  q4,  #14
        vext.8          q4,  q4,  q5,  #2
        b               88f

8:      // w > 5, w == 6, 9 pixels valid in q2-q3, 1 pixel valid in q3
        vext.8          q3,  q3,  q3,  #2
        vext.8          q3,  q3,  q11, #14
        vext.8          q5,  q5,  q5,  #2
        vext.8          q5,  q5,  q12, #14

88:
        // w < 7, q2-q3 padded properly
        cmp             r5,  #4
        blt             888f

        // w >= 4, filter 4 pixels
        filter_4
        vst1.16         {d12}, [r0,  :64]!
        vst1.16         {d13}, [r12, :64]!
        subs            r5,  r5,  #4 // 0 <= w < 4
        vext.8          q2,  q2,  q3,  #8
        vext.8          q4,  q4,  q5,  #8
        beq             9f
888:    // 1 <= w < 4, filter 1 pixel at a time
        vmull.s16       q6,  d4,  d0
        vmull.s16       q7,  d5,  d1
        vmull.s16       q8,  d8,  d0
        vmull.s16       q9,  d9,  d1
        vadd.i32        q6,  q7
        vadd.i32        q8,  q9
        vpadd.i32       d12, d12, d13
        vpadd.i32       d13, d16, d17
        vpadd.i32       d12, d12, d13
        vadd.i32        d12, d12, d28
        vmvn.i16        d20, #0x8000 // 0x7fff = (1 << 15) - 1
        vrshl.s32       d12, d12, d26
        vqmovun.s32     d12, q6
        vmin.u16        d12, d12, d20
        vsub.i16        d12, d12, d30
        vst1.16         {d12[0]}, [r0,  :16]!
        vst1.16         {d12[1]}, [r12, :16]!
        subs            r5,  r5,  #1
        vext.8          q2,  q2,  q3,  #2
        vext.8          q4,  q4,  q5,  #2
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
        vpop            {q4-q7}
        pop             {r4-r11,pc}
.purgem filter_4
endfunc

// void dav1d_wiener_filter_v_16bpc_neon(pixel *dst, ptrdiff_t stride,
//                                       const int16_t *mid, int w, int h,
//                                       const int16_t fv[7], enum LrEdgeFlags edges,
//                                       ptrdiff_t mid_stride, const int bitdepth_max);
function wiener_filter_v_16bpc_neon, export=1
        push            {r4-r7,lr}
        vpush           {q4-q5}
        ldrd            r4,  r5,  [sp, #52]
        ldrd            r6,  r7,  [sp, #60]
        ldr             lr,       [sp, #68] // bitdepth_max
        vld1.16         {q0},  [r5, :128]
        vdup.16         q5,  lr
        clz             lr,  lr
        sub             lr,  lr,  #11   // round_bits_v
        vdup.32         q4,  lr
        mov             lr,  r4
        vneg.s32        q4,  q4         // -round_bits_v

        // Calculate the number of rows to move back when looping vertically
        mov             r12, r4
        tst             r6,  #4 // LR_HAVE_TOP
        beq             0f
        sub             r2,  r2,  r7, lsl #1
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
        vmull.s16       q2,  d16, d0[0]
        vmlal.s16       q2,  d18, d0[1]
        vmlal.s16       q2,  d20, d0[2]
        vmlal.s16       q2,  d22, d0[3]
        vmlal.s16       q2,  d24, d1[0]
        vmlal.s16       q2,  d26, d1[1]
        vmlal.s16       q2,  d28, d1[2]
        vmull.s16       q3,  d17, d0[0]
        vmlal.s16       q3,  d19, d0[1]
        vmlal.s16       q3,  d21, d0[2]
        vmlal.s16       q3,  d23, d0[3]
        vmlal.s16       q3,  d25, d1[0]
        vmlal.s16       q3,  d27, d1[1]
        vmlal.s16       q3,  d29, d1[2]
        vrshl.s32       q2,  q2,  q4    // round_bits_v
        vrshl.s32       q3,  q3,  q4
        vqmovun.s32     d4,  q2
        vqmovun.s32     d5,  q3
        vmin.u16        q2,  q2,  q5    // bitdepth_max
        vst1.16         {q2}, [r0], r1
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
        vmov            q15, q14
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

8:      // At this point, all registers up to q14-q15,q1 are loaded with
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
        add             r0,  r0,  #16
        add             r2,  r2,  #16
        mov             r4,  lr
        b               1b

0:
        vpop            {q4-q5}
        pop             {r4-r7,pc}
.purgem filter
endfunc

// void dav1d_copy_narrow_16bpc_neon(pixel *dst, ptrdiff_t stride,
//                                   const pixel *src, int w, int h);
function copy_narrow_16bpc_neon, export=1
        push            {r4,lr}
        ldr             r4,  [sp, #8]
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
        vld1.16         {q0}, [r2, :128]!
        vst1.16         {d0[0]}, [r0, :16], r1
        vst1.16         {d0[1]}, [r3, :16], r1
        vst1.16         {d0[2]}, [r0, :16], r1
        vst1.16         {d0[3]}, [r3, :16], r1
        vst1.16         {d1[0]}, [r0, :16], r1
        vst1.16         {d1[1]}, [r3, :16], r1
        vst1.16         {d1[2]}, [r0, :16], r1
        vst1.16         {d1[3]}, [r3, :16], r1
        ble             0f
        b               18b
110:
        add             r4,  r4,  #8
        asr             r1,  r1,  #1
11:
        subs            r4,  r4,  #1
        vld1.16         {d0[]},  [r2]!
        vst1.16         {d0[0]}, [r0], r1
        bgt             11b
0:
        pop             {r4,pc}

20:
        add             r3,  r0,  r1
        lsl             r1,  r1,  #1
24:
        subs            r4,  r4,  #4
        blt             210f
        vld1.32         {q0}, [r2, :128]!
        vst1.32         {d0[0]}, [r0, :32], r1
        vst1.32         {d0[1]}, [r3, :32], r1
        vst1.32         {d1[0]}, [r0, :32], r1
        vst1.32         {d1[1]}, [r3, :32], r1
        ble             0f
        b               24b
210:
        add             r4,  r4,  #4
        asr             r1,  r1,  #1
22:
        subs            r4,  r4,  #1
        vld1.32         {d0[]},  [r2, :32]!
        vst1.32         {d0[0]}, [r0, :32], r1
        bgt             22b
0:
        pop             {r4,pc}

30:
        ldr             r3,  [r2]
        ldrh            r12, [r2, #4]
        add             r2,  r2,  #6
        subs            r4,  r4,  #1
        str             r3,  [r0]
        strh            r12, [r0, #4]
        add             r0,  r0,  r1
        bgt             30b
        pop             {r4,pc}

40:
        add             r3,  r0,  r1
        lsl             r1,  r1,  #1
42:
        subs            r4,  r4,  #2
        blt             41f
        vld1.16         {q0}, [r2, :128]!
        vst1.16         {d0}, [r0, :64], r1
        vst1.16         {d1}, [r3, :64], r1
        ble             0f
        b               42b
41:
        vld1.16         {d0}, [r2, :64]
        vst1.16         {d0}, [r0, :64]
0:
        pop             {r4,pc}

50:
        vld1.16         {d0}, [r2]
        ldrh            r12, [r2, #8]
        add             r2,  r2,  #10
        subs            r4,  r4,  #1
        vst1.16         {d0}, [r0]
        strh            r12, [r0, #8]
        add             r0,  r0,  r1
        bgt             50b
        pop             {r4,pc}

60:
        vld1.16         {d0}, [r2]
        ldr             r12, [r2, #8]
        add             r2,  r2,  #12
        subs            r4,  r4,  #1
        vst1.16         {d0}, [r0]
        str             r12, [r0, #8]
        add             r0,  r0,  r1
        bgt             60b
        pop             {r4,pc}

70:
        vld1.16         {d0}, [r2]
        ldr             r12, [r2, #8]
        ldrh            lr,  [r2, #12]
        add             r2,  r2,  #14
        subs            r4,  r4,  #1
        vst1.16         {d0}, [r0]
        str             r12, [r0, #8]
        strh            lr,  [r0, #12]
        add             r0,  r0,  r1
        bgt             70b
        pop             {r4,pc}
endfunc

#define SUM_STRIDE (384+16)

#include "looprestoration_tmpl.S"

// void dav1d_sgr_box3_h_16bpc_neon(int32_t *sumsq, int16_t *sum,
//                                  const pixel (*left)[4],
//                                  const pixel *src, const ptrdiff_t stride,
//                                  const int w, const int h,
//                                  const enum LrEdgeFlags edges);
function sgr_box3_h_16bpc_neon, export=1
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
        sub             r4,  r4,  lr, lsl #1

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             r7,  #1 // LR_HAVE_LEFT
        beq             2f
        // LR_HAVE_LEFT
        cmp             r2,  #0
        bne             0f
        // left == NULL
        sub             r3,  r3,  #4
        sub             r12, r12, #4
        b               1f
0:      // LR_HAVE_LEFT, left != NULL
2:      // !LR_HAVE_LEFT, increase the stride.
        // For this case we don't read the left 2 pixels from the src pointer,
        // but shift it as if we had done that.
        add             r4,  r4,  #4


1:      // Loop vertically
        vld1.16         {q0, q1}, [r3]!
        vld1.16         {q4, q5}, [r12]!

        tst             r7,  #1 // LR_HAVE_LEFT
        beq             0f
        cmp             r2,  #0
        beq             2f
        // LR_HAVE_LEFT, left != NULL
        vld1.16         {d5}, [r2]!
        // Move r3/r12 back to account for the last 2 pixels we loaded earlier,
        // which we'll shift out.
        sub             r3,  r3,  #4
        sub             r12, r12, #4
        vld1.16         {d13}, [r2]!
        vext.8          q1,  q0,  q1,  #12
        vext.8          q0,  q2,  q0,  #12
        vext.8          q5,  q4,  q5,  #12
        vext.8          q4,  q6,  q4,  #12
        b               2f
0:
        // !LR_HAVE_LEFT, fill q2 with the leftmost pixel
        // and shift q0 to have 2x the first byte at the front.
        vdup.16         q2,  d0[0]
        vdup.16         q6,  d8[0]
        // Move r3 back to account for the last 2 pixels we loaded before,
        // which we shifted out.
        sub             r3,  r3,  #4
        sub             r12, r12, #4
        vext.8          q1,  q0,  q1,  #12
        vext.8          q0,  q2,  q0,  #12
        vext.8          q5,  q4,  q5,  #12
        vext.8          q4,  q6,  q4,  #12

2:
        tst             r7,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that pixel to pad with
        // here since we can find it pretty easily from here.
        sub             lr,  r5,  #(2 + 16 - 2 + 1)
        lsl             lr,  lr,  #1
        ldrh            r11, [r3,  lr]
        ldrh            lr,  [r12, lr]
        // Fill q14/q15 with the right padding pixel
        vdup.16         q14, r11
        vdup.16         q15, lr
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
.macro add3 w
.if \w > 4
        vext.8          q8,  q0,  q1,  #2
        vext.8          q10, q4,  q5,  #2
        vext.8          q9,  q0,  q1,  #4
        vext.8          q11, q4,  q5,  #4
        vadd.i16        q2,  q0,  q8
        vadd.i16        q3,  q4,  q10
        vadd.i16        q2,  q2,  q9
        vadd.i16        q3,  q3,  q11
.else
        vext.8          d16, d0,  d1,  #2
        vext.8          d20, d8,  d9,  #2
        vext.8          d18, d0,  d1,  #4
        vext.8          d22, d8,  d9,  #4
        vadd.i16        d4,  d0,  d16
        vadd.i16        d6,  d8,  d20
        vadd.i16        d4,  d4,  d18
        vadd.i16        d6,  d6,  d22
.endif

        vmull.u16       q6,  d0,  d0
        vmlal.u16       q6,  d16, d16
        vmlal.u16       q6,  d18, d18
        vmull.u16       q12, d8,  d8
        vmlal.u16       q12, d20, d20
        vmlal.u16       q12, d22, d22
.if \w > 4
        vmull.u16       q7,  d1,  d1
        vmlal.u16       q7,  d17, d17
        vmlal.u16       q7,  d19, d19
        vmull.u16       q13, d9,  d9
        vmlal.u16       q13, d21, d21
        vmlal.u16       q13, d23, d23
.endif
.endm
        add3            8
        vst1.16         {q2},       [r1,  :128]!
        vst1.16         {q3},       [r11, :128]!
        vst1.32         {q6,  q7},  [r0,  :128]!
        vst1.32         {q12, q13}, [r10, :128]!

        subs            r5,  r5,  #8
        ble             9f
        tst             r7,  #2 // LR_HAVE_RIGHT
        vmov            q0,  q1
        vmov            q4,  q5
        vld1.16         {q1}, [r3]!
        vld1.16         {q5}, [r12]!

        bne             4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

5:      // Produce 4 pixels, 6 <= w < 10
        add3            4
        vst1.16         {d4},  [r1,  :64]!
        vst1.16         {d6},  [r11, :64]!
        vst1.32         {q6},  [r0,  :128]!
        vst1.32         {q12}, [r10, :128]!

        subs            r5,  r5,  #4 // 2 <= w < 6
        vext.8          q0,  q0,  q1,  #8
        vext.8          q4,  q4,  q5,  #8

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
        vext.8          q0,  q0,  q0,  #4
        vext.8          q4,  q4,  q4,  #4
        vext.8          q0,  q0,  q14, #12
        vext.8          q4,  q4,  q15, #12
        b               88f
33:     // 3 pixels valid
        vext.8          q0,  q0,  q0,  #6
        vext.8          q4,  q4,  q4,  #6
        vext.8          q0,  q0,  q14, #10
        vext.8          q4,  q4,  q15, #10
        b               88f
44:     // 4 pixels valid
        vmov            d1,  d28
        vmov            d9,  d30
        b               88f
55:     // 5 pixels valid
        vext.8          q0,  q0,  q0,  #10
        vext.8          q4,  q4,  q4,  #10
        vext.8          q0,  q0,  q14, #6
        vext.8          q4,  q4,  q15, #6

88:
        // Restore r11 after using it for a temporary value above
        add             r11, r1,  #(2*SUM_STRIDE)

        add3            4
        subs            r5,  r5,  #4
        vst1.16         {d4},  [r1,  :64]!
        vst1.16         {d6},  [r11, :64]!
        vst1.32         {q6},  [r0,  :128]!
        vst1.32         {q12}, [r10, :128]!
        ble             9f
        vext.8          q0,  q0,  q0,  #8
        vext.8          q4,  q4,  q4,  #8
        // Only one needed pixel left, but do a normal 4 pixel
        // addition anyway
        add3            4
        vst1.16         {d4},  [r1,  :64]!
        vst1.16         {d6},  [r11, :64]!
        vst1.32         {q6},  [r0,  :128]!
        vst1.32         {q12}, [r10, :128]!

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

// void dav1d_sgr_box5_h_16bpc_neon(int32_t *sumsq, int16_t *sum,
//                                  const pixel (*left)[4],
//                                  const pixel *src, const ptrdiff_t stride,
//                                  const int w, const int h,
//                                  const enum LrEdgeFlags edges);
function sgr_box5_h_16bpc_neon, export=1
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
        sub             r4,  r4,  r8, lsl #1

        // Store the width for the vertical loop
        mov             r8,  r5

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             r7,  #1 // LR_HAVE_LEFT
        beq             2f
        // LR_HAVE_LEFT
        cmp             r2,  #0
        bne             0f
        // left == NULL
        sub             r3,  r3,  #6
        sub             r12, r12, #6
        b               1f
0:      // LR_HAVE_LEFT, left != NULL
2:      // !LR_HAVE_LEFT, increase the stride.
        // For this case we don't read the left 3 pixels from the src pointer,
        // but shift it as if we had done that.
        add             r4,  r4,  #6

1:      // Loop vertically
        vld1.16         {q0, q1}, [r3]!
        vld1.16         {q4, q5}, [r12]!

        tst             r7,  #1 // LR_HAVE_LEFT
        beq             0f
        cmp             r2,  #0
        beq             2f
        // LR_HAVE_LEFT, left != NULL
        vld1.16         {d5}, [r2]!
        // Move r3/r12 back to account for the last 3 pixels we loaded earlier,
        // which we'll shift out.
        sub             r3,  r3,  #6
        sub             r12, r12, #6
        vld1.16         {d13}, [r2]!
        vext.8          q1,  q0,  q1,  #10
        vext.8          q0,  q2,  q0,  #10
        vext.8          q5,  q4,  q5,  #10
        vext.8          q4,  q6,  q4,  #10
        b               2f
0:
        // !LR_HAVE_LEFT, fill q2 with the leftmost pixel
        // and shift q0 to have 3x the first pixel at the front.
        vdup.16         q2,  d0[0]
        vdup.16         q6,  d8[0]
        // Move r3 back to account for the last 3 pixels we loaded before,
        // which we shifted out.
        sub             r3,  r3,  #6
        sub             r12, r12, #6
        vext.8          q1,  q0,  q1,  #10
        vext.8          q0,  q2,  q0,  #10
        vext.8          q5,  q4,  q5,  #10
        vext.8          q4,  q6,  q4,  #10

2:
        tst             r7,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that pixel to pad with
        // here since we can find it pretty easily from here.
        sub             lr,  r5,  #(2 + 16 - 3 + 1)
        lsl             lr,  lr,  #1
        ldrh            r11, [r3,  lr]
        ldrh            lr,  [r12, lr]
        // Fill q14/q15 with the right padding pixel
        vdup.16         q14, r11
        vdup.16         q15, lr
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
.if \w > 4
        vext.8          q8,  q0,  q1,  #2
        vext.8          q10, q4,  q5,  #2
        vext.8          q9,  q0,  q1,  #4
        vext.8          q11, q4,  q5,  #4
        vadd.i16        q2,  q0,  q8
        vadd.i16        q3,  q4,  q10
        vadd.i16        q2,  q2,  q9
        vadd.i16        q3,  q3,  q11
.else
        vext.8          d16, d0,  d1,  #2
        vext.8          d20, d8,  d9,  #2
        vext.8          d18, d0,  d1,  #4
        vext.8          d22, d8,  d9,  #4
        vadd.i16        d4,  d0,  d16
        vadd.i16        d6,  d8,  d20
        vadd.i16        d4,  d4,  d18
        vadd.i16        d6,  d6,  d22
.endif

        vmull.u16       q6,  d0,  d0
        vmlal.u16       q6,  d16, d16
        vmlal.u16       q6,  d18, d18
        vmull.u16       q12, d8,  d8
        vmlal.u16       q12, d20, d20
        vmlal.u16       q12, d22, d22
.if \w > 4
        vmull.u16       q7,  d1,  d1
        vmlal.u16       q7,  d17, d17
        vmlal.u16       q7,  d19, d19
        vmull.u16       q13, d9,  d9
        vmlal.u16       q13, d21, d21
        vmlal.u16       q13, d23, d23
.endif

.if \w > 4
        vext.8          q8,  q0,  q1,  #6
        vext.8          q10, q4,  q5,  #6
        vext.8          q9,  q0,  q1,  #8
        vext.8          q11, q4,  q5,  #8
        vadd.i16        q2,  q2,  q8
        vadd.i16        q3,  q3,  q10
        vadd.i16        q2,  q2,  q9
        vadd.i16        q3,  q3,  q11
.else
        vext.8          d16, d0,  d1,  #6
        // d18 would be equal to d1; using d1 instead
        vext.8          d20, d8,  d9,  #6
        // d22 would be equal to d9; using d9 instead
        vadd.i16        d4,  d4,  d16
        vadd.i16        d6,  d6,  d20
        vadd.i16        d4,  d4,  d1
        vadd.i16        d6,  d6,  d9
.endif

        vmlal.u16       q6,  d16, d16
        vmlal.u16       q6,  d1,  d1
        vmlal.u16       q12, d20, d20
        vmlal.u16       q12, d9,  d9
.if \w > 4
        vmlal.u16       q7,  d17, d17
        vmlal.u16       q7,  d19, d19
        vmlal.u16       q13, d21, d21
        vmlal.u16       q13, d23, d23
.endif
.endm
        add5            8
        vst1.16         {q2},       [r1,  :128]!
        vst1.16         {q3},       [r11, :128]!
        vst1.32         {q6,  q7},  [r0,  :128]!
        vst1.32         {q12, q13}, [r10, :128]!

        subs            r5,  r5,  #8
        ble             9f
        tst             r7,  #2 // LR_HAVE_RIGHT
        vmov            q0,  q1
        vmov            q4,  q5
        vld1.16         {q1}, [r3]!
        vld1.16         {q5}, [r12]!
        bne             4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

5:      // Produce 4 pixels, 7 <= w < 11
        add5            4
        vst1.16         {d4},  [r1,  :64]!
        vst1.16         {d6},  [r11, :64]!
        vst1.32         {q6},  [r0,  :128]!
        vst1.32         {q12}, [r10, :128]!

        subs            r5,  r5,  #4 // 3 <= w < 7
        vext.8          q0,  q0,  q1,  #8
        vext.8          q4,  q4,  q5,  #8

6:      // Pad the right edge and produce the last few pixels.
        // w < 7, w+1 pixels valid in q0/q4
        sub             lr,  r5,  #1
        // lr = pixels valid - 2
        adr             r11, L(box5_variable_shift_tbl)
        ldr             lr,  [r11, lr, lsl #2]
        vmov            q1,  q14
        vmov            q5,  q15
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
        vext.8          q0,  q0,  q0,  #4
        vext.8          q4,  q4,  q4,  #4
        vext.8          q0,  q0,  q14, #12
        vext.8          q4,  q4,  q15, #12
        b               88f
33:     // 3 pixels valid
        vext.8          q0,  q0,  q0,  #6
        vext.8          q4,  q4,  q4,  #6
        vext.8          q0,  q0,  q14, #10
        vext.8          q4,  q4,  q15, #10
        b               88f
44:     // 4 pixels valid
        vmov            d1,  d28
        vmov            d9,  d30
        b               88f
55:     // 5 pixels valid
        vext.8          q0,  q0,  q0,  #10
        vext.8          q4,  q4,  q4,  #10
        vext.8          q0,  q0,  q14, #6
        vext.8          q4,  q4,  q15, #6
        b               88f
66:     // 6 pixels valid
        vext.8          q0,  q0,  q0,  #12
        vext.8          q4,  q4,  q4,  #12
        vext.8          q0,  q0,  q14, #4
        vext.8          q4,  q4,  q15, #4
        b               88f
77:     // 7 pixels valid
        vext.8          q0,  q0,  q0,  #14
        vext.8          q4,  q4,  q4,  #14
        vext.8          q0,  q0,  q14, #2
        vext.8          q4,  q4,  q15, #2

88:
        // Restore r11 after using it for a temporary value above
        add             r11, r1,  #(2*SUM_STRIDE)

        add5            4
        subs            r5,  r5,  #4
        vst1.16         {d4},  [r1,  :64]!
        vst1.16         {d6},  [r11, :64]!
        vst1.32         {q6},  [r0,  :128]!
        vst1.32         {q12}, [r10, :128]!
        ble             9f
        vext.8          q0,  q0,  q1,  #8
        vext.8          q4,  q4,  q5,  #8
        add5            4
        vst1.16         {d4},  [r1,  :64]!
        vst1.16         {d6},  [r11, :64]!
        vst1.32         {q6},  [r0,  :128]!
        vst1.32         {q12}, [r10, :128]!

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

sgr_funcs 16
