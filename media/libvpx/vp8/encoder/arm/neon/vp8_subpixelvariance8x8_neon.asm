;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_sub_pixel_variance8x8_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
; r0    unsigned char  *src_ptr,
; r1    int  src_pixels_per_line,
; r2    int  xoffset,
; r3    int  yoffset,
; stack(r4) unsigned char *dst_ptr,
; stack(r5) int dst_pixels_per_line,
; stack(r6) unsigned int *sse
;note: most of the code is copied from bilinear_predict8x8_neon and vp8_variance8x8_neon.

|vp8_sub_pixel_variance8x8_neon| PROC
    push            {r4-r5, lr}

    ldr             r12, _BilinearTaps_coeff_
    ldr             r4, [sp, #12]           ;load *dst_ptr from stack
    ldr             r5, [sp, #16]           ;load dst_pixels_per_line from stack
    ldr             lr, [sp, #20]           ;load *sse from stack

    cmp             r2, #0                  ;skip first_pass filter if xoffset=0
    beq             skip_firstpass_filter

;First pass: output_height lines x output_width columns (9x8)
    add             r2, r12, r2, lsl #3     ;calculate filter location

    vld1.u8         {q1}, [r0], r1          ;load src data
    vld1.u32        {d31}, [r2]             ;load first_pass filter
    vld1.u8         {q2}, [r0], r1
    vdup.8          d0, d31[0]              ;first_pass filter (d0 d1)
    vld1.u8         {q3}, [r0], r1
    vdup.8          d1, d31[4]
    vld1.u8         {q4}, [r0], r1

    vmull.u8        q6, d2, d0              ;(src_ptr[0] * Filter[0])
    vmull.u8        q7, d4, d0
    vmull.u8        q8, d6, d0
    vmull.u8        q9, d8, d0

    vext.8          d3, d2, d3, #1          ;construct src_ptr[-1]
    vext.8          d5, d4, d5, #1
    vext.8          d7, d6, d7, #1
    vext.8          d9, d8, d9, #1

    vmlal.u8        q6, d3, d1              ;(src_ptr[1] * Filter[1])
    vmlal.u8        q7, d5, d1
    vmlal.u8        q8, d7, d1
    vmlal.u8        q9, d9, d1

    vld1.u8         {q1}, [r0], r1          ;load src data
    vqrshrn.u16    d22, q6, #7              ;shift/round/saturate to u8
    vld1.u8         {q2}, [r0], r1
    vqrshrn.u16    d23, q7, #7
    vld1.u8         {q3}, [r0], r1
    vqrshrn.u16    d24, q8, #7
    vld1.u8         {q4}, [r0], r1
    vqrshrn.u16    d25, q9, #7

    ;first_pass filtering on the rest 5-line data
    vld1.u8         {q5}, [r0], r1

    vmull.u8        q6, d2, d0              ;(src_ptr[0] * Filter[0])
    vmull.u8        q7, d4, d0
    vmull.u8        q8, d6, d0
    vmull.u8        q9, d8, d0
    vmull.u8        q10, d10, d0

    vext.8          d3, d2, d3, #1          ;construct src_ptr[-1]
    vext.8          d5, d4, d5, #1
    vext.8          d7, d6, d7, #1
    vext.8          d9, d8, d9, #1
    vext.8          d11, d10, d11, #1

    vmlal.u8        q6, d3, d1              ;(src_ptr[1] * Filter[1])
    vmlal.u8        q7, d5, d1
    vmlal.u8        q8, d7, d1
    vmlal.u8        q9, d9, d1
    vmlal.u8        q10, d11, d1

    vqrshrn.u16    d26, q6, #7              ;shift/round/saturate to u8
    vqrshrn.u16    d27, q7, #7
    vqrshrn.u16    d28, q8, #7
    vqrshrn.u16    d29, q9, #7
    vqrshrn.u16    d30, q10, #7

;Second pass: 8x8
secondpass_filter
    cmp             r3, #0                  ;skip second_pass filter if yoffset=0
    ;skip_secondpass_filter
    beq             sub_pixel_variance8x8_neon

    add             r3, r12, r3, lsl #3

    vld1.u32        {d31}, [r3]             ;load second_pass filter

    vdup.8          d0, d31[0]              ;second_pass filter parameters (d0 d1)
    vdup.8          d1, d31[4]

    vmull.u8        q1, d22, d0             ;(src_ptr[0] * Filter[0])
    vmull.u8        q2, d23, d0
    vmull.u8        q3, d24, d0
    vmull.u8        q4, d25, d0
    vmull.u8        q5, d26, d0
    vmull.u8        q6, d27, d0
    vmull.u8        q7, d28, d0
    vmull.u8        q8, d29, d0

    vmlal.u8        q1, d23, d1             ;(src_ptr[pixel_step] * Filter[1])
    vmlal.u8        q2, d24, d1
    vmlal.u8        q3, d25, d1
    vmlal.u8        q4, d26, d1
    vmlal.u8        q5, d27, d1
    vmlal.u8        q6, d28, d1
    vmlal.u8        q7, d29, d1
    vmlal.u8        q8, d30, d1

    vqrshrn.u16    d22, q1, #7              ;shift/round/saturate to u8
    vqrshrn.u16    d23, q2, #7
    vqrshrn.u16    d24, q3, #7
    vqrshrn.u16    d25, q4, #7
    vqrshrn.u16    d26, q5, #7
    vqrshrn.u16    d27, q6, #7
    vqrshrn.u16    d28, q7, #7
    vqrshrn.u16    d29, q8, #7

    b               sub_pixel_variance8x8_neon

;--------------------
skip_firstpass_filter
    vld1.u8         {d22}, [r0], r1         ;load src data
    vld1.u8         {d23}, [r0], r1
    vld1.u8         {d24}, [r0], r1
    vld1.u8         {d25}, [r0], r1
    vld1.u8         {d26}, [r0], r1
    vld1.u8         {d27}, [r0], r1
    vld1.u8         {d28}, [r0], r1
    vld1.u8         {d29}, [r0], r1
    vld1.u8         {d30}, [r0], r1

    b               secondpass_filter

;----------------------
;vp8_variance8x8_neon
sub_pixel_variance8x8_neon
    vmov.i8         q8, #0                      ;q8 - sum
    vmov.i8         q9, #0                      ;q9, q10 - sse
    vmov.i8         q10, #0

    mov             r12, #2

sub_pixel_variance8x8_neon_loop
    vld1.8          {d0}, [r4], r5              ;load dst data
    subs            r12, r12, #1
    vld1.8          {d1}, [r4], r5
    vld1.8          {d2}, [r4], r5
    vsubl.u8        q4, d22, d0                 ;calculate diff
    vld1.8          {d3}, [r4], r5

    vsubl.u8        q5, d23, d1
    vsubl.u8        q6, d24, d2

    vpadal.s16      q8, q4                      ;sum
    vmlal.s16       q9, d8, d8                  ;sse
    vmlal.s16       q10, d9, d9

    vsubl.u8        q7, d25, d3

    vpadal.s16      q8, q5
    vmlal.s16       q9, d10, d10
    vmlal.s16       q10, d11, d11

    vmov            q11, q13

    vpadal.s16      q8, q6
    vmlal.s16       q9, d12, d12
    vmlal.s16       q10, d13, d13

    vmov            q12, q14

    vpadal.s16      q8, q7
    vmlal.s16       q9, d14, d14
    vmlal.s16       q10, d15, d15

    bne             sub_pixel_variance8x8_neon_loop

    vadd.u32        q10, q9, q10                ;accumulate sse
    vpaddl.s32      q0, q8                      ;accumulate sum

    vpaddl.u32      q1, q10
    vadd.s64        d0, d0, d1
    vadd.u64        d1, d2, d3

    vmull.s32       q5, d0, d0
    vst1.32         {d1[0]}, [lr]               ;store sse
    vshr.s32        d10, d10, #6
    vsub.s32        d0, d1, d10

    vmov.32         r0, d0[0]                   ;return
    pop             {r4-r5, pc}

    ENDP

;-----------------

_BilinearTaps_coeff_
    DCD     bilinear_taps_coeff
bilinear_taps_coeff
    DCD     128, 0, 112, 16, 96, 32, 80, 48, 64, 64, 48, 80, 32, 96, 16, 112

    END
