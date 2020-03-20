/*
 * Copyright © 2018, VideoLAN and dav1d authors
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

// void dav1d_wiener_filter_h_8bpc_neon(int16_t *dst, const pixel (*left)[4],
//                                      const pixel *src, ptrdiff_t stride,
//                                      const int16_t fh[7], const intptr_t w,
//                                      int h, enum LrEdgeFlags edges);
function wiener_filter_h_8bpc_neon, export=1
        mov             w8,  w5
        ld1             {v0.8h},  [x4]
        mov             w9,  #(1 << 14) - (1 << 2)
        dup             v30.8h,  w9
        movi            v31.8h,  #8, lsl #8
        // Calculate mid_stride
        add             w10, w5,  #7
        bic             w10, w10, #7
        lsl             w10, w10, #1

        // Clear the last unused element of v0, to allow filtering a single
        // pixel with one plain mul+addv.
        ins             v0.h[7], wzr

        // Set up pointers for reading/writing alternate rows
        add             x12, x0,  x10
        lsl             w10, w10, #1
        add             x13, x2,  x3
        lsl             x3,  x3,  #1

        // Subtract the width from mid_stride
        sub             x10, x10, w5, uxtw #1

        // For w >= 8, we read (w+5)&~7+8 pixels, for w < 8 we read 16 pixels.
        cmp             w5,  #8
        add             w11, w5,  #13
        bic             w11, w11, #7
        b.ge            1f
        mov             w11, #16
1:
        sub             x3,  x3,  w11, uxtw

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             w7,  #1 // LR_HAVE_LEFT
        b.eq            2f
        // LR_HAVE_LEFT
        cbnz            x1,  0f
        // left == NULL
        sub             x2,  x2,  #3
        sub             x13, x13, #3
        b               1f
0:      // LR_HAVE_LEFT, left != NULL
2:      // !LR_HAVE_LEFT, increase the stride.
        // For this case we don't read the left 3 pixels from the src pointer,
        // but shift it as if we had done that.
        add             x3,  x3,  #3


1:      // Loop vertically
        ld1             {v3.16b},  [x2],  #16
        ld1             {v5.16b},  [x13], #16

        tst             w7,  #1 // LR_HAVE_LEFT
        b.eq            0f
        cbz             x1,  2f
        // LR_HAVE_LEFT, left != NULL
        ld1             {v2.s}[3],  [x1], #4
        // Move x2/x13 back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             x2,  x2,  #3
        sub             x13, x13, #3
        ld1             {v4.s}[3],  [x1], #4
        ext             v3.16b, v2.16b, v3.16b, #13
        ext             v5.16b, v4.16b, v5.16b, #13
        b               2f
0:
        // !LR_HAVE_LEFT, fill v2 with the leftmost byte
        // and shift v3 to have 3x the first byte at the front.
        dup             v2.16b, v3.b[0]
        dup             v4.16b, v5.b[0]
        // Move x2 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             x2,  x2,  #3
        sub             x13, x13, #3
        ext             v3.16b, v2.16b, v3.16b, #13
        ext             v5.16b, v4.16b, v5.16b, #13

2:
        uxtl            v2.8h,  v3.8b
        uxtl2           v3.8h,  v3.16b
        uxtl            v4.8h,  v5.8b
        uxtl2           v5.8h,  v5.16b

        tst             w7,  #2 // LR_HAVE_RIGHT
        b.ne            4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             w9,  w5, #14
        ldr             b28, [x2,  w9, sxtw]
        ldr             b29, [x13, w9, sxtw]
        // Fill v28/v29 with the right padding pixel
        dup             v28.8b,  v28.b[0]
        dup             v29.8b,  v29.b[0]
        uxtl            v28.8h,  v28.8b
        uxtl            v29.8h,  v29.8b
3:      // !LR_HAVE_RIGHT
        // If we'll have to pad the right edge we need to quit early here.
        cmp             w5,  #11
        b.ge            4f   // If w >= 11, all used input pixels are valid
        cmp             w5,  #7
        b.ge            5f   // If w >= 7, we can filter 4 pixels
        b               6f

4:      // Loop horizontally
.macro filter wd
        // Interleaving the mul/mla chains actually hurts performance
        // significantly on Cortex A53, thus keeping mul/mla tightly
        // chained like this.
        ext             v16.16b, v2.16b,  v3.16b, #2
        ext             v17.16b, v2.16b,  v3.16b, #4
        ext             v18.16b, v2.16b,  v3.16b, #6
        ext             v19.16b, v2.16b,  v3.16b, #8
        ext             v20.16b, v2.16b,  v3.16b, #10
        ext             v21.16b, v2.16b,  v3.16b, #12
        mul             v6\wd,   v2\wd,   v0.h[0]
        mla             v6\wd,   v16\wd,  v0.h[1]
        mla             v6\wd,   v17\wd,  v0.h[2]
        mla             v6\wd,   v18\wd,  v0.h[3]
        mla             v6\wd,   v19\wd,  v0.h[4]
        mla             v6\wd,   v20\wd,  v0.h[5]
        mla             v6\wd,   v21\wd,  v0.h[6]
        ext             v22.16b, v4.16b,  v5.16b, #2
        ext             v23.16b, v4.16b,  v5.16b, #4
        ext             v24.16b, v4.16b,  v5.16b, #6
        ext             v25.16b, v4.16b,  v5.16b, #8
        ext             v26.16b, v4.16b,  v5.16b, #10
        ext             v27.16b, v4.16b,  v5.16b, #12
        mul             v7\wd,   v4\wd,   v0.h[0]
        mla             v7\wd,   v22\wd,  v0.h[1]
        mla             v7\wd,   v23\wd,  v0.h[2]
        mla             v7\wd,   v24\wd,  v0.h[3]
        mla             v7\wd,   v25\wd,  v0.h[4]
        mla             v7\wd,   v26\wd,  v0.h[5]
        mla             v7\wd,   v27\wd,  v0.h[6]

        shl             v18\wd,  v18\wd,  #7
        shl             v24\wd,  v24\wd,  #7
        sub             v18\wd,  v18\wd,  v30\wd
        sub             v24\wd,  v24\wd,  v30\wd
        sqadd           v6\wd,   v6\wd,   v18\wd
        sqadd           v7\wd,   v7\wd,   v24\wd
        sshr            v6\wd,   v6\wd,   #3
        sshr            v7\wd,   v7\wd,   #3
        add             v6\wd,   v6\wd,   v31\wd
        add             v7\wd,   v7\wd,   v31\wd
.endm
        filter          .8h
        st1             {v6.8h},  [x0],  #16
        st1             {v7.8h},  [x12], #16

        subs            w5,  w5,  #8
        b.le            9f
        tst             w7,  #2 // LR_HAVE_RIGHT
        mov             v2.16b,  v3.16b
        mov             v4.16b,  v5.16b
        ld1             {v3.8b},  [x2],  #8
        ld1             {v5.8b},  [x13], #8
        uxtl            v3.8h,   v3.8b
        uxtl            v5.8h,   v5.8b
        b.ne            4b // If we don't need to pad, just keep filtering.
        b               3b // If we need to pad, check how many pixels we have left.

5:      // Filter 4 pixels, 7 <= w < 11
        filter          .4h
        st1             {v6.4h},  [x0],  #8
        st1             {v7.4h},  [x12], #8

        subs            w5,  w5,  #4 // 3 <= w < 7
        ext             v2.16b,  v2.16b,  v3.16b, #8
        ext             v3.16b,  v3.16b,  v3.16b, #8
        ext             v4.16b,  v4.16b,  v5.16b, #8
        ext             v5.16b,  v5.16b,  v5.16b, #8

6:      // Pad the right edge and filter the last few pixels.
        // w < 7, w+3 pixels valid in v2-v3
        cmp             w5,  #5
        b.lt            7f
        b.gt            8f
        // w == 5, 8 pixels valid in v2, v3 invalid
        mov             v3.16b,  v28.16b
        mov             v5.16b,  v29.16b
        b               88f

7:      // 1 <= w < 5, 4-7 pixels valid in v2
        sub             w9,  w5,  #1
        // w9 = (pixels valid - 4)
        adr             x11, L(variable_shift_tbl)
        ldrh            w9,  [x11, w9, uxtw #1]
        sub             x11, x11, w9, uxth
        mov             v3.16b,  v28.16b
        mov             v5.16b,  v29.16b
        br              x11
44:     // 4 pixels valid in v2/v4, fill the high half with padding.
        ins             v2.d[1], v3.d[0]
        ins             v4.d[1], v5.d[0]
        b               88f
        // Shift v2 right, shifting out invalid pixels,
        // shift v2 left to the original offset, shifting in padding pixels.
55:     // 5 pixels valid
        ext             v2.16b,  v2.16b,  v2.16b,  #10
        ext             v2.16b,  v2.16b,  v3.16b,  #6
        ext             v4.16b,  v4.16b,  v4.16b,  #10
        ext             v4.16b,  v4.16b,  v5.16b,  #6
        b               88f
66:     // 6 pixels valid, fill the upper 2 pixels with padding.
        ins             v2.s[3], v3.s[0]
        ins             v4.s[3], v5.s[0]
        b               88f
77:     // 7 pixels valid, fill the last pixel with padding.
        ins             v2.h[7], v3.h[0]
        ins             v4.h[7], v5.h[0]
        b               88f

L(variable_shift_tbl):
        .hword L(variable_shift_tbl) - 44b
        .hword L(variable_shift_tbl) - 55b
        .hword L(variable_shift_tbl) - 66b
        .hword L(variable_shift_tbl) - 77b

8:      // w > 5, w == 6, 9 pixels valid in v2-v3, 1 pixel valid in v3
        ins             v28.h[0],  v3.h[0]
        ins             v29.h[0],  v5.h[0]
        mov             v3.16b,  v28.16b
        mov             v5.16b,  v29.16b

88:
        // w < 7, v2-v3 padded properly
        cmp             w5,  #4
        b.lt            888f

        // w >= 4, filter 4 pixels
        filter          .4h
        st1             {v6.4h},  [x0],  #8
        st1             {v7.4h},  [x12], #8
        subs            w5,  w5,  #4 // 0 <= w < 4
        ext             v2.16b,  v2.16b,  v3.16b, #8
        ext             v4.16b,  v4.16b,  v5.16b, #8
        b.eq            9f
888:    // 1 <= w < 4, filter 1 pixel at a time
        mul             v6.8h,   v2.8h,   v0.8h
        mul             v7.8h,   v4.8h,   v0.8h
        addv            h6,      v6.8h
        addv            h7,      v7.8h
        dup             v16.4h,  v2.h[3]
        ins             v16.h[1], v4.h[3]
        ins             v6.h[1], v7.h[0]
        shl             v16.4h,  v16.4h,  #7
        sub             v16.4h,  v16.4h,  v30.4h
        sqadd           v6.4h,   v6.4h,   v16.4h
        sshr            v6.4h,   v6.4h,   #3
        add             v6.4h,   v6.4h,   v31.4h
        st1             {v6.h}[0], [x0],  #2
        st1             {v6.h}[1], [x12], #2
        subs            w5,  w5,  #1
        ext             v2.16b,  v2.16b,  v3.16b,  #2
        ext             v4.16b,  v4.16b,  v5.16b,  #2
        b.gt            888b

9:
        subs            w6,  w6,  #2
        b.le            0f
        // Jump to the next row and loop horizontally
        add             x0,  x0,  x10
        add             x12, x12, x10
        add             x2,  x2,  x3
        add             x13, x13, x3
        mov             w5,  w8
        b               1b
0:
        ret
.purgem filter
endfunc

// void dav1d_wiener_filter_v_8bpc_neon(pixel *dst, ptrdiff_t stride,
//                                      const int16_t *mid, int w, int h,
//                                      const int16_t fv[7], enum LrEdgeFlags edges,
//                                      ptrdiff_t mid_stride);
function wiener_filter_v_8bpc_neon, export=1
        mov             w8,  w4
        ld1             {v0.8h},  [x5]
        movi            v1.8h, #128
        add             v1.8h,  v1.8h,  v0.8h

        // Calculate the number of rows to move back when looping vertically
        mov             w11, w4
        tst             w6,  #4 // LR_HAVE_TOP
        b.eq            0f
        sub             x2,  x2,  x7,  lsl #1
        add             w11, w11, #2
0:
        tst             w6,  #8 // LR_HAVE_BOTTOM
        b.eq            1f
        add             w11, w11, #2

1:      // Start of horizontal loop; start one vertical filter slice.
        // Load rows into v16-v19 and pad properly.
        tst             w6,  #4 // LR_HAVE_TOP
        ld1             {v16.8h}, [x2], x7
        b.eq            2f
        // LR_HAVE_TOP
        ld1             {v18.8h}, [x2], x7
        mov             v17.16b, v16.16b
        ld1             {v19.8h}, [x2], x7
        b               3f
2:      // !LR_HAVE_TOP
        mov             v17.16b, v16.16b
        mov             v18.16b, v16.16b
        mov             v19.16b, v16.16b

3:
        cmp             w4,  #4
        b.lt            5f
        // Start filtering normally; fill in v20-v22 with unique rows.
        ld1             {v20.8h}, [x2], x7
        ld1             {v21.8h}, [x2], x7
        ld1             {v22.8h}, [x2], x7

4:
.macro filter compare
        subs            w4,  w4,  #1
        // Interleaving the mul/mla chains actually hurts performance
        // significantly on Cortex A53, thus keeping mul/mla tightly
        // chained like this.
        smull           v2.4s,  v16.4h,  v0.h[0]
        smlal           v2.4s,  v17.4h,  v0.h[1]
        smlal           v2.4s,  v18.4h,  v0.h[2]
        smlal           v2.4s,  v19.4h,  v1.h[3]
        smlal           v2.4s,  v20.4h,  v0.h[4]
        smlal           v2.4s,  v21.4h,  v0.h[5]
        smlal           v2.4s,  v22.4h,  v0.h[6]
        smull2          v3.4s,  v16.8h,  v0.h[0]
        smlal2          v3.4s,  v17.8h,  v0.h[1]
        smlal2          v3.4s,  v18.8h,  v0.h[2]
        smlal2          v3.4s,  v19.8h,  v1.h[3]
        smlal2          v3.4s,  v20.8h,  v0.h[4]
        smlal2          v3.4s,  v21.8h,  v0.h[5]
        smlal2          v3.4s,  v22.8h,  v0.h[6]
        sqrshrun        v2.4h,  v2.4s,   #11
        sqrshrun2       v2.8h,  v3.4s,   #11
        sqxtun          v2.8b,  v2.8h
        st1             {v2.8b}, [x0], x1
.if \compare
        cmp             w4,  #4
.else
        b.le            9f
.endif
        mov             v16.16b,  v17.16b
        mov             v17.16b,  v18.16b
        mov             v18.16b,  v19.16b
        mov             v19.16b,  v20.16b
        mov             v20.16b,  v21.16b
        mov             v21.16b,  v22.16b
.endm
        filter          1
        b.lt            7f
        ld1             {v22.8h}, [x2], x7
        b               4b

5:      // Less than 4 rows in total; not all of v20-v21 are filled yet.
        tst             w6,  #8 // LR_HAVE_BOTTOM
        b.eq            6f
        // LR_HAVE_BOTTOM
        cmp             w4,  #2
        // We load at least 2 rows in all cases.
        ld1             {v20.8h}, [x2], x7
        ld1             {v21.8h}, [x2], x7
        b.gt            53f // 3 rows in total
        b.eq            52f // 2 rows in total
51:     // 1 row in total, v19 already loaded, load edge into v20-v22.
        mov             v22.16b,  v21.16b
        b               8f
52:     // 2 rows in total, v19 already loaded, load v20 with content data
        // and 2 rows of edge.
        ld1             {v22.8h}, [x2], x7
        mov             v23.16b,  v22.16b
        b               8f
53:
        // 3 rows in total, v19 already loaded, load v20 and v21 with content
        // and 2 rows of edge.
        ld1             {v22.8h}, [x2], x7
        ld1             {v23.8h}, [x2], x7
        mov             v24.16b,  v23.16b
        b               8f

6:
        // !LR_HAVE_BOTTOM
        cmp             w4,  #2
        b.gt            63f // 3 rows in total
        b.eq            62f // 2 rows in total
61:     // 1 row in total, v19 already loaded, pad that into v20-v22.
        mov             v20.16b,  v19.16b
        mov             v21.16b,  v19.16b
        mov             v22.16b,  v19.16b
        b               8f
62:     // 2 rows in total, v19 already loaded, load v20 and pad that into v21-v23.
        ld1             {v20.8h}, [x2], x7
        mov             v21.16b,  v20.16b
        mov             v22.16b,  v20.16b
        mov             v23.16b,  v20.16b
        b               8f
63:
        // 3 rows in total, v19 already loaded, load v20 and v21 and pad v21 into v22-v24.
        ld1             {v20.8h}, [x2], x7
        ld1             {v21.8h}, [x2], x7
        mov             v22.16b,  v21.16b
        mov             v23.16b,  v21.16b
        mov             v24.16b,  v21.16b
        b               8f

7:
        // All registers up to v21 are filled already, 3 valid rows left.
        // < 4 valid rows left; fill in padding and filter the last
        // few rows.
        tst             w6,  #8 // LR_HAVE_BOTTOM
        b.eq            71f
        // LR_HAVE_BOTTOM; load 2 rows of edge.
        ld1             {v22.8h}, [x2], x7
        ld1             {v23.8h}, [x2], x7
        mov             v24.16b,  v23.16b
        b               8f
71:
        // !LR_HAVE_BOTTOM, pad 3 rows
        mov             v22.16b,  v21.16b
        mov             v23.16b,  v21.16b
        mov             v24.16b,  v21.16b

8:      // At this point, all registers up to v22-v24 are loaded with
        // edge/padding (depending on how many rows are left).
        filter          0 // This branches to 9f when done
        mov             v22.16b,  v23.16b
        mov             v23.16b,  v24.16b
        b               8b

9:      // End of one vertical slice.
        subs            w3,  w3,  #8
        b.le            0f
        // Move pointers back up to the top and loop horizontally.
        msub            x0,  x1,  x8,  x0
        msub            x2,  x7,  x11, x2
        add             x0,  x0,  #8
        add             x2,  x2,  #16
        mov             w4,  w8
        b               1b

0:
        ret
.purgem filter
endfunc

// void dav1d_copy_narrow_8bpc_neon(pixel *dst, ptrdiff_t stride,
//                                  const pixel *src, int w, int h);
function copy_narrow_8bpc_neon, export=1
        adr             x5,  L(copy_narrow_tbl)
        ldrh            w6,  [x5, w3, uxtw #1]
        sub             x5,  x5,  w6, uxth
        br              x5
10:
        add             x7,  x0,  x1
        lsl             x1,  x1,  #1
18:
        subs            w4,  w4,  #8
        b.lt            110f
        ld1             {v0.8b}, [x2], #8
        st1             {v0.b}[0], [x0], x1
        st1             {v0.b}[1], [x7], x1
        st1             {v0.b}[2], [x0], x1
        st1             {v0.b}[3], [x7], x1
        st1             {v0.b}[4], [x0], x1
        st1             {v0.b}[5], [x7], x1
        st1             {v0.b}[6], [x0], x1
        st1             {v0.b}[7], [x7], x1
        b.le            0f
        b               18b
110:
        add             w4,  w4,  #8
        asr             x1,  x1,  #1
11:
        subs            w4,  w4,  #1
        ld1             {v0.b}[0], [x2], #1
        st1             {v0.b}[0], [x0], x1
        b.gt            11b
0:
        ret

20:
        add             x7,  x0,  x1
        lsl             x1,  x1,  #1
24:
        subs            w4,  w4,  #4
        b.lt            210f
        ld1             {v0.4h}, [x2], #8
        st1             {v0.h}[0], [x0], x1
        st1             {v0.h}[1], [x7], x1
        st1             {v0.h}[2], [x0], x1
        st1             {v0.h}[3], [x7], x1
        b.le            0f
        b               24b
210:
        add             w4,  w4,  #4
        asr             x1,  x1,  #1
22:
        subs            w4,  w4,  #1
        ld1             {v0.h}[0], [x2], #2
        st1             {v0.h}[0], [x0], x1
        b.gt            22b
0:
        ret

30:
        ldrh            w5,  [x2]
        ldrb            w6,  [x2, #2]
        add             x2,  x2,  #3
        subs            w4,  w4,  #1
        strh            w5,  [x0]
        strb            w6,  [x0, #2]
        add             x0,  x0,  x1
        b.gt            30b
        ret

40:
        add             x7,  x0,  x1
        lsl             x1,  x1,  #1
42:
        subs            w4,  w4,  #2
        b.lt            41f
        ld1             {v0.2s}, [x2], #8
        st1             {v0.s}[0], [x0], x1
        st1             {v0.s}[1], [x7], x1
        b.le            0f
        b               42b
41:
        ld1             {v0.s}[0], [x2]
        st1             {v0.s}[0], [x0]
0:
        ret

50:
        ldr             w5,  [x2]
        ldrb            w6,  [x2, #4]
        add             x2,  x2,  #5
        subs            w4,  w4,  #1
        str             w5,  [x0]
        strb            w6,  [x0, #4]
        add             x0,  x0,  x1
        b.gt            50b
        ret

60:
        ldr             w5,  [x2]
        ldrh            w6,  [x2, #4]
        add             x2,  x2,  #6
        subs            w4,  w4,  #1
        str             w5,  [x0]
        strh            w6,  [x0, #4]
        add             x0,  x0,  x1
        b.gt            60b
        ret

70:
        ldr             w5,  [x2]
        ldrh            w6,  [x2, #4]
        ldrb            w7,  [x2, #6]
        add             x2,  x2,  #7
        subs            w4,  w4,  #1
        str             w5,  [x0]
        strh            w6,  [x0, #4]
        strb            w7,  [x0, #6]
        add             x0,  x0,  x1
        b.gt            70b
        ret

L(copy_narrow_tbl):
        .hword 0
        .hword L(copy_narrow_tbl) - 10b
        .hword L(copy_narrow_tbl) - 20b
        .hword L(copy_narrow_tbl) - 30b
        .hword L(copy_narrow_tbl) - 40b
        .hword L(copy_narrow_tbl) - 50b
        .hword L(copy_narrow_tbl) - 60b
        .hword L(copy_narrow_tbl) - 70b
endfunc

#define SUM_STRIDE (384+16)

#include "looprestoration_tmpl.S"

// void dav1d_sgr_box3_h_8bpc_neon(int32_t *sumsq, int16_t *sum,
//                                 const pixel (*left)[4],
//                                 const pixel *src, const ptrdiff_t stride,
//                                 const int w, const int h,
//                                 const enum LrEdgeFlags edges);
function sgr_box3_h_8bpc_neon, export=1
        add             w5,  w5,  #2 // w += 2

        // Set up pointers for reading/writing alternate rows
        add             x10, x0,  #(4*SUM_STRIDE)   // sumsq
        add             x11, x1,  #(2*SUM_STRIDE)   // sum
        add             x12, x3,  x4                // src
        lsl             x4,  x4,  #1
        mov             x9,       #(2*2*SUM_STRIDE) // double sum stride

        // Subtract the aligned width from the output stride.
        // With LR_HAVE_RIGHT, align to 8, without it, align to 4.
        tst             w7,  #2 // LR_HAVE_RIGHT
        b.ne            0f
        // !LR_HAVE_RIGHT
        add             w13, w5,  #3
        bic             w13, w13, #3
        b               1f
0:
        add             w13, w5,  #7
        bic             w13, w13, #7
1:
        sub             x9,  x9,  w13, uxtw #1

        // Store the width for the vertical loop
        mov             w8,  w5

        // Subtract the number of pixels read from the input from the stride
        add             w13, w5,  #14
        bic             w13, w13, #7
        sub             x4,  x4,  w13, uxtw

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             w7,  #1 // LR_HAVE_LEFT
        b.eq            2f
        // LR_HAVE_LEFT
        cbnz            x2,  0f
        // left == NULL
        sub             x3,  x3,  #2
        sub             x12, x12, #2
        b               1f
0:      // LR_HAVE_LEFT, left != NULL
2:      // !LR_HAVE_LEFT, increase the stride.
        // For this case we don't read the left 2 pixels from the src pointer,
        // but shift it as if we had done that.
        add             x4,  x4,  #2


1:      // Loop vertically
        ld1             {v0.16b},  [x3],  #16
        ld1             {v4.16b},  [x12], #16

        tst             w7,  #1 // LR_HAVE_LEFT
        b.eq            0f
        cbz             x2,  2f
        // LR_HAVE_LEFT, left != NULL
        ld1             {v1.s}[3],  [x2], #4
        // Move x3/x12 back to account for the last 2 bytes we loaded earlier,
        // which we'll shift out.
        sub             x3,  x3,  #2
        sub             x12, x12, #2
        ld1             {v5.s}[3],  [x2], #4
        ext             v0.16b, v1.16b, v0.16b, #14
        ext             v4.16b, v5.16b, v4.16b, #14
        b               2f
0:
        // !LR_HAVE_LEFT, fill v1 with the leftmost byte
        // and shift v0 to have 2x the first byte at the front.
        dup             v1.16b, v0.b[0]
        dup             v5.16b, v4.b[0]
        // Move x3 back to account for the last 2 bytes we loaded before,
        // which we shifted out.
        sub             x3,  x3,  #2
        sub             x12, x12, #2
        ext             v0.16b, v1.16b, v0.16b, #14
        ext             v4.16b, v5.16b, v4.16b, #14

2:
        umull           v1.8h,   v0.8b,   v0.8b
        umull2          v2.8h,   v0.16b,  v0.16b
        umull           v5.8h,   v4.8b,   v4.8b
        umull2          v6.8h,   v4.16b,  v4.16b

        tst             w7,  #2 // LR_HAVE_RIGHT
        b.ne            4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             w13, w5, #(2 + 16 - 2 + 1)
        ldr             b30, [x3,  w13, sxtw]
        ldr             b31, [x12, w13, sxtw]
        // Fill v30/v31 with the right padding pixel
        dup             v30.8b,  v30.b[0]
        dup             v31.8b,  v31.b[0]
3:      // !LR_HAVE_RIGHT
        // If we'll have to pad the right edge we need to quit early here.
        cmp             w5,  #10
        b.ge            4f   // If w >= 10, all used input pixels are valid
        cmp             w5,  #6
        b.ge            5f   // If w >= 6, we can filter 4 pixels
        b               6f

4:      // Loop horizontally
.macro uaddl_nh         dst1, dst2, src1, src2, w
        uaddl           \dst1,  \src1\().4h,  \src2\().4h
.if \w > 4
        uaddl2          \dst2,  \src1\().8h,  \src2\().8h
.endif
.endm
.macro uaddw_nh         dst1, dst2, src, w
        uaddw           \dst1,  \dst1,  \src\().4h
.if \w > 4
        uaddw2          \dst2,  \dst2,  \src\().8h
.endif
.endm
.macro add_nh           dst1, dst2, src1, src2, w
        add             \dst1,  \dst1,  \src1
.if \w > 4
        add             \dst2,  \dst2,  \src2
.endif
.endm

.macro add3 w
        ext             v16.16b, v0.16b,  v0.16b, #1
        ext             v17.16b, v0.16b,  v0.16b, #2
        ext             v18.16b, v4.16b,  v4.16b, #1
        ext             v19.16b, v4.16b,  v4.16b, #2
        uaddl           v3.8h,   v0.8b,   v16.8b
        uaddw           v3.8h,   v3.8h,   v17.8b
        uaddl           v7.8h,   v4.8b,   v18.8b
        uaddw           v7.8h,   v7.8h,   v19.8b

        ext             v20.16b, v1.16b,  v2.16b, #2
        ext             v21.16b, v1.16b,  v2.16b, #4
        ext             v22.16b, v5.16b,  v6.16b, #2
        ext             v23.16b, v5.16b,  v6.16b, #4

        uaddl_nh        v26.4s,  v27.4s,  v1,   v20,  \w
        uaddw_nh        v26.4s,  v27.4s,  v21,        \w

        uaddl_nh        v28.4s,  v29.4s,  v5,   v22,  \w
        uaddw_nh        v28.4s,  v29.4s,  v23,        \w
.endm
        add3            8
        st1             {v3.8h},         [x1],  #16
        st1             {v7.8h},         [x11], #16
        st1             {v26.4s,v27.4s}, [x0],  #32
        st1             {v28.4s,v29.4s}, [x10], #32

        subs            w5,  w5,  #8
        b.le            9f
        tst             w7,  #2 // LR_HAVE_RIGHT
        ld1             {v3.8b},  [x3],  #8
        ld1             {v7.8b},  [x12], #8
        mov             v1.16b,  v2.16b
        mov             v5.16b,  v6.16b
        ext             v0.16b,  v0.16b,  v3.16b, #8
        ext             v4.16b,  v4.16b,  v7.16b, #8
        umull           v2.8h,   v3.8b,   v3.8b
        umull           v6.8h,   v7.8b,   v7.8b

        b.ne            4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

5:      // Produce 4 pixels, 6 <= w < 10
        add3            4
        st1             {v3.4h},  [x1],  #8
        st1             {v7.4h},  [x11], #8
        st1             {v26.4s}, [x0],  #16
        st1             {v28.4s}, [x10], #16

        subs            w5,  w5,  #4 // 2 <= w < 6
        ext             v0.16b,  v0.16b,  v0.16b, #4
        ext             v4.16b,  v4.16b,  v4.16b, #4

6:      // Pad the right edge and produce the last few pixels.
        // 2 <= w < 6, 2-5 pixels valid in v0
        sub             w13,  w5,  #2
        // w13 = (pixels valid - 2)
        adr             x14, L(box3_variable_shift_tbl)
        ldrh            w13, [x14, w13, uxtw #1]
        sub             x13, x14, w13, uxth
        br              x13
        // Shift v0 right, shifting out invalid pixels,
        // shift v0 left to the original offset, shifting in padding pixels.
22:     // 2 pixels valid
        ext             v0.16b,  v0.16b,  v0.16b,  #2
        ext             v4.16b,  v4.16b,  v4.16b,  #2
        ext             v0.16b,  v0.16b,  v30.16b, #14
        ext             v4.16b,  v4.16b,  v31.16b, #14
        b               88f
33:     // 3 pixels valid
        ext             v0.16b,  v0.16b,  v0.16b,  #3
        ext             v4.16b,  v4.16b,  v4.16b,  #3
        ext             v0.16b,  v0.16b,  v30.16b, #13
        ext             v4.16b,  v4.16b,  v31.16b, #13
        b               88f
44:     // 4 pixels valid
        ext             v0.16b,  v0.16b,  v0.16b,  #4
        ext             v4.16b,  v4.16b,  v4.16b,  #4
        ext             v0.16b,  v0.16b,  v30.16b, #12
        ext             v4.16b,  v4.16b,  v31.16b, #12
        b               88f
55:     // 5 pixels valid
        ext             v0.16b,  v0.16b,  v0.16b,  #5
        ext             v4.16b,  v4.16b,  v4.16b,  #5
        ext             v0.16b,  v0.16b,  v30.16b, #11
        ext             v4.16b,  v4.16b,  v31.16b, #11
        b               88f

L(box3_variable_shift_tbl):
        .hword L(box3_variable_shift_tbl) - 22b
        .hword L(box3_variable_shift_tbl) - 33b
        .hword L(box3_variable_shift_tbl) - 44b
        .hword L(box3_variable_shift_tbl) - 55b

88:
        umull           v1.8h,   v0.8b,   v0.8b
        umull2          v2.8h,   v0.16b,  v0.16b
        umull           v5.8h,   v4.8b,   v4.8b
        umull2          v6.8h,   v4.16b,  v4.16b

        add3            4
        subs            w5,  w5,  #4
        st1             {v3.4h},  [x1],  #8
        st1             {v7.4h},  [x11], #8
        st1             {v26.4s}, [x0],  #16
        st1             {v28.4s}, [x10], #16
        b.le            9f
        ext             v0.16b,  v0.16b,  v0.16b, #4
        ext             v4.16b,  v4.16b,  v4.16b, #4
        ext             v1.16b,  v1.16b,  v2.16b, #8
        ext             v5.16b,  v5.16b,  v6.16b, #8
        // Only one needed pixel left, but do a normal 4 pixel
        // addition anyway
        add3            4
        st1             {v3.4h},  [x1],  #8
        st1             {v7.4h},  [x11], #8
        st1             {v26.4s}, [x0],  #16
        st1             {v28.4s}, [x10], #16

9:
        subs            w6,  w6,  #2
        b.le            0f
        // Jump to the next row and loop horizontally
        add             x0,  x0,  x9, lsl #1
        add             x10, x10, x9, lsl #1
        add             x1,  x1,  x9
        add             x11, x11, x9
        add             x3,  x3,  x4
        add             x12, x12, x4
        mov             w5,  w8
        b               1b
0:
        ret
.purgem add3
endfunc

// void dav1d_sgr_box5_h_8bpc_neon(int32_t *sumsq, int16_t *sum,
//                                 const pixel (*left)[4],
//                                 const pixel *src, const ptrdiff_t stride,
//                                 const int w, const int h,
//                                 const enum LrEdgeFlags edges);
function sgr_box5_h_8bpc_neon, export=1
        add             w5,  w5,  #2 // w += 2

        // Set up pointers for reading/writing alternate rows
        add             x10, x0,  #(4*SUM_STRIDE)   // sumsq
        add             x11, x1,  #(2*SUM_STRIDE)   // sum
        add             x12, x3,  x4                // src
        lsl             x4,  x4,  #1
        mov             x9,       #(2*2*SUM_STRIDE) // double sum stride

        // Subtract the aligned width from the output stride.
        // With LR_HAVE_RIGHT, align to 8, without it, align to 4.
        // Subtract the number of pixels read from the input from the stride.
        tst             w7,  #2 // LR_HAVE_RIGHT
        b.ne            0f
        // !LR_HAVE_RIGHT
        add             w13, w5,  #3
        bic             w13, w13, #3
        add             w14, w5,  #13
        b               1f
0:
        add             w13, w5,  #7
        bic             w13, w13, #7
        add             w14, w5,  #15
1:
        sub             x9,  x9,  w13, uxtw #1
        bic             w14, w14, #7
        sub             x4,  x4,  w14, uxtw

        // Store the width for the vertical loop
        mov             w8,  w5

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             w7,  #1 // LR_HAVE_LEFT
        b.eq            2f
        // LR_HAVE_LEFT
        cbnz            x2,  0f
        // left == NULL
        sub             x3,  x3,  #3
        sub             x12, x12, #3
        b               1f
0:      // LR_HAVE_LEFT, left != NULL
2:      // !LR_HAVE_LEFT, increase the stride.
        // For this case we don't read the left 3 pixels from the src pointer,
        // but shift it as if we had done that.
        add             x4,  x4,  #3

1:      // Loop vertically
        ld1             {v0.16b},  [x3],  #16
        ld1             {v4.16b},  [x12], #16

        tst             w7,  #1 // LR_HAVE_LEFT
        b.eq            0f
        cbz             x2,  2f
        // LR_HAVE_LEFT, left != NULL
        ld1             {v1.s}[3],  [x2], #4
        // Move x3/x12 back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             x3,  x3,  #3
        sub             x12, x12, #3
        ld1             {v5.s}[3],  [x2], #4
        ext             v0.16b, v1.16b, v0.16b, #13
        ext             v4.16b, v5.16b, v4.16b, #13
        b               2f
0:
        // !LR_HAVE_LEFT, fill v1 with the leftmost byte
        // and shift v0 to have 3x the first byte at the front.
        dup             v1.16b, v0.b[0]
        dup             v5.16b, v4.b[0]
        // Move x3 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             x3,  x3,  #3
        sub             x12, x12, #3
        ext             v0.16b, v1.16b, v0.16b, #13
        ext             v4.16b, v5.16b, v4.16b, #13

2:
        umull           v1.8h,   v0.8b,   v0.8b
        umull2          v2.8h,   v0.16b,  v0.16b
        umull           v5.8h,   v4.8b,   v4.8b
        umull2          v6.8h,   v4.16b,  v4.16b

        tst             w7,  #2 // LR_HAVE_RIGHT
        b.ne            4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             w13, w5, #(2 + 16 - 3 + 1)
        ldr             b30, [x3,  w13, sxtw]
        ldr             b31, [x12, w13, sxtw]
        // Fill v30/v31 with the right padding pixel
        dup             v30.8b,  v30.b[0]
        dup             v31.8b,  v31.b[0]
3:      // !LR_HAVE_RIGHT
        // If we'll have to pad the right edge we need to quit early here.
        cmp             w5,  #11
        b.ge            4f   // If w >= 11, all used input pixels are valid
        cmp             w5,  #7
        b.ge            5f   // If w >= 7, we can produce 4 pixels
        b               6f

4:      // Loop horizontally
.macro add5 w
        ext             v16.16b, v0.16b,  v0.16b, #1
        ext             v17.16b, v0.16b,  v0.16b, #2
        ext             v18.16b, v0.16b,  v0.16b, #3
        ext             v19.16b, v0.16b,  v0.16b, #4
        ext             v20.16b, v4.16b,  v4.16b, #1
        ext             v21.16b, v4.16b,  v4.16b, #2
        ext             v22.16b, v4.16b,  v4.16b, #3
        ext             v23.16b, v4.16b,  v4.16b, #4
        uaddl           v3.8h,   v0.8b,   v16.8b
        uaddl           v24.8h,  v17.8b,  v18.8b
        uaddl           v7.8h,   v4.8b,   v20.8b
        uaddw           v3.8h,   v3.8h,   v19.8b
        uaddl           v25.8h,  v21.8b,  v22.8b
        uaddw           v7.8h,   v7.8h,   v23.8b
        add             v3.8h,   v3.8h,   v24.8h
        add             v7.8h,   v7.8h,   v25.8h

        ext             v16.16b, v1.16b,  v2.16b, #2
        ext             v17.16b, v1.16b,  v2.16b, #4
        ext             v18.16b, v1.16b,  v2.16b, #6
        ext             v19.16b, v1.16b,  v2.16b, #8
        ext             v20.16b, v5.16b,  v6.16b, #2
        ext             v21.16b, v5.16b,  v6.16b, #4
        ext             v22.16b, v5.16b,  v6.16b, #6
        ext             v23.16b, v5.16b,  v6.16b, #8

        uaddl_nh        v26.4s,  v27.4s,  v1,   v16,  \w
        uaddl_nh        v16.4s,  v17.4s,  v17,  v18,  \w
        uaddl_nh        v28.4s,  v29.4s,  v5,   v20,  \w
        uaddw_nh        v26.4s,  v27.4s,  v19,        \w
        uaddl_nh        v20.4s,  v21.4s,  v21,  v22,  \w
        uaddw_nh        v28.4s,  v29.4s,  v23,        \w
        add_nh          v26.4s,  v27.4s,  v16.4s, v17.4s, \w
        add_nh          v28.4s,  v29.4s,  v20.4s, v21.4s, \w
.endm
        add5            8
        st1             {v3.8h},         [x1],  #16
        st1             {v7.8h},         [x11], #16
        st1             {v26.4s,v27.4s}, [x0],  #32
        st1             {v28.4s,v29.4s}, [x10], #32

        subs            w5,  w5,  #8
        b.le            9f
        tst             w7,  #2 // LR_HAVE_RIGHT
        ld1             {v3.8b},  [x3],  #8
        ld1             {v7.8b},  [x12], #8
        mov             v1.16b,  v2.16b
        mov             v5.16b,  v6.16b
        ext             v0.16b,  v0.16b,  v3.16b, #8
        ext             v4.16b,  v4.16b,  v7.16b, #8
        umull           v2.8h,   v3.8b,   v3.8b
        umull           v6.8h,   v7.8b,   v7.8b
        b.ne            4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

5:      // Produce 4 pixels, 7 <= w < 11
        add5            4
        st1             {v3.4h},  [x1],  #8
        st1             {v7.4h},  [x11], #8
        st1             {v26.4s}, [x0],  #16
        st1             {v28.4s}, [x10], #16

        subs            w5,  w5,  #4 // 3 <= w < 7
        ext             v0.16b,  v0.16b,  v0.16b, #4
        ext             v4.16b,  v4.16b,  v4.16b, #4

6:      // Pad the right edge and produce the last few pixels.
        // w < 7, w+1 pixels valid in v0/v4
        sub             w13,  w5,  #1
        // w13 = pixels valid - 2
        adr             x14, L(box5_variable_shift_tbl)
        ldrh            w13, [x14, w13, uxtw #1]
        sub             x13, x14, w13, uxth
        br              x13
        // Shift v0 right, shifting out invalid pixels,
        // shift v0 left to the original offset, shifting in padding pixels.
22:     // 2 pixels valid
        ext             v0.16b,  v0.16b,  v0.16b,  #2
        ext             v4.16b,  v4.16b,  v4.16b,  #2
        ext             v0.16b,  v0.16b,  v30.16b, #14
        ext             v4.16b,  v4.16b,  v31.16b, #14
        b               88f
33:     // 3 pixels valid
        ext             v0.16b,  v0.16b,  v0.16b,  #3
        ext             v4.16b,  v4.16b,  v4.16b,  #3
        ext             v0.16b,  v0.16b,  v30.16b, #13
        ext             v4.16b,  v4.16b,  v31.16b, #13
        b               88f
44:     // 4 pixels valid
        ext             v0.16b,  v0.16b,  v0.16b,  #4
        ext             v4.16b,  v4.16b,  v4.16b,  #4
        ext             v0.16b,  v0.16b,  v30.16b, #12
        ext             v4.16b,  v4.16b,  v31.16b, #12
        b               88f
55:     // 5 pixels valid
        ext             v0.16b,  v0.16b,  v0.16b,  #5
        ext             v4.16b,  v4.16b,  v4.16b,  #5
        ext             v0.16b,  v0.16b,  v30.16b, #11
        ext             v4.16b,  v4.16b,  v31.16b, #11
        b               88f
66:     // 6 pixels valid
        ext             v0.16b,  v0.16b,  v0.16b,  #6
        ext             v4.16b,  v4.16b,  v4.16b,  #6
        ext             v0.16b,  v0.16b,  v30.16b, #10
        ext             v4.16b,  v4.16b,  v31.16b, #10
        b               88f
77:     // 7 pixels valid
        ext             v0.16b,  v0.16b,  v0.16b,  #7
        ext             v4.16b,  v4.16b,  v4.16b,  #7
        ext             v0.16b,  v0.16b,  v30.16b, #9
        ext             v4.16b,  v4.16b,  v31.16b, #9
        b               88f

L(box5_variable_shift_tbl):
        .hword L(box5_variable_shift_tbl) - 22b
        .hword L(box5_variable_shift_tbl) - 33b
        .hword L(box5_variable_shift_tbl) - 44b
        .hword L(box5_variable_shift_tbl) - 55b
        .hword L(box5_variable_shift_tbl) - 66b
        .hword L(box5_variable_shift_tbl) - 77b

88:
        umull           v1.8h,   v0.8b,   v0.8b
        umull2          v2.8h,   v0.16b,  v0.16b
        umull           v5.8h,   v4.8b,   v4.8b
        umull2          v6.8h,   v4.16b,  v4.16b

        add5            4
        subs            w5,  w5,  #4
        st1             {v3.4h},  [x1],  #8
        st1             {v7.4h},  [x11], #8
        st1             {v26.4s}, [x0],  #16
        st1             {v28.4s}, [x10], #16
        b.le            9f
        ext             v0.16b,  v0.16b,  v0.16b, #4
        ext             v1.16b,  v1.16b,  v2.16b, #8
        ext             v4.16b,  v4.16b,  v4.16b, #4
        ext             v5.16b,  v5.16b,  v6.16b, #8
        add5            4
        st1             {v3.4h},  [x1],  #8
        st1             {v7.4h},  [x11], #8
        st1             {v26.4s}, [x0],  #16
        st1             {v28.4s}, [x10], #16

9:
        subs            w6,  w6,  #2
        b.le            0f
        // Jump to the next row and loop horizontally
        add             x0,  x0,  x9, lsl #1
        add             x10, x10, x9, lsl #1
        add             x1,  x1,  x9
        add             x11, x11, x9
        add             x3,  x3,  x4
        add             x12, x12, x4
        mov             w5,  w8
        b               1b
0:
        ret
.purgem add5
endfunc

sgr_funcs 8
