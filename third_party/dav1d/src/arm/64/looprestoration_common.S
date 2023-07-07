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

// void dav1d_sgr_box3_vert_neon(int32_t **sumsq, int16_t **sum,
//                               int32_t *AA, int16_t *BB,
//                               const int w, const int s,
//                               const int bitdepth_max);
function sgr_box3_vert_neon, export=1
        stp             d8,  d9,  [sp, #-0x30]!
        stp             d10, d11, [sp, #0x10]
        stp             d12, d13, [sp, #0x20]

        add             w4,  w4,  #2
        clz             w9,  w6        // bitdepth_max
        dup             v28.4s,   w5   // strength

        ldp             x5,  x6,  [x0]
        ldr             x0,       [x0, #16]
        ldp             x7,  x8,  [x1]
        ldr             x1,       [x1, #16]

        movi            v31.4s,   #9   // n

        sub             w9,  w9,  #24  // -bitdepth_min_8
        movrel          x12, X(sgr_x_by_x)
        mov             w13, #455      // one_by_x
        ld1             {v16.16b, v17.16b, v18.16b}, [x12]
        dup             v6.8h,    w9   // -bitdepth_min_8
        movi            v19.16b,  #5
        movi            v20.8b,   #55  // idx of last 5
        movi            v21.8b,   #72  // idx of last 4
        movi            v22.8b,   #101 // idx of last 3
        movi            v23.8b,   #169 // idx of last 2
        movi            v24.8b,   #254 // idx of last 1
        saddl           v7.4s,    v6.4h,   v6.4h  // -2*bitdepth_min_8
        movi            v29.8h,   #1, lsl #8
        dup             v30.4s,   w13  // one_by_x

        sub             v16.16b, v16.16b, v19.16b
        sub             v17.16b, v17.16b, v19.16b
        sub             v18.16b, v18.16b, v19.16b

        ld1             {v8.4s,  v9.4s},  [x5], #32
        ld1             {v10.4s, v11.4s}, [x6], #32
        ld1             {v12.8h},         [x7], #16
        ld1             {v13.8h},         [x8], #16
        ld1             {v0.4s, v1.4s},   [x0], #32
        ld1             {v2.8h},          [x1], #16
1:

        add             v8.4s,   v8.4s,   v10.4s
        add             v9.4s,   v9.4s,   v11.4s

        add             v12.8h,  v12.8h,  v13.8h

        subs            w4,  w4,  #8
        add             v0.4s,   v0.4s,   v8.4s
        add             v1.4s,   v1.4s,   v9.4s
        add             v2.8h,   v2.8h,   v12.8h

        srshl           v0.4s,   v0.4s,   v7.4s
        srshl           v1.4s,   v1.4s,   v7.4s
        srshl           v4.8h,   v2.8h,   v6.8h
        mul             v0.4s,   v0.4s,   v31.4s // a * n
        mul             v1.4s,   v1.4s,   v31.4s // a * n
        umull           v3.4s,   v4.4h,   v4.4h  // b * b
        umull2          v4.4s,   v4.8h,   v4.8h  // b * b
        uqsub           v0.4s,   v0.4s,   v3.4s  // imax(a * n - b * b, 0)
        uqsub           v1.4s,   v1.4s,   v4.4s  // imax(a * n - b * b, 0)
        mul             v0.4s,   v0.4s,   v28.4s // p * s
        mul             v1.4s,   v1.4s,   v28.4s // p * s
        ld1             {v8.4s,  v9.4s},  [x5], #32
        uqshrn          v0.4h,   v0.4s,   #16
        uqshrn2         v0.8h,   v1.4s,   #16
        ld1             {v10.4s, v11.4s}, [x6], #32
        uqrshrn         v0.8b,   v0.8h,   #4     // imin(z, 255)

        ld1             {v12.8h},         [x7], #16

        cmhi            v25.8b,  v0.8b,   v20.8b // = -1 if sgr_x_by_x[v0] < 5
        cmhi            v26.8b,  v0.8b,   v21.8b // = -1 if sgr_x_by_x[v0] < 4
        tbl             v1.8b, {v16.16b,  v17.16b, v18.16b}, v0.8b
        cmhi            v27.8b,  v0.8b,   v22.8b // = -1 if sgr_x_by_x[v0] < 3
        cmhi            v4.8b,   v0.8b,   v23.8b // = -1 if sgr_x_by_x[v0] < 2
        add             v25.8b,  v25.8b,  v26.8b
        cmhi            v5.8b,   v0.8b,   v24.8b // = -1 if sgr_x_by_x[v0] < 1
        add             v27.8b,  v27.8b,  v4.8b
        add             v5.8b,   v5.8b,   v19.8b
        add             v25.8b,  v25.8b,  v27.8b
        add             v5.8b,   v1.8b,   v5.8b
        ld1             {v13.8h},         [x8], #16
        add             v5.8b,   v5.8b,   v25.8b
        ld1             {v0.4s, v1.4s},   [x0], #32
        uxtl            v5.8h,   v5.8b           // x

        umull           v3.4s,   v5.4h,   v2.4h  // x * BB[i]
        umull2          v4.4s,   v5.8h,   v2.8h  // x * BB[i]
        mul             v3.4s,   v3.4s,   v30.4s // x * BB[i] * sgr_one_by_x
        mul             v4.4s,   v4.4s,   v30.4s // x * BB[i] * sgr_one_by_x
        srshr           v3.4s,   v3.4s,   #12    // AA[i]
        srshr           v4.4s,   v4.4s,   #12    // AA[i]
        sub             v5.8h,   v29.8h,  v5.8h  // 256 - x
        ld1             {v2.8h},          [x1], #16

        st1             {v3.4s, v4.4s}, [x2], #32
        st1             {v5.8h}, [x3], #16
        b.gt            1b

        ldp             d12, d13, [sp, #0x20]
        ldp             d10, d11, [sp, #0x10]
        ldp             d8,  d9,  [sp], 0x30
        ret
endfunc

// void dav1d_sgr_box5_vert_neon(int32_t **sumsq, int16_t **sum,
//                               int32_t *AA, int16_t *BB,
//                               const int w, const int s,
//                               const int bitdepth_max);
function sgr_box5_vert_neon, export=1
        stp             d8,  d9,  [sp, #-0x40]!
        stp             d10, d11, [sp, #0x10]
        stp             d12, d13, [sp, #0x20]
        stp             d14, d15, [sp, #0x30]

        add             w4,  w4,  #2
        clz             w15, w6        // bitdepth_max
        dup             v28.4s,   w5   // strength

        ldp             x5,  x6,  [x0]
        ldp             x7,  x8,  [x0, #16]
        ldr             x0,       [x0, #32]
        ldp             x9,  x10, [x1]
        ldp             x11, x12, [x1, #16]
        ldr             x1,       [x1, #32]

        movi            v31.4s,   #25   // n

        sub             w15, w15, #24  // -bitdepth_min_8
        movrel          x13, X(sgr_x_by_x)
        mov             w14, #164      // one_by_x
        ld1             {v16.16b, v17.16b, v18.16b}, [x13]
        dup             v6.8h,   w15  // -bitdepth_min_8
        movi            v19.16b, #5
        movi            v24.8b,  #254 // idx of last 1
        saddl           v7.4s,   v6.4h,   v6.4h  // -2*bitdepth_min_8
        movi            v29.8h,  #1, lsl #8
        dup             v30.4s,  w14  // one_by_x

        sub             v16.16b, v16.16b, v19.16b
        sub             v17.16b, v17.16b, v19.16b
        sub             v18.16b, v18.16b, v19.16b

        ld1             {v8.4s,  v9.4s},  [x5], #32
        ld1             {v10.4s, v11.4s}, [x6], #32
        ld1             {v12.4s, v13.4s}, [x7], #32
        ld1             {v14.4s, v15.4s}, [x8], #32
        ld1             {v20.8h},         [x9], #16
        ld1             {v21.8h},         [x10], #16
        ld1             {v22.8h},         [x11], #16
        ld1             {v23.8h},         [x12], #16
        ld1             {v0.4s,  v1.4s},  [x0], #32
        ld1             {v2.8h},          [x1], #16

1:
        add             v8.4s,   v8.4s,   v10.4s
        add             v9.4s,   v9.4s,   v11.4s
        add             v12.4s,  v12.4s,  v14.4s
        add             v13.4s,  v13.4s,  v15.4s

        add             v20.8h,  v20.8h,  v21.8h
        add             v22.8h,  v22.8h,  v23.8h

        add             v0.4s,   v0.4s,   v8.4s
        add             v1.4s,   v1.4s,   v9.4s
        add             v2.8h,   v2.8h,   v20.8h

        add             v0.4s,   v0.4s,   v12.4s
        add             v1.4s,   v1.4s,   v13.4s
        add             v2.8h,   v2.8h,   v22.8h

        subs            w4,  w4,  #8

        movi            v20.8b,  #55  // idx of last 5
        movi            v21.8b,  #72  // idx of last 4
        movi            v22.8b,  #101 // idx of last 3
        movi            v23.8b,  #169 // idx of last 2

        srshl           v0.4s,   v0.4s,   v7.4s
        srshl           v1.4s,   v1.4s,   v7.4s
        srshl           v4.8h,   v2.8h,   v6.8h
        mul             v0.4s,   v0.4s,   v31.4s // a * n
        mul             v1.4s,   v1.4s,   v31.4s // a * n
        umull           v3.4s,   v4.4h,   v4.4h  // b * b
        umull2          v4.4s,   v4.8h,   v4.8h  // b * b
        uqsub           v0.4s,   v0.4s,   v3.4s  // imax(a * n - b * b, 0)
        uqsub           v1.4s,   v1.4s,   v4.4s  // imax(a * n - b * b, 0)
        mul             v0.4s,   v0.4s,   v28.4s // p * s
        mul             v1.4s,   v1.4s,   v28.4s // p * s
        ld1             {v8.4s,  v9.4s},  [x5], #32
        uqshrn          v0.4h,   v0.4s,   #16
        uqshrn2         v0.8h,   v1.4s,   #16
        ld1             {v10.4s, v11.4s}, [x6], #32
        uqrshrn         v0.8b,   v0.8h,   #4     // imin(z, 255)

        ld1             {v12.4s, v13.4s}, [x7], #32

        cmhi            v25.8b,  v0.8b,   v20.8b // = -1 if sgr_x_by_x[v0] < 5
        cmhi            v26.8b,  v0.8b,   v21.8b // = -1 if sgr_x_by_x[v0] < 4
        tbl             v1.8b, {v16.16b,  v17.16b, v18.16b}, v0.8b
        cmhi            v27.8b,  v0.8b,   v22.8b // = -1 if sgr_x_by_x[v0] < 3
        cmhi            v4.8b,   v0.8b,   v23.8b // = -1 if sgr_x_by_x[v0] < 2
        ld1             {v14.4s, v15.4s}, [x8], #32
        add             v25.8b,  v25.8b,  v26.8b
        cmhi            v5.8b,   v0.8b,   v24.8b // = -1 if sgr_x_by_x[v0] < 1
        add             v27.8b,  v27.8b,  v4.8b
        ld1             {v20.8h},         [x9], #16
        add             v5.8b,   v5.8b,   v19.8b
        add             v25.8b,  v25.8b,  v27.8b
        ld1             {v21.8h},         [x10], #16
        add             v5.8b,   v1.8b,   v5.8b
        ld1             {v22.8h},         [x11], #16
        add             v5.8b,   v5.8b,   v25.8b
        ld1             {v23.8h},         [x12], #16
        uxtl            v5.8h,   v5.8b           // x

        ld1             {v0.4s,  v1.4s},  [x0], #32
        umull           v3.4s,   v5.4h,   v2.4h  // x * BB[i]
        umull2          v4.4s,   v5.8h,   v2.8h  // x * BB[i]
        mul             v3.4s,   v3.4s,   v30.4s // x * BB[i] * sgr_one_by_x
        mul             v4.4s,   v4.4s,   v30.4s // x * BB[i] * sgr_one_by_x
        srshr           v3.4s,   v3.4s,   #12    // AA[i]
        srshr           v4.4s,   v4.4s,   #12    // AA[i]
        sub             v5.8h,   v29.8h,  v5.8h  // 256 - x
        ld1             {v2.8h},          [x1], #16

        st1             {v3.4s, v4.4s}, [x2], #32
        st1             {v5.8h}, [x3], #16
        b.gt            1b

        ldp             d14, d15, [sp, #0x30]
        ldp             d12, d13, [sp, #0x20]
        ldp             d10, d11, [sp, #0x10]
        ldp             d8,  d9,  [sp], 0x40
        ret
endfunc
