;
; Copyright (c) 2016, Alliance for Open Media. All rights reserved
;
; This source code is subject to the terms of the BSD 2 Clause License and
; the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
; was not distributed with this source code in the LICENSE file, you can
; obtain it at www.aomedia.org/license/software. If the Alliance for Open
; Media Patent License 1.0 was not distributed with this source code in the
; PATENTS file, you can obtain it at www.aomedia.org/license/patent.
;

;

    EXPORT  |aom_lpf_horizontal_4_dual_neon|
    ARM

    AREA ||.text||, CODE, READONLY, ALIGN=2

;void aom_lpf_horizontal_4_dual_neon(uint8_t *s, int p,
;                                    const uint8_t *blimit0,
;                                    const uint8_t *limit0,
;                                    const uint8_t *thresh0,
;                                    const uint8_t *blimit1,
;                                    const uint8_t *limit1,
;                                    const uint8_t *thresh1)
; r0    uint8_t *s,
; r1    int p,
; r2    const uint8_t *blimit0,
; r3    const uint8_t *limit0,
; sp    const uint8_t *thresh0,
; sp+4  const uint8_t *blimit1,
; sp+8  const uint8_t *limit1,
; sp+12 const uint8_t *thresh1,

|aom_lpf_horizontal_4_dual_neon| PROC
    push        {lr}

    ldr         r12, [sp, #4]              ; load thresh0
    vld1.8      {d0}, [r2]                 ; load blimit0 to first half q
    vld1.8      {d2}, [r3]                 ; load limit0 to first half q

    add         r1, r1, r1                 ; double pitch
    ldr         r2, [sp, #8]               ; load blimit1

    vld1.8      {d4}, [r12]                ; load thresh0 to first half q

    ldr         r3, [sp, #12]              ; load limit1
    ldr         r12, [sp, #16]             ; load thresh1
    vld1.8      {d1}, [r2]                 ; load blimit1 to 2nd half q

    sub         r2, r0, r1, lsl #1         ; s[-4 * p]

    vld1.8      {d3}, [r3]                 ; load limit1 to 2nd half q
    vld1.8      {d5}, [r12]                ; load thresh1 to 2nd half q

    vpush       {d8-d15}                   ; save neon registers

    add         r3, r2, r1, lsr #1         ; s[-3 * p]

    vld1.u8     {q3}, [r2@64], r1          ; p3
    vld1.u8     {q4}, [r3@64], r1          ; p2
    vld1.u8     {q5}, [r2@64], r1          ; p1
    vld1.u8     {q6}, [r3@64], r1          ; p0
    vld1.u8     {q7}, [r2@64], r1          ; q0
    vld1.u8     {q8}, [r3@64], r1          ; q1
    vld1.u8     {q9}, [r2@64]              ; q2
    vld1.u8     {q10}, [r3@64]             ; q3

    sub         r2, r2, r1, lsl #1
    sub         r3, r3, r1, lsl #1

    bl          aom_loop_filter_neon_16

    vst1.u8     {q5}, [r2@64], r1          ; store op1
    vst1.u8     {q6}, [r3@64], r1          ; store op0
    vst1.u8     {q7}, [r2@64], r1          ; store oq0
    vst1.u8     {q8}, [r3@64], r1          ; store oq1

    vpop        {d8-d15}                   ; restore neon registers

    pop         {pc}
    ENDP        ; |aom_lpf_horizontal_4_dual_neon|

; void aom_loop_filter_neon_16();
; This is a helper function for the loopfilters. The invidual functions do the
; necessary load, transpose (if necessary) and store. This function uses
; registers d8-d15, so the calling function must save those registers.
;
; r0-r3, r12 PRESERVE
; q0    blimit
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
;
; Outputs:
; q5    op1
; q6    op0
; q7    oq0
; q8    oq1
|aom_loop_filter_neon_16| PROC

    ; filter_mask
    vabd.u8     q11, q3, q4                 ; m1 = abs(p3 - p2)
    vabd.u8     q12, q4, q5                 ; m2 = abs(p2 - p1)
    vabd.u8     q13, q5, q6                 ; m3 = abs(p1 - p0)
    vabd.u8     q14, q8, q7                 ; m4 = abs(q1 - q0)
    vabd.u8     q3, q9, q8                  ; m5 = abs(q2 - q1)
    vabd.u8     q4, q10, q9                 ; m6 = abs(q3 - q2)

    ; only compare the largest value to limit
    vmax.u8     q11, q11, q12               ; m7 = max(m1, m2)
    vmax.u8     q12, q13, q14               ; m8 = max(m3, m4)

    vabd.u8     q9, q6, q7                  ; abs(p0 - q0)

    vmax.u8     q3, q3, q4                  ; m9 = max(m5, m6)

    vmov.u8     q10, #0x80

    vmax.u8     q15, q11, q12               ; m10 = max(m7, m8)

    vcgt.u8     q13, q13, q2                ; (abs(p1 - p0) > thresh)*-1
    vcgt.u8     q14, q14, q2                ; (abs(q1 - q0) > thresh)*-1
    vmax.u8     q15, q15, q3                ; m11 = max(m10, m9)

    vabd.u8     q2, q5, q8                  ; a = abs(p1 - q1)
    vqadd.u8    q9, q9, q9                  ; b = abs(p0 - q0) * 2

    veor        q7, q7, q10                 ; qs0

    vcge.u8     q15, q1, q15                ; abs(m11) > limit

    vshr.u8     q2, q2, #1                  ; a = a / 2
    veor        q6, q6, q10                 ; ps0

    veor        q5, q5, q10                 ; ps1
    vqadd.u8    q9, q9, q2                  ; a = b + a

    veor        q8, q8, q10                 ; qs1

    vmov.u16    q4, #3

    vsubl.s8    q2, d14, d12                ; ( qs0 - ps0)
    vsubl.s8    q11, d15, d13

    vcge.u8     q9, q0, q9                  ; a > blimit

    vqsub.s8    q1, q5, q8                  ; filter = clamp(ps1-qs1)
    vorr        q14, q13, q14               ; hev

    vmul.i16    q2, q2, q4                  ; 3 * ( qs0 - ps0)
    vmul.i16    q11, q11, q4

    vand        q1, q1, q14                 ; filter &= hev
    vand        q15, q15, q9                ; mask

    vmov.u8     q4, #3

    vaddw.s8    q2, q2, d2                  ; filter + 3 * (qs0 - ps0)
    vaddw.s8    q11, q11, d3

    vmov.u8     q9, #4

    ; filter = clamp(filter + 3 * ( qs0 - ps0))
    vqmovn.s16  d2, q2
    vqmovn.s16  d3, q11
    vand        q1, q1, q15                 ; filter &= mask

    vqadd.s8    q2, q1, q4                  ; filter2 = clamp(filter+3)
    vqadd.s8    q1, q1, q9                  ; filter1 = clamp(filter+4)
    vshr.s8     q2, q2, #3                  ; filter2 >>= 3
    vshr.s8     q1, q1, #3                  ; filter1 >>= 3


    vqadd.s8    q11, q6, q2                 ; u = clamp(ps0 + filter2)
    vqsub.s8    q0, q7, q1                  ; u = clamp(qs0 - filter1)

    ; outer tap adjustments
    vrshr.s8    q1, q1, #1                  ; filter = ++filter1 >> 1

    veor        q7, q0,  q10                ; *oq0 = u^0x80

    vbic        q1, q1, q14                 ; filter &= ~hev

    vqadd.s8    q13, q5, q1                 ; u = clamp(ps1 + filter)
    vqsub.s8    q12, q8, q1                 ; u = clamp(qs1 - filter)

    veor        q6, q11, q10                ; *op0 = u^0x80
    veor        q5, q13, q10                ; *op1 = u^0x80
    veor        q8, q12, q10                ; *oq1 = u^0x80

    bx          lr
    ENDP        ; |aom_loop_filter_neon_16|

    END
