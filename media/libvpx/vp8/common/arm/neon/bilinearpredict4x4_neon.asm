;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_bilinear_predict4x4_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
; r0    unsigned char  *src_ptr,
; r1    int  src_pixels_per_line,
; r2    int  xoffset,
; r3    int  yoffset,
; r4    unsigned char *dst_ptr,
; stack(lr) int  dst_pitch

|vp8_bilinear_predict4x4_neon| PROC
    push            {r4, lr}

    ldr             r12, _bifilter4_coeff_
    ldr             r4, [sp, #8]            ;load parameters from stack
    ldr             lr, [sp, #12]           ;load parameters from stack

    cmp             r2, #0                  ;skip first_pass filter if xoffset=0
    beq             skip_firstpass_filter

;First pass: output_height lines x output_width columns (5x4)
    vld1.u8         {d2}, [r0], r1          ;load src data
    add             r2, r12, r2, lsl #3     ;calculate Hfilter location (2coeffsx4bytes=8bytes)

    vld1.u8         {d3}, [r0], r1
    vld1.u32        {d31}, [r2]             ;first_pass filter

    vld1.u8         {d4}, [r0], r1
    vdup.8          d0, d31[0]              ;first_pass filter (d0-d1)
    vld1.u8         {d5}, [r0], r1
    vdup.8          d1, d31[4]
    vld1.u8         {d6}, [r0], r1

    vshr.u64        q4, q1, #8              ;construct src_ptr[1]
    vshr.u64        q5, q2, #8
    vshr.u64        d12, d6, #8

    vzip.32         d2, d3                  ;put 2-line data in 1 register (src_ptr[0])
    vzip.32         d4, d5
    vzip.32         d8, d9                  ;put 2-line data in 1 register (src_ptr[1])
    vzip.32         d10, d11

    vmull.u8        q7, d2, d0              ;(src_ptr[0] * vp8_filter[0])
    vmull.u8        q8, d4, d0
    vmull.u8        q9, d6, d0

    vmlal.u8        q7, d8, d1              ;(src_ptr[1] * vp8_filter[1])
    vmlal.u8        q8, d10, d1
    vmlal.u8        q9, d12, d1

    vqrshrn.u16    d28, q7, #7              ;shift/round/saturate to u8
    vqrshrn.u16    d29, q8, #7
    vqrshrn.u16    d30, q9, #7

;Second pass: 4x4
secondpass_filter
    cmp             r3, #0                  ;skip second_pass filter if yoffset=0
    beq             skip_secondpass_filter

    add             r3, r12, r3, lsl #3 ;calculate Vfilter location
    vld1.u32        {d31}, [r3]         ;load second_pass filter

    vdup.8          d0, d31[0]              ;second_pass filter parameters (d0-d5)
    vdup.8          d1, d31[4]

    vmull.u8        q1, d28, d0
    vmull.u8        q2, d29, d0

    vext.8          d26, d28, d29, #4       ;construct src_ptr[pixel_step]
    vext.8          d27, d29, d30, #4

    vmlal.u8        q1, d26, d1
    vmlal.u8        q2, d27, d1

    add             r0, r4, lr
    add             r1, r0, lr
    add             r2, r1, lr

    vqrshrn.u16    d2, q1, #7               ;shift/round/saturate to u8
    vqrshrn.u16    d3, q2, #7

    vst1.32         {d2[0]}, [r4]           ;store result
    vst1.32         {d2[1]}, [r0]
    vst1.32         {d3[0]}, [r1]
    vst1.32         {d3[1]}, [r2]

    pop             {r4, pc}

;--------------------
skip_firstpass_filter

    vld1.32         {d28[0]}, [r0], r1      ;load src data
    vld1.32         {d28[1]}, [r0], r1
    vld1.32         {d29[0]}, [r0], r1
    vld1.32         {d29[1]}, [r0], r1
    vld1.32         {d30[0]}, [r0], r1

    b               secondpass_filter

;---------------------
skip_secondpass_filter
    vst1.32         {d28[0]}, [r4], lr      ;store result
    vst1.32         {d28[1]}, [r4], lr
    vst1.32         {d29[0]}, [r4], lr
    vst1.32         {d29[1]}, [r4], lr

    pop             {r4, pc}

    ENDP

;-----------------
    AREA    bilinearfilters4_dat, DATA, READWRITE           ;read/write by default
;Data section with name data_area is specified. DCD reserves space in memory for 48 data.
;One word each is reserved. Label filter_coeff can be used to access the data.
;Data address: filter_coeff, filter_coeff+4, filter_coeff+8 ...
_bifilter4_coeff_
    DCD     bifilter4_coeff
bifilter4_coeff
    DCD     128, 0, 112, 16, 96, 32, 80, 48, 64, 64, 48, 80, 32, 96, 16, 112

    END
