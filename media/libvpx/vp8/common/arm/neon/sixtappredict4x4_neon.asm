;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_sixtap_predict4x4_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

filter4_coeff
    DCD     0,  0,  128,    0,   0,  0,   0,  0
    DCD     0, -6,  123,   12,  -1,  0,   0,  0
    DCD     2, -11, 108,   36,  -8,  1,   0,  0
    DCD     0, -9,   93,   50,  -6,  0,   0,  0
    DCD     3, -16,  77,   77, -16,  3,   0,  0
    DCD     0, -6,   50,   93,  -9,  0,   0,  0
    DCD     1, -8,   36,  108, -11,  2,   0,  0
    DCD     0, -1,   12,  123,  -6,   0,  0,  0

; r0    unsigned char  *src_ptr,
; r1    int  src_pixels_per_line,
; r2    int  xoffset,
; r3    int  yoffset,
; stack(r4) unsigned char *dst_ptr,
; stack(lr) int  dst_pitch

|vp8_sixtap_predict4x4_neon| PROC
    push            {r4, lr}

    adr             r12, filter4_coeff
    ldr             r4, [sp, #8]            ;load parameters from stack
    ldr             lr, [sp, #12]           ;load parameters from stack

    cmp             r2, #0                  ;skip first_pass filter if xoffset=0
    beq             secondpass_filter4x4_only

    add             r2, r12, r2, lsl #5     ;calculate filter location

    cmp             r3, #0                  ;skip second_pass filter if yoffset=0
    vld1.s32        {q14, q15}, [r2]        ;load first_pass filter

    beq             firstpass_filter4x4_only

    vabs.s32        q12, q14                ;get abs(filer_parameters)
    vabs.s32        q13, q15

    sub             r0, r0, #2              ;go back 2 columns of src data
    sub             r0, r0, r1, lsl #1      ;go back 2 lines of src data

;First pass: output_height lines x output_width columns (9x4)
    vld1.u8         {q3}, [r0], r1          ;load first 4-line src data
    vdup.8          d0, d24[0]              ;first_pass filter (d0-d5)
    vld1.u8         {q4}, [r0], r1
    vdup.8          d1, d24[4]
    vld1.u8         {q5}, [r0], r1
    vdup.8          d2, d25[0]
    vld1.u8         {q6}, [r0], r1
    vdup.8          d3, d25[4]
    vdup.8          d4, d26[0]
    vdup.8          d5, d26[4]

    pld             [r0]
    pld             [r0, r1]
    pld             [r0, r1, lsl #1]

    vext.8          d18, d6, d7, #5         ;construct src_ptr[3]
    vext.8          d19, d8, d9, #5
    vext.8          d20, d10, d11, #5
    vext.8          d21, d12, d13, #5

    vswp            d7, d8                  ;discard 2nd half data after src_ptr[3] is done
    vswp            d11, d12

    vzip.32         d18, d19                ;put 2-line data in 1 register (src_ptr[3])
    vzip.32         d20, d21
    vmull.u8        q7, d18, d5             ;(src_ptr[3] * vp8_filter[5])
    vmull.u8        q8, d20, d5

    vmov            q4, q3                  ;keep original src data in q4 q6
    vmov            q6, q5

    vzip.32         d6, d7                  ;construct src_ptr[-2], and put 2-line data together
    vzip.32         d10, d11
    vshr.u64        q9, q4, #8              ;construct src_ptr[-1]
    vshr.u64        q10, q6, #8
    vmlal.u8        q7, d6, d0              ;+(src_ptr[-2] * vp8_filter[0])
    vmlal.u8        q8, d10, d0

    vzip.32         d18, d19                ;put 2-line data in 1 register (src_ptr[-1])
    vzip.32         d20, d21
    vshr.u64        q3, q4, #32             ;construct src_ptr[2]
    vshr.u64        q5, q6, #32
    vmlsl.u8        q7, d18, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q8, d20, d1

    vzip.32         d6, d7                  ;put 2-line data in 1 register (src_ptr[2])
    vzip.32         d10, d11
    vshr.u64        q9, q4, #16             ;construct src_ptr[0]
    vshr.u64        q10, q6, #16
    vmlsl.u8        q7, d6, d4              ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q8, d10, d4

    vzip.32         d18, d19                ;put 2-line data in 1 register (src_ptr[0])
    vzip.32         d20, d21
    vshr.u64        q3, q4, #24             ;construct src_ptr[1]
    vshr.u64        q5, q6, #24
    vmlal.u8        q7, d18, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q8, d20, d2

    vzip.32         d6, d7                  ;put 2-line data in 1 register (src_ptr[1])
    vzip.32         d10, d11
    vmull.u8        q9, d6, d3              ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q10, d10, d3

    vld1.u8         {q3}, [r0], r1          ;load rest 5-line src data
    vld1.u8         {q4}, [r0], r1

    vqadd.s16       q7, q9                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q8, q10

    vld1.u8         {q5}, [r0], r1
    vld1.u8         {q6}, [r0], r1

    vqrshrun.s16    d27, q7, #7             ;shift/round/saturate to u8
    vqrshrun.s16    d28, q8, #7

    ;First Pass on rest 5-line data
    vld1.u8         {q11}, [r0], r1

    vext.8          d18, d6, d7, #5         ;construct src_ptr[3]
    vext.8          d19, d8, d9, #5
    vext.8          d20, d10, d11, #5
    vext.8          d21, d12, d13, #5

    vswp            d7, d8                  ;discard 2nd half data after src_ptr[3] is done
    vswp            d11, d12

    vzip.32         d18, d19                ;put 2-line data in 1 register (src_ptr[3])
    vzip.32         d20, d21
    vext.8          d31, d22, d23, #5       ;construct src_ptr[3]
    vmull.u8        q7, d18, d5             ;(src_ptr[3] * vp8_filter[5])
    vmull.u8        q8, d20, d5
    vmull.u8        q12, d31, d5            ;(src_ptr[3] * vp8_filter[5])

    vmov            q4, q3                  ;keep original src data in q4 q6
    vmov            q6, q5

    vzip.32         d6, d7                  ;construct src_ptr[-2], and put 2-line data together
    vzip.32         d10, d11
    vshr.u64        q9, q4, #8              ;construct src_ptr[-1]
    vshr.u64        q10, q6, #8

    vmlal.u8        q7, d6, d0              ;+(src_ptr[-2] * vp8_filter[0])
    vmlal.u8        q8, d10, d0
    vmlal.u8        q12, d22, d0            ;(src_ptr[-2] * vp8_filter[0])

    vzip.32         d18, d19                ;put 2-line data in 1 register (src_ptr[-1])
    vzip.32         d20, d21
    vshr.u64        q3, q4, #32             ;construct src_ptr[2]
    vshr.u64        q5, q6, #32
    vext.8          d31, d22, d23, #1       ;construct src_ptr[-1]

    vmlsl.u8        q7, d18, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q8, d20, d1
    vmlsl.u8        q12, d31, d1            ;-(src_ptr[-1] * vp8_filter[1])

    vzip.32         d6, d7                  ;put 2-line data in 1 register (src_ptr[2])
    vzip.32         d10, d11
    vshr.u64        q9, q4, #16             ;construct src_ptr[0]
    vshr.u64        q10, q6, #16
    vext.8          d31, d22, d23, #4       ;construct src_ptr[2]

    vmlsl.u8        q7, d6, d4              ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q8, d10, d4
    vmlsl.u8        q12, d31, d4            ;-(src_ptr[2] * vp8_filter[4])

    vzip.32         d18, d19                ;put 2-line data in 1 register (src_ptr[0])
    vzip.32         d20, d21
    vshr.u64        q3, q4, #24             ;construct src_ptr[1]
    vshr.u64        q5, q6, #24
    vext.8          d31, d22, d23, #2       ;construct src_ptr[0]

    vmlal.u8        q7, d18, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q8, d20, d2
    vmlal.u8        q12, d31, d2            ;(src_ptr[0] * vp8_filter[2])

    vzip.32         d6, d7                  ;put 2-line data in 1 register (src_ptr[1])
    vzip.32         d10, d11
    vext.8          d31, d22, d23, #3       ;construct src_ptr[1]
    vmull.u8        q9, d6, d3              ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q10, d10, d3
    vmull.u8        q11, d31, d3            ;(src_ptr[1] * vp8_filter[3])

    add             r3, r12, r3, lsl #5

    vqadd.s16       q7, q9                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q8, q10
    vqadd.s16       q12, q11

    vext.8          d23, d27, d28, #4
    vld1.s32        {q5, q6}, [r3]          ;load second_pass filter

    vqrshrun.s16    d29, q7, #7             ;shift/round/saturate to u8
    vqrshrun.s16    d30, q8, #7
    vqrshrun.s16    d31, q12, #7

;Second pass: 4x4
    vabs.s32        q7, q5
    vabs.s32        q8, q6

    vext.8          d24, d28, d29, #4
    vext.8          d25, d29, d30, #4
    vext.8          d26, d30, d31, #4

    vdup.8          d0, d14[0]              ;second_pass filter parameters (d0-d5)
    vdup.8          d1, d14[4]
    vdup.8          d2, d15[0]
    vdup.8          d3, d15[4]
    vdup.8          d4, d16[0]
    vdup.8          d5, d16[4]

    vmull.u8        q3, d27, d0             ;(src_ptr[-2] * vp8_filter[0])
    vmull.u8        q4, d28, d0

    vmull.u8        q5, d25, d5             ;(src_ptr[3] * vp8_filter[5])
    vmull.u8        q6, d26, d5

    vmlsl.u8        q3, d29, d4             ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q4, d30, d4

    vmlsl.u8        q5, d23, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q6, d24, d1

    vmlal.u8        q3, d28, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q4, d29, d2

    vmlal.u8        q5, d24, d3             ;(src_ptr[1] * vp8_filter[3])
    vmlal.u8        q6, d25, d3

    add             r0, r4, lr
    add             r1, r0, lr
    add             r2, r1, lr

    vqadd.s16       q5, q3                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q6, q4

    vqrshrun.s16    d3, q5, #7              ;shift/round/saturate to u8
    vqrshrun.s16    d4, q6, #7

    vst1.32         {d3[0]}, [r4]           ;store result
    vst1.32         {d3[1]}, [r0]
    vst1.32         {d4[0]}, [r1]
    vst1.32         {d4[1]}, [r2]

    pop             {r4, pc}


;---------------------
firstpass_filter4x4_only
    vabs.s32        q12, q14                ;get abs(filer_parameters)
    vabs.s32        q13, q15

    sub             r0, r0, #2              ;go back 2 columns of src data

;First pass: output_height lines x output_width columns (4x4)
    vld1.u8         {q3}, [r0], r1          ;load first 4-line src data
    vdup.8          d0, d24[0]              ;first_pass filter (d0-d5)
    vld1.u8         {q4}, [r0], r1
    vdup.8          d1, d24[4]
    vld1.u8         {q5}, [r0], r1
    vdup.8          d2, d25[0]
    vld1.u8         {q6}, [r0], r1

    vdup.8          d3, d25[4]
    vdup.8          d4, d26[0]
    vdup.8          d5, d26[4]

    vext.8          d18, d6, d7, #5         ;construct src_ptr[3]
    vext.8          d19, d8, d9, #5
    vext.8          d20, d10, d11, #5
    vext.8          d21, d12, d13, #5

    vswp            d7, d8                  ;discard 2nd half data after src_ptr[3] is done
    vswp            d11, d12

    vzip.32         d18, d19                ;put 2-line data in 1 register (src_ptr[3])
    vzip.32         d20, d21
    vmull.u8        q7, d18, d5             ;(src_ptr[3] * vp8_filter[5])
    vmull.u8        q8, d20, d5

    vmov            q4, q3                  ;keep original src data in q4 q6
    vmov            q6, q5

    vzip.32         d6, d7                  ;construct src_ptr[-2], and put 2-line data together
    vzip.32         d10, d11
    vshr.u64        q9, q4, #8              ;construct src_ptr[-1]
    vshr.u64        q10, q6, #8
    vmlal.u8        q7, d6, d0              ;+(src_ptr[-2] * vp8_filter[0])
    vmlal.u8        q8, d10, d0

    vzip.32         d18, d19                ;put 2-line data in 1 register (src_ptr[-1])
    vzip.32         d20, d21
    vshr.u64        q3, q4, #32             ;construct src_ptr[2]
    vshr.u64        q5, q6, #32
    vmlsl.u8        q7, d18, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q8, d20, d1

    vzip.32         d6, d7                  ;put 2-line data in 1 register (src_ptr[2])
    vzip.32         d10, d11
    vshr.u64        q9, q4, #16             ;construct src_ptr[0]
    vshr.u64        q10, q6, #16
    vmlsl.u8        q7, d6, d4              ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q8, d10, d4

    vzip.32         d18, d19                ;put 2-line data in 1 register (src_ptr[0])
    vzip.32         d20, d21
    vshr.u64        q3, q4, #24             ;construct src_ptr[1]
    vshr.u64        q5, q6, #24
    vmlal.u8        q7, d18, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q8, d20, d2

    vzip.32         d6, d7                  ;put 2-line data in 1 register (src_ptr[1])
    vzip.32         d10, d11
    vmull.u8        q9, d6, d3              ;(src_ptr[1] * vp8_filter[3])
    vmull.u8        q10, d10, d3

    add             r0, r4, lr
    add             r1, r0, lr
    add             r2, r1, lr

    vqadd.s16       q7, q9                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q8, q10

    vqrshrun.s16    d27, q7, #7             ;shift/round/saturate to u8
    vqrshrun.s16    d28, q8, #7

    vst1.32         {d27[0]}, [r4]          ;store result
    vst1.32         {d27[1]}, [r0]
    vst1.32         {d28[0]}, [r1]
    vst1.32         {d28[1]}, [r2]

    pop             {r4, pc}


;---------------------
secondpass_filter4x4_only
    sub             r0, r0, r1, lsl #1
    add             r3, r12, r3, lsl #5

    vld1.32         {d27[0]}, [r0], r1      ;load src data
    vld1.s32        {q5, q6}, [r3]          ;load second_pass filter
    vld1.32         {d27[1]}, [r0], r1
    vabs.s32        q7, q5
    vld1.32         {d28[0]}, [r0], r1
    vabs.s32        q8, q6
    vld1.32         {d28[1]}, [r0], r1
    vdup.8          d0, d14[0]              ;second_pass filter parameters (d0-d5)
    vld1.32         {d29[0]}, [r0], r1
    vdup.8          d1, d14[4]
    vld1.32         {d29[1]}, [r0], r1
    vdup.8          d2, d15[0]
    vld1.32         {d30[0]}, [r0], r1
    vdup.8          d3, d15[4]
    vld1.32         {d30[1]}, [r0], r1
    vdup.8          d4, d16[0]
    vld1.32         {d31[0]}, [r0], r1
    vdup.8          d5, d16[4]

    vext.8          d23, d27, d28, #4
    vext.8          d24, d28, d29, #4
    vext.8          d25, d29, d30, #4
    vext.8          d26, d30, d31, #4

    vmull.u8        q3, d27, d0             ;(src_ptr[-2] * vp8_filter[0])
    vmull.u8        q4, d28, d0

    vmull.u8        q5, d25, d5             ;(src_ptr[3] * vp8_filter[5])
    vmull.u8        q6, d26, d5

    vmlsl.u8        q3, d29, d4             ;-(src_ptr[2] * vp8_filter[4])
    vmlsl.u8        q4, d30, d4

    vmlsl.u8        q5, d23, d1             ;-(src_ptr[-1] * vp8_filter[1])
    vmlsl.u8        q6, d24, d1

    vmlal.u8        q3, d28, d2             ;(src_ptr[0] * vp8_filter[2])
    vmlal.u8        q4, d29, d2

    vmlal.u8        q5, d24, d3             ;(src_ptr[1] * vp8_filter[3])
    vmlal.u8        q6, d25, d3

    add             r0, r4, lr
    add             r1, r0, lr
    add             r2, r1, lr

    vqadd.s16       q5, q3                  ;sum of all (src_data*filter_parameters)
    vqadd.s16       q6, q4

    vqrshrun.s16    d3, q5, #7              ;shift/round/saturate to u8
    vqrshrun.s16    d4, q6, #7

    vst1.32         {d3[0]}, [r4]           ;store result
    vst1.32         {d3[1]}, [r0]
    vst1.32         {d4[0]}, [r1]
    vst1.32         {d4[1]}, [r2]

    pop             {r4, pc}

    ENDP

;-----------------

    END
