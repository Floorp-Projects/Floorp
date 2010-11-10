;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_sixtap_predict16x16_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
; r0    unsigned char  *src_ptr,
; r1    int  src_pixels_per_line,
; r2    int  xoffset,
; r3    int  yoffset,
; r4    unsigned char *dst_ptr,
; stack(r5) int  dst_pitch

;Note: To take advantage of 8-bit mulplication instruction in NEON. First apply abs() to
; filter coeffs to make them u8. Then, use vmlsl for negtive coeffs. After multiplication,
; the result can be negtive. So, I treat the result as s16. But, since it is also possible
; that the result can be a large positive number (> 2^15-1), which could be confused as a
; negtive number. To avoid that error, apply filter coeffs in the order of 0, 1, 4 ,5 ,2,
; which ensures that the result stays in s16 range. Finally, saturated add the result by
; applying 3rd filter coeff. Same applys to other filter functions.

|vp8_sixtap_predict16x16_neon| PROC
    push            {r4-r5, lr}

    ldr             r12, _filter16_coeff_
    ldr             r4, [sp, #12]           ;load parameters from stack
    ldr             r5, [sp, #16]           ;load parameters from stack

    cmp             r2, #0                  ;skip first_pass filter if xoffset=0
    beq             secondpass_filter16x16_only

    add             r2, r12, r2, lsl #5     ;calculate filter location

    cmp             r3, #0                  ;skip second_pass filter if yoffset=0

    vld1.s32        {q14, q15}, [r2]        ;load first_pass filter

    beq             firstpass_filter16x16_only

    sub             sp, sp, #336            ;reserve space on stack for temporary storage
    mov             lr, sp

    vabs.s32        q12, q14
    vabs.s32        q13, q15

    mov             r2, #7                  ;loop counter
    sub             r0, r0, #2              ;move srcptr back to (line-2) and (column-2)
    sub             r0, r0, r1, lsl #1

    vdup.8          d0, d24[0]              ;first_pass filter (d0-d5)
    vdup.8          d1, d24[4]
    vdup.8          d2, d25[0]
    vdup.8          d3, d25[4]
    vdup.8          d4, d26[0]
    vdup.8          d5, d26[4]

;First Pass: output_height lines x output_width columns (21x16)
filt_blk2d_fp16x16_loop_neon
    vld1.u8         {d6, d7, d8}, [r0], r1      ;load src data
    vld1.u8         {d9, d10, d11}, [r0], r1
    vld1.u8         {d12, d13, d14}, [r0], r1

    pld             [r0]
    pld             [r0, r1]
    pld             [r0, r1, lsl #1]

    vmull.u8        q8, d6, d0              ;(src_ptr[-2] * vp8_filter[0])
    vmull.u8        q9, d7, d0
    vmull.u8        q10, d9, d0
    vmull.u8        q11, d10, d0
    vmull.u8        q12, d12, d0
    vmull.u8        q13, d13, d0

    vext.8          d28, d6, d7, #1         ;construct src_ptr[-1]
    vext.8          d29, d9, d10, #1
    vext.8          d30, d12, d13, #1

    vmlsl.u8        q8, d28, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q10, d29, d1
    vmlsl.u8        q12, d30, d1

    vext.8          d28, d7, d8, #1
    vext.8          d29, d10, d11, #1
    vext.8          d30, d13, d14, #1

    vmlsl.u8        q9, d28, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q11, d29, d1
    vmlsl.u8        q13, d30, d1

    vext.8          d28, d6, d7, #4         ;construct src_ptr[2]
    vext.8          d29, d9, d10, #4
    vext.8          d30, d12, d13, #4

    vmlsl.u8        q8, d28, d4             ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q10, d29, d4
    vmlsl.u8        q12, d30, d4

    vext.8          d28, d7, d8, #4
    vext.8          d29, d10, d11, #4
    vext.8          d30, d13, d14, #4

    vmlsl.u8        q9, d28, d4             ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q11, d29, d4
    vmlsl.u8        q13, d30, d4

    vext.8          d28, d6, d7, #5         ;construct src_ptr[3]
    vext.8          d29, d9, d10, #5
    vext.8          d30, d12, d13, #5

    vmlal.u8        q8, d28, d5             ;(src_ptr[3] * vp8_filter[5])
    vmlal.u8        q10, d29, d5
    vmlal.u8        q12, d30, d5

    vext.8          d28, d7, d8, #5
    vext.8          d29, d10, d11, #5
    vext.8          d30, d13, d14, #5

    vmlal.u8        q9, d28, d5             ;(src_ptr[3] * vp8_filter[5])
    vmlal.u8        q11, d29, d5
    vmlal.u8        q13, d30, d5

    vext.8          d28, d6, d7, #2         ;construct src_ptr[0]
    vext.8          d29, d9, d10, #2
    vext.8          d30, d12, d13, #2

    vmlal.u8        q8, d28, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q10, d29, d2
    vmlal.u8        q12, d30, d2

    vext.8          d28, d7, d8, #2
    vext.8          d29, d10, d11, #2
    vext.8          d30, d13, d14, #2

    vmlal.u8        q9, d28, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q11, d29, d2
    vmlal.u8        q13, d30, d2

    vext.8          d28, d6, d7, #3         ;construct src_ptr[1]
    vext.8          d29, d9, d10, #3
    vext.8          d30, d12, d13, #3

    vext.8          d15, d7, d8, #3
    vext.8          d31, d10, d11, #3
    vext.8          d6, d13, d14, #3

    vmull.u8        q4, d28, d3             ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q5, d29, d3
    vmull.u8        q6, d30, d3

    vqadd.s16       q8, q4                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q10, q5
    vqadd.s16       q12, q6

    vmull.u8        q6, d15, d3             ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q7, d31, d3
    vmull.u8        q3, d6, d3

    subs            r2, r2, #1

    vqadd.s16       q9, q6
    vqadd.s16       q11, q7
    vqadd.s16       q13, q3

    vqrshrun.s16    d6, q8, #7              ;shift/round/saturate to u8
    vqrshrun.s16    d7, q9, #7
    vqrshrun.s16    d8, q10, #7
    vqrshrun.s16    d9, q11, #7
    vqrshrun.s16    d10, q12, #7
    vqrshrun.s16    d11, q13, #7

    vst1.u8         {d6, d7, d8}, [lr]!     ;store result
    vst1.u8         {d9, d10, d11}, [lr]!

    bne             filt_blk2d_fp16x16_loop_neon

;Second pass: 16x16
;secondpass_filter - do first 8-columns and then second 8-columns
    add             r3, r12, r3, lsl #5
    sub             lr, lr, #336

    vld1.s32        {q5, q6}, [r3]          ;load second_pass filter
    mov             r3, #2                  ;loop counter

    vabs.s32        q7, q5
    vabs.s32        q8, q6

    mov             r2, #16

    vdup.8          d0, d14[0]              ;second_pass filter parameters (d0-d5)
    vdup.8          d1, d14[4]
    vdup.8          d2, d15[0]
    vdup.8          d3, d15[4]
    vdup.8          d4, d16[0]
    vdup.8          d5, d16[4]

filt_blk2d_sp16x16_outloop_neon
    vld1.u8         {d18}, [lr], r2         ;load src data
    vld1.u8         {d19}, [lr], r2
    vld1.u8         {d20}, [lr], r2
    vld1.u8         {d21}, [lr], r2
    mov             r12, #4                 ;loop counter
    vld1.u8         {d22}, [lr], r2

secondpass_inner_loop_neon
    vld1.u8         {d23}, [lr], r2         ;load src data
    vld1.u8         {d24}, [lr], r2
    vld1.u8         {d25}, [lr], r2
    vld1.u8         {d26}, [lr], r2

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

    subs            r12, r12, #1

    vqadd.s16       q7, q3                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q8, q4
    vqadd.s16       q9, q5
    vqadd.s16       q10, q6

    vqrshrun.s16    d6, q7, #7              ;shift/round/saturate to u8
    vqrshrun.s16    d7, q8, #7
    vqrshrun.s16    d8, q9, #7
    vqrshrun.s16    d9, q10, #7

    vst1.u8         {d6}, [r4], r5          ;store result
    vmov            q9, q11
    vst1.u8         {d7}, [r4], r5
    vmov            q10, q12
    vst1.u8         {d8}, [r4], r5
    vmov            d22, d26
    vst1.u8         {d9}, [r4], r5

    bne             secondpass_inner_loop_neon

    subs            r3, r3, #1
    sub             lr, lr, #336
    add             lr, lr, #8

    sub             r4, r4, r5, lsl #4
    add             r4, r4, #8

    bne filt_blk2d_sp16x16_outloop_neon

    add             sp, sp, #336
    pop             {r4-r5,pc}

;--------------------
firstpass_filter16x16_only
    vabs.s32        q12, q14
    vabs.s32        q13, q15

    mov             r2, #8                  ;loop counter
    sub             r0, r0, #2              ;move srcptr back to (column-2)

    vdup.8          d0, d24[0]              ;first_pass filter (d0-d5)
    vdup.8          d1, d24[4]
    vdup.8          d2, d25[0]
    vdup.8          d3, d25[4]
    vdup.8          d4, d26[0]
    vdup.8          d5, d26[4]

;First Pass: output_height lines x output_width columns (16x16)
filt_blk2d_fpo16x16_loop_neon
    vld1.u8         {d6, d7, d8}, [r0], r1      ;load src data
    vld1.u8         {d9, d10, d11}, [r0], r1

    pld             [r0]
    pld             [r0, r1]

    vmull.u8        q6, d6, d0              ;(src_ptr[-2] * vp8_filter[0])
    vmull.u8        q7, d7, d0
    vmull.u8        q8, d9, d0
    vmull.u8        q9, d10, d0

    vext.8          d20, d6, d7, #1         ;construct src_ptr[-1]
    vext.8          d21, d9, d10, #1
    vext.8          d22, d7, d8, #1
    vext.8          d23, d10, d11, #1
    vext.8          d24, d6, d7, #4         ;construct src_ptr[2]
    vext.8          d25, d9, d10, #4
    vext.8          d26, d7, d8, #4
    vext.8          d27, d10, d11, #4
    vext.8          d28, d6, d7, #5         ;construct src_ptr[3]
    vext.8          d29, d9, d10, #5

    vmlsl.u8        q6, d20, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q8, d21, d1
    vmlsl.u8        q7, d22, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q9, d23, d1
    vmlsl.u8        q6, d24, d4             ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q8, d25, d4
    vmlsl.u8        q7, d26, d4             ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q9, d27, d4
    vmlal.u8        q6, d28, d5             ;(src_ptr[3] * vp8_filter[5])
    vmlal.u8        q8, d29, d5

    vext.8          d20, d7, d8, #5
    vext.8          d21, d10, d11, #5
    vext.8          d22, d6, d7, #2         ;construct src_ptr[0]
    vext.8          d23, d9, d10, #2
    vext.8          d24, d7, d8, #2
    vext.8          d25, d10, d11, #2

    vext.8          d26, d6, d7, #3         ;construct src_ptr[1]
    vext.8          d27, d9, d10, #3
    vext.8          d28, d7, d8, #3
    vext.8          d29, d10, d11, #3

    vmlal.u8        q7, d20, d5             ;(src_ptr[3] * vp8_filter[5])
    vmlal.u8        q9, d21, d5
    vmlal.u8        q6, d22, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q8, d23, d2
    vmlal.u8        q7, d24, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q9, d25, d2

    vmull.u8        q10, d26, d3            ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q11, d27, d3
    vmull.u8        q12, d28, d3            ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q15, d29, d3

    vqadd.s16       q6, q10                 ;sum of all (src_data*filter_parameters)
    vqadd.s16       q8, q11
    vqadd.s16       q7, q12
    vqadd.s16       q9, q15

    subs            r2, r2, #1

    vqrshrun.s16    d6, q6, #7              ;shift/round/saturate to u8
    vqrshrun.s16    d7, q7, #7
    vqrshrun.s16    d8, q8, #7
    vqrshrun.s16    d9, q9, #7

    vst1.u8         {q3}, [r4], r5              ;store result
    vst1.u8         {q4}, [r4], r5

    bne             filt_blk2d_fpo16x16_loop_neon

    pop             {r4-r5,pc}

;--------------------
secondpass_filter16x16_only
;Second pass: 16x16
    add             r3, r12, r3, lsl #5
    sub             r0, r0, r1, lsl #1

    vld1.s32        {q5, q6}, [r3]          ;load second_pass filter
    mov             r3, #2                  ;loop counter

    vabs.s32        q7, q5
    vabs.s32        q8, q6

    vdup.8          d0, d14[0]              ;second_pass filter parameters (d0-d5)
    vdup.8          d1, d14[4]
    vdup.8          d2, d15[0]
    vdup.8          d3, d15[4]
    vdup.8          d4, d16[0]
    vdup.8          d5, d16[4]

filt_blk2d_spo16x16_outloop_neon
    vld1.u8         {d18}, [r0], r1         ;load src data
    vld1.u8         {d19}, [r0], r1
    vld1.u8         {d20}, [r0], r1
    vld1.u8         {d21}, [r0], r1
    mov             r12, #4                 ;loop counter
    vld1.u8         {d22}, [r0], r1

secondpass_only_inner_loop_neon
    vld1.u8         {d23}, [r0], r1         ;load src data
    vld1.u8         {d24}, [r0], r1
    vld1.u8         {d25}, [r0], r1
    vld1.u8         {d26}, [r0], r1

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

    subs            r12, r12, #1

    vqadd.s16       q7, q3                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q8, q4
    vqadd.s16       q9, q5
    vqadd.s16       q10, q6

    vqrshrun.s16    d6, q7, #7              ;shift/round/saturate to u8
    vqrshrun.s16    d7, q8, #7
    vqrshrun.s16    d8, q9, #7
    vqrshrun.s16    d9, q10, #7

    vst1.u8         {d6}, [r4], r5          ;store result
    vmov            q9, q11
    vst1.u8         {d7}, [r4], r5
    vmov            q10, q12
    vst1.u8         {d8}, [r4], r5
    vmov            d22, d26
    vst1.u8         {d9}, [r4], r5

    bne             secondpass_only_inner_loop_neon

    subs            r3, r3, #1
    sub             r0, r0, r1, lsl #4
    sub             r0, r0, r1, lsl #2
    sub             r0, r0, r1
    add             r0, r0, #8

    sub             r4, r4, r5, lsl #4
    add             r4, r4, #8

    bne filt_blk2d_spo16x16_outloop_neon

    pop             {r4-r5,pc}

    ENDP

;-----------------
    AREA    subpelfilters16_dat, DATA, READWRITE            ;read/write by default
;Data section with name data_area is specified. DCD reserves space in memory for 48 data.
;One word each is reserved. Label filter_coeff can be used to access the data.
;Data address: filter_coeff, filter_coeff+4, filter_coeff+8 ...
_filter16_coeff_
    DCD     filter16_coeff
filter16_coeff
    DCD     0,  0,  128,    0,   0,  0,   0,  0
    DCD     0, -6,  123,   12,  -1,  0,   0,  0
    DCD     2, -11, 108,   36,  -8,  1,   0,  0
    DCD     0, -9,   93,   50,  -6,  0,   0,  0
    DCD     3, -16,  77,   77, -16,  3,   0,  0
    DCD     0, -6,   50,   93,  -9,  0,   0,  0
    DCD     1, -8,   36,  108, -11,  2,   0,  0
    DCD     0, -1,   12,  123,  -6,   0,  0,  0

    END
