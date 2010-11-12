;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_bilinear_predict16x16_neon|
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

|vp8_bilinear_predict16x16_neon| PROC
    push            {r4-r5, lr}

    ldr             r12, _bifilter16_coeff_
    ldr             r4, [sp, #12]           ;load parameters from stack
    ldr             r5, [sp, #16]           ;load parameters from stack

    cmp             r2, #0                  ;skip first_pass filter if xoffset=0
    beq             secondpass_bfilter16x16_only

    add             r2, r12, r2, lsl #3     ;calculate filter location

    cmp             r3, #0                  ;skip second_pass filter if yoffset=0

    vld1.s32        {d31}, [r2]             ;load first_pass filter

    beq             firstpass_bfilter16x16_only

    sub             sp, sp, #272            ;reserve space on stack for temporary storage
    vld1.u8         {d2, d3, d4}, [r0], r1      ;load src data
    mov             lr, sp
    vld1.u8         {d5, d6, d7}, [r0], r1

    mov             r2, #3                  ;loop counter
    vld1.u8         {d8, d9, d10}, [r0], r1

    vdup.8          d0, d31[0]              ;first_pass filter (d0 d1)
    vld1.u8         {d11, d12, d13}, [r0], r1

    vdup.8          d1, d31[4]

;First Pass: output_height lines x output_width columns (17x16)
filt_blk2d_fp16x16_loop_neon
    pld             [r0]
    pld             [r0, r1]
    pld             [r0, r1, lsl #1]

    vmull.u8        q7, d2, d0              ;(src_ptr[0] * vp8_filter[0])
    vmull.u8        q8, d3, d0
    vmull.u8        q9, d5, d0
    vmull.u8        q10, d6, d0
    vmull.u8        q11, d8, d0
    vmull.u8        q12, d9, d0
    vmull.u8        q13, d11, d0
    vmull.u8        q14, d12, d0

    vext.8          d2, d2, d3, #1          ;construct src_ptr[1]
    vext.8          d5, d5, d6, #1
    vext.8          d8, d8, d9, #1
    vext.8          d11, d11, d12, #1

    vmlal.u8        q7, d2, d1              ;(src_ptr[0] * vp8_filter[1])
    vmlal.u8        q9, d5, d1
    vmlal.u8        q11, d8, d1
    vmlal.u8        q13, d11, d1

    vext.8          d3, d3, d4, #1
    vext.8          d6, d6, d7, #1
    vext.8          d9, d9, d10, #1
    vext.8          d12, d12, d13, #1

    vmlal.u8        q8, d3, d1              ;(src_ptr[0] * vp8_filter[1])
    vmlal.u8        q10, d6, d1
    vmlal.u8        q12, d9, d1
    vmlal.u8        q14, d12, d1

    subs            r2, r2, #1

    vqrshrn.u16    d14, q7, #7              ;shift/round/saturate to u8
    vqrshrn.u16    d15, q8, #7
    vqrshrn.u16    d16, q9, #7
    vqrshrn.u16    d17, q10, #7
    vqrshrn.u16    d18, q11, #7
    vqrshrn.u16    d19, q12, #7
    vqrshrn.u16    d20, q13, #7

    vld1.u8         {d2, d3, d4}, [r0], r1      ;load src data
    vqrshrn.u16    d21, q14, #7
    vld1.u8         {d5, d6, d7}, [r0], r1

    vst1.u8         {d14, d15, d16, d17}, [lr]!     ;store result
    vld1.u8         {d8, d9, d10}, [r0], r1
    vst1.u8         {d18, d19, d20, d21}, [lr]!
    vld1.u8         {d11, d12, d13}, [r0], r1

    bne             filt_blk2d_fp16x16_loop_neon

;First-pass filtering for rest 5 lines
    vld1.u8         {d14, d15, d16}, [r0], r1

    vmull.u8        q9, d2, d0              ;(src_ptr[0] * vp8_filter[0])
    vmull.u8        q10, d3, d0
    vmull.u8        q11, d5, d0
    vmull.u8        q12, d6, d0
    vmull.u8        q13, d8, d0
    vmull.u8        q14, d9, d0

    vext.8          d2, d2, d3, #1          ;construct src_ptr[1]
    vext.8          d5, d5, d6, #1
    vext.8          d8, d8, d9, #1

    vmlal.u8        q9, d2, d1              ;(src_ptr[0] * vp8_filter[1])
    vmlal.u8        q11, d5, d1
    vmlal.u8        q13, d8, d1

    vext.8          d3, d3, d4, #1
    vext.8          d6, d6, d7, #1
    vext.8          d9, d9, d10, #1

    vmlal.u8        q10, d3, d1             ;(src_ptr[0] * vp8_filter[1])
    vmlal.u8        q12, d6, d1
    vmlal.u8        q14, d9, d1

    vmull.u8        q1, d11, d0
    vmull.u8        q2, d12, d0
    vmull.u8        q3, d14, d0
    vmull.u8        q4, d15, d0

    vext.8          d11, d11, d12, #1       ;construct src_ptr[1]
    vext.8          d14, d14, d15, #1

    vmlal.u8        q1, d11, d1             ;(src_ptr[0] * vp8_filter[1])
    vmlal.u8        q3, d14, d1

    vext.8          d12, d12, d13, #1
    vext.8          d15, d15, d16, #1

    vmlal.u8        q2, d12, d1             ;(src_ptr[0] * vp8_filter[1])
    vmlal.u8        q4, d15, d1

    vqrshrn.u16    d10, q9, #7              ;shift/round/saturate to u8
    vqrshrn.u16    d11, q10, #7
    vqrshrn.u16    d12, q11, #7
    vqrshrn.u16    d13, q12, #7
    vqrshrn.u16    d14, q13, #7
    vqrshrn.u16    d15, q14, #7
    vqrshrn.u16    d16, q1, #7
    vqrshrn.u16    d17, q2, #7
    vqrshrn.u16    d18, q3, #7
    vqrshrn.u16    d19, q4, #7

    vst1.u8         {d10, d11, d12, d13}, [lr]!         ;store result
    vst1.u8         {d14, d15, d16, d17}, [lr]!
    vst1.u8         {d18, d19}, [lr]!

;Second pass: 16x16
;secondpass_filter
    add             r3, r12, r3, lsl #3
    sub             lr, lr, #272

    vld1.u32        {d31}, [r3]             ;load second_pass filter

    vld1.u8         {d22, d23}, [lr]!       ;load src data

    vdup.8          d0, d31[0]              ;second_pass filter parameters (d0 d1)
    vdup.8          d1, d31[4]
    mov             r12, #4                 ;loop counter

filt_blk2d_sp16x16_loop_neon
    vld1.u8         {d24, d25}, [lr]!
    vmull.u8        q1, d22, d0             ;(src_ptr[0] * vp8_filter[0])
    vld1.u8         {d26, d27}, [lr]!
    vmull.u8        q2, d23, d0
    vld1.u8         {d28, d29}, [lr]!
    vmull.u8        q3, d24, d0
    vld1.u8         {d30, d31}, [lr]!

    vmull.u8        q4, d25, d0
    vmull.u8        q5, d26, d0
    vmull.u8        q6, d27, d0
    vmull.u8        q7, d28, d0
    vmull.u8        q8, d29, d0

    vmlal.u8        q1, d24, d1             ;(src_ptr[pixel_step] * vp8_filter[1])
    vmlal.u8        q2, d25, d1
    vmlal.u8        q3, d26, d1
    vmlal.u8        q4, d27, d1
    vmlal.u8        q5, d28, d1
    vmlal.u8        q6, d29, d1
    vmlal.u8        q7, d30, d1
    vmlal.u8        q8, d31, d1

    subs            r12, r12, #1

    vqrshrn.u16    d2, q1, #7               ;shift/round/saturate to u8
    vqrshrn.u16    d3, q2, #7
    vqrshrn.u16    d4, q3, #7
    vqrshrn.u16    d5, q4, #7
    vqrshrn.u16    d6, q5, #7
    vqrshrn.u16    d7, q6, #7
    vqrshrn.u16    d8, q7, #7
    vqrshrn.u16    d9, q8, #7

    vst1.u8         {d2, d3}, [r4], r5      ;store result
    vst1.u8         {d4, d5}, [r4], r5
    vst1.u8         {d6, d7}, [r4], r5
    vmov            q11, q15
    vst1.u8         {d8, d9}, [r4], r5

    bne             filt_blk2d_sp16x16_loop_neon

    add             sp, sp, #272

    pop             {r4-r5,pc}

;--------------------
firstpass_bfilter16x16_only
    mov             r2, #4                      ;loop counter
    vdup.8          d0, d31[0]                  ;first_pass filter (d0 d1)
    vdup.8          d1, d31[4]

;First Pass: output_height lines x output_width columns (16x16)
filt_blk2d_fpo16x16_loop_neon
    vld1.u8         {d2, d3, d4}, [r0], r1      ;load src data
    vld1.u8         {d5, d6, d7}, [r0], r1
    vld1.u8         {d8, d9, d10}, [r0], r1
    vld1.u8         {d11, d12, d13}, [r0], r1

    pld             [r0]
    pld             [r0, r1]
    pld             [r0, r1, lsl #1]

    vmull.u8        q7, d2, d0              ;(src_ptr[0] * vp8_filter[0])
    vmull.u8        q8, d3, d0
    vmull.u8        q9, d5, d0
    vmull.u8        q10, d6, d0
    vmull.u8        q11, d8, d0
    vmull.u8        q12, d9, d0
    vmull.u8        q13, d11, d0
    vmull.u8        q14, d12, d0

    vext.8          d2, d2, d3, #1          ;construct src_ptr[1]
    vext.8          d5, d5, d6, #1
    vext.8          d8, d8, d9, #1
    vext.8          d11, d11, d12, #1

    vmlal.u8        q7, d2, d1              ;(src_ptr[0] * vp8_filter[1])
    vmlal.u8        q9, d5, d1
    vmlal.u8        q11, d8, d1
    vmlal.u8        q13, d11, d1

    vext.8          d3, d3, d4, #1
    vext.8          d6, d6, d7, #1
    vext.8          d9, d9, d10, #1
    vext.8          d12, d12, d13, #1

    vmlal.u8        q8, d3, d1              ;(src_ptr[0] * vp8_filter[1])
    vmlal.u8        q10, d6, d1
    vmlal.u8        q12, d9, d1
    vmlal.u8        q14, d12, d1

    subs            r2, r2, #1

    vqrshrn.u16    d14, q7, #7              ;shift/round/saturate to u8
    vqrshrn.u16    d15, q8, #7
    vqrshrn.u16    d16, q9, #7
    vqrshrn.u16    d17, q10, #7
    vqrshrn.u16    d18, q11, #7
    vqrshrn.u16    d19, q12, #7
    vqrshrn.u16    d20, q13, #7
    vst1.u8         {d14, d15}, [r4], r5        ;store result
    vqrshrn.u16    d21, q14, #7

    vst1.u8         {d16, d17}, [r4], r5
    vst1.u8         {d18, d19}, [r4], r5
    vst1.u8         {d20, d21}, [r4], r5

    bne             filt_blk2d_fpo16x16_loop_neon
    pop             {r4-r5,pc}

;---------------------
secondpass_bfilter16x16_only
;Second pass: 16x16
;secondpass_filter
    add             r3, r12, r3, lsl #3
    mov             r12, #4                     ;loop counter
    vld1.u32        {d31}, [r3]                 ;load second_pass filter
    vld1.u8         {d22, d23}, [r0], r1        ;load src data

    vdup.8          d0, d31[0]                  ;second_pass filter parameters (d0 d1)
    vdup.8          d1, d31[4]

filt_blk2d_spo16x16_loop_neon
    vld1.u8         {d24, d25}, [r0], r1
    vmull.u8        q1, d22, d0             ;(src_ptr[0] * vp8_filter[0])
    vld1.u8         {d26, d27}, [r0], r1
    vmull.u8        q2, d23, d0
    vld1.u8         {d28, d29}, [r0], r1
    vmull.u8        q3, d24, d0
    vld1.u8         {d30, d31}, [r0], r1

    vmull.u8        q4, d25, d0
    vmull.u8        q5, d26, d0
    vmull.u8        q6, d27, d0
    vmull.u8        q7, d28, d0
    vmull.u8        q8, d29, d0

    vmlal.u8        q1, d24, d1             ;(src_ptr[pixel_step] * vp8_filter[1])
    vmlal.u8        q2, d25, d1
    vmlal.u8        q3, d26, d1
    vmlal.u8        q4, d27, d1
    vmlal.u8        q5, d28, d1
    vmlal.u8        q6, d29, d1
    vmlal.u8        q7, d30, d1
    vmlal.u8        q8, d31, d1

    vqrshrn.u16    d2, q1, #7               ;shift/round/saturate to u8
    vqrshrn.u16    d3, q2, #7
    vqrshrn.u16    d4, q3, #7
    vqrshrn.u16    d5, q4, #7
    vqrshrn.u16    d6, q5, #7
    vqrshrn.u16    d7, q6, #7
    vqrshrn.u16    d8, q7, #7
    vqrshrn.u16    d9, q8, #7

    vst1.u8         {d2, d3}, [r4], r5      ;store result
    subs            r12, r12, #1
    vst1.u8         {d4, d5}, [r4], r5
    vmov            q11, q15
    vst1.u8         {d6, d7}, [r4], r5
    vst1.u8         {d8, d9}, [r4], r5

    bne             filt_blk2d_spo16x16_loop_neon
    pop             {r4-r5,pc}

    ENDP

;-----------------
    AREA    bifilters16_dat, DATA, READWRITE            ;read/write by default
;Data section with name data_area is specified. DCD reserves space in memory for 48 data.
;One word each is reserved. Label filter_coeff can be used to access the data.
;Data address: filter_coeff, filter_coeff+4, filter_coeff+8 ...
_bifilter16_coeff_
    DCD     bifilter16_coeff
bifilter16_coeff
    DCD     128, 0, 112, 16, 96, 32, 80, 48, 64, 64, 48, 80, 32, 96, 16, 112

    END
