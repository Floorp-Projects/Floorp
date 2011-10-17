;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_bilinear_predict8x4_neon|
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

|vp8_bilinear_predict8x4_neon| PROC
    push            {r4, lr}

    adr             r12, bifilter8x4_coeff
    ldr             r4, [sp, #8]            ;load parameters from stack
    ldr             lr, [sp, #12]           ;load parameters from stack

    cmp             r2, #0                  ;skip first_pass filter if xoffset=0
    beq             skip_firstpass_filter

;First pass: output_height lines x output_width columns (5x8)
    add             r2, r12, r2, lsl #3     ;calculate filter location

    vld1.u8         {q1}, [r0], r1          ;load src data
    vld1.u32        {d31}, [r2]             ;load first_pass filter
    vld1.u8         {q2}, [r0], r1
    vdup.8          d0, d31[0]              ;first_pass filter (d0 d1)
    vld1.u8         {q3}, [r0], r1
    vdup.8          d1, d31[4]
    vld1.u8         {q4}, [r0], r1

    vmull.u8        q6, d2, d0              ;(src_ptr[0] * vp8_filter[0])
    vld1.u8         {q5}, [r0], r1
    vmull.u8        q7, d4, d0
    vmull.u8        q8, d6, d0
    vmull.u8        q9, d8, d0
    vmull.u8        q10, d10, d0

    vext.8          d3, d2, d3, #1          ;construct src_ptr[-1]
    vext.8          d5, d4, d5, #1
    vext.8          d7, d6, d7, #1
    vext.8          d9, d8, d9, #1
    vext.8          d11, d10, d11, #1

    vmlal.u8        q6, d3, d1              ;(src_ptr[1] * vp8_filter[1])
    vmlal.u8        q7, d5, d1
    vmlal.u8        q8, d7, d1
    vmlal.u8        q9, d9, d1
    vmlal.u8        q10, d11, d1

    vqrshrn.u16    d22, q6, #7              ;shift/round/saturate to u8
    vqrshrn.u16    d23, q7, #7
    vqrshrn.u16    d24, q8, #7
    vqrshrn.u16    d25, q9, #7
    vqrshrn.u16    d26, q10, #7

;Second pass: 4x8
secondpass_filter
    cmp             r3, #0                  ;skip second_pass filter if yoffset=0
    beq             skip_secondpass_filter

    add             r3, r12, r3, lsl #3
    add             r0, r4, lr

    vld1.u32        {d31}, [r3]             ;load second_pass filter
    add             r1, r0, lr

    vdup.8          d0, d31[0]              ;second_pass filter parameters (d0 d1)
    vdup.8          d1, d31[4]

    vmull.u8        q1, d22, d0             ;(src_ptr[0] * vp8_filter[0])
    vmull.u8        q2, d23, d0
    vmull.u8        q3, d24, d0
    vmull.u8        q4, d25, d0

    vmlal.u8        q1, d23, d1             ;(src_ptr[pixel_step] * vp8_filter[1])
    vmlal.u8        q2, d24, d1
    vmlal.u8        q3, d25, d1
    vmlal.u8        q4, d26, d1

    add             r2, r1, lr

    vqrshrn.u16    d2, q1, #7               ;shift/round/saturate to u8
    vqrshrn.u16    d3, q2, #7
    vqrshrn.u16    d4, q3, #7
    vqrshrn.u16    d5, q4, #7

    vst1.u8         {d2}, [r4]              ;store result
    vst1.u8         {d3}, [r0]
    vst1.u8         {d4}, [r1]
    vst1.u8         {d5}, [r2]

    pop             {r4, pc}

;--------------------
skip_firstpass_filter
    vld1.u8         {d22}, [r0], r1         ;load src data
    vld1.u8         {d23}, [r0], r1
    vld1.u8         {d24}, [r0], r1
    vld1.u8         {d25}, [r0], r1
    vld1.u8         {d26}, [r0], r1

    b               secondpass_filter

;---------------------
skip_secondpass_filter
    vst1.u8         {d22}, [r4], lr         ;store result
    vst1.u8         {d23}, [r4], lr
    vst1.u8         {d24}, [r4], lr
    vst1.u8         {d25}, [r4], lr

    pop             {r4, pc}

    ENDP

;-----------------

bifilter8x4_coeff
    DCD     128, 0, 112, 16, 96, 32, 80, 48, 64, 64, 48, 80, 32, 96, 16, 112

    END
