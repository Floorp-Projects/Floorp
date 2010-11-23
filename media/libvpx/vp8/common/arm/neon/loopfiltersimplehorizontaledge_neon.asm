;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_loop_filter_simple_horizontal_edge_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
;Note: flimit, limit, and thresh shpuld be positive numbers. All 16 elements in flimit
;are equal. So, in the code, only one load is needed
;for flimit. Same way applies to limit and thresh.
; r0    unsigned char *s,
; r1    int p, //pitch
; r2    const signed char *flimit,
; r3    const signed char *limit,
; stack(r4) const signed char *thresh,
; //stack(r5)   int count --unused

|vp8_loop_filter_simple_horizontal_edge_neon| PROC
    sub         r0, r0, r1, lsl #1          ; move src pointer down by 2 lines

    ldr         r12, _lfhy_coeff_
    vld1.u8     {q5}, [r0], r1              ; p1
    vld1.s8     {d2[], d3[]}, [r2]          ; flimit
    vld1.s8     {d26[], d27[]}, [r3]        ; limit -> q13
    vld1.u8     {q6}, [r0], r1              ; p0
    vld1.u8     {q0}, [r12]!                ; 0x80
    vld1.u8     {q7}, [r0], r1              ; q0
    vld1.u8     {q10}, [r12]!               ; 0x03
    vld1.u8     {q8}, [r0]                  ; q1

    ;vp8_filter_mask() function
    vabd.u8     q15, q6, q7                 ; abs(p0 - q0)
    vabd.u8     q14, q5, q8                 ; abs(p1 - q1)
    vqadd.u8    q15, q15, q15               ; abs(p0 - q0) * 2
    vshr.u8     q14, q14, #1                ; abs(p1 - q1) / 2
    vqadd.u8    q15, q15, q14               ; abs(p0 - q0) * 2 + abs(p1 - q1) / 2

    ;vp8_filter() function
    veor        q7, q7, q0                  ; qs0: q0 offset to convert to a signed value
    veor        q6, q6, q0                  ; ps0: p0 offset to convert to a signed value
    veor        q5, q5, q0                  ; ps1: p1 offset to convert to a signed value
    veor        q8, q8, q0                  ; qs1: q1 offset to convert to a signed value

    vadd.u8     q1, q1, q1                  ; flimit * 2
    vadd.u8     q1, q1, q13                 ; flimit * 2 + limit
    vcge.u8     q15, q1, q15                ; (abs(p0 - q0)*2 + abs(p1-q1)/2 > flimit*2 + limit)*-1

;;;;;;;;;;
    ;vqsub.s8   q2, q7, q6                  ; ( qs0 - ps0)
    vsubl.s8    q2, d14, d12                ; ( qs0 - ps0)
    vsubl.s8    q3, d15, d13

    vqsub.s8    q4, q5, q8                  ; q4: vp8_filter = vp8_signed_char_clamp(ps1-qs1)

    ;vmul.i8    q2, q2, q10                 ;  3 * ( qs0 - ps0)
    vadd.s16    q11, q2, q2                 ;  3 * ( qs0 - ps0)
    vadd.s16    q12, q3, q3

    vld1.u8     {q9}, [r12]!                ; 0x04

    vadd.s16    q2, q2, q11
    vadd.s16    q3, q3, q12

    vaddw.s8    q2, q2, d8                  ; vp8_filter + 3 * ( qs0 - ps0)
    vaddw.s8    q3, q3, d9

    ;vqadd.s8   q4, q4, q2                  ; vp8_filter = vp8_signed_char_clamp(vp8_filter + 3 * ( qs0 - ps0))
    vqmovn.s16  d8, q2                      ; vp8_filter = vp8_signed_char_clamp(vp8_filter + 3 * ( qs0 - ps0))
    vqmovn.s16  d9, q3
;;;;;;;;;;;;;

    vand        q4, q4, q15                 ; vp8_filter &= mask

    vqadd.s8    q2, q4, q10                 ; Filter2 = vp8_signed_char_clamp(vp8_filter+3)
    vqadd.s8    q4, q4, q9                  ; Filter1 = vp8_signed_char_clamp(vp8_filter+4)
    vshr.s8     q2, q2, #3                  ; Filter2 >>= 3
    vshr.s8     q4, q4, #3                  ; Filter1 >>= 3

    sub         r0, r0, r1, lsl #1

    ;calculate output
    vqadd.s8    q11, q6, q2                 ; u = vp8_signed_char_clamp(ps0 + Filter2)
    vqsub.s8    q10, q7, q4                 ; u = vp8_signed_char_clamp(qs0 - Filter1)

    add         r3, r0, r1

    veor        q6, q11, q0                 ; *op0 = u^0x80
    veor        q7, q10, q0                 ; *oq0 = u^0x80

    vst1.u8     {q6}, [r0]                  ; store op0
    vst1.u8     {q7}, [r3]                  ; store oq0

    bx          lr
    ENDP        ; |vp8_loop_filter_simple_horizontal_edge_neon|

;-----------------
    AREA    hloopfiltery_dat, DATA, READWRITE           ;read/write by default
;Data section with name data_area is specified. DCD reserves space in memory for 16 data.
;One word each is reserved. Label filter_coeff can be used to access the data.
;Data address: filter_coeff, filter_coeff+4, filter_coeff+8 ...
_lfhy_coeff_
    DCD     lfhy_coeff
lfhy_coeff
    DCD     0x80808080, 0x80808080, 0x80808080, 0x80808080
    DCD     0x03030303, 0x03030303, 0x03030303, 0x03030303
    DCD     0x04040404, 0x04040404, 0x04040404, 0x04040404

    END
