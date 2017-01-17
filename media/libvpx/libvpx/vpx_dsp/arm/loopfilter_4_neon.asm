;
;  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;

    EXPORT  |vpx_lpf_horizontal_4_neon|
    EXPORT  |vpx_lpf_vertical_4_neon|
    ARM

    AREA ||.text||, CODE, READONLY, ALIGN=2

; Currently vpx only works on iterations 8 at a time. The vp8 loop filter
; works on 16 iterations at a time.
;
; void vpx_lpf_horizontal_4_neon(uint8_t *s,
;                                int p /* pitch */,
;                                const uint8_t *blimit,
;                                const uint8_t *limit,
;                                const uint8_t *thresh)
;
; r0    uint8_t *s,
; r1    int p, /* pitch */
; r2    const uint8_t *blimit,
; r3    const uint8_t *limit,
; sp    const uint8_t *thresh,
|vpx_lpf_horizontal_4_neon| PROC
    push        {lr}

    vld1.8      {d0[]}, [r2]               ; duplicate *blimit
    ldr         r2, [sp, #4]               ; load thresh
    add         r1, r1, r1                 ; double pitch

    vld1.8      {d1[]}, [r3]               ; duplicate *limit
    vld1.8      {d2[]}, [r2]               ; duplicate *thresh

    sub         r2, r0, r1, lsl #1         ; move src pointer down by 4 lines
    add         r3, r2, r1, lsr #1         ; set to 3 lines down

    vld1.u8     {d3}, [r2@64], r1          ; p3
    vld1.u8     {d4}, [r3@64], r1          ; p2
    vld1.u8     {d5}, [r2@64], r1          ; p1
    vld1.u8     {d6}, [r3@64], r1          ; p0
    vld1.u8     {d7}, [r2@64], r1          ; q0
    vld1.u8     {d16}, [r3@64], r1         ; q1
    vld1.u8     {d17}, [r2@64]             ; q2
    vld1.u8     {d18}, [r3@64]             ; q3

    sub         r2, r2, r1, lsl #1
    sub         r3, r3, r1, lsl #1

    bl          vpx_loop_filter_neon

    vst1.u8     {d4}, [r2@64], r1          ; store op1
    vst1.u8     {d5}, [r3@64], r1          ; store op0
    vst1.u8     {d6}, [r2@64], r1          ; store oq0
    vst1.u8     {d7}, [r3@64], r1          ; store oq1

    pop         {pc}
    ENDP        ; |vpx_lpf_horizontal_4_neon|

; Currently vpx only works on iterations 8 at a time. The vp8 loop filter
; works on 16 iterations at a time.
;
; void vpx_lpf_vertical_4_neon(uint8_t *s,
;                              int p /* pitch */,
;                              const uint8_t *blimit,
;                              const uint8_t *limit,
;                              const uint8_t *thresh)
;
; r0    uint8_t *s,
; r1    int p, /* pitch */
; r2    const uint8_t *blimit,
; r3    const uint8_t *limit,
; sp    const uint8_t *thresh,
|vpx_lpf_vertical_4_neon| PROC
    push        {lr}

    vld1.8      {d0[]}, [r2]              ; duplicate *blimit
    vld1.8      {d1[]}, [r3]              ; duplicate *limit

    ldr         r3, [sp, #4]              ; load thresh
    sub         r2, r0, #4                ; move s pointer down by 4 columns

    vld1.8      {d2[]}, [r3]              ; duplicate *thresh

    vld1.u8     {d3}, [r2], r1             ; load s data
    vld1.u8     {d4}, [r2], r1
    vld1.u8     {d5}, [r2], r1
    vld1.u8     {d6}, [r2], r1
    vld1.u8     {d7}, [r2], r1
    vld1.u8     {d16}, [r2], r1
    vld1.u8     {d17}, [r2], r1
    vld1.u8     {d18}, [r2]

    ;transpose to 8x16 matrix
    vtrn.32     d3, d7
    vtrn.32     d4, d16
    vtrn.32     d5, d17
    vtrn.32     d6, d18

    vtrn.16     d3, d5
    vtrn.16     d4, d6
    vtrn.16     d7, d17
    vtrn.16     d16, d18

    vtrn.8      d3, d4
    vtrn.8      d5, d6
    vtrn.8      d7, d16
    vtrn.8      d17, d18

    bl          vpx_loop_filter_neon

    sub         r0, r0, #2

    ;store op1, op0, oq0, oq1
    vst4.8      {d4[0], d5[0], d6[0], d7[0]}, [r0], r1
    vst4.8      {d4[1], d5[1], d6[1], d7[1]}, [r0], r1
    vst4.8      {d4[2], d5[2], d6[2], d7[2]}, [r0], r1
    vst4.8      {d4[3], d5[3], d6[3], d7[3]}, [r0], r1
    vst4.8      {d4[4], d5[4], d6[4], d7[4]}, [r0], r1
    vst4.8      {d4[5], d5[5], d6[5], d7[5]}, [r0], r1
    vst4.8      {d4[6], d5[6], d6[6], d7[6]}, [r0], r1
    vst4.8      {d4[7], d5[7], d6[7], d7[7]}, [r0]

    pop         {pc}
    ENDP        ; |vpx_lpf_vertical_4_neon|

; void vpx_loop_filter_neon();
; This is a helper function for the loopfilters. The invidual functions do the
; necessary load, transpose (if necessary) and store. The function does not use
; registers d8-d15.
;
; Inputs:
; r0-r3, r12 PRESERVE
; d0    blimit
; d1    limit
; d2    thresh
; d3    p3
; d4    p2
; d5    p1
; d6    p0
; d7    q0
; d16   q1
; d17   q2
; d18   q3
;
; Outputs:
; d4    op1
; d5    op0
; d6    oq0
; d7    oq1
|vpx_loop_filter_neon| PROC
    ; filter_mask
    vabd.u8     d19, d3, d4                 ; m1 = abs(p3 - p2)
    vabd.u8     d20, d4, d5                 ; m2 = abs(p2 - p1)
    vabd.u8     d21, d5, d6                 ; m3 = abs(p1 - p0)
    vabd.u8     d22, d16, d7                ; m4 = abs(q1 - q0)
    vabd.u8     d3, d17, d16                ; m5 = abs(q2 - q1)
    vabd.u8     d4, d18, d17                ; m6 = abs(q3 - q2)

    ; only compare the largest value to limit
    vmax.u8     d19, d19, d20               ; m1 = max(m1, m2)
    vmax.u8     d20, d21, d22               ; m2 = max(m3, m4)

    vabd.u8     d17, d6, d7                 ; abs(p0 - q0)

    vmax.u8     d3, d3, d4                  ; m3 = max(m5, m6)

    vmov.u8     d18, #0x80

    vmax.u8     d23, d19, d20               ; m1 = max(m1, m2)

    ; hevmask
    vcgt.u8     d21, d21, d2                ; (abs(p1 - p0) > thresh)*-1
    vcgt.u8     d22, d22, d2                ; (abs(q1 - q0) > thresh)*-1
    vmax.u8     d23, d23, d3                ; m1 = max(m1, m3)

    vabd.u8     d28, d5, d16                ; a = abs(p1 - q1)
    vqadd.u8    d17, d17, d17               ; b = abs(p0 - q0) * 2

    veor        d7, d7, d18                 ; qs0

    vcge.u8     d23, d1, d23                ; abs(m1) > limit

    ; filter() function
    ; convert to signed

    vshr.u8     d28, d28, #1                ; a = a / 2
    veor        d6, d6, d18                 ; ps0

    veor        d5, d5, d18                 ; ps1
    vqadd.u8    d17, d17, d28               ; a = b + a

    veor        d16, d16, d18               ; qs1

    vmov.u8     d19, #3

    vsub.s8     d28, d7, d6                 ; ( qs0 - ps0)

    vcge.u8     d17, d0, d17                ; a > blimit

    vqsub.s8    d27, d5, d16                ; filter = clamp(ps1-qs1)
    vorr        d22, d21, d22               ; hevmask

    vmull.s8    q12, d28, d19               ; 3 * ( qs0 - ps0)

    vand        d27, d27, d22               ; filter &= hev
    vand        d23, d23, d17               ; filter_mask

    vaddw.s8    q12, q12, d27               ; filter + 3 * (qs0 - ps0)

    vmov.u8     d17, #4

    ; filter = clamp(filter + 3 * ( qs0 - ps0))
    vqmovn.s16  d27, q12

    vand        d27, d27, d23               ; filter &= mask

    vqadd.s8    d28, d27, d19               ; filter2 = clamp(filter+3)
    vqadd.s8    d27, d27, d17               ; filter1 = clamp(filter+4)
    vshr.s8     d28, d28, #3                ; filter2 >>= 3
    vshr.s8     d27, d27, #3                ; filter1 >>= 3

    vqadd.s8    d19, d6, d28                ; u = clamp(ps0 + filter2)
    vqsub.s8    d26, d7, d27                ; u = clamp(qs0 - filter1)

    ; outer tap adjustments
    vrshr.s8    d27, d27, #1                ; filter = ++filter1 >> 1

    veor        d6, d26, d18                ; *oq0 = u^0x80

    vbic        d27, d27, d22               ; filter &= ~hev

    vqadd.s8    d21, d5, d27                ; u = clamp(ps1 + filter)
    vqsub.s8    d20, d16, d27               ; u = clamp(qs1 - filter)

    veor        d5, d19, d18                ; *op0 = u^0x80
    veor        d4, d21, d18                ; *op1 = u^0x80
    veor        d7, d20, d18                ; *oq1 = u^0x80

    bx          lr
    ENDP        ; |vpx_loop_filter_neon|

    END
