;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    EXPORT  |vp8_loop_filter_simple_vertical_edge_neon|
    ARM
    REQUIRE8
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2
;Note: flimit, limit, and thresh should be positive numbers. All 16 elements in flimit
;are equal. So, in the code, only one load is needed
;for flimit. Same way applies to limit and thresh.
; r0    unsigned char *s,
; r1    int p, //pitch
; r2    const signed char *flimit,
; r3    const signed char *limit,
; stack(r4) const signed char *thresh,
; //stack(r5)   int count --unused

|vp8_loop_filter_simple_vertical_edge_neon| PROC
    sub         r0, r0, #2                  ; move src pointer down by 2 columns

    vld4.8      {d6[0], d7[0], d8[0], d9[0]}, [r0], r1
    vld1.s8     {d2[], d3[]}, [r2]          ; flimit
    vld1.s8     {d26[], d27[]}, [r3]        ; limit -> q13
    vld4.8      {d6[1], d7[1], d8[1], d9[1]}, [r0], r1
    ldr         r12, _vlfy_coeff_
    vld4.8      {d6[2], d7[2], d8[2], d9[2]}, [r0], r1
    vld4.8      {d6[3], d7[3], d8[3], d9[3]}, [r0], r1
    vld4.8      {d6[4], d7[4], d8[4], d9[4]}, [r0], r1
    vld4.8      {d6[5], d7[5], d8[5], d9[5]}, [r0], r1
    vld4.8      {d6[6], d7[6], d8[6], d9[6]}, [r0], r1
    vld4.8      {d6[7], d7[7], d8[7], d9[7]}, [r0], r1

    vld4.8      {d10[0], d11[0], d12[0], d13[0]}, [r0], r1
    vld1.u8     {q0}, [r12]!                ; 0x80
    vld4.8      {d10[1], d11[1], d12[1], d13[1]}, [r0], r1
    vld1.u8     {q11}, [r12]!               ; 0x03
    vld4.8      {d10[2], d11[2], d12[2], d13[2]}, [r0], r1
    vld1.u8     {q12}, [r12]!               ; 0x04
    vld4.8      {d10[3], d11[3], d12[3], d13[3]}, [r0], r1
    vld4.8      {d10[4], d11[4], d12[4], d13[4]}, [r0], r1
    vld4.8      {d10[5], d11[5], d12[5], d13[5]}, [r0], r1
    vld4.8      {d10[6], d11[6], d12[6], d13[6]}, [r0], r1
    vld4.8      {d10[7], d11[7], d12[7], d13[7]}, [r0], r1

    vswp        d7, d10
    vswp        d12, d9
    ;vswp       q4, q5                      ; p1:q3, p0:q5, q0:q4, q1:q6

    ;vp8_filter_mask() function
    ;vp8_hevmask() function
    sub         r0, r0, r1, lsl #4
    vabd.u8     q15, q5, q4                 ; abs(p0 - q0)
    vabd.u8     q14, q3, q6                 ; abs(p1 - q1)
    vqadd.u8    q15, q15, q15               ; abs(p0 - q0) * 2
    vshr.u8     q14, q14, #1                ; abs(p1 - q1) / 2
    vqadd.u8    q15, q15, q14               ; abs(p0 - q0) * 2 + abs(p1 - q1) / 2

    veor        q4, q4, q0                  ; qs0: q0 offset to convert to a signed value
    veor        q5, q5, q0                  ; ps0: p0 offset to convert to a signed value
    veor        q3, q3, q0                  ; ps1: p1 offset to convert to a signed value
    veor        q6, q6, q0                  ; qs1: q1 offset to convert to a signed value

    vadd.u8     q1, q1, q1                  ; flimit * 2
    vadd.u8     q1, q1, q13                 ; flimit * 2 + limit
    vcge.u8     q15, q1, q15                ; abs(p0 - q0)*2 + abs(p1-q1)/2 > flimit*2 + limit)*-1

    ;vp8_filter() function
