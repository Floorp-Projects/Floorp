;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_sixtap_predict8x8_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
; r0    unsigned char  *src_ptr,
; r1    int  src_pixels_per_line,
; r2    int  xoffset,
; r3    int  yoffset,
; stack(r4) unsigned char *dst_ptr,
; stack(r5) int  dst_pitch

|vp8_sixtap_predict8x8_neon| PROC
    push            {r4-r5, lr}

    ldr             r12, _filter8_coeff_

    ldr             r4, [sp, #12]           ;load parameters from stack
    ldr             r5, [sp, #16]           ;load parameters from stack

    cmp             r2, #0                  ;skip first_pass filter if xoffset=0
    beq             secondpass_filter8x8_only

    add             r2, r12, r2, lsl #5     ;calculate filter location

    cmp             r3, #0                  ;skip second_pass filter if yoffset=0

    vld1.s32        {q14, q15}, [r2]        ;load first_pass filter

    beq             firstpass_filter8x8_only

    sub             sp, sp, #64             ;reserve space on stack for temporary storage
    mov             lr, sp

    vabs.s32        q12, q14
    vabs.s32        q13, q15

    mov             r2, #2                  ;loop counter
    sub             r0, r0, #2              ;move srcptr back to (line-2) and (column-2)
    sub             r0, r0, r1, lsl #1

    vdup.8          d0, d24[0]              ;first_pass filter (d0-d5)
    vdup.8          d1, d24[4]
    vdup.8          d2, d25[0]

;First pass: output_height lines x output_width columns (13x8)
    vld1.u8         {q3}, [r0], r1          ;load src data
    vdup.8          d3, d25[4]
    vld1.u8         {q4}, [r0], r1
    vdup.8          d4, d26[0]
    vld1.u8         {q5}, [r0], r1
    vdup.8          d5, d26[4]
    vld1.u8         {q6}, [r0], r1

filt_blk2d_fp8x8_loop_neon
    pld             [r0]
    pld             [r0, r1]
    pld             [r0, r1, lsl #1]

    vmull.u8        q7, d6, d0              ;(src_ptr[-2] * vp8_filter[0])
    vmull.u8        q8, d8, d0
    vmull.u8        q9, d10, d0
    vmull.u8        q10, d12, d0

    vext.8          d28, d6, d7, #1         ;construct src_ptr[-1]
    vext.8          d29, d8, d9, #1
    vext.8          d30, d10, d11, #1
    vext.8          d31, d12, d13, #1

    vmlsl.u8        q7, d28, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q8, d29, d1
    vmlsl.u8        q9, d30, d1
    vmlsl.u8        q10, d31, d1

    vext.8          d28, d6, d7, #4         ;construct src_ptr[2]
    vext.8          d29, d8, d9, #4
    vext.8          d30, d10, d11, #4
    vext.8          d31, d12, d13, #4

    vmlsl.u8        q7, d28, d4             ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q8, d29, d4
    vmlsl.u8        q9, d30, d4
    vmlsl.u8        q10, d31, d4

    vext.8          d28, d6, d7, #2         ;construct src_ptr[0]
    vext.8          d29, d8, d9, #2
    vext.8          d30, d10, d11, #2
    vext.8          d31, d12, d13, #2

    vmlal.u8        q7, d28, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q8, d29, d2
    vmlal.u8        q9, d30, d2
    vmlal.u8        q10, d31, d2

    vext.8          d28, d6, d7, #5         ;construct src_ptr[3]
    vext.8          d29, d8, d9, #5
    vext.8          d30, d10, d11, #5
    vext.8          d31, d12, d13, #5

    vmlal.u8        q7, d28, d5             ;(src_ptr[3] * vp8_filter[5])
    vmlal.u8        q8, d29, d5
    vmlal.u8        q9, d30, d5
    vmlal.u8        q10, d31, d5

    vext.8          d28, d6, d7, #3         ;construct src_ptr[1]
    vext.8          d29, d8, d9, #3
    vext.8          d30, d10, d11, #3
    vext.8          d31, d12, d13, #3

    vmull.u8        q3, d28, d3             ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q4, d29, d3
    vmull.u8        q5, d30, d3
    vmull.u8        q6, d31, d3

    subs            r2, r2, #1

    vqadd.s16       q7, q3                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q8, q4
    vqadd.s16       q9, q5
    vqadd.s16       q10, q6

    vld1.u8         {q3}, [r0], r1          ;load src data

    vqrshrun.s16    d22, q7, #7             ;shift/round/saturate to u8
    vqrshrun.s16    d23, q8, #7
    vqrshrun.s16    d24, q9, #7
    vqrshrun.s16    d25, q10, #7

    vst1.u8         {d22}, [lr]!            ;store result
    vld1.u8         {q4}, [r0], r1
    vst1.u8         {d23}, [lr]!
    vld1.u8         {q5}, [r0], r1
    vst1.u8         {d24}, [lr]!
    vld1.u8         {q6}, [r0], r1
    vst1.u8         {d25}, [lr]!

    bne             filt_blk2d_fp8x8_loop_neon

    ;first_pass filtering on the rest 5-line data
    ;vld1.u8            {q3}, [r0], r1          ;load src data
    ;vld1.u8            {q4}, [r0], r1
    ;vld1.u8            {q5}, [r0], r1
    ;vld1.u8            {q6}, [r0], r1
    vld1.u8         {q7}, [r0], r1

    vmull.u8        q8, d6, d0              ;(src_ptr[-2] * vp8_filter[0])
    vmull.u8        q9, d8, d0
    vmull.u8        q10, d10, d0
    vmull.u8        q11, d12, d0
    vmull.u8        q12, d14, d0

    vext.8          d27, d6, d7, #1         ;construct src_ptr[-1]
    vext.8          d28, d8, d9, #1
    vext.8          d29, d10, d11, #1
    vext.8          d30, d12, d13, #1
    vext.8          d31, d14, d15, #1

    vmlsl.u8        q8, d27, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q9, d28, d1
    vmlsl.u8        q10, d29, d1
    vmlsl.u8        q11, d30, d1
    vmlsl.u8        q12, d31, d1

    vext.8          d27, d6, d7, #4         ;construct src_ptr[2]
    vext.8          d28, d8, d9, #4
    vext.8          d29, d10, d11, #4
    vext.8          d30, d12, d13, #4
    vext.8          d31, d14, d15, #4

    vmlsl.u8        q8, d27, d4             ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q9, d28, d4
    vmlsl.u8        q10, d29, d4
    vmlsl.u8        q11, d30, d4
    vmlsl.u8        q12, d31, d4

    vext.8          d27, d6, d7, #2         ;construct src_ptr[0]
    vext.8          d28, d8, d9, #2
    vext.8          d29, d10, d11, #2
    vext.8          d30, d12, d13, #2
    vext.8          d31, d14, d15, #2

    vmlal.u8        q8, d27, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q9, d28, d2
    vmlal.u8        q10, d29, d2
    vmlal.u8        q11, d30, d2
    vmlal.u8        q12, d31, d2

    vext.8          d27, d6, d7, #5         ;construct src_ptr[3]
    vext.8          d28, d8, d9, #5
    vext.8          d29, d10, d11, #5
    vext.8          d30, d12, d13, #5
    vext.8          d31, d14, d15, #5

    vmlal.u8        q8, d27, d5             ;(src_ptr[3] * vp8_filter[5])
    vmlal.u8        q9, d28, d5
    vmlal.u8        q10, d29, d5
    vmlal.u8        q11, d30, d5
    vmlal.u8        q12, d31, d5

    vext.8          d27, d6, d7, #3         ;construct src_ptr[1]
    vext.8          d28, d8, d9, #3
    vext.8          d29, d10, d11, #3
    vext.8          d30, d12, d13, #3
    vext.8          d31, d14, d15, #3

    vmull.u8        q3, d27, d3             ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q4, d28, d3
    vmull.u8        q5, d29, d3
    vmull.u8        q6, d30, d3
    vmull.u8        q7, d31, d3

    vqadd.s16       q8, q3                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q9, q4
    vqadd.s16       q10, q5
    vqadd.s16       q11, q6
    vqadd.s16       q12, q7

    add             r3, r12, r3, lsl #5

    vqrshrun.s16    d26, q8, #7             ;shift/round/saturate to u8
    sub             lr, lr, #64
    vqrshrun.s16    d27, q9, #7
    vld1.u8         {q9}, [lr]!             ;load intermediate data from stack
    vqrshrun.s16    d28, q10, #7
    vld1.u8         {q10}, [lr]!

    vld1.s32        {q5, q6}, [r3]          ;load second_pass filter

    vqrshrun.s16    d29, q11, #7
    vld1.u8         {q11}, [lr]!

    vabs.s32        q7, q5
    vabs.s32        q8, q6

    vqrshrun.s16    d30, q12, #7
    vld1.u8         {q12}, [lr]!

;Second pass: 8x8
    mov             r3, #2                  ;loop counter

    vdup.8          d0, d14[0]              ;second_pass filter parameters (d0-d5)
    vdup.8          d1, d14[4]
    vdup.8          d2, d15[0]
    vdup.8          d3, d15[4]
    vdup.8          d4, d16[0]
    vdup.8          d5, d16[4]

filt_blk2d_sp8x8_loop_neon
    vmull.u8        q3, d18, d0             ;(src_ptr[-2] * vp8_filter[0])
    vmull.u8        q4, d19, d0
    vmull.u8        q5, d20, d0
    vmull.u8        q6, d21, d0

    vmlsl.u8        q3, d19, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q4, d20, d1
    vmlsl.u8        q5, d21, d1
    vmlsl.u8        q6, d22, d1

    vmlsl.u8        q3, d22, d4             ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q4, d23, d4
    vmlsl.u8        q5, d24, d4
    vmlsl.u8        q6, d25, d4

    vmlal.u8        q3, d20, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q4, d21, d2
    vmlal.u8        q5, d22, d2
    vmlal.u8        q6, d23, d2

    vmlal.u8        q3, d23, d5             ;(src_ptr[3] * vp8_filter[5])
    vmlal.u8        q4, d24, d5
    vmlal.u8        q5, d25, d5
    vmlal.u8        q6, d26, d5

    vmull.u8        q7, d21, d3             ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q8, d22, d3
    vmull.u8        q9, d23, d3
    vmull.u8        q10, d24, d3

    subs            r3, r3, #1

    vqadd.s16       q7, q3                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q8, q4
    vqadd.s16       q9, q5
    vqadd.s16       q10, q6

    vqrshrun.s16    d6, q7, #7              ;shift/round/saturate to u8
    vqrshrun.s16    d7, q8, #7
    vqrshrun.s16    d8, q9, #7
    vqrshrun.s16    d9, q10, #7

    vmov            q9, q11
    vst1.u8         {d6}, [r4], r5          ;store result
    vmov            q10, q12
    vst1.u8         {d7}, [r4], r5
    vmov            q11, q13
    vst1.u8         {d8}, [r4], r5
    vmov            q12, q14
    vst1.u8         {d9}, [r4], r5
    vmov            d26, d30

    bne filt_blk2d_sp8x8_loop_neon

    add             sp, sp, #64
    pop             {r4-r5,pc}

;---------------------
firstpass_filter8x8_only
    ;add                r2, r12, r2, lsl #5     ;calculate filter location
    ;vld1.s32       {q14, q15}, [r2]        ;load first_pass filter
    vabs.s32        q12, q14
    vabs.s32        q13, q15

    mov             r2, #2                  ;loop counter
    sub             r0, r0, #2              ;move srcptr back to (line-2) and (column-2)

    vdup.8          d0, d24[0]              ;first_pass filter (d0-d5)
    vdup.8          d1, d24[4]
    vdup.8          d2, d25[0]
    vdup.8          d3, d25[4]
    vdup.8          d4, d26[0]
    vdup.8          d5, d26[4]

;First pass: output_height lines x output_width columns (8x8)
filt_blk2d_fpo8x8_loop_neon
    vld1.u8         {q3}, [r0], r1          ;load src data
    vld1.u8         {q4}, [r0], r1
    vld1.u8         {q5}, [r0], r1
    vld1.u8         {q6}, [r0], r1

    pld             [r0]
    pld             [r0, r1]
    pld             [r0, r1, lsl #1]

    vmull.u8        q7, d6, d0              ;(src_ptr[-2] * vp8_filter[0])
    vmull.u8        q8, d8, d0
    vmull.u8        q9, d10, d0
    vmull.u8        q10, d12, d0

    vext.8          d28, d6, d7, #1         ;construct src_ptr[-1]
    vext.8          d29, d8, d9, #1
    vext.8          d30, d10, d11, #1
    vext.8          d31, d12, d13, #1

    vmlsl.u8        q7, d28, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q8, d29, d1
    vmlsl.u8        q9, d30, d1
    vmlsl.u8        q10, d31, d1

    vext.8          d28, d6, d7, #4         ;construct src_ptr[2]
    vext.8          d29, d8, d9, #4
    vext.8          d30, d10, d11, #4
    vext.8          d31, d12, d13, #4

    vmlsl.u8        q7, d28, d4             ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q8, d29, d4
    vmlsl.u8        q9, d30, d4
    vmlsl.u8        q10, d31, d4

    vext.8          d28, d6, d7, #2         ;construct src_ptr[0]
    vext.8          d29, d8, d9, #2
    vext.8          d30, d10, d11, #2
    vext.8          d31, d12, d13, #2

    vmlal.u8        q7, d28, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q8, d29, d2
    vmlal.u8        q9, d30, d2
    vmlal.u8        q10, d31, d2

    vext.8          d28, d6, d7, #5         ;construct src_ptr[3]
    vext.8          d29, d8, d9, #5
    vext.8          d30, d10, d11, #5
    vext.8          d31, d12, d13, #5

    vmlal.u8        q7, d28, d5             ;(src_ptr[3] * vp8_filter[5])
    vmlal.u8        q8, d29, d5
    vmlal.u8        q9, d30, d5
    vmlal.u8        q10, d31, d5

    vext.8          d28, d6, d7, #3         ;construct src_ptr[1]
    vext.8          d29, d8, d9, #3
    vext.8          d30, d10, d11, #3
    vext.8          d31, d12, d13, #3

    vmull.u8        q3, d28, d3             ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q4, d29, d3
    vmull.u8        q5, d30, d3
    vmull.u8        q6, d31, d3
 ;
    vqadd.s16       q7, q3                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q8, q4
    vqadd.s16       q9, q5
    vqadd.s16       q10, q6

    subs            r2, r2, #1

    vqrshrun.s16    d22, q7, #7             ;shift/round/saturate to u8
    vqrshrun.s16    d23, q8, #7
    vqrshrun.s16    d24, q9, #7
    vqrshrun.s16    d25, q10, #7

    vst1.u8         {d22}, [r4], r5         ;store result
    vst1.u8         {d23}, [r4], r5
    vst1.u8         {d24}, [r4], r5
    vst1.u8         {d25}, [r4], r5

    bne             filt_blk2d_fpo8x8_loop_neon

    pop             {r4-r5,pc}

;---------------------
secondpass_filter8x8_only
    sub             r0, r0, r1, lsl #1
    add             r3, r12, r3, lsl #5

    vld1.u8         {d18}, [r0], r1         ;load src data
    vld1.s32        {q5, q6}, [r3]          ;load second_pass filter
    vld1.u8         {d19}, [r0], r1
    vabs.s32        q7, q5
    vld1.u8         {d20}, [r0], r1
    vabs.s32        q8, q6
    vld1.u8         {d21}, [r0], r1
    mov             r3, #2                  ;loop counter
    vld1.u8         {d22}, [r0], r1
    vdup.8          d0, d14[0]              ;second_pass filter parameters (d0-d5)
    vld1.u8         {d23}, [r0], r1
    vdup.8          d1, d14[4]
    vld1.u8         {d24}, [r0], r1
    vdup.8          d2, d15[0]
    vld1.u8         {d25}, [r0], r1
    vdup.8          d3, d15[4]
    vld1.u8         {d26}, [r0], r1
    vdup.8          d4, d16[0]
    vld1.u8         {d27}, [r0], r1
    vdup.8          d5, d16[4]
    vld1.u8         {d28}, [r0], r1
    vld1.u8         {d29}, [r0], r1
    vld1.u8         {d30}, [r0], r1

;Second pass: 8x8
filt_blk2d_spo8x8_loop_neon
    vmull.u8        q3, d18, d0             ;(src_ptr[-2] * vp8_filter[0])
    vmull.u8        q4, d19, d0
    vmull.u8        q5, d20, d0
    vmull.u8        q6, d21, d0

    vmlsl.u8        q3, d19, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q4, d20, d1
    vmlsl.u8        q5, d21, d1
    vmlsl.u8        q6, d22, d1

    vmlsl.u8        q3, d22, d4             ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q4, d23, d4
    vmlsl.u8        q5, d24, d4
    vmlsl.u8        q6, d25, d4

    vmlal.u8        q3, d20, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q4, d21, d2
    vmlal.u8        q5, d22, d2
    vmlal.u8        q6, d23, d2

    vmlal.u8        q3, d23, d5             ;(src_ptr[3] * vp8_filter[5])
    vmlal.u8        q4, d24, d5
    vmlal.u8        q5, d25, d5
    vmlal.u8        q6, d26, d5

    vmull.u8        q7, d21, d3             ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q8, d22, d3
    vmull.u8        q9, d23, d3
    vmull.u8        q10, d24, d3

    subs            r3, r3, #1

    vqadd.s16       q7, q3                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q8, q4
    vqadd.s16       q9, q5
    vqadd.s16       q10, q6

    vqrshrun.s16    d6, q7, #7              ;shift/round/saturate to u8
    vqrshrun.s16    d7, q8, #7
    vqrshrun.s16    d8, q9, #7
    vqrshrun.s16    d9, q10, #7

    vmov            q9, q11
    vst1.u8         {d6}, [r4], r5          ;store result
    vmov            q10, q12
    vst1.u8         {d7}, [r4], r5
    vmov            q11, q13
    vst1.u8         {d8}, [r4], r5
    vmov            q12, q14
    vst1.u8         {d9}, [r4], r5
    vmov            d26, d30

    bne filt_blk2d_spo8x8_loop_neon

    pop             {r4-r5,pc}

    ENDP

;-----------------
    AREA    subpelfilters8_dat, DATA, READWRITE         ;read/write by default
;Data section with name data_area is specified. DCD reserves space in memory for 48 data.
;One word each is reserved. Label filter_coeff can be used to access the data.
;Data address: filter_coeff, filter_coeff+4, filter_coeff+8 ...
_filter8_coeff_
    DCD     filter8_coeff
filter8_coeff
    DCD     0,  0,  128,    0,   0,  0,   0,  0
    DCD     0, -6,  123,   12,  -1,  0,   0,  0
    DCD     2, -11, 108,   36,  -8,  1,   0,  0
    DCD     0, -9,   93,   50,  -6,  0,   0,  0
    DCD     3, -16,  77,   77, -16,  3,   0,  0
    DCD     0, -6,   50,   93,  -9,  0,   0,  0
    DCD     1, -8,   36,  108, -11,  2,   0,  0
    DCD     0, -1,   12,  123,  -6,   0,  0,  0

    END
