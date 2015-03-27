;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_mbloop_filter_horizontal_edge_y_neon|
    EXPORT  |vp8_mbloop_filter_horizontal_edge_uv_neon|
    EXPORT  |vp8_mbloop_filter_vertical_edge_y_neon|
    EXPORT  |vp8_mbloop_filter_vertical_edge_uv_neon|
    ARM

    AREA ||.text||, CODE, READONLY, ALIGN=2

; void vp8_mbloop_filter_horizontal_edge_y_neon(unsigned char *src, int pitch,
;                                               const unsigned char *blimit,
;                                               const unsigned char *limit,
;                                               const unsigned char *thresh)
; r0    unsigned char *src,
; r1    int pitch,
; r2    unsigned char blimit
; r3    unsigned char limit
; sp    unsigned char thresh,
|vp8_mbloop_filter_horizontal_edge_y_neon| PROC
    push        {lr}
    add         r1, r1, r1                  ; double stride
    ldr         r12, [sp, #4]               ; load thresh
    sub         r0, r0, r1, lsl #1          ; move src pointer down by 4 lines
    vdup.u8     q2, r12                     ; thresh
    add         r12, r0, r1,  lsr #1        ; move src pointer up by 1 line

    vld1.u8     {q3}, [r0@128], r1              ; p3
    vld1.u8     {q4}, [r12@128], r1             ; p2
    vld1.u8     {q5}, [r0@128], r1              ; p1
    vld1.u8     {q6}, [r12@128], r1             ; p0
    vld1.u8     {q7}, [r0@128], r1              ; q0
    vld1.u8     {q8}, [r12@128], r1             ; q1
    vld1.u8     {q9}, [r0@128], r1              ; q2
    vld1.u8     {q10}, [r12@128], r1            ; q3

    bl          vp8_mbloop_filter_neon

    sub         r12, r12, r1, lsl #2
    add         r0, r12, r1, lsr #1

    vst1.u8     {q4}, [r12@128],r1         ; store op2
    vst1.u8     {q5}, [r0@128],r1          ; store op1
    vst1.u8     {q6}, [r12@128], r1        ; store op0
    vst1.u8     {q7}, [r0@128],r1          ; store oq0
    vst1.u8     {q8}, [r12@128]            ; store oq1
    vst1.u8     {q9}, [r0@128]             ; store oq2

    pop         {pc}
    ENDP        ; |vp8_mbloop_filter_horizontal_edge_y_neon|

; void vp8_mbloop_filter_horizontal_edge_uv_neon(unsigned char *u, int pitch,
;                                                const unsigned char *blimit,
;                                                const unsigned char *limit,
;                                                const unsigned char *thresh,
;                                                unsigned char *v)
; r0    unsigned char *u,
; r1    int pitch,
; r2    unsigned char blimit
; r3    unsigned char limit
; sp    unsigned char thresh,
; sp+4  unsigned char *v

|vp8_mbloop_filter_horizontal_edge_uv_neon| PROC
    push        {lr}
    ldr         r12, [sp, #4]                 ; load thresh
    sub         r0, r0, r1, lsl #2            ; move u pointer down by 4 lines
    vdup.u8     q2, r12                       ; thresh
    ldr         r12, [sp, #8]                 ; load v ptr
    sub         r12, r12, r1, lsl #2          ; move v pointer down by 4 lines

    vld1.u8     {d6}, [r0@64], r1              ; p3
    vld1.u8     {d7}, [r12@64], r1              ; p3
    vld1.u8     {d8}, [r0@64], r1              ; p2
    vld1.u8     {d9}, [r12@64], r1              ; p2
    vld1.u8     {d10}, [r0@64], r1             ; p1
    vld1.u8     {d11}, [r12@64], r1             ; p1
    vld1.u8     {d12}, [r0@64], r1             ; p0
    vld1.u8     {d13}, [r12@64], r1             ; p0
    vld1.u8     {d14}, [r0@64], r1             ; q0
    vld1.u8     {d15}, [r12@64], r1             ; q0
    vld1.u8     {d16}, [r0@64], r1             ; q1
    vld1.u8     {d17}, [r12@64], r1             ; q1
    vld1.u8     {d18}, [r0@64], r1             ; q2
    vld1.u8     {d19}, [r12@64], r1             ; q2
    vld1.u8     {d20}, [r0@64], r1             ; q3
    vld1.u8     {d21}, [r12@64], r1             ; q3

    bl          vp8_mbloop_filter_neon

    sub         r0, r0, r1, lsl #3
    sub         r12, r12, r1, lsl #3

    add         r0, r0, r1
    add         r12, r12, r1

    vst1.u8     {d8}, [r0@64], r1              ; store u op2
    vst1.u8     {d9}, [r12@64], r1              ; store v op2
    vst1.u8     {d10}, [r0@64], r1             ; store u op1
    vst1.u8     {d11}, [r12@64], r1             ; store v op1
    vst1.u8     {d12}, [r0@64], r1             ; store u op0
    vst1.u8     {d13}, [r12@64], r1             ; store v op0
    vst1.u8     {d14}, [r0@64], r1             ; store u oq0
    vst1.u8     {d15}, [r12@64], r1             ; store v oq0
    vst1.u8     {d16}, [r0@64], r1             ; store u oq1
    vst1.u8     {d17}, [r12@64], r1             ; store v oq1
    vst1.u8     {d18}, [r0@64], r1             ; store u oq2
    vst1.u8     {d19}, [r12@64], r1             ; store v oq2

    pop         {pc}
    ENDP        ; |vp8_mbloop_filter_horizontal_edge_uv_neon|

; void vp8_mbloop_filter_vertical_edge_y_neon(unsigned char *src, int pitch,
;                                             const unsigned char *blimit,
;                                             const unsigned char *limit,
;                                             const unsigned char *thresh)
; r0    unsigned char *src,
; r1    int pitch,
; r2    unsigned char blimit
; r3    unsigned char limit
; sp    unsigned char thresh,
|vp8_mbloop_filter_vertical_edge_y_neon| PROC
    push        {lr}
    ldr         r12, [sp, #4]               ; load thresh
    sub         r0, r0, #4                  ; move src pointer down by 4 columns
    vdup.s8     q2, r12                     ; thresh
    add         r12, r0, r1, lsl #3         ; move src pointer down by 8 lines

    vld1.u8     {d6}, [r0], r1              ; load first 8-line src data
    vld1.u8     {d7}, [r12], r1             ; load second 8-line src data
    vld1.u8     {d8}, [r0], r1
    vld1.u8     {d9}, [r12], r1
    vld1.u8     {d10}, [r0], r1
    vld1.u8     {d11}, [r12], r1
    vld1.u8     {d12}, [r0], r1
    vld1.u8     {d13}, [r12], r1
    vld1.u8     {d14}, [r0], r1
    vld1.u8     {d15}, [r12], r1
    vld1.u8     {d16}, [r0], r1
    vld1.u8     {d17}, [r12], r1
    vld1.u8     {d18}, [r0], r1
    vld1.u8     {d19}, [r12], r1
    vld1.u8     {d20}, [r0], r1
    vld1.u8     {d21}, [r12], r1

    ;transpose to 8x16 matrix
    vtrn.32     q3, q7
    vtrn.32     q4, q8
    vtrn.32     q5, q9
    vtrn.32     q6, q10

    vtrn.16     q3, q5
    vtrn.16     q4, q6
    vtrn.16     q7, q9
    vtrn.16     q8, q10

    vtrn.8      q3, q4
    vtrn.8      q5, q6
    vtrn.8      q7, q8
    vtrn.8      q9, q10

    sub         r0, r0, r1, lsl #3

    bl          vp8_mbloop_filter_neon

    sub         r12, r12, r1, lsl #3

    ;transpose to 16x8 matrix
    vtrn.32     q3, q7
    vtrn.32     q4, q8
    vtrn.32     q5, q9
    vtrn.32     q6, q10

    vtrn.16     q3, q5
    vtrn.16     q4, q6
    vtrn.16     q7, q9
    vtrn.16     q8, q10

    vtrn.8      q3, q4
    vtrn.8      q5, q6
    vtrn.8      q7, q8
    vtrn.8      q9, q10

    ;store op2, op1, op0, oq0, oq1, oq2
    vst1.8      {d6}, [r0], r1
    vst1.8      {d7}, [r12], r1
    vst1.8      {d8}, [r0], r1
    vst1.8      {d9}, [r12], r1
    vst1.8      {d10}, [r0], r1
    vst1.8      {d11}, [r12], r1
    vst1.8      {d12}, [r0], r1
    vst1.8      {d13}, [r12], r1
    vst1.8      {d14}, [r0], r1
    vst1.8      {d15}, [r12], r1
    vst1.8      {d16}, [r0], r1
    vst1.8      {d17}, [r12], r1
    vst1.8      {d18}, [r0], r1
    vst1.8      {d19}, [r12], r1
    vst1.8      {d20}, [r0]
    vst1.8      {d21}, [r12]

    pop         {pc}
    ENDP        ; |vp8_mbloop_filter_vertical_edge_y_neon|

; void vp8_mbloop_filter_vertical_edge_uv_neon(unsigned char *u, int pitch,
;                                              const unsigned char *blimit,
;                                              const unsigned char *limit,
;                                              const unsigned char *thresh,
;                                              unsigned char *v)
; r0    unsigned char *u,
; r1    int pitch,
; r2    const signed char *flimit,
; r3    const signed char *limit,
; sp    const signed char *thresh,
; sp+4  unsigned char *v
|vp8_mbloop_filter_vertical_edge_uv_neon| PROC
    push        {lr}
    ldr         r12, [sp, #4]               ; load thresh
    sub         r0, r0, #4                  ; move u pointer down by 4 columns
    vdup.u8     q2, r12                     ; thresh
    ldr         r12, [sp, #8]               ; load v ptr
    sub         r12, r12, #4                ; move v pointer down by 4 columns

    vld1.u8     {d6}, [r0], r1              ;load u data
    vld1.u8     {d7}, [r12], r1             ;load v data
    vld1.u8     {d8}, [r0], r1
    vld1.u8     {d9}, [r12], r1
    vld1.u8     {d10}, [r0], r1
    vld1.u8     {d11}, [r12], r1
    vld1.u8     {d12}, [r0], r1
    vld1.u8     {d13}, [r12], r1
    vld1.u8     {d14}, [r0], r1
    vld1.u8     {d15}, [r12], r1
    vld1.u8     {d16}, [r0], r1
    vld1.u8     {d17}, [r12], r1
    vld1.u8     {d18}, [r0], r1
    vld1.u8     {d19}, [r12], r1
    vld1.u8     {d20}, [r0], r1
    vld1.u8     {d21}, [r12], r1

    ;transpose to 8x16 matrix
    vtrn.32     q3, q7
    vtrn.32     q4, q8
    vtrn.32     q5, q9
    vtrn.32     q6, q10

    vtrn.16     q3, q5
    vtrn.16     q4, q6
    vtrn.16     q7, q9
    vtrn.16     q8, q10

    vtrn.8      q3, q4
    vtrn.8      q5, q6
    vtrn.8      q7, q8
    vtrn.8      q9, q10

    sub         r0, r0, r1, lsl #3

    bl          vp8_mbloop_filter_neon

    sub         r12, r12, r1, lsl #3

    ;transpose to 16x8 matrix
    vtrn.32     q3, q7
    vtrn.32     q4, q8
    vtrn.32     q5, q9
    vtrn.32     q6, q10

    vtrn.16     q3, q5
    vtrn.16     q4, q6
    vtrn.16     q7, q9
    vtrn.16     q8, q10

    vtrn.8      q3, q4
    vtrn.8      q5, q6
    vtrn.8      q7, q8
    vtrn.8      q9, q10

    ;store op2, op1, op0, oq0, oq1, oq2
    vst1.8      {d6}, [r0], r1
    vst1.8      {d7}, [r12], r1
    vst1.8      {d8}, [r0], r1
    vst1.8      {d9}, [r12], r1
    vst1.8      {d10}, [r0], r1
    vst1.8      {d11}, [r12], r1
    vst1.8      {d12}, [r0], r1
    vst1.8      {d13}, [r12], r1
    vst1.8      {d14}, [r0], r1
    vst1.8      {d15}, [r12], r1
    vst1.8      {d16}, [r0], r1
    vst1.8      {d17}, [r12], r1
    vst1.8      {d18}, [r0], r1
    vst1.8      {d19}, [r12], r1
    vst1.8      {d20}, [r0]
    vst1.8      {d21}, [r12]

    pop         {pc}
    ENDP        ; |vp8_mbloop_filter_vertical_edge_uv_neon|

; void vp8_mbloop_filter_neon()
; This is a helper function for the macroblock loopfilters. The individual
; functions do the necessary load, transpose (if necessary), preserve (if
; necessary) and store.

; r0,r1 PRESERVE
; r2    mblimit
; r3    limit

; q2    thresh
; q3    p3 PRESERVE
; q4    p2
; q5    p1
; q6    p0
; q7    q0
; q8    q1
; q9    q2
; q10   q3 PRESERVE

|vp8_mbloop_filter_neon| PROC

    ; vp8_filter_mask
    vabd.u8     q11, q3, q4                 ; abs(p3 - p2)
    vabd.u8     q12, q4, q5                 ; abs(p2 - p1)
    vabd.u8     q13, q5, q6                 ; abs(p1 - p0)
    vabd.u8     q14, q8, q7                 ; abs(q1 - q0)
    vabd.u8     q1, q9, q8                  ; abs(q2 - q1)
    vabd.u8     q0, q10, q9                 ; abs(q3 - q2)

    vmax.u8     q11, q11, q12
    vmax.u8     q12, q13, q14
    vmax.u8     q1, q1, q0
    vmax.u8     q15, q11, q12

    vabd.u8     q12, q6, q7                 ; abs(p0 - q0)

    ; vp8_hevmask
    vcgt.u8     q13, q13, q2                ; (abs(p1 - p0) > thresh) * -1
    vcgt.u8     q14, q14, q2                ; (abs(q1 - q0) > thresh) * -1
    vmax.u8     q15, q15, q1

    vdup.u8     q1, r3                      ; limit
    vdup.u8     q2, r2                      ; mblimit

    vmov.u8     q0, #0x80                   ; 0x80

    vcge.u8     q15, q1, q15

    vabd.u8     q1, q5, q8                  ; a = abs(p1 - q1)
    vqadd.u8    q12, q12, q12               ; b = abs(p0 - q0) * 2
    vmov.u16    q11, #3                     ; #3

    ; vp8_filter
    ; convert to signed
    veor        q7, q7, q0                  ; qs0
    vshr.u8     q1, q1, #1                  ; a = a / 2
    veor        q6, q6, q0                  ; ps0
    veor        q5, q5, q0                  ; ps1

    vqadd.u8    q12, q12, q1                ; a = b + a

    veor        q8, q8, q0                  ; qs1
    veor        q4, q4, q0                  ; ps2
    veor        q9, q9, q0                  ; qs2

    vorr        q14, q13, q14               ; vp8_hevmask

    vcge.u8     q12, q2, q12                ; (a > flimit * 2 + limit) * -1

    vsubl.s8    q2, d14, d12                ; qs0 - ps0
    vsubl.s8    q13, d15, d13

    vqsub.s8    q1, q5, q8                  ; vp8_filter = clamp(ps1-qs1)

    vmul.i16    q2, q2, q11                 ; 3 * ( qs0 - ps0)

    vand        q15, q15, q12               ; vp8_filter_mask

    vmul.i16    q13, q13, q11

    vmov.u8     q12, #3                     ; #3

    vaddw.s8    q2, q2, d2                  ; vp8_filter + 3 * ( qs0 - ps0)
    vaddw.s8    q13, q13, d3

    vmov.u8     q11, #4                     ; #4

    ; vp8_filter = clamp(vp8_filter + 3 * ( qs0 - ps0))
    vqmovn.s16  d2, q2
    vqmovn.s16  d3, q13

    vand        q1, q1, q15                 ; vp8_filter &= mask

    vmov.u16    q15, #63                    ; #63

    vand        q13, q1, q14                ; Filter2 &= hev

    vqadd.s8    q2, q13, q11                ; Filter1 = clamp(Filter2+4)
    vqadd.s8    q13, q13, q12               ; Filter2 = clamp(Filter2+3)

    vmov        q0, q15

    vshr.s8     q2, q2, #3                  ; Filter1 >>= 3
    vshr.s8     q13, q13, #3                ; Filter2 >>= 3

    vmov        q11, q15
    vmov        q12, q15

    vqsub.s8    q7, q7, q2                  ; qs0 = clamp(qs0 - Filter1)

    vqadd.s8    q6, q6, q13                 ; ps0 = clamp(ps0 + Filter2)

    vbic        q1, q1, q14                 ; vp8_filter &= ~hev

    ; roughly 1/7th difference across boundary
    ; roughly 2/7th difference across boundary
    ; roughly 3/7th difference across boundary

    vmov.u8     d5, #9                      ; #9
    vmov.u8     d4, #18                     ; #18

    vmov        q13, q15
    vmov        q14, q15

    vmlal.s8    q0, d2, d5                  ; 63 + Filter2 * 9
    vmlal.s8    q11, d3, d5
    vmov.u8     d5, #27                     ; #27
    vmlal.s8    q12, d2, d4                 ; 63 + Filter2 * 18
    vmlal.s8    q13, d3, d4
    vmlal.s8    q14, d2, d5                 ; 63 + Filter2 * 27
    vmlal.s8    q15, d3, d5

    vqshrn.s16  d0, q0, #7                  ; u = clamp((63 + Filter2 * 9)>>7)
    vqshrn.s16  d1, q11, #7
    vqshrn.s16  d24, q12, #7                ; u = clamp((63 + Filter2 * 18)>>7)
    vqshrn.s16  d25, q13, #7
    vqshrn.s16  d28, q14, #7                ; u = clamp((63 + Filter2 * 27)>>7)
    vqshrn.s16  d29, q15, #7

    vmov.u8     q1, #0x80                   ; 0x80

    vqsub.s8    q11, q9, q0                 ; s = clamp(qs2 - u)
    vqadd.s8    q0, q4, q0                  ; s = clamp(ps2 + u)
    vqsub.s8    q13, q8, q12                ; s = clamp(qs1 - u)
    vqadd.s8    q12, q5, q12                ; s = clamp(ps1 + u)
    vqsub.s8    q15, q7, q14                ; s = clamp(qs0 - u)
    vqadd.s8    q14, q6, q14                ; s = clamp(ps0 + u)

    veor        q9, q11, q1                 ; *oq2 = s^0x80
    veor        q4, q0, q1                  ; *op2 = s^0x80
    veor        q8, q13, q1                 ; *oq1 = s^0x80
    veor        q5, q12, q1                 ; *op2 = s^0x80
    veor        q7, q15, q1                 ; *oq0 = s^0x80
    veor        q6, q14, q1                 ; *op0 = s^0x80

    bx          lr
    ENDP        ; |vp8_mbloop_filter_neon|

;-----------------

    END