;;;;;;;;;;
    ;vqsub.s8   q2, q5, q4                  ; ( qs0 - ps0)
    vsubl.s8    q2, d8, d10                 ; ( qs0 - ps0)
    vsubl.s8    q13, d9, d11

    vqsub.s8    q1, q3, q6                  ; vp8_filter = vp8_signed_char_clamp(ps1-qs1)

    ;vmul.i8    q2, q2, q11                 ; vp8_filter = vp8_signed_char_clamp(vp8_filter + 3 * ( qs0 - ps0))
    vadd.s16    q10, q2, q2                 ;  3 * ( qs0 - ps0)
    vadd.s16    q14, q13, q13
    vadd.s16    q2, q2, q10
    vadd.s16    q13, q13, q14

    ;vqadd.s8   q1, q1, q2
    vaddw.s8    q2, q2, d2                  ; vp8_filter + 3 * ( qs0 - ps0)
    vaddw.s8    q13, q13, d3

    vqmovn.s16  d2, q2                      ; vp8_filter = vp8_signed_char_clamp(vp8_filter + 3 * ( qs0 - ps0))
    vqmovn.s16  d3, q13

    add         r0, r0, #1
    add         r2, r0, r1
;;;;;;;;;;;

    vand        q1, q1, q15                 ; vp8_filter &= mask

    vqadd.s8    q2, q1, q11                 ; Filter2 = vp8_signed_char_clamp(vp8_filter+3)
    vqadd.s8    q1, q1, q12                 ; Filter1 = vp8_signed_char_clamp(vp8_filter+4)
    vshr.s8     q2, q2, #3                  ; Filter2 >>= 3
    vshr.s8     q1, q1, #3                  ; Filter1 >>= 3

    ;calculate output
    vqsub.s8    q10, q4, q1                 ; u = vp8_signed_char_clamp(qs0 - Filter1)
    vqadd.s8    q11, q5, q2                 ; u = vp8_signed_char_clamp(ps0 + Filter2)

    veor        q7, q10, q0                 ; *oq0 = u^0x80
    veor        q6, q11, q0                 ; *op0 = u^0x80

    add         r3, r2, r1
    vswp        d13, d14
    add         r12, r3, r1

    ;store op1, op0, oq0, oq1
    vst2.8      {d12[0], d13[0]}, [r0]
    vst2.8      {d12[1], d13[1]}, [r2]
    vst2.8      {d12[2], d13[2]}, [r3]
    vst2.8      {d12[3], d13[3]}, [r12], r1
    add         r0, r12, r1
    vst2.8      {d12[4], d13[4]}, [r12]
    vst2.8      {d12[5], d13[5]}, [r0], r1
    add         r2, r0, r1
    vst2.8      {d12[6], d13[6]}, [r0]
    vst2.8      {d12[7], d13[7]}, [r2], r1
    add         r3, r2, r1
    vst2.8      {d14[0], d15[0]}, [r2]
    vst2.8      {d14[1], d15[1]}, [r3], r1
    add         r12, r3, r1
    vst2.8      {d14[2], d15[2]}, [r3]
    vst2.8      {d14[3], d15[3]}, [r12], r1
    add         r0, r12, r1
    vst2.8      {d14[4], d15[4]}, [r12]
    vst2.8      {d14[5], d15[5]}, [r0], r1
    add         r2, r0, r1
    vst2.8      {d14[6], d15[6]}, [r0]
    vst2.8      {d14[7], d15[7]}, [r2]

    bx          lr
    ENDP        ; |vp8_loop_filter_simple_vertical_edge_neon|

;-----------------
    AREA    vloopfiltery_dat, DATA, READWRITE           ;read/write by default
;Data section with name data_area is specified. DCD reserves space in memory for 16 data.
;One word each is reserved. Label filter_coeff can be used to access the data.
;Data address: filter_coeff, filter_coeff+4, filter_coeff+8 ...
_vlfy_coeff_
    DCD     vlfy_coeff
vlfy_coeff
    DCD     0x80808080, 0x80808080, 0x80808080, 0x80808080
    DCD     0x03030303, 0x03030303, 0x03030303, 0x03030303
    DCD     0x04040404, 0x04040404, 0x04040404, 0x04040404

    END
