;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_loop_filter_horizontal_edge_y_neon|
    EXPORT  |vp8_loop_filter_horizontal_edge_uv_neon|
    EXPORT  |vp8_loop_filter_vertical_edge_y_neon|
    EXPORT  |vp8_loop_filter_vertical_edge_uv_neon|
    ARM

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char *src
; r1    int pitch
; r2    unsigned char blimit
; r3    unsigned char limit
; sp    unsigned char thresh,
|vp8_loop_filter_horizontal_edge_y_neon| PROC
    push        {lr}
    vdup.u8     q0, r2                     ; duplicate blimit
    vdup.u8     q1, r3                     ; duplicate limit
    sub         r2, r0, r1, lsl #2         ; move src pointer down by 4 lines
    ldr         r3, [sp, #4]               ; load thresh
    add         r12, r2, r1
    add         r1, r1, r1

    vdup.u8     q2, r3                     ; duplicate thresh

    vld1.u8     {q3}, [r2@128], r1              ; p3
    vld1.u8     {q4}, [r12@128], r1             ; p2
    vld1.u8     {q5}, [r2@128], r1              ; p1
    vld1.u8     {q6}, [r12@128], r1             ; p0
    vld1.u8     {q7}, [r2@128], r1              ; q0
    vld1.u8     {q8}, [r12@128], r1             ; q1
    vld1.u8     {q9}, [r2@128]                  ; q2
    vld1.u8     {q10}, [r12@128]                ; q3

    sub         r2, r2, r1, lsl #1
    sub         r12, r12, r1, lsl #1

    bl          vp8_loop_filter_neon

    vst1.u8     {q5}, [r2@128], r1              ; store op1
    vst1.u8     {q6}, [r12@128], r1             ; store op0
    vst1.u8     {q7}, [r2@128], r1              ; store oq0
    vst1.u8     {q8}, [r12@128], r1             ; store oq1

    pop         {pc}
    ENDP        ; |vp8_loop_filter_horizontal_edge_y_neon|


; r0    unsigned char *u,
; r1    int pitch,
; r2    unsigned char blimit
; r3    unsigned char limit
; sp    unsigned char thresh,
; sp+4  unsigned char *v
|vp8_loop_filter_horizontal_edge_uv_neon| PROC
    push        {lr}
    vdup.u8     q0, r2                      ; duplicate blimit
    vdup.u8     q1, r3                      ; duplicate limit
    ldr         r12, [sp, #4]               ; load thresh
    ldr         r2, [sp, #8]                ; load v ptr
    vdup.u8     q2, r12                     ; duplicate thresh

    sub         r3, r0, r1, lsl #2          ; move u pointer down by 4 lines
    sub         r12, r2, r1, lsl #2         ; move v pointer down by 4 lines

    vld1.u8     {d6}, [r3@64], r1              ; p3
    vld1.u8     {d7}, [r12@64], r1             ; p3
    vld1.u8     {d8}, [r3@64], r1              ; p2
    vld1.u8     {d9}, [r12@64], r1             ; p2
    vld1.u8     {d10}, [r3@64], r1             ; p1
    vld1.u8     {d11}, [r12@64], r1            ; p1
    vld1.u8     {d12}, [r3@64], r1             ; p0
    vld1.u8     {d13}, [r12@64], r1            ; p0
    vld1.u8     {d14}, [r3@64], r1             ; q0
    vld1.u8     {d15}, [r12@64], r1            ; q0
    vld1.u8     {d16}, [r3@64], r1             ; q1
    vld1.u8     {d17}, [r12@64], r1            ; q1
    vld1.u8     {d18}, [r3@64], r1             ; q2
    vld1.u8     {d19}, [r12@64], r1            ; q2
    vld1.u8     {d20}, [r3@64]                 ; q3
    vld1.u8     {d21}, [r12@64]                ; q3

    bl          vp8_loop_filter_neon

    sub         r0, r0, r1, lsl #1
    sub         r2, r2, r1, lsl #1

    vst1.u8     {d10}, [r0@64], r1             ; store u op1
    vst1.u8     {d11}, [r2@64], r1             ; store v op1
    vst1.u8     {d12}, [r0@64], r1             ; store u op0
    vst1.u8     {d13}, [r2@64], r1             ; store v op0
    vst1.u8     {d14}, [r0@64], r1             ; store u oq0
    vst1.u8     {d15}, [r2@64], r1             ; store v oq0
    vst1.u8     {d16}, [r0@64]                 ; store u oq1
    vst1.u8     {d17}, [r2@64]                 ; store v oq1

    pop         {pc}
    ENDP        ; |vp8_loop_filter_horizontal_edge_uv_neon|

; void vp8_loop_filter_vertical_edge_y_neon(unsigned char *src, int pitch,
;                                           const signed char *flimit,
;                                           const signed char *limit,
;                                           const signed char *thresh,
;                                           int count)
; r0    unsigned char *src
; r1    int pitch
; r2    unsigned char blimit
; r3    unsigned char limit
; sp    unsigned char thresh,

|vp8_loop_filter_vertical_edge_y_neon| PROC
    push        {lr}
    vdup.u8     q0, r2                     ; duplicate blimit
    vdup.u8     q1, r3                     ; duplicate limit
    sub         r2, r0, #4                 ; src ptr down by 4 columns
    add         r1, r1, r1
    ldr         r3, [sp, #4]               ; load thresh
    add         r12, r2, r1, asr #1

    vld1.u8     {d6}, [r2], r1
    vld1.u8     {d8}, [r12], r1
    vld1.u8     {d10}, [r2], r1
    vld1.u8     {d12}, [r12], r1
    vld1.u8     {d14}, [r2], r1
    vld1.u8     {d16}, [r12], r1
    vld1.u8     {d18}, [r2], r1
    vld1.u8     {d20}, [r12], r1

    vld1.u8     {d7}, [r2], r1              ; load second 8-line src data
    vld1.u8     {d9}, [r12], r1
    vld1.u8     {d11}, [r2], r1
    vld1.u8     {d13}, [r12], r1
    vld1.u8     {d15}, [r2], r1
    vld1.u8     {d17}, [r12], r1
    vld1.u8     {d19}, [r2]
    vld1.u8     {d21}, [r12]

    ;transpose to 8x16 matrix
    vtrn.32     q3, q7
    vtrn.32     q4, q8
    vtrn.32     q5, q9
    vtrn.32     q6, q10

    vdup.u8     q2, r3                     ; duplicate thresh

    vtrn.16     q3, q5
    vtrn.16     q4, q6
    vtrn.16     q7, q9
    vtrn.16     q8, q10

    vtrn.8      q3, q4
    vtrn.8      q5, q6
    vtrn.8      q7, q8
    vtrn.8      q9, q10

    bl          vp8_loop_filter_neon

    vswp        d12, d11
    vswp        d16, d13

    sub         r0, r0, #2                 ; dst ptr

    vswp        d14, d12
    vswp        d16, d15

    add         r12, r0, r1, asr #1

    ;store op1, op0, oq0, oq1
    vst4.8      {d10[0], d11[0], d12[0], d13[0]}, [r0], r1
    vst4.8      {d10[1], d11[1], d12[1], d13[1]}, [r12], r1
    vst4.8      {d10[2], d11[2], d12[2], d13[2]}, [r0], r1
    vst4.8      {d10[3], d11[3], d12[3], d13[3]}, [r12], r1
    vst4.8      {d10[4], d11[4], d12[4], d13[4]}, [r0], r1
    vst4.8      {d10[5], d11[5], d12[5], d13[5]}, [r12], r1
    vst4.8      {d10[6], d11[6], d12[6], d13[6]}, [r0], r1
    vst4.8      {d10[7], d11[7], d12[7], d13[7]}, [r12], r1

    vst4.8      {d14[0], d15[0], d16[0], d17[0]}, [r0], r1
    vst4.8      {d14[1], d15[1], d16[1], d17[1]}, [r12], r1
    vst4.8      {d14[2], d15[2], d16[2], d17[2]}, [r0], r1
    vst4.8      {d14[3], d15[3], d16[3], d17[3]}, [r12], r1
    vst4.8      {d14[4], d15[4], d16[4], d17[4]}, [r0], r1
    vst4.8      {d14[5], d15[5], d16[5], d17[5]}, [r12], r1
    vst4.8      {d14[6], d15[6], d16[6], d17[6]}, [r0]
    vst4.8      {d14[7], d15[7], d16[7], d17[7]}, [r12]

    pop         {pc}
    ENDP        ; |vp8_loop_filter_vertical_edge_y_neon|

; void vp8_loop_filter_vertical_edge_uv_neon(unsigned char *u, int pitch
;                                            const signed char *flimit,
;                                            const signed char *limit,
;                                            const signed char *thresh,
;                                            unsigned char *v)
; r0    unsigned char *u,
; r1    int pitch,
; r2    unsigned char blimit
; r3    unsigned char limit
; sp    unsigned char thresh,
; sp+4  unsigned char *v
|vp8_loop_filter_vertical_edge_uv_neon| PROC
    push        {lr}
    vdup.u8     q0, r2                      ; duplicate blimit
    sub         r12, r0, #4                 ; move u pointer down by 4 columns
    ldr         r2, [sp, #8]                ; load v ptr
    vdup.u8     q1, r3                      ; duplicate limit
    sub         r3, r2, #4                  ; move v pointer down by 4 columns

    vld1.u8     {d6}, [r12], r1             ;load u data
    vld1.u8     {d7}, [r3], r1              ;load v data
    vld1.u8     {d8}, [r12], r1
    vld1.u8     {d9}, [r3], r1
    vld1.u8     {d10}, [r12], r1
    vld1.u8     {d11}, [r3], r1
    vld1.u8     {d12}, [r12], r1
    vld1.u8     {d13}, [r3], r1
    vld1.u8     {d14}, [r12], r1
    vld1.u8     {d15}, [r3], r1
    vld1.u8     {d16}, [r12], r1
    vld1.u8     {d17}, [r3], r1
    vld1.u8     {d18}, [r12], r1
    vld1.u8     {d19}, [r3], r1
    vld1.u8     {d20}, [r12]
    vld1.u8     {d21}, [r3]

    ldr        r12, [sp, #4]               ; load thresh

    ;transpose to 8x16 matrix
    vtrn.32     q3, q7
    vtrn.32     q4, q8
    vtrn.32     q5, q9
    vtrn.32     q6, q10

    vdup.u8     q2, r12                     ; duplicate thresh

    vtrn.16     q3, q5
    vtrn.16     q4, q6
    vtrn.16     q7, q9
    vtrn.16     q8, q10

    vtrn.8      q3, q4
    vtrn.8      q5, q6
    vtrn.8      q7, q8
    vtrn.8      q9, q10

    bl          vp8_loop_filter_neon

    vswp        d12, d11
    vswp        d16, d13
    vswp        d14, d12
    vswp        d16, d15

    sub         r0, r0, #2
    sub         r2, r2, #2

    ;store op1, op0, oq0, oq1
    vst4.8      {d10[0], d11[0], d12[0], d13[0]}, [r0], r1
    vst4.8      {d14[0], d15[0], d16[0], d17[0]}, [r2], r1
    vst4.8      {d10[1], d11[1], d12[1], d13[1]}, [r0], r1
    vst4.8      {d14[1], d15[1], d16[1], d17[1]}, [r2], r1
    vst4.8      {d10[2], d11[2], d12[2], d13[2]}, [r0], r1
    vst4.8      {d14[2], d15[2], d16[2], d17[2]}, [r2], r1
    vst4.8      {d10[3], d11[3], d12[3], d13[3]}, [r0], r1
    vst4.8      {d14[3], d15[3], d16[3], d17[3]}, [r2], r1
    vst4.8      {d10[4], d11[4], d12[4], d13[4]}, [r0], r1
    vst4.8      {d14[4], d15[4], d16[4], d17[4]}, [r2], r1
    vst4.8      {d10[5], d11[5], d12[5], d13[5]}, [r0], r1
    vst4.8      {d14[5], d15[5], d16[5], d17[5]}, [r2], r1
    vst4.8      {d10[6], d11[6], d12[6], d13[6]}, [r0], r1
    vst4.8      {d14[6], d15[6], d16[6], d17[6]}, [r2], r1
    vst4.8      {d10[7], d11[7], d12[7], d13[7]}, [r0]
    vst4.8      {d14[7], d15[7], d16[7], d17[7]}, [r2]

    pop         {pc}
    ENDP        ; |vp8_loop_filter_vertical_edge_uv_neon|

; void vp8_loop_filter_neon();
; This is a helper function for the loopfilters. The invidual functions do the
; necessary load, transpose (if necessary) and store.

; r0-r3 PRESERVE
; q0    flimit
; q1    limit
; q2    thresh
; q3    p3
; q4    p2
; q5    p1
; q6    p0
; q7    q0
; q8    q1
; q9    q2
; q10   q3
|vp8_loop_filter_neon| PROC

    ; vp8_filter_mask
    vabd.u8     q11, q3, q4                 ; abs(p3 - p2)
    vabd.u8     q12, q4, q5                 ; abs(p2 - p1)
    vabd.u8     q13, q5, q6                 ; abs(p1 - p0)
    vabd.u8     q14, q8, q7                 ; abs(q1 - q0)
    vabd.u8     q3, q9, q8                  ; abs(q2 - q1)
    vabd.u8     q4, q10, q9                 ; abs(q3 - q2)

    vmax.u8     q11, q11, q12
    vmax.u8     q12, q13, q14
    vmax.u8     q3, q3, q4
    vmax.u8     q15, q11, q12

    vabd.u8     q9, q6, q7                  ; abs(p0 - q0)

    ; vp8_hevmask
    vcgt.u8     q13, q13, q2                ; (abs(p1 - p0) > thresh)*-1
    vcgt.u8     q14, q14, q2                ; (abs(q1 - q0) > thresh)*-1
    vmax.u8     q15, q15, q3

    vmov.u8     q10, #0x80                   ; 0x80

    vabd.u8     q2, q5, q8                  ; a = abs(p1 - q1)
    vqadd.u8    q9, q9, q9                  ; b = abs(p0 - q0) * 2

    vcge.u8     q15, q1, q15

    ; vp8_filter() function
    ; convert to signed
    veor        q7, q7, q10                 ; qs0
    vshr.u8     q2, q2, #1                  ; a = a / 2
    veor        q6, q6, q10                 ; ps0

    veor        q5, q5, q10                 ; ps1
    vqadd.u8    q9, q9, q2                  ; a = b + a

    veor        q8, q8, q10                 ; qs1

    vmov.u8     q10, #3                     ; #3

    vsubl.s8    q2, d14, d12                ; ( qs0 - ps0)
    vsubl.s8    q11, d15, d13

    vcge.u8     q9, q0, q9                  ; (a > flimit * 2 + limit) * -1

    vmovl.u8    q4, d20

    vqsub.s8    q1, q5, q8                  ; vp8_filter = clamp(ps1-qs1)
    vorr        q14, q13, q14               ; vp8_hevmask

    vmul.i16    q2, q2, q4                  ; 3 * ( qs0 - ps0)
    vmul.i16    q11, q11, q4

    vand        q1, q1, q14                 ; vp8_filter &= hev
    vand        q15, q15, q9                ; vp8_filter_mask

    vaddw.s8    q2, q2, d2
    vaddw.s8    q11, q11, d3

    vmov.u8     q9, #4                      ; #4

    ; vp8_filter = clamp(vp8_filter + 3 * ( qs0 - ps0))
    vqmovn.s16  d2, q2
    vqmovn.s16  d3, q11
    vand        q1, q1, q15                 ; vp8_filter &= mask

    vqadd.s8    q2, q1, q10                 ; Filter2 = clamp(vp8_filter+3)
    vqadd.s8    q1, q1, q9                  ; Filter1 = clamp(vp8_filter+4)
    vshr.s8     q2, q2, #3                  ; Filter2 >>= 3
    vshr.s8     q1, q1, #3                  ; Filter1 >>= 3


    vqadd.s8    q11, q6, q2                 ; u = clamp(ps0 + Filter2)
    vqsub.s8    q10, q7, q1                 ; u = clamp(qs0 - Filter1)

    ; outer tap adjustments: ++vp8_filter >> 1
    vrshr.s8    q1, q1, #1
    vbic        q1, q1, q14                 ; vp8_filter &= ~hev
    vmov.u8     q0, #0x80                   ; 0x80
    vqadd.s8    q13, q5, q1                 ; u = clamp(ps1 + vp8_filter)
    vqsub.s8    q12, q8, q1                 ; u = clamp(qs1 - vp8_filter)

    veor        q6, q11, q0                 ; *op0 = u^0x80
    veor        q7, q10, q0                 ; *oq0 = u^0x80
    veor        q5, q13, q0                 ; *op1 = u^0x80
    veor        q8, q12, q0                 ; *oq1 = u^0x80

    bx          lr
    ENDP        ; |vp8_loop_filter_horizontal_edge_y_neon|

;-----------------

    END
