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

#define FILTER_OUT_STRIDE 384

.macro sgr_funcs bpc
// void dav1d_sgr_finish_filter1_2rows_Xbpc_neon(int16_t *tmp,
//                                               const pixel *src,
//                                               const ptrdiff_t src_stride,
//                                               const int32_t **a,
//                                               const int16_t **b,
//                                               const int w, const int h);
function sgr_finish_filter1_2rows_\bpc\()bpc_neon, export=1
        stp             d8,  d9,  [sp, #-0x40]!
        stp             d10, d11, [sp, #0x10]
        stp             d12, d13, [sp, #0x20]
        stp             d14, d15, [sp, #0x30]

        ldp             x7,  x8,  [x3]
        ldp             x9,  x3,  [x3, #16]
        ldp             x10, x11, [x4]
        ldp             x12, x4,  [x4, #16]

        mov             x13, #FILTER_OUT_STRIDE
        cmp             w6,  #1
        add             x2,  x1,  x2 // src + stride
        csel            x2,  x1,  x2,  le // if (h <= 1) x2 = x1
        add             x13, x0,  x13, lsl #1

        movi            v30.8h, #3
        movi            v31.4s, #3
1:
        ld1             {v0.8h, v1.8h}, [x10], #32
        ld1             {v2.8h, v3.8h}, [x11], #32
        ld1             {v4.8h, v5.8h}, [x12], #32
        ld1             {v6.8h, v7.8h}, [x4],  #32
        ld1             {v16.4s, v17.4s, v18.4s}, [x7], #48
        ld1             {v19.4s, v20.4s, v21.4s}, [x8], #48
        ld1             {v22.4s, v23.4s, v24.4s}, [x9], #48
        ld1             {v25.4s, v26.4s, v27.4s}, [x3], #48

2:
        ext             v8.16b,  v0.16b,  v1.16b, #2  // [0][1]
        ext             v9.16b,  v2.16b,  v3.16b, #2  // [1][1]
        ext             v10.16b, v4.16b,  v5.16b, #2  // [2][1]
        ext             v11.16b, v0.16b,  v1.16b, #4  // [0][2]
        ext             v12.16b, v2.16b,  v3.16b, #4  // [1][2]
        ext             v13.16b, v4.16b,  v5.16b, #4  // [2][2]

        add             v14.8h,  v2.8h,   v8.8h       // [1][0] + [0][1]
        add             v15.8h,  v9.8h,   v10.8h      // [1][1] + [2][1]

        add             v28.8h,  v0.8h,   v11.8h      // [0][0] + [0][2]
        add             v14.8h,  v14.8h,  v12.8h      // () + [1][2]
        add             v29.8h,  v4.8h,   v13.8h      // [2][0] + [2][2]

        ext             v8.16b,  v6.16b,  v7.16b, #2  // [3][1]
        ext             v11.16b, v6.16b,  v7.16b, #4  // [3][2]

        add             v14.8h,  v14.8h,  v15.8h      // mid
        add             v15.8h,  v28.8h,  v29.8h      // corners

        add             v28.8h,  v4.8h,   v9.8h       // [2][0] + [1][1]
        add             v29.8h,  v10.8h,  v8.8h       // [2][1] + [3][1]

        add             v2.8h,   v2.8h,   v12.8h      // [1][0] + [1][2]
        add             v28.8h,  v28.8h,  v13.8h      // () + [2][2]
        add             v4.8h,   v6.8h,   v11.8h      // [3][0] + [3][2]

        add             v0.8h,   v28.8h,  v29.8h      // mid
        add             v2.8h,   v2.8h,   v4.8h       // corners

        shl             v4.8h,   v14.8h,  #2
        mla             v4.8h,   v15.8h,  v30.8h      // * 3 -> a

        shl             v0.8h,   v0.8h,   #2
        mla             v0.8h,   v2.8h,   v30.8h      // * 3 -> a

        ext             v8.16b,  v16.16b, v17.16b, #4 // [0][1]
        ext             v9.16b,  v17.16b, v18.16b, #4
        ext             v10.16b, v16.16b, v17.16b, #8 // [0][2]
        ext             v11.16b, v17.16b, v18.16b, #8
        ext             v12.16b, v19.16b, v20.16b, #4 // [1][1]
        ext             v13.16b, v20.16b, v21.16b, #4
        add             v8.4s,   v8.4s,   v19.4s      // [0][1] + [1][0]
        add             v9.4s,   v9.4s,   v20.4s
        add             v16.4s,  v16.4s,  v10.4s      // [0][0] + [0][2]
        add             v17.4s,  v17.4s,  v11.4s
        ext             v14.16b, v19.16b, v20.16b, #8 // [1][2]
        ext             v15.16b, v20.16b, v21.16b, #8
        add             v16.4s,  v16.4s,  v22.4s      // () + [2][0]
        add             v17.4s,  v17.4s,  v23.4s
        add             v28.4s,  v12.4s,  v14.4s      // [1][1] + [1][2]
        add             v29.4s,  v13.4s,  v15.4s
        ext             v10.16b, v22.16b, v23.16b, #4 // [2][1]
        ext             v11.16b, v23.16b, v24.16b, #4
        add             v8.4s,   v8.4s,   v28.4s      // mid (incomplete)
        add             v9.4s,   v9.4s,   v29.4s

        add             v19.4s,  v19.4s,  v14.4s      // [1][0] + [1][2]
        add             v20.4s,  v20.4s,  v15.4s
        add             v14.4s,  v22.4s,  v12.4s      // [2][0] + [1][1]
        add             v15.4s,  v23.4s,  v13.4s

        ext             v12.16b, v22.16b, v23.16b, #8 // [2][2]
        ext             v13.16b, v23.16b, v24.16b, #8
        ext             v28.16b, v25.16b, v26.16b, #4 // [3][1]
        ext             v29.16b, v26.16b, v27.16b, #4
        add             v8.4s,   v8.4s,   v10.4s      // () + [2][1] = mid
        add             v9.4s,   v9.4s,   v11.4s
        add             v14.4s,  v14.4s,  v10.4s      // () + [2][1]
        add             v15.4s,  v15.4s,  v11.4s
        ext             v10.16b, v25.16b, v26.16b, #8 // [3][2]
        ext             v11.16b, v26.16b, v27.16b, #8
        add             v16.4s,  v16.4s,  v12.4s      // () + [2][2] = corner
        add             v17.4s,  v17.4s,  v13.4s

        add             v12.4s,  v12.4s,  v28.4s      // [2][2] + [3][1]
        add             v13.4s,  v13.4s,  v29.4s
        add             v25.4s,  v25.4s,  v10.4s      // [3][0] + [3][2]
        add             v26.4s,  v26.4s,  v11.4s

        add             v14.4s,  v14.4s,  v12.4s      // mid
        add             v15.4s,  v15.4s,  v13.4s
        add             v19.4s,  v19.4s,  v25.4s      // corner
        add             v20.4s,  v20.4s,  v26.4s

.if \bpc == 8
        ld1             {v25.8b}, [x1], #8            // src
        ld1             {v26.8b}, [x2], #8
.else
        ld1             {v25.8h}, [x1], #16           // src
        ld1             {v26.8h}, [x2], #16
.endif

        shl             v8.4s,   v8.4s,   #2
        shl             v9.4s,   v9.4s,   #2
        mla             v8.4s,   v16.4s,  v31.4s      // * 3 -> b
        mla             v9.4s,   v17.4s,  v31.4s

.if \bpc == 8
        uxtl            v25.8h,  v25.8b               // src
        uxtl            v26.8h,  v26.8b
.endif

        shl             v14.4s,  v14.4s,  #2
        shl             v15.4s,  v15.4s,  #2
        mla             v14.4s,  v19.4s,  v31.4s      // * 3 -> b
        mla             v15.4s,  v20.4s,  v31.4s

        umlal           v8.4s,   v4.4h,   v25.4h      // b + a * src
        umlal2          v9.4s,   v4.8h,   v25.8h
        umlal           v14.4s,  v0.4h,   v26.4h      // b + a * src
        umlal2          v15.4s,  v0.8h,   v26.8h
        mov             v0.16b,  v1.16b
        rshrn           v8.4h,   v8.4s,   #9
        rshrn2          v8.8h,   v9.4s,   #9
        mov             v2.16b,  v3.16b
        rshrn           v14.4h,  v14.4s,  #9
        rshrn2          v14.8h,  v15.4s,  #9
        subs            w5,  w5,  #8
        mov             v4.16b,  v5.16b
        st1             {v8.8h},  [x0],  #16
        mov             v6.16b,  v7.16b
        st1             {v14.8h}, [x13], #16

        b.le            3f
        mov             v16.16b, v18.16b
        mov             v19.16b, v21.16b
        mov             v22.16b, v24.16b
        mov             v25.16b, v27.16b
        ld1             {v1.8h}, [x10], #16
        ld1             {v3.8h}, [x11], #16
        ld1             {v5.8h}, [x12], #16
        ld1             {v7.8h}, [x4],  #16
        ld1             {v17.4s, v18.4s}, [x7], #32
        ld1             {v20.4s, v21.4s}, [x8], #32
        ld1             {v23.4s, v24.4s}, [x9], #32
        ld1             {v26.4s, v27.4s}, [x3], #32
        b               2b

3:
        ldp             d14, d15, [sp, #0x30]
        ldp             d12, d13, [sp, #0x20]
        ldp             d10, d11, [sp, #0x10]
        ldp             d8,  d9,  [sp], 0x40
        ret
endfunc

// void dav1d_sgr_finish_weighted1_Xbpc_neon(pixel *dst,
//                                           const int32_t **a, const int16_t **b,
//                                           const int w, const int w1,
//                                           const int bitdepth_max);
function sgr_finish_weighted1_\bpc\()bpc_neon, export=1
        ldp             x7,  x8,  [x1]
        ldr             x1,       [x1, #16]
        ldp             x9,  x10, [x2]
        ldr             x2,       [x2, #16]

        dup             v31.8h, w4
        dup             v30.8h, w5

        movi            v6.8h,  #3
        movi            v7.4s,  #3
1:
        ld1             {v0.8h, v1.8h}, [x9],  #32
        ld1             {v2.8h, v3.8h}, [x10], #32
        ld1             {v4.8h, v5.8h}, [x2],  #32
        ld1             {v16.4s, v17.4s, v18.4s}, [x7], #48
        ld1             {v19.4s, v20.4s, v21.4s}, [x8], #48
        ld1             {v22.4s, v23.4s, v24.4s}, [x1], #48

2:
        ext             v25.16b, v0.16b,  v1.16b, #2  // -stride
        ext             v26.16b, v2.16b,  v3.16b, #2  // 0
        ext             v27.16b, v4.16b,  v5.16b, #2  // +stride
        ext             v28.16b, v0.16b,  v1.16b, #4  // +1-stride
        ext             v29.16b, v2.16b,  v3.16b, #4  // +1
        add             v2.8h,   v2.8h,   v25.8h      // -1, -stride
        ext             v25.16b, v4.16b,  v5.16b, #4  // +1+stride
        add             v26.8h,  v26.8h,  v27.8h      // 0, +stride
        add             v0.8h,   v0.8h,   v28.8h      // -1-stride, +1-stride
        add             v2.8h,   v2.8h,   v26.8h
        add             v4.8h,   v4.8h,   v25.8h      // -1+stride, +1+stride
        add             v2.8h,   v2.8h,   v29.8h      // +1
        add             v0.8h,   v0.8h,   v4.8h

        ext             v25.16b, v16.16b, v17.16b, #4 // -stride
        ext             v26.16b, v17.16b, v18.16b, #4
        shl             v2.8h,   v2.8h,   #2
        ext             v27.16b, v16.16b, v17.16b, #8 // +1-stride
        ext             v28.16b, v17.16b, v18.16b, #8
        ext             v29.16b, v19.16b, v20.16b, #4 // 0
        ext             v4.16b,  v20.16b, v21.16b, #4
        mla             v2.8h,   v0.8h,   v6.8h       // * 3 -> a
        add             v25.4s,  v25.4s,  v19.4s      // -stride, -1
        add             v26.4s,  v26.4s,  v20.4s
        add             v16.4s,  v16.4s,  v27.4s      // -1-stride, +1-stride
        add             v17.4s,  v17.4s,  v28.4s
        ext             v27.16b, v19.16b, v20.16b, #8 // +1
        ext             v28.16b, v20.16b, v21.16b, #8
        add             v16.4s,  v16.4s,  v22.4s      // -1+stride
        add             v17.4s,  v17.4s,  v23.4s
        add             v29.4s,  v29.4s,  v27.4s      // 0, +1
        add             v4.4s,   v4.4s,   v28.4s
        add             v25.4s,  v25.4s,  v29.4s
        add             v26.4s,  v26.4s,  v4.4s
        ext             v27.16b, v22.16b, v23.16b, #4 // +stride
        ext             v28.16b, v23.16b, v24.16b, #4
        ext             v29.16b, v22.16b, v23.16b, #8 // +1+stride
        ext             v4.16b,  v23.16b, v24.16b, #8
.if \bpc == 8
        ld1             {v19.8b}, [x0]                // src
.else
        ld1             {v19.8h}, [x0]                // src
.endif
        add             v25.4s,  v25.4s,  v27.4s      // +stride
        add             v26.4s,  v26.4s,  v28.4s
        add             v16.4s,  v16.4s,  v29.4s      // +1+stride
        add             v17.4s,  v17.4s,  v4.4s
        shl             v25.4s,  v25.4s,  #2
        shl             v26.4s,  v26.4s,  #2
        mla             v25.4s,  v16.4s,  v7.4s       // * 3 -> b
        mla             v26.4s,  v17.4s,  v7.4s
.if \bpc == 8
        uxtl            v19.8h,  v19.8b               // src
.endif
        mov             v0.16b,  v1.16b
        umlal           v25.4s,  v2.4h,   v19.4h      // b + a * src
        umlal2          v26.4s,  v2.8h,   v19.8h
        mov             v2.16b,  v3.16b
        rshrn           v25.4h,  v25.4s,  #9
        rshrn2          v25.8h,  v26.4s,  #9

        subs            w3,  w3,  #8

        // weighted1
        shl             v19.8h,  v19.8h,  #4   // u
        mov             v4.16b,  v5.16b

        sub             v25.8h,  v25.8h,  v19.8h // t1 - u
        ld1             {v1.8h}, [x9],  #16
        ushll           v26.4s,  v19.4h,  #7     // u << 7
        ushll2          v27.4s,  v19.8h,  #7     // u << 7
        ld1             {v3.8h}, [x10], #16
        smlal           v26.4s,  v25.4h,  v31.4h // v
        smlal2          v27.4s,  v25.8h,  v31.8h // v
        ld1             {v5.8h}, [x2],  #16
.if \bpc == 8
        rshrn           v26.4h,  v26.4s,  #11
        rshrn2          v26.8h,  v27.4s,  #11
        mov             v16.16b, v18.16b
        sqxtun          v26.8b,  v26.8h
        mov             v19.16b, v21.16b
        mov             v22.16b, v24.16b
        st1             {v26.8b}, [x0], #8
.else
        sqrshrun        v26.4h,  v26.4s,  #11
        sqrshrun2       v26.8h,  v27.4s,  #11
        mov             v16.16b, v18.16b
        umin            v26.8h,  v26.8h,  v30.8h
        mov             v19.16b, v21.16b
        mov             v22.16b, v24.16b
        st1             {v26.8h}, [x0], #16
.endif

        b.le            3f
        ld1             {v17.4s, v18.4s}, [x7], #32
        ld1             {v20.4s, v21.4s}, [x8], #32
        ld1             {v23.4s, v24.4s}, [x1], #32
        b               2b

3:
        ret
endfunc

// void dav1d_sgr_finish_filter2_2rows_Xbpc_neon(int16_t *tmp,
//                                               const pixel *src,
//                                               const ptrdiff_t stride,
//                                               const int32_t **a,
//                                               const int16_t **b,
//                                               const int w, const int h);
function sgr_finish_filter2_2rows_\bpc\()bpc_neon, export=1
        stp             d8,  d9,  [sp, #-0x40]!
        stp             d10, d11, [sp, #0x10]
        stp             d12, d13, [sp, #0x20]
        stp             d14, d15, [sp, #0x30]

        ldp             x3,  x7,  [x3]
        ldp             x4,  x8,  [x4]
        mov             x10, #FILTER_OUT_STRIDE
        cmp             w6,  #1
        add             x2,  x1,  x2 // src + stride
        csel            x2,  x1,  x2,  le // if (h <= 1) x2 = x1
        add             x10, x0,  x10, lsl #1
        movi            v4.8h,  #5
        movi            v5.4s,  #5
        movi            v6.8h,  #6
        movi            v7.4s,  #6
1:
        ld1             {v0.8h, v1.8h}, [x4], #32
        ld1             {v2.8h, v3.8h}, [x8], #32
        ld1             {v16.4s, v17.4s, v18.4s}, [x3], #48
        ld1             {v19.4s, v20.4s, v21.4s}, [x7], #48

2:
        ext             v24.16b, v0.16b,  v1.16b, #4  // +1-stride
        ext             v25.16b, v2.16b,  v3.16b, #4  // +1+stride
        ext             v22.16b, v0.16b,  v1.16b, #2  // -stride
        ext             v23.16b, v2.16b,  v3.16b, #2  // +stride
        add             v0.8h,   v0.8h,   v24.8h      // -1-stride, +1-stride
        add             v25.8h,  v2.8h,   v25.8h      // -1+stride, +1+stride
        add             v2.8h,   v22.8h,  v23.8h      // -stride, +stride
        add             v0.8h,   v0.8h,   v25.8h

        mul             v8.8h,   v25.8h,  v4.8h       // * 5
        mla             v8.8h,   v23.8h,  v6.8h       // * 6

        ext             v22.16b, v16.16b, v17.16b, #4 // -stride
        ext             v23.16b, v17.16b, v18.16b, #4
        ext             v24.16b, v19.16b, v20.16b, #4 // +stride
        ext             v25.16b, v20.16b, v21.16b, #4
        ext             v26.16b, v16.16b, v17.16b, #8 // +1-stride
        ext             v27.16b, v17.16b, v18.16b, #8
        ext             v28.16b, v19.16b, v20.16b, #8 // +1+stride
        ext             v29.16b, v20.16b, v21.16b, #8
        mul             v0.8h,   v0.8h,   v4.8h       // * 5
        mla             v0.8h,   v2.8h,   v6.8h       // * 6
.if \bpc == 8
        ld1             {v31.8b}, [x1], #8
        ld1             {v30.8b}, [x2], #8
.else
        ld1             {v31.8h}, [x1], #16
        ld1             {v30.8h}, [x2], #16
.endif
        add             v16.4s,  v16.4s,  v26.4s      // -1-stride, +1-stride
        add             v17.4s,  v17.4s,  v27.4s
        add             v19.4s,  v19.4s,  v28.4s      // -1+stride, +1+stride
        add             v20.4s,  v20.4s,  v29.4s
        add             v16.4s,  v16.4s,  v19.4s
        add             v17.4s,  v17.4s,  v20.4s

        mul             v9.4s,   v19.4s,  v5.4s       // * 5
        mla             v9.4s,   v24.4s,  v7.4s       // * 6
        mul             v10.4s,  v20.4s,  v5.4s       // * 5
        mla             v10.4s,  v25.4s,  v7.4s       // * 6

        add             v22.4s,  v22.4s,  v24.4s      // -stride, +stride
        add             v23.4s,  v23.4s,  v25.4s
        // This is, surprisingly, faster than other variants where the
        // mul+mla pairs are further apart, on Cortex A53.
        mul             v16.4s,  v16.4s,  v5.4s       // * 5
        mla             v16.4s,  v22.4s,  v7.4s       // * 6
        mul             v17.4s,  v17.4s,  v5.4s       // * 5
        mla             v17.4s,  v23.4s,  v7.4s       // * 6

.if \bpc == 8
        uxtl            v31.8h,  v31.8b
        uxtl            v30.8h,  v30.8b
.endif
        umlal           v16.4s,  v0.4h,   v31.4h      // b + a * src
        umlal2          v17.4s,  v0.8h,   v31.8h
        umlal           v9.4s,   v8.4h,   v30.4h      // b + a * src
        umlal2          v10.4s,  v8.8h,   v30.8h
        mov             v0.16b,  v1.16b
        rshrn           v16.4h,  v16.4s,  #9
        rshrn2          v16.8h,  v17.4s,  #9
        rshrn           v9.4h,   v9.4s,   #8
        rshrn2          v9.8h,   v10.4s,  #8
        subs            w5,  w5,  #8
        mov             v2.16b,  v3.16b
        st1             {v16.8h}, [x0],  #16
        st1             {v9.8h},  [x10], #16

        b.le            9f
        mov             v16.16b, v18.16b
        mov             v19.16b, v21.16b
        ld1             {v1.8h}, [x4], #16
        ld1             {v3.8h}, [x8], #16
        ld1             {v17.4s, v18.4s}, [x3], #32
        ld1             {v20.4s, v21.4s}, [x7], #32
        b               2b

9:
        ldp             d14, d15, [sp, #0x30]
        ldp             d12, d13, [sp, #0x20]
        ldp             d10, d11, [sp, #0x10]
        ldp             d8,  d9,  [sp], 0x40
        ret
endfunc

// void dav1d_sgr_finish_weighted2_Xbpc_neon(pixel *dst, const ptrdiff_t stride,
//                                           const int32_t **a,
//                                           const int16_t **b,
//                                           const int w, const int h,
//                                           const int w1,
//                                           const int bitdepth_max);
function sgr_finish_weighted2_\bpc\()bpc_neon, export=1
        stp             d8,  d9,  [sp, #-0x30]!
        str             d10,      [sp, #0x10]
        stp             d14, d15, [sp, #0x20]

        dup             v14.8h, w6
        dup             v15.8h, w7

        ldp             x2,  x7,  [x2]
        ldp             x3,  x8,  [x3]
        cmp             w5,  #1
        add             x1,  x0,  x1 // src + stride
        // if (h <= 1), set the pointer to the second row to any dummy buffer
        // we can clobber (x2 in this case)
        csel            x1,  x2,  x1,  le
        movi            v4.8h,  #5
        movi            v5.4s,  #5
        movi            v6.8h,  #6
        movi            v7.4s,  #6
1:
        ld1             {v0.8h, v1.8h}, [x3], #32
        ld1             {v2.8h, v3.8h}, [x8], #32
        ld1             {v16.4s, v17.4s, v18.4s}, [x2], #48
        ld1             {v19.4s, v20.4s, v21.4s}, [x7], #48

2:
        ext             v24.16b, v0.16b,  v1.16b, #4  // +1-stride
        ext             v25.16b, v2.16b,  v3.16b, #4  // +1+stride
        ext             v22.16b, v0.16b,  v1.16b, #2  // -stride
        ext             v23.16b, v2.16b,  v3.16b, #2  // +stride
        add             v0.8h,   v0.8h,   v24.8h      // -1-stride, +1-stride
        add             v25.8h,  v2.8h,   v25.8h      // -1+stride, +1+stride
        add             v2.8h,   v22.8h,  v23.8h      // -stride, +stride
        add             v0.8h,   v0.8h,   v25.8h

        mul             v8.8h,   v25.8h,  v4.8h       // * 5
        mla             v8.8h,   v23.8h,  v6.8h       // * 6

        ext             v22.16b, v16.16b, v17.16b, #4 // -stride
        ext             v23.16b, v17.16b, v18.16b, #4
        ext             v24.16b, v19.16b, v20.16b, #4 // +stride
        ext             v25.16b, v20.16b, v21.16b, #4
        ext             v26.16b, v16.16b, v17.16b, #8 // +1-stride
        ext             v27.16b, v17.16b, v18.16b, #8
        ext             v28.16b, v19.16b, v20.16b, #8 // +1+stride
        ext             v29.16b, v20.16b, v21.16b, #8
        mul             v0.8h,   v0.8h,   v4.8h       // * 5
        mla             v0.8h,   v2.8h,   v6.8h       // * 6
.if \bpc == 8
        ld1             {v31.8b}, [x0]
        ld1             {v30.8b}, [x1]
.else
        ld1             {v31.8h}, [x0]
        ld1             {v30.8h}, [x1]
.endif
        add             v16.4s,  v16.4s,  v26.4s      // -1-stride, +1-stride
        add             v17.4s,  v17.4s,  v27.4s
        add             v19.4s,  v19.4s,  v28.4s      // -1+stride, +1+stride
        add             v20.4s,  v20.4s,  v29.4s
        add             v16.4s,  v16.4s,  v19.4s
        add             v17.4s,  v17.4s,  v20.4s

        mul             v9.4s,   v19.4s,  v5.4s       // * 5
        mla             v9.4s,   v24.4s,  v7.4s       // * 6
        mul             v10.4s,  v20.4s,  v5.4s       // * 5
        mla             v10.4s,  v25.4s,  v7.4s       // * 6

        add             v22.4s,  v22.4s,  v24.4s      // -stride, +stride
        add             v23.4s,  v23.4s,  v25.4s
        // This is, surprisingly, faster than other variants where the
        // mul+mla pairs are further apart, on Cortex A53.
        mul             v16.4s,  v16.4s,  v5.4s       // * 5
        mla             v16.4s,  v22.4s,  v7.4s       // * 6
        mul             v17.4s,  v17.4s,  v5.4s       // * 5
        mla             v17.4s,  v23.4s,  v7.4s       // * 6

.if \bpc == 8
        uxtl            v31.8h,  v31.8b
        uxtl            v30.8h,  v30.8b
.endif
        umlal           v16.4s,  v0.4h,   v31.4h      // b + a * src
        umlal2          v17.4s,  v0.8h,   v31.8h
        umlal           v9.4s,   v8.4h,   v30.4h      // b + a * src
        umlal2          v10.4s,  v8.8h,   v30.8h
        mov             v0.16b,  v1.16b
        rshrn           v16.4h,  v16.4s,  #9
        rshrn2          v16.8h,  v17.4s,  #9
        rshrn           v9.4h,   v9.4s,   #8
        rshrn2          v9.8h,   v10.4s,  #8

        subs            w4,  w4,  #8

        // weighted1
        shl             v31.8h,  v31.8h,  #4     // u
        shl             v30.8h,  v30.8h,  #4
        mov             v2.16b,  v3.16b

        sub             v16.8h,  v16.8h,  v31.8h // t1 - u
        sub             v9.8h,   v9.8h,   v30.8h
        ld1             {v1.8h}, [x3], #16
        ushll           v22.4s,  v31.4h,  #7     // u << 7
        ushll2          v23.4s,  v31.8h,  #7
        ushll           v24.4s,  v30.4h,  #7
        ushll2          v25.4s,  v30.8h,  #7
        ld1             {v3.8h}, [x8], #16
        smlal           v22.4s,  v16.4h,  v14.4h // v
        smlal2          v23.4s,  v16.8h,  v14.8h
        mov             v16.16b, v18.16b
        smlal           v24.4s,  v9.4h,   v14.4h
        smlal2          v25.4s,  v9.8h,   v14.8h
        mov             v19.16b, v21.16b
.if \bpc == 8
        rshrn           v22.4h,  v22.4s,  #11
        rshrn2          v22.8h,  v23.4s,  #11
        rshrn           v23.4h,  v24.4s,  #11
        rshrn2          v23.8h,  v25.4s,  #11
        sqxtun          v22.8b,  v22.8h
        sqxtun          v23.8b,  v23.8h
        st1             {v22.8b}, [x0], #8
        st1             {v23.8b}, [x1], #8
.else
        sqrshrun        v22.4h,  v22.4s,  #11
        sqrshrun2       v22.8h,  v23.4s,  #11
        sqrshrun        v23.4h,  v24.4s,  #11
        sqrshrun2       v23.8h,  v25.4s,  #11
        umin            v22.8h,  v22.8h,  v15.8h
        umin            v23.8h,  v23.8h,  v15.8h
        st1             {v22.8h}, [x0], #16
        st1             {v23.8h}, [x1], #16
.endif

        b.le            3f
        ld1             {v17.4s, v18.4s}, [x2], #32
        ld1             {v20.4s, v21.4s}, [x7], #32
        b               2b

3:
        ldp             d14, d15, [sp, #0x20]
        ldr             d10,      [sp, #0x10]
        ldp             d8,  d9,  [sp], 0x30
        ret
endfunc

// void dav1d_sgr_weighted2_Xbpc_neon(pixel *dst, const ptrdiff_t stride,
//                                    const pixel *src, const ptrdiff_t src_stride,
//                                    const int16_t *t1, const int16_t *t2,
//                                    const int w, const int h,
//                                    const int16_t wt[2], const int bitdepth_max);
function sgr_weighted2_\bpc\()bpc_neon, export=1
.if \bpc == 8
        ldr             x8,  [sp]
.else
        ldp             x8,  x9,  [sp]
.endif
        cmp             w7,  #2
        add             x10, x0,  x1
        add             x11, x2,  x3
        add             x12, x4,  #2*FILTER_OUT_STRIDE
        add             x13, x5,  #2*FILTER_OUT_STRIDE
        ld2r            {v30.8h, v31.8h}, [x8] // wt[0], wt[1]
.if \bpc == 16
        dup             v29.8h,  w9
.endif
        mov             x8,  #4*FILTER_OUT_STRIDE
        lsl             x1,  x1,  #1
        lsl             x3,  x3,  #1
        add             x9,  x6,  #7
        bic             x9,  x9,  #7 // Aligned width
.if \bpc == 8
        sub             x1,  x1,  x9
        sub             x3,  x3,  x9
.else
        sub             x1,  x1,  x9, lsl #1
        sub             x3,  x3,  x9, lsl #1
.endif
        sub             x8,  x8,  x9, lsl #1
        mov             w9,  w6
        b.lt            2f
1:
.if \bpc == 8
        ld1             {v0.8b},  [x2],  #8
        ld1             {v16.8b}, [x11], #8
.else
        ld1             {v0.8h},  [x2],  #16
        ld1             {v16.8h}, [x11], #16
.endif
        ld1             {v1.8h},  [x4],  #16
        ld1             {v17.8h}, [x12], #16
        ld1             {v2.8h},  [x5],  #16
        ld1             {v18.8h}, [x13], #16
        subs            w6,  w6,  #8
.if \bpc == 8
        ushll           v0.8h,  v0.8b,  #4     // u
        ushll           v16.8h, v16.8b, #4     // u
.else
        shl             v0.8h,  v0.8h,  #4     // u
        shl             v16.8h, v16.8h, #4     // u
.endif
        sub             v1.8h,  v1.8h,  v0.8h  // t1 - u
        sub             v2.8h,  v2.8h,  v0.8h  // t2 - u
        sub             v17.8h, v17.8h, v16.8h // t1 - u
        sub             v18.8h, v18.8h, v16.8h // t2 - u
        ushll           v3.4s,  v0.4h,  #7     // u << 7
        ushll2          v4.4s,  v0.8h,  #7     // u << 7
        ushll           v19.4s, v16.4h, #7     // u << 7
        ushll2          v20.4s, v16.8h, #7     // u << 7
        smlal           v3.4s,  v1.4h,  v30.4h // wt[0] * (t1 - u)
        smlal           v3.4s,  v2.4h,  v31.4h // wt[1] * (t2 - u)
        smlal2          v4.4s,  v1.8h,  v30.8h // wt[0] * (t1 - u)
        smlal2          v4.4s,  v2.8h,  v31.8h // wt[1] * (t2 - u)
        smlal           v19.4s, v17.4h, v30.4h // wt[0] * (t1 - u)
        smlal           v19.4s, v18.4h, v31.4h // wt[1] * (t2 - u)
        smlal2          v20.4s, v17.8h, v30.8h // wt[0] * (t1 - u)
        smlal2          v20.4s, v18.8h, v31.8h // wt[1] * (t2 - u)
.if \bpc == 8
        rshrn           v3.4h,  v3.4s,  #11
        rshrn2          v3.8h,  v4.4s,  #11
        rshrn           v19.4h, v19.4s, #11
        rshrn2          v19.8h, v20.4s, #11
        sqxtun          v3.8b,  v3.8h
        sqxtun          v19.8b, v19.8h
        st1             {v3.8b},  [x0],  #8
        st1             {v19.8b}, [x10], #8
.else
        sqrshrun        v3.4h,  v3.4s,  #11
        sqrshrun2       v3.8h,  v4.4s,  #11
        sqrshrun        v19.4h, v19.4s, #11
        sqrshrun2       v19.8h, v20.4s, #11
        umin            v3.8h,  v3.8h,  v29.8h
        umin            v19.8h, v19.8h, v29.8h
        st1             {v3.8h},  [x0],  #16
        st1             {v19.8h}, [x10], #16
.endif
        b.gt            1b

        subs            w7,  w7,  #2
        cmp             w7,  #1
        b.lt            0f
        mov             w6,  w9
        add             x0,  x0,  x1
        add             x10, x10, x1
        add             x2,  x2,  x3
        add             x11, x11, x3
        add             x4,  x4,  x8
        add             x12, x12, x8
        add             x5,  x5,  x8
        add             x13, x13, x8
        b.eq            2f
        b               1b

2:
.if \bpc == 8
        ld1             {v0.8b}, [x2], #8
.else
        ld1             {v0.8h}, [x2], #16
.endif
        ld1             {v1.8h}, [x4], #16
        ld1             {v2.8h}, [x5], #16
        subs            w6,  w6,  #8
.if \bpc == 8
        ushll           v0.8h,  v0.8b,  #4     // u
.else
        shl             v0.8h,  v0.8h,  #4     // u
.endif
        sub             v1.8h,  v1.8h,  v0.8h  // t1 - u
        sub             v2.8h,  v2.8h,  v0.8h  // t2 - u
        ushll           v3.4s,  v0.4h,  #7     // u << 7
        ushll2          v4.4s,  v0.8h,  #7     // u << 7
        smlal           v3.4s,  v1.4h,  v30.4h // wt[0] * (t1 - u)
        smlal           v3.4s,  v2.4h,  v31.4h // wt[1] * (t2 - u)
        smlal2          v4.4s,  v1.8h,  v30.8h // wt[0] * (t1 - u)
        smlal2          v4.4s,  v2.8h,  v31.8h // wt[1] * (t2 - u)
.if \bpc == 8
        rshrn           v3.4h,  v3.4s,  #11
        rshrn2          v3.8h,  v4.4s,  #11
        sqxtun          v3.8b,  v3.8h
        st1             {v3.8b}, [x0], #8
.else
        sqrshrun        v3.4h,  v3.4s,  #11
        sqrshrun2       v3.8h,  v4.4s,  #11
        umin            v3.8h,  v3.8h,  v29.8h
        st1             {v3.8h}, [x0], #16
.endif
        b.gt            1b
0:
        ret
endfunc
.endm
