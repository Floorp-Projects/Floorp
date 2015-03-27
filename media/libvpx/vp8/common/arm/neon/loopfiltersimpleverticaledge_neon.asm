;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    ;EXPORT  |vp8_loop_filter_simple_vertical_edge_neon|
    EXPORT |vp8_loop_filter_bvs_neon|
    EXPORT |vp8_loop_filter_mbvs_neon|
    ARM
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char *s, PRESERVE
; r1    int p, PRESERVE
; q1    limit, PRESERVE

|vp8_loop_filter_simple_vertical_edge_neon| PROC
    sub         r0, r0, #2                  ; move src pointer down by 2 columns
    add         r12, r1, r1
    add         r3, r0, r1

    vld4.8      {d6[0], d7[0], d8[0], d9[0]}, [r0], r12
    vld4.8      {d6[1], d7[1], d8[1], d9[1]}, [r3], r12
    vld4.8      {d6[2], d7[2], d8[2], d9[2]}, [r0], r12
    vld4.8      {d6[3], d7[3], d8[3], d9[3]}, [r3], r12
    vld4.8      {d6[4], d7[4], d8[4], d9[4]}, [r0], r12
    vld4.8      {d6[5], d7[5], d8[5], d9[5]}, [r3], r12
    vld4.8      {d6[6], d7[6], d8[6], d9[6]}, [r0], r12
    vld4.8      {d6[7], d7[7], d8[7], d9[7]}, [r3], r12

    vld4.8      {d10[0], d11[0], d12[0], d13[0]}, [r0], r12
    vld4.8      {d10[1], d11[1], d12[1], d13[1]}, [r3], r12
    vld4.8      {d10[2], d11[2], d12[2], d13[2]}, [r0], r12
    vld4.8      {d10[3], d11[3], d12[3], d13[3]}, [r3], r12
    vld4.8      {d10[4], d11[4], d12[4], d13[4]}, [r0], r12
    vld4.8      {d10[5], d11[5], d12[5], d13[5]}, [r3], r12
    vld4.8      {d10[6], d11[6], d12[6], d13[6]}, [r0], r12
    vld4.8      {d10[7], d11[7], d12[7], d13[7]}, [r3]

    vswp        d7, d10
    vswp        d12, d9

    ;vp8_filter_mask() function
    ;vp8_hevmask() function
    sub         r0, r0, r1, lsl #4
    vabd.u8     q15, q5, q4                 ; abs(p0 - q0)
    vabd.u8     q14, q3, q6                 ; abs(p1 - q1)

    vqadd.u8    q15, q15, q15               ; abs(p0 - q0) * 2
    vshr.u8     q14, q14, #1                ; abs(p1 - q1) / 2
    vmov.u8     q0, #0x80                   ; 0x80
    vmov.s16    q11, #3
    vqadd.u8    q15, q15, q14               ; abs(p0 - q0) * 2 + abs(p1 - q1) / 2

    veor        q4, q4, q0                  ; qs0: q0 offset to convert to a signed value
    veor        q5, q5, q0                  ; ps0: p0 offset to convert to a signed value
    veor        q3, q3, q0                  ; ps1: p1 offset to convert to a signed value
    veor        q6, q6, q0                  ; qs1: q1 offset to convert to a signed value

    vcge.u8     q15, q1, q15                ; abs(p0 - q0)*2 + abs(p1-q1)/2 > flimit*2 + limit)*-1

    vsubl.s8    q2, d8, d10                 ; ( qs0 - ps0)
    vsubl.s8    q13, d9, d11

    vqsub.s8    q14, q3, q6                  ; vp8_filter = vp8_signed_char_clamp(ps1-qs1)

    vmul.s16    q2, q2, q11                 ;  3 * ( qs0 - ps0)
    vmul.s16    q13, q13, q11

    vmov.u8     q11, #0x03                  ; 0x03
    vmov.u8     q12, #0x04                  ; 0x04

    vaddw.s8    q2, q2, d28                  ; vp8_filter + 3 * ( qs0 - ps0)
    vaddw.s8    q13, q13, d29

    vqmovn.s16  d28, q2                      ; vp8_filter = vp8_signed_char_clamp(vp8_filter + 3 * ( qs0 - ps0))
    vqmovn.s16  d29, q13

    add         r0, r0, #1
    add         r3, r0, r1

    vand        q14, q14, q15                 ; vp8_filter &= mask

    vqadd.s8    q2, q14, q11                 ; Filter2 = vp8_signed_char_clamp(vp8_filter+3)
    vqadd.s8    q3, q14, q12                 ; Filter1 = vp8_signed_char_clamp(vp8_filter+4)
    vshr.s8     q2, q2, #3                  ; Filter2 >>= 3
    vshr.s8     q14, q3, #3                  ; Filter1 >>= 3

    ;calculate output
    vqadd.s8    q11, q5, q2                 ; u = vp8_signed_char_clamp(ps0 + Filter2)
    vqsub.s8    q10, q4, q14                 ; u = vp8_signed_char_clamp(qs0 - Filter1)

    veor        q6, q11, q0                 ; *op0 = u^0x80
    veor        q7, q10, q0                 ; *oq0 = u^0x80
    add         r12, r1, r1
    vswp        d13, d14

    ;store op1, op0, oq0, oq1
    vst2.8      {d12[0], d13[0]}, [r0], r12
    vst2.8      {d12[1], d13[1]}, [r3], r12
    vst2.8      {d12[2], d13[2]}, [r0], r12
    vst2.8      {d12[3], d13[3]}, [r3], r12
    vst2.8      {d12[4], d13[4]}, [r0], r12
    vst2.8      {d12[5], d13[5]}, [r3], r12
    vst2.8      {d12[6], d13[6]}, [r0], r12
    vst2.8      {d12[7], d13[7]}, [r3], r12
    vst2.8      {d14[0], d15[0]}, [r0], r12
    vst2.8      {d14[1], d15[1]}, [r3], r12
    vst2.8      {d14[2], d15[2]}, [r0], r12
    vst2.8      {d14[3], d15[3]}, [r3], r12
    vst2.8      {d14[4], d15[4]}, [r0], r12
    vst2.8      {d14[5], d15[5]}, [r3], r12
    vst2.8      {d14[6], d15[6]}, [r0], r12
    vst2.8      {d14[7], d15[7]}, [r3]

    bx          lr
    ENDP        ; |vp8_loop_filter_simple_vertical_edge_neon|

; r0    unsigned char *y
; r1    int ystride
; r2    const unsigned char *blimit

|vp8_loop_filter_bvs_neon| PROC
    push        {r4, lr}
    ldrb        r3, [r2]                   ; load blim from mem
    mov         r4, r0
    add         r0, r0, #4
    vdup.s8     q1, r3                     ; duplicate blim
    bl          vp8_loop_filter_simple_vertical_edge_neon
    ; vp8_loop_filter_simple_vertical_edge_neon preserves  r1 and q1
    add         r0, r4, #8
    bl          vp8_loop_filter_simple_vertical_edge_neon
    add         r0, r4, #12
    pop         {r4, lr}
    b           vp8_loop_filter_simple_vertical_edge_neon
    ENDP        ;|vp8_loop_filter_bvs_neon|

; r0    unsigned char *y
; r1    int ystride
; r2    const unsigned char *blimit

|vp8_loop_filter_mbvs_neon| PROC
    ldrb        r3, [r2]                   ; load mblim from mem
    vdup.s8     q1, r3                     ; duplicate mblim
    b           vp8_loop_filter_simple_vertical_edge_neon
    ENDP        ;|vp8_loop_filter_bvs_neon|
    END
