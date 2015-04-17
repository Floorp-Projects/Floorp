;
;  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
;
;  Use of this source code is governed by a BSD-style license
;  that can be found in the LICENSE file in the root of the source
;  tree. An additional intellectual property rights grant can be found
;  in the file PATENTS.  All contributing project authors may
;  be found in the AUTHORS file in the root of the source tree.
;


    ;EXPORT  |vp8_loop_filter_simple_horizontal_edge_neon|
    EXPORT  |vp8_loop_filter_bhs_neon|
    EXPORT  |vp8_loop_filter_mbhs_neon|
    ARM
    PRESERVE8

    AREA ||.text||, CODE, READONLY, ALIGN=2

; r0    unsigned char *s, PRESERVE
; r1    int p, PRESERVE
; q1    limit, PRESERVE

|vp8_loop_filter_simple_horizontal_edge_neon| PROC

    sub         r3, r0, r1, lsl #1          ; move src pointer down by 2 lines

    vld1.u8     {q7}, [r0@128], r1          ; q0
    vld1.u8     {q5}, [r3@128], r1          ; p0
    vld1.u8     {q8}, [r0@128]              ; q1
    vld1.u8     {q6}, [r3@128]              ; p1

    vabd.u8     q15, q6, q7                 ; abs(p0 - q0)
    vabd.u8     q14, q5, q8                 ; abs(p1 - q1)

    vqadd.u8    q15, q15, q15               ; abs(p0 - q0) * 2
    vshr.u8     q14, q14, #1                ; abs(p1 - q1) / 2
    vmov.u8     q0, #0x80                   ; 0x80
    vmov.s16    q13, #3
    vqadd.u8    q15, q15, q14               ; abs(p0 - q0) * 2 + abs(p1 - q1) / 2

    veor        q7, q7, q0                  ; qs0: q0 offset to convert to a signed value
    veor        q6, q6, q0                  ; ps0: p0 offset to convert to a signed value
    veor        q5, q5, q0                  ; ps1: p1 offset to convert to a signed value
    veor        q8, q8, q0                  ; qs1: q1 offset to convert to a signed value

    vcge.u8     q15, q1, q15                ; (abs(p0 - q0)*2 + abs(p1-q1)/2 > limit)*-1

    vsubl.s8    q2, d14, d12                ; ( qs0 - ps0)
    vsubl.s8    q3, d15, d13

    vqsub.s8    q4, q5, q8                  ; q4: vp8_filter = vp8_signed_char_clamp(ps1-qs1)

    vmul.s16    q2, q2, q13                 ;  3 * ( qs0 - ps0)
    vmul.s16    q3, q3, q13

    vmov.u8     q10, #0x03                  ; 0x03
    vmov.u8     q9, #0x04                   ; 0x04

    vaddw.s8    q2, q2, d8                  ; vp8_filter + 3 * ( qs0 - ps0)
    vaddw.s8    q3, q3, d9

    vqmovn.s16  d8, q2                      ; vp8_filter = vp8_signed_char_clamp(vp8_filter + 3 * ( qs0 - ps0))
    vqmovn.s16  d9, q3

    vand        q14, q4, q15                ; vp8_filter &= mask

    vqadd.s8    q2, q14, q10                ; Filter2 = vp8_signed_char_clamp(vp8_filter+3)
    vqadd.s8    q3, q14, q9                 ; Filter1 = vp8_signed_char_clamp(vp8_filter+4)
    vshr.s8     q2, q2, #3                  ; Filter2 >>= 3
    vshr.s8     q4, q3, #3                  ; Filter1 >>= 3

    sub         r0, r0, r1

    ;calculate output
    vqadd.s8    q11, q6, q2                 ; u = vp8_signed_char_clamp(ps0 + Filter2)
    vqsub.s8    q10, q7, q4                 ; u = vp8_signed_char_clamp(qs0 - Filter1)

    veor        q6, q11, q0                 ; *op0 = u^0x80
    veor        q7, q10, q0                 ; *oq0 = u^0x80

    vst1.u8     {q6}, [r3@128]              ; store op0
    vst1.u8     {q7}, [r0@128]              ; store oq0

    bx          lr
    ENDP        ; |vp8_loop_filter_simple_horizontal_edge_neon|

; r0    unsigned char *y
; r1    int ystride
; r2    const unsigned char *blimit

|vp8_loop_filter_bhs_neon| PROC
    push        {r4, lr}
    ldrb        r3, [r2]                    ; load blim from mem
    vdup.s8     q1, r3                      ; duplicate blim

    add         r0, r0, r1, lsl #2          ; src = y_ptr + 4 * y_stride
    bl          vp8_loop_filter_simple_horizontal_edge_neon
    ; vp8_loop_filter_simple_horizontal_edge_neon preserves r0, r1 and q1
    add         r0, r0, r1, lsl #2          ; src = y_ptr + 8* y_stride
    bl          vp8_loop_filter_simple_horizontal_edge_neon
    add         r0, r0, r1, lsl #2          ; src = y_ptr + 12 * y_stride
    pop         {r4, lr}
    b           vp8_loop_filter_simple_horizontal_edge_neon
    ENDP        ;|vp8_loop_filter_bhs_neon|

; r0    unsigned char *y
; r1    int ystride
; r2    const unsigned char *blimit

|vp8_loop_filter_mbhs_neon| PROC
    ldrb        r3, [r2]                   ; load blim from mem
    vdup.s8     q1, r3                     ; duplicate mblim
    b           vp8_loop_filter_simple_horizontal_edge_neon
    ENDP        ;|vp8_loop_filter_bhs_neon|

    END
